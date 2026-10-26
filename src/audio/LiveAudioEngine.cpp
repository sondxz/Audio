#include "LiveAudioEngine.h"
#include "PassThroughCore.h"
#include <algorithm>
#include <chrono>

namespace lah::audio
{
LiveAudioEngine::LiveAudioEngine()
{
    nativeRack.setSpectrumTap(&nativeSpectrum,false);
    outputNativeRack.setSpectrumTap(&nativeSpectrum,true);
    for (auto& value : inputPeaks)  value.store (0.0f, std::memory_order_relaxed);
    for (auto& value : outputPeaks) value.store (0.0f, std::memory_order_relaxed);
}

LiveAudioEngine::~LiveAudioEngine()
{
    // Owner must detach the audio callback before destruction.
    removePlugin();
}

void LiveAudioEngine::audioProcessorChanged(juce::AudioProcessor* processor,
                                           const juce::AudioProcessorListener::ChangeDetails& details)
{
    if (details.latencyChanged && processor != nullptr)
    {
        const int latency=processor->getLatencySamples();
        if (pluginLatency.exchange(latency, std::memory_order_acq_rel)!=latency)
            pluginLatencyChanged.store(true, std::memory_order_release);
    }
}

void LiveAudioEngine::publishPeak (std::atomic<float>& target, float peak) noexcept
{
    float current = target.load (std::memory_order_relaxed);
    while (current < peak && ! target.compare_exchange_weak (current, peak,
                                                       std::memory_order_relaxed,
                                                       std::memory_order_relaxed)) {}
}

float LiveAudioEngine::consumeInputPeak (int ch) noexcept
{
    if (ch < 0 || ch >= visualChannelLimit) return 0.0f;
    return inputPeaks[static_cast<std::size_t> (ch)].exchange (0.0f, std::memory_order_relaxed);
}

float LiveAudioEngine::consumeOutputPeak (int ch) noexcept
{
    if (ch < 0 || ch >= visualChannelLimit) return 0.0f;
    return outputPeaks[static_cast<std::size_t> (ch)].exchange (0.0f, std::memory_order_relaxed);
}

bool LiveAudioEngine::installStereoPlugin (std::unique_ptr<juce::AudioPluginInstance> p, double rate,
                                           int block, juce::String& error)
{
    if (p == nullptr) {error = juce::String::fromUTF8("Không tải được VST3");return false;}
    if (!std::isfinite(rate) || rate < 8000.0 || rate > 192000.0 || block <= 0 || block > 65536)
    {
        error = juce::String::fromUTF8("Sample rate / block size của audio device không hợp lệ.");
        return false;
    }
    if (p->getTotalNumInputChannels() != 2 || p->getTotalNumOutputChannels() != 2)
    {
        error = juce::String::fromUTF8("Hiện chỉ hỗ trợ VST3 stereo (2 input / 2 output).");
        return false;
    }
    removePlugin();
    try
    {
        p->setRateAndBufferSizeDetails (rate, block);
        p->prepareToPlay (rate, block);
        p->reset(); // detached: third-party reset may allocate or block
    }
    catch (...)
    {
        setPassThroughEnabled(false);
        errors.fetch_add(1, std::memory_order_relaxed);
        try { p->releaseResources(); } catch (...) { errors.fetch_add(1, std::memory_order_relaxed); }
        error=juce::String::fromUTF8("VST3 phát sinh C++ exception khi prepare/reset. Audio vẫn MUTE.");
        return false;
    }
    pluginPrepared = true;
    pluginMaximumBlockSize = std::min(block, 4096);
    pluginBuffer.setSize (2, 4096, false, true, true);
    pluginMidi.ensureSize (2048); // reserve routine MIDI capacity outside the audio callback
    stereoPlugin = std::move(p);
    stereoPlugin->addListener(this);
    pluginBypassed.store (true, std::memory_order_relaxed);
    pluginResetRequired.store(false, std::memory_order_release);
    pluginLatency.store (stereoPlugin->getLatencySamples(), std::memory_order_relaxed);
    pluginLatencyChanged.store(false, std::memory_order_release);
    return true;
}

void LiveAudioEngine::removePlugin()
{
    pluginBypassed.store (true, std::memory_order_relaxed);
    if (stereoPlugin != nullptr) stereoPlugin->removeListener(this);
    releasePluginResourcesSafely();
    pluginMaximumBlockSize = 0;
    stereoPlugin.reset();
    pluginLatency.store (0, std::memory_order_relaxed);
    pluginResetRequired.store(false, std::memory_order_release);
    pluginLatencyChanged.store(false, std::memory_order_release);
}

bool LiveAudioEngine::releasePluginResourcesSafely() noexcept
{
    if (stereoPlugin == nullptr || !pluginPrepared) return true;
    pluginPrepared=false;
    pluginMaximumBlockSize=0;
    try { stereoPlugin->releaseResources(); }
    catch (...)
    {
        errors.fetch_add(1, std::memory_order_relaxed);
        setPassThroughEnabled(false);
        return false;
    }
    return true;
}

bool LiveAudioEngine::installStereoFir(const fir::Design& design,double deviceRate,juce::String& error)
{
    firBypassed.store(true,std::memory_order_release);
    stereoFir.release();
    firWasActive = false;
    if (!stereoFir.prepare(design,deviceRate)) {
        error = juce::String::fromUTF8("Realtime FIR LAB chỉ cho 129/257 taps, sample rate phải khớp audio device.");
        firSampleRate=0;
        return false;
    }
    firSampleRate=deviceRate;
    return true; // Stays BYPASSED until explicit manual enable while audio MUTED.
}

void LiveAudioEngine::removeStereoFir()
{
    firBypassed.store(true,std::memory_order_release);
    stereoFir.release();
    firWasActive = false;
    firSampleRate=0;
}

juce::String LiveAudioEngine::getPluginName() const
{
    try { return stereoPlugin != nullptr ? stereoPlugin->getName() : juce::String ("None"); }
    catch (...) { return "Unavailable (plug-in name error)"; }
}

void LiveAudioEngine::audioDeviceIOCallbackWithContext (const float* const* inputs, int ni,
                                                         float* const* outputs, int no, int samples,
                                                         const juce::AudioIODeviceCallbackContext&)
{
    const juce::ScopedNoDenormals noDenormals;
    const auto callbackStarted = std::chrono::steady_clock::now();
    if (samples <= 0 || samples > maximumCallbackFrames)
    {
        // Reject invalid geometry before any DSP/plug-in/capture reads. The driver
        // contract still requires each supplied output to hold samples frames.
        if (outputs != nullptr && samples > 0)
            for (int ch = 0; ch < no; ++ch)
                if (outputs[ch] != nullptr) std::fill_n(outputs[ch], samples, 0.0f);
        safety.record(samples, callbackSampleRate.load(std::memory_order_relaxed), 0);
        setPassThroughEnabled(false);
        compensation.invalidate();
        compensationWasActive=false;
        appliedCompensation.store(0, std::memory_order_relaxed);
        stereoFir.clear();
        firWasActive=false;
        callbacks.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    float currentInputs[visualChannelLimit] {};
    float currentOutputs[visualChannelLimit] {};
    const auto muteEpoch=muteGeneration.load(std::memory_order_acquire);
    const bool muteEdge=muteEpoch!=consumedMuteGeneration;
    consumedMuteGeneration=muteEpoch;
    // Even a MUTE/re-enable pair between callbacks must execute one silent
    // block, invalidating mixer, FIR and PDC histories before audio resumes.
    bool enabled = passEnabled.load (std::memory_order_acquire)
                   && running.load(std::memory_order_acquire) && !safety.isFaulted() && !muteEdge;
    const bool latencyNotification = pluginLatencyChanged.exchange(false, std::memory_order_acq_rel);
    bool pluginActive = enabled && stereoPlugin != nullptr && pluginPrepared && pluginMaximumBlockSize > 0
                        && !pluginBypassed.load(std::memory_order_acquire)
                        && outputs && no>=2 && outputs[0] && outputs[1];
    bool firActive = enabled && stereoFir.ready() && !firBypassed.load(std::memory_order_acquire)
                     && outputs && no>=2 && outputs[0] && outputs[1];
    const int reportedPluginLatency=pluginActive ? pluginLatency.load(std::memory_order_acquire) : 0;
    const auto requestedLatency = static_cast<std::int64_t>(reportedPluginLatency)
                                  + (firActive ? stereoFir.latencySamples() : 0);
    const bool unsupportedLatency = reportedPluginLatency<0 || requestedLatency<0
                                     || requestedLatency>LatencyCompensator::maximumSupportedDelay;
    // Latency switches while processing cannot preserve cross-channel timing.
    // Refuse the block and require an explicitly muted reset/reprepare instead.
    if ((pluginActive && latencyNotification) || (enabled && unsupportedLatency)
        || (enabled && compensationWasActive && requestedLatency!=compensation.latencySamples()))
    {
        setPassThroughEnabled(false);
        errors.fetch_add(1, std::memory_order_relaxed);
        enabled=pluginActive=firActive=false;
    }
    if (!enabled)
    {
        compensation.invalidate();
        compensationWasActive=false;
        appliedCompensation.store(0, std::memory_order_relaxed);
    }
    else if (!compensationWasActive)
    {
        if (!compensation.configure(static_cast<int>(requestedLatency)))
        {
            setPassThroughEnabled(false);
            errors.fetch_add(1, std::memory_order_relaxed);
            enabled=pluginActive=firActive=false;
        }
        compensationWasActive=enabled;
        appliedCompensation.store(enabled ? static_cast<int>(requestedLatency) : 0, std::memory_order_relaxed);
    }
    mixer.process (inputs, std::min (ni, dsp::channels), outputs, std::min (no, dsp::channels), samples,
                   enabled, currentInputs, currentOutputs, visualChannelLimit);
    // Experimental short-tap linear-phase FIR on OUT 1/2. Prepared off callback;
    // reconfiguration and enabling are only allowed while muted/detached.
    if (!firActive && firWasActive)
        stereoFir.clear(); // bounded: at most 2 x 257 samples; no allocation, no stale PANIC tail
    firWasActive = firActive;
    if (firActive)
        stereoFir.process(outputs[0],outputs[1],samples);
    bool pluginThrew=false;
    if (pluginActive)
    {
        // Logical buffer size can change within its preallocated capacity.
        for (int offset = 0; offset < samples; offset += pluginMaximumBlockSize)
        {
            const int count = std::min (pluginMaximumBlockSize, samples - offset);
            // JUCE retains preallocated 4096-frame capacity (avoidReallocating=true).
            // Adjust logical block length so effects don't advance 4096 samples for a 128-frame callback.
            pluginBuffer.setSize (2, count, false, false, true);
            for (int ch = 0; ch < 2; ++ch)
                std::copy_n (outputs[ch] + offset, count, pluginBuffer.getWritePointer (ch));
            pluginMidi.clear();
            // Third-party code may introduce its own latency/allocations/crashes: UNVERIFIED FOR SHOW USE.
            try
            {
                stereoPlugin->processBlock (pluginBuffer, pluginMidi);
            }
            catch (...)
            {
                // Handles C++ exceptions only. Access violations, deadlocks and
                // arbitrary third-party corruption still require process isolation.
                pluginThrew=true;
                break;
            }
            for (int ch = 0; ch < 2; ++ch)
            {
                const auto* src = pluginBuffer.getReadPointer (ch);
                for (int i = 0; i < count; ++i)
                    outputs[ch][offset + i] = dsp::clean(src[i]);
            }
        }
    }
    if (pluginThrew || (pluginActive && pluginLatencyChanged.load(std::memory_order_acquire)))
    {
        // Silence the entire block, including any earlier successful chunks.
        // Third-party reset/reprepare is deferred to the muted detached path.
        setPassThroughEnabled(false);
        errors.fetch_add(1, std::memory_order_relaxed);
        enabled=false;
        compensation.invalidate();
        compensationWasActive=false;
        appliedCompensation.store(0, std::memory_order_relaxed);
        if (outputs)
            for(int ch=0;ch<no;++ch)
                if(outputs[ch]) std::fill_n(outputs[ch],samples,0.0f);
    }
    if (enabled) compensation.process(outputs,no,samples);
    // Final output protection/meters belong after every insert and PDC delay.
    // Stored delay history may have been captured under a higher old ceiling.
    for(int ch=0;ch<std::min(no,dsp::channels);++ch)
    {
        currentOutputs[ch]=0.0f;
        if (!outputs || !outputs[ch]) continue;
        const auto cap=dsp::dB(dsp::clamp(parameters.outputCeiling[ch].load(std::memory_order_relaxed),-24.0f,0.0f));
        for(int sample=0;sample<samples;++sample)
        {
            const auto safe=std::clamp(dsp::clean(outputs[ch][sample]),-cap,cap);
            outputs[ch][sample]=safe;
            currentOutputs[ch]=std::max(currentOutputs[ch],std::abs(safe));
        }
    }
    // JUCE may expose more channels than the implemented 32x32 matrix.
    for (int ch = dsp::channels; ch < no; ++ch)
        if (outputs != nullptr && outputs[ch] != nullptr && samples > 0)
            std::fill_n (outputs[ch], samples, 0.0f);
    for (int ch = dsp::channels; ch < std::min (ni, visualChannelLimit); ++ch)
        if (inputs != nullptr && inputs[ch] != nullptr && samples > 0)
            for (int i = 0; i < samples; ++i)
                currentInputs[ch] = std::max (currentInputs[ch], std::abs (dsp::clean (inputs[ch][i])));
    if (inputs != nullptr && samples > 0)
    {
        const auto ref = parameters.referenceChannel.load (std::memory_order_relaxed);
        const auto mic = parameters.measurementChannel.load (std::memory_order_relaxed);
        if (mic >= 0 && mic < ni && inputs[mic])
        {
            const float* reference = ref >= 0 && ref < ni && ref != mic ? inputs[ref] : nullptr;
            inputCapture.put (reference, inputs[mic], samples);
        }
    }

    const int trackedInputs  = std::min (std::max (0, ni), visualChannelLimit);
    const int trackedOutputs = std::min (std::max (0, no), visualChannelLimit);
    for (int i = 0; i < trackedInputs; ++i)
        publishPeak (inputPeaks[static_cast<std::size_t> (i)], currentInputs[i]);
    for (int i = 0; i < trackedOutputs; ++i)
        publishPeak (outputPeaks[static_cast<std::size_t> (i)], currentOutputs[i]);
    inputCount.store (ni, std::memory_order_relaxed);
    outputCount.store (no, std::memory_order_relaxed);
    callbacks.fetch_add (1, std::memory_order_relaxed);
    const auto nanos=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now()-callbackStarted).count());
    auto previous=maxCallbackNs.load(std::memory_order_relaxed);
    while(nanos>previous && !maxCallbackNs.compare_exchange_weak(
                     previous,nanos,std::memory_order_relaxed,std::memory_order_relaxed)){}
    const auto fs=callbackSampleRate.load(std::memory_order_relaxed);
    if(fs>0 && samples>0 && static_cast<double>(nanos)>1e9*static_cast<double>(samples)/fs)
        deadlineOverruns.fetch_add(1,std::memory_order_relaxed);
    if (!safety.record(samples,fs,nanos))
        setPassThroughEnabled(false); // next callback is silent; operator re-arms
    // A deadline overrun is NOT identical to a hardware/driver-reported XRun.
}

void LiveAudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    nativeSpectrum.prepare(0);
    passEnabled.store (false, std::memory_order_release);
    running.store (false, std::memory_order_release);
    pluginBypassed.store (true, std::memory_order_relaxed);
    pluginResetRequired.store(true, std::memory_order_release);
    firBypassed.store(true,std::memory_order_release);
    firWasActive = false;
    compensationWasActive=false;
    appliedCompensation.store(0, std::memory_order_relaxed);
    consumedMuteGeneration=muteGeneration.load(std::memory_order_acquire);
    inputCapture.reset();
    if (device == nullptr)
    {
        deviceGeneration.fetch_add (1, std::memory_order_release);
        return;
    }
    const auto rate = device->getCurrentSampleRate();
    const auto block = device->getCurrentBufferSizeSamples();
    if (!std::isfinite(rate) || rate < 8000.0 || rate > 192000.0 || block <= 0 || block > 65536)
    {
        callbackSampleRate.store(0.0, std::memory_order_relaxed);
        errors.fetch_add(1, std::memory_order_relaxed);
        deviceGeneration.fetch_add (1, std::memory_order_release);
        return;
    }
    {
        callbackSampleRate.store(device->getCurrentSampleRate(),std::memory_order_relaxed);
        nativeSpectrum.prepare(device->getCurrentSampleRate());
        mixer.prepare (device->getCurrentSampleRate()); // allocations outside process callback
        compensation.prepare(); // bounded history allocation outside the audio callback
        // Never silently reactivate a FIR prepared for a different device clock.
        firBypassed.store(true,std::memory_order_release);
        stereoFir.clear();
        if(std::abs(firSampleRate-device->getCurrentSampleRate())>0.1)
            removeStereoFir();
        if (stereoPlugin != nullptr)
        {
            if (!releasePluginResourcesSafely())
            {
                deviceGeneration.fetch_add(1, std::memory_order_release);
                return; // logical engine remains stopped/muted until unload or a successful restart
            }
            try
            {
                stereoPlugin->setRateAndBufferSizeDetails (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
                pluginPrepared=true; // release partially prepared resources if prepare/reset throws
                stereoPlugin->prepareToPlay (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
                stereoPlugin->reset(); // off callback: discard old plug-in tails before explicit rearm
            }
            catch (...)
            {
                errors.fetch_add(1, std::memory_order_relaxed);
                releasePluginResourcesSafely();
                deviceGeneration.fetch_add(1, std::memory_order_release);
                return; // engine is not ready; no automatic dry audio after a failed restart
            }
            pluginResetRequired.store(false, std::memory_order_release);
            pluginPrepared = true;
            pluginMaximumBlockSize = std::min(block, 4096);
            pluginLatency.store (stereoPlugin->getLatencySamples(), std::memory_order_relaxed);
            pluginLatencyChanged.store(false, std::memory_order_release);
        }
    }
    // A new device does NOT clear a safety latch without operator acknowledgement.
    // A restarted/disconnected device never silently resumes speaker output.
    passEnabled.store (false, std::memory_order_release);
    running.store (true, std::memory_order_release);
    // Publish only after capture reset/preparation, so readers never establish a
    // new-session sample threshold from the old stream's committed count.
    deviceGeneration.fetch_add (1, std::memory_order_release);
}

void LiveAudioEngine::audioDeviceStopped()
{
    releasePluginResourcesSafely();
    deviceGeneration.fetch_add (1, std::memory_order_release);
    setPassThroughEnabled(false);
    running.store (false, std::memory_order_release);
    compensation.invalidate();
    compensationWasActive=false;
    appliedCompensation.store(0, std::memory_order_relaxed);
    inputCount.store (0, std::memory_order_relaxed);
    outputCount.store (0, std::memory_order_relaxed);
}

void LiveAudioEngine::audioDeviceError (const juce::String&)
{
    deviceGeneration.fetch_add (1, std::memory_order_release);
    setPassThroughEnabled(false);
    running.store (false, std::memory_order_release);
    appliedCompensation.store(0, std::memory_order_relaxed);
    errors.fetch_add (1, std::memory_order_relaxed);
}
} // namespace lah::audio

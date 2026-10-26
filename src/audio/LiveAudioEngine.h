#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <cstdint>
#include "../dsp/MixDspCore.h"
#include "../fir/FIRCore.h"
#include "../measurement/MeasurementCore.h"
#include "../reliability/StreamSafety.h"
#include "LatencyCompensator.h"

namespace lah::audio
{
class LiveAudioEngine final : public juce::AudioIODeviceCallback, private juce::AudioProcessorListener
{
public:
    static constexpr int visualChannelLimit = 64;
    static constexpr int maximumCallbackFrames = 65536;

    LiveAudioEngine();
    ~LiveAudioEngine() override;
    dsp::Controls& controls() noexcept { return parameters; }
    builtin::Controls& builtinControls() noexcept { return nativeParameters; }
    builtin::Controls& outputBuiltinControls() noexcept { return outputNativeParameters; }
    builtin::FeedbackStatus builtinFeedbackStatus(int channel,int slot)const noexcept {return isPassThroughEnabled()&&isDeviceRunning()?nativeRack.feedbackStatus(channel,slot):builtin::FeedbackStatus{};}
    float builtinFeedbackCandidateHz(int channel,int slot) const noexcept {return isPassThroughEnabled()&&isDeviceRunning()?nativeRack.feedbackCandidateHz(channel,slot):0;}
    builtin::FeedbackStatus outputBuiltinFeedbackStatus(int channel,int slot)const noexcept {return isPassThroughEnabled()&&isDeviceRunning()?outputNativeRack.feedbackStatus(channel,slot):builtin::FeedbackStatus{};}
    float outputBuiltinFeedbackCandidateHz(int channel,int slot) const noexcept {return isPassThroughEnabled()&&isDeviceRunning()?outputNativeRack.feedbackCandidateHz(channel,slot):0;}
    builtin::MeterLevels builtinMeterLevels(int channel,int slot,bool output=false)const noexcept {
        if(!isPassThroughEnabled()||!isDeviceRunning())return {};
        return output?outputNativeRack.meterLevels(channel,slot):nativeRack.meterLevels(channel,slot);
    }
    double getSampleRate()const noexcept{return callbackSampleRate.load(std::memory_order_relaxed);}
    dynamicspro::GainReduction builtinDynamicsProReduction(int channel,int slot,bool output=false)const noexcept {
        if(!isPassThroughEnabled()||!isDeviceRunning())return {};
        return output?outputNativeRack.dynamicsProReduction(channel,slot):nativeRack.dynamicsProReduction(channel,slot);
    }
    spectrum::Handle acquireBuiltinSpectrum(int channel,int slot,bool output=false){return nativeSpectrum.acquire(output,channel,slot);}
    void releaseBuiltinSpectrum(spectrum::Handle handle)noexcept{nativeSpectrum.release(handle);}
    bool builtinSpectrumSnapshot(spectrum::Handle handle,spectrum::Snapshot& result)const noexcept {
        if(!isPassThroughEnabled() || !isDeviceRunning()){result.hasData=false;result.signalPresent=false;result.generation=0;return false;}
        return nativeSpectrum.read(handle,result);
    }
    float consumeBusMasterPeak(int index)noexcept{return isPassThroughEnabled()&&isDeviceRunning()?mixer.consumeBusMasterPeak(index):0;}
    measurement::Capture& capture() noexcept { return inputCapture; }
    // Caller MUST remove this callback from AudioDeviceManager before mutating plug-in.
    bool installStereoPlugin (std::unique_ptr<juce::AudioPluginInstance>, double rate, int block, juce::String& error);
    void removePlugin();
    // GUI must first mute AND detach this callback from device manager.
    bool installStereoFir(const fir::Design& design,double deviceRate,juce::String& error);
    void removeStereoFir();
    bool hasStereoFir() const noexcept {return stereoFir.ready();}
    void setFirBypassed(bool value) noexcept {if(!isPassThroughEnabled()) firBypassed.store(value,std::memory_order_release);}
    bool isFirBypassed() const noexcept {return firBypassed.load(std::memory_order_acquire);}
    int getFirLatencySamples() const noexcept {return stereoFir.latencySamples();}
    // Reactivation requires a muted, detached/reprepared/reset insert. A plug-in
    // can retain delayed audio while skipped, so PANIC never resumes that state.
    void setPluginBypassed (bool bypass) noexcept {
        if (isPassThroughEnabled()) return;
        if (bypass) {
            pluginBypassed.store(true, std::memory_order_release);
            pluginResetRequired.store(true, std::memory_order_release);
        } else if (!pluginResetRequired.load(std::memory_order_acquire)) {
            pluginBypassed.store(false, std::memory_order_release);
        }
    }
    bool isPluginBypassed() const noexcept { return pluginBypassed.load(std::memory_order_relaxed); }
    bool isPluginResetRequired() const noexcept { return pluginResetRequired.load(std::memory_order_acquire); }
    juce::String getPluginName() const;
    int getPluginLatencySamples() const noexcept { return pluginLatency.load(std::memory_order_relaxed); }
    int getAppliedCompensationSamples() const noexcept { return appliedCompensation.load(std::memory_order_relaxed); }
    void setPassThroughEnabled (bool enabled) noexcept {
        const bool activate=enabled && isDeviceRunning() && !safety.isFaulted();
        if (!activate) {
            passEnabled.store(false, std::memory_order_release);
            nativeRack.invalidateMeters();outputNativeRack.invalidateMeters();
            mixer.invalidateBusMasterMeters();
            for(auto& channel:nativeParameters.slots)for(auto& slot:channel)
                slot.feedbackArmed.store(false,std::memory_order_release);
            for(auto& channel:outputNativeParameters.slots)for(auto& slot:channel)
                slot.feedbackArmed.store(false,std::memory_order_release);
            pluginBypassed.store(true, std::memory_order_release);
            pluginResetRequired.store(true, std::memory_order_release);
            muteGeneration.fetch_add(1, std::memory_order_release);
        } else {
            if (pluginResetRequired.load(std::memory_order_acquire))
                pluginBypassed.store(true, std::memory_order_release);
            passEnabled.store(true, std::memory_order_release);
        }
    }
    bool isPassThroughEnabled() const noexcept { return passEnabled.load (std::memory_order_acquire); }
    bool isDeviceRunning() const noexcept { return running.load (std::memory_order_acquire); }
    bool isSafetyLatched() const noexcept {return safety.isFaulted();}
    unsigned getConsecutiveProcessingOverruns() const noexcept {return safety.overrunStreak();}
    // Call only when audio has been MUTED; never automatically clears after device loss.
    void acknowledgeSafetyWhileMuted() noexcept {if(!isPassThroughEnabled()) safety.clearWhileMuted();}
    int getInputChannelCount() const noexcept { return inputCount.load (std::memory_order_relaxed); }
    int getOutputChannelCount() const noexcept { return outputCount.load (std::memory_order_relaxed); }
    std::uint64_t getCallbackCount() const noexcept { return callbacks.load (std::memory_order_relaxed); }
    std::uint64_t getDeadlineOverruns() const noexcept { return deadlineOverruns.load(std::memory_order_relaxed); }
    double getLongestCallbackMs() const noexcept { return maxCallbackNs.load(std::memory_order_relaxed)/1e6; }
    int getErrorCount() const noexcept { return errors.load (std::memory_order_relaxed); }
    std::uint64_t getDeviceGeneration() const noexcept { return deviceGeneration.load (std::memory_order_acquire); }
    float consumeInputPeak (int channel) noexcept;
    float consumeOutputPeak (int channel) noexcept;

    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void audioDeviceError (const juce::String&) override;

private:
    static void publishPeak (std::atomic<float>& target, float peak) noexcept;
    bool releasePluginResourcesSafely() noexcept; // detached control path only
    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged(juce::AudioProcessor*, const juce::AudioProcessorListener::ChangeDetails&) override;
    std::atomic<bool> passEnabled { false };
    std::atomic<std::uint64_t> muteGeneration {0};
    std::uint64_t consumedMuteGeneration {0}; // callback-owned, prepared while detached
    std::atomic<bool> running { false };
    std::atomic<int> inputCount { 0 }, outputCount { 0 };
    std::atomic<int> errors { 0 };
    std::atomic<std::uint64_t> deadlineOverruns {0},maxCallbackNs {0};
    std::atomic<double> callbackSampleRate {48000.0};
    std::atomic<std::uint64_t> callbacks { 0 };
    std::atomic<std::uint64_t> deviceGeneration { 0 };
    reliability::StreamSafety safety;
    dsp::Controls parameters;
    builtin::Controls nativeParameters;
    builtin::Controls outputNativeParameters;
    spectrum::Capture nativeSpectrum;
    builtin::Rack nativeRack;
    builtin::Rack outputNativeRack;
    dsp::Mixer mixer { parameters,&nativeRack,&nativeParameters,&outputNativeRack,&outputNativeParameters };
    measurement::Capture inputCapture;
    fir::StereoRealtimeFIR stereoFir;
    std::atomic<bool> firBypassed{true};
    bool firWasActive {false}; // callback-owned; state is reset before a new stream starts
    double firSampleRate=0.0;
    std::unique_ptr<juce::AudioPluginInstance> stereoPlugin;
    juce::AudioBuffer<float> pluginBuffer;
    juce::MidiBuffer pluginMidi;
    bool pluginPrepared {false}; // audio callback detached whenever plugin lifetime mutates
    int pluginMaximumBlockSize {0};
    std::atomic<bool> pluginBypassed {true};
    std::atomic<bool> pluginResetRequired {false};
    std::atomic<int> pluginLatency {0};
    std::atomic<bool> pluginLatencyChanged {false};
    LatencyCompensator compensation;
    bool compensationWasActive {false}; // callback-owned; reset while detached
    std::atomic<int> appliedCompensation {0};
    std::array<std::atomic<float>, visualChannelLimit> inputPeaks {};
    std::array<std::atomic<float>, visualChannelLimit> outputPeaks {};
};
} // namespace lah::audio

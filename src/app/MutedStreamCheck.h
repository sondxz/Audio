#pragma once
#include "../audio/LiveAudioEngine.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <algorithm>
#include <exception>
#include <functional>
#include <memory>

// Explicit opt-in observation of the default shared Windows device. This mode
// never enables pass-through, loads a plug-in, saves settings or writes samples.
class MutedStreamCheck final : private juce::Timer
{
public:
    MutedStreamCheck(int seconds,juce::File output,std::function<void(int)> completion)
        :durationSeconds(seconds),destination(std::move(output)),onComplete(std::move(completion)) {}
    ~MutedStreamCheck() override {stopTimer();close();}

    void start(){
        try{
            const auto error=manager.initialise(2,2,nullptr,false);
            if(error.isNotEmpty()){finish("Device initialise failed: "+error);return;}
            auto* device=manager.getCurrentAudioDevice();
            if(device==nullptr || device->getTypeName()!="Windows Audio"){
                finish("Default shared WASAPI device unavailable; no alternate backend was attempted.");return;
            }
            engine->setPassThroughEnabled(false);
            attached=true;manager.addAudioCallback(engine.get());
            startedAt=juce::Time::getMillisecondCounterHiRes();
            initialJuceXruns=manager.getXRunCount();
            initialDeviceXruns=device->getXRunCount();
            if(!engine->isDeviceRunning()){
                finish("Audio Engine did not start on the selected shared WASAPI device.");return;
            }
            startTimer(100);
        }catch(const std::exception& exception){
            finish("Stream setup C++ exception: "+juce::String::fromUTF8(exception.what()));
        }catch(...){finish("Stream setup raised an unknown C++ exception.");}
    }

private:
    class SharedWindowsDeviceManager final : public juce::AudioDeviceManager {
        void createAudioDeviceTypes(juce::OwnedArray<juce::AudioIODeviceType>& types) override {
            // Restrict the manager itself, rather than merely requesting a type
            // and allowing JUCE to fall back to ASIO or exclusive mode.
            if(auto* shared=juce::AudioIODeviceType::createAudioIODeviceType_WASAPI(juce::WASAPIDeviceMode::shared))
                types.add(shared);
        }
    };
    void sample(){
        remainedMuted=remainedMuted && !engine->isPassThroughEnabled();
        maximumCpuPercent=std::max(maximumCpuPercent,manager.getCpuUsage()*100.0);
        for(int channel=0;channel<lah::audio::LiveAudioEngine::visualChannelLimit;++channel)
            maximumOutputPeak=std::max(maximumOutputPeak,engine->consumeOutputPeak(channel));
    }
    void timerCallback() override {
        sample();
        if(!engine->isDeviceRunning() || manager.getCurrentAudioDevice()==nullptr){
            finish("Device stopped during the muted observation.");return;
        }
        if(juce::Time::getMillisecondCounterHiRes()-startedAt>=durationSeconds*1000.0)finish({});
    }
    void close() noexcept {
        engine->setPassThroughEnabled(false);
        if(attached){manager.removeAudioCallback(engine.get());attached=false;}
        manager.closeAudioDevice();
    }
    void finish(const juce::String& failure){
        if(finished)return;
        finished=true;stopTimer();sample();
        auto report=std::make_unique<juce::DynamicObject>();
        report->setProperty("version",LAH_VERSION_STRING);
        report->setProperty("timestamp",juce::Time::getCurrentTime().toISO8601(true));
        report->setProperty("scope","Muted default WASAPI shared-stream observation only. No audible pass-through, load test, measured round-trip latency, ASIO/M32 acceptance, calibrated SPL or endurance claim.");
        report->setProperty("requested_seconds",durationSeconds);
        const double elapsed=startedAt>0?(juce::Time::getMillisecondCounterHiRes()-startedAt)/1000.0:0.0;
        report->setProperty("observed_seconds",elapsed);
        report->setProperty("requested_input_channels",2);
        report->setProperty("requested_output_channels",2);
        const auto setup=manager.getAudioDeviceSetup();
        report->setProperty("input_device",setup.inputDeviceName);
        report->setProperty("output_device",setup.outputDeviceName);
        auto* device=manager.getCurrentAudioDevice();
        report->setProperty("backend",device?device->getTypeName():"none");
        report->setProperty("device",device?device->getName():"none");
        report->setProperty("sample_rate",device?device->getCurrentSampleRate():0.0);
        report->setProperty("buffer_samples",device?device->getCurrentBufferSizeSamples():0);
        report->setProperty("input_channels",engine->getInputChannelCount());
        report->setProperty("output_channels",engine->getOutputChannelCount());
        report->setProperty("driver_input_latency_samples",device?device->getInputLatencyInSamples():-1);
        report->setProperty("driver_output_latency_samples",device?device->getOutputLatencyInSamples():-1);
        const int juceXruns=manager.getXRunCount(),deviceXruns=device?device->getXRunCount():-1;
        report->setProperty("initial_juce_xruns",initialJuceXruns);
        report->setProperty("initial_device_xruns",initialDeviceXruns);
        report->setProperty("juce_xrun_delta",initialJuceXruns>=0 && juceXruns>=initialJuceXruns?juceXruns-initialJuceXruns:-1);
        report->setProperty("device_xrun_delta",initialDeviceXruns>=0 && deviceXruns>=initialDeviceXruns?deviceXruns-initialDeviceXruns:-1);
        report->setProperty("juce_xruns",juceXruns);
        report->setProperty("device_xruns",deviceXruns);
        report->setProperty("xrun_note","Negative means unavailable. These counters do not constitute an independent dropout measurement.");
        report->setProperty("callback_count",static_cast<juce::int64>(engine->getCallbackCount()));
        report->setProperty("deadline_overruns",static_cast<juce::int64>(engine->getDeadlineOverruns()));
        report->setProperty("maximum_callback_ms",engine->getLongestCallbackMs());
        report->setProperty("device_errors",engine->getErrorCount());
        report->setProperty("maximum_observed_cpu_percent",maximumCpuPercent);
        report->setProperty("final_cpu_percent",manager.getCpuUsage()*100.0);
        report->setProperty("maximum_output_peak",maximumOutputPeak);
        report->setProperty("output_peak_source","Audio Engine output peak counters, not an independent electrical or acoustic output measurement.");
        report->setProperty("audio_remained_muted",remainedMuted);
        report->setProperty("safety_latched",engine->isSafetyLatched());
        const bool completed=failure.isEmpty() && elapsed>=durationSeconds && device!=nullptr && engine->isDeviceRunning();
        const bool success=completed && engine->getCallbackCount()>0 && engine->getInputChannelCount()>0 &&
            engine->getOutputChannelCount()>0 && remainedMuted && maximumOutputPeak==0.0f &&
            engine->getErrorCount()==0 && engine->getDeadlineOverruns()==0 && !engine->isSafetyLatched() &&
            juceXruns<=0 && deviceXruns<=0;
        report->setProperty("completed",completed);
        report->setProperty("observation_criteria_met",success);
        report->setProperty("error",failure);
        close(); // Detach and close before serializing data or destroying the engine.
        report->setProperty("stream_closed_before_result",manager.getCurrentAudioDevice()==nullptr);
        report->setProperty("audio_sample_files_written",false);
        report->setProperty("application_settings_read_or_saved",false);
        const auto json=juce::JSON::toString(juce::var(report.release()),false);
        const bool written=destination.getParentDirectory().createDirectory() && destination.replaceWithText(json);
        onComplete(success && written?0:1);
    }
    int durationSeconds;
    int initialJuceXruns=-1,initialDeviceXruns=-1;
    juce::File destination;
    std::function<void(int)> onComplete;
    SharedWindowsDeviceManager manager;
    std::unique_ptr<lah::audio::LiveAudioEngine> engine=std::make_unique<lah::audio::LiveAudioEngine>();
    double startedAt=0.0,maximumCpuPercent=0.0;
    float maximumOutputPeak=0.0f;
    bool attached=false,remainedMuted=true,finished=false;
};

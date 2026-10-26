#pragma once
#include "../audio/LiveAudioEngine.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <functional>
#include <memory>

// Explicit M32/DN32 ASIO observation, always muted. Directly owns the chosen
// device so neither a default M-Audio driver nor WASAPI can be used as fallback.
class M32MutedStreamCheck final:private juce::Timer {
public:
    M32MutedStreamCheck(int channels,int seconds,juce::File file,std::function<void(int)> completion)
        :requestedChannels(channels),duration(seconds),destination(std::move(file)),onComplete(std::move(completion)){}
    ~M32MutedStreamCheck()override{stopTimer();close();}
    void start(){
        try{
           #if defined(JUCE_ASIO) && JUCE_ASIO
            asio.reset(juce::AudioIODeviceType::createAudioIODeviceType_ASIO());
           #endif
            if(!asio){finish("This build has no ASIO backend.");return;}
            asio->scanForDevices();available=asio->getDeviceNames();juce::StringArray candidates,live;
            for(const auto& name:available)if(name.containsIgnoreCase("DN32")||name.containsIgnoreCase("M32")){
                candidates.addIfNotAlreadyThere(name);if(name.containsIgnoreCase("LIVE"))live.addIfNotAlreadyThere(name);
            }
            if(live.size()==1)selected=live[0];else if(candidates.size()==1)selected=candidates[0];
            else{finish(candidates.isEmpty()?"No DN32/M32 ASIO driver found. No other device was opened.":"Multiple DN32/M32 ASIO devices found. Choose a driver in the application's Devices page.");return;}
            device.reset(asio->createDevice(selected,selected));
            if(!device){finish("The selected DN32/M32 ASIO driver could not create a device.");return;}
            const auto rates=device->getAvailableSampleRates();const double rate=rates.contains(48000.0)?48000.0:rates.contains(44100.0)?44100.0:0.0;
            if(rate==0){finish("DN32/M32 did not report 44.1 or 48 kHz. No stream was opened.");return;}
            const auto buffers=device->getAvailableBufferSizes();const int buffer=buffers.contains(128)?128:buffers.contains(256)?256:device->getDefaultBufferSize();
            juce::BigInteger mask;mask.setRange(0,requestedChannels,true);
            const auto error=device->open(mask,mask,rate,buffer);
            if(error.isNotEmpty()){finish("ASIO open failed: "+error);return;}
            opened=true;
            if(device->getActiveInputChannels()!=mask||device->getActiveOutputChannels()!=mask){finish("ASIO active masks do not match physical IN/OUT 1-N.");return;}
            engine->setPassThroughEnabled(false);initialXruns=device->getXRunCount();
            device->start(engine.get());started=true;
            if(!engine->isDeviceRunning()){finish("ASIO callback failed to start.");return;}
            startedAt=juce::Time::getMillisecondCounterHiRes();startTimer(100);
        }catch(const std::exception& e){finish("ASIO setup exception: "+juce::String::fromUTF8(e.what()));}
        catch(...){finish("ASIO setup raised an unknown exception.");}
    }
private:
    void sample(){
        stayedMuted=stayedMuted&&!engine->isPassThroughEnabled();
        for(int i=0;i<32;++i){inputPeaks[i]=std::max(inputPeaks[i],engine->consumeInputPeak(i));outputPeak=std::max(outputPeak,engine->consumeOutputPeak(i));}
    }
    void timerCallback()override{
        sample();if(!device||!device->isPlaying()||!engine->isDeviceRunning()){finish("ASIO stream stopped during observation.");return;}
        if(juce::Time::getMillisecondCounterHiRes()-startedAt>=duration*1000.0)finish({});
    }
    void close()noexcept{
        engine->setPassThroughEnabled(false);
        if(device){if(started){device->stop();started=false;}device->close();}
    }
    void finish(const juce::String& error){
        if(finished)return;finished=true;stopTimer();sample();
        auto result=std::make_unique<juce::DynamicObject>();
        const auto elapsed=startedAt>0?(juce::Time::getMillisecondCounterHiRes()-startedAt)/1000.0:0;
        const bool runningAtEnd=device&&device->isPlaying()&&engine->isDeviceRunning();
        const int actualInputs=engine->getInputChannelCount(),actualOutputs=engine->getOutputChannelCount();
        const double sampleRate=device?device->getCurrentSampleRate():0;
        const int block=device?device->getCurrentBufferSizeSamples():0;
        const auto inputMask=device?device->getActiveInputChannels().toString(2):juce::String();
        const auto outputMask=device?device->getActiveOutputChannels().toString(2):juce::String();
        const int inputLatency=device?device->getInputLatencyInSamples():-1,outputLatency=device?device->getOutputLatencyInSamples():-1;
        // Stop drains any in-flight callback before the final peaks/counters are
        // evaluated; a late error/overrun must not produce a passing report.
        if(device&&started){device->stop();started=false;}
        const int xruns=device?device->getXRunCount():-1;
        close();sample();
        const bool completed=error.isEmpty()&&elapsed>=duration&&runningAtEnd&&engine->getCallbackCount()>0;
        const bool success=completed&&stayedMuted&&outputPeak==0&&engine->getErrorCount()==0&&engine->getDeadlineOverruns()==0&&!engine->isSafetyLatched()&&(xruns<0||initialXruns<0||xruns==initialXruns);
        result->setProperty("timestamp",juce::Time::getCurrentTime().toISO8601(true));result->setProperty("backend","ASIO");
        result->setProperty("scope","Explicit DN32/M32 muted stream only; no audible return, electrical/acoustic output, processed loopback, measured round-trip or endurance claim.");
        juce::Array<juce::var> names;for(const auto& name:available)names.add(name);result->setProperty("available_asio_drivers",juce::var(names));
        result->setProperty("selected_driver",selected);result->setProperty("requested_channels",requestedChannels);result->setProperty("requested_seconds",duration);
        result->setProperty("observed_seconds",elapsed);result->setProperty("device_opened",opened);result->setProperty("completed",completed);
        result->setProperty("observation_criteria_met",success);result->setProperty("error",error);result->setProperty("audio_remained_muted",stayedMuted);
        result->setProperty("sample_rate",sampleRate);result->setProperty("buffer_samples",block);
        result->setProperty("input_mask",inputMask);result->setProperty("output_mask",outputMask);
        result->setProperty("input_channels",actualInputs);result->setProperty("output_channels",actualOutputs);
        result->setProperty("driver_input_latency_samples",inputLatency);result->setProperty("driver_output_latency_samples",outputLatency);
        result->setProperty("initial_device_xruns",initialXruns);result->setProperty("final_device_xruns",xruns);
        result->setProperty("xrun_note","Negative means unavailable; counters are not an independent dropout measurement.");
        result->setProperty("callbacks",static_cast<juce::int64>(engine->getCallbackCount()));result->setProperty("deadline_overruns",static_cast<juce::int64>(engine->getDeadlineOverruns()));
        result->setProperty("maximum_callback_ms",engine->getLongestCallbackMs());result->setProperty("device_errors",engine->getErrorCount());
        juce::Array<juce::var> peaks;for(int i=0;i<requestedChannels;++i)peaks.add(inputPeaks[i]);result->setProperty("input_peaks",juce::var(peaks));result->setProperty("maximum_output_peak",outputPeak);
        result->setProperty("stream_closed_before_result",!device||!device->isOpen());result->setProperty("settings_read_or_saved",false);result->setProperty("audio_sample_files_written",false);
        const bool saved=destination.getParentDirectory().createDirectory()&&destination.replaceWithText(juce::JSON::toString(juce::var(result.release()),false));
        onComplete(saved&&success?0:1);
    }
    int requestedChannels,duration,initialXruns=-1;juce::File destination;std::function<void(int)> onComplete;
    std::unique_ptr<juce::AudioIODeviceType> asio;std::unique_ptr<juce::AudioIODevice> device;
    std::unique_ptr<lah::audio::LiveAudioEngine> engine=std::make_unique<lah::audio::LiveAudioEngine>();
    juce::StringArray available;juce::String selected;std::array<float,32> inputPeaks{};float outputPeak=0;
    double startedAt=0;bool opened=false,started=false,finished=false,stayedMuted=true;
};

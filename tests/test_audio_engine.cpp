// Executes the actual JUCE callback with deterministic device/processor doubles.
// No hardware is opened. These tests do not establish ASIO or VST3 compatibility.
#include "../src/audio/LiveAudioEngine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#define REQUIRE(condition) do { if (!(condition)) { std::cerr << "FAILED line " << __LINE__ << ": " #condition "\n"; std::exit(1); } } while (false)

namespace {
bool insideCallback=false;
class TestDevice final : public juce::AudioIODevice {
public:
    TestDevice() : AudioIODevice("Synthetic callback device", "TEST ONLY") {}
    juce::StringArray getOutputChannelNames() override { return names("OUT"); }
    juce::StringArray getInputChannelNames() override { return names("IN"); }
    juce::Array<double> getAvailableSampleRates() override { return {44100.0, 48000.0, 96000.0}; }
    juce::Array<int> getAvailableBufferSizes() override { return {64, 128, 256, 512}; }
    int getDefaultBufferSize() override { return 128; }
    juce::String open(const juce::BigInteger&, const juce::BigInteger&, double fs, int frames) override { rate=fs; block=frames; return {}; }
    void close() override {}
    bool isOpen() override { return true; }
    void start(juce::AudioIODeviceCallback*) override {}
    void stop() override {}
    bool isPlaying() override { return true; }
    juce::String getLastError() override { return {}; }
    int getCurrentBufferSizeSamples() override { return block; }
    double getCurrentSampleRate() override { return rate; }
    int getCurrentBitDepth() override { return 32; }
    juce::BigInteger getActiveOutputChannels() const override { return activeChannels(); }
    juce::BigInteger getActiveInputChannels() const override { return activeChannels(); }
    int getOutputLatencyInSamples() override { return 0; }
    int getInputLatencyInSamples() override { return 0; }
    double rate=48000.0;
    int block=128;
    int channelCount=2;
private:
    juce::StringArray names(const char* prefix) const {
        juce::StringArray result;
        for(int channel=0;channel<channelCount;++channel)result.add(juce::String(prefix)+juce::String(channel+1));
        return result;
    }
    juce::BigInteger activeChannels() const {
        juce::BigInteger result;result.setRange(0,channelCount,true);return result;
    }
};

class TestPlugin final : public juce::AudioPluginInstance {
public:
    TestPlugin() { setPlayConfigDetails(2, 2, 48000.0, 128); }
    ~TestPlugin() override {if(destructionFlag) *destructionFlag=true;}
    const juce::String getName() const override {
        if(throwName) throw std::runtime_error("synthetic name failure");
        return "Synthetic gain processor";
    }
    void prepareToPlay(double, int frames) override {
        REQUIRE(!insideCallback);
        if(throwPrepare) throw std::runtime_error("synthetic prepare failure");
        preparedFrames=frames; prepared=true;
    }
    void releaseResources() override {
        REQUIRE(!insideCallback);
        prepared=false;
        if(throwRelease) throw std::runtime_error("synthetic release failure");
    }
    void reset() override {
        REQUIRE(!insideCallback); // third-party reset is not a realtime operation
        if(throwReset) throw std::runtime_error("synthetic reset failure");
        for(auto& channel:delayHistory) channel.fill(0.0f);
        delayPosition={0,0};
        ++resetCalls;
    }
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override {
        REQUIRE(prepared);
        REQUIRE(buffer.getNumChannels()==2);
        REQUIRE(buffer.getNumSamples()>0 && buffer.getNumSamples()<=preparedFrames);
        const auto* address=buffer.getReadPointer(0);
        if (firstAddress==nullptr) firstAddress=address;
        REQUIRE(address==firstAddress); // scratch storage must survive shrink/growth
        largestBlock=std::max(largestBlock,buffer.getNumSamples());
        totalFrames+=buffer.getNumSamples(); ++processCalls;
        if(throwDuringProcess && (throwOnCall==0 || processCalls==throwOnCall))
            throw std::runtime_error("synthetic processor failure");
        if(changeLatencyDuringProcess) {
            changeLatencyDuringProcess=false;
            setLatencySamples(getLatencySamples()+16);
        }
        for (int ch=0;ch<2;++ch)
            for (int i=0;i<buffer.getNumSamples();++i) {
                float output=buffer.getSample(ch,i)*gain;
                if(delayMode) {
                    auto& position=delayPosition[static_cast<std::size_t>(ch)];
                    auto& history=delayHistory[static_cast<std::size_t>(ch)];
                    output=history[position]; history[position]=buffer.getSample(ch,i);
                    position=(position+1)%history.size();
                }
                if(nonFinite) output=i%2==0 ? std::numeric_limits<float>::infinity()
                                           : std::numeric_limits<float>::quiet_NaN();
                buffer.setSample(ch,i,output);
            }
    }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*,int) override {}
    void fillInPluginDescription(juce::PluginDescription& d) const override { d.name=getName(); }
    float gain=8.0f;
    bool nonFinite=false,prepared=false,delayMode=false,changeLatencyDuringProcess=false,throwDuringProcess=false;
    bool throwPrepare=false,throwReset=false,throwRelease=false,throwName=false;
    bool* destructionFlag=nullptr;
    int preparedFrames=0,largestBlock=0,totalFrames=0,processCalls=0,resetCalls=0,throwOnCall=0;
    const float* firstAddress=nullptr;
    std::array<std::array<float,64>,2> delayHistory{};
    std::array<std::size_t,2> delayPosition{};
};

struct Buffers {
    explicit Buffers(int frameCapacity=8192, int outputCount=40)
      : left(static_cast<std::size_t>(frameCapacity),0.1f), right(static_cast<std::size_t>(frameCapacity),-0.2f),
        data(static_cast<std::size_t>(outputCount),std::vector<float>(static_cast<std::size_t>(frameCapacity),9.0f)),
        outputs(static_cast<std::size_t>(outputCount)) {
        inputs={left.data(),right.data()};
        for (std::size_t ch=0;ch<data.size();++ch) outputs[ch]=data[ch].data();
    }
    void process(lah::audio::LiveAudioEngine& engine,int frames) {
        insideCallback=true;
        engine.audioDeviceIOCallbackWithContext(inputs.data(),2,outputs.data(),static_cast<int>(outputs.size()),frames,{});
        insideCallback=false;
    }
    void requireSilent(int frames) const {
        for(const auto& ch:data)
            for(int i=0;i<frames;++i) REQUIRE(ch[static_cast<std::size_t>(i)]==0.0f);
    }
    std::vector<float> left,right;
    std::vector<std::vector<float>> data;
    std::array<const float*,2> inputs{};
    std::vector<float*> outputs;
};

void testLifecycleAndRouting() {
    auto engine=std::make_unique<lah::audio::LiveAudioEngine>();
    TestDevice device;
    Buffers b;
    REQUIRE(!engine->isDeviceRunning());
    engine->setPassThroughEnabled(true);
    REQUIRE(!engine->isPassThroughEnabled());
    engine->audioDeviceAboutToStart(nullptr);
    REQUIRE(!engine->isDeviceRunning());
    engine->audioDeviceAboutToStart(&device);
    REQUIRE(engine->isDeviceRunning() && !engine->isPassThroughEnabled());
    b.process(*engine,128);
    b.requireSilent(128);
    REQUIRE(std::abs(engine->consumeInputPeak(0)-0.1f)<1e-6f);
    engine->setPassThroughEnabled(true);
    b.process(*engine,128);
    REQUIRE(std::abs(b.data[0][127]-0.1f)<1e-5f);
    REQUIRE(std::abs(b.data[1][127]+0.2f)<1e-5f);
    for(std::size_t ch=2;ch<b.data.size();++ch)
        for(int i=0;i<128;++i) REQUIRE(b.data[ch][static_cast<std::size_t>(i)]==0.0f);
    REQUIRE(engine->getInputChannelCount()==2 && engine->getOutputChannelCount()==40);
    const auto generation=engine->getDeviceGeneration();
    engine->audioDeviceError("synthetic disconnect");
    REQUIRE(engine->getErrorCount()==1 && !engine->isDeviceRunning() && !engine->isPassThroughEnabled());
    REQUIRE(engine->getDeviceGeneration()>generation);
    engine->setPassThroughEnabled(true);
    REQUIRE(!engine->isPassThroughEnabled());
    b.process(*engine,128);
    b.requireSilent(128);
    engine->audioDeviceAboutToStart(&device);
    REQUIRE(engine->isDeviceRunning() && !engine->isPassThroughEnabled());
    engine->audioDeviceStopped();
    REQUIRE(!engine->isDeviceRunning() && engine->getOutputChannelCount()==0);
    for(double invalidRate:{0.0,384000.0,std::numeric_limits<double>::quiet_NaN()}) {
        device.rate=invalidRate;
        engine->audioDeviceAboutToStart(&device);
        REQUIRE(!engine->isDeviceRunning());
    }
    device.rate=48000.0; device.block=0;
    engine->audioDeviceAboutToStart(&device);
    REQUIRE(!engine->isDeviceRunning());
}

void testFirPanicAndRestart() {
    auto engine=std::make_unique<lah::audio::LiveAudioEngine>();
    TestDevice device;
    Buffers b;
    engine->audioDeviceAboutToStart(&device);
    juce::String error;
    const auto design=lah::fir::make(lah::fir::Mode::Lowpass,4000,48000,257);
    REQUIRE(engine->installStereoFir(design,48000,error));
    REQUIRE(engine->isFirBypassed() && engine->getFirLatencySamples()==128);
    engine->setFirBypassed(false);
    std::fill(b.left.begin(),b.left.end(),0.0f); std::fill(b.right.begin(),b.right.end(),0.0f);
    b.left[0]=0.5f; b.right[0]=-0.5f;
    engine->setPassThroughEnabled(true);
    b.process(*engine,32); // leave most impulse history inside the FIR
    engine->setPassThroughEnabled(false);
    b.left[0]=b.right[0]=0;
    b.process(*engine,32);
    b.requireSilent(32);
    engine->setPassThroughEnabled(true);
    b.process(*engine,512);
    b.requireSilent(512); // must not release the pre-PANIC FIR impulse
    engine->audioDeviceAboutToStart(&device);
    REQUIRE(engine->isFirBypassed() && !engine->isPassThroughEnabled());
    REQUIRE(engine->hasStereoFir());
    device.rate=44100.0;
    engine->audioDeviceAboutToStart(&device);
    REQUIRE(!engine->hasStereoFir());
    engine->audioDeviceStopped();
}

void testPluginBoundary() {
    auto engine=std::make_unique<lah::audio::LiveAudioEngine>();
    TestDevice device;
    Buffers b;
    engine->audioDeviceAboutToStart(&device);
    auto plugin=std::make_unique<TestPlugin>();
    auto* probe=plugin.get();
    juce::String error;
    REQUIRE(engine->installStereoPlugin(std::move(plugin),48000,128,error));
    REQUIRE(engine->isPluginBypassed());
    engine->controls().outputCeiling[0].store(-12.0f);
    engine->controls().outputCeiling[1].store(-18.0f);
    engine->setPluginBypassed(false);
    engine->setPassThroughEnabled(true);
    int expectedFrames=0;
    for(int frames:{64,128,1,256,4097,8192}) {
        b.process(*engine,frames);
        expectedFrames+=frames;
        const float capL=std::pow(10.0f,-12.0f/20.0f),capR=std::pow(10.0f,-18.0f/20.0f);
        for(int i=0;i<frames;++i) {
            REQUIRE(std::abs(b.data[0][static_cast<std::size_t>(i)]-capL)<1e-6f);
            REQUIRE(std::abs(b.data[1][static_cast<std::size_t>(i)]+capR)<1e-6f);
        }
    }
    REQUIRE(probe->totalFrames==expectedFrames && probe->largestBlock==128);
    REQUIRE(!engine->isSafetyLatched());
    probe->nonFinite=true;
    b.process(*engine,128);
    b.requireSilent(128);
    engine->audioDeviceAboutToStart(&device);
    REQUIRE(engine->isPluginBypassed() && !engine->isPassThroughEnabled());
    REQUIRE(probe->prepared);
    engine->audioDeviceStopped();
    REQUIRE(!probe->prepared);
    engine->removePlugin();
    REQUIRE(engine->getPluginName()=="None" && engine->getPluginLatencySamples()==0);
    REQUIRE(!engine->installStereoPlugin(std::make_unique<TestPlugin>(),0,128,error));
}

void testInvalidCallbackGeometry() {
    auto engine=std::make_unique<lah::audio::LiveAudioEngine>();
    TestDevice device;
    Buffers b(lah::audio::LiveAudioEngine::maximumCallbackFrames+1,2);
    engine->audioDeviceAboutToStart(&device);
    engine->setPassThroughEnabled(true);
    b.process(*engine,lah::audio::LiveAudioEngine::maximumCallbackFrames+1);
    b.requireSilent(lah::audio::LiveAudioEngine::maximumCallbackFrames+1);
    REQUIRE(engine->isSafetyLatched() && !engine->isPassThroughEnabled());
    engine->acknowledgeSafetyWhileMuted();
    REQUIRE(!engine->isSafetyLatched());
    b.process(*engine,0);
    REQUIRE(engine->isSafetyLatched());
    engine->audioDeviceStopped();
}

void testPluginPanicHistory() {
    auto engine=std::make_unique<lah::audio::LiveAudioEngine>();
    TestDevice device;
    Buffers b;
    engine->audioDeviceAboutToStart(&device);
    auto plugin=std::make_unique<TestPlugin>();
    auto* probe=plugin.get(); probe->delayMode=true;
    probe->setLatencySamples(64);
    engine->controls().route[2*lah::dsp::channels].store(1);
    juce::String error;
    REQUIRE(engine->installStereoPlugin(std::move(plugin),48000,128,error));
    REQUIRE(probe->resetCalls==1);
    engine->setPluginBypassed(false);
    engine->setPassThroughEnabled(true);
    engine->setPluginBypassed(true); // a live toggle must be refused
    REQUIRE(!engine->isPluginBypassed());
    std::fill(b.left.begin(),b.left.end(),0.0f); std::fill(b.right.begin(),b.right.end(),0.0f);
    b.left[0]=0.5f; b.right[0]=-0.25f;
    b.process(*engine,32); // leave delayed impulse queued inside the plug-in
    b.requireSilent(32);
    engine->setPassThroughEnabled(false);
    REQUIRE(engine->isPluginBypassed() && engine->isPluginResetRequired());
    b.left[0]=b.right[0]=0;
    b.process(*engine,32);
    engine->setPluginBypassed(false); // dirty state cannot be explicitly re-enabled
    REQUIRE(engine->isPluginBypassed());
    engine->setPassThroughEnabled(true);
    b.process(*engine,128);
    b.requireSilent(128);
    REQUIRE(probe->processCalls==1); // global audio rearm does not resume a frozen tail
    engine->setPassThroughEnabled(false);
    engine->audioDeviceStopped();
    engine->audioDeviceAboutToStart(&device); // detached prepare/reset path
    REQUIRE(!engine->isPluginResetRequired() && probe->resetCalls==2);
    engine->setPluginBypassed(false);
    engine->setPassThroughEnabled(true);
    b.process(*engine,128);
    b.requireSilent(128); // explicitly reset/reactivated insert also has no old tail
    engine->audioDeviceStopped();
}

void testStaticLatencyCompensation() {
    auto engine=std::make_unique<lah::audio::LiveAudioEngine>();
    TestDevice device;
    Buffers b;
    engine->audioDeviceAboutToStart(&device);
    engine->controls().route[2*lah::dsp::channels].store(1);
    auto plugin=std::make_unique<TestPlugin>();
    auto* probe=plugin.get(); probe->delayMode=true; probe->setLatencySamples(64);
    juce::String error;
    REQUIRE(engine->installStereoPlugin(std::move(plugin),48000,128,error));
    engine->setPluginBypassed(false);
    std::fill(b.left.begin(),b.left.end(),0.0f); std::fill(b.right.begin(),b.right.end(),0.0f);
    b.left[0]=b.right[0]=0.5f;
    engine->setPassThroughEnabled(true);
    b.process(*engine,256);
    REQUIRE(engine->getAppliedCompensationSamples()==64);
    for(int ch=0;ch<3;++ch)
        for(int sample=0;sample<256;++sample)
            REQUIRE(std::abs(b.data[static_cast<std::size_t>(ch)][static_cast<std::size_t>(sample)]
                             -(sample==64 ? 0.5f : 0.0f))<1e-6f);

    // Restart outside the callback, then combine FIR64 + reported plugin64.
    engine->setPassThroughEnabled(false);
    engine->audioDeviceStopped();
    engine->audioDeviceAboutToStart(&device);
    REQUIRE(engine->installStereoFir(lah::fir::make(lah::fir::Mode::Lowpass,4000,48000,129),48000,error));
    engine->setFirBypassed(false);
    engine->setPluginBypassed(false);
    engine->setPassThroughEnabled(true);
    b.process(*engine,512);
    REQUIRE(engine->getAppliedCompensationSamples()==128);
    for(int ch=0;ch<3;++ch) {
        const auto& data=b.data[static_cast<std::size_t>(ch)];
        const auto peak=std::max_element(data.begin(),data.begin()+512,
                                        [](float a,float value){return std::abs(a)<std::abs(value);});
        REQUIRE(std::distance(data.begin(),peak)==128);
    }
    // A processor latency notification while live must silence before publishing
    // a block with cross-channel alignment based on the previous value.
    probe->setLatencySamples(80);
    b.process(*engine,128);
    b.requireSilent(128);
    REQUIRE(!engine->isPassThroughEnabled() && engine->isPluginResetRequired());
    REQUIRE(engine->getPluginLatencySamples()==80 && engine->getAppliedCompensationSamples()==0);
    engine->audioDeviceStopped();
}

void testLatencyChangeDuringProcessAndBound() {
    auto engine=std::make_unique<lah::audio::LiveAudioEngine>();
    TestDevice device;
    Buffers b;
    engine->audioDeviceAboutToStart(&device);
    auto plugin=std::make_unique<TestPlugin>();
    auto* probe=plugin.get(); probe->delayMode=true; probe->setLatencySamples(64);
    probe->changeLatencyDuringProcess=true;
    juce::String error;
    REQUIRE(engine->installStereoPlugin(std::move(plugin),48000,128,error));
    engine->setPluginBypassed(false);
    engine->setPassThroughEnabled(true);
    b.process(*engine,128);
    b.requireSilent(128);
    REQUIRE(probe->processCalls==1 && !engine->isPassThroughEnabled());
    REQUIRE(engine->isPluginResetRequired());
    engine->audioDeviceStopped();
    engine->audioDeviceAboutToStart(&device);
    auto unsupported=std::make_unique<TestPlugin>();
    unsupported->setLatencySamples(lah::audio::LatencyCompensator::maximumSupportedDelay+1);
    REQUIRE(engine->installStereoPlugin(std::move(unsupported),48000,128,error));
    engine->setPluginBypassed(false);
    engine->setPassThroughEnabled(true);
    b.process(*engine,128);
    b.requireSilent(128);
    REQUIRE(!engine->isPassThroughEnabled() && engine->getAppliedCompensationSamples()==0);
    engine->audioDeviceStopped();
}

void testRapidPanicRearm() {
    auto engine=std::make_unique<lah::audio::LiveAudioEngine>();
    TestDevice device;
    Buffers b;
    engine->audioDeviceAboutToStart(&device);
    auto& controls=engine->controls();
    controls.route[2*lah::dsp::channels].store(1);
    controls.outputDelayMs[0].store(2.0f); // queued speaker-delay impulse
    controls.fxSend[0].store(1.0f);
    controls.delayReturn.store(1.0f); controls.reverbReturn.store(1.0f);
    controls.fxDelayMs.store(10.0f); controls.fxFeedback.store(0.7f);
    juce::String error;
    REQUIRE(engine->installStereoFir(lah::fir::make(lah::fir::Mode::Lowpass,4000,48000,257),48000,error));
    engine->setFirBypassed(false);
    std::fill(b.left.begin(),b.left.end(),0.0f); std::fill(b.right.begin(),b.right.end(),0.0f);
    b.left[0]=0.5f; b.right[0]=0.25f;
    engine->setPassThroughEnabled(true);
    b.process(*engine,32); // pending speaker delay / FIR / PDC / delay-reverb state
    REQUIRE(engine->getAppliedCompensationSamples()==128);
    b.left[0]=b.right[0]=0;
    engine->setPassThroughEnabled(false);
    engine->setPassThroughEnabled(true); // no callback observes passEnabled==false
    b.process(*engine,128);
    b.requireSilent(128);
    for(int block=0;block<32;++block) {
        b.process(*engine,128);
        b.requireSilent(128);
    }
    engine->audioDeviceStopped();
}

void testPluginExceptionRecovery() {
    auto engine=std::make_unique<lah::audio::LiveAudioEngine>();
    TestDevice device;
    Buffers b;
    engine->audioDeviceAboutToStart(&device);
    auto plugin=std::make_unique<TestPlugin>();
    auto* probe=plugin.get(); probe->throwDuringProcess=true; probe->throwOnCall=2;
    juce::String error;
    REQUIRE(engine->installStereoPlugin(std::move(plugin),48000,128,error));
    engine->setPluginBypassed(false);
    engine->setPassThroughEnabled(true);
    b.process(*engine,256); // a C++ exception must not escape the device callback
    b.requireSilent(256);
    REQUIRE(!engine->isPassThroughEnabled() && engine->isPluginBypassed());
    REQUIRE(engine->isPluginResetRequired() && engine->getErrorCount()==1);
    REQUIRE(engine->getAppliedCompensationSamples()==0);
    probe->throwDuringProcess=false;
    engine->setPluginBypassed(false);
    REQUIRE(engine->isPluginBypassed());
    engine->audioDeviceStopped();
    engine->audioDeviceAboutToStart(&device);
    REQUIRE(!engine->isPluginResetRequired());
    engine->setPluginBypassed(false);
    engine->setPassThroughEnabled(true);
    b.process(*engine,128);
    REQUIRE(std::abs(b.data[0][127]-0.8f)<1e-6f);
    REQUIRE(b.data[1][127]==-1.0f);
    engine->audioDeviceStopped();
}

void testPluginLifecycleExceptions() {
    TestDevice device;
    juce::String error;
    for(int operation=0;operation<2;++operation) {
        auto engine=std::make_unique<lah::audio::LiveAudioEngine>();
        engine->audioDeviceAboutToStart(&device);
        auto failing=std::make_unique<TestPlugin>();
        failing->throwPrepare=operation==0; failing->throwReset=operation==1;
        REQUIRE(!engine->installStereoPlugin(std::move(failing),48000,128,error));
        REQUIRE(error.isNotEmpty() && engine->getPluginName()=="None");
        REQUIRE(!engine->isPassThroughEnabled() && engine->isPluginBypassed());
        REQUIRE(engine->getErrorCount()>=1);
        engine->audioDeviceStopped();
    }
    for(int operation=0;operation<3;++operation) {
        auto engine=std::make_unique<lah::audio::LiveAudioEngine>();
        engine->audioDeviceAboutToStart(&device);
        auto plugin=std::make_unique<TestPlugin>();
        auto* probe=plugin.get();
        REQUIRE(engine->installStereoPlugin(std::move(plugin),48000,128,error));
        probe->throwPrepare=operation==0; probe->throwReset=operation==1; probe->throwRelease=operation==2;
        engine->audioDeviceAboutToStart(&device);
        REQUIRE(!engine->isDeviceRunning() && !engine->isPassThroughEnabled());
        REQUIRE(engine->isPluginBypassed() && engine->isPluginResetRequired());
        REQUIRE(engine->getErrorCount()>=1);
        engine->setPassThroughEnabled(true);
        REQUIRE(!engine->isPassThroughEnabled());
        probe->throwPrepare=probe->throwReset=probe->throwRelease=false;
        engine->audioDeviceAboutToStart(&device);
        REQUIRE(engine->isDeviceRunning() && !engine->isPassThroughEnabled());
        probe->throwName=true;
        REQUIRE(engine->getPluginName().contains("name error"));
        probe->throwName=false;
        probe->throwRelease=true;
        engine->audioDeviceStopped();
        REQUIRE(!engine->isDeviceRunning() && !engine->isPassThroughEnabled());
        engine->removePlugin();
        REQUIRE(engine->getPluginName()=="None");
    }
    bool destroyed=false;
    {
        auto engine=std::make_unique<lah::audio::LiveAudioEngine>();
        engine->audioDeviceAboutToStart(&device);
        auto plugin=std::make_unique<TestPlugin>();
        plugin->destructionFlag=&destroyed; plugin->throwRelease=true;
        REQUIRE(engine->installStereoPlugin(std::move(plugin),48000,128,error));
        // A detached owner may destroy a prepared processor without a preceding
        // stopped callback; releaseResources' C++ exception still cannot escape.
    }
    REQUIRE(destroyed);
}
void testNativeRackCallbackAndMute(){
    auto engine=std::make_unique<lah::audio::LiveAudioEngine>();TestDevice device;Buffers b;
    auto& slot=engine->builtinControls().slots[0][0];
    lah::builtin::configureSlot(slot,lah::builtin::Dynamics,true);
    slot.params[0].store(-12);slot.params[1].store(4);slot.params[2].store(.2f);slot.params[3].store(10);
    slot.params[4].store(-100);slot.params[5].store(0);
    engine->audioDeviceAboutToStart(&device);b.process(*engine,128);
    std::fill(b.left.begin(),b.left.end(),.5f);std::fill(b.right.begin(),b.right.end(),.1f);
    engine->setPassThroughEnabled(true);
    for(int block=0;block<8;++block)b.process(*engine,128);
    const double threshold=std::pow(10.0,-12.0/20),expected=.5*std::pow(threshold/.5,.75);
    REQUIRE(std::abs(b.data[0][127]-expected)<2e-5);
    REQUIRE(std::abs(b.data[1][127]-.1f)<1e-6); // other input remains independent
    slot.enabled.store(false);b.process(*engine,128);
    REQUIRE(std::abs(b.data[0][127]-.5f)<1e-6);
    // Exercise native delay through the real matrix/callback, not a direct core call.
    engine->setPassThroughEnabled(false);lah::builtin::configureSlot(slot,lah::builtin::Fx,true);
    slot.params[0].store(100);slot.params[1].store(10);slot.params[2].store(0);slot.params[4].store(0);slot.params[5].store(0);
    std::fill(b.left.begin(),b.left.end(),0);std::fill(b.right.begin(),b.right.end(),0);b.process(*engine,128);
    engine->setPassThroughEnabled(true);b.left[0]=.5f;b.process(*engine,128);b.left[0]=0;
    b.process(*engine,128);b.process(*engine,128);b.process(*engine,128);
    REQUIRE(std::abs(b.data[0][96]-.5f)<1e-6); // 10ms =480 samples at48kHz
    b.left[0]=.5f;b.process(*engine,128);b.left[0]=0;
    engine->controls().inputMute[0].store(1);b.process(*engine,128);b.requireSilent(128);
    engine->controls().inputMute[0].store(0);
    for(int block=0;block<8;++block){b.process(*engine,128);b.requireSilent(128);}
    b.left[0]=.5f;b.process(*engine,128);b.left[0]=0;
    engine->setPassThroughEnabled(false);engine->setPassThroughEnabled(true);
    for(int block=0;block<8;++block){b.process(*engine,128);b.requireSilent(128);}
    engine->audioDeviceStopped();
}
void testAutomaticFeedbackCallbackAndPanic(){
    auto engine=std::make_unique<lah::audio::LiveAudioEngine>();TestDevice device;Buffers b;
    auto& slot=engine->builtinControls().slots[0][0];
    lah::builtin::configureSlot(slot,lah::builtin::AutoFeedback,true);slot.params[3].store(350);
    engine->audioDeviceAboutToStart(&device);b.process(*engine,128);engine->setPassThroughEnabled(true);
    b.process(*engine,128);slot.feedbackArmed.store(true);
    bool sawCut=false;double dry=0,wet=0;
    for(int block=0;block<1500;++block){
        for(int frame=0;frame<128;++frame){const int n=block*128+frame;const double amplitude=std::min(.25,.002*std::pow(10.,n/48000.*12/20));b.left[static_cast<std::size_t>(frame)]=static_cast<float>(amplitude*std::sin(2*lah::builtin::pi*1234.56*n/48000));b.right[static_cast<std::size_t>(frame)]=.05f;}
        b.process(*engine,128);
        sawCut=sawCut||engine->builtinFeedbackStatus(0,0).notchCount>0;
        REQUIRE(std::abs(b.data[1][127]-.05f)<1e-6f);
        if(block>1300)for(int frame=0;frame<128;++frame){dry+=b.left[static_cast<std::size_t>(frame)]*b.left[static_cast<std::size_t>(frame)];wet+=b.data[0][static_cast<std::size_t>(frame)]*b.data[0][static_cast<std::size_t>(frame)];}
    }
    REQUIRE(sawCut&&wet<dry*.2);
    REQUIRE(engine->builtinFeedbackStatus(0,0).notchCount>0);
    REQUIRE(engine->builtinFeedbackCandidateHz(0,0)>0);
    engine->setPassThroughEnabled(false);REQUIRE(!engine->builtinFeedbackStatus(0,0).armed);
    REQUIRE(engine->builtinFeedbackCandidateHz(0,0)==0);
    engine->setPassThroughEnabled(true);
    REQUIRE(engine->builtinFeedbackStatus(0,0).notchCount==0);
    REQUIRE(engine->builtinFeedbackStatus(0,0).candidateHz==0);
    REQUIRE(engine->builtinFeedbackCandidateHz(0,0)==0);
    b.process(*engine,128);b.requireSilent(128);
    REQUIRE(!slot.feedbackArmed.load()&&engine->builtinFeedbackStatus(0,0).notchCount==0);
    // Input mute also clears every learned cut and requires an explicit re-arm.
    slot.feedbackArmed.store(true);b.process(*engine,128);
    engine->controls().inputMute[0].store(1);b.process(*engine,128);
    REQUIRE(!slot.feedbackArmed.load()&&engine->builtinFeedbackStatus(0,0).notchCount==0);
    engine->audioDeviceStopped();
}

void testEightUsbReturnsAndOutputRacks(){
    auto engine=std::make_unique<lah::audio::LiveAudioEngine>();
    TestDevice device;device.channelCount=8;
    std::array<std::array<float,128>,8> inputs{},outputs{};
    std::array<const float*,8> inputPointers{};std::array<float*,8> outputPointers{};
    for(int channel=0;channel<8;++channel){
        inputs[static_cast<std::size_t>(channel)].fill(.05f*static_cast<float>(channel+1));
        inputPointers[static_cast<std::size_t>(channel)]=inputs[static_cast<std::size_t>(channel)].data();
        outputPointers[static_cast<std::size_t>(channel)]=outputs[static_cast<std::size_t>(channel)].data();
    }
    auto process=[&]{
        insideCallback=true;
        engine->audioDeviceIOCallbackWithContext(inputPointers.data(),8,outputPointers.data(),8,128,{});
        insideCallback=false;
    };
    auto configureGain=[](lah::builtin::SlotControl& slot,float makeup){
        lah::builtin::configureSlot(slot,lah::builtin::Dynamics,true);
        slot.params[0].store(0);slot.params[1].store(1);slot.params[4].store(-100);slot.params[5].store(makeup);
    };
    configureGain(engine->builtinControls().slots[2][0],-6);
    configureGain(engine->outputBuiltinControls().slots[3][0],-6);
    configureGain(engine->outputBuiltinControls().slots[3][1],-3);
    auto& compressor=engine->outputBuiltinControls().slots[7][0];
    lah::builtin::configureSlot(compressor,lah::builtin::Dynamics,true);
    compressor.params[0].store(-12);compressor.params[1].store(4);compressor.params[2].store(.2f);
    compressor.params[3].store(10);compressor.params[4].store(-100);compressor.params[5].store(0);
    engine->controls().outputGain[7].store(-12);
    engine->audioDeviceAboutToStart(&device);process();
    engine->setPassThroughEnabled(true);
    for(int block=0;block<10;++block)process();
    const float threshold=std::pow(10.0f,-12.0f/20);
    for(int channel=0;channel<8;++channel){
        float expected=.05f*static_cast<float>(channel+1);
        if(channel==2)expected*=std::pow(10.0f,-6.0f/20);
        if(channel==3)expected*=std::pow(10.0f,-9.0f/20);
        if(channel==7)expected*=std::pow(threshold/expected,.75f)*threshold;
        REQUIRE(std::abs(outputs[static_cast<std::size_t>(channel)][127]-expected)<2e-5f);
    }
    REQUIRE(engine->getInputChannelCount()==8&&engine->getOutputChannelCount()==8);
    REQUIRE(engine->getSampleRate()==48000);
    const auto inputEight=engine->builtinMeterLevels(7,0);
    const auto outputEight=engine->builtinMeterLevels(7,0,true);
    REQUIRE(inputEight.generation!=0&&outputEight.generation!=0);
    REQUIRE(std::abs(inputEight.inputPeak-.4f)<1e-6f&&std::abs(inputEight.outputPeak-.4f)<1e-6f);
    REQUIRE(std::abs(outputEight.inputPeak-.4f)<1e-6f);
    REQUIRE(std::abs(outputEight.outputPeak-.4f*std::pow(threshold/.4f,.75f))<2e-5f);
    REQUIRE(std::abs(engine->builtinMeterLevels(2,0).outputPeak-.15f*std::pow(10.0f,-6.0f/20))<1e-6f);
    // Native editor telemetry never steals the main console's consuming meters.
    REQUIRE(std::abs(engine->consumeInputPeak(7)-.4f)<1e-6f);
    REQUIRE(engine->consumeInputPeak(7)==0);
    REQUIRE(engine->builtinMeterLevels(7,0).generation==inputEight.generation);
    REQUIRE(engine->builtinMeterLevels(7,0).inputPeak==inputEight.inputPeak);
    engine->outputBuiltinControls().slots[3][0].enabled.store(false);
    for(int block=0;block<3;++block)process();
    REQUIRE(std::abs(engine->builtinMeterLevels(3,0,true).inputPeak-.2f)<1e-6f);
    REQUIRE(std::abs(engine->builtinMeterLevels(3,0,true).outputPeak-.2f)<1e-6f);
    // Re-route two dry inputs into USB return 8. Its compressor must receive
    // their sum, independently of input identity and the other seven returns.
    engine->controls().route[7*32+7].store(0);
    engine->controls().route[7*32+0].store(1);
    engine->controls().route[7*32+1].store(1);
    for(int block=0;block<80;++block)process();
    REQUIRE(std::abs(outputs[7][127]-.15f*threshold)<2e-5f);
    REQUIRE(std::abs(outputs[0][127]-.05f)<1e-6f);
    REQUIRE(std::abs(outputs[1][127]-.10f)<1e-6f);
    engine->setPassThroughEnabled(false);
    REQUIRE(engine->builtinMeterLevels(7,0,true).generation==0);
    engine->setPassThroughEnabled(true);
    REQUIRE(engine->builtinMeterLevels(7,0,true).generation==0); // rapid PANIC/rearm has no stale readout
    process();
    for(const auto& channel:outputs)for(float sample:channel)REQUIRE(sample==0);
    REQUIRE(engine->builtinMeterLevels(7,0,true).generation==0);
    engine->audioDeviceStopped();
    REQUIRE(engine->builtinMeterLevels(7,0).generation==0);
    REQUIRE(engine->builtinMeterLevels(7,0,true).generation==0);
    engine->audioDeviceAboutToStart(&device);engine->setPassThroughEnabled(true);
    REQUIRE(engine->builtinMeterLevels(7,0).generation==0);
    REQUIRE(engine->builtinMeterLevels(7,0,true).generation==0);
    engine->audioDeviceStopped();
}

void testOutputRackTailsPanicAndRestart(){
    auto engine=std::make_unique<lah::audio::LiveAudioEngine>();TestDevice device;Buffers b;
    auto& slot=engine->outputBuiltinControls().slots[1][0];
    lah::builtin::configureSlot(slot,lah::builtin::Fx,true);
    slot.params[0].store(100);slot.params[1].store(10);slot.params[2].store(0);
    slot.params[4].store(0);slot.params[5].store(0);
    engine->audioDeviceAboutToStart(&device);
    std::fill(b.left.begin(),b.left.end(),0);std::fill(b.right.begin(),b.right.end(),0);b.process(*engine,128);
    engine->setPassThroughEnabled(true);b.right[0]=.5f;b.process(*engine,128);b.right[0]=0;
    b.process(*engine,128);b.process(*engine,128);b.process(*engine,128);
    REQUIRE(std::abs(b.data[1][96]-.5f)<1e-6f);
    for(int frame=0;frame<128;++frame)REQUIRE(b.data[0][static_cast<std::size_t>(frame)]==0); // no cross-return leakage
    b.right[0]=.5f;b.process(*engine,128);b.right[0]=0;
    engine->setPassThroughEnabled(false);engine->setPassThroughEnabled(true);
    for(int block=0;block<8;++block){b.process(*engine,128);b.requireSilent(128);}
    b.right[0]=.5f;b.process(*engine,128);b.right[0]=0;
    engine->audioDeviceStopped();engine->audioDeviceAboutToStart(&device);
    b.process(*engine,128);b.requireSilent(128);engine->setPassThroughEnabled(true);
    for(int block=0;block<8;++block){b.process(*engine,128);b.requireSilent(128);}
    // PANIC must disarm output feedback controls even between callbacks.
    engine->setPassThroughEnabled(false);
    lah::builtin::configureSlot(slot,lah::builtin::AutoFeedback,true);
    slot.feedbackArmed.store(true);engine->setPassThroughEnabled(false);
    REQUIRE(!slot.feedbackArmed.load()&&!engine->outputBuiltinFeedbackStatus(1,0).armed);
    engine->audioDeviceStopped();
}

void testEightChannelBusMasterCallback(){
    auto engine=std::make_unique<lah::audio::LiveAudioEngine>();TestDevice device;device.channelCount=8;
    std::array<std::array<float,128>,8> inputs{},outputs{};
    std::array<const float*,8> inputPointers{};std::array<float*,8> outputPointers{};
    for(int channel=0;channel<8;++channel){inputPointers[static_cast<std::size_t>(channel)]=inputs[static_cast<std::size_t>(channel)].data();outputPointers[static_cast<std::size_t>(channel)]=outputs[static_cast<std::size_t>(channel)].data();}
    inputs[7].fill(.4f);
    auto process=[&]{insideCallback=true;engine->audioDeviceIOCallbackWithContext(inputPointers.data(),8,outputPointers.data(),8,128,{});insideCallback=false;};
    auto& controls=engine->controls();for(auto& route:controls.route)route.store(0);
    auto& bus=controls.busMaster;bus.enabled.store(true);bus.sends[7*32+7].store(1);
    bus.gainDb[7].store(-6);bus.pan[7].store(0);bus.toMaster[7].store(1);
    bus.masterLeft.store(0);bus.masterRight.store(1);bus.masterGainDb[1].store(-6);
    // The native output insert must process the bus/master return too, after
    // summation and before the physical output gain.
    auto& insert=engine->outputBuiltinControls().slots[0][0];lah::builtin::configureSlot(insert,lah::builtin::Dynamics,true);
    insert.params[0].store(0);insert.params[1].store(1);insert.params[4].store(-100);insert.params[5].store(-6);
    controls.outputGain[1].store(-3);
    engine->audioDeviceAboutToStart(&device);process();engine->setPassThroughEnabled(true);
    for(int block=0;block<4;++block)process();
    const float minus6=std::pow(10.0f,-6.0f/20),minus3=std::pow(10.0f,-3.0f/20);
    const float busEight=.4f*minus6,masterLeft=busEight*std::sqrt(.5f),masterRight=masterLeft*minus6;
    REQUIRE(std::abs(outputs[0][127]-masterLeft*minus6)<2e-6f);
    REQUIRE(std::abs(outputs[1][127]-masterRight*minus3)<2e-6f);
    for(int channel=2;channel<8;++channel)for(float sample:outputs[static_cast<std::size_t>(channel)])REQUIRE(sample==0);
    REQUIRE(std::abs(engine->consumeBusMasterPeak(7)-busEight)<2e-6f);
    REQUIRE(std::abs(engine->consumeBusMasterPeak(8)-masterLeft)<2e-6f);
    REQUIRE(std::abs(engine->consumeBusMasterPeak(9)-masterRight)<2e-6f);
    REQUIRE(engine->consumeBusMasterPeak(7)==0&&engine->consumeBusMasterPeak(8)==0&&engine->consumeBusMasterPeak(9)==0);
    REQUIRE(std::abs(engine->builtinMeterLevels(0,0,true).inputPeak-masterLeft)<2e-6f);
    REQUIRE(std::abs(engine->builtinMeterLevels(0,0,true).outputPeak-masterLeft*minus6)<2e-6f);
    REQUIRE(engine->getInputChannelCount()==8&&engine->getOutputChannelCount()==8);

    bus.masterMute.store(true);process();process();
    (void)engine->consumeBusMasterPeak(8);(void)engine->consumeBusMasterPeak(9);process();
    for(const auto& channel:outputs)for(float sample:channel)REQUIRE(sample==0);
    REQUIRE(engine->consumeBusMasterPeak(8)==0&&engine->consumeBusMasterPeak(9)==0);
    REQUIRE(std::abs(engine->consumeBusMasterPeak(7)-busEight)<2e-6f); // bus continues, master alone is muted
    bus.masterMute.store(false);process();process();
    engine->setPassThroughEnabled(false);
    REQUIRE(engine->consumeBusMasterPeak(7)==0&&engine->consumeBusMasterPeak(8)==0&&engine->consumeBusMasterPeak(9)==0);
    engine->setPassThroughEnabled(true); // no callback between PANIC and rearm
    REQUIRE(engine->consumeBusMasterPeak(7)==0&&engine->consumeBusMasterPeak(8)==0&&engine->consumeBusMasterPeak(9)==0);
    inputs[7].fill(0);
    for(int block=0;block<8;++block){process();for(const auto& channel:outputs)for(float sample:channel)REQUIRE(sample==0);
        REQUIRE(engine->consumeBusMasterPeak(7)==0&&engine->consumeBusMasterPeak(8)==0&&engine->consumeBusMasterPeak(9)==0);}
    engine->audioDeviceStopped();
}
} // namespace

int main() {
    testLifecycleAndRouting();
    testFirPanicAndRestart();
    testPluginBoundary();
    testInvalidCallbackGeometry();
    testPluginPanicHistory();
    testStaticLatencyCompensation();
    testLatencyChangeDuringProcessAndBound();
    testRapidPanicRearm();
    testPluginExceptionRecovery();
    testPluginLifecycleExceptions();
    testNativeRackCallbackAndMute();
    testAutomaticFeedbackCallbackAndPanic();
    testEightUsbReturnsAndOutputRacks();
    testOutputRackTailsPanicAndRestart();
    testEightChannelBusMasterCallback();
    std::cout << "PASS: actual JUCE engine callback lifecycle, 40-output silence, FIR PANIC history, plugin ceiling/block lengths/storage/nonfinite handling; synthetic only\n";
}

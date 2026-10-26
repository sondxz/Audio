#include "NativePluginAudit.h"
#include "NativePluginEditor.h"
#include "BuiltinRackPanel.h"
#include "../dsp/BuiltinSceneState.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

namespace lah::ui {
namespace {
constexpr int auditChannels=8,auditFrames=128,targetChannel=7,targetSlot=2;
constexpr double auditRate=48000.0;
void require(bool condition,const juce::String& message){
    if(!condition)throw std::runtime_error(message.toStdString());
}
double dbRatio(double numerator,double denominator){
    require(numerator>0 && denominator>0,"A measured signal is silent; ratio cannot establish DSP operation.");
    return 20*std::log10(numerator/denominator);
}

class AuditDevice final : public juce::AudioIODevice {
public:
    AuditDevice():AudioIODevice("Synthetic 8-channel GUI audit","SYNTHETIC / NO HARDWARE"){}
    juce::StringArray getOutputChannelNames()override{return names("OUT");}
    juce::StringArray getInputChannelNames()override{return names("IN");}
    juce::Array<double> getAvailableSampleRates()override{return {auditRate};}
    juce::Array<int> getAvailableBufferSizes()override{return {auditFrames};}
    int getDefaultBufferSize()override{return auditFrames;}
    juce::String open(const juce::BigInteger&,const juce::BigInteger&,double,int)override{
        ++openCalls;return "This synthetic fixture must not be opened.";
    }
    void close()override{}
    bool isOpen()override{return true;}
    void start(juce::AudioIODeviceCallback*)override{++startCalls;}
    void stop()override{}
    bool isPlaying()override{return true;}
    juce::String getLastError()override{return {};}
    int getCurrentBufferSizeSamples()override{return auditFrames;}
    double getCurrentSampleRate()override{return auditRate;}
    int getCurrentBitDepth()override{return 32;}
    juce::BigInteger getActiveOutputChannels()const override{return active();}
    juce::BigInteger getActiveInputChannels()const override{return active();}
    int getOutputLatencyInSamples()override{return 0;}
    int getInputLatencyInSamples()override{return 0;}
    int openCalls=0,startCalls=0;
private:
    static juce::StringArray names(const char* prefix){
        juce::StringArray result;for(int ch=0;ch<auditChannels;++ch)result.add(juce::String(prefix)+juce::String(ch+1));return result;
    }
    static juce::BigInteger active(){juce::BigInteger result;result.setRange(0,auditChannels,true);return result;}
};

std::vector<juce::Slider*> sliders(juce::Component& component){
    std::vector<juce::Slider*> result,named;
    for(int child=0;child<component.getNumChildComponents();++child)
        if(auto* slider=dynamic_cast<juce::Slider*>(component.getChildComponent(child))){
            result.push_back(slider);if(slider->getName().startsWith("native-parameter-"))named.push_back(slider);
        }
    if(!named.empty()){
        std::sort(named.begin(),named.end(),[](const auto* a,const auto* b){return a->getName()<b->getName();});result=std::move(named);
    }
    require(result.size()==6,"Expected six real top-level native DSP parameter sliders.");
    return result;
}
juce::ComboBox& moduleSelector(NativePluginEditor& editor){
    for(int child=0;child<editor.getNumChildComponents();++child)
        if(auto* combo=dynamic_cast<juce::ComboBox*>(editor.getChildComponent(child));combo && combo->getNumItems()==builtin::moduleCount+1)return *combo;
    throw std::runtime_error("Cannot find the native editor's real processor selector.");
}
juce::TextButton& button(juce::Component& component,const juce::String& label){
    for(int child=0;child<component.getNumChildComponents();++child)
        if(auto* control=dynamic_cast<juce::TextButton*>(component.getChildComponent(child));control && control->getButtonText()==label)return *control;
    throw std::runtime_error(("Cannot find real UI button: "+label).toStdString());
}
void click(juce::TextButton& control){
    require(control.isEnabled() && static_cast<bool>(control.onClick),"Attempted to invoke an unavailable UI button.");control.onClick();
}
juce::Component* namedChild(juce::Component& component,const juce::String& name){
    if(component.getName()==name)return &component;
    for(int child=0;child<component.getNumChildComponents();++child)if(auto* found=namedChild(*component.getChildComponent(child),name))return found;
    return nullptr;
}
struct Measurement {std::array<double,auditChannels> rms{};};

class Fixture {
public:
    explicit Fixture(bool output):engine(std::make_unique<audio::LiveAudioEngine>()),outputDomain(output){
        engine->audioDeviceAboutToStart(&device);
        require(engine->isDeviceRunning() && !engine->isPassThroughEnabled(),"Synthetic engine did not start muted.");
        for(int ch=0;ch<auditChannels;++ch){inputPointers[static_cast<std::size_t>(ch)]=input[static_cast<std::size_t>(ch)].data();outputPointers[static_cast<std::size_t>(ch)]=outputSamples[static_cast<std::size_t>(ch)].data();}
        editor=std::make_unique<NativePluginEditor>(*engine,targetChannel,targetSlot,outputDomain);
        require(editor->getSelectedModule()==builtin::None,"The audited insert must begin empty.");
    }
    ~Fixture(){engine->setPassThroughEnabled(false);engine->audioDeviceStopped();}
    builtin::SlotControl& selected(){
        return (outputDomain?engine->outputBuiltinControls():engine->builtinControls()).slots[targetChannel][targetSlot];
    }
    void load(int module){
        require(!engine->isPassThroughEnabled(),"Replacement must occur while transport is muted.");
        moduleSelector(*editor).setSelectedId(module+1,juce::sendNotificationSync);
        require(selected().type.load()==module && editor->getSelectedModule()==module,"The processor selector loaded the wrong module/target.");
        require(!selected().enabled.load(),"A newly inserted module must begin bypassed.");
    }
    void parameter(int index,double value){
        auto controls=sliders(*editor);require(index>=0 && index<static_cast<int>(controls.size()),"Invalid GUI parameter index.");
        controls[static_cast<std::size_t>(index)]->setValue(value,juce::sendNotificationSync);
        require(std::abs(selected().params[static_cast<std::size_t>(index)].load()-controls[static_cast<std::size_t>(index)]->getValue())<1e-5,"GUI slider callback did not reach the selected parameter atomic.");
    }
    void enableSlot(){click(button(*editor,"BYPASS"));require(selected().enabled.load(),"GUI ON did not enable the selected insert.");}
    void bypassSlot(){click(button(*editor,"ON"));require(!selected().enabled.load(),"GUI bypass did not disable the selected insert.");}
    void start(){engine->setPassThroughEnabled(true);require(engine->isPassThroughEnabled(),"Synthetic audio could not be enabled.");}
    void process(){
        for(auto& samples:outputSamples)samples.fill(1234.0f);
        engine->audioDeviceIOCallbackWithContext(inputPointers.data(),auditChannels,outputPointers.data(),auditChannels,auditFrames,{});
        require(engine->isPassThroughEnabled() && !engine->isSafetyLatched(),"Audio stopped during GUI/DSP audit; inspect processing deadline or safety fault.");
        require(engine->getErrorCount()==0,"The engine reported a processing error.");
        for(const auto& samples:outputSamples)for(float sample:samples)require(std::isfinite(sample) && std::abs(sample)<1,"Audio callback left an invalid/unwritten output sample.");
    }
    Measurement tone(){
        // 48 * 128 = 6144 measured frames, exactly 128 cycles at 1 kHz.
        Measurement result;constexpr int settleBlocks=48,measureBlocks=48;
        for(int block=0;block<settleBlocks+measureBlocks;++block){
            for(int frame=0;frame<auditFrames;++frame,++toneFrame){
                for(int ch=0;ch<auditChannels;++ch){
                    const double amplitude=ch==targetChannel?.1:.04+.007*ch;
                    input[static_cast<std::size_t>(ch)][static_cast<std::size_t>(frame)]=static_cast<float>(amplitude*std::sin(2*builtin::pi*1000*toneFrame/auditRate+.1*ch));
                }
            }
            process();
            if(block>=settleBlocks)for(int ch=0;ch<auditChannels;++ch)for(float sample:outputSamples[static_cast<std::size_t>(ch)])result.rms[static_cast<std::size_t>(ch)]+=static_cast<double>(sample)*sample;
        }
        for(auto& rms:result.rms)rms=std::sqrt(rms/(measureBlocks*auditFrames));return result;
    }
    std::vector<float> impulse(){
        std::vector<float> result;result.reserve(2048);
        for(int block=0;block<16;++block){
            for(auto& samples:input)samples.fill(0);
            input[0].fill(.05f);if(block==0)input[targetChannel][0]=.5f;
            process();
            for(float sample:outputSamples[0])require(std::abs(sample-.05f)<1e-6,"Delay edit changed the independent CH1 reference.");
            result.insert(result.end(),outputSamples[targetChannel].begin(),outputSamples[targetChannel].end());
        }
        return result;
    }
    void settleWithSilence(){
        // Live parameter changes deliberately crossfade for 64 samples. Finish
        // that bounded transition before probing the steady delay's arrival.
        for(int block=0;block<2;++block){for(auto& samples:input)samples.fill(0);input[0].fill(.05f);process();}
    }
    void assertNoHardware(){require(device.openCalls==0 && device.startCalls==0,"The audit attempted to open/start its fixture instead of calling the synthetic callback directly.");}
    AuditDevice device;
    std::unique_ptr<audio::LiveAudioEngine> engine;
    std::unique_ptr<NativePluginEditor> editor;
    bool outputDomain=false;
    std::array<std::array<float,auditFrames>,auditChannels> input{},outputSamples{};
private:
    std::array<const float*,auditChannels> inputPointers{};
    std::array<float*,auditChannels> outputPointers{};
    std::uint64_t toneFrame=0;
};

struct Report {
    juce::String csv="case,domain,channel,insert,parameter,before,after,expected_db,measured_db,max_other_channel_delta_db,audio_enabled,result\n";
    juce::String detail="Native plugin GUI to actual audio callback audit\n"
        "Scope: synthetic eight-channel AudioIODevice; no ASIO/WASAPI/physical device opened.\n"
        "GUI actions invoke the real ComboBox and Slider synchronous notification callbacks and TextButton onClick handlers.\n"
        "Signal path uses LiveAudioEngine::audioDeviceIOCallbackWithContext at 48 kHz / 128 frames.\n"
        "Every target is CH8 / INSERT3; all seven other channels are checked for isolation.\n\n";
    int checks=0;
    void row(const juce::String& name,bool output,const juce::String& parameter,double before,double after,double expected,double measured,double referenceDelta,bool enabled){
        csv+=name+","+(output?"output":"input")+",8,3,"+parameter+","+juce::String(before,5)+","+juce::String(after,5)+","+juce::String(expected,6)+","+juce::String(measured,8)+","+juce::String(referenceDelta,8)+","+(enabled?"1":"0")+",PASS\n";
        detail+=name+" / "+(output?"output":"input")+": "+juce::String(measured,5)+" dB (expected "+juce::String(expected,2)+"), maximum other-channel delta "+juce::String(referenceDelta,7)+" dB; transport remained enabled.\n";++checks;
    }
};
double isolationDelta(const Measurement& before,const Measurement& after){
    double maximum=0;
    for(int ch=0;ch<auditChannels;++ch)if(ch!=targetChannel)maximum=std::max(maximum,std::abs(dbRatio(after.rms[static_cast<std::size_t>(ch)],before.rms[static_cast<std::size_t>(ch)])));
    require(maximum<.0001,"A GUI edit changed an unrelated audio channel.");return maximum;
}
void toneCase(bool output,int module,Report& report){
    Fixture fixture(output);fixture.load(module);
    const bool colour=module==builtin::VocalColor;
    if(colour){fixture.parameter(4,0);fixture.parameter(5,0);}
    else {fixture.parameter(0,20);fixture.parameter(1,0);fixture.parameter(2,1000);fixture.parameter(3,0);fixture.parameter(4,1);fixture.parameter(5,0);}
    fixture.enableSlot();fixture.start();const auto before=fixture.tone();
    require(std::abs(before.rms[targetChannel]-.1/std::sqrt(2.0))<.00001,"Baseline CH8 signal does not match the identity USB return.");
    const int parameter=colour?5:3;const double changed=colour?-12:6;
    fixture.parameter(parameter,changed);const auto after=fixture.tone();
    const double measured=dbRatio(after.rms[targetChannel],before.rms[targetChannel]);
    require(std::abs(measured-changed)<.05,"Real audio level did not follow the edited GUI parameter: "+juce::String(measured,5)+" dB.");
    const double delta=isolationDelta(before,after);
    report.row(colour?"VocalColor_live_gain":"Equalizer_live_mid_gain",output,"p"+juce::String(parameter),0,changed,changed,measured,delta,fixture.engine->isPassThroughEnabled());
    fixture.bypassSlot();const auto dry=fixture.tone();
    const double dryRatio=dbRatio(dry.rms[targetChannel],.1/std::sqrt(2.0));require(std::abs(dryRatio)<.001,"GUI bypass did not restore dry audio.");
    report.row("GUI_bypass_restores_dry",output,"enabled",1,0,0,dryRatio,isolationDelta(before,dry),fixture.engine->isPassThroughEnabled());
    click(button(*fixture.editor,"RESET"));
    const auto& metadata=builtin::metadata(module);for(int p=0;p<6;++p)require(fixture.selected().params[static_cast<std::size_t>(p)].load()==metadata.params[static_cast<std::size_t>(p)].defaultValue,"GUI reset did not restore real DSP defaults.");
    require(!fixture.selected().enabled.load(),"Reset unexpectedly enabled the bypassed insert.");
    moduleSelector(*fixture.editor).setSelectedId(builtin::Fx+1,juce::sendNotificationSync);
    require(fixture.selected().type.load()==module && fixture.editor->getSelectedModule()==module,"Processor replacement was permitted during active audio.");
    fixture.assertNoHardware();
    report.detail+="  Reset preserved bypass; replacement while transport active was rejected by the real handler.\n";
}
void delayCase(bool output,Report& report){
    Fixture fixture(output);fixture.load(builtin::Fx);fixture.parameter(0,100);fixture.parameter(1,10);fixture.parameter(2,0);fixture.parameter(4,0);fixture.parameter(5,0);fixture.enableSlot();fixture.start();
    fixture.settleWithSilence();const auto first=fixture.impulse();fixture.parameter(1,30);fixture.settleWithSilence();const auto second=fixture.impulse();
    const auto validate=[](const std::vector<float>& signal,int lag){
        for(int n=0;n<lag;++n)require(std::abs(signal[static_cast<std::size_t>(n)])<1e-6,"An echo arrived before the delay selected in the GUI.");
        require(std::abs(signal[static_cast<std::size_t>(lag)]-.5f)<1e-6,"The first echo does not match the selected GUI delay.");
        for(std::size_t n=static_cast<std::size_t>(lag+1);n<signal.size();++n)require(std::abs(signal[n])<1e-6,"Zero-feedback delay retained an unexpected echo.");
    };
    validate(first,480);validate(second,1440);fixture.assertNoHardware();
    report.detail+="FX Delay / "+juce::String(output?"output":"input")+": real GUI time edit 10 -> 30 ms moved impulse arrival 480 -> 1440 samples after 256 silent settling frames for the intentional 64-frame parameter crossfade; dry CH1 unchanged and audio enabled.\n";++report.checks;
}
void staleRackCase(bool output,Report& report){
    Fixture fixture(output);fixture.load(builtin::VocalClean);
    juce::AudioDeviceManager unopenedManager;require(unopenedManager.getCurrentAudioDevice()==nullptr,"Audit device manager unexpectedly opened hardware.");
    BuiltinRackPanel oldRack(*fixture.engine,unopenedManager);oldRack.selectChannel(targetChannel,targetSlot,output);
    auto oldSliders=sliders(oldRack);fixture.load(builtin::Fx);
    oldSliders[0]->setValue(120,juce::sendNotificationSync);
    require(fixture.selected().params[0].load()==builtin::metadata(builtin::Fx).params[0].defaultValue,"A stale rack slider wrote an old parameter meaning into the replaced module.");

    fixture.load(builtin::VocalClean);oldRack.selectChannel(targetChannel,targetSlot,output);
    auto* oldEnable=&button(oldRack,"Enable slot");fixture.load(builtin::Fx);click(*oldEnable);
    require(!fixture.selected().enabled.load(),"Stale rack enable changed the replacement module.");

    fixture.load(builtin::VocalClean);oldRack.selectChannel(targetChannel,targetSlot,output);
    auto* oldReset=&button(oldRack,"Reset parameters");fixture.load(builtin::Fx);fixture.parameter(0,61);click(*oldReset);
    require(fixture.selected().params[0].load()==61,"Stale rack reset changed the replacement module's parameters.");

    fixture.load(builtin::AutoFeedback);fixture.enableSlot();oldRack.selectChannel(targetChannel,targetSlot,output);
    auto* oldClear=&button(oldRack,"Clear notches");fixture.load(builtin::Fx);
    const auto resetGeneration=fixture.selected().feedbackResetGeneration.load();click(*oldClear);
    require(fixture.selected().feedbackResetGeneration.load()==resetGeneration,"Stale feedback clear modified the replacement module.");
    require(unopenedManager.getCurrentAudioDevice()==nullptr,"Stale-control audit opened a physical device.");fixture.assertNoHardware();
    report.detail+="Stale rack / "+juce::String(output?"output":"input")+": old slider, enable, reset and feedback-clear handlers left the replaced FX slot unchanged; no device manager was initialized.\n";++report.checks;
}
void extendedEqCase(bool output,Report& report){
    Fixture fixture(output);fixture.load(builtin::Equalizer);
    juce::AudioDeviceManager unopenedManager;BuiltinRackPanel oldRack(*fixture.engine,unopenedManager);oldRack.selectChannel(targetChannel,targetSlot,output);
    const auto legacySliders=sliders(oldRack);
    click(button(*fixture.editor,"8 BAND EQ"));require(fixture.selected().extendedEq.load(),"The actual EQ mode button did not activate eight-band processing.");
    // This event was queued by the old six-knob view before its mode refresh.
    legacySliders[3]->setValue(9,juce::sendNotificationSync);
    require(fixture.selected().params[3].load()==0 && fixture.selected().eqBands[3].gainDb.load()==0,"A legacy EQ control modified state after eight-band mode was enabled.");
    for(auto* control:legacySliders)require(!control->isEnabled(),"Legacy EQ controls remain enabled while eight-band processing is active.");
    auto* selector=dynamic_cast<juce::TextButton*>(namedChild(*fixture.editor,"eq-band-3"));
    auto* gain=dynamic_cast<juce::Slider*>(namedChild(*fixture.editor,"eq-gain"));
    require(selector && gain,"Cannot find the real extended EQ band controls.");click(*selector);
    fixture.enableSlot();fixture.start();const auto baseline=fixture.tone();
    gain->setValue(6,juce::sendNotificationSync);require(fixture.selected().eqBands[3].gainDb.load()==6,"Eight-band gain callback missed the selected band.");
    const auto changed=fixture.tone();const double measured=dbRatio(changed.rms[targetChannel],baseline.rms[targetChannel]);
    require(std::abs(measured-6)<.05,"The GUI's eight-band EQ gain did not change callback audio by +6 dB.");
    report.row("EightBandEQ_live_gain",output,"band3_gain",0,6,6,measured,isolationDelta(baseline,changed),fixture.engine->isPassThroughEnabled());
    fixture.engine->setPassThroughEnabled(false);oldRack.refreshControls();click(button(oldRack,"Reset parameters"));
    require(!fixture.selected().extendedEq.load() && !fixture.selected().eqInitialized.load(),"Rack RESET did not return extended EQ to legacy defaults.");
    for(auto* control:legacySliders)require(control->isEnabled(),"Legacy EQ controls were not restored after RESET.");
    require(unopenedManager.getCurrentAudioDevice()==nullptr,"Extended EQ regression unexpectedly opened hardware.");fixture.assertNoHardware();
    report.detail+="  Eight-band mode disables legacy controls; queued legacy edits were rejected; rack RESET cleared extended state and restored editable legacy controls.\n";
}
void proLegacyRackCase(bool output,int module,Report& report){
    Fixture fixture(output);fixture.load(module);const bool digital=module==builtin::DigitalFx;
    juce::AudioDeviceManager unopenedManager;BuiltinRackPanel oldRack(*fixture.engine,unopenedManager);oldRack.selectChannel(targetChannel,targetSlot,output);
    const auto legacy=sliders(oldRack);const auto before=builtin::snapshotSlot(fixture.selected());
    const auto proButton=[&]() -> juce::TextButton& {auto* result=dynamic_cast<juce::TextButton*>(namedChild(*fixture.editor,"native-pro-mode"));require(result!=nullptr,"Missing the actual native PRO mode button.");return *result;};
    const auto proEnabled=[&]{return digital?fixture.selected().digitalFxPro.enabled.load():fixture.selected().dynamicsPro.enabled.load();};
    const int parameter=digital?digitalfx::EchoTimeMs:dynamicspro::CompThreshold;const float desired=digital?250.f:-24.f;
    const auto editPro=[&]{auto* control=dynamic_cast<juce::Slider*>(namedChild(*fixture.editor,(digital?"digital-fx-pro-param-":"dynamics-pro-parameter-")+juce::String(parameter)));
        require(control!=nullptr && control->isEnabled(),"Missing the active PRO numeric control.");control->setValue(desired,juce::sendNotificationSync);};
    const auto proValue=[&]{return digital?fixture.selected().digitalFxPro.params[static_cast<std::size_t>(parameter)].load():fixture.selected().dynamicsPro.params[static_cast<std::size_t>(parameter)].load();};
    const auto unchangedLegacy=[&]{for(std::size_t p=0;p<before.params.size();++p)require(fixture.selected().params[p].load()==before.params[p],"A queued legacy control modified its inactive parameter state.");};
    click(proButton());require(proEnabled(),"PRO button did not enable the advanced DSP mode.");editPro();
    legacy[0]->setValue(digital?20:-30,juce::sendNotificationSync);unchangedLegacy();require(proValue()==desired,"A legacy slider changed active PRO settings.");
    for(auto* control:legacy)require(!control->isEnabled(),"A legacy control remains enabled while PRO DSP is active.");
    // Each stale action begins with the old panel still displaying legacy mode.
    click(proButton());oldRack.refreshControls();auto* staleReset=&button(oldRack,"Reset parameters");click(proButton());editPro();click(*staleReset);
    require(proEnabled() && proValue()==desired,"A stale legacy reset cleared newly activated PRO settings.");
    click(proButton());oldRack.refreshControls();auto* staleEnable=&button(oldRack,"Enable slot");click(proButton());click(*staleEnable);
    require(!fixture.selected().enabled.load() && proEnabled(),"A stale legacy enable changed a newly activated PRO insert.");
    oldRack.refreshControls();click(button(oldRack,"Reset parameters"));
    require(!proEnabled() && !fixture.selected().digitalFxPro.enabled.load() && !fixture.selected().dynamicsPro.enabled.load() && !fixture.selected().extendedEq.load(),"Rack RESET failed to clear optional processor modes.");
    require(digitalfx::equal(digitalfx::snapshot(fixture.selected().digitalFxPro),digitalfx::Snapshot{}) && dynamicspro::snapshot(fixture.selected().dynamicsPro)==dynamicspro::Snapshot{},"Rack RESET failed to restore PRO parameter defaults.");
    for(auto* control:legacy)require(control->isEnabled(),"Rack RESET did not restore the six legacy controls.");unchangedLegacy();
    // The floating editor's RESET changes the current candidate, preserving the
    // operator's A/B bank for comparison and keeping the insert bypassed.
    click(proButton());editPro();auto* storeA=dynamic_cast<juce::TextButton*>(namedChild(*fixture.editor,"native-store-a"));auto* recallA=dynamic_cast<juce::TextButton*>(namedChild(*fixture.editor,"native-recall-a"));
    require(storeA && recallA,"Missing actual preset A/B controls.");click(*storeA);click(button(*fixture.editor,"RESET"));require(!proEnabled(),"Floating RESET did not return PRO to legacy mode.");
    click(*recallA);require(proEnabled() && proValue()==desired && !fixture.selected().enabled.load(),"RESET erased A/B settings or A recall changed the insert bypass state.");
    click(button(*fixture.editor,"RESET"));
    // Same-type replacement invalidates the old comparison identity. Reopening
    // (refreshing the reusable editor) must construct a usable new comparison.
    builtin::configureSlot(fixture.selected(),module,false);fixture.editor->refreshControls();
    recallA=dynamic_cast<juce::TextButton*>(namedChild(*fixture.editor,"native-recall-a"));require(recallA && recallA->isEnabled(),"Same-type replacement left the reopened preset editor permanently disabled.");
    require(unopenedManager.getCurrentAudioDevice()==nullptr,"PRO legacy-control regression opened physical hardware.");fixture.assertNoHardware();
    report.detail+=juce::String(digital?"Digital FX PRO":"Dynamics PRO")+" / "+(output?"output":"input")+": legacy knobs disabled; queued slider/reset/enable rejected; rack and floating RESET restore legacy defaults; stored A survives RESET; same-type replacement rebinds comparison; no hardware opened.\n";++report.checks;
}
}

bool runNativePluginAudioAudit(const juce::File& destination,juce::String& error){
    Report report;bool success=true;
    try{
        require(juce::MessageManager::getInstance()->isThisTheMessageThread(),"Native plugin audit must execute on the JUCE message thread.");
        for(bool output:{false,true}){toneCase(output,builtin::VocalColor,report);toneCase(output,builtin::Equalizer,report);delayCase(output,report);staleRackCase(output,report);extendedEqCase(output,report);proLegacyRackCase(output,builtin::DigitalFx,report);proLegacyRackCase(output,builtin::Dynamics,report);}
        report.detail+="\nPASS: "+juce::String(report.checks)+" GUI/audio and stale-control checks.\n"
            "This establishes synthetic control-to-audio operation only. It does not establish M32R USB/ASIO compatibility or measured hardware round-trip latency.\n";
    }catch(const std::exception& failure){success=false;error=juce::String::fromUTF8(failure.what());report.detail+="\nFAIL: "+error+"\n";}
    if(!destination.createDirectory().wasOk() || !destination.getChildFile("plugin-audio-audit.csv").replaceWithText(report.csv) || !destination.getChildFile("report.txt").replaceWithText(report.detail)){
        if(success)error="Unable to save the native plugin audio audit evidence.";success=false;
    }
    return success;
}
}

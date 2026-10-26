#include "DigitalFxProPanel.h"
#include "../dsp/BuiltinSceneState.h"
#include <algorithm>
#include <cmath>

namespace lah::ui {
namespace {
const juce::Colour background{0xff0c111a},card{0xff142130},edge{0xff2c4051},text{0xffe8f3fa},muted{0xff8da6b8};
const juce::Colour echoColour{0xff55d5ed},reverbColour{0xffb69aff};
constexpr std::array<int,9> echoParameters{{digitalfx::EchoOn,digitalfx::EchoTimeMs,digitalfx::EchoFeedback,digitalfx::EchoMix,
    digitalfx::EchoLowCut,digitalfx::EchoHighCut,digitalfx::EchoToReverb,digitalfx::Ducking,digitalfx::EchoMasterDb}};
constexpr std::array<int,9> reverbParameters{{digitalfx::ReverbOn,digitalfx::PreDelayMs,digitalfx::DecaySeconds,digitalfx::DampingHz,
    digitalfx::RoomSize,digitalfx::Diffusion,digitalfx::ReverbMix,digitalfx::ReverbMasterDb,digitalfx::OutputDb}};
float decibelGain(float decibels){return std::pow(10.0f,decibels*.05f);}
bool snapshotsEqual(const builtin::SlotSnapshot& a,const builtin::SlotSnapshot& b){
    return a.type==b.type&&a.enabled==b.enabled&&a.params==b.params&&a.extendedEq==b.extendedEq&&a.eqInitialized==b.eqInitialized&&
        a.eqBands==b.eqBands&&digitalfx::equal(a.digitalFxPro,b.digitalFxPro);
}
}

class DigitalFxProPanel::Content final : public juce::Component {
public:
    explicit Content(DigitalFxProPanel& panel):owner(panel){setName("digital-fx-pro-content");}
    void paint(juce::Graphics& g)override {
        g.fillAll(background);
        for(const auto& bounds:{owner.echoCard,owner.reverbCard}){
            g.setColour(card);g.fillRoundedRectangle(bounds.toFloat(),7);
            g.setColour(edge);g.drawRoundedRectangle(bounds.toFloat().reduced(.5f),7,1);
        }
    }
private:DigitalFxProPanel& owner;
};
class DigitalFxProPanel::Model final : public juce::Component {
public:
    Model(DigitalFxProPanel& panel,bool echoModel):owner(panel),echo(echoModel){setInterceptsMouseClicks(false,false);setName(echo?"digital-echo-model":"digital-reverb-model");}
    void paint(juce::Graphics& g)override {
        const auto colour=echo?echoColour:reverbColour;
        g.setColour(juce::Colour(0xff0a1420));g.fillRoundedRectangle(getLocalBounds().toFloat(),5);
        auto area=getLocalBounds().reduced(7,4);g.setColour(colour);g.setFont(juce::FontOptions(10,juce::Font::bold));
        g.drawText(echo?"ECHO REPEAT MODEL":"REVERB RT60 ENVELOPE MODEL",area.removeFromTop(15),juce::Justification::centredLeft);
        if(!owner.valid()){g.setColour(muted);g.drawText("Slot changed or PRO mode off",area,juce::Justification::centred);return;}
        const auto value=owner.current();const auto& p=value.params;const auto plot=area.withTrimmedBottom(16).toFloat().reduced(2,2);
        g.setColour(edge.withAlpha(.7f));g.drawLine(plot.getX(),plot.getBottom(),plot.getRight(),plot.getBottom());
        for(int n=1;n<=3;++n)g.drawVerticalLine(juce::roundToInt(plot.getX()+plot.getWidth()*n/4),plot.getY(),plot.getBottom());
        const bool on=p[static_cast<std::size_t>(echo?digitalfx::EchoOn:digitalfx::ReverbOn)]>=.5f;
        if(!on){g.setColour(muted);g.drawText(echo?"Echo circuit OFF":"Reverb circuit OFF",plot.toNearestInt(),juce::Justification::centred);}
        else if(echo){
            const float delay=p[digitalfx::EchoTimeMs],feedback=p[digitalfx::EchoFeedback]*.01f;
            const float level=p[digitalfx::EchoMix]*.01f*decibelGain(p[digitalfx::EchoMasterDb]);
            const float span=std::max(4*delay,350.0f);
            for(int repeat=1;repeat<=96;++repeat){const float time=repeat*delay;if(time>span)break;
                const float amplitude=level*std::pow(feedback,static_cast<float>(repeat-1));
                const float x=plot.getX()+plot.getWidth()*time/span;
                g.setColour(colour.withAlpha(std::max(.15f,1.0f-repeat*.06f)));
                g.drawLine(x,plot.getBottom(),x,plot.getBottom()-plot.getHeight()*std::min(1.0f,amplitude),3);
            }
        }else{
            const float predelay=p[digitalfx::PreDelayMs]*.001f,decay=p[digitalfx::DecaySeconds],span=predelay+decay;
            const float level=p[digitalfx::ReverbMix]*.01f*decibelGain(p[digitalfx::ReverbMasterDb]);
            juce::Path envelope;
            for(int n=0;n<=160;++n){const float time=span*n/160.0f;
                const float amplitude=time<predelay?0:level*std::pow(.001f,(time-predelay)/std::max(.2f,decay));
                const float x=plot.getX()+plot.getWidth()*n/160.0f,y=plot.getBottom()-plot.getHeight()*std::min(1.0f,amplitude);
                if(n==0)envelope.startNewSubPath(x,y);else envelope.lineTo(x,y);
            }
            g.setColour(colour);g.strokePath(envelope,juce::PathStrokeType(1.8f));
        }
        g.setColour(muted);g.setFont(8.5f);
        g.drawText(echo?"Ideal repeat levels; not measured audio":"Ideal decay guide; not an impulse response",area.removeFromBottom(14),juce::Justification::centredLeft);
    }
private:DigitalFxProPanel& owner;const bool echo;
};

DigitalFxProPanel::DigitalFxProPanel(audio::LiveAudioEngine& audio,int channel,int insert,bool output)
    :engine(audio),channelIndex(channel),slotIndex(insert),outputDomain(output),
     expectedGeneration(slot()?slot()->feedbackResetGeneration.load(std::memory_order_acquire):0),content(std::make_unique<Content>(*this)){
    setName("digital-fx-pro-panel");addAndMakeVisible(viewport);viewport.setName("digital-fx-pro-viewport");
    viewport.setViewedComponent(content.get(),false);viewport.setScrollBarsShown(true,true);viewport.setScrollBarThickness(12);
    for(int section=0;section<2;++section){models[static_cast<std::size_t>(section)]=std::make_unique<Model>(*this,section==0);content->addAndMakeVisible(*models[static_cast<std::size_t>(section)]);}
    for(auto* label:{&echoTitle,&reverbTitle,&timingText,&bpmLabel}){content->addAndMakeVisible(*label);label->setColour(juce::Label::textColourId,muted);}
    echoTitle.setText("ECHO / MONO",juce::dontSendNotification);reverbTitle.setText("REVERB / MONO",juce::dontSendNotification);
    echoTitle.setFont(juce::FontOptions(13,juce::Font::bold));reverbTitle.setFont(juce::FontOptions(13,juce::Font::bold));
    echoTitle.setColour(juce::Label::textColourId,echoColour);reverbTitle.setColour(juce::Label::textColourId,reverbColour);
    timingText.setFont(juce::FontOptions(10));bpmLabel.setFont(juce::FontOptions(10));bpmLabel.setText("SET BPM (1/4)",juce::dontSendNotification);
    content->addAndMakeVisible(tapButton);tapButton.setName("digital-fx-tap");tapButton.onClick=[this]{tapAt(juce::Time::getMillisecondCounterHiRes());};
    tapButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff20556a));tapButton.setColour(juce::TextButton::textColourOffId,text);
    tapButton.setTooltip("Tap twice. The interval sets mono Echo Time, bounded to 5-350 ms; longer intervals are capped at 350 ms. Your requested BPM stays unchanged.");
    content->addAndMakeVisible(applyBpmButton);applyBpmButton.setName("digital-fx-apply-bpm");applyBpmButton.onClick=[this]{bpmChanged();};
    applyBpmButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff20556a));applyBpmButton.setColour(juce::TextButton::textColourOffId,text);
    applyBpmButton.setTooltip("Apply the requested BPM to Echo Time, even when the number has not changed. Quarter-note time = 60000 / BPM, maximum 350 ms.");
    content->addAndMakeVisible(bpm);bpm.setName("digital-fx-bpm");bpm.setSliderStyle(juce::Slider::LinearHorizontal);bpm.setTextBoxStyle(juce::Slider::TextBoxRight,false,55,23);
    bpm.setRange(40,240,.1);bpm.setValue(120,juce::dontSendNotification);bpm.onValueChange=[this]{if(!refreshing)bpmChanged();};
    bpm.setTooltip("Enter a tempo to set quarter-note Echo Time = 60000 / BPM, capped at 350 ms. This is a tempo command, not a measured BPM display. Values below 171.4 BPM produce the same 350 ms limit.");
    const auto& info=digitalfx::specs();
    for(int index=0;index<digitalfx::parameterCount;++index){const auto p=static_cast<std::size_t>(index);auto& slider=parameters[p];auto& label=labels[p];const auto& spec=info[p];
        content->addAndMakeVisible(label);content->addAndMakeVisible(slider);label.setFont(juce::FontOptions(10));label.setJustificationType(juce::Justification::centred);label.setColour(juce::Label::textColourId,text);
        label.setText(index==digitalfx::Ducking?"Wet ducking (both)":index==digitalfx::OutputDb?"Global output":spec.name,juce::dontSendNotification);
        slider.setName("digital-fx-pro-param-"+juce::String(index));slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,83,19);
        slider.setRange(spec.minimum,spec.maximum,spec.step);slider.setDoubleClickReturnValue(true,spec.defaultValue);
        if(index==digitalfx::EchoLowCut||index==digitalfx::EchoHighCut||index==digitalfx::DampingHz)slider.setSkewFactorFromMidPoint(std::sqrt(static_cast<double>(spec.minimum)*spec.maximum));
        slider.setColour(juce::Slider::textBoxTextColourId,text);slider.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xff09111c));slider.setColour(juce::Slider::textBoxOutlineColourId,edge);
        const bool echoParameter=std::find(echoParameters.begin(),echoParameters.end(),index)!=echoParameters.end();slider.setColour(juce::Slider::rotarySliderFillColourId,echoParameter?echoColour:reverbColour);
        if(index==digitalfx::EchoOn||index==digitalfx::ReverbOn){slider.textFromValueFunction=[](double value){return value>=.5?juce::String("ON"):juce::String("OFF");};
            slider.valueFromTextFunction=[](const juce::String& value){return value.trim().equalsIgnoreCase("ON")?1.0:value.getDoubleValue()>=.5?1.0:0.0;};}
        else slider.setTextValueSuffix(juce::String(spec.unit).isEmpty()?juce::String{}:" "+juce::String(spec.unit));
        slider.setTooltip(juce::String(spec.name)+" - live parameter; drag or type a value. Double-click restores default.");
        slider.onValueChange=[this,index]{parameterChanged(index);};
    }
    refreshControls();
}
DigitalFxProPanel::~DigitalFxProPanel(){viewport.setViewedComponent(nullptr,false);}
builtin::SlotControl* DigitalFxProPanel::slot()const noexcept{
    if(channelIndex<0||channelIndex>=builtin::channelCount||slotIndex<0||slotIndex>=builtin::slotCount)return nullptr;
    auto& rack=outputDomain?engine.outputBuiltinControls():engine.builtinControls();return &rack.slots[static_cast<std::size_t>(channelIndex)][static_cast<std::size_t>(slotIndex)];
}
bool DigitalFxProPanel::valid()const noexcept{const auto* target=slot();return target&&target->type.load(std::memory_order_acquire)==builtin::DigitalFx&&
    target->feedbackResetGeneration.load(std::memory_order_acquire)==expectedGeneration&&target->digitalFxPro.enabled.load(std::memory_order_acquire);}
digitalfx::Snapshot DigitalFxProPanel::current()const noexcept{const auto* target=slot();return target?digitalfx::snapshot(target->digitalFxPro):digitalfx::Snapshot{};}
void DigitalFxProPanel::parameterChanged(int index){
    if(refreshing||!valid()||index<0||index>=digitalfx::parameterCount)return;
    const auto& spec=digitalfx::specs()[static_cast<std::size_t>(index)];const float value=std::clamp(static_cast<float>(parameters[static_cast<std::size_t>(index)].getValue()),spec.minimum,spec.maximum);
    slot()->digitalFxPro.params[static_cast<std::size_t>(index)].store(value,std::memory_order_release);updateTimingText();
    for(auto& model:models)model->repaint();if(onChanged)onChanged();
}
void DigitalFxProPanel::refreshControls(){
    refreshing=true;const bool available=valid();const auto value=current();
    for(std::size_t index=0;index<parameters.size();++index){auto& slider=parameters[index];slider.setEnabled(available);
        if(!slider.isMouseButtonDown()&&!slider.hasKeyboardFocus(true))slider.setValue(value.params[index],juce::dontSendNotification);}
    tapButton.setEnabled(available);applyBpmButton.setEnabled(available);bpm.setEnabled(available);refreshing=false;updateTimingText();for(auto& model:models)model->repaint();
}
void DigitalFxProPanel::tapAt(double milliseconds){
    if(!valid()||!std::isfinite(milliseconds))return;
    if(previousTapMs>=0){const double interval=milliseconds-previousTapMs;
        if(interval>0&&interval<=5000){const double time=std::clamp(interval,5.0,350.0);parameters[digitalfx::EchoTimeMs].setValue(time,juce::sendNotificationSync);
            // BPM is the operator's requested tempo, not an estimate derived
            // from a free echo time or a TAP interval outside its 40-240 range.
            updateTimingText();}}
    previousTapMs=milliseconds;
}
void DigitalFxProPanel::bpmChanged(){if(!valid())return;const double time=std::clamp(60000.0/bpm.getValue(),5.0,350.0);
    // APPLY is an explicit command: write through the normal guarded handler
    // even if the echo slider already displays this value and emits no event.
    parameters[digitalfx::EchoTimeMs].setValue(time,juce::dontSendNotification);parameterChanged(digitalfx::EchoTimeMs);updateTimingText();}
void DigitalFxProPanel::updateTimingText(){
    if(!valid()){timingText.setText("Digital FX slot changed or PRO mode off - reopen editor",juce::dontSendNotification);return;}
    const auto value=current();const double requestedTime=std::clamp(60000.0/bpm.getValue(),5.0,350.0);
    timingText.setText("Echo "+juce::String(value.params[digitalfx::EchoTimeMs],1)+" ms  |  Requested "+juce::String(bpm.getValue(),1)+" BPM -> "+juce::String(requestedTime,1)+" ms (max 350 ms)  |  Free Echo / TAP may differ",juce::dontSendNotification);
}
void DigitalFxProPanel::paint(juce::Graphics& g){g.fillAll(background);}
void DigitalFxProPanel::resized(){viewport.setBounds(getLocalBounds());content->setSize(std::max(730,getWidth()-viewport.getScrollBarThickness()),470);layoutContent();}
void DigitalFxProPanel::layoutContent(){
    auto area=content->getLocalBounds().reduced(6);auto tempo=area.removeFromTop(27);tapButton.setBounds(tempo.removeFromLeft(61));tempo.removeFromLeft(7);
    bpmLabel.setBounds(tempo.removeFromLeft(93));bpm.setBounds(tempo.removeFromLeft(200));tempo.removeFromLeft(9);applyBpmButton.setBounds(tempo.removeFromLeft(90));
    area.removeFromTop(3);timingText.setBounds(area.removeFromTop(18));
    area.removeFromTop(6);echoCard=area.removeFromLeft((area.getWidth()-10)/2);area.removeFromLeft(10);reverbCard=area;
    for(int section=0;section<2;++section){auto cardArea=(section==0?echoCard:reverbCard).reduced(7,5);
        (section==0?echoTitle:reverbTitle).setBounds(cardArea.removeFromTop(22));models[static_cast<std::size_t>(section)]->setBounds(cardArea.removeFromTop(82));cardArea.removeFromTop(4);
        const auto& indices=section==0?echoParameters:reverbParameters;const int cellWidth=cardArea.getWidth()/3;
        for(int position=0;position<9;++position){const int index=indices[static_cast<std::size_t>(position)];auto cell=juce::Rectangle<int>(cardArea.getX()+(position%3)*cellWidth,cardArea.getY()+(position/3)*94,cellWidth,92).reduced(2,0);
            labels[static_cast<std::size_t>(index)].setBounds(cell.removeFromTop(16));parameters[static_cast<std::size_t>(index)].setBounds(cell);}
    }
}
bool DigitalFxProPanel::runValidation(juce::String& error){
    if(engine.isPassThroughEnabled()||!valid()){error="Digital FX PRO validation requires a muted valid PRO target.";return false;}
    auto& selected=*slot();const auto saved=builtin::snapshotSlot(selected);const auto inputBefore=builtin::snapshot(engine.builtinControls()),outputBefore=builtin::snapshot(engine.outputBuiltinControls());
    const auto originalTap=previousTapMs;const double originalBpm=bpm.getValue();bool pass=true;juce::String failure;
    const auto check=[&](bool condition,const char* message){if(!condition&&pass){pass=false;failure=message;}};
    const auto& info=digitalfx::specs();
    for(int index=0;index<digitalfx::parameterCount;++index){const auto& spec=info[static_cast<std::size_t>(index)];
        for(float value:{spec.minimum,spec.maximum,spec.defaultValue}){parameters[static_cast<std::size_t>(index)].setValue(value,juce::sendNotificationSync);
            check(std::abs(selected.digitalFxPro.params[static_cast<std::size_t>(index)].load()-value)<=std::max(.0001f,spec.step*.51f),"A Digital FX PRO numeric callback did not update its real DSP parameter.");}}
    bpm.setValue(240,juce::dontSendNotification);applyBpmButton.onClick();check(selected.digitalFxPro.params[digitalfx::EchoTimeMs].load()==250,"APPLY BPM did not map requested 240 BPM to 250 ms.");
    previousTapMs=-1;tapAt(1000);tapAt(1125);check(selected.digitalFxPro.params[digitalfx::EchoTimeMs].load()==125,"TAP did not set 125 ms echo time.");
    tapAt(1925);check(selected.digitalFxPro.params[digitalfx::EchoTimeMs].load()==350,"TAP interval did not clamp at 350 ms.");
    tapAt(1926);check(selected.digitalFxPro.params[digitalfx::EchoTimeMs].load()==5,"TAP interval did not clamp at 5 ms.");
    // A free TAP edit must preserve the requested tempo. Applying the same
    // number explicitly must still work when JUCE emits no value-change event.
    refreshControls();check(bpm.getValue()==240,"TAP or refresh overwrote the requested BPM.");
    bpm.setValue(240,juce::sendNotificationSync);check(selected.digitalFxPro.params[digitalfx::EchoTimeMs].load()==5,"Unchanged BPM unexpectedly applied before the explicit button.");
    applyBpmButton.onClick();check(selected.digitalFxPro.params[digitalfx::EchoTimeMs].load()==250,"APPLY BPM failed after TAP 5 ms with the field already at 240 BPM.");
    bpm.setValue(120,juce::sendNotificationSync);check(selected.digitalFxPro.params[digitalfx::EchoTimeMs].load()==350,"120 BPM did not use the explicit 350 ms maximum.");
    bpm.setValue(240,juce::sendNotificationSync);check(selected.digitalFxPro.params[digitalfx::EchoTimeMs].load()==250,"240 BPM did not map to a quarter-note 250 ms.");
    bpm.setValue(40,juce::sendNotificationSync);check(selected.digitalFxPro.params[digitalfx::EchoTimeMs].load()==350,"40 BPM did not use the explicit 350 ms maximum.");
    applyBpmButton.onClick();check(selected.digitalFxPro.params[digitalfx::EchoTimeMs].load()==350&&timingText.getText().contains("max 350 ms"),"40 BPM did not retain and show the 350 ms maximum after APPLY BPM.");
    // Queue-like stale callbacks after mode disable cannot write parameters.
    selected.digitalFxPro.enabled.store(false);const auto disabled=digitalfx::snapshot(selected.digitalFxPro);
    parameters[digitalfx::EchoFeedback].setValue(67,juce::sendNotificationSync);tapAt(2100);bpm.setValue(180,juce::sendNotificationSync);applyBpmButton.onClick();
    check(digitalfx::equal(disabled,digitalfx::snapshot(selected.digitalFxPro)),"Disabled PRO mode accepted a stale callback.");
    selected.digitalFxPro.enabled.store(true);digitalfx::recall(selected.digitalFxPro,saved.digitalFxPro);
    const auto token=selected.feedbackResetGeneration.load();const auto beforeStale=digitalfx::snapshot(selected.digitalFxPro);
    selected.feedbackResetGeneration.fetch_add(1);parameters[digitalfx::EchoFeedback].setValue(41,juce::sendNotificationSync);
    check(digitalfx::equal(beforeStale,digitalfx::snapshot(selected.digitalFxPro)),"A replaced-slot generation accepted a stale callback.");selected.feedbackResetGeneration.store(token);
    selected.type.store(builtin::Analog);parameters[digitalfx::ReverbMix].setValue(39,juce::sendNotificationSync);
    check(digitalfx::equal(beforeStale,digitalfx::snapshot(selected.digitalFxPro)),"A different plugin type accepted a stale callback.");selected.type.store(saved.type);
    check(selected.enabled.load()==saved.enabled&&selected.type.load()==saved.type,"Digital FX PRO controls changed plugin type or ON/BYPASS.");
    for(bool output:{false,true}){const auto& before=output?outputBefore:inputBefore;auto& rack=output?engine.outputBuiltinControls():engine.builtinControls();
        for(int channel=0;channel<builtin::channelCount;++channel)for(int insert=0;insert<builtin::slotCount;++insert)
            if(output!=outputDomain||channel!=channelIndex||insert!=slotIndex)check(snapshotsEqual(builtin::snapshotSlot(rack.slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(insert)]),before[static_cast<std::size_t>(channel)][static_cast<std::size_t>(insert)]),"Digital FX PRO wrote another input/output slot.");}
    previousTapMs=originalTap;refreshing=true;bpm.setValue(originalBpm,juce::dontSendNotification);refreshing=false;refreshControls();if(onChanged)onChanged();
    if(!pass)error=failure;return pass;
}
} // namespace lah::ui

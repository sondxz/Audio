#include "VocalCleanProPanel.h"
#include "../dsp/BuiltinSceneState.h"
#include <algorithm>
#include <cmath>

namespace lah::ui {
namespace {
const juce::Colour background{0xff0c111a},panel{0xff142130},edge{0xff2c4051},text{0xffe8f3fa},muted{0xff8da6b8},cyan{0xff66dce9};
constexpr std::array<int,6> switches{{vocalpro::GateOn,vocalpro::DeEssOn,vocalpro::DeHarshOn,vocalpro::CompOn,vocalpro::AutoGain,vocalpro::Listen}};
constexpr std::array<const char*,6> switchNames{{"GATE","DE-ESS","DEHARSH","COMP","AUTO GAIN","LISTEN REMOVED"}};
constexpr std::array<int,4> globalControls{{vocalpro::Wet,vocalpro::AutoGain,vocalpro::Output,vocalpro::Listen}};
constexpr std::array<int,2> lowCutControls{{vocalpro::HighPassHz,vocalpro::HighPassSlope}},toneControls{{vocalpro::Warmth,vocalpro::Air}};
constexpr std::array<int,3> gateControls{{vocalpro::GateOn,vocalpro::GateThreshold,vocalpro::GateRange}},deEssControls{{vocalpro::DeEssOn,vocalpro::DeEssAmount,vocalpro::DeEssHz}},
    deHarshControls{{vocalpro::DeHarshOn,vocalpro::DeHarshAmount,vocalpro::DeHarshHz}};
constexpr std::array<int,6> compressorControls{{vocalpro::CompOn,vocalpro::CompThreshold,vocalpro::CompRatio,vocalpro::CompAttack,vocalpro::CompRelease,vocalpro::CompKnee}};
bool sameSlot(const builtin::SlotSnapshot& a,const builtin::SlotSnapshot& b){return a.type==b.type&&a.enabled==b.enabled&&a.params==b.params&&
    a.extendedEq==b.extendedEq&&a.eqInitialized==b.eqInitialized&&a.eqBands==b.eqBands&&digitalfx::equal(a.digitalFxPro,b.digitalFxPro)&&
    a.dynamicsPro==b.dynamicsPro&&vocalpro::equal(a.vocalCleanPro,b.vocalCleanPro);}
}

class VocalCleanProPanel::Content final : public juce::Component {
public:
    explicit Content(VocalCleanProPanel& panelOwner):owner(panelOwner){setName("vocal-clean-pro-content");}
    void paint(juce::Graphics& g)override{g.fillAll(background);for(const auto& card:owner.cards){g.setColour(panel);g.fillRoundedRectangle(card.toFloat(),7);
        g.setColour(edge);g.drawRoundedRectangle(card.toFloat().reduced(.5f),7,1);}}
private:VocalCleanProPanel& owner;
};
class VocalCleanProPanel::Model final : public juce::Component {
public:
    Model(VocalCleanProPanel& panelOwner,int modelKind):owner(panelOwner),kind(modelKind){setInterceptsMouseClicks(false,false);setName("vocal-clean-settings-model-"+juce::String(kind));}
    void paint(juce::Graphics& g)override{
        g.setColour(juce::Colour(0xff0a1420));g.fillRoundedRectangle(getLocalBounds().toFloat(),4);auto area=getLocalBounds().reduced(6,3);
        g.setColour(cyan.withRotatedHue(kind*.06f));g.setFont(juce::FontOptions(9,juce::Font::bold));
        g.drawText(kind==0?"LOW CUT SLOPE GUIDE / MODEL":"SETTINGS MODEL / NOT MEASURED GR",area.removeFromTop(13),juce::Justification::centredLeft);
        if(!owner.valid()){g.setColour(muted);g.drawText("Slot changed or PRO mode off",area,juce::Justification::centred);return;}
        const auto settings=owner.current();const auto& p=settings.params;const float clean=p[vocalpro::CleanAmount]*.01f;
        if(kind==0){
            const float cutoff=p[vocalpro::HighPassHz],slope=p[vocalpro::HighPassSlope];
            g.setColour(muted);g.setFont(9.0f);g.drawText(cutoff>0?juce::String(cutoff,0)+" Hz / "+juce::String(slope,0)+" dB per octave":"High-pass OFF (0 Hz)",area.removeFromTop(13),juce::Justification::centredLeft);
            auto plot=area.toFloat().reduced(2,1);g.setColour(edge);g.drawLine(plot.getX(),plot.getBottom(),plot.getRight(),plot.getBottom());
            juce::Path path;for(int n=0;n<=80;++n){const double hz=20*std::pow(1500.0/20,n/80.0);
                const double attenuation=cutoff>0?std::clamp(slope*std::log2(hz/cutoff),-36.0,0.0):0.0;
                const float x=plot.getX()+plot.getWidth()*n/80.0f,y=plot.getY()+plot.getHeight()*static_cast<float>(-attenuation/36);
                if(n==0)path.startNewSubPath(x,y);else path.lineTo(x,y);}
            g.setColour(cyan);g.strokePath(path,juce::PathStrokeType(1.5f));return;
        }
        if(kind==4){g.setColour(text);g.setFont(10.0f);
            g.drawText(juce::String(p[vocalpro::CompThreshold],1)+" dBFS / "+juce::String(p[vocalpro::CompRatio],1)+":1 / knee "+juce::String(p[vocalpro::CompKnee],1)+" dB",area.removeFromTop(16),juce::Justification::centredLeft);
            g.setColour(muted);g.setFont(9.5f);g.drawText("CLEAN "+juce::String(p[vocalpro::CleanAmount],0)+"% treatment  |  Attack "+juce::String(p[vocalpro::CompAttack],1)+" ms / release "+juce::String(p[vocalpro::CompRelease],0)+" ms",area,juce::Justification::centredLeft);return;}
        const int onIndex=kind==1?vocalpro::GateOn:kind==2?vocalpro::DeEssOn:vocalpro::DeHarshOn;
        const int amountIndex=kind==1?vocalpro::GateRange:kind==2?vocalpro::DeEssAmount:vocalpro::DeHarshAmount;
        const bool enabled=p[static_cast<std::size_t>(onIndex)]>=.5f;const float maximum=enabled?p[static_cast<std::size_t>(amountIndex)]*clean:0;
        juce::String detail=kind==1?"Threshold "+juce::String(p[vocalpro::GateThreshold],1)+" dBFS":
            "Detector "+juce::String(p[static_cast<std::size_t>(kind==2?vocalpro::DeEssHz:vocalpro::DeHarshHz)],0)+" Hz";
        g.setColour(text);g.setFont(9.5f);g.drawText(enabled?detail:"Stage OFF",area.removeFromTop(14),juce::Justification::centredLeft);
        g.setColour(muted);g.drawText("CLEAN-scaled maximum "+juce::String(maximum,1)+" dB",area.removeFromTop(14),juce::Justification::centredLeft);
        auto bar=area.removeFromTop(5).toFloat();g.setColour(edge);g.fillRoundedRectangle(bar,2);
        g.setColour(cyan.withRotatedHue(kind*.06f));g.fillRoundedRectangle(bar.withWidth(bar.getWidth()*std::clamp(maximum/(kind==1?60.0f:kind==2?18.0f:12.0f),0.0f,1.0f)),2);
    }
private:VocalCleanProPanel& owner;const int kind;
};

VocalCleanProPanel::VocalCleanProPanel(audio::LiveAudioEngine& audio,int channel,int insert,bool output)
    :engine(audio),channelIndex(channel),slotIndex(insert),outputDomain(output),
     expectedGeneration(slot()?slot()->feedbackResetGeneration.load(std::memory_order_acquire):0),content(std::make_unique<Content>(*this)){
    setName("vocal-clean-pro-panel");addAndMakeVisible(viewport);viewport.setName("vocal-clean-pro-viewport");viewport.setViewedComponent(content.get(),false);
    viewport.setScrollBarsShown(true,true);viewport.setScrollBarThickness(12);
    for(int kind=0;kind<5;++kind){models[static_cast<std::size_t>(kind)]=std::make_unique<Model>(*this,kind);content->addAndMakeVisible(*models[static_cast<std::size_t>(kind)]);}
    const char* names[]{"CLEAN / MONO VOCAL CHAIN","LOW CUT","GATE / EXPANDER","TONE","DE-ESS","DEHARSH","COMPRESSOR"};
    for(std::size_t index=0;index<titles.size();++index){content->addAndMakeVisible(titles[index]);titles[index].setText(names[index],juce::dontSendNotification);
        titles[index].setFont(juce::FontOptions(12,juce::Font::bold));titles[index].setColour(juce::Label::textColourId,cyan.withRotatedHue(index*.035f));}
    for(auto* label:{&explanation,&stateText}){content->addAndMakeVisible(*label);label->setColour(juce::Label::textColourId,muted);label->setFont(juce::FontOptions(10));}
    explanation.setText("CLEAN scales Gate, De-ess, Deharsh and Comp. Low Cut / Tone stay independent. Gate attenuates quiet signals; no AI denoising.",juce::dontSendNotification);
    explanation.setTooltip("LISTEN plays wet-scaled input minus the processed signal before Auto Gain and Output. It includes filter phase/tone differences, not isolated noise. Auto Gain uses useful signal only and is bounded to +6 dB.");
    for(std::size_t index=0;index<stageButtons.size();++index){auto& button=stageButtons[index];content->addAndMakeVisible(button);
        button.setName("vocal-clean-stage-"+juce::String(switches[index]));button.setColour(juce::TextButton::textColourOffId,text);
        button.onClick=[this,index]{toggleParameter(switches[index]);};}
    stageButtons[4].setTooltip("Actual level compensation, bounded to +6 dB and active only with useful signal; it does not add a fixed gain to silence.");
    stageButtons[5].setTooltip("Monitor the wet-scaled removed signal: input minus processed, before Auto Gain and Output. Filter phase/tone differences are included; this is not isolated noise.");
    const auto& specs=vocalpro::specs();
    for(int index=0;index<vocalpro::parameterCount;++index){const auto position=static_cast<std::size_t>(index);const auto& spec=specs[position];auto& slider=parameters[position];auto& label=labels[position];
        content->addAndMakeVisible(slider);content->addAndMakeVisible(label);label.setText(spec.name,juce::dontSendNotification);label.setFont(juce::FontOptions(10));label.setJustificationType(juce::Justification::centred);label.setColour(juce::Label::textColourId,text);
        slider.setName("vocal-clean-pro-param-"+juce::String(index));slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,82,19);
        slider.setRange(spec.minimum,spec.maximum,spec.step);slider.setDoubleClickReturnValue(true,spec.defaultValue);slider.setColour(juce::Slider::rotarySliderFillColourId,cyan.withRotatedHue(index*.007f));
        slider.setColour(juce::Slider::textBoxTextColourId,text);slider.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xff09111c));slider.setColour(juce::Slider::textBoxOutlineColourId,edge);
        if(index==vocalpro::DeEssHz||index==vocalpro::DeHarshHz)slider.setSkewFactorFromMidPoint(std::sqrt(static_cast<double>(spec.minimum)*spec.maximum));
        if(std::find(switches.begin(),switches.end(),index)!=switches.end()){
            slider.textFromValueFunction=[](double value){return value>=.5?juce::String("ON"):juce::String("OFF");};
            slider.valueFromTextFunction=[](const juce::String& value){return value.trim().equalsIgnoreCase("ON")?1.0:value.getDoubleValue()>=.5?1.0:0.0;};}
        else slider.setTextValueSuffix(juce::String(spec.unit).isEmpty()?juce::String{}:" "+juce::String(spec.unit));
        slider.setTooltip(juce::String(spec.name)+" - real live DSP parameter. Drag or type; double-click restores default.");
        slider.onValueChange=[this,index]{parameterChanged(index);};
    }
    refreshControls();
}
VocalCleanProPanel::~VocalCleanProPanel(){viewport.setViewedComponent(nullptr,false);}
builtin::SlotControl* VocalCleanProPanel::slot()const noexcept{
    if(channelIndex<0||channelIndex>=builtin::channelCount||slotIndex<0||slotIndex>=builtin::slotCount)return nullptr;
    auto& rack=outputDomain?engine.outputBuiltinControls():engine.builtinControls();return &rack.slots[static_cast<std::size_t>(channelIndex)][static_cast<std::size_t>(slotIndex)];
}
bool VocalCleanProPanel::valid()const noexcept{const auto* selected=slot();return selected&&selected->type.load(std::memory_order_acquire)==builtin::VocalClean&&
    selected->feedbackResetGeneration.load(std::memory_order_acquire)==expectedGeneration&&selected->vocalCleanPro.enabled.load(std::memory_order_acquire);}
vocalpro::Snapshot VocalCleanProPanel::current()const noexcept{const auto* selected=slot();return selected?vocalpro::snapshot(selected->vocalCleanPro):vocalpro::Snapshot{};}
void VocalCleanProPanel::parameterChanged(int index){
    if(refreshing||!valid()||index<0||index>=vocalpro::parameterCount)return;
    const auto& spec=vocalpro::specs()[static_cast<std::size_t>(index)];const float value=std::clamp(static_cast<float>(parameters[static_cast<std::size_t>(index)].getValue()),spec.minimum,spec.maximum);
    slot()->vocalCleanPro.params[static_cast<std::size_t>(index)].store(value,std::memory_order_release);updateStageButtons();for(auto& model:models)model->repaint();if(onChanged)onChanged();
}
void VocalCleanProPanel::toggleParameter(int index){if(!valid())return;auto& slider=parameters[static_cast<std::size_t>(index)];
    const bool enabled=slot()->vocalCleanPro.params[static_cast<std::size_t>(index)].load(std::memory_order_acquire)>=.5f;
    slider.setValue(enabled?0:1,juce::dontSendNotification);parameterChanged(index);}
void VocalCleanProPanel::updateStageButtons(){
    const bool available=valid();const auto value=current();for(std::size_t index=0;index<stageButtons.size();++index){const bool enabled=value.params[static_cast<std::size_t>(switches[index])]>=.5f;auto& button=stageButtons[index];
        button.setEnabled(available);button.setButtonText(juce::String(switchNames[index])+(enabled?" ON":" OFF"));button.setColour(juce::TextButton::buttonColourId,enabled?juce::Colour(0xff235866):juce::Colour(0xff263340));}
    stateText.setText(!available?"Slot changed or PRO mode off - reopen editor":value.params[vocalpro::Listen]>=.5f?"LISTEN REMOVED is ON - monitoring input minus processed, not the normal vocal output":"Normal vocal output  |  Auto Gain max +6 dB  |  Models are settings guides",juce::dontSendNotification);
    stateText.setColour(juce::Label::textColourId,value.params[vocalpro::Listen]>=.5f?juce::Colour(0xffffb177):muted);
}
void VocalCleanProPanel::refreshControls(){
    refreshing=true;const bool available=valid();const auto value=current();for(std::size_t index=0;index<parameters.size();++index){auto& slider=parameters[index];slider.setEnabled(available);
        if(!slider.isMouseButtonDown()&&!slider.hasKeyboardFocus(true))slider.setValue(value.params[index],juce::dontSendNotification);}
    refreshing=false;updateStageButtons();for(auto& model:models)model->repaint();
}
void VocalCleanProPanel::paint(juce::Graphics& g){g.fillAll(background);}
void VocalCleanProPanel::resized(){viewport.setBounds(getLocalBounds());content->setSize(std::max(730,getWidth()-viewport.getScrollBarThickness()),690);layoutContent();}
void VocalCleanProPanel::layoutContent(){
    auto area=content->getLocalBounds().reduced(6);cards[0]=area.removeFromTop(167);area.removeFromTop(8);
    auto first=area.removeFromTop(151);const int column=(first.getWidth()-16)/3;for(int index=1;index<=3;++index){cards[static_cast<std::size_t>(index)]=first.removeFromLeft(column);first.removeFromLeft(8);}area.removeFromTop(8);
    auto second=area.removeFromTop(151);cards[4]=second.removeFromLeft((second.getWidth()-8)/2);second.removeFromLeft(8);cards[5]=second;area.removeFromTop(8);cards[6]=area;
    auto hero=cards[0].reduced(8,5);titles[0].setBounds(hero.removeFromTop(21));explanation.setBounds(hero.removeFromBottom(17));stateText.setBounds(hero.removeFromBottom(17));
    auto clean=hero.removeFromLeft(154);labels[vocalpro::CleanAmount].setBounds(clean.removeFromTop(15));parameters[vocalpro::CleanAmount].setBounds(clean);
    const int globalWidth=(hero.getWidth()-12)/4;for(int index=0;index<4;++index){auto cell=hero.removeFromLeft(globalWidth);hero.removeFromLeft(4);const int p=globalControls[static_cast<std::size_t>(index)];
        labels[static_cast<std::size_t>(p)].setBounds(cell.removeFromTop(15));if(p==vocalpro::AutoGain||p==vocalpro::Listen){stageButtons[static_cast<std::size_t>(p==vocalpro::AutoGain?4:5)].setBounds(cell.removeFromTop(22));}parameters[static_cast<std::size_t>(p)].setBounds(cell);}
    const auto put=[&](int cardIndex,const int* indices,int count,int modelIndex,int switchIndex){auto cell=cards[static_cast<std::size_t>(cardIndex)].reduced(6,4);auto heading=cell.removeFromTop(20);
        if(switchIndex>=0){stageButtons[static_cast<std::size_t>(switchIndex)].setBounds(heading.removeFromRight(cardIndex==6?125:111));}titles[static_cast<std::size_t>(cardIndex)].setBounds(heading);
        if(modelIndex>=0){models[static_cast<std::size_t>(modelIndex)]->setBounds(cell.removeFromTop(cardIndex==6?48:51));cell.removeFromTop(2);}
        const int width=cell.getWidth()/count;for(int index=0;index<count;++index){const int p=indices[index];auto control=cell.withX(cell.getX()+index*width).withWidth(width).reduced(2,0);
            labels[static_cast<std::size_t>(p)].setBounds(control.removeFromTop(14));parameters[static_cast<std::size_t>(p)].setBounds(control);}};
    put(1,lowCutControls.data(),2,0,-1);put(2,gateControls.data(),3,1,0);put(3,toneControls.data(),2,-1,-1);
    put(4,deEssControls.data(),3,2,1);put(5,deHarshControls.data(),3,3,2);put(6,compressorControls.data(),6,4,3);
}
bool VocalCleanProPanel::runValidation(juce::String& error){
    if(engine.isPassThroughEnabled()||!valid()){error="Vocal Clean PRO validation requires a muted valid PRO target.";return false;}
    auto& selected=*slot();const auto saved=builtin::snapshotSlot(selected);const auto beforeInput=builtin::snapshot(engine.builtinControls()),beforeOutput=builtin::snapshot(engine.outputBuiltinControls());
    bool pass=true;juce::String failure;const auto check=[&](bool condition,const char* message){if(!condition&&pass){pass=false;failure=message;}};const auto& specs=vocalpro::specs();
    for(int index=0;index<vocalpro::parameterCount;++index){const auto& spec=specs[static_cast<std::size_t>(index)];for(float value:{spec.minimum,spec.maximum,spec.defaultValue}){
        parameters[static_cast<std::size_t>(index)].setValue(value,juce::sendNotificationSync);check(std::abs(selected.vocalCleanPro.params[static_cast<std::size_t>(index)].load()-value)<=std::max(.0001f,spec.step*.51f),"Vocal Clean PRO numeric control did not update its real DSP parameter.");}}
    for(std::size_t index=0;index<stageButtons.size();++index){const int parameter=switches[index];const float before=selected.vocalCleanPro.params[static_cast<std::size_t>(parameter)].load();stageButtons[index].onClick();
        check(selected.vocalCleanPro.params[static_cast<std::size_t>(parameter)].load()==(before>=.5f?0:1),"A Vocal Clean PRO stage/monitor button did not toggle its real parameter.");stageButtons[index].onClick();
        check(selected.vocalCleanPro.params[static_cast<std::size_t>(parameter)].load()==before,"A Vocal Clean PRO stage/monitor button did not restore its real parameter.");}
    parameters[vocalpro::CleanAmount].setValue(65,juce::sendNotificationSync);check(selected.vocalCleanPro.params[vocalpro::CleanAmount].load()==65,"CLEAN macro did not update the real treatment amount.");
    selected.vocalCleanPro.enabled.store(false);const auto disabled=vocalpro::snapshot(selected.vocalCleanPro);parameters[vocalpro::Wet].setValue(27,juce::sendNotificationSync);stageButtons[5].onClick();
    check(vocalpro::equal(disabled,vocalpro::snapshot(selected.vocalCleanPro)),"PRO mode off accepted a stale Vocal Clean callback.");selected.vocalCleanPro.enabled.store(true);
    const auto token=selected.feedbackResetGeneration.load();const auto stale=vocalpro::snapshot(selected.vocalCleanPro);selected.feedbackResetGeneration.fetch_add(1);
    parameters[vocalpro::Output].setValue(-7,juce::sendNotificationSync);stageButtons[4].onClick();check(vocalpro::equal(stale,vocalpro::snapshot(selected.vocalCleanPro)),"Replaced-slot generation accepted a stale Vocal Clean callback.");selected.feedbackResetGeneration.store(token);
    selected.type.store(builtin::Analog);parameters[vocalpro::DeEssAmount].setValue(11,juce::sendNotificationSync);stageButtons[1].onClick();
    check(vocalpro::equal(stale,vocalpro::snapshot(selected.vocalCleanPro)),"Another plugin type accepted a stale Vocal Clean callback.");selected.type.store(saved.type);
    vocalpro::recall(selected.vocalCleanPro,saved.vocalCleanPro);check(selected.enabled.load()==saved.enabled&&selected.type.load()==saved.type,"Vocal Clean PRO changed plugin type or ON/BYPASS.");
    for(bool output:{false,true}){const auto& before=output?beforeOutput:beforeInput;auto& rack=output?engine.outputBuiltinControls():engine.builtinControls();for(int channel=0;channel<builtin::channelCount;++channel)for(int insert=0;insert<builtin::slotCount;++insert)
        if(output!=outputDomain||channel!=channelIndex||insert!=slotIndex)check(sameSlot(builtin::snapshotSlot(rack.slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(insert)]),before[static_cast<std::size_t>(channel)][static_cast<std::size_t>(insert)]),"Vocal Clean PRO changed another input/output slot.");}
    refreshControls();if(onChanged)onChanged();if(!pass)error=failure;return pass;
}
} // namespace lah::ui

#include "DynamicsProPanel.h"
#include <algorithm>
#include <cmath>

namespace lah::ui {
namespace {
const juce::Colour background{0xff0c111a},panel{0xff121e2b},edge{0xff2c4052},text{0xffe7f4fa},muted{0xff91a9bb};
const std::array<juce::Colour,3> accents{{juce::Colour(0xff7cd8d0),juce::Colour(0xff60c9f5),juce::Colour(0xfff0c578)}};
constexpr std::array<int,3> stageIndices{{dynamicspro::GateOn,dynamicspro::CompOn,dynamicspro::LimiterOn}};
constexpr int contentHeight=748;
bool isStage(int p)noexcept{return p==dynamicspro::GateOn || p==dynamicspro::CompOn || p==dynamicspro::LimiterOn;}
}
class DynamicsProPanel::Content final : public juce::Component {
public:
    explicit Content(DynamicsProPanel& parent):owner(parent){setOpaque(true);}
    void paint(juce::Graphics& g)override {
        g.fillAll(background);
        for(std::size_t stage=0;stage<owner.cards.size();++stage){g.setColour(panel);g.fillRoundedRectangle(owner.cards[stage].toFloat(),6);g.setColour(edge);g.drawRoundedRectangle(owner.cards[stage].toFloat().reduced(.5f),6,1);}
        g.setColour(muted);g.setFont(10.f);g.drawFittedText("MONO  /  GATE → COMPRESSOR → MIX / OUTPUT → FINAL SAMPLE-PEAK LIMITER  /  NO LOOKAHEAD OR TRUE-PEAK MODE",getLocalBounds().reduced(10).removeFromBottom(19),juce::Justification::centred,1,.75f);
    }
private:DynamicsProPanel& owner;
};
class DynamicsProPanel::Model final : public juce::Component {
public:
    Model(DynamicsProPanel& parent,int index):owner(parent),stage(index){setInterceptsMouseClicks(false,false);}
    void paint(juce::Graphics& g)override {
        const auto bounds=getLocalBounds();const auto colour=accents[static_cast<std::size_t>(stage)];auto plot=bounds.toFloat().reduced(20,31).withTrimmedBottom(18);
        g.setColour(juce::Colour(0xff09121c));g.fillRoundedRectangle(bounds.toFloat(),4);
        g.setColour(colour);g.setFont(juce::FontOptions(10.f,juce::Font::bold));g.drawFittedText("STATIC INPUT / OUTPUT MODEL",bounds.reduced(6).removeFromTop(18),juce::Justification::centred,1,.8f);
        g.setColour(edge);for(int i=0;i<=4;++i){const auto x=plot.getX()+plot.getWidth()*i/4.f,y=plot.getY()+plot.getHeight()*i/4.f;g.drawVerticalLine(juce::roundToInt(x),plot.getY(),plot.getBottom());g.drawHorizontalLine(juce::roundToInt(y),plot.getX(),plot.getRight());}
        const auto settings=owner.current();juce::Path reference,response;
        for(int i=0;i<=200;++i){const double input=-100+106.*i/200;double output=input;
            if(settings.params[static_cast<std::size_t>(stageIndices[static_cast<std::size_t>(stage)])]>=.5f){
                if(stage==0)output-=dynamicspro::gateCurveAttenuationDb(input,settings);
                else if(stage==1)output-=dynamicspro::compressorCurveReductionDb(input,settings)-settings.params[dynamicspro::CompMakeup];
                else output=std::min(output,static_cast<double>(settings.params[dynamicspro::LimiterCeiling]));
            }
            const float x=plot.getX()+plot.getWidth()*i/200.f,y0=plot.getBottom()-plot.getHeight()*static_cast<float>((input+100)/106),y1=plot.getBottom()-plot.getHeight()*static_cast<float>((std::clamp(output,-100.,6.)+100)/106);
            if(!i){reference.startNewSubPath(x,y0);response.startNewSubPath(x,y1);}else{reference.lineTo(x,y0);response.lineTo(x,y1);}
        }
        g.setColour(muted.withAlpha(.4f));g.strokePath(reference,juce::PathStrokeType(1));g.setColour(colour);g.strokePath(response,juce::PathStrokeType(1.8f));
        g.setFont(10.f);g.setColour(muted);g.drawText("-100",0,juce::roundToInt(plot.getBottom())-7,31,13,juce::Justification::centredLeft);g.drawText("+6",0,juce::roundToInt(plot.getY())-5,24,13,juce::Justification::centredLeft);
        g.drawText("INPUT dBFS",juce::roundToInt(plot.getX()),juce::roundToInt(plot.getBottom())+2,juce::roundToInt(plot.getWidth()),13,juce::Justification::centred);
        const auto reductions=owner.engine.builtinDynamicsProReduction(owner.channelIndex,owner.slotIndex,owner.outputDomain);
        const float reduction=stage==0?reductions.gateDb:stage==1?reductions.compressorDb:reductions.limiterDb;
        auto meter=bounds.reduced(10,3).removeFromBottom(16);g.setColour(edge);g.fillRoundedRectangle(meter.toFloat(),2);g.setColour(colour.withAlpha(.6f));g.fillRoundedRectangle(meter.withWidth(juce::roundToInt(meter.getWidth()*std::clamp(reduction/60.f,0.f,1.f))).toFloat(),2);
        g.setColour(text);g.drawText(reductions.generation?"LIVE GR  "+juce::String(reduction,1)+" dB":"LIVE GR  —  NO AUDIO DATA",meter,juce::Justification::centred);
    }
private:DynamicsProPanel& owner;const int stage;
};
DynamicsProPanel::DynamicsProPanel(audio::LiveAudioEngine& audio,int channel,int insert,bool output)
    :engine(audio),channelIndex(channel),slotIndex(insert),outputDomain(output),
     expectedGeneration(slot()?slot()->feedbackResetGeneration.load(std::memory_order_acquire):0),content(std::make_unique<Content>(*this)) {
    setName("dynamics-pro-panel");addAndMakeVisible(viewport);viewport.setViewedComponent(content.get(),false);viewport.setScrollBarsShown(true,true);viewport.setScrollBarThickness(12);
    for(std::size_t stage=0;stage<stageButtons.size();++stage){auto& button=stageButtons[stage];content->addAndMakeVisible(button);content->addAndMakeVisible(stageTitles[stage]);
        button.setName("dynamics-pro-stage-"+juce::String(static_cast<int>(stage)));button.onClick=[this,stage]{toggleStage(stageIndices[stage]);};
        stageTitles[stage].setText(stage==0?"GATE / DOWNWARD EXPANDER":stage==1?"SOFT-KNEE COMPRESSOR":"FINAL SAMPLE-PEAK LIMITER + PARALLEL MIX",juce::dontSendNotification);
        stageTitles[stage].setFont(juce::FontOptions(13.f,juce::Font::bold));stageTitles[stage].setColour(juce::Label::textColourId,accents[stage]);stageTitles[stage].setBorderSize(juce::BorderSize<int>(0));
        models[stage]=std::make_unique<Model>(*this,static_cast<int>(stage));content->addAndMakeVisible(*models[stage]);
    }
    const auto& specs=dynamicspro::parameterSpecs();
    for(int p=0;p<dynamicspro::parameterCount;++p){if(isStage(p))continue;const auto i=static_cast<std::size_t>(p);auto& slider=parameters[i];auto& label=labels[i];const auto& spec=specs[i];
        content->addAndMakeVisible(slider);content->addAndMakeVisible(label);slider.setName("dynamics-pro-parameter-"+juce::String(p));label.setText(juce::String::fromUTF8(spec.name).toUpperCase(),juce::dontSendNotification);
        label.setFont(juce::FontOptions(10.f,juce::Font::bold));label.setColour(juce::Label::textColourId,muted);label.setJustificationType(juce::Justification::centred);label.setBorderSize(juce::BorderSize<int>(0));label.setMinimumHorizontalScale(.8f);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);slider.setRotaryParameters(1.2f*juce::MathConstants<float>::pi,2.8f*juce::MathConstants<float>::pi,true);slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,94,23);
        slider.setRange(spec.minimum,spec.maximum,spec.step);if(juce::String(spec.unit)=="ms" && spec.maximum>100)slider.setSkewFactorFromMidPoint(std::sqrt(std::max(.1,static_cast<double>(spec.minimum))*spec.maximum));
        slider.setTextValueSuffix(juce::String(spec.unit).isEmpty()?juce::String():" "+juce::String::fromUTF8(spec.unit));slider.setDoubleClickReturnValue(true,spec.defaultValue);
        slider.setColour(juce::Slider::rotarySliderFillColourId,accents[p<dynamicspro::CompOn?0:p<dynamicspro::LimiterOn?1:2]);slider.onValueChange=[this,p]{parameterChanged(p);};
        slider.setTooltip(juce::String::fromUTF8(spec.name)+": drag or type a value. Double-click restores default. Final limiter follows mix and output trim.");
    }
    refreshControls();
}
DynamicsProPanel::~DynamicsProPanel(){viewport.setViewedComponent(nullptr,false);}
builtin::SlotControl* DynamicsProPanel::slot()const noexcept {
    if(channelIndex<0 || channelIndex>=32 || slotIndex<0 || slotIndex>=4)return nullptr;
    auto& controls=outputDomain?engine.outputBuiltinControls():engine.builtinControls();return &controls.slots[static_cast<std::size_t>(channelIndex)][static_cast<std::size_t>(slotIndex)];
}
bool DynamicsProPanel::valid()const noexcept {const auto* target=slot();return target && target->type.load(std::memory_order_acquire)==builtin::Dynamics && target->feedbackResetGeneration.load(std::memory_order_acquire)==expectedGeneration && target->dynamicsPro.enabled.load(std::memory_order_acquire);}
dynamicspro::Snapshot DynamicsProPanel::current()const noexcept {return slot()?dynamicspro::snapshot(slot()->dynamicsPro):dynamicspro::Snapshot{};}
void DynamicsProPanel::parameterChanged(int p){
    if(refreshing || !valid())return;const auto index=static_cast<std::size_t>(p);const auto& spec=dynamicspro::parameterSpecs()[index];
    slot()->dynamicsPro.params[index].store(std::clamp(static_cast<float>(parameters[index].getValue()),spec.minimum,spec.maximum),std::memory_order_release);
    for(auto& model:models)model->repaint();if(onChanged)onChanged();
}
void DynamicsProPanel::toggleStage(int p){if(!valid())return;auto& value=slot()->dynamicsPro.params[static_cast<std::size_t>(p)];value.store(value.load(std::memory_order_acquire)>=.5f?0.f:1.f,std::memory_order_release);refreshControls();if(onChanged)onChanged();}
void DynamicsProPanel::refreshControls(){
    refreshing=true;const bool available=valid();const auto state=current();
    for(int p=0;p<dynamicspro::parameterCount;++p){if(isStage(p))continue;auto& slider=parameters[static_cast<std::size_t>(p)];slider.setEnabled(available);if(!slider.isMouseButtonDown() && !slider.hasKeyboardFocus(true))slider.setValue(state.params[static_cast<std::size_t>(p)],juce::dontSendNotification);}
    for(std::size_t stage=0;stage<stageButtons.size();++stage){auto& button=stageButtons[stage];const bool enabled=state.params[static_cast<std::size_t>(stageIndices[stage])]>=.5f;button.setEnabled(available);button.setButtonText(enabled?"STAGE ON":"STAGE OFF");button.setColour(juce::TextButton::buttonColourId,enabled?accents[stage].darker(2.1f):juce::Colour(0xff293544));models[stage]->repaint();}
    refreshing=false;repaint();
}
void DynamicsProPanel::paint(juce::Graphics& g){g.fillAll(background);}
void DynamicsProPanel::resized(){viewport.setBounds(getLocalBounds());layoutContent();}
void DynamicsProPanel::layoutContent(){
    const int width=std::max(740,viewport.getWidth()-14);content->setSize(width,contentHeight);
    cards[0]={8,8,width-16,246};cards[1]={8,264,width-16,246};cards[2]={8,520,width-16,200};
    for(std::size_t stage=0;stage<cards.size();++stage){const auto card=cards[stage];stageTitles[stage].setBounds(card.getX()+12,card.getY()+7,card.getWidth()-130,25);stageButtons[stage].setBounds(card.getRight()-110,card.getY()+9,98,24);models[stage]->setBounds(card.getX()+12,card.getY()+42,195,card.getHeight()-54);
        const int first=stage==0?dynamicspro::GateThreshold:stage==1?dynamicspro::CompThreshold:dynamicspro::LimiterCeiling;
        const int count=stage==2?4:6,columns=stage==2?2:3,columnWidth=(card.getWidth()-230)/columns,rowHeight=(card.getHeight()-46)/2;
        for(int n=0;n<count;++n){const int p=first+n;auto cell=juce::Rectangle<int>(card.getX()+218+(n%columns)*columnWidth,card.getY()+40+(n/columns)*rowHeight,columnWidth,rowHeight).reduced(3,0);labels[static_cast<std::size_t>(p)].setBounds(cell.removeFromTop(17));parameters[static_cast<std::size_t>(p)].setBounds(cell);}
    }
}
bool DynamicsProPanel::runValidation(juce::String& error){
    if(engine.isPassThroughEnabled() || !valid()){error="Dynamics PRO validation requires a muted, enabled PRO target.";return false;}
    const auto saved=current();bool success=true;const auto fail=[&](const juce::String& reason){if(success)error=reason;success=false;};
    for(int p=0;p<dynamicspro::parameterCount;++p){if(isStage(p))continue;const auto i=static_cast<std::size_t>(p);const auto& spec=dynamicspro::parameterSpecs()[i];auto& slider=parameters[i];
        slider.setValue(spec.minimum+(spec.maximum-spec.minimum)*.67f,juce::sendNotificationSync);if(std::abs(slot()->dynamicsPro.params[i].load()-static_cast<float>(slider.getValue()))>.0001f)fail("Dynamics PRO knob did not reach selected DSP control.");
        slider.setValue(static_cast<double>(spec.maximum)+1000,juce::sendNotificationSync);if(std::abs(slot()->dynamicsPro.params[i].load()-spec.maximum)>.0001f)fail("Dynamics PRO maximum bound failed.");
        slider.setValue(static_cast<double>(spec.minimum)-1000,juce::sendNotificationSync);if(std::abs(slot()->dynamicsPro.params[i].load()-spec.minimum)>.0001f)fail("Dynamics PRO minimum bound failed.");
    }
    for(std::size_t stage=0;stage<stageButtons.size();++stage){const auto i=static_cast<std::size_t>(stageIndices[stage]);const auto value=slot()->dynamicsPro.params[i].load();stageButtons[stage].onClick();if(slot()->dynamicsPro.params[i].load()==value)fail("Dynamics PRO stage enable failed.");stageButtons[stage].onClick();}
    dynamicspro::recall(slot()->dynamicsPro,saved);refreshControls();if(onChanged)onChanged();return success;
}
}

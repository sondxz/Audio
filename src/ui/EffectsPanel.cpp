#include "EffectsPanel.h"
#include <algorithm>
namespace lah::ui {
namespace {
const juce::Colour effectsBackground {0xff101318},effectsCard {0xff1a1f27},effectsBorder {0xff303a46};
const juce::Colour effectsText {0xffe7edf4},effectsMuted {0xffa5b3c3},effectsAccent {0xff27d3b4};
}
EffectsPanel::EffectsPanel(dsp::Controls& controls):c(controls){
    setOpaque(true);
    for(auto* l:{&title,&warning,&sendLabel,&sourceLabel,&sendHelp,&delayHelp,&reverbHelp}) {
        addAndMakeVisible(*l);
        l->setColour(juce::Label::textColourId,effectsMuted);
        l->setFont(juce::FontOptions(13.5f));
        l->setMinimumHorizontalScale(1.0f);
        l->setBorderSize(juce::BorderSize<int>(0));
    }
    title.setText("STEREO EFFECTS",juce::dontSendNotification);
    title.setFont(juce::FontOptions(19.0f,juce::Font::bold));
    title.setColour(juce::Label::textColourId,effectsText);
    warning.setText(juce::String::fromUTF8("FX returns chỉ xuất ra OUTPUT 1/2. FX đơn giản, chưa thay thế reverb chuyên nghiệp."),juce::dontSendNotification);
    warning.setColour(juce::Label::textColourId,juce::Colour(0xffedbe76));
    warning.setFont(juce::FontOptions(13.0f));
    sendLabel.setText("Select an input send, then shape the shared delay and reverb returns.",juce::dontSendNotification);
    sourceLabel.setText("SOURCE INPUT",juce::dontSendNotification);
    sendHelp.setText("Each input has its own send level.\nAll sends feed the same stereo FX bus.",juce::dontSendNotification);
    delayHelp.setText("Time and feedback apply to the shared delay.",juce::dontSendNotification);
    reverbHelp.setText("Shared stereo reverb return.\nReturns are mixed into output 1 / 2.",juce::dontSendNotification);
    addAndMakeVisible(source);
    for(int i=0;i<dsp::channels;++i)source.addItem("IN "+juce::String(i+1),i+1);
    source.setSelectedId(1,juce::dontSendNotification);
    const char* names[]={"Input FX send (0 - 1)","Delay return (0 - 1)","Reverb return (0 - 1)",
                          "Delay time (ms)","Feedback (0 - 0.85)"};
    const double hi[]={1,1,1,1800,0.85},lo[]={0,0,0,10,0},step[]={0.01,0.01,0.01,1,0.01};
    for(int j=0;j<5;++j){
        auto& label=labels[static_cast<std::size_t>(j)];auto& slider=slides[static_cast<std::size_t>(j)];
        addAndMakeVisible(label);addAndMakeVisible(slider);
        label.setText(names[j],juce::dontSendNotification);
        label.setColour(juce::Label::textColourId,effectsText);
        label.setFont(juce::FontOptions(14.0f));
        label.setBorderSize(juce::BorderSize<int>(0));
        label.setMinimumHorizontalScale(1.0f);
        slider.setRange(lo[j],hi[j],step[j]);
        slider.setSliderStyle(j==0||j==2?juce::Slider::RotaryHorizontalVerticalDrag:juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(j==0||j==2?juce::Slider::TextBoxBelow:juce::Slider::TextBoxRight,false,80,30);
        slider.setColour(juce::Slider::thumbColourId,effectsAccent);
        slider.setColour(juce::Slider::rotarySliderFillColourId,effectsAccent);
        slider.setColour(juce::Slider::rotarySliderOutlineColourId,juce::Colour(0xff34414d));
        slider.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xff11161d));
        slider.setColour(juce::Slider::textBoxOutlineColourId,effectsBorder);
        slider.onValueChange=[this,j]{
            const auto value=static_cast<float>(slides[static_cast<std::size_t>(j)].getValue());
            switch(j){case 0:c.fxSend[std::max(0,source.getSelectedId()-1)].store(value);break;
                case 1:c.delayReturn.store(value);break;case 2:c.reverbReturn.store(value);break;
                case 3:c.fxDelayMs.store(value);break;default:c.fxFeedback.store(value);break;}
        };
    }
    source.onChange=[this]{slides[0].setValue(c.fxSend[std::max(0,source.getSelectedId()-1)].load(),juce::dontSendNotification);repaint(flowCard);};
    slides[0].setValue(0,juce::dontSendNotification);slides[1].setValue(c.delayReturn.load(),juce::dontSendNotification);
    slides[2].setValue(c.reverbReturn.load(),juce::dontSendNotification);
    slides[3].setValue(c.fxDelayMs.load(),juce::dontSendNotification);
    slides[4].setValue(c.fxFeedback.load(),juce::dontSendNotification);
}
void EffectsPanel::refreshControls(){
    slides[0].setValue(c.fxSend[std::max(0,source.getSelectedId()-1)].load(),juce::dontSendNotification);
    slides[1].setValue(c.delayReturn.load(),juce::dontSendNotification);
    slides[2].setValue(c.reverbReturn.load(),juce::dontSendNotification);
    slides[3].setValue(c.fxDelayMs.load(),juce::dontSendNotification);
    slides[4].setValue(c.fxFeedback.load(),juce::dontSendNotification);
}
void EffectsPanel::paint(juce::Graphics& g){
    g.fillAll(effectsBackground);
    for(const auto& card:{sendCard,delayCard,reverbCard,flowCard}) {
        g.setColour(effectsCard);g.fillRoundedRectangle(card.toFloat(),10.0f);
        g.setColour(effectsBorder);g.drawRoundedRectangle(card.toFloat().reduced(0.5f),10.0f,1.0f);
    }
    auto drawHeading=[&](const juce::Rectangle<int>& card,const juce::String& text) {
        g.setColour(effectsAccent);g.fillRoundedRectangle(static_cast<float>(card.getX()+18),static_cast<float>(card.getY()+12),24.0f,3.0f,1.5f);
        g.setColour(effectsText);g.setFont(juce::FontOptions(15.0f,juce::Font::bold));
        g.drawText(text,card.getX()+18,card.getY()+24,card.getWidth()-36,25,juce::Justification::centredLeft);
    };
    drawHeading(sendCard,"INPUT SEND");drawHeading(delayCard,"STEREO DELAY");drawHeading(reverbCard,"STEREO REVERB");
    auto flow=flowCard.reduced(20,10);
    const int stageWidth=(flow.getWidth()-88)/3;
    const juce::String stages[]={"INPUT "+juce::String(source.getSelectedId()),"SHARED FX BUS","OUTPUT 1 / 2"};
    const juce::String descriptions[]={"Individual send level","Stereo delay + reverb","Fixed stereo return"};
    for(int j=0;j<3;++j) {
        auto stage=flow.removeFromLeft(stageWidth);
        g.setColour(j==2?effectsAccent:effectsText);g.setFont(juce::FontOptions(14.0f,juce::Font::bold));
        g.drawText(stages[j],stage.removeFromTop(23),juce::Justification::centred);
        g.setColour(effectsMuted);g.setFont(juce::FontOptions(13.0f));
        g.drawText(descriptions[j],stage,juce::Justification::centred);
        if(j<2) {
            g.setColour(effectsAccent);g.setFont(juce::FontOptions(20.0f));
            g.drawText(juce::String::fromUTF8("→"),flow.removeFromLeft(44),juce::Justification::centred);
        }
    }
    g.setColour(juce::Colour(0xff2b251e));
    g.fillRoundedRectangle(warning.getBounds().expanded(12,5).toFloat(),7.0f);
}
void EffectsPanel::resized(){
    auto area=getLocalBounds().reduced(18);
    title.setBounds(area.removeFromTop(28));
    sendLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(14);
    flowCard=area.removeFromTop(68);
    area.removeFromTop(16);
    warning.setBounds(area.removeFromBottom(40).reduced(12,0));
    area.removeFromBottom(16);
    const int usable=area.getWidth()-32;
    sendCard=area.removeFromLeft(juce::roundToInt(usable*0.29f));area.removeFromLeft(16);
    delayCard=area.removeFromLeft(juce::roundToInt(usable*0.42f));area.removeFromLeft(16);
    reverbCard=area;
    auto send=sendCard.reduced(18);send.removeFromTop(43);
    sourceLabel.setBounds(send.removeFromTop(20));
    source.setBounds(send.removeFromTop(34));send.removeFromTop(12);
    sendHelp.setBounds(send.removeFromBottom(49));
    labels[0].setBounds(send.removeFromTop(23));
    slides[0].setBounds(send.reduced(0,4));
    auto delay=delayCard.reduced(18);delay.removeFromTop(43);
    delayHelp.setBounds(delay.removeFromBottom(44));
    const int rowHeight=delay.getHeight()/3;
    for(int j:{1,3,4}) {
        auto row=delay.removeFromTop(rowHeight);
        labels[static_cast<std::size_t>(j)].setBounds(row.removeFromTop(23));
        slides[static_cast<std::size_t>(j)].setBounds(row.removeFromTop(38));
    }
    auto reverb=reverbCard.reduced(18);reverb.removeFromTop(43);
    reverbHelp.setBounds(reverb.removeFromBottom(49));
    labels[2].setBounds(reverb.removeFromTop(23));
    slides[2].setBounds(reverb.reduced(0,4));
}
}

#include "MixerPanel.h"
#include <algorithm>
#include <cmath>

namespace lah::ui {
namespace {
const juce::Colour mixerBackground {0xff0c1012}, mixerCard {0xff141a1d}, mixerBorder {0xff30383c};
const juce::Colour mixerText {0xffe7edf4}, mixerMuted {0xff91a5ad}, mixerAccent {0xff40d3ed};
juce::String boundedChannelName(juce::String value,int channel){
    value=value.replaceCharacters("\r\n\t","   ").trim().substring(0,32);
    return value.isEmpty()?"CH"+juce::String(channel+1):value;
}
class MixerFaderLookAndFeel final:public juce::LookAndFeel_V4 {
public:
    MixerFaderLookAndFeel(){
        setDefaultSansSerifTypefaceName("Segoe UI");
        setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff10181d));
        setColour(juce::ComboBox::textColourId,mixerText);
        setColour(juce::ComboBox::outlineColourId,mixerBorder);
        setColour(juce::PopupMenu::backgroundColourId,juce::Colour(0xff182126));
        setColour(juce::PopupMenu::textColourId,mixerText);
        setColour(juce::PopupMenu::highlightedBackgroundColourId,juce::Colour(0xff21515b));
    }
    juce::Font getTextButtonFont(juce::TextButton& button,int height)override{
        return juce::FontOptions(button.getWidth()<36?8.5f:height<=27?10.5f:12.0f);
    }
    juce::Font getLabelFont(juce::Label& label)override{return label.getFont();}
    juce::Font getComboBoxFont(juce::ComboBox& box)override{return juce::FontOptions(box.getWidth()<145?11.0f:12.0f);}
    void drawButtonBackground(juce::Graphics& g,juce::Button& button,const juce::Colour& colour,bool over,bool down)override{
        const auto bounds=button.getLocalBounds().toFloat().reduced(.5f);
        g.setColour(colour.withMultipliedBrightness(down?.8f:over?1.2f:1.0f).withMultipliedAlpha(button.isEnabled()?1.0f:.4f));
        g.fillRoundedRectangle(bounds,2);
        g.setColour(mixerBorder.withAlpha(button.isEnabled()?1.0f:.4f));g.drawRoundedRectangle(bounds,2,1);
    }
    void drawButtonText(juce::Graphics& g,juce::TextButton& button,bool,bool)override{
        g.setFont(getTextButtonFont(button,button.getHeight()));
        g.setColour(button.findColour(juce::TextButton::textColourOffId).withMultipliedAlpha(button.isEnabled()?1.0f:.4f));
        g.drawFittedText(button.getButtonText(),button.getLocalBounds().reduced(3,2),juce::Justification::centred,1,.8f);
    }
    void positionComboBoxText(juce::ComboBox& box,juce::Label& label)override{
        label.setBounds(5,1,std::max(1,box.getWidth()-23),std::max(1,box.getHeight()-2));
        label.setFont(getComboBoxFont(box));
    }
    void drawLinearSlider(juce::Graphics& g,int x,int y,int width,int height,float sliderPosition,
                          float minPosition,float maxPosition,juce::Slider::SliderStyle style,juce::Slider& slider)override {
        if(style!=juce::Slider::LinearVertical) {
            juce::LookAndFeel_V4::drawLinearSlider(g,x,y,width,height,sliderPosition,minPosition,maxPosition,style,slider);
            return;
        }
        const float centre=static_cast<float>(x)+static_cast<float>(width)*0.5f;
        g.setColour(juce::Colour(0xff090d12));
        g.fillRoundedRectangle(centre-4.0f,static_cast<float>(y),8.0f,static_cast<float>(height),4.0f);
        g.setColour(mixerAccent.withAlpha(0.62f));
        g.fillRoundedRectangle(centre-2.0f,sliderPosition,4.0f,std::max(0.0f,static_cast<float>(y+height)-sliderPosition),2.0f);
        const float handleWidth=std::min(38.0f,static_cast<float>(width)-2.0f);
        const juce::Rectangle<float> handle(centre-handleWidth*0.5f,sliderPosition-12.0f,handleWidth,24.0f);
        g.setColour(juce::Colours::black.withAlpha(0.45f));g.fillRoundedRectangle(handle.translated(1.0f,2.0f),3.0f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff355d80),centre,handle.getY(),
            juce::Colour(0xff101e2b),centre,handle.getBottom(),false));
        g.fillRoundedRectangle(handle,3.0f);
        g.setColour(juce::Colour(0xff448dc7));g.drawRoundedRectangle(handle.reduced(0.5f),3.0f,1.0f);
        g.setColour(juce::Colour(0xff42617e));
        g.drawHorizontalLine(juce::roundToInt(handle.getY()+5),handle.getX()+4,handle.getRight()-4);
        g.drawHorizontalLine(juce::roundToInt(handle.getBottom()-5),handle.getX()+4,handle.getRight()-4);
        g.setColour(slider.isEnabled()?mixerAccent:mixerMuted);
        g.fillRect(handle.reduced(5.0f,0.0f).withSizeKeepingCentre(handleWidth-10.0f,2.0f));
    }
};
MixerFaderLookAndFeel& mixerFaderSkin(){
    // Font cache access requires the JUCE application to have initialized.
    // A namespace-scope skin can run before JUCE's own static state exists.
    static MixerFaderLookAndFeel skin;
    return skin;
}
void paintPeak(juce::Graphics& g,juce::Rectangle<int> bounds,float peak,bool available) {
    g.setColour(juce::Colour(0xff0b1015));
    g.fillRoundedRectangle(bounds.toFloat(),3.0f);
    if(!available)return;
    const float db=20.0f*std::log10(std::max(peak,0.00001f));
    const float proportion=std::clamp((db+60.0f)/60.0f,0.0f,1.0f);
    auto fill=bounds.toFloat().reduced(2.0f);
    fill.removeFromTop(fill.getHeight()*(1.0f-proportion));
    g.setColour(peak>=1.0f?juce::Colour(0xfff0767e)
        :(peak>=0.7f?juce::Colour(0xffeaba71):mixerAccent));
    if(proportion>0.0f)g.fillRoundedRectangle(fill,2.0f);
    g.setColour(mixerCard);
    for(int n=1;n<12;++n)
        g.drawHorizontalLine(bounds.getBottom()-n*bounds.getHeight()/12,
            static_cast<float>(bounds.getX()),static_cast<float>(bounds.getRight()));
}
}

class MixerPanel::ChannelStrip final:public juce::Component {
public:
    ChannelStrip(MixerPanel& panel,std::function<void(int,bool)> selection,
                 std::function<void(int,int)> openProcessors,std::function<void()> routingChanged)
        :owner(panel),engine(panel.engine),c(panel.control),select(std::move(selection)) {
        for(auto* component:std::array<juce::Component*,12>{&selectButton,&gain,&balance,&mute,&details,
            &sourceLabel,&balanceLabel,&meterLabel,&insertLabel,&destination,&channelName,&peakReset})
            addAndMakeVisible(*component);
        selectButton.onClick=[this]{select(channel,false);};
        selectButton.setTooltip("Select this mixer channel.");
        details.setButtonText("...");
        details.onClick=[this]{select(channel,true);};
        details.setTooltip("Open this channel's detailed DSP and routing matrix.");
        channelName.setEditable(false,true,false);
        channelName.setFont(juce::FontOptions(11.0f,juce::Font::bold));
        channelName.setColour(juce::Label::textColourId,mixerText);
        channelName.setJustificationType(juce::Justification::centred);
        channelName.setBorderSize(juce::BorderSize<int>(0));
        channelName.setTooltip("Double-click to rename this input channel. Names are saved in mixer scenes.");
        channelName.onEditorShow=[this]{if(auto* editor=channelName.getCurrentTextEditor())editor->setInputRestrictions(32);};
        channelName.onTextChange=[this]{
            owner.channelNames.set(channel,boundedChannelName(channelName.getText(),channel));
            channelName.setText(owner.channelNames[channel],juce::dontSendNotification);
            owner.refreshConsole();
        };
        peakReset.onClick=[this]{owner.inputPeakHolds[static_cast<std::size_t>(channel)]=0;owner.inputClips[static_cast<std::size_t>(channel)]=false;setPeak(peak,available,0,false);};
        peakReset.setTooltip("Real input peak hold since reset; CLIP latches at 0 dBFS. Click to reset this channel.");
        for(int slot=0;slot<4;++slot){
            auto& button=inserts[static_cast<std::size_t>(slot)];
            auto& bypass=bypassButtons[static_cast<std::size_t>(slot)];
            addAndMakeVisible(button);addAndMakeVisible(bypass);
            button.onClick=[this,slot,openProcessors]{openProcessors(channel,slot);};
            button.onContextMenu=[this,slot]{owner.showInsertMenu(channel,slot,false,inserts[static_cast<std::size_t>(slot)]);};
            bypass.onClick=[this,slot]{
                auto& processor=engine.builtinControls().slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slot)];
                if(processor.type.load()==builtin::None)return;
                const bool enabled=!processor.enabled.load();
                if(!enabled)processor.feedbackArmed.store(false,std::memory_order_release);
                processor.enabled.store(enabled,std::memory_order_release);refresh();
            };
        }
        gain.setSliderStyle(juce::Slider::LinearVertical);
        gain.setLookAndFeel(&mixerFaderSkin());
        gain.setTextBoxStyle(juce::Slider::TextBoxAbove,false,72,22);
        gain.setRange(-60,12,0.1);
        gain.setTextValueSuffix(" dB");
        gain.setDoubleClickReturnValue(true,0);
        gain.setColour(juce::Slider::thumbColourId,mixerAccent);
        gain.setColour(juce::Slider::trackColourId,mixerAccent);
        gain.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xff11161d));
        gain.setColour(juce::Slider::textBoxTextColourId,mixerText);
        gain.setColour(juce::Slider::textBoxOutlineColourId,mixerBorder);
        gain.onValueChange=[this]{c.inputGain[channel].store(static_cast<float>(gain.getValue()));};
        gain.setTooltip("Input gain before channel DSP and insert plugins. Use the selected output fader for return level. Double-click for 0 dB.");
        balance.setSliderStyle(juce::Slider::LinearHorizontal);
        balance.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        balance.setRange(-1,1,0.01);
        balance.setColour(juce::Slider::thumbColourId,mixerAccent);
        balance.setTooltip("Input balance: left / right on output 1 / 2.");
        balance.onValueChange=[this]{c.inputPan[channel].store(static_cast<float>(balance.getValue()));};
        mute.onClick=[this]{auto& value=c.inputMute[channel];value.store(value.load()>0.5f?0.0f:1.0f);refresh();};
        destination.addItem("Off",1);
        for(int output=0;output<32;++output)destination.addItem("Out "+juce::String(output+1),output+2);
        destination.addItem("Custom mix",100);
        destination.setItemEnabled(100,false);
        destination.setTooltip("USB return destination. Selecting one output replaces this input's matrix sends with a unity send.");
        destination.onChange=[this,routingChanged]{
            const int id=destination.getSelectedId();
            if(id<1||id>33)return;
            for(int output=0;output<32;++output)
                c.route[output*32+channel].store(id==output+2?1.0f:0.0f,std::memory_order_release);
            routingChanged();refresh();
        };
        for(auto* label:{&sourceLabel,&balanceLabel,&meterLabel,&insertLabel}) {
            label->setFont(juce::FontOptions(10.0f));
            label->setColour(juce::Label::textColourId,mixerMuted);
            label->setJustificationType(juce::Justification::centred);
            label->setBorderSize(juce::BorderSize<int>(0));
            label->setMinimumHorizontalScale(1.0f);
        }
        insertLabel.setText(juce::String::fromUTF8("PLUGIN CHÈN"),juce::dontSendNotification);
        balanceLabel.setText("PAN  L                 R",juce::dontSendNotification);
        setChannel(0,false);
    }
    ~ChannelStrip()override{gain.setLookAndFeel(nullptr);}
    void setChannel(int index,bool isSelected) {
        if(channel!=index){channelName.hideEditor(false);peak=0;available=false;}
        channel=index;selected=isSelected;
        channelName.setName("input-channel-name-"+juce::String(channel+1));
        selectButton.setButtonText("CH"+juce::String(channel+1));
        sourceLabel.setText("In "+juce::String(channel+1)+"  /  USB",juce::dontSendNotification);
        selectButton.setColour(juce::TextButton::buttonColourId,selected?juce::Colour(0xff1b414c):juce::Colour(0xff1b2327));
        refresh();
    }
    void setUsbEnabled(bool value){usbEnabled=value;refresh();}
    int getChannel()const{return channel;}
    void refresh() {
        if(!channelName.isBeingEdited())channelName.setText(owner.channelNames[channel],juce::dontSendNotification);
        gain.setValue(c.inputGain[channel].load(),juce::dontSendNotification);
        balance.setValue(c.inputPan[channel].load(),juce::dontSendNotification);
        const bool muted=c.inputMute[channel].load()>0.5f;
        mute.setButtonText(muted?"MUTED":"MUTE");
        mute.setColour(juce::TextButton::buttonColourId,muted?juce::Colour(0xff984452):juce::Colour(0xff303944));
        int routeCount=0,routeOutput=-1;float routeValue=0;
        for(int output=0;output<32;++output){
            const float value=c.route[output*32+channel].load();
            if(std::abs(value)>0.00001f){++routeCount;routeOutput=output;routeValue=value;}
        }
        destination.setSelectedId(routeCount==0?1:routeCount==1&&std::abs(routeValue-1.0f)<0.00001f?routeOutput+2:100,
                                  juce::dontSendNotification);
        for(int slot=0;slot<4;++slot){
            const auto& processor=engine.builtinControls().slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slot)];
            const int type=processor.type.load(std::memory_order_acquire);
            const bool enabled=type!=builtin::None&&processor.enabled.load(std::memory_order_acquire);
            auto& button=inserts[static_cast<std::size_t>(slot)];
            auto& bypass=bypassButtons[static_cast<std::size_t>(slot)];
            const auto name=juce::String::fromUTF8(builtin::metadata(type).name);
            button.setButtonText(type==builtin::None?juce::String(slot+1)+" +":name);
            button.setTooltip("CH "+juce::String(channel+1)+" / insert "+juce::String(slot+1)+": "+name+". Click to edit; right-click to move, copy, paste or remove.");
            button.setColour(juce::TextButton::buttonColourId,enabled?juce::Colour(0xff163735):juce::Colour(0xff10171b));
            button.setColour(juce::TextButton::textColourOffId,type==builtin::None?mixerMuted:mixerText);
            bypass.setButtonText(enabled?"ON":"OFF");
            bypass.setColour(juce::TextButton::buttonColourId,enabled?juce::Colour(0xff236149):juce::Colour(0xff1b252a));
            bypass.setColour(juce::TextButton::textColourOffId,enabled?juce::Colour(0xff7dedc2):mixerMuted);
            bypass.setEnabled(usbEnabled&&type!=builtin::None);
            bypass.setTooltip(enabled?"Bypass this processor":"Enable this processor");
            button.setEnabled(usbEnabled);
        }
        gain.setEnabled(usbEnabled);balance.setEnabled(usbEnabled);mute.setEnabled(usbEnabled);destination.setEnabled(usbEnabled);
        sourceLabel.setText("In "+juce::String(channel+1)+(usbEnabled?" / USB":" / inactive"),juce::dontSendNotification);
        sourceLabel.setColour(juce::Label::textColourId,usbEnabled?mixerMuted:juce::Colour(0xff52626a));
        repaint();
    }
    void setPeak(float value,bool isAvailable,float held,bool clip) {
        available=isAvailable;
        heldPeak=held;clipped=clip;
        peak=available?std::max(value,peak*0.8f):0.0f;
        meterLabel.setText(!available?juce::String::fromUTF8("—")
            :(peak<=0.00001f?"-inf":juce::String(20.0f*std::log10(peak),1)),juce::dontSendNotification);
        meterLabel.setColour(juce::Label::textColourId,peak>=1.0f?juce::Colour(0xfff0767e):mixerMuted);
        peakReset.setButtonText((clip?"CLIP ":"PK ")+(held>0.00001f?juce::String(20*std::log10(held),1):"-inf"));
        peakReset.setColour(juce::TextButton::buttonColourId,clip?juce::Colour(0xff8a303c):juce::Colour(0xff18262c));
        peakReset.setColour(juce::TextButton::textColourOffId,clip?juce::Colours::white:mixerMuted);
        repaint();
    }
    void resized()override {
        auto area=getLocalBounds().reduced(6,8);
        auto header=area.removeFromTop(27);
        details.setBounds(header.removeFromRight(22));header.removeFromRight(3);
        selectButton.setBounds(header);
        channelName.setBounds(area.removeFromTop(22));
        sourceLabel.setBounds(area.removeFromTop(22));
        area.removeFromTop(4);
        insertLabel.setBounds(area.removeFromTop(20));
        for(int slot=0;slot<4;++slot){
            auto row=area.removeFromTop(26);area.removeFromTop(3);
            bypassButtons[static_cast<std::size_t>(slot)].setBounds(row.removeFromRight(29));row.removeFromRight(2);
            inserts[static_cast<std::size_t>(slot)].setBounds(row);
        }
        area.removeFromTop(7);
        balanceLabel.setBounds(area.removeFromTop(14));
        balance.setBounds(area.removeFromTop(22));
        auto muteRow=area.removeFromTop(24);
        mute.setBounds(muteRow.removeFromLeft(std::min(62,muteRow.getWidth()*2/3)));
        meterLabel.setBounds(muteRow);
        area.removeFromTop(3);
        peakReset.setBounds(area.removeFromTop(21));
        area.removeFromTop(5);
        destination.setBounds(area.removeFromBottom(27));
        outputCaption=area.removeFromBottom(21);
        area.removeFromBottom(4);
        auto fader=area;
        meterBounds=fader.removeFromRight(10).withTrimmedTop(28).withTrimmedBottom(10);
        fader.removeFromRight(2);
        gain.setTextBoxStyle(juce::Slider::TextBoxAbove,false,std::max(40,std::min(74,fader.getWidth())),22);
        gain.setBounds(fader.withTrimmedLeft(17));
        tickBounds=fader.withWidth(20).withTrimmedTop(32).withTrimmedBottom(12);
    }
    void paint(juce::Graphics& g)override {
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff1c2427),0,0,mixerCard,0,static_cast<float>(getHeight()),false));
        g.fillRoundedRectangle(getLocalBounds().toFloat(),3.0f);
        g.setColour(selected?mixerAccent:mixerBorder);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f),3.0f,selected?1.5f:1.0f);
        g.setColour(mixerMuted);g.setFont(juce::FontOptions(9.0f));
        for(int db:{12,0,-12,-24,-36,-48,-60}) {
            const int y=juce::roundToInt(tickBounds.getY()+(12.0f-db)/72.0f*tickBounds.getHeight());
            g.drawText(juce::String(db),tickBounds.getX(),y-8,tickBounds.getWidth()-2,16,juce::Justification::centredRight);
        }
        paintPeak(g,meterBounds,peak,available);
        if(heldPeak>0&&!meterBounds.isEmpty()){
            const float proportion=std::clamp((20*std::log10(heldPeak)+60)/60,0.0f,1.0f);
            g.setColour(clipped?juce::Colour(0xffff7185):juce::Colour(0xffeaca75));
            g.drawHorizontalLine(juce::roundToInt(meterBounds.getBottom()-proportion*meterBounds.getHeight()),static_cast<float>(meterBounds.getX()),static_cast<float>(meterBounds.getRight()));
        }
        g.setFont(juce::FontOptions(10.0f,juce::Font::bold));g.setColour(mixerText);
        g.drawText(juce::String::fromUTF8("NGÕ RA / USB"),outputCaption,juce::Justification::centredLeft);
    }
private:
    MixerPanel& owner;
    audio::LiveAudioEngine& engine;
    dsp::Controls& c;
    std::function<void(int,bool)> select;
    int channel=0;
    bool selected=false,available=false,usbEnabled=true,clipped=false;
    float peak=0.0f,heldPeak=0.0f;
    juce::TextButton selectButton,mute,details,peakReset;
    std::array<RackInsertButton,4> inserts;
    std::array<juce::TextButton,4> bypassButtons;
    juce::ComboBox destination;
    juce::Slider gain,balance;
    juce::Label sourceLabel,balanceLabel,meterLabel,insertLabel,channelName;
    juce::Rectangle<int> meterBounds,tickBounds,outputCaption;
};

MixerPanel::MixerPanel(audio::LiveAudioEngine& e):engine(e),control(e.controls()) {
    setOpaque(true);
    setLookAndFeel(&mixerFaderSkin());
    for(int channel=0;channel<32;++channel)channelNames.add("CH"+juce::String(channel+1));
    addAndMakeVisible(stripsViewport);
    stripsViewport.setViewedComponent(&stripsContent,false);
    stripsViewport.setScrollBarsShown(true,true);
    stripsViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
    stripsViewport.setScrollBarThickness(12);
    for(auto* l:{&inputLabel,&outputLabel,&routeLabel,&noteLabel,&matLabel,
                 &inGroupLabel,&outGroupLabel,&selectedRoute,&matrixHint}) {
        l->setColour(juce::Label::textColourId,mixerText);
        l->setFont(juce::FontOptions(14.0f));
        l->setBorderSize(juce::BorderSize<int>(0));
        l->setMinimumHorizontalScale(1.0f);
        addAndMakeVisible(*l);
    }
    for(auto* l:{&inputLabel,&outputLabel,&matLabel})
        l->setFont(juce::FontOptions(16.0f,juce::Font::bold));
    inputLabel.setText("INPUT / CHANNEL DSP",juce::dontSendNotification);
    outputLabel.setText("OUTPUT / SPEAKER DSP",juce::dontSendNotification);
    routeLabel.setText("SELECTED MATRIX SEND",juce::dontSendNotification);
    routeLabel.setColour(juce::Label::textColourId,mixerAccent);
    matLabel.setText("ROUTING MATRIX",juce::dontSendNotification);
    inGroupLabel.setText("INPUTS / COLUMNS",juce::dontSendNotification);
    outGroupLabel.setText("OUTPUTS / ROWS",juce::dontSendNotification);
    for(auto* l:{&inGroupLabel,&outGroupLabel,&matrixHint}) {
        l->setFont(juce::FontOptions(13.0f));
        l->setColour(juce::Label::textColourId,mixerMuted);
    }
    matrixHint.setText("Click a cell to connect / disconnect.\nGreen = active; values are linear send levels.",juce::dontSendNotification);
    for(int j=0;j<8;++j) {
        for(auto* l:{&matrixInputs[static_cast<std::size_t>(j)],&matrixOutputs[static_cast<std::size_t>(j)]}) {
            addAndMakeVisible(*l);
            l->setFont(juce::FontOptions(13.0f));
            l->setColour(juce::Label::textColourId,mixerMuted);
            l->setBorderSize(juce::BorderSize<int>(0));
            l->setJustificationType(juce::Justification::centred);
            l->setMinimumHorizontalScale(1.0f);
        }
    }
    noteLabel.setText("USB IN > INPUT GAIN > INPUT PLUGINS > ROUTING > OUTPUT PLUGINS > OUTPUT FADER > USB OUT",juce::dontSendNotification);
    noteLabel.setColour(juce::Label::textColourId,mixerMuted);
    noteLabel.setFont(juce::FontOptions(11.0f));
    for(int i=0;i<dsp::channels;++i) {
        inSelect.addItem("IN "+juce::String(i+1),i+1);
        outSelect.addItem("OUT "+juce::String(i+1),i+1);
    }
    for(int p=0;p<4;++p) {
        const auto text="CH "+juce::String(p*8+1)+"-"+juce::String(p*8+8);
        inGroup.addItem(text,p+1);outGroup.addItem(text,p+1);
    }
    for(auto* cbox:{&inSelect,&outSelect,&inGroup,&outGroup})addAndMakeVisible(*cbox);
    inSelect.onChange=[this]{in=std::max(0,inSelect.getSelectedId()-1);selectedChannels();};
    outSelect.onChange=[this]{out=std::max(0,outSelect.getSelectedId()-1);selectedChannels();};
    inGroup.onChange=[this]{inPage=std::max(0,inGroup.getSelectedId()-1);reloadMatrix();};
    outGroup.onChange=[this]{outPage=std::max(0,outGroup.getSelectedId()-1);reloadMatrix();};
    inSelect.setSelectedId(1,juce::dontSendNotification);
    outSelect.setSelectedId(1,juce::dontSendNotification);
    inGroup.setSelectedId(1,juce::dontSendNotification);
    outGroup.setSelectedId(1,juce::dontSendNotification);
    auto inp=[this](int idx)->std::atomic<float>& {
        switch(idx) {
            case 0:return control.inputGain[in];case 1:return control.hpfHz[in];
            case 2:return control.eqHz[in];case 3:return control.eqDb[in];
            case 4:return control.eqQ[in];case 5:return control.compThreshold[in];
            case 6:return control.compRatio[in];default:return control.gateThreshold[in];
        }
    };
    const juce::String names[]={"Input gain (dB)","High-pass filter (Hz; 0 = off)","PEQ frequency (Hz)",
        "PEQ gain (dB)","PEQ Q", "Compressor threshold (dB)","Compressor ratio", "Expander / gate threshold (dB)"};
    const double mins[]={-60,0,20,-18,0.1,-60,1,-100};
    const double maxs[]={12,300,18000,18,10,0,20,0};
    const double steps[]={0.1,1,1,0.1,0.1,0.5,0.1,1};
    for(int j=0;j<8;++j) {
        addParameter(inputParams[j],names[j],mins[j],maxs[j],steps[j],
            [inp,j]{return inp(j).load();},[inp,j](float v){inp(j).store(v);});
    }
    auto op=[this](int idx)->std::atomic<float>&{
        switch(idx){case 0:return control.outputGain[out];case 1:return control.outputDelayMs[out];
            case 2:return control.outputCeiling[out];default:return control.outputPolarity[out];}
    };
    addParameter(outputParams[0],"Output Gain dB",-60,12,0.1,[op]{return op(0).load();},[op](float v){op(0).store(v);});
    addParameter(outputParams[1],"Speaker Delay ms",0,250,0.01,[op]{return op(1).load();},[op](float v){op(1).store(v);});
    addParameter(outputParams[2],"Safety Clip Ceiling dBFS",-24,0,0.5,[op]{return op(2).load();},[op](float v){op(2).store(v);});
    addParameter(outputParams[3],"Invert Output (0 / 1)",0,1,1,[op]{return op(3).load()<0?1.0f:0.0f;},[op](float v){op(3).store(v>=0.5f?-1.0f:1.0f);});
    addParameter(routeGain,"Send Level (linear; -2..+2)",-2,2,0.01,
        [this]{return control.route[out*dsp::channels+in].load();},
        [this](float v){control.route[out*dsp::channels+in].store(v);reloadMatrix();});
    addParameter(pan,"Input balance / output 1-2 L/R",-1,1,0.01,
        [this]{return control.inputPan[in].load();},
        [this](float v){control.inputPan[in].store(v);});
    addAndMakeVisible(muteInput);
    muteInput.setTooltip("Mute the selected input channel.");
    muteInput.onClick=[this]{
        auto& mute=control.inputMute[in];mute.store(mute.load()>0.5f?0.0f:1.0f);
        selectedChannels();
    };
    addAndMakeVisible(polarity);
    polarity.onClick=[this]{
        auto& v=control.outputPolarity[out];v.store(v.load()<0?1.0f:-1.0f);selectedChannels();
    };
    for(int j=0;j<64;++j) {
        auto& b=matrixButtons[static_cast<std::size_t>(j)];addAndMakeVisible(b);
        b.setColour(juce::TextButton::textColourOffId,mixerText);
        const int row=j/8,col=j%8;
        b.onClick=[this,row,col]{
            const int o=outPage*8+row,i=inPage*8+col;
            auto& r=control.route[o*dsp::channels+i];
            r.store(std::abs(r.load())>0.00001f?0.0f:1.0f);
            outSelect.setSelectedId(o+1,juce::dontSendNotification);
            inSelect.setSelectedId(i+1,juce::dontSendNotification);
            in=i;out=o;selectedChannels();
        };
    }
    for(auto* component:std::array<juce::Component*,14>{&stripViewButton,&detailViewButton,&stripBank,&bankLabel,&masterTitle,&insertNote,&audioState,&audioPower,&panic,&eightView,&sixteenView,&resetPeaks,&chainTitle,&outputPeakReset})
        addAndMakeVisible(*component);
    stripViewButton.onClick=[this]{setStripView(true);};
    detailViewButton.onClick=[this]{setStripView(false);};
    eightView.onClick=[this]{setChannelViewCount(8);};
    sixteenView.onClick=[this]{setChannelViewCount(16);};
    resetPeaks.onClick=[this]{resetPeakHolds();};
    resetPeaks.setTooltip("Reset all real input/output peak holds and latched clip indicators.");
    rebuildStripBank();
    stripBank.onChange=[this]{
        stripPage=std::max(0,stripBank.getSelectedId()-1);
        selectedChannels();stripsViewport.setViewPosition(0,0);
    };
    for(int n=0;n<16;++n) {
        auto& strip=channelStrips[static_cast<std::size_t>(n)];
        strip=std::make_unique<ChannelStrip>(*this,[this](int channel,bool details){
            in=channel;inSelect.setSelectedId(in+1,juce::dontSendNotification);
            inPage=in/8;inGroup.setSelectedId(inPage+1,juce::dontSendNotification);
            selectedChannels();
            if(details)setStripView(false);
        },[this](int channel,int slot){if(onOpenBuiltin)onOpenBuiltin(channel,slot,false);},[this]{reloadMatrix();});
        stripsContent.addAndMakeVisible(*strip);
    }
    chainTitle.setFont(juce::FontOptions(11.0f,juce::Font::bold));
    chainTitle.setColour(juce::Label::textColourId,mixerAccent);
    chainTitle.setBorderSize(juce::BorderSize<int>(0));
    for(int slot=0;slot<4;++slot){
        auto& button=chainOverview[static_cast<std::size_t>(slot)];
        addAndMakeVisible(button);
        button.onClick=[this,slot]{if(onOpenBuiltin)onOpenBuiltin(in,slot,false);};
        button.onContextMenu=[this,slot]{showInsertMenu(in,slot,false,chainOverview[static_cast<std::size_t>(slot)]);};
    }
    for(auto* label:{&bankLabel,&masterTitle,&insertNote,&audioState}) {
        label->setFont(juce::FontOptions(13.0f));
        label->setColour(juce::Label::textColourId,mixerMuted);
        label->setBorderSize(juce::BorderSize<int>(0));
        label->setMinimumHorizontalScale(1.0f);
    }
    bankLabel.setText("CHANNEL BANK",juce::dontSendNotification);
    masterTitle.setText("USB OUTPUT",juce::dontSendNotification);
    masterTitle.setJustificationType(juce::Justification::centred);
    masterTitle.setFont(juce::FontOptions(15.0f,juce::Font::bold));
    masterTitle.setColour(juce::Label::textColourId,mixerText);
    insertNote.setText(juce::String::fromUTF8("PLUGIN CHÈN / OUTPUT"),juce::dontSendNotification);
    insertNote.setFont(juce::FontOptions(10.0f));
    audioState.setJustificationType(juce::Justification::centred);
    audioState.setFont(juce::FontOptions(11.0f,juce::Font::bold));
    audioPower.onClick=[this]{if(onToggleAudio)onToggleAudio();};
    panic.onClick=[this]{if(onPanic)onPanic();else engine.setPassThroughEnabled(false);refreshConsole();};
    panic.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff452630));
    panic.setTooltip("Immediately mute all computer outputs and discard retained effect tails.");
    audioPower.setTooltip("Enable or mute USB audio processing.");
    outputPeakReset.onClick=[this]{outputPeakHolds[static_cast<std::size_t>(out)]=0;outputClips[static_cast<std::size_t>(out)]=false;refreshConsole();};
    outputPeakReset.setTooltip("Peak hold for the selected physical USB output. Click to reset its peak and clip latch.");
    for(int slot=0;slot<4;++slot){
        auto& button=outputInserts[static_cast<std::size_t>(slot)];
        auto& bypass=outputBypass[static_cast<std::size_t>(slot)];
        addAndMakeVisible(button);addAndMakeVisible(bypass);
        button.onClick=[this,slot]{if(onOpenBuiltin)onOpenBuiltin(out,slot,true);};
        button.onContextMenu=[this,slot]{showInsertMenu(out,slot,true,outputInserts[static_cast<std::size_t>(slot)]);};
        bypass.onClick=[this,slot]{
            auto& processor=engine.outputBuiltinControls().slots[static_cast<std::size_t>(out)][static_cast<std::size_t>(slot)];
            if(processor.type.load()==builtin::None)return;
            const bool enabled=!processor.enabled.load();
            if(!enabled)processor.feedbackArmed.store(false,std::memory_order_release);
            processor.enabled.store(enabled,std::memory_order_release);refreshConsole();
        };
    }
    addParameter(masterGain,"OUTPUT FADER / dB",-60,12,0.1,
        [this]{return control.outputGain[out].load();},[this](float value){control.outputGain[out].store(value);});
    masterGain.slider.setSliderStyle(juce::Slider::LinearVertical);
    masterGain.slider.setLookAndFeel(&mixerFaderSkin());
    masterGain.slider.setTextBoxStyle(juce::Slider::TextBoxAbove,false,80,22);
    masterGain.slider.setTextValueSuffix(" dB");
    masterGain.slider.setDoubleClickReturnValue(true,0);
    masterGain.label.setFont(juce::FontOptions(10.0f));
    addParameter(masterDelay,"Delay / ms",0,250,0.01,
        [this]{return control.outputDelayMs[out].load();},[this](float value){control.outputDelayMs[out].store(value);});
    masterDelay.label.setFont(juce::FontOptions(10.0f));
    masterDelay.slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,54,22);
    selectedChannels();
    setStripView(true);
}
MixerPanel::~MixerPanel(){stripsViewport.setViewedComponent(nullptr,false);masterGain.slider.setLookAndFeel(nullptr);setLookAndFeel(nullptr);}

void MixerPanel::setChannelNames(const juce::StringArray& names){
    for(int channel=0;channel<32;++channel){
        channelNames.set(channel,boundedChannelName(channel<names.size()?names[channel]:juce::String(),channel));
        inSelect.changeItemText(channel+1,"IN "+juce::String(channel+1)+" / "+channelNames[channel]);
    }
    selectedChannels();
}
void MixerPanel::rebuildStripBank(){
    stripBank.clear(juce::dontSendNotification);
    for(int start=0;start<32;start+=visibleStripCount)
        stripBank.addItem("INPUT "+juce::String(start+1)+" - "+juce::String(start+visibleStripCount),start/visibleStripCount+1);
    stripBank.setSelectedId(stripPage+1,juce::dontSendNotification);
}
void MixerPanel::setChannelViewCount(int count){
    const int first=stripPage*visibleStripCount;
    visibleStripCount=count>=16?16:8;stripPage=first/visibleStripCount;
    rebuildStripBank();setStripView(true);stripsViewport.setViewPosition(0,0);
}
void MixerPanel::resetPeakHolds(){
    inputPeakHolds.fill(0);outputPeakHolds.fill(0);inputClips.fill(false);outputClips.fill(false);
    for(auto& strip:channelStrips)if(strip)strip->setPeak(0,false,0,false);
    refreshConsole();repaint();
}
void MixerPanel::showInsertMenu(int channel,int slot,bool output,juce::Component& anchor){
    auto& controls=output?engine.outputBuiltinControls():engine.builtinControls();
    const int expectedType=controls.slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slot)].type.load();
    const bool muted=!engine.isPassThroughEnabled();
    juce::PopupMenu menu;
    menu.addSectionHeader((output?"USB OUT ":"CH ")+juce::String(channel+1)+" / INSERT "+juce::String(slot+1));
    menu.addItem(1,"Open editor");
    menu.addSeparator();
    menu.addItem(2,"Move up",muted&&slot>0);
    menu.addItem(3,"Move down",muted&&slot<3);
    menu.addItem(4,"Copy settings",expectedType!=builtin::None);
    menu.addItem(5,"Paste settings",muted&&rackOperations.hasClipboard());
    menu.addItem(6,"Remove plugin",muted&&expectedType!=builtin::None);
    if(!muted)menu.addSectionHeader("Mute audio to move, paste or remove.");
    const juce::Component::SafePointer<MixerPanel> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&anchor),[safe,channel,slot,output,expectedType](int action){
        if(!safe||action==0)return;
        auto& current=output?safe->engine.outputBuiltinControls():safe->engine.builtinControls();
        if(action!=1&&current.slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(slot)].type.load()!=expectedType){
            safe->noteLabel.setText("Insert changed while the menu was open. Open its menu again.",juce::dontSendNotification);return;
        }
        safe->performInsertAction(channel,slot,output,action);
    });
}
bool MixerPanel::performInsertAction(int channel,int slot,bool output,int action){
    if(channel<0||channel>=32||slot<0||slot>=4)return false;
    if(action==1){if(onOpenBuiltin)onOpenBuiltin(channel,slot,output);return true;}
    auto& controls=output?engine.outputBuiltinControls():engine.builtinControls();
    const bool running=engine.isPassThroughEnabled();bool success=false;
    switch(action){
        case 2:success=rackOperations.move(controls,channel,slot,-1,running);break;
        case 3:success=rackOperations.move(controls,channel,slot,1,running);break;
        case 4:success=rackOperations.copy(controls,channel,slot);break;
        case 5:success=rackOperations.paste(controls,channel,slot,running);break;
        case 6:success=rackOperations.remove(controls,channel,slot,running);break;
        default:return false;
    }
    if(!success){noteLabel.setText(running&&action!=4?"Mute audio before changing the insert order or content.":"Insert operation is unavailable at this position.",juce::dontSendNotification);return false;}
    noteLabel.setText(action==4?"Copied the complete plugin settings. Right-click another insert to paste.":"Insert chain updated. Check its order before enabling audio.",juce::dontSendNotification);
    refreshControls();return true;
}
bool MixerPanel::runWorkflowValidation(juce::String& error){
    if(engine.isDeviceRunning()||engine.isPassThroughEnabled()){
        error="Mixer workflow validation requires an offline muted engine.";return false;
    }
    const auto savedNames=channelNames;
    const auto savedInput=builtin::snapshot(engine.builtinControls()),savedOutput=builtin::snapshot(engine.outputBuiltinControls());
    const juce::Point<int> savedSize{getWidth(),getHeight()};const int savedView=visibleStripCount,savedPage=stripPage,savedIn=in,savedOut=out;
    const auto savedNote=noteLabel.getText();
    bool success=true;
    const auto fail=[&](const juce::String& reason){success=false;if(error.isEmpty())error=reason;};
    auto names=savedNames;names.set(0,"  Lead vocal  ");names.set(1," \t ");names.set(2,juce::String::repeatedString("x",80));
    names.set(7,juce::String::fromUTF8("Giọng hát 8"));setChannelNames(names);
    if(channelNames.size()!=32||channelNames[0]!="Lead vocal"||channelNames[1]!="CH2"||channelNames[2].length()!=32||channelNames[7]!=names[7])
        fail("Channel name validation, Unicode or 32-name preservation failed.");
    auto& source=engine.builtinControls().slots[7][2];builtin::configureSlot(source,builtin::Equalizer,true);
    builtin::setExtendedEqEnabled(source,true);builtin::recallEqBand(source.eqBands[6],{true,builtin::EqHighShelf,6500,4,.9f});
    if(!performInsertAction(7,2,false,4)||!performInsertAction(31,3,true,5))fail("Real insert copy/paste handlers failed.");
    const auto pasted=builtin::snapshotSlot(engine.outputBuiltinControls().slots[31][3]);
    const auto original=builtin::snapshotSlot(source);
    if(pasted.type!=original.type||pasted.enabled!=original.enabled||pasted.params!=original.params||pasted.extendedEq!=original.extendedEq||pasted.eqInitialized!=original.eqInitialized||pasted.eqBands!=original.eqBands)
        fail("Insert copy/paste lost complete eight-band EQ state.");
    if(!performInsertAction(31,3,true,2)||engine.outputBuiltinControls().slots[31][2].type.load()!=builtin::Equalizer)
        fail("Real move-up handler failed at an output slot.");
    if(!performInsertAction(31,2,true,6)||engine.outputBuiltinControls().slots[31][2].type.load()!=builtin::None)
        fail("Real remove handler failed.");
    std::array<float,32> inputs{},outputs{};inputs[7]=1.2f;outputs[31]=.9f;
    setMeterLevels(inputs.data(),32,outputs.data(),32);inputs[7]=.1f;outputs[31]=.1f;
    setMeterLevels(inputs.data(),32,outputs.data(),32);
    if(inputPeakHolds[7]!=1.2f||!inputClips[7]||outputPeakHolds[31]!=.9f||outputClips[31])
        fail("Real meter peak-hold/clip latch failed.");
    resetPeaks.onClick();
    if(inputPeakHolds[7]!=0||inputClips[7]||outputPeakHolds[31]!=0||outputClips[31])fail("Peak reset handler failed.");
    std::function<void(juce::Component&)> boundsCheck;
    boundsCheck=[&](juce::Component& parent){
        if(auto* viewport=dynamic_cast<juce::Viewport*>(&parent)){
            if(auto* content=viewport->getViewedComponent())boundsCheck(*content);
            return;
        }
        for(int n=0;n<parent.getNumChildComponents();++n){
            auto* child=parent.getChildComponent(n);if(!child->isVisible()||child->getBounds().isEmpty())continue;
            if(!parent.getLocalBounds().contains(parent.getLocalArea(child,child->getLocalBounds())))fail("Mixer 8/16-channel layout overflow: "+child->getName());
            boundsCheck(*child);
        }
    };
    for(const auto size:{juce::Point<int>{1100,650},juce::Point<int>{1400,850}}){
        setSize(size.x,size.y);
        for(int count:{8,16}){
            setChannelViewCount(count);stripBank.setSelectedId(32/count,juce::sendNotificationSync);
            if(channelStrips[0]->getChannel()!=32-count||channelStrips[static_cast<std::size_t>(count-1)]->getChannel()!=31)
                fail("8/16-channel bank does not address the correct physical input indices.");
            boundsCheck(*this);
        }
    }
    builtin::recall(engine.builtinControls(),savedInput);builtin::recall(engine.outputBuiltinControls(),savedOutput);
    setChannelNames(savedNames);in=savedIn;out=savedOut;visibleStripCount=savedView;stripPage=savedPage;
    rebuildStripBank();setSize(savedSize.x,savedSize.y);setStripView(true);resetPeakHolds();selectedChannels();
    noteLabel.setText(savedNote,juce::dontSendNotification);
    return success;
}

void MixerPanel::setUsbChannelCount(int count){
    const int next=std::clamp(count,0,8);
    if(usbChannelCount==next)return;
    usbChannelCount=next;refreshConsole();
}
void MixerPanel::refreshControls(){selectedChannels();}
void MixerPanel::refreshConsole(){
    for(auto& strip:channelStrips)if(strip){
        const int channel=strip->getChannel();
        strip->setUsbEnabled(channel<8?channel<usbChannelCount:
            channel<engine.getInputChannelCount()&&channel<engine.getOutputChannelCount());
    }
    const bool running=engine.isPassThroughEnabled();
    audioState.setText(running?juce::String::fromUTF8("AUDIO ĐANG BẬT"):engine.isDeviceRunning()?"AUDIO MUTED":"USB OFFLINE",juce::dontSendNotification);
    audioState.setColour(juce::Label::textColourId,running?juce::Colour(0xff5ee4b8):mixerMuted);
    audioPower.setButtonText(juce::String::fromUTF8(running?"TẮT AUDIO":"BẬT AUDIO"));
    audioPower.setColour(juce::TextButton::buttonColourId,running?juce::Colour(0xff18634f):juce::Colour(0xff163c57));
    audioPower.setEnabled(engine.isDeviceRunning());
    const auto held=outputPeakHolds[static_cast<std::size_t>(out)];
    outputPeakReset.setButtonText((outputClips[static_cast<std::size_t>(out)]?"CLIP ":"PEAK ")+(held>0.00001f?juce::String(20*std::log10(held),1):"-inf"));
    outputPeakReset.setColour(juce::TextButton::buttonColourId,outputClips[static_cast<std::size_t>(out)]?juce::Colour(0xff8a303c):juce::Colour(0xff18262c));
    chainTitle.setText(channelNames[in]+" / CH "+juce::String(in+1)+" CHAIN",juce::dontSendNotification);
    for(int slot=0;slot<4;++slot){
        const auto& processor=engine.builtinControls().slots[static_cast<std::size_t>(in)][static_cast<std::size_t>(slot)];
        auto& button=chainOverview[static_cast<std::size_t>(slot)];
        const int type=processor.type.load();const bool active=type!=builtin::None&&processor.enabled.load();
        button.setButtonText(juce::String(slot+1)+"  "+(type==builtin::None?"+ Add plugin":juce::String::fromUTF8(builtin::metadata(type).name))+(active?"  ON":type==builtin::None?"":"  BYPASS"));
        button.setColour(juce::TextButton::buttonColourId,active?juce::Colour(0xff19443e):juce::Colour(0xff17252c));
        button.setColour(juce::TextButton::textColourOffId,type==builtin::None?mixerMuted:mixerText);
        button.setTooltip("Selected channel signal flows left to right. Click to edit; right-click for move/copy/paste/remove.");
    }
    eightView.setColour(juce::TextButton::buttonColourId,visibleStripCount==8?juce::Colour(0xff235160):juce::Colour(0xff17252c));
    sixteenView.setColour(juce::TextButton::buttonColourId,visibleStripCount==16?juce::Colour(0xff235160):juce::Colour(0xff17252c));
    insertNote.setText(juce::String::fromUTF8("PLUGIN CHÈN / OUT ")+juce::String(out+1),juce::dontSendNotification);
    for(int slot=0;slot<4;++slot){
        const auto& processor=engine.outputBuiltinControls().slots[static_cast<std::size_t>(out)][static_cast<std::size_t>(slot)];
        const int type=processor.type.load(std::memory_order_acquire);
        const bool enabled=type!=builtin::None&&processor.enabled.load(std::memory_order_acquire);
        const auto name=juce::String::fromUTF8(builtin::metadata(type).name);
        auto& button=outputInserts[static_cast<std::size_t>(slot)];
        auto& bypass=outputBypass[static_cast<std::size_t>(slot)];
        button.setButtonText(type==builtin::None?juce::String(slot+1)+" + Add plugin":name);
        button.setTooltip("OUT "+juce::String(out+1)+" / insert "+juce::String(slot+1)+": "+name+". Click to edit; right-click to move/copy/paste/remove. Processing precedes physical output gain/delay.");
        button.setColour(juce::TextButton::buttonColourId,enabled?juce::Colour(0xff163735):juce::Colour(0xff10171b));
        button.setColour(juce::TextButton::textColourOffId,type==builtin::None?mixerMuted:mixerText);
        bypass.setButtonText(enabled?"ON":"OFF");
        bypass.setColour(juce::TextButton::buttonColourId,enabled?juce::Colour(0xff236149):juce::Colour(0xff1b252a));
        bypass.setColour(juce::TextButton::textColourOffId,enabled?juce::Colour(0xff7dedc2):mixerMuted);
        bypass.setEnabled(type!=builtin::None);
        bypass.setTooltip(enabled?"Bypass this output processor":"Enable this output processor");
    }
}

void MixerPanel::setStripView(bool showStrips) {
    stripView=showStrips;
    selectedChannels();
    for(auto* label:{&inputLabel,&outputLabel,&routeLabel,&matLabel,&inGroupLabel,&outGroupLabel,&selectedRoute,&matrixHint})
        label->setVisible(!stripView);
    for(auto& label:matrixInputs)label.setVisible(!stripView);
    for(auto& label:matrixOutputs)label.setVisible(!stripView);
    for(auto* combo:{&inSelect,&inGroup,&outGroup})combo->setVisible(!stripView);
    for(auto& parameter:inputParams){parameter.label.setVisible(!stripView);parameter.slider.setVisible(!stripView);}
    for(auto& parameter:outputParams){parameter.label.setVisible(!stripView);parameter.slider.setVisible(!stripView);}
    routeGain.label.setVisible(!stripView);routeGain.slider.setVisible(!stripView);
    for(auto& button:matrixButtons)button.setVisible(!stripView);
    pan.label.setVisible(false);pan.slider.setVisible(false);
    muteInput.setVisible(!stripView);
    stripsViewport.setVisible(stripView);
    for(int n=0;n<16;++n)channelStrips[static_cast<std::size_t>(n)]->setVisible(stripView&&n<visibleStripCount);
    for(auto* component:std::array<juce::Component*,16>{&stripBank,&bankLabel,&masterTitle,&insertNote,
        &masterGain.label,&masterGain.slider,&masterDelay.label,&masterDelay.slider,&audioState,&audioPower,&panic,
        &eightView,&sixteenView,&resetPeaks,&chainTitle,&outputPeakReset})component->setVisible(stripView);
    for(auto& button:chainOverview)button.setVisible(stripView);
    for(auto& button:outputInserts)button.setVisible(stripView);
    for(auto& button:outputBypass)button.setVisible(stripView);
    stripViewButton.setColour(juce::TextButton::buttonColourId,stripView?juce::Colour(0xff27665e):juce::Colour(0xff27313c));
    detailViewButton.setColour(juce::TextButton::buttonColourId,!stripView?juce::Colour(0xff27665e):juce::Colour(0xff27313c));
    resized();repaint();
}

void MixerPanel::setMeterLevels(const float* inputs,int inputCount,const float* outputs,int outputCount) {
    meterOutputCount=std::clamp(outputCount,0,dsp::channels);
    for(int channel=0;channel<32;++channel){
        const float input=inputs&&channel<inputCount&&std::isfinite(inputs[channel])?std::max(0.0f,inputs[channel]):0;
        const float output=outputs&&channel<outputCount&&std::isfinite(outputs[channel])?std::max(0.0f,outputs[channel]):0;
        inputPeakHolds[static_cast<std::size_t>(channel)]=std::max(inputPeakHolds[static_cast<std::size_t>(channel)],input);
        outputPeakHolds[static_cast<std::size_t>(channel)]=std::max(outputPeakHolds[static_cast<std::size_t>(channel)],output);
        inputClips[static_cast<std::size_t>(channel)]=inputClips[static_cast<std::size_t>(channel)]||input>=1;
        outputClips[static_cast<std::size_t>(channel)]=outputClips[static_cast<std::size_t>(channel)]||output>=1;
    }
    for(auto& strip:channelStrips) {
        const int channel=strip->getChannel();
        const bool available=inputs!=nullptr&&channel<inputCount;
        const float peak=available&&std::isfinite(inputs[channel])?std::max(0.0f,inputs[channel]):0.0f;
        strip->setPeak(peak,available,inputPeakHolds[static_cast<std::size_t>(channel)],inputClips[static_cast<std::size_t>(channel)]);
    }
    for(int n=0;n<dsp::channels;++n)
        outputPeaks[static_cast<std::size_t>(n)]=outputs!=nullptr&&n<outputCount&&std::isfinite(outputs[n])?std::max(0.0f,outputs[n]):0.0f;
    selectedOutputPeak=outputs!=nullptr&&out<outputCount?outputPeaks[static_cast<std::size_t>(out)]:-1.0f;
    refreshConsole();
    if(stripView)repaint(masterMeter);
}
void MixerPanel::addParameter(Param& p,const juce::String& text,double lo,double hi,double step,
                            std::function<float()> getter,std::function<void(float)> setter) {
    p.label.setText(text,juce::dontSendNotification);
    p.label.setColour(juce::Label::textColourId,mixerText);
    p.label.setFont(juce::FontOptions(13.5f));
    p.label.setBorderSize(juce::BorderSize<int>(0));
    p.label.setMinimumHorizontalScale(1.0f);
    addAndMakeVisible(p.label);addAndMakeVisible(p.slider);
    p.slider.setSliderStyle(juce::Slider::LinearHorizontal);
    p.slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,74,28);
    p.slider.setColour(juce::Slider::thumbColourId,mixerAccent);
    p.slider.setColour(juce::Slider::trackColourId,mixerAccent);
    p.slider.setColour(juce::Slider::textBoxTextColourId,mixerText);
    p.slider.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xff11161d));
    p.slider.setColour(juce::Slider::textBoxOutlineColourId,mixerBorder);
    p.slider.setRange(lo,hi,step);
    p.slider.setValue(getter(),juce::dontSendNotification);
    p.slider.onValueChange=[&p,setter]{setter(static_cast<float>(p.slider.getValue()));};
}
void MixerPanel::selectedChannels() {
    const float inputVals[]={control.inputGain[in].load(),control.hpfHz[in].load(),control.eqHz[in].load(),
        control.eqDb[in].load(),control.eqQ[in].load(),control.compThreshold[in].load(),
        control.compRatio[in].load(),control.gateThreshold[in].load()};
    const float outputVals[]={control.outputGain[out].load(),control.outputDelayMs[out].load(),
        control.outputCeiling[out].load(),control.outputPolarity[out].load()<0?1.0f:0.0f};
    for(int j=0;j<8;++j)inputParams[j].slider.setValue(inputVals[j],juce::dontSendNotification);
    for(int j=0;j<4;++j)outputParams[j].slider.setValue(outputVals[j],juce::dontSendNotification);
    routeGain.slider.setValue(control.route[out*dsp::channels+in].load(),juce::dontSendNotification);
    polarity.setButtonText(control.outputPolarity[out].load()<0?"Polarity: -":"Polarity: +");
    pan.slider.setValue(control.inputPan[in].load(),juce::dontSendNotification);
    muteInput.setButtonText(control.inputMute[in].load()>0.5f?"INPUT MUTED":"INPUT MUTE OFF");
    muteInput.setColour(juce::TextButton::buttonColourId,control.inputMute[in].load()>0.5f
        ?juce::Colour(0xff994754):juce::Colour(0xff2a3541));
    masterGain.slider.setValue(control.outputGain[out].load(),juce::dontSendNotification);
    masterDelay.slider.setValue(control.outputDelayMs[out].load(),juce::dontSendNotification);
    selectedOutputPeak=out<meterOutputCount?outputPeaks[static_cast<std::size_t>(out)]:-1.0f;
    for(int n=0;n<16;++n)if(channelStrips[static_cast<std::size_t>(n)]){
        auto& strip=channelStrips[static_cast<std::size_t>(n)];
        strip->setVisible(stripView&&n<visibleStripCount);
        if(n<visibleStripCount)strip->setChannel(stripPage*visibleStripCount+n,in==stripPage*visibleStripCount+n);
    }
    refreshConsole();
    reloadMatrix();
}
void MixerPanel::reloadMatrix(){
    for(int n=0;n<8;++n) {
        auto& input=matrixInputs[static_cast<std::size_t>(n)];
        auto& output=matrixOutputs[static_cast<std::size_t>(n)];
        input.setText("IN "+juce::String(inPage*8+n+1),juce::dontSendNotification);
        output.setText("OUT "+juce::String(outPage*8+n+1),juce::dontSendNotification);
        input.setColour(juce::Label::textColourId,in==inPage*8+n?mixerAccent:mixerMuted);
        output.setColour(juce::Label::textColourId,out==outPage*8+n?mixerAccent:mixerMuted);
    }
    selectedRoute.setText("IN "+juce::String(in+1)+juce::String::fromUTF8("  →  OUT ")+juce::String(out+1)
        +"   /   "+juce::String(control.route[out*dsp::channels+in].load(),2),juce::dontSendNotification);
    for(int row=0;row<8;++row)for(int col=0;col<8;++col) {
        const int i=inPage*8+col,o=outPage*8+row;
        auto& b=matrixButtons[static_cast<std::size_t>(row*8+col)];
        const auto v=control.route[o*dsp::channels+i].load();
        const bool on=std::abs(v)>0.00001f;
        const bool selected=i==in&&o==out;
        b.setButtonText(on?juce::String(v,1):juce::String::fromUTF8("·"));
        b.setColour(juce::TextButton::buttonColourId,selected?juce::Colour(0xff397e76)
            :(on?juce::Colour(0xff24534e):juce::Colour(0xff232c36)));
        b.setColour(juce::TextButton::textColourOffId,on||selected?mixerText:mixerMuted);
        b.setTooltip("OUT "+juce::String(o+1)+juce::String::fromUTF8(" ← IN ")+juce::String(i+1)+" / gain="+juce::String(v,2));
    }
}
void MixerPanel::paint(juce::Graphics& g){
    g.fillAll(mixerBackground);
    const std::array<juce::Rectangle<int>,3> cards=stripView
        ?std::array<juce::Rectangle<int>,3>{masterCard,juce::Rectangle<int>{},juce::Rectangle<int>{}}
        :std::array<juce::Rectangle<int>,3>{inputCard,outputCard,matrixCard};
    for(const auto& card:cards) {
        if(card.isEmpty())continue;
        g.setColour(mixerCard);
        g.fillRoundedRectangle(card.toFloat(),3.0f);
        g.setColour(mixerBorder);
        g.drawRoundedRectangle(card.toFloat().reduced(0.5f),3.0f,1.0f);
    }
    const auto warning=noteLabel.getBounds().expanded(12,4).toFloat();
    g.setColour(juce::Colour(0xff11191d));
    g.fillRoundedRectangle(warning,3.0f);
    if(stripView) {
        paintPeak(g,masterMeter,std::max(0.0f,selectedOutputPeak),selectedOutputPeak>=0.0f);
        g.setColour(mixerMuted);g.setFont(juce::FontOptions(9.0f));
        const auto ticks=masterGain.slider.getBounds().withTrimmedTop(32).withTrimmedBottom(12);
        for(int db:{12,0,-12,-24,-36,-48,-60}) {
            const int y=juce::roundToInt(ticks.getY()+(12.0f-db)/72.0f*ticks.getHeight());
            g.drawText(juce::String(db),ticks.getX()-24,y-8,24,16,juce::Justification::centredRight);
        }
        const auto peakText=selectedOutputPeak<0.0f?juce::String::fromUTF8("—")
            :(selectedOutputPeak<=0.00001f?"-inf":juce::String(20.0f*std::log10(selectedOutputPeak),1));
        g.drawText(peakText,masterMeter.getCentreX()-21,masterMeter.getBottom()+1,42,15,juce::Justification::centred);
    }
}
void MixerPanel::resized(){
    if(getWidth()<1||getHeight()<1)return;
    auto area=getLocalBounds().reduced(8);
    auto navigation=area.removeFromTop(28);
    stripViewButton.setBounds(navigation.removeFromLeft(78));navigation.removeFromLeft(6);
    detailViewButton.setBounds(navigation.removeFromLeft(122));navigation.removeFromLeft(10);
    eightView.setBounds(navigation.removeFromLeft(54));navigation.removeFromLeft(4);
    sixteenView.setBounds(navigation.removeFromLeft(54));navigation.removeFromLeft(8);
    resetPeaks.setBounds(navigation.removeFromLeft(90));
    stripBank.setBounds(navigation.removeFromRight(174));navigation.removeFromRight(10);
    bankLabel.setBounds(navigation.removeFromRight(110));
    area.removeFromTop(8);
    auto warning=area.removeFromBottom(20);
    noteLabel.setBounds(warning.reduced(6,0));
    area.removeFromBottom(8);
    if(stripView) {
        auto overview=area.removeFromTop(30);
        chainTitle.setBounds(overview.removeFromLeft(std::clamp(getWidth()/7,140,220)));overview.removeFromLeft(8);
        const int overviewWidth=(overview.getWidth()-18)/4;
        for(auto& button:chainOverview){button.setBounds(overview.removeFromLeft(overviewWidth));overview.removeFromLeft(6);}
        area.removeFromTop(8);
        masterCard=area.removeFromRight(std::clamp(getWidth()/8,155,188));area.removeFromRight(10);
        stripsViewport.setBounds(area);
        const int availableWidth=std::max(1,stripsViewport.getWidth()-12);
        const int contentWidth=std::max(availableWidth,visibleStripCount*108+(visibleStripCount-1)*5);
        const int contentHeight=std::max(500,stripsViewport.getHeight()-12);
        stripsContent.setSize(contentWidth,contentHeight);
        const int channelWidth=(contentWidth-(visibleStripCount-1)*5)/visibleStripCount;
        for(int n=0;n<16;++n){
            auto& strip=channelStrips[static_cast<std::size_t>(n)];strip->setVisible(n<visibleStripCount);
            if(n<visibleStripCount)strip->setBounds(n*(channelWidth+5),0,channelWidth,contentHeight);
        }
        auto master=masterCard.reduced(10,8);
        masterTitle.setBounds(master.removeFromTop(24));
        outSelect.setBounds(master.removeFromTop(27));
        master.removeFromTop(7);
        audioState.setBounds(master.removeFromTop(20));
        outputPeakReset.setBounds(master.removeFromTop(21));
        master.removeFromTop(3);
        audioPower.setBounds(master.removeFromTop(30));
        master.removeFromTop(4);
        panic.setBounds(master.removeFromTop(24));
        master.removeFromTop(7);
        insertNote.setBounds(master.removeFromTop(18));
        for(int slot=0;slot<4;++slot){
            auto row=master.removeFromTop(24);master.removeFromTop(3);
            outputBypass[static_cast<std::size_t>(slot)].setBounds(row.removeFromRight(29));row.removeFromRight(3);
            outputInserts[static_cast<std::size_t>(slot)].setBounds(row);
        }
        master.removeFromTop(6);
        masterDelay.slider.setBounds(master.removeFromBottom(24));
        masterDelay.label.setBounds(master.removeFromBottom(15));
        master.removeFromBottom(4);
        polarity.setBounds(master.removeFromBottom(24));
        master.removeFromBottom(4);
        masterGain.label.setBounds(master.removeFromTop(17));
        masterMeter=master.removeFromRight(12).withTrimmedTop(28).withTrimmedBottom(15);
        master.removeFromRight(8);
        masterGain.slider.setBounds(master.withTrimmedLeft(23));
        return;
    }
    const int usable=area.getWidth()-32;
    const int inputWidth=juce::roundToInt(usable*0.34f);
    const int outputWidth=juce::roundToInt(usable*0.28f);
    inputCard=area.removeFromLeft(inputWidth);area.removeFromLeft(16);
    outputCard=area.removeFromLeft(outputWidth);area.removeFromLeft(16);
    matrixCard=area;
    auto inputArea=inputCard.reduced(18);
    inputLabel.setBounds(inputArea.removeFromTop(24));
    inputArea.removeFromTop(4);
    inSelect.setBounds(inputArea.removeFromTop(32));
    inputArea.removeFromTop(4);
    auto muteArea=inputArea.removeFromBottom(32);
    muteInput.setBounds(muteArea);
    inputArea.removeFromBottom(4);
    const int inputRowHeight=inputArea.getHeight()/8;
    auto placeParameter=[](Param& parameter,juce::Rectangle<int> row) {
        parameter.label.setBounds(row.removeFromTop(20));
        parameter.slider.setBounds(row.removeFromTop(std::min(34,row.getHeight())));
    };
    for(auto& parameter:inputParams)placeParameter(parameter,inputArea.removeFromTop(inputRowHeight));

    auto outputArea=outputCard.reduced(18);
    outputLabel.setBounds(outputArea.removeFromTop(28));
    outputArea.removeFromTop(6);
    outSelect.setBounds(outputArea.removeFromTop(34));
    outputArea.removeFromTop(12);
    const int outputRowHeight=std::clamp((outputArea.getHeight()-160)/4,58,90);
    for(auto& parameter:outputParams)placeParameter(parameter,outputArea.removeFromTop(outputRowHeight));
    polarity.setBounds(outputArea.removeFromTop(34));
    outputArea.removeFromTop(18);
    routeLabel.setBounds(outputArea.removeFromTop(25));
    outputArea.removeFromTop(5);
    placeParameter(routeGain,outputArea.removeFromTop(58));

    auto matrixArea=matrixCard.reduced(16,18);
    matLabel.setBounds(matrixArea.removeFromTop(28));
    matrixArea.removeFromTop(10);
    auto groupLabels=matrixArea.removeFromTop(21);
    const int groupWidth=(matrixArea.getWidth()-10)/2;
    inGroupLabel.setBounds(groupLabels.removeFromLeft(groupWidth));
    groupLabels.removeFromLeft(10);outGroupLabel.setBounds(groupLabels);
    auto groupBoxes=matrixArea.removeFromTop(34);
    inGroup.setBounds(groupBoxes.removeFromLeft(groupWidth));
    groupBoxes.removeFromLeft(10);outGroup.setBounds(groupBoxes);
    matrixArea.removeFromTop(12);
    const int rowLabelWidth=53;
    const int cellWidth=(matrixArea.getWidth()-rowLabelWidth)/8;
    auto columnHeaders=matrixArea.removeFromTop(24);
    columnHeaders.removeFromLeft(rowLabelWidth);
    for(auto& label:matrixInputs)label.setBounds(columnHeaders.removeFromLeft(cellWidth));
    const int cellHeight=std::clamp((matrixArea.getHeight()-82)/8,34,70);
    for(int row=0;row<8;++row) {
        auto line=matrixArea.removeFromTop(cellHeight);
        matrixOutputs[static_cast<std::size_t>(row)].setBounds(line.removeFromLeft(rowLabelWidth).reduced(0,2));
        for(int col=0;col<8;++col)
            matrixButtons[static_cast<std::size_t>(row*8+col)].setBounds(line.removeFromLeft(cellWidth).reduced(2));
    }
    matrixArea.removeFromTop(10);
    selectedRoute.setBounds(matrixArea.removeFromTop(26));
    matrixHint.setBounds(matrixArea.removeFromTop(42));
}
} // namespace lah::ui

#include "BuiltinRackPanel.h"
#include "../dsp/BuiltinRackCore.h"
#include <algorithm>
#include <cmath>

namespace lah::ui {
namespace {
const juce::Colour rackBackground {0xff0d1015},rackCard {0xff171d24},rackBorder {0xff303b47};
const juce::Colour rackText {0xffe0e9ef},rackMuted {0xffa8b7c6},rackAccent {0xff37d8b7},rackAmber {0xffe6b96f};
constexpr int moduleCount=builtin::moduleCount+1;
float visibleParameterValue(const builtin::ParameterInfo& parameter,float value) noexcept {
    return std::isfinite(value)?std::clamp(value,parameter.minimum,parameter.maximum):parameter.defaultValue;
}
bool advancedMode(const builtin::SlotControl& slot,int module)noexcept {
    return module==builtin::Equalizer?slot.extendedEq.load(std::memory_order_acquire):
        module==builtin::DigitalFx?slot.digitalFxPro.enabled.load(std::memory_order_acquire):
        module==builtin::Dynamics && slot.dynamicsPro.enabled.load(std::memory_order_acquire);
}
}

BuiltinRackPanel::BuiltinRackPanel(audio::LiveAudioEngine& e,juce::AudioDeviceManager& manager)
    : engine(e),devices(manager) {
    setOpaque(true);
    for(auto* label:{&title,&subtitle,&status,&moduleLabel,&description,&libraryNote,
                    &feedbackState,&feedbackCandidate,&feedbackScore,&feedbackLimit,&feedbackNotches,&feedbackNote}) {
        addAndMakeVisible(*label);
        label->setFont(juce::FontOptions(13.0f));
        label->setMinimumHorizontalScale(1.0f);
        label->setBorderSize(juce::BorderSize<int>(0));
        label->setColour(juce::Label::textColourId,rackMuted);
    }
    title.setText("Native processor rack",juce::dontSendNotification);
    title.setFont(juce::FontOptions(22.0f,juce::Font::bold));
    title.setColour(juce::Label::textColourId,rackText);
    subtitle.setText("USB inputs and mixer returns / 4 ordered inserts per channel / 17 native processors",juce::dontSendNotification);
    status.setFont(juce::FontOptions(14.0f));
    moduleLabel.setText("Processor for selected slot",juce::dontSendNotification);
    description.setJustificationType(juce::Justification::topLeft);
    libraryNote.setJustificationType(juce::Justification::topLeft);
    feedbackState.setFont(juce::FontOptions(14.0f,juce::Font::bold));
    feedbackCandidate.setFont(juce::FontOptions(21.0f,juce::Font::bold));
    feedbackCandidate.setColour(juce::Label::textColourId,rackText);
    feedbackNotches.setJustificationType(juce::Justification::topLeft);
    feedbackNote.setJustificationType(juce::Justification::topLeft);
    feedbackLimit.setTooltip("Up to six automatic cuts, with a combined cut-depth budget of 36 dB.");
    addAndMakeVisible(domainSelect);addAndMakeVisible(inputSelect);addAndMakeVisible(moduleSelect);
    domainSelect.addItem("INPUT INSERTS",1);domainSelect.addItem("OUTPUT INSERTS",2);
    domainSelect.setSelectedId(1,juce::dontSendNotification);
    domainSelect.onChange=[this]{if(!refreshing)selectChannel(channel,selectedSlot,domainSelect.getSelectedId()==2);};
    for(int i=0;i<32;++i)inputSelect.addItem("INPUT "+juce::String(i+1),i+1);
    for(int i=0;i<moduleCount;++i)moduleSelect.addItem(juce::String::fromUTF8(builtin::metadata(i).name),i+1);
    inputSelect.setSelectedId(1,juce::dontSendNotification);
    inputSelect.onChange=[this]{if(!refreshing)selectChannel(inputSelect.getSelectedId()-1,selectedSlot,outputDomain);};
    moduleSelect.onChange=[this]{if(!refreshing)changeModule();};
    inputSelect.setTooltip("Choose which channel's four native insert slots to edit.");
    domainSelect.setTooltip("Input inserts process USB audio before routing. Output inserts process a return after routing, before its output fader.");
    moduleSelect.setTooltip("Audio must be muted before replacing a processor. Loading a processor leaves the slot bypassed.");
    for(int i=0;i<4;++i){
        auto& button=slotButtons[static_cast<std::size_t>(i)];
        addAndMakeVisible(button);
        button.setButtonText("Edit slot "+juce::String(i+1));
        button.onClick=[this,i]{selectSlot(i);};
        button.setTooltip("Select this ordered processor slot. Signal flows from slot 1 through slot 4.");
    }
    addAndMakeVisible(enabledButton);addAndMakeVisible(resetButton);
    addAndMakeVisible(feedbackArmButton);addAndMakeVisible(feedbackClearButton);
    enabledButton.onClick=[this]{setSlotEnabled();};
    resetButton.onClick=[this]{resetParameters();};
    feedbackArmButton.onClick=[this]{setFeedbackArmed();};
    feedbackClearButton.onClick=[this]{clearFeedbackNotches();};
    enabledButton.setTooltip("Enable or bypass this selected native processor slot.");
    resetButton.setTooltip("Restore defaults while audio is muted. Eight-band EQ, Echo/Reverb PRO and Dynamics PRO return to legacy mode.");
    feedbackArmButton.setTooltip("ARM permits new automatic cuts for this slot. DISARM stops new cuts and retains existing filters. Clear notches removes the retained cuts. Mute/PANIC clears and disarms.");
    feedbackClearButton.setTooltip("Clear this slot's detected frequencies and automatic cuts on the audio thread. Manual notch parameters remain unchanged.");
    for(int i=0;i<6;++i){
        auto& label=parameterLabels[static_cast<std::size_t>(i)];
        auto& slider=parameters[static_cast<std::size_t>(i)];
        addAndMakeVisible(label);addAndMakeVisible(slider);
        label.setFont(juce::FontOptions(14.0f));
        label.setMinimumHorizontalScale(1.0f);label.setBorderSize(juce::BorderSize<int>(0));
        label.setColour(juce::Label::textColourId,rackText);
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,90,28);
        slider.setColour(juce::Slider::thumbColourId,rackAccent);
        slider.setColour(juce::Slider::trackColourId,rackAccent);
        slider.setColour(juce::Slider::rotarySliderFillColourId,rackAccent);
        slider.setColour(juce::Slider::rotarySliderOutlineColourId,juce::Colour(0xff34414d));
        slider.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xff10161d));
        slider.setColour(juce::Slider::textBoxOutlineColourId,rackBorder);
        slider.onValueChange=[this,i]{
            if(refreshing)return;
            if(currentControls().slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(selectedSlot)].type.load(std::memory_order_acquire)!=displayedModule){refreshControls();return;}
            auto& slot=currentControls().slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(selectedSlot)];
            if(advancedMode(slot,displayedModule) || displayedAdvanced){refreshControls();return;}
            slot.params[static_cast<std::size_t>(i)].store(static_cast<float>(parameters[static_cast<std::size_t>(i)].getValue()),std::memory_order_release);
        };
    }
    juce::String library;
    for(int i=1;i<moduleCount;++i){
        if(i>1)library+="\n";
        library+=juce::String(i).paddedLeft('0',2)+"  "+juce::String::fromUTF8(builtin::metadata(i).name);
    }
    libraryNote.setText(library,juce::dontSendNotification);
    refreshControls();
    startTimerHz(5);
}

void BuiltinRackPanel::selectChannel(int value,int slot,bool output){
    channel=juce::jlimit(0,31,value);
    selectedSlot=juce::jlimit(0,3,slot);
    outputDomain=output;
    refreshControls();
}

void BuiltinRackPanel::setValidationScenario(int scenario){
    if(engine.isPassThroughEnabled())return;
    channel=0;selectedSlot=0;outputDomain=scenario>=builtin::moduleCount;
    auto& slot=currentControls().slots[0][0];
    builtin::configureSlot(slot,(std::max(0,scenario)%builtin::moduleCount)+1,true);
    refreshControls();
}

void BuiltinRackPanel::selectSlot(int value){
    selectedSlot=juce::jlimit(0,3,value);
    refreshControls();
}

void BuiltinRackPanel::changeModule(){
    if(engine.isPassThroughEnabled()){
        refreshControls();
        status.setText("Mute audio in the main transport before replacing a processor.",juce::dontSendNotification);
        return;
    }
    auto& slot=currentControls().slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(selectedSlot)];
    builtin::configureSlot(slot,juce::jlimit(0,moduleCount-1,moduleSelect.getSelectedId()-1),false);
    refreshControls();
}

void BuiltinRackPanel::setSlotEnabled(){
    if(currentControls().slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(selectedSlot)].type.load(std::memory_order_acquire)!=displayedModule){refreshControls();return;}
    auto& slot=currentControls().slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(selectedSlot)];
    if(advancedMode(slot,displayedModule)!=displayedAdvanced){refreshControls();return;}
    if(slot.type.load(std::memory_order_acquire)==0)return;
    const bool enable=!slot.enabled.load(std::memory_order_acquire);
    if(!enable)slot.feedbackArmed.store(false,std::memory_order_release);
    slot.enabled.store(enable,std::memory_order_release);
    refreshControls();
}

void BuiltinRackPanel::resetParameters(){
    if(engine.isPassThroughEnabled())return;
    if(currentControls().slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(selectedSlot)].type.load(std::memory_order_acquire)!=displayedModule){refreshControls();return;}
    auto& slot=currentControls().slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(selectedSlot)];
    if(advancedMode(slot,displayedModule)!=displayedAdvanced){refreshControls();return;}
    const auto& module=builtin::metadata(slot.type.load(std::memory_order_acquire));
    for(int i=0;i<module.parameterCount;++i)
        slot.params[static_cast<std::size_t>(i)].store(module.params[static_cast<std::size_t>(i)].defaultValue,std::memory_order_release);
    builtin::resetExtendedEq(slot);digitalfx::reset(slot.digitalFxPro);dynamicspro::reset(slot.dynamicsPro);
    refreshControls();
}

bool BuiltinRackPanel::isFeedbackModule() const noexcept {
    return displayedModule==builtin::FeedbackGuard || displayedModule==builtin::AutoFeedback;
}

void BuiltinRackPanel::setFeedbackArmed(){
    if(currentControls().slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(selectedSlot)].type.load(std::memory_order_acquire)!=displayedModule){refreshControls();return;}
    if(!isFeedbackModule())return;
    auto& slot=currentControls().slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(selectedSlot)];
    if(slot.feedbackArmed.load(std::memory_order_acquire))
        slot.feedbackArmed.store(false,std::memory_order_release);
    else if(slot.enabled.load(std::memory_order_acquire) && engine.isPassThroughEnabled() && engine.isDeviceRunning())
        slot.feedbackArmed.store(true,std::memory_order_release);
    updateParameters();repaint();
}

void BuiltinRackPanel::clearFeedbackNotches(){
    if(currentControls().slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(selectedSlot)].type.load(std::memory_order_acquire)!=displayedModule){refreshControls();return;}
    if(!isFeedbackModule())return;
    auto& slot=currentControls().slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(selectedSlot)];
    slot.feedbackResetGeneration.fetch_add(1,std::memory_order_acq_rel);
    updateParameters();repaint();
}

void BuiltinRackPanel::refreshControls(){
    refreshing=true;
    domainSelect.setSelectedId(outputDomain?2:1,juce::dontSendNotification);
    inputSelect.clear(juce::dontSendNotification);
    for(int i=0;i<32;++i)inputSelect.addItem((outputDomain?"OUTPUT ":"INPUT ")+juce::String(i+1),i+1);
    inputSelect.setSelectedId(channel+1,juce::dontSendNotification);
    auto& slot=currentControls().slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(selectedSlot)];
    const int type=slot.type.load(std::memory_order_acquire);
    displayedModule=type>=0 && type<moduleCount?type:0;
    displayedAdvanced=advancedMode(slot,displayedModule);
    const auto& module=builtin::metadata(displayedModule);
    moduleSelect.setSelectedId(displayedModule+1,juce::dontSendNotification);
    auto moduleDescription=juce::String::fromUTF8(module.description);
    if(displayedModule==builtin::FeedbackGuard)
        moduleDescription="Manual notch / HPF plus synthetic-trained AI detection. Automatic narrow cuts require ARM.";
    else if(displayedModule==builtin::AutoFeedback)
        moduleDescription="Manual notch plus spectral peak, growth and persistence detection. Automatic cuts require ARM.";
    else if(displayedAdvanced && displayedModule==builtin::Equalizer)
        moduleDescription="8 BAND EQ is active. Open this slot from Mixer to edit its bands in the floating editor. The six legacy controls below are inactive.";
    else if(displayedAdvanced && displayedModule==builtin::DigitalFx)
        moduleDescription="ECHO / REVERB PRO is active. Open this slot from Mixer to edit all PRO controls in its floating editor. The six legacy controls below are inactive.";
    else if(displayedAdvanced && displayedModule==builtin::Dynamics)
        moduleDescription="DYNAMICS PRO is active. Open this slot from Mixer for Gate, Compressor and final Limiter controls. The six legacy controls below are inactive.";
    description.setText(moduleDescription,juce::dontSendNotification);
    for(int i=0;i<6;++i){
        auto& label=parameterLabels[static_cast<std::size_t>(i)];
        auto& slider=parameters[static_cast<std::size_t>(i)];
        const bool visible=i<module.parameterCount;
        label.setVisible(visible);slider.setVisible(visible);
        if(!visible)continue;
        const auto& parameter=module.params[static_cast<std::size_t>(i)];
        label.setText(juce::String::fromUTF8(parameter.name),juce::dontSendNotification);
        slider.setRange(parameter.minimum,parameter.maximum,parameter.step);
        const auto unit=juce::String::fromUTF8(parameter.unit);
        slider.setSkewFactor(1.0);
        if(unit=="Hz" && parameter.minimum>0.0f && parameter.maximum>parameter.minimum*30.0f)
            slider.setSkewFactorFromMidPoint(std::sqrt(static_cast<double>(parameter.minimum)*parameter.maximum));
        slider.setTextValueSuffix(unit.isEmpty()?juce::String():" "+unit);
        slider.setDoubleClickReturnValue(true,parameter.defaultValue);
        slider.setValue(visibleParameterValue(parameter,slot.params[static_cast<std::size_t>(i)].load(std::memory_order_acquire)),juce::dontSendNotification);
        slider.setTooltip(displayedModule==builtin::AutoFeedback && i==4
            ?"Legacy zero-crossing tone advisory only; does not change audio or automatic cuts. Double-click for default."
            :juce::String::fromUTF8(parameter.name)+" / double-click for default");
    }
    refreshing=false;
    updateParameters();
    resized();repaint();
}

void BuiltinRackPanel::updateParameters(){
    const auto& slot=currentControls().slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(selectedSlot)];
    const bool active=slot.enabled.load(std::memory_order_acquire) && displayedModule!=0;
    const bool muted=!engine.isPassThroughEnabled();
    enabledButton.setEnabled(displayedModule!=0);
    enabledButton.setButtonText(active?"Bypass slot":"Enable slot");
    enabledButton.setColour(juce::TextButton::buttonColourId,active?juce::Colour(0xff24695e):juce::Colour(0xff303944));
    moduleSelect.setEnabled(muted);
    resetButton.setEnabled(muted && displayedModule!=0);
    for(auto& parameter:parameters)parameter.setEnabled(displayedModule!=0 && !displayedAdvanced);
    const bool feedback=isFeedbackModule();
    libraryNote.setVisible(!feedback);
    for(auto* label:{&feedbackState,&feedbackCandidate,&feedbackScore,&feedbackLimit,&feedbackNotches,&feedbackNote})label->setVisible(feedback);
    feedbackArmButton.setVisible(feedback);feedbackClearButton.setVisible(feedback);
    if(feedback){
        const auto snapshot=outputDomain?engine.outputBuiltinFeedbackStatus(channel,selectedSlot):engine.builtinFeedbackStatus(channel,selectedSlot);
        const bool requested=slot.feedbackArmed.load(std::memory_order_acquire);
        const bool running=active && !muted && engine.isDeviceRunning();
        feedbackArmButton.setEnabled(requested || running);
        feedbackArmButton.setButtonText(requested?"DISARM AUTO CUT":"ARM AUTO CUT");
        feedbackArmButton.setColour(juce::TextButton::buttonColourId,requested?juce::Colour(0xff80552b):juce::Colour(0xff24695e));
        feedbackClearButton.setEnabled(active || snapshot.notchCount>0);
        feedbackState.setColour(juce::Label::textColourId,snapshot.armed?rackAmber:rackAccent);
        feedbackState.setText(!active?"IDLE / SLOT BYPASSED":muted?"IDLE / AUDIO MUTED":
            !engine.isDeviceRunning()?"IDLE / NO AUDIO DEVICE":snapshot.armed?"ARMED / AUTOMATIC CUTS":"DISARMED / EXISTING CUTS HELD",juce::dontSendNotification);
        const bool candidate=running && std::isfinite(snapshot.candidateHz) && snapshot.candidateHz>0;
        feedbackCandidate.setText(candidate?juce::String(snapshot.candidateHz,1)+" Hz":"-- Hz",juce::dontSendNotification);
        feedbackCandidate.setTooltip("Current detector candidate; not a confirmed acoustic feedback measurement.");
        const auto modelScore=std::isfinite(snapshot.modelScore)?std::clamp(snapshot.modelScore,0.0f,1.0f):0.0f;
        feedbackScore.setText(displayedModule==builtin::FeedbackGuard?
            "AI model score: "+(running?juce::String(modelScore*100.0f,1)+"%":"--"):
            "Detector: spectral peaks / persistence",juce::dontSendNotification);
        feedbackScore.setTooltip("A model score is not a calibrated probability of real acoustic feedback.");
        const int cuts=juce::jlimit(0,6,snapshot.notchCount);
        feedbackLimit.setText("AUTOMATIC NOTCHES   "+juce::String(cuts)+" / 6",juce::dontSendNotification);
        juce::String cutText;
        for(int i=0;i<6;++i){
            if(i>0)cutText+="\n";
            cutText+=juce::String(i+1)+"   ";
            const float hz=snapshot.notchHz[static_cast<std::size_t>(i)],depth=snapshot.depthDb[static_cast<std::size_t>(i)];
            cutText+=i<cuts && std::isfinite(hz) && hz>0 && std::isfinite(depth)?
                juce::String(hz,1)+" Hz    "+juce::String(depth,1)+" dB":"--";
        }
        feedbackNotches.setText(cutText,juce::dontSendNotification);
        feedbackNote.setText(displayedModule==builtin::FeedbackGuard?
            "Synthetic-trained AI / LAB. Music can trigger cuts. DISARM holds cuts; Clear removes them.":
            "Spectral detector / LAB. Music can trigger cuts. DISARM holds cuts; Clear removes them.",juce::dontSendNotification);
    }
    status.setColour(juce::Label::textColourId,muted?rackAccent:rackAmber);
    status.setText(displayedAdvanced?"Advanced mode active / edit controls in the floating Mixer editor. RESET returns to legacy defaults.":muted?"Audio muted / choose a processor, adjust its parameters, then enable the slot.":
        "Audio passing / parameters update live. Mute audio before replacing a processor.",juce::dontSendNotification);
    for(int i=0;i<4;++i){
        auto& button=slotButtons[static_cast<std::size_t>(i)];
        button.setButtonText(i==selectedSlot?"Editing slot "+juce::String(i+1):"Edit slot "+juce::String(i+1));
        button.setColour(juce::TextButton::buttonColourId,i==selectedSlot?juce::Colour(0xff24695e):juce::Colour(0xff27313c));
    }
}

void BuiltinRackPanel::timerCallback(){
    const auto& slot=currentControls().slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(selectedSlot)];
    if(slot.type.load(std::memory_order_acquire)!=displayedModule){refreshControls();return;}
    if(advancedMode(slot,displayedModule)!=displayedAdvanced){refreshControls();return;}
    refreshing=true;
    const auto& module=builtin::metadata(displayedModule);
    for(int i=0;i<module.parameterCount;++i){
        auto& slider=parameters[static_cast<std::size_t>(i)];
        if(!slider.isMouseButtonDown() && !slider.hasKeyboardFocus(true))slider.setValue(visibleParameterValue(module.params[static_cast<std::size_t>(i)],slot.params[static_cast<std::size_t>(i)].load(std::memory_order_acquire)),juce::dontSendNotification);
    }
    refreshing=false;
    updateParameters();repaint();
}

void BuiltinRackPanel::paint(juce::Graphics& g){
    g.fillAll(rackBackground);
    const auto drawCard=[&g](juce::Rectangle<int> rectangle,juce::Colour border){
        g.setColour(rackCard);g.fillRoundedRectangle(rectangle.toFloat(),7.0f);
        g.setColour(border);g.drawRoundedRectangle(rectangle.toFloat().reduced(0.5f),7.0f,1.0f);
    };
    g.setColour(rackMuted);g.setFont(13.0f);
    g.drawText("Insert position / channel",18,74,330,20,juce::Justification::left);
    g.setColour(juce::Colour(0xff18232a));g.fillRoundedRectangle(status.getBounds().expanded(12,5).toFloat(),6.0f);
    for(int i=0;i<4;++i){
        const auto card=slotCards[static_cast<std::size_t>(i)];
        const auto& slot=currentControls().slots[static_cast<std::size_t>(channel)][static_cast<std::size_t>(i)];
        const int requestedType=slot.type.load(std::memory_order_acquire);
        const int type=requestedType>=0 && requestedType<moduleCount?requestedType:0;
        const bool enabled=type!=0 && slot.enabled.load(std::memory_order_acquire);
        drawCard(card,i==selectedSlot?rackAccent:rackBorder);
        g.setColour(enabled?rackAccent:rackMuted);g.setFont(juce::FontOptions(14.0f,juce::Font::bold));
        g.drawText("SLOT "+juce::String(i+1)+" / "+(enabled?"ACTIVE":"BYPASSED"),card.reduced(12,5).removeFromTop(20),juce::Justification::left);
        g.setColour(rackText);g.setFont(14.0f);
        g.drawText(juce::String::fromUTF8(builtin::metadata(type).name),card.reduced(12,5).withTrimmedTop(22).removeFromTop(24),juce::Justification::left);
    }
    drawCard(editorCard,rackBorder);drawCard(infoCard,rackBorder);
    g.setColour(rackText);g.setFont(juce::FontOptions(15.0f,juce::Font::bold));
    g.drawText(juce::String(outputDomain?"OUTPUT ":"INPUT ")+juce::String(channel+1)+" / SLOT "+juce::String(selectedSlot+1),editorCard.reduced(16,10).removeFromTop(24),juce::Justification::left);
    g.drawText(isFeedbackModule()?"Feedback control":"Native processor library",infoCard.reduced(16,10).removeFromTop(24),juce::Justification::left);
    if(displayedModule==0){
        g.setColour(rackMuted);g.setFont(15.0f);
        g.drawFittedText("Choose a processor above to see its parameters.\nAn empty slot passes the signal unchanged.",
            editorCard.reduced(24).withTrimmedTop(145),juce::Justification::centred,3,1.0f);
    }
    g.setColour(rackMuted);g.setFont(13.0f);
    auto* device=devices.getCurrentAudioDevice();
    const juce::String deviceState=device?juce::String(device->getCurrentSampleRate()/1000.0,1)+" kHz / "+juce::String(device->getCurrentBufferSizeSamples())+" samples":"No audio device";
    g.drawText("Signal order: slot 1 > 2 > 3 > 4. Loaded processors start bypassed.  "+deviceState,18,getHeight()-32,getWidth()-36,22,juce::Justification::left);
}

void BuiltinRackPanel::resized(){
    title.setBounds(18,10,getWidth()-36,31);
    subtitle.setBounds(18,42,getWidth()-36,22);
    domainSelect.setBounds(18,98,160,34);
    inputSelect.setBounds(186,98,128,34);
    status.setBounds(340,88,getWidth()-370,39);
    const int slotWidth=(getWidth()-72)/4;
    for(int i=0;i<4;++i){
        slotCards[static_cast<std::size_t>(i)]={18+i*(slotWidth+12),150,slotWidth,80};
        slotButtons[static_cast<std::size_t>(i)].setBounds(slotCards[static_cast<std::size_t>(i)].reduced(12,0).withY(202).withHeight(23));
    }
    const int libraryWidth=juce::jlimit(265,340,(getWidth()-48)/3);
    editorCard={18,242,getWidth()-48-libraryWidth,getHeight()-286};
    infoCard={getWidth()-18-libraryWidth,242,libraryWidth,getHeight()-286};
    libraryNote.setBounds(infoCard.reduced(16).withTrimmedTop(29));
    auto feedback=infoCard.reduced(16);
    feedback.removeFromTop(28);
    feedbackState.setBounds(feedback.removeFromTop(22));feedback.removeFromTop(4);
    auto armRow=feedback.removeFromTop(32);
    feedbackArmButton.setBounds(armRow.removeFromLeft((armRow.getWidth()-8)/2));armRow.removeFromLeft(8);
    feedbackClearButton.setBounds(armRow);
    feedback.removeFromTop(9);
    feedbackCandidate.setBounds(feedback.removeFromTop(29));
    feedbackScore.setBounds(feedback.removeFromTop(22));feedback.removeFromTop(8);
    feedbackLimit.setBounds(feedback.removeFromTop(20));
    const int noteHeight=43;
    feedbackNote.setBounds(feedback.removeFromBottom(noteHeight));
    feedbackNotches.setBounds(feedback);
    auto editor=editorCard.reduced(16);
    editor.removeFromTop(24);
    moduleLabel.setBounds(editor.removeFromTop(20));
    auto moduleRow=editor.removeFromTop(34);
    const int buttonWidth=juce::jlimit(100,126,moduleRow.getWidth()/5);
    enabledButton.setBounds(moduleRow.removeFromRight(buttonWidth));moduleRow.removeFromRight(8);
    resetButton.setBounds(moduleRow.removeFromRight(juce::jlimit(125,150,editorCard.getWidth()/4)));moduleRow.removeFromRight(8);
    moduleSelect.setBounds(moduleRow);
    editor.removeFromTop(8);
    description.setBounds(editor.removeFromTop(48));
    editor.removeFromTop(6);
    const bool useKnobs=editor.getHeight()>=230;
    const int rowHeight=editor.getHeight()/2;
    const int columnWidth=(editor.getWidth()-24)/3;
    for(int i=0;i<6;++i){
        auto cell=juce::Rectangle<int>(editor.getX()+(i%3)*(columnWidth+12),editor.getY()+(i/3)*rowHeight,columnWidth,rowHeight-5);
        parameterLabels[static_cast<std::size_t>(i)].setBounds(cell.removeFromTop(22));
        auto& slider=parameters[static_cast<std::size_t>(i)];
        slider.setSliderStyle(useKnobs?juce::Slider::RotaryHorizontalVerticalDrag:juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(useKnobs?juce::Slider::TextBoxBelow:juce::Slider::TextBoxRight,false,useKnobs?100:82,26);
        slider.setBounds(cell);
    }
}

} // namespace lah::ui

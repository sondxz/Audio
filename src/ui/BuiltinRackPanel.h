#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/LiveAudioEngine.h"
#include <array>

namespace lah::ui {

class BuiltinRackPanel final : public juce::Component, private juce::Timer {
public:
    BuiltinRackPanel(audio::LiveAudioEngine&,juce::AudioDeviceManager&);
    void selectChannel(int channel,int slot=0,bool output=false);
    void refreshControls();
    void setValidationScenario(int scenario);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void selectSlot(int slot);
    void changeModule();
    void setSlotEnabled();
    void resetParameters();
    void updateParameters();
    void setFeedbackArmed();
    void clearFeedbackNotches();
    bool isFeedbackModule() const noexcept;
    builtin::Controls& currentControls() noexcept {
        return outputDomain?engine.outputBuiltinControls():engine.builtinControls();
    }

    audio::LiveAudioEngine& engine;
    juce::AudioDeviceManager& devices;
    juce::ComboBox domainSelect,inputSelect,moduleSelect;
    std::array<juce::TextButton,4> slotButtons;
    juce::TextButton enabledButton {"Enable slot"},resetButton {"Reset parameters"};
    juce::TextButton feedbackArmButton {"ARM AUTO CUT"},feedbackClearButton {"Clear notches"};
    juce::Label title,subtitle,status,moduleLabel,description,libraryNote;
    juce::Label feedbackState,feedbackCandidate,feedbackScore,feedbackLimit,feedbackNotches,feedbackNote;
    std::array<juce::Label,6> parameterLabels;
    std::array<juce::Slider,6> parameters;
    juce::Rectangle<int> editorCard,infoCard;
    std::array<juce::Rectangle<int>,4> slotCards;
    int channel=0,selectedSlot=0,displayedModule=-1;
    bool refreshing=false,outputDomain=false,displayedAdvanced=false;
};

} // namespace lah::ui

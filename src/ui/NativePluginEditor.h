#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/LiveAudioEngine.h"
#include <array>
#include <memory>

namespace lah::ui {
class NativePresetControls;
class DigitalFxProPanel;
class DynamicsProPanel;

// A non-owning, reusable editor for one native insert. The audio engine outlives it.
class NativePluginEditor final : public juce::Component, private juce::Timer {
public:
    NativePluginEditor(audio::LiveAudioEngine&,int channel,int slot,bool output,bool diskEnabled=false);
    ~NativePluginEditor() override;
    void setTarget(int channel,int slot,bool output);
    void refreshControls();
    int getSelectedModule() const noexcept { return displayedModule; }
    int getTargetChannel() const noexcept { return channel; }
    int getTargetSlot() const noexcept { return slotIndex; }
    bool isOutputTarget() const noexcept { return outputDomain; }
    // Exercises the real control callbacks against atomics. Requires muted audio;
    // restores the original state and checks every other slot remains unchanged.
    bool runControlValidation(juce::String& error);
    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    void parentHierarchyChanged() override;

private:
    class EditorLookAndFeel;
    class ExtendedEqPanel;
    class SpectrumPanel;
    void timerCallback() override;
    builtin::SlotControl& target() noexcept;
    void replaceModule();
    void resetParameters();
    void toggleBypass();
    void toggleFeedbackArm();
    void updateState();
    void updateSmartMacroView();
    std::array<float,6> values() const noexcept;
    builtin::FeedbackStatus feedbackStatus() const noexcept;
    juce::Colour accentColour() const noexcept;
    void paintMeters(juce::Graphics&);
    void paintModels(juce::Graphics&);
    void updateSpectrumSubscription();
    bool proModeEnabled()noexcept;

    audio::LiveAudioEngine& engine;
    std::unique_ptr<EditorLookAndFeel> skin;
    std::unique_ptr<ExtendedEqPanel> eqPanel;
    std::unique_ptr<SpectrumPanel> spectrumPanel;
    std::unique_ptr<NativePresetControls> presetControls;
    std::unique_ptr<DigitalFxProPanel> digitalFxPanel;
    std::unique_ptr<DynamicsProPanel> dynamicsPanel;
    juce::ComboBox moduleSelect;
    juce::TextButton bypassButton{"BYPASS"},resetButton{"RESET"};
    juce::TextButton feedbackArmButton{"ARM AUTO CUT"},feedbackClearButton{"CLEAR CUTS"};
    juce::TextButton eqModeButton{"8 BAND EQ"};
    juce::TextButton analyzerButton{"ANALYZER"};
    juce::TextButton proModeButton{"PRO"};
    juce::Label breadcrumb,title,description,status;
    std::array<juce::Label,6> parameterLabels;
    std::array<juce::Slider,6> parameters;
    juce::Slider smartMacro;
    juce::Label smartMacroState;
    juce::Rectangle<int> modelBounds,meterBounds,controlBounds,heroBounds;
    int channel=0,slotIndex=0,displayedModule=-1,presetModule=-1;
    std::uint32_t presetGeneration=0;
    std::uint32_t proPanelGeneration=0;
    int proPanelModule=-1;
    bool outputDomain=false,refreshing=false,analyzerVisible=false,diskPresetsEnabled=false;
    spectrum::Handle spectrumHandle;
    float inputPeak=0,outputPeak=0;
};
}

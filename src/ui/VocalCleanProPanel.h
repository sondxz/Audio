#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/LiveAudioEngine.h"
#include "../dsp/VocalCleanPro.h"
#include <array>
#include <functional>
#include <memory>

namespace lah::ui {
// A mono vocal processor's actual CLEAN, gate, de-ess, deharsh, compressor,
// tone and monitoring controls. No device, file or AppData access on creation.
class VocalCleanProPanel final : public juce::Component {
public:
    VocalCleanProPanel(audio::LiveAudioEngine&,int channel,int slot,bool output);
    ~VocalCleanProPanel()override;
    void refreshControls();
    bool runValidation(juce::String& error);
    std::function<void()> onChanged;
    void resized()override;
    void paint(juce::Graphics&)override;
private:
    class Content;
    class Model;
    builtin::SlotControl* slot()const noexcept;
    bool valid()const noexcept;
    vocalpro::Snapshot current()const noexcept;
    void parameterChanged(int);
    void toggleParameter(int);
    void updateStageButtons();
    void layoutContent();
    audio::LiveAudioEngine& engine;
    const int channelIndex,slotIndex;
    const bool outputDomain;
    const std::uint32_t expectedGeneration;
    juce::Viewport viewport;
    std::unique_ptr<Content> content;
    std::array<std::unique_ptr<Model>,5> models;
    std::array<juce::Label,vocalpro::parameterCount> labels;
    std::array<juce::Slider,vocalpro::parameterCount> parameters;
    std::array<juce::TextButton,6> stageButtons;
    std::array<juce::Label,7> titles;
    juce::Label explanation,stateText;
    std::array<juce::Rectangle<int>,7> cards;
    bool refreshing=false;
};
} // namespace lah::ui

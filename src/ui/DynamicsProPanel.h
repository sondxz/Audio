#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/LiveAudioEngine.h"
#include "../dsp/DynamicsPro.h"
#include <array>
#include <functional>
#include <memory>

namespace lah::ui {
// One immutable native Dynamics insert. All three stages process actual audio;
// the viewport keeps every control accessible in a small floating editor.
class DynamicsProPanel final : public juce::Component {
public:
    DynamicsProPanel(audio::LiveAudioEngine&,int channel,int slot,bool output);
    ~DynamicsProPanel()override;
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
    dynamicspro::Snapshot current()const noexcept;
    void parameterChanged(int);
    void toggleStage(int);
    void layoutContent();
    audio::LiveAudioEngine& engine;
    const int channelIndex,slotIndex;
    const bool outputDomain;
    const std::uint32_t expectedGeneration;
    juce::Viewport viewport;
    std::unique_ptr<Content> content;
    std::array<std::unique_ptr<Model>,3> models;
    std::array<juce::Label,dynamicspro::parameterCount> labels;
    std::array<juce::Slider,dynamicspro::parameterCount> parameters;
    std::array<juce::TextButton,3> stageButtons;
    std::array<juce::Label,3> stageTitles;
    std::array<juce::Rectangle<int>,3> cards;
    bool refreshing=false;
};
}

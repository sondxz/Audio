#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/LiveAudioEngine.h"
#include "../dsp/DigitalFxPro.h"
#include <array>
#include <functional>
#include <memory>

namespace lah::ui {
// Two mono DSP sections for ONE immutable native Digital FX insert. The
// viewport keeps all eighteen real parameters accessible in a small editor.
class DigitalFxProPanel final : public juce::Component {
public:
    DigitalFxProPanel(audio::LiveAudioEngine&,int channel,int slot,bool output);
    ~DigitalFxProPanel()override;
    void refreshControls();
    bool runValidation(juce::String& error); // muted control-path checks, no device/disk
    std::function<void()> onChanged;
    void resized()override;
    void paint(juce::Graphics&)override;
private:
    class Content;
    class Model;
    bool valid()const noexcept;
    builtin::SlotControl* slot()const noexcept;
    void parameterChanged(int);
    void tapAt(double milliseconds);
    void bpmChanged();
    void layoutContent();
    void updateTimingText();
    digitalfx::Snapshot current()const noexcept;
    audio::LiveAudioEngine& engine;
    const int channelIndex,slotIndex;
    const bool outputDomain;
    const std::uint32_t expectedGeneration;
    juce::Viewport viewport;
    std::unique_ptr<Content> content;
    std::array<std::unique_ptr<Model>,2> models;
    std::array<juce::Label,digitalfx::parameterCount> labels;
    std::array<juce::Slider,digitalfx::parameterCount> parameters;
    juce::Label echoTitle,reverbTitle,timingText,bpmLabel;
    juce::TextButton tapButton{"TAP"},applyBpmButton{"APPLY BPM"};
    juce::Slider bpm;
    juce::Rectangle<int> echoCard,reverbCard;
    double previousTapMs=-1;
    bool refreshing=false;
};
} // namespace lah::ui

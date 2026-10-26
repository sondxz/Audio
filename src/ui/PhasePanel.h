#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../engineering/PhaseCore.h"
#include <array>

namespace lah::ui {
class PhasePanel final : public juce::Component {
public:
    PhasePanel();
    void paint(juce::Graphics&) override;
    void resized() override;
    // Deterministic, silent mathematical scenarios: 0=in phase, 1=cancellation,
    // 2=+90-degree quadrature, 3=positive quarter-cycle delay, 4=mixed frequency.
    void setTestScenario(int scenario);
    bool writeValidationArtifacts(const juce::File& directory,juce::String& error);
private:
    void refreshSelected();
    void recalculate();
    void recalculateConversion();
    void rebuildSources();
    double displayDuration() const;
    juce::Colour sourceColour(std::size_t index) const;
    std::vector<engineering::phase::Source> sources;
    engineering::phase::Sum sum;
    engineering::phase::Waveform trace;
    std::size_t selected=0;
    bool valid=true;
    juce::Label title,notice,countLabel,sourceLabel,summary,selectedDetails,cursorLabel,conversionTitle,conversionResult;
    juce::ComboBox presets,count,sourceSelect,cycles,conversionMode;
    juce::ToggleButton linked {"LINK SOURCE FREQUENCIES"},enabled {"SOURCE ENABLED"};
    juce::TextButton polarity {"POLARITY +"};
    juce::TextButton converterToggle {"PHASE / DELAY CONVERTER"};
    std::array<juce::Slider,4> sourceValues;
    std::array<juce::Label,4> sourceLabels;
    juce::Slider cursor,conversionPhase,conversionDelay,conversionFrequency,cycleBranch;
    juce::Label phaseLabel,delayLabel,frequencyLabel,branchLabel;
    juce::Rectangle<float> sourceBounds,wheelBounds,waveBounds,rightBounds;
};
} // namespace lah::ui

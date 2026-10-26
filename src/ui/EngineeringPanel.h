#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

namespace lah::ui {
class EngineeringPanel final : public juce::Component {
public:
    EngineeringPanel();
    void resized() override;
    void paint(juce::Graphics&) override;
    void setValidationScenario(int scenario);
private:
    void chooseCalculator();
    void recalculate();
    void recalculateExtended();
    void setResults(const juce::String&,const juce::String&);
    juce::ComboBox calculator;
    std::array<juce::Label,9> labels;
    std::array<juce::Slider,9> values;
    juce::Label title,warning;
    juce::TextEditor result;
    juce::TextButton details {"FORMULAS & NOTES"};
    std::array<juce::Label,12> metricNames,metricValues;
    std::array<juce::Rectangle<float>,12> metricBounds;
    juce::Rectangle<float> inputBounds,resultBounds,absorptionBounds;
    int metricCount {};
    int kind {1};
};
} // namespace lah::ui

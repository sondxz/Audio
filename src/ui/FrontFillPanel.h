#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../engineering/CoverageCore.h"
#include <array>

namespace lah::ui {
class FrontFillPanel final : public juce::Component {
public:
    FrontFillPanel();
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void setValidationScenario(int scenario);
private:
    void recalculate();
    void exportLayout();
    juce::ComboBox mode;
    std::array<juce::Slider,8> values;
    std::array<juce::Label,8> captions;
    juce::Label info,warning,probeInfo;
    juce::TextEditor coordinates;
    juce::TextButton applyCoordinates{"Apply CSV"},copyCoordinates{"Edit current layout"},saveCoordinates{"Export geometry CSV"};
    std::unique_ptr<juce::FileChooser> chooser;
    engineering::FillCoverage current;
    std::vector<engineering::AudiencePoint> listeners;
    engineering::CoverageAnalysis analysis;
    engineering::AudiencePoint probe{0,5};
    juce::Rectangle<float> chartBounds,controlBounds;
    double plotCentreX{},plotCentreY{},plotScale{1};
};
} // namespace lah::ui

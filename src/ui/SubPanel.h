#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../simulation/SubArrayCore.h"
#include "../simulation/MeasuredSpeakerCore.h"

namespace lah::ui {
class SubPanel final: public juce::Component {
public:
    SubPanel();
    void setValidationScenario(int scenario);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void recalc();
    void rebuildPreset();
    void loadSub();
    void loadProbe();
    void importProfile();
    simulation::FrequencyPoint evaluate(const std::vector<simulation::Sub>&,double,
        const simulation::PolarProfile&,simulation::Point3,double,double) const;
    simulation::FrequencyTrace makeTrace(const std::vector<simulation::Sub>&,double,
        const simulation::PolarProfile&,simulation::Point3,double) const;
    juce::Colour levelColour(float db) const;
    std::vector<simulation::Sub> subs;
    simulation::Grid grid;
    simulation::Parameters params;
    simulation::PolarProfile profile;
    std::vector<simulation::Sub> savedSubs;
    simulation::PolarProfile savedProfile;
    double savedSpeed=343.0,presetSpeed=343.0;
    bool geometryEdited=false;
    std::array<simulation::Point3,4> probes{{{0,12,1.2},{0,-8,1.2},{-8,12,1.2},{8,12,1.2}}};
    std::array<simulation::FrequencyTrace,4> traces;
    simulation::FrequencyTrace savedTrace;
    std::array<simulation::FrequencyPoint,4> readings;
    std::array<simulation::ArrivalEstimate,4> arrivals;
    std::unique_ptr<juce::FileChooser> filePicker;
    juce::ComboBox presetSelect,viewSelect,sourceSelect,probeSelect,referenceSelect,editorSelect;
    juce::Slider frequency,spacing,sourceDelay,sourceGain,temperature,sourceX,sourceY,sourceZ,
        probeX,probeY,probeZ,arcDepth,focusDistance;
    juce::TextButton invert {"Polarity +"};
    juce::TextButton sourceEnabled {"SOURCE ON"},referencePhase {"TIME REF: ON"},focusProbe {"FOCUS AT PROBE"};
    juce::TextButton saveA {"SAVE ARRAY A"},clearA {"CLEAR A"},resetPreset {"RESET PRESET"};
    juce::TextButton importData {"IMPORT POLAR CSV"},clearData {"OMNI MODEL"};
    juce::Label info,caution,definition;
    juce::Rectangle<float> mapBounds,magnitudeBounds,phaseBounds;
};
} // namespace lah::ui

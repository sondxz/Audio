#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../fir/FIRCore.h"
#include "../audio/LiveAudioEngine.h"

namespace lah::ui {
class FIRPanel final : public juce::Component,private juce::Timer {
public:
    FIRPanel(audio::LiveAudioEngine&,juce::AudioDeviceManager&);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void design();
    void exportCsv();
    void armFIR();
    void bypassFIR();
    void timerCallback() override;
    audio::LiveAudioEngine& audio;
    juce::AudioDeviceManager& deviceManager;
    juce::ComboBox mode,order,rate;
    juce::Slider cutoff;
    juce::Label title,note,status,cutoffLabel;
    juce::TextButton designButton {"DESIGN + VERIFY"},exportButton {"EXPORT FIR CSV"};
    juce::TextButton armButton {"LOAD FIR to OUT 1/2 (MUTE)"},bypassButton {"ENABLE FIR WHILE MUTED"};
    lah::fir::Design filter;
    bool verified=false;
};
}

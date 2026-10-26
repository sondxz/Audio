#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/LiveAudioEngine.h"
#include "../spl/CalibratedSplCore.h"
#include "../spl/SplWorker.h"
#include <functional>
#include <vector>

namespace lah::ui {
class SPLPanel final : public juce::Component,private juce::Timer {
public:
    SPLPanel(audio::LiveAudioEngine&,std::function<double()> rateGetter);
    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshCaptureSelection() {synchronizeInput();}
    std::function<void()> onInputSelectionChanged;
private:
    void timerCallback() override;
    void calibrate();
    void reset();
    bool synchronizeInput();
    audio::LiveAudioEngine& engine;
    std::function<double()> getRate;
    juce::ComboBox micInput,calDb;
    juce::Label title, note, status, details;
    juce::TextButton calibrateButton {"CALIBRATE NOW"}, resetButton {"RESET CAL"};
    std::vector<float> ref,mic;
    spl::SplWorker worker;
    spl::Calibration calibration;
    spl::Statistics stats;
    bool captureGap=false;
    spl::Reading last;
    std::uint64_t deviceGen=0;
    std::uint64_t calibrationEarliestCount=32768;
    int observedMic=-1;
};
}

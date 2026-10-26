#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/LiveAudioEngine.h"
#include "../measurement/MeasurementWorker.h"
#include <functional>
#include <vector>

namespace lah::ui {
class MeasurementPanel final: public juce::Component,private juce::Timer {
public:
    MeasurementPanel(audio::LiveAudioEngine& engine,std::function<double()> sampleRate);
    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshCaptureSelection() {synchronizeInputs();}
    std::function<void()> onInputSelectionChanged;
private:
    void timerCallback() override;
    void exportCsv();
    void takeSnapshot(int slot);
    void synchronizeInputs();
    void drawTrace(juce::Graphics&,const juce::Rectangle<float>& bounds,
                   const std::array<double,measurement::fftSize/2+1>& data,double low,double high,
                   double rate,juce::Colour color,
                   const std::array<bool,measurement::fftSize/2+1>* mask=nullptr);
    audio::LiveAudioEngine& audio;
    std::function<double()> getRate;
    juce::ComboBox refSelect,micSelect,viewSelect;
    juce::Label refLabel,micLabel,note,summary;
    juce::TextButton exportButton {"Export TF/RTA CSV"};
    juce::TextButton snapshotAButton {"SAVE A"},snapshotBButton {"SAVE B"},clearSnapshotsButton {"CLEAR A/B"};
    measurement::MeasurementWorker worker;
    std::uint64_t workerGeneration=0;
    std::uint64_t lastDeviceGeneration=0;
    int observedReference=-1,observedMicrophone=-1;
    measurement::Result result;
    measurement::Result snapshots[2];
    bool hasSnapshot[2] {false,false};
};
} // namespace lah::ui

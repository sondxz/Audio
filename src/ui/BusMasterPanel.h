#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/LiveAudioEngine.h"

namespace lah::ui {
class BusMasterPanel final:public juce::Component,private juce::Timer {
public:
    explicit BusMasterPanel(audio::LiveAudioEngine&);
    void refreshControls();
    void resized()override;
    void paint(juce::Graphics&)override;
    bool runControlValidation(juce::String& error);
private:
    void timerCallback()override;
    void selectSource();
    void configureMasterRouting();
    void configureUsbRouting();
    audio::LiveAudioEngine& engine;
    dsp::BusMasterControls& controls;
    juce::Label title,hint,status,sourceLabel,sourcePanLabel,masterSendLabel;
    juce::TextButton enabled{"BUS AUDIO OFF"},masterRoute{"IN 1-8 > MASTER"},usbRoute{"USB 1:1"};
    juce::ComboBox source,leftOutput,rightOutput;
    juce::Slider sourcePan,sourceMaster;
    std::array<juce::Label,8> busLabels,sendLabels,panLabels;
    std::array<juce::Slider,8> busGain,busPan,sourceSends;
    std::array<juce::ComboBox,8> busOutputs;
    std::array<juce::TextButton,8> busMute,busMaster;
    std::array<juce::Slider,2> masterGain;
    juce::TextButton masterMute{"MASTER MUTE"};
    juce::Label masterTitle,leftLabel,rightLabel;
    std::array<float,10> levels{};
    std::array<juce::Rectangle<int>,10> meters{};
    int selectedSource=0;
};
}

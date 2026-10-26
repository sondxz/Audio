#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/MixDspCore.h"
#include <array>

namespace lah::ui {
class EffectsPanel final: public juce::Component {
public:
    explicit EffectsPanel(dsp::Controls& controls);
    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshControls();
private:
    dsp::Controls& c;
    juce::Label title,warning,sendLabel;
    juce::ComboBox source;
    std::array<juce::Slider,5> slides;
    std::array<juce::Label,5> labels;
    juce::Label sourceLabel,sendHelp,delayHelp,reverbHelp;
    juce::Rectangle<int> flowCard,sendCard,delayCard,reverbCard;
};
}

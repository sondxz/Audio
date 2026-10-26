#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

namespace lah::ui
{
class MeterBank final : public juce::Component
{
public:
    explicit MeterBank (juce::String label);
    void update (const float* values, int channels);
    void paint (juce::Graphics&) override;
private:
    juce::String title;
    std::array<float, 64> visiblePeaks {};
    int numVisible { 0 };
};
} // namespace lah::ui

#include "MeterBank.h"
#include <algorithm>
#include <cmath>

namespace lah::ui
{
MeterBank::MeterBank (juce::String label) : title (std::move (label))
{
    setSize (310, 64);
}

void MeterBank::update (const float* values, int channels)
{
    numVisible = std::clamp (channels, 0, static_cast<int> (visiblePeaks.size()));
    for (int i = 0; i < static_cast<int> (visiblePeaks.size()); ++i)
    {
        const auto peak = (values != nullptr && i < numVisible && std::isfinite (values[i]))
                              ? std::max (0.0f, values[i]) : 0.0f;
        visiblePeaks[static_cast<std::size_t> (i)] = std::max (peak, visiblePeaks[static_cast<std::size_t> (i)] * 0.80f);
    }
    setSize (getWidth(), std::max (64, numVisible * 30 + 42));
    repaint();
}

void MeterBank::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff151b22));
    if (numVisible == 0)
    {
        g.setColour (juce::Colour (0xffa5b3c3));
        g.setFont (juce::FontOptions (13.0f));
        g.drawFittedText (juce::String::fromUTF8("Chưa có kênh ") + title + juce::String::fromUTF8(" đang hoạt động"), getLocalBounds().reduced (10),
                    juce::Justification::centred, 3, 1.0f);
        return;
    }
    const int labelWidth = getWidth() >= 240 ? 59 : 44;
    const int x = labelWidth + 12;
    const int readoutWidth = 46;
    const int barWidth = std::max (16, getWidth() - x - readoutWidth - 18);
    g.setColour (juce::Colour (0xff8192a4));
    g.setFont (juce::FontOptions (11.5f));
    g.drawText ("CH", 10, 7, labelWidth, 20, juce::Justification::centredLeft);
    if (barWidth >= 85)
    {
        for (const int db : {-60, -30, 0})
        {
            const int tickX = x + (db + 60) * barWidth / 60;
            g.drawText (juce::String (db), tickX - 13, 7, 26, 20, juce::Justification::centred);
        }
    }
    g.drawText ("dBFS", x + barWidth + 7, 7, readoutWidth, 20, juce::Justification::centredRight);
    for (int ch = 0; ch < numVisible; ++ch)
    {
        const int y = 32 + ch * 30;
        if ((ch & 1) == 0)
        {
            g.setColour (juce::Colour (0xff19212a));
            g.fillRoundedRectangle (6.0f, static_cast<float> (y - 1), static_cast<float> (getWidth() - 12), 28.0f, 4.0f);
        }
        g.setColour (juce::Colour (0xffc3cfda));
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (title + " " + juce::String (ch + 1).paddedLeft ('0', 2), 10, y, labelWidth, 24,
                    juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff0b1117));
        g.fillRoundedRectangle ((float) x, (float) y + 5.0f, (float) barWidth, 14.0f, 3.0f);

        const auto level = visiblePeaks[static_cast<std::size_t> (ch)];
        const auto dB = 20.0f * std::log10 (std::max (level, 0.00001f));
        const auto fraction = std::clamp ((dB + 60.0f) / 60.0f, 0.0f, 1.0f);
        g.setColour (level >= 1.0f ? juce::Colour::fromRGB (243, 100, 108)
                                     : (level >= 0.7f ? juce::Colour::fromRGB (243, 183, 89)
                                                       : juce::Colour::fromRGB (54, 213, 169)));
        if (fraction > 0.0f)
            g.fillRoundedRectangle ((float) x, (float) y + 5.0f,
                                    std::max (1.0f, barWidth * fraction), 14.0f, 3.0f);
        g.setColour (juce::Colour (0xff151b22));
        for (int tick = 1; tick < 10; ++tick)
            g.drawVerticalLine (x + tick * barWidth / 10, static_cast<float> (y + 5), static_cast<float> (y + 19));
        g.setColour (level >= 1.0f ? juce::Colour (0xfff0767e) : juce::Colour (0xffd6e0e9));
        g.drawText (level <= 0.00001f ? "-inf" : juce::String (dB, 1),
                    x + barWidth + 7, y, readoutWidth, 24, juce::Justification::centredRight);
    }
}
} // namespace lah::ui

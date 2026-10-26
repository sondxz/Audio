#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace lah::audio
{
// Plain C++ core: no allocations, locks, file access or framework dependency.
// Callback slot i is copied to callback slot i. Drivers may compact active physical channels. Missing inputs mean silence.
// peak arrays may be null. Values in the first meterCapacity entries are reset
// at the start of each process() call; enabled and disabled channels are metered.
inline void processPassThrough (const float* const* inputs, int inputCount,
                                float* const* outputs, int outputCount,
                                int sampleCount, bool enabled,
                                float* inputPeaks = nullptr,
                                float* outputPeaks = nullptr,
                                int meterCapacity = 0) noexcept
{
    inputCount  = std::max (0, inputCount);
    outputCount = std::max (0, outputCount);
    sampleCount = std::max (0, sampleCount);
    meterCapacity = std::max (0, meterCapacity);

    for (int ch = 0; ch < meterCapacity; ++ch)
    {
        if (inputPeaks  != nullptr) inputPeaks[ch]  = 0.0f;
        if (outputPeaks != nullptr) outputPeaks[ch] = 0.0f;
    }

    if (inputs != nullptr && inputPeaks != nullptr)
        for (int ch = 0; ch < std::min (inputCount, meterCapacity); ++ch)
            if (const float* source = inputs[ch])
                for (int i = 0; i < sampleCount; ++i)
                {
                    const float x = std::isfinite (source[i]) ? source[i] : 0.0f;
                    inputPeaks[ch] = std::max (inputPeaks[ch], std::abs (x));
                }

    if (outputs == nullptr)
        return;

    for (int ch = 0; ch < outputCount; ++ch)
    {
        float* destination = outputs[ch];
        if (destination == nullptr)
            continue; // Channel is disabled by the driver.

        const float* source = enabled && inputs != nullptr && ch < inputCount ? inputs[ch] : nullptr;
        float peak = 0.0f;
        if (source != nullptr)
        {
            for (int i = 0; i < sampleCount; ++i)
            {
                // Keep finite samples unchanged; drop NaN/Inf before output.
                const float x = std::isfinite (source[i]) ? source[i] : 0.0f;
                destination[i] = x;
                peak = std::max (peak, std::abs (x));
            }
        }
        else
        {
            std::fill_n (destination, static_cast<std::size_t> (sampleCount), 0.0f);
        }
        if (outputPeaks != nullptr && ch < meterCapacity)
            outputPeaks[ch] = peak;
    }
}
} // namespace lah::audio

#include "PassThroughCore.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

#define CHECK(condition) do { if (!(condition)) { std::cerr << "FAILED line " << __LINE__ << ": " #condition "\n"; std::exit (1); } } while (false)

void testBasicAndMute()
{
    float a[] { 0.1f, -0.25f, 0.75f, 0.0f };
    float b[] { -0.8f, 0.2f, 0.0f, 0.5f };
    float outA[] { 9, 9, 9, 9 }, outB[] { 9, 9, 9, 9 };
    const float* inputs[] { a, b };
    float* outputs[] { outA, outB };
    float inPeaks[3] {}, outPeaks[3] {};
    lah::audio::processPassThrough (inputs, 2, outputs, 2, 4, true, inPeaks, outPeaks, 3);
    for (int i = 0; i < 4; ++i) { CHECK (outA[i] == a[i]); CHECK (outB[i] == b[i]); }
    CHECK (inPeaks[0] == 0.75f && inPeaks[1] == 0.8f);
    CHECK (outPeaks[0] == 0.75f && outPeaks[1] == 0.8f && outPeaks[2] == 0.0f);
    lah::audio::processPassThrough (inputs, 2, outputs, 2, 4, false, inPeaks, outPeaks, 3);
    for (int i = 0; i < 4; ++i) { CHECK (outA[i] == 0.0f); CHECK (outB[i] == 0.0f); }
    CHECK (inPeaks[0] == 0.75f && outPeaks[0] == 0.0f);
}

void testChannelMismatchAndDisabledChannels()
{
    float one[] { 0.3f, -0.4f }, other[] { 4, 4 }, absent[] { 8, 8 };
    const float* ins[] { one, nullptr };
    float* outs[] { other, absent, nullptr };
    lah::audio::processPassThrough (ins, 2, outs, 3, 2, true);
    CHECK (other[0] == 0.3f && other[1] == -0.4f);
    CHECK (absent[0] == 0 && absent[1] == 0);
    float* emptyInputOutputs[] { other };
    lah::audio::processPassThrough (nullptr, 0, emptyInputOutputs, 1, 2, true);
    CHECK (other[0] == 0 && other[1] == 0);
    lah::audio::processPassThrough (ins, 2, emptyInputOutputs, 1, 0, true); // zero frames must be safe
}

void testNaNAndInfinite()
{
    float a[] { 0.6f, std::numeric_limits<float>::quiet_NaN(),
                std::numeric_limits<float>::infinity(), -0.2f };
    float b[] { 9, 9, 9, 9 };
    const float* ins[] { a };
    float* outs[] { b };
    lah::audio::processPassThrough (ins, 1, outs, 1, 4, true);
    CHECK (b[0] == 0.6f && b[1] == 0 && b[2] == 0 && b[3] == -0.2f);
}

void testBufferLengthsAndChannels()
{
    for (int frames : { 1, 16, 64, 128, 256, 512, 1024, 2048 })
    {
        const int channelCount = 32;
        std::vector<std::vector<float>> source (channelCount, std::vector<float> (frames));
        std::vector<std::vector<float>> dest (channelCount, std::vector<float> (frames, -99.0f));
        std::array<const float*, channelCount> in {};
        std::array<float*, channelCount> out {};
        for (int ch = 0; ch < channelCount; ++ch)
        {
            for (int i = 0; i < frames; ++i) source[ch][i] = (ch + i % 5) * 0.001f;
            in[ch] = source[ch].data();
            out[ch] = dest[ch].data();
        }
        lah::audio::processPassThrough (in.data(), channelCount, out.data(), channelCount, frames, true);
        for (int ch = 0; ch < channelCount; ++ch)
            for (int i = 0; i < frames; ++i) CHECK (source[ch][i] == dest[ch][i]);
    }
}

int main()
{
    testBasicAndMute();
    testChannelMismatchAndDisabledChannels();
    testNaNAndInfinite();
    testBufferLengthsAndChannels();
    std::cout << "PASS: core pass-through, mute, channel mismatch, nulls, NaN/Inf, 1-2048 frames, 32 ch\n";
}

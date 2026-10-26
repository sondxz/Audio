#pragma once
// Generated numeric weights: tools/train_feedback_model.py. Do not tune by hand.
// Synthetic-only LAB classifier, not calibrated confidence or a PA safety system.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
namespace lah::feedback_model {
inline constexpr std::size_t featureCount = 12, hiddenCount = 12;
inline constexpr float suggestedThreshold = 0.9f;
inline constexpr const char* modelVersion = "synthetic-mlp12-v1";
inline constexpr const char* trainingDatasetSha256 = "066fbd06948d043be19ee8ecbd9e342b26fbabfe3948a43fa03e4c8f6a056f69";
inline constexpr std::array<float, 12> featureMean {{
    -55.7828712f, 7.93440247f, 2.64809847f, 0.269901663f, 0.361038297f, 0.274719507f, 1.12408543f, 0.581424415f, 0.0192291401f, -0.0779019669f, 0.145032868f, 0.414929658f
}};
inline constexpr std::array<float, 12> featureInvStd {{
    0.0334338211f, 0.262628168f, 0.491132319f, 4.72234488f, 2.22928882f, 2.10514784f, 1.0740155f, 1.98138452f, 0.228247687f, 0.541531324f, 2.50520873f, 3.00039983f
}};
inline constexpr std::array<std::array<float, 12>, 12> hiddenWeights {{
    {{-1.38221717f, -2.35934687f, 0.903146863f, -2.36639977f, -2.54714799f, -0.0566650331f, 1.71398199f, 2.13362503f, -1.13952446f, -0.0962571874f, 1.34069395f, 1.763533f}},
    {{0.764973581f, 0.160912752f, 0.291068107f, -0.361893833f, -0.647780597f, -0.271286935f, -0.209361598f, -0.123305634f, 0.719409585f, -0.575237751f, -0.903478622f, -0.189313143f}},
    {{1.40814948f, 0.666759968f, 1.01912105f, -1.00779808f, -0.745343864f, -0.831944525f, 0.579941571f, 1.27271461f, 0.370896429f, 0.193940744f, -0.279964387f, -0.175388694f}},
    {{-0.190679744f, -0.168455258f, -0.648277223f, 0.241594762f, 0.293709815f, -0.284829021f, -0.155302435f, 0.277982682f, 0.101309717f, 0.0636796355f, -0.0738304853f, 0.112360828f}},
    {{-0.168139696f, 1.24659753f, 0.627696335f, 0.374263436f, -0.33815223f, 0.0341693088f, -0.441096276f, -0.722669303f, 0.218495056f, 0.128179669f, -0.0622461103f, -1.09641838f}},
    {{0.413644612f, 1.10291851f, 0.0929716825f, 0.169954777f, -0.357624441f, 0.335912019f, 0.0710437447f, -0.641308367f, -0.00859402213f, -0.680801988f, -0.00648284284f, -1.01845825f}},
    {{-0.455180198f, -1.04804027f, -0.913935721f, 0.394606411f, -0.451588213f, 0.246220514f, -0.350682199f, 0.501820505f, 0.328309655f, 0.244465381f, -0.371746153f, -0.02608216f}},
    {{0.0846802294f, 0.673101485f, -0.0262061954f, -0.024721982f, -0.526060998f, -0.0694993362f, -0.0593091622f, -2.07786107f, 0.0257616136f, -0.608510554f, 0.1066285f, 0.238302693f}},
    {{0.890053928f, -0.655067146f, -0.571302235f, 0.051109314f, 0.794143498f, 0.2374672f, 0.88026315f, -0.01003493f, 1.59714603f, 0.06116613f, -2.01681685f, -1.91663718f}},
    {{1.50425446f, 0.0764487237f, -0.31150341f, -0.302269787f, -3.8818047f, 0.497534633f, -2.20967078f, 0.441363245f, 1.40435803f, 0.449428916f, -1.41840804f, -2.23373795f}},
    {{-0.07893572f, -0.117799453f, 0.0354293101f, 0.0212612189f, 0.233779699f, -0.196266204f, -0.0416201428f, 0.654243708f, 0.0939586386f, 0.0437601805f, -0.0542388782f, -0.600817263f}},
    {{-0.592902005f, -0.411521912f, -0.0184905641f, 0.148907736f, 1.18071485f, 0.330023199f, -0.264114976f, 1.33703482f, 0.387735575f, 0.293250769f, -0.430427164f, 0.951393604f}}
}};
inline constexpr std::array<float, 12> hiddenBias {{
    -1.73932362f, -1.43410599f, -1.54273927f, 0.445800096f, -2.13299966f, -2.51258373f, -0.0749223977f, -1.41296017f, 0.120251797f, -2.08051181f, -0.219923019f, -0.960956395f
}};
inline constexpr std::array<float, 12> outputWeights {{
    -2.07853556f, -2.26690125f, -0.685412765f, 2.10666728f, -4.53667212f, 3.44260931f, -1.69383764f, -2.70461559f, 1.12696791f, 3.41677094f, -1.29661357f, -1.94864631f
}};
inline constexpr std::array<float, 1> outputBias {{
    -2.97776723f
}};

// Features are standardized with training statistics, then clipped to +/-8.
// Return value is a learned sigmoid score, NOT real-world calibrated confidence.
inline float classify(const std::array<float, featureCount>& features) noexcept {
    std::array<float, featureCount> x {};
    for (std::size_t i = 0; i < featureCount; ++i) {
        if (!std::isfinite(features[i])) return 0.0f;
        x[i] = std::clamp((features[i] - featureMean[i]) * featureInvStd[i], -8.0f, 8.0f);
    }
    float logit = outputBias[0];
    for (std::size_t j = 0; j < hiddenCount; ++j) {
        float sum = hiddenBias[j];
        for (std::size_t i = 0; i < featureCount; ++i) sum += x[i] * hiddenWeights[i][j];
        logit += std::tanh(sum) * outputWeights[j];
    }
    return 1.0f / (1.0f + std::exp(-std::clamp(logit, -40.0f, 40.0f)));
}

// O(1) state and work per sample; no FFT, heap allocation, locks or audio delay.
// Call at the real analysis sample rate. Trained at 12/16/22.05/24/44.1 kHz.
class Analyzer {
public:
    void prepare(double rate) noexcept {
        sampleRate = std::isfinite(rate) && rate >= 8000 && rate <= 192000 ? rate : 48000;
        frameSize = static_cast<unsigned>(std::floor(sampleRate * .032 + .5));
        reset();
    }
    void reset() noexcept {
        count = sinceCross = intervals = agreements = 0;
        energy = fourth = lag1 = lag2 = difference = periodSum = periodSquared = peak = 0;
        previous = previous2 = lastInterval = lastDb = lastPeriod = growthEma = toneHz = value = 0;
        haveCross = haveFrame = false;
        lastFeatures.fill(0);
    }
    bool processSample(float input) noexcept {
        const double x = std::clamp(std::isfinite(input) ? static_cast<double>(input) : 0.0, -16.0, 16.0);
        const double square = x * x;
        energy += square; fourth += square * square;
        lag1 += x * previous; lag2 += x * previous2;
        const double delta = x - previous; difference += delta * delta;
        peak = std::max(peak, std::abs(x));
        ++sinceCross;
        if (previous <= 0 && x > 0) {
            const double interval = sinceCross;
            if (haveCross && interval >= sampleRate / 12000.0 && interval <= sampleRate / 60.0) {
                ++intervals; periodSum += interval; periodSquared += interval * interval;
                if (lastInterval > 0 && std::abs(interval - lastInterval) <= std::max(1.0, .05 * lastInterval)) ++agreements;
                lastInterval = interval;
            } else lastInterval = 0;
            sinceCross = 0; haveCross = true;
        }
        previous2 = previous; previous = x;
        if (++count < frameSize) return false;
        const double rms = std::sqrt(energy / frameSize);
        const double db = std::clamp(20 * std::log10(std::max(rms, 1.e-6)), -120.0, 24.0);
        const double crest = std::clamp(20 * std::log10(std::max(peak, 1.e-6) / std::max(rms, 1.e-6)), 0.0, 40.0);
        const double meanPeriod = intervals ? periodSum / intervals : 0;
        toneHz = meanPeriod > 0 ? static_cast<float>(sampleRate / meanPeriod) : 0;
        const double cv = intervals >= 2 ? std::min(2.0, std::sqrt(std::max(0.0, periodSquared / intervals - meanPeriod * meanPeriod)) / meanPeriod) : 2.0;
        const double growth = haveFrame ? std::clamp(db - lastDb, -24.0, 24.0) : 0;
        growthEma = .75 * growthEma + .25 * growth;
        const double change = meanPeriod > 0 && lastPeriod > 0 ? std::min(4.0, std::abs(std::log2(meanPeriod / lastPeriod))) : 0;
        const double denom = std::max(energy, 1.e-24);
        lastFeatures = {{static_cast<float>(db), static_cast<float>(crest),
            static_cast<float>(std::clamp(fourth * frameSize / std::max(energy * energy, 1.e-24), 0.0, 50.0)),
            std::min(1.0f, toneHz / 12000.0f), static_cast<float>(std::clamp(lag1 / denom, -1.0, 1.0)),
            static_cast<float>(std::clamp(lag2 / denom, -1.0, 1.0)), static_cast<float>(std::clamp(difference / denom, 0.0, 4.0)),
            static_cast<float>(cv), static_cast<float>(growth), static_cast<float>(growthEma),
            static_cast<float>(change), intervals ? static_cast<float>(agreements) / intervals : 0}};
        value = classify(lastFeatures);
        lastDb = db; lastPeriod = meanPeriod; haveFrame = true;
        count = intervals = agreements = 0;
        energy = fourth = lag1 = lag2 = difference = periodSum = periodSquared = peak = 0;
        return true;
    }
    float score() const noexcept { return value; }
    float estimatedToneHz() const noexcept { return toneHz; }
    const std::array<float, featureCount>& features() const noexcept { return lastFeatures; }
private:
    double sampleRate = 48000;
    unsigned frameSize = 1536, count = 0, sinceCross = 0, intervals = 0, agreements = 0;
    double energy = 0, fourth = 0, lag1 = 0, lag2 = 0, difference = 0, periodSum = 0, periodSquared = 0, peak = 0;
    double previous = 0, previous2 = 0, lastInterval = 0, lastDb = 0, lastPeriod = 0, growthEma = 0;
    float toneHz = 0, value = 0;
    bool haveCross = false, haveFrame = false;
    std::array<float, featureCount> lastFeatures {};
};
} // namespace lah::feedback_model

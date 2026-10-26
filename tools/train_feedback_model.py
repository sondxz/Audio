"""Train the small, synthetic-only LiveAudioHost feedback classifier.

No downloads, recordings or proprietary weights are used. This script owns
signal generation, 12-feature extraction, weighted BCE/Adam optimization and
held-out evaluation. It exports a dependency-free C++17 header and numerical
fixtures. Metrics describe this generator only, never a measured PA system.
"""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from scipy import signal

SEED = 20261023
FEATURE_NAMES = [
    "level_dbfs", "crest_db", "kurtosis", "cross_frequency_over_12000",
    "lag1_correlation", "lag2_correlation", "difference_energy_ratio",
    "cross_interval_cv", "growth_db", "growth_ema_db",
    "period_change_octaves", "adjacent_interval_agreement",
]
RATES = (12000, 16000, 22050, 24000, 44100)
HIDDEN = 12


def features(samples, rate):
    """Independent batch implementation of the streaming header contract."""
    x = np.clip(np.nan_to_num(samples.astype(np.float64), nan=0, posinf=0,
                              neginf=0), -16, 16)
    window = int(np.floor(rate * .032 + .5))
    count = len(x) // window
    x = x[:count * window]
    previous = np.r_[0., x[:-1]]
    previous2 = np.r_[0., 0., x[:-2]]
    crossings = np.flatnonzero((previous <= 0) & (x > 0))
    periods = np.diff(crossings).astype(float)
    ends = crossings[1:]
    valid = (periods >= rate / 12000) & (periods <= rate / 60)
    agreement = np.zeros(len(periods), dtype=bool)
    if len(periods) > 1:
        agreement[1:] = (valid[1:] & valid[:-1]
                         & (np.abs(periods[1:] - periods[:-1])
                            <= np.maximum(1., .05 * periods[:-1])))
    rows = []
    last_db = last_period = ema = 0.
    for frame in range(count):
        a, b = frame * window, (frame + 1) * window
        part = x[a:b]
        energy = np.dot(part, part)
        rms = np.sqrt(energy / window)
        db = np.clip(20 * np.log10(max(rms, 1.e-6)), -120, 24)
        crest = np.clip(20 * np.log10(max(np.max(np.abs(part)), 1.e-6)
                                    / max(rms, 1.e-6)), 0, 40)
        kurtosis = np.clip(np.sum(part ** 4) * window
                           / max(energy * energy, 1.e-24), 0, 50)
        denom = max(energy, 1.e-24)
        lag1 = np.clip(np.dot(part, previous[a:b]) / denom, -1, 1)
        lag2 = np.clip(np.dot(part, previous2[a:b]) / denom, -1, 1)
        diff = np.clip(np.dot(part - previous[a:b], part - previous[a:b])
                       / denom, 0, 4)
        mask = (ends >= a) & (ends < b) & valid
        p = periods[mask]
        mean_period = float(np.mean(p)) if len(p) else 0.
        hz = rate / mean_period if mean_period else 0.
        cv = min(2., np.std(p) / mean_period) if len(p) >= 2 else 2.
        growth = np.clip(db - last_db, -24, 24) if frame else 0.
        ema = .75 * ema + .25 * growth
        change = min(4., abs(np.log2(mean_period / last_period))) \
            if mean_period and last_period else 0.
        agrees = np.sum(agreement[mask]) / len(p) if len(p) else 0.
        rows.append([db, crest, kurtosis, min(1., hz / 12000), lag1, lag2,
                     diff, cv, growth, ema, change, agrees])
        last_db, last_period = db, mean_period
    return np.asarray(rows, dtype=np.float32)


def normalize_peak(x, peak):
    return x * (peak / max(np.max(np.abs(x)), 1.e-20))


def closed_loop(rng, rate, duration, delay_ms, q, unstable=True, reflections=False):
    """y[n] = source[n] + gain * BPF(y[n-D]); optional delayed echo path.

    Processing D-sized chunks evaluates the real recursive loop, not an added
    sinusoid. The center aligns a delay mode, ensuring an unstable pole when
    loop gain > 1. Output remains linear and is normalized after simulation.
    A second path uses a shorter echo inside the delayed acoustic return.
    """
    n = int(rate * duration)
    delay = max(4, int(round(rate * delay_ms / 1000)))
    low_k, high_k = max(1, int(np.ceil(100 * delay / rate))), \
        max(1, int(np.floor(.42 * delay)))
    mode = int(rng.integers(low_k, max(low_k + 1, high_k + 1)))
    center = mode * rate / delay
    b, a = signal.iirpeak(center, q, fs=rate)
    src = rng.normal(0, 1, n)
    # Excitation includes an unrelated voiced/harmonic input and broadband noise.
    fundamental = rng.uniform(85, 350)
    t = np.arange(n) / rate
    voiced = sum(np.sin(2 * np.pi * fundamental * k * t + rng.uniform(0, 6.28))
                 / k for k in range(1, 6))
    src = .65 * src + .35 * voiced
    gain = rng.uniform(1.12, 1.85) if unstable else rng.uniform(.35, .92)
    y = np.zeros(n, dtype=float)
    zi = np.zeros(max(len(a), len(b)) - 1)
    echo_delay = max(1, delay // 3)
    for start in range(0, n, delay):
        size = min(delay, n - start)
        delayed = np.zeros(size) if start == 0 else y[start-delay:start-delay+size].copy()
        if reflections and start >= 2 * delay:
            # Both paths use already available samples; no algebraic loop.
            delayed += .18 * y[start-delay-echo_delay:start-delay-echo_delay+size]
        filtered, zi = signal.lfilter(b, a, delayed, zi=zi)
        y[start:start+size] = src[start:start+size] + gain * filtered
    y = normalize_peak(y, 10 ** rng.uniform(-1.8, -.2))
    # Keep program/noise in the observed microphone signal at a varying SNR.
    y += normalize_peak(src, max(np.max(np.abs(y)), 1.e-6)
                        * 10 ** rng.uniform(-3.0, -1.5))
    return y.astype(np.float32), {"delay_ms": delay_ms, "q": q,
                                 "center_hz": center, "loop_gain": gain,
                                 "reflections": reflections}


def negative(rng, rate, duration, kind):
    n = int(rate * duration)
    t = np.arange(n) / rate
    if kind == "noise":
        x = rng.normal(size=n)
        if rng.random() < .5:
            x = signal.lfilter([1], [1, -.92], x)
    elif kind == "tone":
        hz = rng.uniform(80, rate * .42)
        x = np.sin(2 * np.pi * hz * t + rng.uniform(0, 6.28))
    elif kind == "sweep":
        x = signal.chirp(t, rng.uniform(80, 500), duration,
                         rng.uniform(rate * .15, rate * .42), method="logarithmic")
    elif kind == "harmonic":
        f0 = rng.uniform(65, 600)
        x = sum(np.sin(2 * np.pi * f0 * k * t + rng.uniform(0, 6.28))
                / k ** rng.uniform(.6, 1.6)
                for k in range(1, min(14, int(rate * .42 / f0))))
    elif kind == "speech_like":
        # Synthetic formants/voicing only; no claim that this is real speech.
        f0 = rng.uniform(80, 280)
        phase = 2 * np.pi * f0 * (t + .002 * np.sin(2 * np.pi * 4.7 * t))
        x = sum(np.sin(k * phase) / k for k in range(1, 16))
        for frequency in (rng.uniform(300, 800), rng.uniform(1000, 2200),
                          min(rate * .42, rng.uniform(2400, 3500))):
            b, a = signal.iirpeak(frequency, 4, fs=rate)
            x += signal.lfilter(b, a, x)
        x *= np.maximum(0, np.sin(2 * np.pi * rng.uniform(2, 5) * t)) ** 2
        x += .05 * rng.normal(size=n)
    elif kind == "music_like":
        f0 = rng.uniform(90, 330)
        notes = [f0, f0 * 2 ** (4 / 12), f0 * 2 ** (7 / 12), f0 * 2]
        x = sum(np.sin(2 * np.pi * f * t + rng.uniform(0, 6.28)) for f in notes)
        x *= .6 + .4 * np.sin(2 * np.pi * rng.uniform(.5, 3) * t) ** 2
        x += .03 * rng.normal(size=n)
    elif kind == "percussive":
        x = rng.normal(size=n)
        envelope = np.exp(-np.mod(t, rng.uniform(.12, .38)) / rng.uniform(.01, .06))
        x *= envelope
    elif kind == "silence":
        return np.zeros(n, dtype=np.float32)
    elif kind == "ambiguous_growing_tone":
        x = np.sin(2 * np.pi * rng.uniform(80, rate * .42) * t)
        # A synthesizer swell can match feedback's observed amplitude trajectory.
        x *= np.exp(rng.uniform(.8, 6) * (t - duration))
    else:
        raise ValueError(kind)
    if kind not in ("silence", "ambiguous_growing_tone"):
        # Some ordinary program material also rises/falls in level.
        x *= np.exp(rng.uniform(-.7, .7) * (t - duration))
    return normalize_peak(x, 10 ** rng.uniform(-2.2, -.2)).astype(np.float32)


def dataset(split, clips_per_kind=50):
    seed_offset = {"train": 0, "validation": 100000, "test": 200000}[split]
    rng = np.random.default_rng(SEED + seed_offset)
    # Entire delay/Q path families are disjoint, not random adjacent frames.
    families = {"train": ([4., 9., 17., 28.], [5., 12., 24.]),
                "validation": ([6., 13., 22., 33.], [8., 17., 32.]),
                "test": ([7., 15., 24., 37.], [10., 20., 40.])}
    delays, qs = families[split]
    rows, labels, categories, clips = [], [], [], []
    specs = [("closed_loop", True)] * 4 + [(k, False) for k in
             ("noise", "tone", "sweep", "harmonic", "speech_like", "music_like",
              "percussive", "silence", "stable_loop")]
    for kind, positive in specs:
        for index in range(clips_per_kind):
            rate = RATES[index % len(RATES)]
            if "loop" in kind:
                x, settings = closed_loop(rng, rate, 1.6,
                                          delays[index % len(delays)],
                                          qs[index % len(qs)], positive,
                                          reflections=(index % 3 == 0))
            else:
                x = negative(rng, rate, 1.6, kind)
                settings = {}
            f = features(x, rate)
            # Below -90 dBFS is not observable feedback evidence. Keep it as
            # negative rather than rewarding the model for invisible signal.
            y = np.full(len(f), float(positive))
            if positive:
                y[f[:, 0] < -90] = 0
            rows.extend(f)
            labels.extend(y)
            categories.extend([kind] * len(f))
            clips.append({"kind": kind, "rate": rate, "settings": settings,
                          "sample_sha256": hashlib.sha256(x.tobytes()).hexdigest()})
    return np.asarray(rows, dtype=np.float64), np.asarray(labels), \
        np.asarray(categories), clips


def sigmoid(x):
    return 1 / (1 + np.exp(-np.clip(x, -40, 40)))


def infer(x, params):
    w1, b1, w2, b2 = params
    return sigmoid(np.tanh(x @ w1 + b1) @ w2 + b2)


def train(x, y, vx, vy, steps):
    rng = np.random.default_rng(SEED + 999)
    params = [rng.normal(0, .15, (len(FEATURE_NAMES), HIDDEN)),
              np.zeros(HIDDEN), rng.normal(0, .15, HIDDEN), np.zeros(1)]
    moments = [np.zeros_like(p) for p in params]
    velocities = [np.zeros_like(p) for p in params]
    weight = np.where(y > .5, .5 / max(np.mean(y), 1.e-6),
                      .5 / max(np.mean(1-y), 1.e-6))
    best_loss, best, best_step = float("inf"), None, 0
    trace = []
    for step in range(1, steps + 1):
        indices = rng.integers(0, len(y), 512)
        a, target, sw = x[indices], y[indices], weight[indices]
        w1, b1, w2, b2 = params
        hidden = np.tanh(a @ w1 + b1)
        out = sigmoid(hidden @ w2 + b2)
        dz = (out - target) * sw / len(indices)
        dh = np.outer(dz, w2) * (1 - hidden * hidden)
        grads = [a.T @ dh + 1.e-4 * w1, np.sum(dh, axis=0),
                 hidden.T @ dz + 1.e-4 * w2, np.array([np.sum(dz)])]
        for p, g, m, v in zip(params, grads, moments, velocities):
            m *= .9; m += .1 * g
            v *= .999; v += .001 * g * g
            p -= .003 * (m / (1 - .9 ** step)) / (np.sqrt(v / (1 - .999 ** step)) + 1.e-8)
        if step == 1 or step % 100 == 0:
            out = infer(vx, params)
            val_loss = float(np.mean(-vy * np.log(np.maximum(out, 1.e-8))
                                    -(1-vy) * np.log(np.maximum(1-out, 1.e-8))))
            trace.append({"step": step, "validation_bce": val_loss})
            if val_loss < best_loss:
                best_loss, best, best_step = val_loss, [p.copy() for p in params], step
    return best, best_step, trace


def metrics(y, score, threshold=.9):
    tp = int(np.sum((y > .5) & (score >= threshold)))
    fp = int(np.sum((y < .5) & (score >= threshold)))
    tn = int(np.sum((y < .5) & (score < threshold)))
    fn = int(np.sum((y > .5) & (score < threshold)))
    # Pairwise rank form with average ties avoids an sklearn dependency.
    order = np.argsort(score, kind="stable")
    ranks = np.empty(len(score), dtype=float)
    ranks[order] = np.arange(1, len(score)+1)
    values, starts, counts = np.unique(score[order], return_index=True, return_counts=True)
    for start, count in zip(starts[counts > 1], counts[counts > 1]):
        ranks[order[start:start+count]] = start + (count+1) / 2
    positive = y > .5
    p, n = int(np.sum(positive)), int(np.sum(~positive))
    auc = (np.sum(ranks[positive])-p*(p+1)/2)/(p*n) if p and n else None
    return {"threshold": threshold, "tp": tp, "fp": fp, "tn": tn, "fn": fn,
            "recall": tp/max(1, tp+fn), "false_positive_rate": fp/max(1, fp+tn),
            "precision": tp/max(1, tp+fp), "roc_auc": auc}


def cpp_array(name, a):
    a = np.asarray(a, dtype=np.float32)
    def literal(x):
        value = format(float(x), ".9g")
        if "." not in value and "e" not in value:
            value += ".0"
        return value + "f"
    if a.ndim == 1:
        return f"inline constexpr std::array<float, {a.size}> {name} {{{{\n    " \
            + ", ".join(map(literal, a)) + "\n}};\n"
    return f"inline constexpr std::array<std::array<float, {a.shape[1]}>, {a.shape[0]}> {name} {{{{\n" \
        + ",\n".join("    {{" + ", ".join(map(literal, row)) + "}}" for row in a) \
        + "\n}};\n"


HEADER_PREFIX = r'''#pragma once
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
'''

HEADER_SUFFIX = r'''
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
'''


def write_header(path, mean, std, params, dataset_hash):
    text = HEADER_PREFIX + f'inline constexpr const char* trainingDatasetSha256 = "{dataset_hash}";\n'
    text += cpp_array("featureMean", mean)
    text += cpp_array("featureInvStd", 1 / std)
    for name, values in zip(("hiddenWeights", "hiddenBias", "outputWeights", "outputBias"), params):
        text += cpp_array(name, values)
    text += HEADER_SUFFIX
    path.write_text(text, encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default="artifacts/feedback-model")
    parser.add_argument("--header", default="src/dsp/FeedbackModelWeights.h")
    parser.add_argument("--canonical", default="tests/data/feedback_model.json")
    parser.add_argument("--clips-per-kind", type=int, default=50)
    parser.add_argument("--steps", type=int, default=4000)
    args = parser.parse_args()
    destination = Path(args.output); destination.mkdir(parents=True, exist_ok=True)
    splits = {}
    for name in ("train", "validation", "test"):
        splits[name] = dataset(name, args.clips_per_kind)
        print(name, "frames", len(splits[name][1]), "clips", len(splits[name][3]), flush=True)
    x, y, _, _ = splits["train"]
    mean, std = np.mean(x, axis=0), np.maximum(np.std(x, axis=0), .01)
    standardized = lambda f: np.clip((f-mean)/std, -8, 8)
    vx, vy, _, _ = splits["validation"]
    params, best_step, trace = train(standardized(x), y, standardized(vx), vy, args.steps)
    params = [p.astype(np.float32) for p in params]
    digest = hashlib.sha256()
    digest.update(np.asarray(x, dtype="<f4").tobytes())
    digest.update(np.asarray(y, dtype="<f4").tobytes())
    dataset_hash = digest.hexdigest()
    results = {}
    for name, (f, target, category, clips) in splits.items():
        score = infer(standardized(f), params)
        results[name] = {"clips": len(clips), "frames": len(target),
                         "metrics": metrics(target, score),
                         "per_category": {str(k): metrics(target[category == k], score[category == k])
                                          for k in np.unique(category)}}
    rng = np.random.default_rng(SEED + 300000)
    ambiguous = np.vstack([features(negative(rng, rate, 1.6, "ambiguous_growing_tone"), rate)
                            for rate in RATES for _ in range(20)])
    ascore = infer(standardized(ambiguous), params)
    results["ambiguous_growing_musical_tones"] = metrics(np.zeros(len(ascore)), ascore)
    manifest = {"seed": SEED, "rates": RATES, "features": FEATURE_NAMES,
                "architecture": [12, HIDDEN, 1], "activation": "tanh, sigmoid",
                "optimizer": "Adam; weighted binary cross entropy; L2 weights 1e-4",
                "steps": args.steps, "best_validation_step": best_step,
                "training_dataset_sha256": dataset_hash,
                "feature_mean": mean.tolist(), "feature_std": std.tolist(),
                "evaluation": results, "training_trace": trace,
                "dataset_clips": {name: clips for name, (_, _, _, clips) in splits.items()},
                "path_split": {"train_delay_ms": [4, 9, 17, 28], "train_q": [5, 12, 24],
                               "validation_delay_ms": [6, 13, 22, 33], "validation_q": [8, 17, 32],
                               "test_delay_ms": [7, 15, 24, 37], "test_q": [10, 20, 40]},
                "limitations": ["Synthetic signals only; no PA/microphone recordings.",
                                "Frames in each clip are correlated; frame counts are not independent trials.",
                                "Growing musical tones may be indistinguishable from feedback.",
                                "Score is not calibrated real-world confidence; default advisory and explicit arming required."]}
    header = Path(args.header)
    write_header(header, mean, std, params, dataset_hash)
    manifest["header_sha256"] = hashlib.sha256(header.read_bytes()).hexdigest()
    manifest["script_sha256"] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    (destination / "training-report.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    # Fixtures include raw waveform, independent features and expected model scores.
    fixtures = []
    for kind in ("tone", "noise", "speech_like", "ambiguous_growing_tone", "closed_loop"):
        rate = 16000
        if kind == "closed_loop":
            raw, _ = closed_loop(rng, rate, .64, 15, 20, True)
        else:
            raw = negative(rng, rate, .64, kind)
        f = features(raw, rate)
        fixtures.append({"kind": kind, "rate": rate, "samples": raw.tolist(),
                         "features": f.tolist(), "scores": infer(standardized(f), params).tolist()})
    (destination / "feature-fixtures.json").write_text(json.dumps(fixtures), encoding="utf-8")
    np.savez(destination / "model.npz", mean=mean, std=std, w1=params[0], b1=params[1],
             w2=params[2], b2=params[3])
    canonical = {"model_version": "synthetic-mlp12-v1", "training_dataset_sha256": dataset_hash,
                 "mean": mean.astype(np.float32).tolist(),
                 "inverse_std": (1 / std).astype(np.float32).tolist(),
                 "hidden_weights": params[0].tolist(), "hidden_bias": params[1].tolist(),
                 "output_weights": params[2].tolist(), "output_bias": params[3].tolist()}
    canonical_path = Path(args.canonical)
    canonical_path.parent.mkdir(parents=True, exist_ok=True)
    canonical_path.write_text(json.dumps(canonical, indent=2), encoding="utf-8")
    for name, result in results.items():
        print(name, json.dumps(result.get("metrics", result)), flush=True)
    print("header", str(header), "dataset_sha256", dataset_hash, flush=True)


if __name__ == "__main__":
    main()

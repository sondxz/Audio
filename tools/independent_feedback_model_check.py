"""Independent source-tree feature/model parity checks, with no audio device.

The bridge only consumes raw f32 and returns C++ features/scores. Python owns
signals and re-derives features; canonical learned constants are checked in as
tests/data/feedback_model.json. This is numerical parity, not PA accuracy.
"""
import argparse
import json
from pathlib import Path
import subprocess

import numpy as np
from scipy import signal


def independent_features(raw, rate):
    x = np.asarray(raw, dtype=np.float64)
    x[~np.isfinite(x)] = 0
    x = np.clip(x, -16, 16)
    frame_length = int(np.floor(rate * .032 + .5))
    frames = len(x) // frame_length
    x = x[:frames * frame_length]
    lagged = np.concatenate(([0.], x[:-1]))
    lagged2 = np.concatenate(([0., 0.], x[:-2]))
    events = np.flatnonzero((lagged <= 0) & (x > 0))
    periods = np.diff(events).astype(float)
    accepted = (periods >= rate / 12000) & (periods <= rate / 60)
    agreement = np.zeros(len(periods))
    agreement[1:] = (accepted[1:] & accepted[:-1]
                     & (np.abs(np.diff(periods)) <= np.maximum(1., .05 * periods[:-1])))
    result = []
    prior_level = prior_period = smoothed_growth = 0.
    for frame in range(frames):
        begin, end = frame * frame_length, (frame+1) * frame_length
        block = x[begin:end]
        squared_sum = np.sum(block * block)
        rms = np.sqrt(squared_sum / frame_length)
        level = np.clip(20 * np.log10(max(rms, 1.e-6)), -120, 24)
        peak = np.max(np.abs(block))
        crest = np.clip(20 * np.log10(max(peak, 1.e-6) / max(rms, 1.e-6)), 0, 40)
        kurtosis = np.clip(np.sum(block ** 4) * frame_length
                           / max(squared_sum ** 2, 1.e-24), 0, 50)
        energy = max(squared_sum, 1.e-24)
        correlation1 = np.clip(np.sum(block * lagged[begin:end]) / energy, -1, 1)
        correlation2 = np.clip(np.sum(block * lagged2[begin:end]) / energy, -1, 1)
        differences = block - lagged[begin:end]
        difference_energy = np.clip(np.sum(differences ** 2) / energy, 0, 4)
        chosen = (events[1:] >= begin) & (events[1:] < end) & accepted
        p = periods[chosen]
        period = np.mean(p) if len(p) else 0.
        hz = rate / period if period else 0.
        variation = min(2., np.std(p) / period) if len(p) >= 2 else 2.
        growth = np.clip(level-prior_level, -24, 24) if frame else 0.
        smoothed_growth = .75 * smoothed_growth + .25 * growth
        change = min(4., abs(np.log2(period/prior_period))) if period and prior_period else 0.
        agreement_rate = np.sum(agreement[chosen]) / len(p) if len(p) else 0.
        result.append([level, crest, kurtosis, min(1., hz / 12000),
                       correlation1, correlation2, difference_energy, variation,
                       growth, smoothed_growth, change, agreement_rate, hz])
        prior_level, prior_period = level, period
    return np.asarray(result)


def independent_score(features, canonical):
    # Decimal constants were rounded to the exact exported float32 values.
    def weight(key):
        return np.asarray(canonical[key], dtype=np.float32).astype(np.float64)
    standardized = np.clip((features - weight('mean')) * weight('inverse_std'), -8, 8)
    hidden = np.tanh(standardized @ weight('hidden_weights') + weight('hidden_bias'))
    logit = hidden @ weight('output_weights') + weight('output_bias')[0]
    return 1 / (1 + np.exp(-np.clip(logit, -40, 40)))


def independent_signal(kind, rate, seed):
    rng = np.random.default_rng(seed)
    n = int(.64 * rate)
    t = np.arange(n) / rate
    if kind == 'tone':
        x = .13 * np.sin(2 * np.pi * 997 * t + .4)
    elif kind == 'noise':
        x = .04 * rng.normal(size=n)
    elif kind == 'voiced':
        phase = 2 * np.pi * 133 * t + .3 * np.sin(2 * np.pi * 5.2 * t)
        x = sum(np.sin(k * phase) / k for k in range(1, 11))
        x *= .08 * (.2 + .8 * np.maximum(0, np.sin(2 * np.pi * 3.3 * t)))
    elif kind == 'growing_tone':
        x = .15 * np.sin(2 * np.pi * 1703 * t) * np.exp(4 * (t-.64))
    elif kind == 'closed_loop':
        # Independent closed-loop transfer polynomial, not the trainer's
        # delay-chunk/state recurrence. Y/U = A / (A - gain*z^-D*B).
        delay = max(4, int(round(.015 * rate)))
        frequency = max(1, round(1400 * delay / rate)) * rate / delay
        numerator, denominator = signal.iirpeak(frequency, 20, fs=rate)
        closed_denominator = np.zeros(delay + len(numerator))
        closed_denominator[:len(denominator)] = denominator
        closed_denominator[delay:] -= 1.3 * numerator
        x = signal.lfilter(denominator, closed_denominator, rng.normal(size=n))
        x *= .2 / max(1.e-15, np.max(np.abs(x)))
    else:
        raise ValueError(kind)
    return np.asarray(x, dtype='<f4')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('--weights', type=Path,
                        default=Path(__file__).resolve().parents[1] / 'tests/data/feedback_model.json')
    args = parser.parse_args()
    binary = args.binary.resolve()
    subprocess.run([str(binary), '--self-test'], check=True, capture_output=True)
    canonical = json.loads(args.weights.read_text(encoding='utf-8'))
    if canonical['model_version'] != 'synthetic-mlp12-v1':
        raise AssertionError('Unexpected reference weights version')
    max_feature = max_score = max_hz = 0.
    total_frames = cases = 0
    for rate in (12000, 16000, 22050, 24000, 44100):
        for kind in ('tone', 'noise', 'voiced', 'growing_tone', 'closed_loop'):
            raw = independent_signal(kind, rate, 20270101 + rate)
            expected = independent_features(raw, rate)
            scores = independent_score(expected[:, :12], canonical)
            process = subprocess.run([str(binary), str(rate)], input=raw.tobytes(),
                                     capture_output=True, check=False)
            if process.returncode:
                raise AssertionError(f'{kind}/{rate}: bridge failed {process.returncode}')
            actual = np.frombuffer(process.stdout, dtype='<f4').reshape(-1, 14)
            if len(actual) != len(expected):
                raise AssertionError((kind, rate, actual.shape, expected.shape))
            feature_error = float(np.max(np.abs(actual[:, :12] - expected[:, :12])))
            score_error = float(np.max(np.abs(actual[:, 12] - scores)))
            hz_error = float(np.max(np.abs(actual[:, 13] - expected[:, 12])))
            if feature_error > 3.e-5 or score_error > 2.e-5 or hz_error > .002:
                raise AssertionError((kind, rate, feature_error, score_error, hz_error))
            max_feature = max(max_feature, feature_error)
            max_score = max(max_score, score_error)
            max_hz = max(max_hz, hz_error)
            total_frames += len(actual); cases += 1
    print(f'PASS: {cases} independently generated clips, {total_frames} frames; '
          f'max features {max_feature:.7g}, score {max_score:.7g}, Hz {max_hz:.7g}; '
          'NaN/Inf guards and reset PASS. Numerical parity only, not PA validation.')


if __name__ == '__main__':
    main()

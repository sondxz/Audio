"""Independent synthetic checks for the native feedback suppressor.

Python owns all waveforms, evaluates steady-state tone amplitudes, derives bell
and bandpass coefficients through analog prototypes/SciPy BLT, and evaluates the
unarmed closed-loop plant as a separate transfer polynomial. The bridge only
processes input samples. These tests do not establish classification accuracy on
real music, gain-before-feedback improvement, RIR robustness, or hearing safety.
Growing musical sine tones remain an explicitly ambiguous case.
"""
import argparse
import json
import subprocess
from pathlib import Path

import numpy as np
from scipy import signal


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def exact(actual, expected):
    return np.array_equal(np.asarray(actual, dtype=np.float32).view(np.uint32),
                          np.asarray(expected, dtype=np.float32).view(np.uint32))


def rms(x):
    return float(np.sqrt(np.mean(np.asarray(x, dtype=np.float64)**2)))


def tone(rate, hz, seconds=4, amplitude=.2):
    t = np.arange(int(round(rate*seconds)), dtype=np.float64)/rate
    return (amplitude*np.sin(2*np.pi*hz*t)).astype(np.float32)


def growing_tone(rate, hz, seconds=5, slope=12, cap=.25):
    t = np.arange(int(round(rate*seconds)), dtype=np.float64)/rate
    envelope = np.minimum(cap, .002*10**(t*slope/20))
    return (envelope*np.sin(2*np.pi*hz*t)).astype(np.float32)


def analog_bell(rate, hz, q, db):
    omega = 2*rate*np.tan(np.pi*hz/rate)
    amplitude = 10**(db/40)
    return signal.bilinear([1, omega*amplitude/q, omega*omega],
                          [1, omega/(amplitude*q), omega*omega], fs=rate)


def analog_bandpass(rate, hz, q):
    omega = 2*rate*np.tan(np.pi*hz/rate)
    return signal.bilinear([omega/q, 0], [1, omega/q, omega*omega], fs=rate)


class Harness:
    def __init__(self, binary):
        self.binary = Path(binary).resolve()
        require(self.binary.is_file(), f"Missing bridge: {self.binary}")
        self.cases = 0

    def render(self, source, rate=48000, armed=True, model=False, persistence=350,
               threshold=.9, depth=12, q=30, reset=-1, disarm=-1,
               loop_delay=0, loop_gain=0, loop_hz=1000, loop_q=20):
        source = np.asarray(source, dtype='<f4')
        args = [str(self.binary), str(rate), str(int(armed)), str(int(model)),
                str(persistence), str(threshold), str(depth), str(q),
                str(reset), str(disarm), str(loop_delay), str(loop_gain),
                str(loop_hz), str(loop_q)]
        result = subprocess.run(args, input=source.tobytes(), capture_output=True, check=False)
        require(result.returncode == 0, f"Bridge failed: {result.stderr.decode(errors='replace')}")
        require(len(result.stdout) == source.nbytes, "Bridge output truncated or wrongly sized")
        output = np.frombuffer(result.stdout, dtype='<f4').astype(np.float64)
        require(np.all(np.isfinite(output)), "Non-finite output")
        meta = json.loads(result.stderr)
        require(0 <= meta['notchCount'] <= 6 and meta['maximumNotchCount'] <= 6, "Notch count outside bound")
        require(0 <= meta['maximumScore'] <= 1, "Model score outside bound")
        depths = np.asarray(meta['depthDb'], dtype=np.float64)
        require(np.all((-12.001 <= depths) & (depths <= 0)) and -sum(depths) <= 36.001, "Cut budget exceeded")
        self.cases += 1
        return output, meta


def basic_and_suppression(h, rate):
    hz = 1234.56
    source = growing_tone(rate, hz)
    output, meta = h.render(source, rate, armed=False)
    require(exact(output, source) and meta['notchCount'] == 0, "Monitor mode modifies audio")
    require(output[0] == source[0] and output[1] == source[1], "Audio analysis introduced lookahead/buffering")
    output, meta = h.render(source, rate)
    count = meta['notchCount']
    require(count >= 1, f"Growing tone not suppressed at {rate}")
    require(any(abs(f-hz) < max(8, hz*.01) for f in meta['notchHz'][:count]), "Wrong target frequency")
    first = meta['firstNotchFrame']/rate
    require(.1 < first < 3, f"Unexpected synthetic reaction time: {first}s")
    attenuation = 20*np.log10(rms(output[-rate//2:])/rms(source[-rate//2:]))
    require(attenuation < -7, f"Insufficient synthetic target attenuation {attenuation:.3f} dB")
    fixed = tone(rate, hz)
    output, meta = h.render(fixed, rate)
    require(meta['notchCount'] == 0 and exact(output, fixed), "Stationary musical/test tone was cut")
    print(f"{rate} Hz: growing off-bin tone first cut {first*1000:.2f} ms; final attenuation {attenuation:.3f} dB; stationary tone unchanged")


def independent_notch_transfer(h, rate):
    # After learning, DISARM freezes installed filters. A silence interval clears
    # the filter's ordinary IIR transient before the independent probe begins.
    training = growing_tone(rate, 1777.77, seconds=3)
    silence = np.zeros(rate//3, dtype=np.float32)
    rng = np.random.default_rng(992173+rate)
    probe = rng.uniform(-.08, .08, rate//2).astype(np.float32)
    source = np.r_[training, silence, probe]
    output, meta = h.render(source, rate, disarm=len(training))
    require(meta['notchCount'] == 1 and not meta['armed'], "DISARM must hold exactly the learned notch")
    f = meta['notchHz'][0]
    db = meta['depthDb'][0]
    b, a = analog_bell(rate, f, 30, db)
    expected = signal.lfilter(b, a, probe)
    error = float(np.max(np.abs(output[-len(probe):]-expected)))
    require(error < 2e-6, f"Independent analog-BLT bell sample error {error}")
    _, response = signal.freqz(b, a, worN=[f, f*1.5], fs=rate)
    require(abs(20*np.log10(abs(response[0]))-db) < 1e-7, "Bell center depth/unit definition")
    require(abs(20*np.log10(abs(response[1]))) < .15, "Non-target passband loss exceeds independent design anchor")
    print(f"{rate} Hz: learned fixed notch vs independent analog BLT/SciPy lfilter max error {error:.3g}")


def multiple_and_negative_sources(h):
    rate = 48000
    source = growing_tone(rate, 713, seconds=6, cap=.15)+growing_tone(rate, 2519, seconds=6, cap=.15)
    output, meta = h.render(source, rate)
    require(meta['notchCount'] >= 2, "Simultaneous independent growing tones were not both cut")
    for hz in (713, 2519):
        require(any(abs(f-hz) < max(8, hz*.01) for f in meta['notchHz'][:meta['notchCount']]), "Multiple-tone notch missing")
        # Independent synchronous projection rejects the other non-harmonic tone.
        n = np.arange(rate, dtype=np.float64)
        basis = np.exp(-2j*np.pi*hz*n/rate)
        dry = abs(np.dot(source[-rate:], basis))
        wet = abs(np.dot(output[-rate:], basis))
        require(20*np.log10(wet/dry) < -7, f"Target {hz} Hz not reduced by 7 dB")
    rng = np.random.default_rng(672818)
    seconds = 4
    n = np.arange(rate*seconds, dtype=np.float64)
    t = n/rate
    negatives = {
        'silence': np.zeros(rate*seconds),
        'broadband crescendo': rng.normal(0, .05, rate*seconds)*np.minimum(1, .05*10**(t*12/20)),
        'harmonic instrument': sum(tone(rate, 220*k, seconds, .1/k) for k in range(1, 7)),
        'vibrato': .2*np.sin(2*np.pi*880*t+3*np.sin(2*np.pi*6*t)),
        'chirp': .2*signal.chirp(t, 180, t[-1], 7000),
        'short tone burst': np.r_[tone(rate, 1800, .1, .4), np.zeros(rate*seconds-rate//10)],
        'percussion': rng.normal(0, .15, rate*seconds)*np.exp(-20*np.remainder(t, .4)),
    }
    for label, source in negatives.items():
        output, meta = h.render(source, rate)
        require(meta['maximumNotchCount'] == 0, f"False cut on named synthetic negative: {label}")
        require(exact(output, source), f"Uncut negative changed audio: {label}")
    print("2 simultaneous tones reduced; silence/noise crescendo/harmonic/vibrato/chirp/burst/percussion: no automatic cuts")


def reset_and_extremes(h):
    rate = 48000
    train = growing_tone(rate, 1357.9, 3)
    dry_probe = tone(rate, 750, 1, .15)
    output, meta = h.render(np.r_[train, dry_probe], rate, reset=len(train))
    # Reset is explicitly disarmed; the bridge reconfigures the requested arm
    # mode but the following constant tone has no qualifying growth history.
    require(meta['maximumNotchCount'] > 0 and meta['notchCount'] == 0, "Reset retained a learned cut")
    require(exact(output[len(train):], dry_probe), "Reset reused stale filter or analysis state")
    for rate in (8000, 44100, 48000, 96000, 192000):
        source = np.r_[np.nan, np.inf, -np.inf, np.finfo(np.float32).max,
                       -np.finfo(np.float32).max, np.zeros(8192)].astype(np.float32)
        h.render(source, rate)
    print("Reset clears learned filters/history; 8..192 kHz damaged input produces finite output")


def closed_loop(h):
    rate, delay, hz, q = 48000, 48, 1000, 20
    source = np.zeros(rate*2, dtype=np.float32)
    source[:rate//10] = tone(rate, hz, .1, .001)
    # Independent polynomial: Y/X = A(z)/(A(z)-g*z^-D*B(z)).
    b, a = analog_bandpass(rate, hz, q)
    gain = .85
    denominator = np.r_[a, np.zeros(delay)]
    denominator[delay:delay+len(b)] -= gain*b
    expected = signal.lfilter(a, denominator, source)
    actual, meta = h.render(source, rate, armed=False, loop_delay=delay,
                            loop_gain=gain, loop_hz=hz, loop_q=q)
    error = float(np.max(np.abs(actual-expected)))
    require(error < 2e-7, f"Closed-loop bridge disagrees with independent transfer polynomial: {error}")
    # Intentionally unstable synthetic loop: seed stops after 100ms, so any
    # later sustained growth comes from the recursive test plant, not the source.
    source = np.zeros(rate*6, dtype=np.float32)
    source[:rate//10] = tone(rate, hz, .1, .001)
    kwargs = dict(loop_delay=delay, loop_gain=1.08, loop_hz=hz, loop_q=q)
    unarmed, no_cut = h.render(source, rate, armed=False, **kwargs)
    armed, cut = h.render(source, rate, **kwargs)
    require(cut['notchCount'] >= 1, "Unstable synthetic closed loop not suppressed")
    tail = rate//2
    ratio = 20*np.log10(max(rms(armed[-tail:]), 1e-30)/max(rms(unarmed[-tail:]), 1e-30))
    require(ratio < -20, f"Unstable closed-loop tail reduction insufficient: {ratio:.3f} dB")
    require(cut['maximumOutput'] < 1, "Armed loop reached high simulated output before containment")
    # Learned gate is exercised on the same external plant, not only on feature fixtures.
    learned, learned_meta = h.render(source, rate, model=True, **kwargs)
    require(learned_meta['notchCount'] >= 1, "AI model gate failed the named synthetic feedback loop")
    require(rms(learned[-tail:]) < .1*rms(unarmed[-tail:]), "AI-gated loop tail not reduced by 20 dB")
    displayed_ratio = f"{ratio:.2f} dB" if ratio >= -120 else "below -120 dB numerical floor"
    print(f"Closed-loop independent polynomial max error {error:.3g}; unstable loop tail reduction {displayed_ratio}; AI-gated loop contained")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary')
    args = parser.parse_args()
    harness = Harness(args.binary)
    for rate in (44100, 48000, 96000):
        basic_and_suppression(harness, rate)
        independent_notch_transfer(harness, rate)
    multiple_and_negative_sources(harness)
    reset_and_extremes(harness)
    closed_loop(harness)
    print(f"PASS: {harness.cases} independent feedback render cases; synthetic only, no real-world AI accuracy or acoustic safety claim")


if __name__ == '__main__':
    main()

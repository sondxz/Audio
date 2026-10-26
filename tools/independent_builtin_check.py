"""Independent offline references for the 17 native processor modules.

Python owns all input vectors. The C++ bridge only passes samples through Rack.
Filter references use analog prototypes + SciPy's bilinear transform; dynamics
use closed-form exponential segments; delay references use explicit impulse
trains. Generic state checks are additional invariants, not algorithm proofs.
No hardware, acoustic feedback classification, ML, or proprietary emulation is
validated by this suite. See docs/BUILTIN_PROCESSOR_VALIDATION.md.
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


def close(actual, expected, tolerance, label):
    error = float(np.max(np.abs(np.asarray(actual) - np.asarray(expected))))
    require(error <= tolerance, f"{label}: maximum sample error {error:.7g} > {tolerance:.7g}")
    return error


def bit_exact_f32(actual, expected):
    return np.array_equal(np.asarray(actual, dtype=np.float32).view(np.uint32),
                          np.asarray(expected, dtype=np.float32).view(np.uint32))


class Harness:
    def __init__(self, binary):
        self.binary = Path(binary).resolve()
        require(self.binary.is_file(), f"Missing CLI: {self.binary}")
        result = subprocess.run([str(self.binary), "--catalog"], capture_output=True, check=False)
        require(result.returncode == 0, f"Catalog failed: {result.stderr.decode(errors='replace')}")
        self.catalog = {item["type"]: item for item in json.loads(result.stdout)}
        require(set(self.catalog) == set(range(1, 18)), "Catalog must contain exactly types 1..17")
        for item in self.catalog.values():
            require(item["name"] and len(item["parameters"]) <= 6, "Invalid module metadata")
            for p in item["parameters"]:
                require(p["min"] <= p["default"] <= p["max"], "Default outside parameter range")
        self.cases = 0
        self.last_candidate = None

    def defaults(self, module):
        return [p["default"] for p in self.catalog[module]["parameters"]] + [0] * (6 - len(self.catalog[module]["parameters"]))

    def render(self, module, rate, mono, params=None, enabled=True, reset=-1, disable=-1, block=0):
        mono = np.asarray(mono, dtype=np.float32)
        # A different deterministic signal in ch1 detects channel-state leakage.
        right = (np.sin(np.arange(len(mono), dtype=np.float64) * 0.073) * 0.123).astype(np.float32)
        source = np.column_stack((mono, right)).astype("<f4")
        parameters = self.defaults(module) if params is None else list(params)
        require(len(parameters) == 6, "Supply all six parameters")
        args = [str(self.binary), str(module), str(rate), *[str(float(p)) for p in parameters],
                str(int(enabled)), str(reset), str(disable), str(block)]
        result = subprocess.run(args, input=source.tobytes(), capture_output=True, check=False)
        label = f"{self.catalog[module]['name']}@{rate}"
        require(result.returncode == 0, f"{label}: CLI error {result.stderr.decode(errors='replace')}")
        require(len(result.stdout) == source.nbytes, f"{label}: truncated/wrong-size output")
        output = np.frombuffer(result.stdout, dtype="<f4").reshape((-1, 2))
        require(np.all(np.isfinite(output)), f"{label}: non-finite output")
        require(bit_exact_f32(output[:, 1], right), f"{label}: inactive channel is not bit-exact bypass")
        self.last_candidate = json.loads(result.stderr)["candidateHz"] if module == 4 else None
        self.cases += 1
        return output[:, 0].astype(np.float64)


def state_invariants(harness, rate):
    rng = np.random.default_rng(9371 + rate)
    probe = rng.uniform(-0.3, 0.3, 4096).astype(np.float32)
    length = int(np.ceil(rate * 0.55))
    segment = np.zeros(length, dtype=np.float32)
    segment[:min(2048, length)] = rng.uniform(-0.2, 0.2, min(2048, length))
    segment[0] = 0.5
    for module in sorted(harness.catalog):
        label = f"{harness.catalog[module]['name']}@{rate}"
        output = harness.render(module, rate, probe, enabled=False)
        require(bit_exact_f32(output, probe), f"{label}: disabled slot modifies finite samples")
        extremes = np.array([np.finfo(np.float32).max, -np.finfo(np.float32).max, 17, -17, 0.0, -0.0], dtype=np.float32)
        output = harness.render(module, rate, extremes, enabled=False)
        require(bit_exact_f32(output, extremes), f"{label}: disabled slot clamps/changes finite input bits")
        output = harness.render(module, rate, np.tile(segment, 2), reset=length)
        close(output[:length], output[length:], 1e-7, f"{label} reset/replay")
        switched = harness.render(module, rate, np.tile(probe, 2), disable=len(probe))
        require(bit_exact_f32(switched[len(probe)+64:], probe[64:]), f"{label}: disable is not exact after the 64-sample transition")
        variable = harness.render(module, rate, probe)
        fixed = harness.render(module, rate, probe, block=64)
        close(variable, fixed, 1e-7, f"{label} callback partition")
        damaged = np.r_[np.nan, np.inf, -np.inf, np.finfo(np.float32).max,
                        -np.finfo(np.float32).max, np.zeros(4091)].astype(np.float32)
        harness.render(module, rate, damaged)
    print(f"all {len(harness.catalog)} modules@{rate}: exact bypass/channel isolation, reset/replay, callback partition, finite-output PASS")


def analog_biquad(kind, rate, hz, q=2 ** -0.5, db=0):
    """Independent analog polynomial + prewarped BLT, not host RBJ code."""
    omega = 2 * rate * np.tan(np.pi * hz / rate)
    if kind == "hp":
        numerator = [1, 0, 0]
        denominator = [1, omega / q, omega * omega]
    elif kind == "lp":
        numerator = [omega * omega]
        denominator = [1, omega / q, omega * omega]
    elif kind == "peak":
        amplitude = 10 ** (db / 40)
        numerator = [1, omega * amplitude / q, omega * omega]
        denominator = [1, omega / (amplitude * q), omega * omega]
    else:
        raise ValueError(kind)
    return signal.bilinear(numerator, denominator, fs=rate)


def filter_chain(source, *filters):
    # The native boundary stores float samples between sections; account for
    # that explicit quantization while using SciPy for each transfer function.
    result = np.asarray(source, dtype=np.float32)
    for b, a in filters:
        result = signal.lfilter(b, a, result).astype(np.float32)
    return result.astype(np.float64)


def exponential_segments(levels, rate, attack_ms, release_ms, initial=0):
    """Closed form on piecewise-constant |x| intervals; no sample recurrence."""
    levels = np.abs(np.asarray(levels, dtype=np.float64))
    boundaries = np.r_[0, np.flatnonzero(np.diff(levels)) + 1, len(levels)]
    result = np.empty_like(levels)
    previous = float(initial)
    for start, end in zip(boundaries[:-1], boundaries[1:]):
        target = levels[start]
        tau = (attack_ms if target > previous else release_ms) / 1000
        ticks = np.arange(1, end - start + 1)
        result[start:end] = target + (previous - target) * np.exp(-ticks / (rate * tau))
        previous = result[end - 1]
    return result


def f32(value):
    return float(np.float32(value))


def gain(db):
    return float(np.float32(10) ** (np.float32(db) * np.float32(0.05)))


def frames(rate, seconds):
    return int(np.ceil(rate * seconds))


def impulse(rate, seconds=0.25, amplitude=0.1):
    result = np.zeros(frames(rate, seconds), dtype=np.float32)
    result[0] = amplitude
    return result


def tone(rate, hz, seconds=0.8, amplitude=0.2):
    return (amplitude * np.sin(2*np.pi*hz*np.arange(frames(rate, seconds))/rate)).astype(np.float32)


def filter_references(h, rate):
    x = impulse(rate)
    hp = lambda hz: analog_biquad("hp", rate, hz)
    lp = lambda hz: analog_biquad("lp", rate, hz)
    bell = lambda hz, q, db: analog_biquad("peak", rate, hz, f32(q), db)
    cases = [
        (1, [100, -6, 0, -18, 1, 0], [hp(100), bell(250, .9, -6)], 1),
        (2, [80, 4, -2, 0, 100, -1], [hp(80), bell(180, .7, 4), bell(3500, .8, -2)], gain(-1)),
        (3, [1200, -18, 15, 40, -3, 100], [bell(1200, 15, -18), hp(40)], 1),
        (4, [1200, -18, 15, 300, 5, 0], [bell(1200, 15, -18)], 1),
        (5, [200, 4, 1500, 9, .8, -3], [hp(200), bell(100, .7, 4), bell(1500, .8, 9), bell(8000, .7, -3)], 1),
        (6, [80, 0, 3, 0, 50, 0], [hp(80), bell(3000, .8, 3)], 1),
        (13, [30, 120, 3, 1, 0, -3], [hp(30), lp(120), lp(120), bell(55, .8, 3)], -gain(-3)),
        (14, [50, 2, -3, -90, 150, 0], [hp(50), bell(90, .8, 2), bell(4500, .8, -3)], 1),
        (16, [35, 3, -2, 0, 100, -1], [hp(35), bell(150, .6, 3), bell(10000, .6, -2)], gain(-1)),
        (17, [40, 2, -4, 3, -1, 0], [hp(40), bell(100, .7, 2), bell(400, 1.1, -4), bell(3000, 1.1, 3), bell(10000, .7, -1)], 1),
    ]
    for module, params, filters, scale in cases:
        actual = h.render(module, rate, x, params)
        expected = filter_chain(x, *filters) * scale
        close(actual, expected, 5e-7, f"{h.catalog[module]['name']}@{rate} independent analog-BLT impulse")
    # Independent frequency/gain anchors prevent a reference with wrong units
    # or a mislabeled fourth-order cutoff from silently becoming the oracle.
    for kind, hz, q, db, expected_db in (("hp", 200, 2**-.5, 0, -3.01029995664),
                                        ("peak", 1500, .8, 9, 9)):
        b, a = analog_biquad(kind, rate, hz, q, db)
        _, response = signal.freqz(b, a, worN=[hz], fs=rate)
        require(abs(20*np.log10(abs(response[0]))-expected_db) < 1e-8, f"{kind} anchor definition")
    b, a = lp(120)
    _, response = signal.freqz(b, a, worN=[120], fs=rate)
    require(abs(40*np.log10(abs(response[0])) + 6.02059991328) < 1e-7, "Sub Pro cascaded LP cutoff")
    print(f"filter chains@{rate}: {len(cases)} module types vs independent analog BLT/SciPy impulse PASS")


def nonlinear_and_adaptive(h, rate):
    rng = np.random.default_rng(rate + 817)
    x = rng.uniform(-1.1, 1.1, 8192).astype(np.float32)
    # Color: independently filter, evaluate a pointwise tanh curve, then mix.
    params = [80, 2, -1, 9, 65, -2]
    filtered = filter_chain(x, analog_biquad("hp", rate, 80),
                            analog_biquad("peak", rate, 180, f32(.7), 2),
                            analog_biquad("peak", rate, 3500, f32(.8), -1))
    drive = gain(9)
    colored = np.tanh(filtered*drive)/drive
    expected = (x + (colored-x)*.65)*gain(-2)
    close(h.render(2, rate, x, params), expected, 8e-7, "Vocal Color tanh/wet/output")
    # New console processors use independently transformed filter prototypes,
    # then a pointwise saturation curve. This covers the active DSP, not labels.
    params = [35, 2, -1, 9, 65, -2]
    filtered = filter_chain(x, analog_biquad("hp", rate, 35),
                            analog_biquad("peak", rate, 150, f32(.6), 2),
                            analog_biquad("peak", rate, 10000, f32(.6), -1))
    expected = (x+(np.tanh(filtered*drive)/drive-x)*.65)*gain(-2)
    close(h.render(16, rate, x, params), expected, 8e-7, "Analog independent filter/tanh/wet/output")
    params = [40, 2, -3, 1, -2, 9]
    filtered = filter_chain(x, analog_biquad("hp", rate, 40),
                            analog_biquad("peak", rate, 100, f32(.7), 2),
                            analog_biquad("peak", rate, 400, f32(1.1), -3),
                            analog_biquad("peak", rate, 3000, f32(1.1), 1),
                            analog_biquad("peak", rate, 10000, f32(.7), -2))
    close(h.render(17, rate, x, params), np.tanh(filtered*drive)/drive, 8e-7, "4K Line independent four-band/tanh")
    # PC Audio: unity compressor ratio isolates the filter and sample ceiling.
    params = [40, 3, -2, 0, 1, -3]
    filtered = filter_chain(x, analog_biquad("hp", rate, 40),
                            analog_biquad("peak", rate, 100, f32(.7), 3),
                            analog_biquad("peak", rate, 7000, f32(.7), -2))
    close(h.render(15, rate, x, params), np.clip(filtered, -gain(-3), gain(-3)), 6e-7, "PC Audio independent clipping curve")
    # A high-frequency tone is reduced by the de-esser; a quiet low-frequency
    # tone remains identical. This is a behavioral check, not voice/noise QA.
    for hz, amplitude, must_reduce in ((8000, .3, True), (200, .01, False)):
        x = tone(rate, hz, .45, amplitude)
        neutral = h.render(1, rate, x, [90, -3, 0, -18, 1, 0])
        active = h.render(1, rate, x, [90, -3, 12, -18, 1, 0])
        if must_reduce:
            require(np.linalg.norm(active[-rate//8:]) < .95*np.linalg.norm(neutral[-rate//8:]), "De-ess does not reduce sustained HF case")
        else:
            close(active, neutral, 2e-7, "De-ess below activation level")
    # Smart EQ's steady sinusoidal branch gains are defined by its band-energy
    # ratios. Long averaging gives an independently predicted complex response;
    # tolerance permits detector ripple, not a claim of frequency-flat matching.
    for hz in (100, 6000):
        x = tone(rate, hz, .7, .2)
        params = [80, 8, 0, 6, 50, 0]
        def response(kind, frequency):
            b, a = analog_biquad(kind, rate, frequency)
            return signal.freqz(b, a, worN=[hz], fs=rate)[1][0]
        hp, low, below = response("hp", 80), response("lp", 400), response("lp", 3000)
        high = 1-below
        gl = 1/(1+8*max(0, abs(low)-.55))
        gh = 1/(1+6*max(0, abs(high)-.45))
        expected_gain = abs(hp*(low*gl+(below-low)+high*gh))
        actual = h.render(6, rate, x, params)
        steady = slice(-frames(rate, .2), None)
        measured_gain = np.linalg.norm(actual[steady])/np.linalg.norm(x[steady])
        require(abs(measured_gain-expected_gain) < .025*max(1, expected_gain), f"Smart EQ steady band law at {hz} Hz: {measured_gain} vs {expected_gain}")
    # A deterministic sustained musical sine is intentionally a candidate:
    # the advisory cannot establish acoustic feedback and applies no cuts.
    x = tone(rate, rate/48, .7, .2)
    params = [1000, 0, 20, 300, 5, 0]
    actual = h.render(4, rate, x, params)
    close(actual, x, 2e-7, "Auto Feedback zero-depth audio")
    require(abs(h.last_candidate-rate/48) < .02*rate/48, "Persistent periodic-tone advisory")
    h.render(4, rate, np.r_[x, np.zeros(frames(rate, .3))], params)
    require(h.last_candidate == 0, "Feedback advisory persists after silence")
    # First transient sample has a closed-form fast/slow envelope anchor.
    x = impulse(rate, .05)
    neutral_params, boosted_params = [35, 0, 0, -90, 150, 0], [35, 0, 0, -90, 150, 9]
    neutral, boosted = h.render(14, rate, x, neutral_params), h.render(14, rate, x, boosted_params)
    y0 = filter_chain(x, analog_biquad("hp", rate, 35))[0]
    fast = (1-np.exp(-1/(rate*.001)))*abs(y0)
    slow = (1-np.exp(-1/(rate*.030)))*abs(y0)
    transient = np.clip((fast-slow)/(fast+1e-7), 0, 1)
    close(boosted[0], neutral[0]*(1+(gain(9)-1)*transient), 2e-6, "Drum Live first transient anchor")
    print(f"nonlinear/adaptive@{rate}: color, clipping, de-ess behavior, steady band law, advisory limits, drum transient PASS")


def dynamics_references(h, rate):
    x = np.repeat(np.array([.02, .5, .05], dtype=np.float32), frames(rate, .25))
    env = exponential_segments(x, rate, 10, 100)
    threshold, gate = gain(-20), gain(-40)
    expected = x*np.minimum(1, (threshold/np.maximum(env, 1e-30))**.75)*np.minimum(1, env/gate)
    actual = h.render(7, rate, x, [-20, 4, 10, 100, -40, 0])
    close(actual, expected, 7e-5, "Dynamics closed-form attack/release + ratio/expansion")
    require(abs(actual[2*frames(rate, .25)-1] - .5*(threshold/.5)**.75) < 5e-5, "Dynamics 4:1 static anchor")
    # Optical model: release is the slower of two explicitly defined detectors.
    fast = exponential_segments(x, rate, 12, 200)
    slow = exponential_segments(x, rate, 12, 600)
    expected = x*np.minimum(1, (threshold/np.maximum(np.maximum(fast, slow), 1e-30))**.75)
    close(h.render(9, rate, x, [-20, 4, 12, 200, 0, 0]), expected, 1.2e-4, "Optical explicit dual-time law")
    # RMS attack has a closed form in squared amplitude. Parallel mixing and
    # makeup are checked independently from the peak-detector cases above.
    x = np.full(frames(rate, .2), .5, dtype=np.float32)
    rms = .5*np.sqrt(1-np.exp(-np.arange(1, len(x)+1)/(rate*.010)))
    compressed = x*np.minimum(1, (threshold/np.maximum(rms, 1e-30))**.75)
    expected = (x+(compressed-x)*.35)*gain(3)
    close(h.render(8, rate, x, [-20, 4, 10, 200, 35, 3]), expected, 8e-5, "Smart Compressor analytic RMS attack/parallel mix")
    # Level-dependent release must lie between its stated 1x and 3x release
    # constants. This verifies a bound, not a second copy of its nonlinear loop.
    split = len(x)
    source = np.r_[x, np.full(frames(rate, .4), .02, dtype=np.float32)]
    actual = h.render(8, rate, source, [-20, 4, 10, 200, 100, 0])[split:]
    initial = .25*(1-np.exp(-split/(rate*.010)))
    ticks = np.arange(1, len(actual)+1)
    lower = .0004+(initial-.0004)*np.exp(-ticks/(rate*.200))
    upper = .0004+(initial-.0004)*np.exp(-ticks/(rate*.600))
    inferred = (threshold*(.02/np.maximum(actual, 1e-30))**(1/.75))**2
    require(np.all(inferred >= lower-2e-4) and np.all(inferred <= upper+2e-4), "Smart Compressor release exceeds defined bounds")
    print(f"dynamics@{rate}: peak/RMS, threshold/ratio, attack/release, optical dual constants, adaptive-release bounds PASS")


def echo_series(length, lag, feedback, damping=None):
    """z^-L / (1 - feedback*D(z)*z^-L), summed as delayed kernels."""
    result = np.zeros(length)
    kernel = np.zeros(length)
    kernel[0] = 1
    for repeat in range(1, (length-1)//lag+1):
        offset = repeat*lag
        result[offset:] += feedback**(repeat-1)*kernel[:length-offset]
        if damping is not None:
            kernel = signal.lfilter([1-damping], [1, -damping], kernel)
    return result


def delay_and_reverb_references(h, rate):
    x = impulse(rate, .35, .2)
    lag = int(np.floor(rate*.017+.5))
    damping = f32(np.exp(-2*np.pi*5000/rate))
    expected = .2*echo_series(len(x), lag, .5, damping)
    actual = h.render(10, rate, x, [100, 17, .5, 5000, 0, 0])
    close(actual, expected, 2e-6, "FX Delay damped geometric impulse series")
    # Float dry/wet cancellation may leave a few ulps of the dry impulse at
    # 100% wet. Timing uses a meaningful echo threshold and separately bounds
    # all pre-arrival samples; the full-vector numerical comparison remains.
    require(np.max(np.abs(actual[:lag])) < 2e-7 and
            np.flatnonzero(abs(actual)>1e-6)[0] == lag, "FX Delay lag convention")
    # Saturation is tested at the write boundary with zero feedback.
    drive = gain(6)
    expected = np.zeros(len(x)); expected[lag] = np.tanh(float(x[0])*drive)/drive
    close(h.render(10, rate, x, [100, 17, 0, 5000, 6, 0]), expected, 2e-7, "FX Delay write saturation")
    # Digital quantization uses half-away-from-zero, including negative ties.
    x = np.resize(np.array([-1.2, -1, -.5, -1/256, 0, 1/256, .5, 1, 1.2], dtype=np.float32), 128)
    quantized = np.sign(x)*np.floor(np.abs(np.clip(x, -1, 1))*128+.5)/128
    close(h.render(11, rate, x, [8, 1, 100, 20, 0, 0]), .65*quantized, 2e-7, "Digital FX quantizer tie/codebook anchors")
    rng = np.random.default_rng(rate + 91)
    x = rng.uniform(-.3, .3, frames(rate, .16)).astype(np.float32)
    sampled = x[::4]
    quantized = np.sign(sampled)*np.floor(np.abs(sampled)*128+.5)/128
    held = np.repeat(quantized, 4)[:len(x)]
    lag = int(np.floor(rate*.020+.5))
    echo = np.zeros(len(x))
    for k in range(1, (len(x)-1)//lag+1):
        echo[k*lag:] += .25**(k-1)*held[:-k*lag]
    close(h.render(11, rate, x, [8, 4, 100, 20, .25, 0]), .65*held+.35*echo, 3e-7, "Digital FX held sequence + feedback echoes")
    # Reverb is composed independently in transfer-function space: four comb
    # impulse series, average, two sparse allpass impulse kernels, predelay.
    x = impulse(rate, 1.0, .1)
    room, decay, predelay = f32(.65), f32(.5), int(np.floor(rate*.011+.5))
    damping = f32(np.exp(-2*np.pi*6000/rate))
    wet = np.zeros(len(x))
    first = None
    for seconds in (.0297, .0371, .0411, .0437):
        lag = max(1, int(np.floor(rate*seconds*room+.5)))
        first = lag if first is None else min(first, lag)
        feedback = f32(10**(-3*lag/(rate*decay)))
        wet += .25*echo_series(len(x), lag, feedback, damping)
    for seconds in (.005, .0017):
        lag = max(1, int(np.floor(rate*seconds+.5)))
        kernel = np.zeros(len(x)); kernel[0] = -.5
        for k in range(1, (len(x)-1)//lag+1):
            kernel[k*lag] = .75*.5**(k-1)
        wet = signal.fftconvolve(wet, kernel)[:len(x)]
    expected = np.r_[np.zeros(predelay), float(x[0])*wet][:len(x)]
    actual = h.render(12, rate, x, [100, decay, 6000, 11, room, 0])
    close(actual, expected, 3e-6, "Reverb96 comb/allpass/predelay independent impulse")
    require(np.max(np.abs(actual[:predelay+first])) < 2e-7 and
            np.flatnonzero(abs(actual)>1e-6)[0] == predelay+first, "Reverb96 first arrival")
    early = np.sum(actual[:frames(rate, .25)]**2)
    late = np.sum(actual[-frames(rate, .25):]**2)
    require(late < early*1e-3, "Reverb96 finite impulse tail fails to decay")
    print(f"FX@{rate}: delay times, damping/feedback series, saturation, quantizer/hold, comb/allpass/predelay and decaying tail PASS")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", help="Compiled lah_builtin_cli executable")
    parser.add_argument("--rate", type=int, action="append", help="Restrict rate; repeat to select several")
    args = parser.parse_args()
    harness = Harness(args.binary)
    for rate in args.rate or (44100, 48000, 96000):
        filter_references(harness, rate)
        nonlinear_and_adaptive(harness, rate)
        dynamics_references(harness, rate)
        delay_and_reverb_references(harness, rate)
        state_invariants(harness, rate)
    print(f"PASS: {harness.cases} native processor render cases (synthetic offline checks only).")


if __name__ == "__main__":
    main()

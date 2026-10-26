"""Independent offline references for mixer filters, dynamics, routing and FX.

Butterworth: scipy.signal.butter/sosfilt. Bell EQ: a continuous-time bell
prototype transformed independently with scipy.signal.bilinear/tf2sos.
Dynamics: closed-form exponentials for each constant-amplitude interval;
FX: explicit delayed impulse trains, not a copy of the engine ring buffers.
No hardware, PA protection, subjective FX quality or acoustic accuracy claim.
"""
import io
import json
import subprocess
import sys
from pathlib import Path

import numpy as np
from scipy import signal


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def fixture(binary, mode, rate, frames=16384):
    result = subprocess.run([str(binary), mode, str(rate), str(frames)],
                            capture_output=True, text=True, check=False)
    require(result.returncode == 0, f"{mode}@{rate}: CLI failed {result.stderr}")
    samples = np.genfromtxt(io.StringIO(result.stdout), delimiter=",", names=True)
    require(len(samples) == frames, f"{mode}@{rate}: incomplete fixture")
    require(all(np.all(np.isfinite(samples[name])) for name in samples.dtype.names),
            f"{mode}@{rate}: non-finite samples")
    return samples, json.loads(result.stderr) if mode in ("hpf", "peq") else None


def close(actual, expected, tolerance, label):
    error = float(np.max(np.abs(actual - expected)))
    require(error < tolerance, f"{label}: maximum absolute error {error:g} >= {tolerance:g}")
    return error


def filters(binary, rate):
    for mode in ("hpf", "peq"):
        samples, coefficients = fixture(binary, mode, rate)
        if mode == "hpf":
            sos = signal.butter(2, 1000, btype="highpass", fs=rate, output="sos")
            b, a = signal.sos2tf(sos)
        else:
            # Analog H(s)=(s²+omega*A/Q*s+omega²)/(s²+omega/(A*Q)*s+omega²).
            # Prewarping + SciPy bilinear transform independently recover the
            # normalized digital coefficients without the host's trig formula.
            q = float(np.float32(0.8))
            amplitude = 10 ** (9 / 40)
            omega = 2 * rate * np.tan(np.pi * 1500 / rate)
            b, a = signal.bilinear([1, omega * amplitude / q, omega ** 2],
                                   [1, omega / (amplitude * q), omega ** 2], fs=rate)
            sos = signal.tf2sos(b, a)
        close(np.array(coefficients["a"]), a, 2e-12, f"{mode} denominator")
        close(np.array(coefficients["b"]), b, 2e-12, f"{mode} numerator")
        expected = signal.sosfilt(sos, samples["input0"])
        error = close(samples["output0"], expected, 2e-8, f"{mode} impulse")
        # Independent anchors, in addition to matching the whole impulse.
        anchor = 1000 if mode == "hpf" else 1500
        _, response = signal.sosfreqz(sos, worN=[anchor], fs=rate)
        target = -3.01029995664 if mode == "hpf" else 9
        require(abs(20 * np.log10(abs(response[0])) - target) < 1e-8, f"{mode} anchor")
        print(f"{mode}@{rate}: coefficients/SOS impulse PASS, max sample delta={error:.3g}")


def closed_form_envelope(levels, rate):
    result = np.empty_like(levels)
    boundaries = np.r_[0, np.flatnonzero(np.diff(levels)) + 1, len(levels)]
    previous = 0.0
    for start, stop in zip(boundaries[:-1], boundaries[1:]):
        amplitude = abs(levels[start])
        tau = 0.010 if amplitude > previous else 0.100
        # Closed-form solution on a constant-amplitude interval, rather than
        # reproducing the host's per-sample recurrence implementation.
        ticks = np.arange(1, stop - start + 1)
        result[start:stop] = amplitude + (previous - amplitude) * np.exp(-ticks / (rate * tau))
        previous = result[stop - 1]
    return result


def dynamics(binary, rate):
    for mode in ("compressor", "gate"):
        samples, _ = fixture(binary, mode, rate, 32768)
        source = samples["input0"]
        envelope = closed_form_envelope(source, rate)
        if mode == "compressor":
            gain = np.minimum(1, (0.1 / np.maximum(envelope, 1e-30)) ** 0.75)
            # Static 4:1 curve at amplitude 0.5 and threshold 0.1.
            plateau = 0.5 * (0.1 / 0.5) ** 0.75
            require(abs(samples["output0"][len(source) // 2 - 1] - plateau) < 4e-5,
                    f"compressor@{rate}: static ratio anchor")
        else:
            gain = np.minimum(1, envelope / 0.1)
            # Long low-level gate plateau: v²/threshold when v<threshold.
            require(abs(samples["output0"][len(source) // 3 - 1] - 0.004) < 4e-5,
                    f"gate@{rate}: low-level static anchor")
        error = close(samples["output0"], source * gain, 5e-5, f"{mode} attack/release")
        print(f"{mode}@{rate}: analytic threshold/ratio/attack/release PASS, max sample delta={error:.3g}")


def routing(binary, rate):
    samples, _ = fixture(binary, "routing", rate)
    left, right = samples["input0"], samples["input1"]
    close(samples["output0"], -0.5 * (0.25 * left - 0.5 * right), 3e-8, "routing polarity/gain")
    expected = np.r_[np.zeros(17), (-0.125 * left + 0.25 * right)[:-17]]
    close(samples["output1"], expected, 3e-8, "routing 17-sample delay")
    expected = np.r_[0.0, (0.1 * left + 0.1 * right)[:-1]]
    close(samples["output31"], expected, 3e-8, "routing output32/1-sample delay")
    print(f"routing@{rate}: signed matrix/gain/polarity/1 and 17-sample delays PASS")


def effects(binary, rate):
    samples, _ = fixture(binary, "fx_delay", rate)
    expected = np.zeros((2, len(samples)))
    lag = int(rate * 0.020)
    amplitude = samples["input0"][0]
    for k in range(1, len(samples) // lag + 1):
        if k * lag < len(samples):
            expected[(k - 1) % 2, k * lag] = amplitude * 0.5 ** (k - 1) * 0.5
    close(samples["output0"], expected[0], 2e-8, "ping-pong left train")
    close(samples["output1"], expected[1], 2e-8, "ping-pong right train")
    print(f"fx_delay@{rate}: independent stereo echo times/feedback series PASS")
    samples, _ = fixture(binary, "fx_reverb", rate)
    expected.fill(0)
    times = (0.0297, 0.0371, 0.0411, 0.0437)
    for channel in range(2):
        for time in times:
            lag = int(np.ceil(rate * (time + (0.0017 if channel else 0)))) + 1
            for k in range(1, len(samples) // lag + 1):
                if k * lag < len(samples):
                    expected[channel, k * lag] += amplitude * 0.2 * 0.72 ** (k - 1)
    close(samples["output0"], expected[0], 3e-8, "comb left impulse series")
    close(samples["output1"], expected[1], 3e-8, "comb right impulse series")
    print(f"fx_reverb@{rate}: four-comb times/feedback sum PASS (not quality validation)")


def main():
    binary = Path(sys.argv[1]).resolve() if len(sys.argv) == 2 else None
    if binary is None or not binary.is_file():
        raise SystemExit("Supply compiled lah_dsp_cli executable path")
    for rate in (44100, 48000, 96000):
        filters(binary, rate)
        dynamics(binary, rate)
        routing(binary, rate)
        effects(binary, rate)
    print("PASS: 21 independent DSP fixture cases at 3 sample rates (synthetic only).")


if __name__ == "__main__":
    main()

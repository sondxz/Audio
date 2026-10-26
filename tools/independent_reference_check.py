"""Compare offline H1/phase/coherence/RTA against independent SciPy Welch/CSD.

Synthetic developer QA only; no SPL, hardware or acoustic accuracy claim.
Usage: python tools/independent_reference_check.py build-core/lah_tf_cli.exe
"""
import csv
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from scipy import signal


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def check_case(exe, name, fs, delay, gain, filtered=False, noise_rms=0.0):
    rng = np.random.default_rng(20261022)
    reference = rng.normal(0, 0.18, size=32768).astype(np.float32)
    source = signal.lfilter([0.6, 0.20, -0.08], [1.0], reference) if filtered else reference
    measured = np.zeros_like(reference)
    if delay > 0:
        measured[delay:] = gain * source[:-delay]
    elif delay < 0:
        measured[:delay] = gain * source[-delay:]
    else:
        measured[:] = gain * source
    measured += rng.normal(0, noise_rms, size=reference.size).astype(np.float32)
    with tempfile.TemporaryDirectory(prefix="lah-bench-") as td:
        folder = Path(td)
        # Float32 inputs match the C++ capture domain, including the added noise.
        np.savetxt(folder / "ref.csv", reference, fmt="%.10f")
        np.savetxt(folder / "mic.csv", measured, fmt="%.10f")
        process = subprocess.run(
            [str(exe), str(fs), str(folder / "ref.csv"), str(folder / "mic.csv"), str(folder / "tf.csv")],
            text=True, capture_output=True, check=False
        )
        require(process.returncode == 0, f"{name}: lah_tf_cli failed: {process.stderr}")
        with (folder / "tf.csv").open(newline="") as fh:
            rows = list(csv.DictReader(fh))
    require(len(rows) == 4096, f"{name}: incomplete spectrum")
    require(int(rows[0]["delay_valid"]) == 1, f"{name}: delay rejected")
    estimated_ms = float(rows[0]["delay_ms"])
    require(abs(estimated_ms - delay * 1000 / fs) < 0.0001,
            f"{name}: incorrect known transport delay {estimated_ms}")

    # Pair exactly the same delay-aligned samples; independent SciPy transforms,
    # spectral normalization and averaging, rather than the host FFT routine.
    x = reference[:-delay] if delay > 0 else reference[-delay:] if delay < 0 else reference
    y = measured[delay:] if delay > 0 else measured[:delay] if delay < 0 else measured
    params = dict(fs=fs, window=np.hanning(8192), nperseg=8192, noverlap=4096,
                  detrend=False, scaling="density", return_onesided=True)
    frequencies, pxx = signal.welch(x.astype(np.float64), **params)
    _, pyy = signal.welch(y.astype(np.float64), **params)
    _, pyx = signal.csd(x.astype(np.float64), y.astype(np.float64), **params)
    predicted = pyx / np.maximum(pxx, 1e-30)
    coherence = np.abs(pyx) ** 2 / np.maximum(pxx * pyy, 1e-30)
    # Density [FS^2/Hz] times bin width gives window-power-normalized bin RMS.
    rta = 10 * np.log10(np.maximum(pyy * fs / 8192, 1e-16))
    magnitude_err, phase_err, coh_err, rta_err = [], [], [], []
    selected_coherence = []
    for row in rows:
        k = round(float(row["frequency_hz"]) * 8192 / fs)
        if not (80 <= frequencies[k] <= min(10000, fs / 2) and int(row["bin_reliable"])):
            continue
        magnitude_err.append(abs(float(row["tf_magnitude_db"]) - 20 * np.log10(abs(predicted[k]))))
        # Compare phase circularly so equivalent -180/+180 values match.
        phase_delta = float(row["tf_phase_deg_compensated"]) - np.angle(predicted[k], deg=True)
        phase_err.append(abs((phase_delta + 180) % 360 - 180))
        coh_err.append(abs(float(row["coherence"]) - coherence[k]))
        rta_err.append(abs(float(row["rta_dbfs_per_bin_rms"]) - rta[k]))
        selected_coherence.append(coherence[k])
    require(len(magnitude_err) > 300, f"{name}: too few independent-validation bins")
    if noise_rms:
        require(min(selected_coherence) < 0.98, f"{name}: case did not exercise non-unit coherence")
    print(f"{name}: {len(magnitude_err)} bins, max delta magnitude={max(magnitude_err):.6f} dB, "
          f"phase={max(phase_err):.6f} deg, coherence={max(coh_err):.7f}, "
          f"RTA={max(rta_err):.6f} dB; delay={estimated_ms:.4f} ms")
    require(max(magnitude_err) < 0.001, f"{name}: H1 magnitude mismatch")
    require(max(phase_err) < 0.001, f"{name}: H1 phase mismatch")
    require(max(coh_err) < 0.00001, f"{name}: coherence mismatch")
    require(max(rta_err) < 0.001, f"{name}: RTA normalization mismatch")


def main():
    exe = Path(sys.argv[1]).resolve() if len(sys.argv) == 2 else None
    if exe is None or not exe.is_file():
        raise SystemExit("Supply compiled lah_tf_cli executable path")
    check_case(exe, "gain and positive delay", 48000, 1024, 0.5)
    check_case(exe, "inverted gain and negative delay", 44100, -120, -0.75)
    check_case(exe, "frequency response and independent noise", 48000, 137, 0.8,
               filtered=True, noise_rms=0.025)
    print("PASS: 3 independent SciPy H1/phase/coherence/RTA cases (synthetic data only).")


if __name__ == "__main__":
    main()

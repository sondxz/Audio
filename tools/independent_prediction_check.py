"""Independent NumPy Cartesian phasor/direct-path fixtures; no JUCE or hardware.

Usage: python independent_prediction_check.py path/to/lah_prediction_cli.exe
Random fixtures are reproducible and sent as one batch. Values are predictions,
never calibrated SPL or microphone coherence.
"""
import subprocess
import sys
import numpy as np


def main():
    rng = np.random.default_rng(10724)
    inputs, expected = [], []
    for case in range(90):
        count = int(rng.integers(2, 17))
        frequency = np.exp(rng.uniform(np.log(.1), np.log(20000), count))
        if case % 3 != 0:
            frequency[:] = frequency[0]
        phase = rng.uniform(-3600, 3600, count)
        delay = rng.uniform(-250, 250, count)
        level = rng.uniform(-60, 12, count)
        polarity = rng.integers(0, 2, count)
        enabled = rng.integers(0, 2, count)
        # Exact cancellation fixture; phase at a null is unavailable.
        if case == 0:
            count = 2
            frequency, phase, delay, level = np.array([100., 100.]), np.array([0., 180.]), np.zeros(2), np.zeros(2)
            polarity, enabled = np.zeros(2, dtype=int), np.ones(2, dtype=int)
        time = float(rng.uniform(-.2, .2))
        inputs.append(f"P {count} {time:.17g}")
        for row in zip(frequency, phase, delay, level, polarity, enabled):
            inputs.append(" ".join(format(float(x), '.17g') for x in row))
        gain = np.power(10., level / 20) * enabled * np.where(polarity, -1., 1.)
        offset = np.deg2rad(phase) - 2 * np.pi * frequency * delay / 1000
        angle = offset + 2 * np.pi * frequency * time
        # Cartesian sin/cos sums rather than the C++ wrapped std::polar path.
        real, imag = np.dot(gain, np.cos(angle)), np.dot(gain, np.sin(angle))
        bound = float(np.sum(np.abs(gain)))
        active = frequency[enabled != 0]
        common = bool(active.size and np.all(active == active[0]))
        peak = float(np.hypot(np.dot(gain, np.cos(offset)), np.dot(gain, np.sin(offset))))
        expected.append([real, imag, bound, float(common), peak / np.sqrt(2) if common else 0., float(common and peak > bound * 1e-12)])
    for case in range(90):
        count = int(rng.integers(1, 17))
        positions = rng.uniform(-6, 6, (count, 3))
        point = rng.uniform(-20, 20, 3)
        speed, frequency = float(rng.uniform(315, 365)), float(rng.uniform(20, 300))
        level, delay = rng.uniform(-40, 12, count), rng.uniform(0, 150, count)
        polarity, enabled = rng.integers(0, 2, count), rng.integers(0, 2, count)
        if case == 0:
            point = positions[0].copy()  # defined 0.25 m singularity floor
        inputs.append("S " + str(count) + " " + " ".join(format(x, '.17g') for x in [speed, frequency, *point]))
        for index in range(count):
            inputs.append(" ".join(format(float(x), '.17g') for x in [*positions[index], level[index], delay[index], polarity[index], enabled[index]]))
        distance = np.maximum(.25, np.linalg.norm(positions - point, axis=1))
        gain = np.power(10., level / 20) / distance * enabled * np.where(polarity, -1., 1.)
        angle = -2 * np.pi * frequency * (distance / speed + delay / 1000)
        real, imag = np.dot(gain, np.cos(angle)), np.dot(gain, np.sin(angle))
        relative = float(np.clip(20 * np.log10(max(1e-8, np.hypot(real, imag))), -100, 60))
        expected.append([real, imag, relative])
    run = subprocess.run([sys.argv[1]], input="\n".join(inputs) + "\n", text=True, capture_output=True, check=True)
    rows = run.stdout.splitlines()
    if len(rows) != len(expected):
        raise AssertionError(f"Expected {len(expected)} rows, got {len(rows)}: {run.stderr}")
    largest = 0.
    for index, (line, wanted) in enumerate(zip(rows, expected)):
        actual = np.fromstring(line, sep=',')
        np.testing.assert_allclose(actual, wanted, rtol=1e-8, atol=2e-9, err_msg=f"prediction fixture {index}")
        largest = max(largest, float(np.max(np.abs(actual - wanted))))
    print(f"PASS independent prediction: 90 phase + 90 sub cases; max absolute difference {largest:.3g}")


if __name__ == '__main__':
    main()

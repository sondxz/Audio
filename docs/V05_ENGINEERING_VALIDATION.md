# LiveAudioHost v0.5.0 — Measurement / SPL / FIR / Reliability validation

## Status and test scope

**Engineering lab source / experimental. NOT show ready; do not use as the sole PA/monitor safety system.**

Verification on 2026-10-22 in containerized Linux:

- GCC C++17: `ctest` **6/6** core suites PASS.
- Clang C++17: `ctest` **6/6** core suites PASS (rebuild after final patch recommended on Windows).
- Clang ASan/UBSan: `ctest` **6/6** core suites PASS.
- Independent FIR reference using `scipy.signal.firwin` blackman, sample rates 44,100 / 48,000 / 96,000, lengths 129, 513, 1025, 2049, LP/HP with 3,000 Hz cutoff: all cases PASS, largest coefficient absolute difference in reference run approximately 3.2e-14.
- Math gates: A and C weighting tested with 125 Hz, 1 kHz and 8 kHz sine; refusal on missing reference, clipping, sample-rate mismatch, and non-calibration signals.
- Polar response: data matrix validation + 10 dB rear directivity and angular interpolation cases passed.
- **No JUCE GUI compile on Windows; no Windows or Midas M32R ASIO hardware verification; no measured round-trip latency; no IEC 61672 compliance tests; no actual loudspeaker FIR frequency sweeps.**

## SPL measurement design

`CalibratedSplCore.h` uses spectral energy integration over overlapped 8192-pt Hann windows. It normalizes FFT RMS bin power to the Hann squared-energy. A/C/Z weighting functions are analytical approximations of standard weighting curves. These curves were tested against ideal single-tone theoretical values, **not certified as an IEC class 1/2 analyzer**. Values are **block averages over the last 32768 samples**, not Fast / Slow / Leq (and not occupational exposure dosimetry).

Required method for any meaningful acoustic SPL:

1. Use a known *acoustic calibrator* at a stated 94 dB or 114 dB, 1 kHz and verified measurement mic, corresponding adaptor/coupler.
2. Ensure current interface input is the mic channel, stable analog gain and 48 kHz or 44.1 kHz as supported.
3. Select SPL/Calibration, select the MIC INPUT, select physical calibrator value, then press CALIBRATE NOW during the real reference tone. The algorithm rejects calibrator captures with near digital clip, too weak signal or lacking a concentrated 1-kHz component.
4. The software stores only a **session calibration offset** from this capture; it intentionally does not claim traceability or auto-read serial/type/environment. Never substitute playback from a loudspeaker or a digital tone as a real calibrator.
5. After changing input gain, cable, microphone, mic pad, preamp, ADC, or rate, repeat calibration manually. Device generation/rate/input-changes invalidate the offset automatically when detected. Changes to analog gain CANNOT be automatically detected.
6. Compare SPL A/C/Z with a calibrated independent reference meter using multiple acoustic levels/frequencies in the actual measurement chain. Record residual errors and room noise/uncertainty.

`dBFS` ≠ `dB SPL`: app SPL numbers will not show until valid calibration. This is **NOT a certified IEC 61672 meter**. Does not have Fast/Slow response, 1/3 octave IEC filters, defined dynamic range, microphone incidence/calibration correction, verified overload margin, uncertainty budget or independent acoustic validation.

## FIR filter design and safety

FIR Studio is **OFFLINE ONLY**, not applied to the live DSP signal chain. Designs low-pass / high-pass odd-length Blackman-windowed sinc taps (129–4097), which are exactly symmetric. It reports theoretical group delay `(N-1)/(2*sample_rate)`. Windowed-sinc cut is the nominal half-magnitude transition neighborhood, not guaranteed pass-band edge. 

`lah_fir_cli` allows independent reference checking against SciPy with `python tools/independent_fir_check.py ./build-core/lah_fir_cli` after building core. Export is CSV for analysis, **not guaranteed compatible** with a particular speaker processor. Real deployment needs impulse verification, passband/stopband measurements, peak headroom margin, phase reference, measured channel latency, and correct sample rate. Long FIR filters are CPU/memory-heavy and can add many ms latency: at 48 kHz, a 2049-tap symmetric filter has theoretical group delay **21.333 ms**; it is NOT installed as a live effect. True FIR system correction remains unimplemented.

## Measurement Transfer Function + snapshots

TF/RTA core is inherited from 0.3/0.4: H1, magnitude, phase, per-bin coherence mask, GCC-PHAT delay. v0.5 adds session capture **A** and **B** overlays and CSV columns. Snapshots do not make weakly coherent data valid; phase overlay only appears when reference and delay estimate are valid. FFT/delay analysis still runs on the JUCE message thread; move to a dedicated worker before relying on long-running stage measurement.

## Import of measured polar speaker data

Sub Array can import 4-column CSV with header:

```
frequency_hz,azimuth_deg,magnitude_db,phase_deg_unwrapped
80,0,0,0
100,0,0,-30
80,90,-3,0
100,90,-3,-30
80,180,-10,0
100,180,-10,-30
80,270,-3,0
100,270,-3,-30
```

The imported angles must be integers in [0,359], have at least **4 angular slices**, and each slice must contain the **same increasing frequency grid** (>=2 points). All phase inputs must be **UNWRAPPED** and captured from the **same acoustic/time reference and same speaker measurement origin**; raw unrelated phase plots are NOT valid to combine. All angular slices apply relative magnitude dB and phase to the same individual source geometry. Linear interpolation is performed in log-frequency for amplitude/phase and angle for the two neighboring azimuths. No extrapolation in frequency: out-of-range frequencies suppress the plot. **The free-field propagation prediction remains a simplification; it does not handle reflections/ground/directivity elevation or licensed manufacturer GLL/EASE data formats.** It never gives calibrated absolute SPL.

When a file is imported, the top-level info shows imported profile status; F/R summary remains labeled `OMNI` to prevent confusing it with the imported heatmap. This import is for measured loudspeaker **frequency/directional prediction**, not a full acoustic simulator.

## Realtime telemetry

JUCE/driver diagnostics retained. Added measured time spent inside callback and count of frames for which callback execution time exceeds `bufferSize/sampleRate`. This is a **soft deadline indicator**, not a hardware ASIO XRun/transport measurement. It does not reveal input-to-output latency. Full show reliability still blocked by VST3 in-process crashes, no PDC, no verified true-peak speaker limiter, no hardware burn-in/driver disconnect matrix or safety certification.

## Windows acceptance test / before any live deployment

1. Build actual JUCE/Win11/MSVC binary, run all tests `ctest`.
2. Run M32 USB/ASIO loopback using separately recorded reference/mic synchronized from the same clock, document sample rate, buffer, patch, interface gain, sample format.
3. Compare H1 phase/coherence/delay with a trusted analyzer on the **same recorded signal**, while matching FFT/window/averaging/smoothing/delay corrections.
4. Calibrate mic acoustically with a physical calibrator, compare A/C/Z SPL against a reference meter using real test fixtures. Disclose tolerance/uncertainty instead of claiming a pass without measurements.
5. Independently characterize actual FIR impulse and transfer response before enabling any live FIR implementation.
6. Record ASIO callback deadline, driver-reported xrun, CPU, true round-trip delay (loopback electrical measurement), 8–24h long-run stability, device unplug/reconnect, plugin crash behavior, recovery/bypass path.
7. Keep an independent hardware mute/bypass and do not put an unverified build in the primary sound reinforcement path.

A successful offline test DOES NOT certify these hardware criteria.

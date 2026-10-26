# Release verification snapshot — LiveAudioHost 0.3.0

**Date:** 2026-10-22. **Status:** OFFLINE CORE VERIFIED / WINDOWS GUI NOT VERIFIED / FIELD USE NOT APPROVED.

## Code changes actually made in 0.3.0

- Linear GCC-PHAT full-capture search to avoid modulo 8192 ambiguity in delay detection.
- Explicit rejection of narrowband/tonal inputs for delay-finder and use of sidelobe confidence.
- Delay-compensated dual-channel H1 (S_yx/S_xx), power spectra and phase; coherence computed from averaged cross-spectrum.
- Window-power-normalized RTA RMS-bin dBFS, not SPL; separate reference floor gate; per-bin validity + coherence reliability masks.
- GUI measurement graphs mask invalid bins, phase withheld when delay is not validated. CSV labels distinguish levels, validity and compensation state.
- New offline `lah_tf_cli` and SciPy independent spectrum comparison, golden-vector regression tests; preserves original DSP/mixer/GUI parts from v0.2.0.

## Actual tests performed in execution container (Linux)

- Compiler GNU g++ 14.2, CMake 3.31.6, Ninja.
- Core-only CMake compile: success.
- `ctest`: **3/3 passed** (`lah_core`, `lah_extended`, `lah_validation`).
- ASan + UBSan debug build: **3/3 passed**.
- SciPy independent FFT comparison (NumPy + SciPy installed): **PASS, 1693 frequency bins**, max |magnitude delta| < 0.001 dB, max |phase delta| < 0.001 degree, max |coherence delta| < 0.00001. These figures are from **synthetic digital signals only**; reported zero-to-six decimals are a result of rounding output, not a claim of physical accuracy.
- Delay test cases: 0, +120, +6000, -120 samples @48k; phase reversal @96k; 1024-sample synthetic latency matched independent SciPy check.
- Negative cases: absent/weak ref, silence, narrowband tone ambiguity, NaN input; Parseval energy check; DSP mixer pass-through and routing regression.

## Explicitly not tested

- Windows 11 MSVC/JUCE GUI build, ASIO/WASAPI, driver enumeration, plugin compatibility, any 32ch M32R LIVE hardware use.
- Round-trip latency, real latency jitter, CPU deadline/XRUN statistics, multi-hour soak, calibrated measurement microphones, external acoustic standards.
- Comparison with commercial Smaart/VOS/RITA raw files from real venue, microphone measurements, sound-pressure levels, PA/system tuning in venue.
- No guarantee of 100% bug-free software or show safety.

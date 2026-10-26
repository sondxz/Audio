# Changelog

## v0.5.0 — 2026-10-22
- Graphite/teal rack theme source; new SPL session calibration and A/C/Z spectral estimator (explicitly not IEC-certified).
- FIR offline low/high pass designer + tap CSV export; independent SciPy verification; no live FIR applied.
- Measured magnitude / phase / polar CSV acoustic profile importer with stricter range validation.
- Measurement A/B trace snapshot overlays and CSV metadata; callback duration + over-deadline diagnostics.
- 6 GCC/Clang/ASan offline core tests pass; GUI/ASIO hardware not tested.

# Changelog

## 0.3.0 — 2026-10-22 (Validation-focused)

- Robustness: linear full-record GCC-PHAT with broadband and sidelobe confidence gates.
- Accuracy: delay-compensated H1 FFT, coherence, reference-floor gates, RTA RMS/bin Hann power correction.
- Data integrity: per-bin validity/reliability and no misleading phase when delay unresolved.
- Tooling: offline TF CSV CLI, independent SciPy comparison, synthetic regression (including long delay).
- Documentation: production gates, hardware validation protocol, written verified-vs-not-verified report.
- Status: 3/3 offline Linux tests and 3/3 sanitizer tests passed; Windows JUCE/ASIO/VST3 and physical M32 not tested.

## v0.2.0 — 2026-10-22 (experimental, source code only)
- Added 32x32 atomically controlled routing + per-channel gain, HPF, PEQ, gate and compressor, speaker output gain/delay/polarity/sample ceiling.
- Added stereo FX send/return, simple feedback delay and reverb.
- Added experimental VST3 single stereo output insert, VST3 scan thread, load/unload/bypass; Windows build not verified.
- Added 8192-bin-size FFT (8192-point), RTA, H1 spectral TF magnitude/phase/coherence, GCC-PHAT delay estimate and CSV export.
- Added free-field complex-pressure Sub Array solver and 2D/3D perspective visualization with sub-array presets.
- Added routing/scene XML save+recall while muted.
- Expanded core C++ unit tests, ASan/UBSan Linux test run.
- Documented latency calculation, operational limits, tuning and safety workflow.

## v0.1.0
- JUCE AudioDeviceManager / pass-through, input/output meters, panic mute and diagnostics (source only).

## v0.4.0 - 2026-10-22
- Added native Engineering Calculator panel, six calculator modes, validated mathematical models including ISO 9613-1 air loss.
- Added native point-source/front-fill nominal coverage planner with AUTO / MANUAL / ARC modes; no claimed SPL prediction.
- Sub array sound-speed parameter now changes with temperature.
- New pure C++ engineering test suite, GCC + Clang Linux pass. Windows JUCE/ASIO not tested.

## v0.7.0 (2026-10-22) — Windows Acceptance / QA (source-only)

- Integrated callback deadline safety latch and explicit operator re-arm while muted.
- New GUI QA session logging to CSV, with separate native device XRun and JUCE reported/estimated count.
- Added synthetic end-to-end tests for TF gain/phase/delay, relative IR, SPL calibration tone, FIR convolution, panic/routing.
- Fixed Release test configuration suppressing `assert` via `NDEBUG`.
- Introduced stdlib Python QA CSV validator (24-hour duration/reconnect/XRun/safety gate).
- Added reproducible local Windows GUI build-and-check script, hardware inventory and on-site acceptance instructions.
- Added experimental relative impulse calculation for coherent, delay-compensated H1 results (DSP only).
- NOT verified on Windows/MSVC or Midas M32R Live; not show-ready.

# LiveAudioHost v0.6.0 — evidence-based release validation
**Date:** 2026-10-22

## Classification
**ENGINEERING QA PREVIEW / NOT SHOW READY.** Source code for C++17/JUCE desktop. There is NO verified Windows executable. Mathematical test success does NOT validate SPL accuracy in situ, plugin stability, PA safety or ASIO timing.

## What changed, verified scope

1. Measurement H1/FFT/GCC-PHAT moved out of JUCE `Timer` to `measurement::MeasurementWorker` (pure-C++ background thread). Its capture ring is atomic; background result publication is mutex-protected; audio callback never locks a mutex for this.
2. Adds `Capture::readNext()` for consecutive non-overlapping blocks and explicit capture-gap detection. Calibration session now calculates spectral A/C/Z on the SPL worker, maintains power-domain **indicative** Leq and exponential block Fast/Slow estimates. These are NOT IEC time-weighting networks/certification. Calibration is invalidated on source/device change; analog preamp gain changes remain undetectable.
3. Adds **experimental** stereo short FIR insert (129/257 taps only) for OUT 1/2 with device callback detachment during installation, explicit mute, manual re-enable, and output clipping guard. `StereoRealtimeFIR::process()` has no allocations or locks. The FIR group delay `(N-1)/2` samples is **not compensated** against other outputs/VST3. FIR is bypassed automatically when the audio callback is restarted. Do not use on main PA.
4. Mixer gains input mute (ramped) and L/R balance on matrix OUT 1/2; persists these to scene. No multibus AFL/PFL architecture.
5. Prevents old comb/delay tails returning after PANIC via epoch-tagged delay-line samples (constant-time invalidation); output-delay is additionally guarded against stale material.
6. Adds an offline 32×32 dense matrix + 257-tap FIR stress benchmark. It is **not a driver ASIO stress test**.

## Executed tests (actual containerized Linux, not Windows hardware)

| Compiler/run | Result | What it validates |
|---|---|---|
| GCC/Release | 11/11 CTest suites | pass through; mixer; DSP; matrix; FFT/TF; simulator; engineering formulas; SPL+FIR offline; polar import; FIR stream; stats; worker; input mute/pan; PANIC tail |
| Clang/Release | 11/11 CTest suites | Same C++ modules compiled with independent compiler |
| Clang/Debug + ASan/UBSan | 11/11 CTest suites | Memory/undefined behavior on tested call paths only |
| SciPy TF independent reference | PASS (synthetic) | 1693 valid bins; per-source output recorded in logs |
| SciPy FIR independent reference | PASS (synthetic) | Needs NumPy/SciPy installed; independent test in `tools` |
| Windows 11 JUCE GUI/MSVC/ASIO | **NOT RUN** | JUCE/MSVC and real Windows audio stack unavailable in this environment |
| M32R Live, SPL meter comparison | **NOT RUN** | Real hardware/calibrator and reference measurement unavailable |
| Plugin rack across 3rd party VST3 | **NOT RUN** | Plugin compatibility and crash isolation not implemented |

**Measured synthetic offline performance (this Linux container only):** dense 32×32 routing, output FIR 257 taps ×2, buffer 128, rate 48 kHz: p50 224 µs, p99 985 µs, max 2626 µs across 1000 runs; buffer period 2667 µs. These numbers vary across machines, and the observed maximum being close to the entire period is a warning, not proof of adequate real-time headroom. They exclude driver, OS, interface, GUI, other plugins and scheduling. See `docs/BENCHMARK_v0.6.txt`.

## Not completed
- **Audio/FOH/BOH:** ASIO performance validation, multi bus/group/matrix mixes, stereo/linking, true dynamics sidechain, plugin crash isolation, multi-VST3 chain, compensation for plugin/FIR latency, true-peak safety limiter, show-safe recovery.
- **Measurement:** multi-mic alignment, pulse/MLS sweep-based impulse response/ETC, spectrograph, multi-clock compensation/drift and standardized comparison with Smaart/Open Sound Meter.
- **Acoustics:** 3D venue reflective model, measured speaker 3D directivity, ground and atmosphere, calibrated absolute SPL prediction, manufacturers' formats/GLL validation.
- **SPL:** complete IEC 61672 design, frequency-response/dynamic-range proof, validated Fast/Slow implementations (block estimates are NOT IEC-certified), Class 1/2 calibrator/mic/reference sound-level-meter comparisons, uncertainty accounting and regulatory compliance.
- **Operational:** multihour Windows and real M32 ASIO soak test, disconnect/reconnect, varying buffers and rates, output cross-channel FIR timing verification; signed installer; user-friendly report export and rollback.

## Release gate (HARD STOP)
Do **not** route the live main PA or wedges through this unvalidated build. Only use with muted/simulated source and independent hardware bypass. The next acceptance gate is Windows JUCE compilation plus 48 kHz ASIO loopback at 64/128/256 buffers, independent latency measurement and microphone acoustic calibration comparison. No "perfect/no errors" guarantee is possible for real-time software.

### Independent mathematical cross-check after final core edit
`tools/independent_fir_check.py` with SciPy passes (44.1/48/96 kHz, multiple FIR sizes, LP/HP); `tools/independent_reference_check.py` passes with synthetic TF (1693 bins). The finite precision deltas are **comparisons of computer algorithms**, not acoustic measurement tolerances.

# Release validation v0.5 — 2026-10-22

Experimental native JUCE app source. No Win11 EXE produced or tested; Linux only for pure C++ core.

| Area | Scope exercised | Result |
|---|---|---|
| Core mixer, audio logic, routing | GCC & Clang C++17 offline tests | PASS (in 6-suite run) |
| H1 TF, RTA, GCC-PHAT delay, coherence, simulated sub | GCC & Clang C++17 offline tests | PASS (in 6-suite run) |
| Engineering calculations and frontfill geometry | GCC & Clang C++17 offline tests | PASS (in 6-suite run) |
| Acoustic SPL core | 1kHz tone reference, 125 Hz A, 8kHz A/C, reject clipping/NaN/noise/mismatch | PASS synthetic |
| FIR core | LP/HP symmetric 129–2049 taps at 44.1/48/96k; offline impulse response, pass/stop anchors | PASS synthetic |
| FIR vs independent SciPy | 24 combinations, max abs tap diff ~3.2e-14 | PASS |
| Polar dataset | Angular interpolation, gain -10dB rear, grid, malformed/out of range | PASS synthetic |
| AddressSanitizer/UndefinedBehaviorSanitizer | Clang all 6 suites | PASS |
| JUCE GUI compilation Windows | Requires MSVC/JUCE/Windows | NOT TESTED |
| ASIO / M32 device | Driver and hardware required | NOT TESTED |
| Acoustic calibrated sound meter comparison | Lab/reference meter required | NOT TESTED |
| Real FIR application + speaker measurements | No live FIR in application | NOT IMPLEMENTED |
| Long-run reliability, VST3 fault isolation | Physical audio+plugin harness required | NOT VALIDATED |

**Conclusion:** Increased offline validation coverage but not certified accurate as Smaart/RITA/VOS and not show-ready. Do not take a calculation test to imply calibration/PA safety.

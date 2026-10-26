# Release validation report — LiveAudioHost v0.4.0

Date: 2026-10-22. This report records **what was actually tested in the current environment**, not assumptions.

| Test environment | Test suites | Observed result |
|---|---|---|
| GCC / Linux via CMake + Ninja, `LAH_BUILD_GUI=OFF` | `lah_core`, `lah_extended`, `lah_validation`, `lah_engineering` | 4 / 4 PASS |
| Clang / Linux via CMake + Ninja, `LAH_BUILD_GUI=OFF` | Same 4 suites | 4 / 4 PASS |
| GCC / Linux ASan + UBSan (engineering suite only) | `lah_engineering_tests` | PASS |
| Windows MSVC + JUCE GUI | Not available in runtime | NOT TESTED |
| Windows ASIO / Midas M32R live loopback | Device unavailable | NOT TESTED |
| 2-channel TF / acoustic comparison with Smaart or Open Sound Meter | No calibrated physical measurement | NOT TESTED |
| Multihour operation / latency / VST3 safety | Not performed | NOT TESTED |

## New deterministic engineering tests (pure C++ core)

- Electrical 500 W / 8 ohms -> 63.24555 Vrms and 7.90569 Arms.
- Inverse dBu conversion roundtrip 70 Vrms.
- RMS nominal limiter threshold at 500 W / 8 ohms, 32 dB amp voltage gain: ~+6.23 dBu. **This is a mathematical number, not a certified safe setting.**
- Ideal free field 98 dB 1W/1m, 500 W, 20m -> ~98.97 dB SPL prediction for one source, without additional environmental corrections.
- Coherent vs incoherent theoretical N=4 gains +12.04 vs +6.02 dB.
- 20m one-way, 2.5mm2 copper at 0.0175 ohm*mm2/m -> 0.28 ohm roundtrip, source impedance 8/2000.
- 20°C speed of sound = 343.2 m/s; time-delay shift for temperature changes.
- ISO 9613-1 air absorption against published ~4.66 dB/km (1 kHz 20C 50% RH), ~105.29 dB/km (8 kHz 20C 50%), and independent NPL cells for 50.12 Hz (-20C 10% RH), 10kHz (20C 15% RH).
- Straight front-fill geometry with contiguous target coverage, one-speaker case, intentional holes, physical arc and invalid input cases.

## Boundaries of these tests

Passing pure-math tests DOES NOT prove JUCE GUI build, show operation, actual calibrated SPL, room propagation, directivity, actual loudspeaker limits, ASIO latency, or microsecond real-time scheduling. The screenshot data is a **functional reference**, not calibration measurements. All calculations are explicitly labeled as either idealized estimates or geometric coverage.

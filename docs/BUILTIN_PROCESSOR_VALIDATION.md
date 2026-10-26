# Native processor definitions and independent validation

**2026-10-23 update — module 3/4 override:** ID 3 now displays **AI-FB LAB** and adds a neural model trained on synthetic signals plus an explicitly armed automatic cut bank. ID 4 now displays **AUTO FB PRO** and adds spectral/growth/persistence detection plus explicitly armed automatic cuts. Neither arms itself. DISARM holds existing filters; Clear removes them. See [current implementation/results](AI_FEEDBACK_IMPLEMENTATION_v0.7.0.md) and [model card](FEEDBACK_MODEL_CARD.md), including ambiguous growing musical tones and unverified real PA discrimination.

The manual-chain definitions and 402-render execution record below are a **historical unarmed baseline**. The current build also passes those 402 renders while automatic cuts remain disarmed; they do not validate the added controller/model. New evidence is 30 independent feedback render cases, 25 model-feature clips/495 frames, and the current integration suites. In particular, historical statements below that module 3 has no AI or module 4 never applies automatic cuts no longer describe the armed 2026-10-23 implementation.

**Independent CLI execution: PASS, 402 rendered vectors on 2026-10-22.**
The compiled Windows Release bridge passed the checks below at 44.1, 48 and
96 kHz. Integrated engine/GUI acceptance and native performance results remain
separate checks; the CLI result does not imply that they passed.

These 15 modules are original native DSP functions in `src/dsp/BuiltinRackCore.h`.
The display names are workflow labels. They do not establish compatibility with
another product, a learned model, a particular analog circuit, a professional
sound quality standard, or protection of loudspeakers/hearing.

The rack has 32 independent channel strips with four serial insert slots per
channel. All slots start disabled. A disabled slot must preserve finite input
samples exactly, including signed zero and large finite float values. Active
slots guard internal/output samples to +/-16; NaN and infinity become zero.
Enable/type changes use a 64-sample transition; disable is exact bypass once
that transition ends. Initial configured rendering starts without a fade.
The independent bridge configures channel 1 / slot 1 only;
channel 2 remains bypassed to detect accidental cross-channel state sharing.

## Definition boundaries

- **VOCAL CLEAN** is conventional causal filtering/level processing. A gate or
  expander suppresses low-level intervals; it does not separate a voice from
  overlapping noise and must not be presented as learned denoising.
- **VOCAL COLOR** is an explicit filter/nonlinear coloration model. A pleasing
  setting or a resemblance to a hardware preamp is not established by numerical
  transfer-function tests.
- **AI-FB** maps to **Feedback Guard**, a manual narrow bell cut with high-pass
  and sample clip guard; it has no feedback detector. **AUTO FB PRO** maps to
  **Auto Feedback (advisory)**, which uses a deterministic periodic-tone
  heuristic and manual bell cut; it never automatically applies a cut. Neither
  module has a trained AI model. A sustained musical tone can trigger the
  advisory. No sound-system stability, false-positive rate, or autonomous
  feedback discrimination is certified.
- **EQ** uses specified causal filters. **SMART EQ** is an explicitly bounded
  EQ workflow; its name does not imply room measurement, source recognition,
  or an automatically correct target response.
- **DYNAMICS**, **SMART COMP**, and **OPTICAL COMP** use documented envelope
  and gain laws. The optical label denotes a mathematical timing/curve model;
  no optocoupler, tube, transformer, or named hardware unit has been measured
  or matched. Input crest factor materially changes detector behavior.
- **FX**, **FX DIGITAL**, and **REVERB96** are specified delay, quantization,
  modulation and/or reverberation algorithms. The digits in REVERB96 are a
  module name, not a promise of a fixed 96 kHz internal rate, impulse-response
  capture, or reproduction of another reverb. Host sample rate is tested.
- **SUB PRO**, **DRUM LIVE**, and **PC AUDIO** are explicit processing chains
  for those workflows. They do not determine safe crossover, limiter or gain
  settings for unknown connected equipment.

## Source algorithm definitions

`HP2(f)` and `LP2(f)` below are second-order causal Butterworth sections with
Q = 0.70710678118655. `Bell(f,Q,g)` is an RBJ peaking filter with center gain
`g` dB. Frequency is limited to 0.45 times the host sample rate. Gains refer to
normalized sample amplitude; a sample clip ceiling is not a true-peak limiter.

| ID / requested label | Actual chain and fixed constants | Independently exercised cases |
| --- | --- | --- |
| 1 / VOCAL CLEAN | HP2; 250 Hz bell, Q .9; high band obtained as signal minus LP2(5 kHz); HF envelope 1/60 ms; strength-dependent HF attenuation; peak compressor 10/120 ms; makeup | Neutral-de-ess filter impulse; sustained HF reduction; quiet LF unchanged |
| 2 / VOCAL COLOR | HP2; warmth bell 180 Hz/Q .7; presence bell 3.5 kHz/Q .8; soft saturation; wet mix and output | Full filter impulse and independent pointwise tanh/mix curve |
| 3 / AI-FB | Metadata name: Feedback Guard. Manual narrow **bell cut**, not a zero-depth ideal notch; HP2; sample clip guard; wet mix. No detector | Manual bell/HP impulse with independent center/Q convention |
| 4 / AUTO FB PRO | Metadata name: Auto Feedback (advisory). Positive zero-crossing periodicity with persistence and tolerance; manual bell cut and output gain. No automatically applied cut | Manual bell impulse; pure periodic musical tone produces advisory while audio stays unchanged at zero depth; advisory clears after silence |
| 5 / EQ | HP2; 100 Hz bell/Q .7; variable mid bell; 8 kHz bell/Q .7 | Nonzero gains/Q full-chain impulse; independent corner and center-gain anchors |
| 6 / SMART EQ | HP2; 400 Hz and 3 kHz LP2 band split; smoothed absolute-band/total ratios; bounded strength-dependent low/high cuts; presence bell 3 kHz/Q .8; output | Neutral-control impulse and steady 100 Hz/6 kHz band law |
| 7 / DYNAMICS | Peak envelope; feed-forward above-threshold compression; below-threshold downward expansion; makeup | Closed-form attack/release vector, 4:1 static curve, expansion law |
| 8 / SMART COMP | Asymmetric smoothed **mean square** detector; RMS gain law; release coefficient interpolated between release and 3x release using previous mean square; parallel wet mix and makeup | Closed-form squared-envelope attack, wet/makeup, independently bounded adaptive release |
| 9 / OPTICAL COMP | Optional tanh; two peak envelopes sharing attack with release and 3x release; larger envelope drives the gain law; makeup | Closed-form dual-release vector with drive disabled |
| 10 / FX | Mono feedback delay; first-order low-pass in feedback; optional write saturation; dry/wet/output | Damped geometric echo series, exact rounded delay time, independent write saturation |
| 11 / FX DIGITAL | Symmetric quantization step `2^(1-bits)` with both +/-1 endpoints included; integer sample hold; short feedback echo; wet signal = .65 held + .35 echo | Half-away-from-zero codebook/tie vectors, sample hold and complete echo series |
| 12 / REVERB96 | Four parallel damped combs at .0297/.0371/.0411/.0437 s times room scale; mean of combs; two serial allpasses at .005/.0017 s, coefficient .5; optional predelay; wet/output | Independent transfer-space comb impulse series, allpass convolution, predelay/first arrival and decaying impulse tail |
| 13 / SUB PRO | HP2; **two cascaded LP2** sections; 55 Hz bell/Q .8; optional tanh, polarity and output | Full-chain impulse/polarity and -6.0206 dB low-pass cutoff anchor |
| 14 / DRUM LIVE | HP2; 90 Hz and 4.5 kHz bells/Q .8; fast/slow envelope attacks 1/30 ms, common gate release; below-threshold attenuation; bounded transient boost | Neutral-transient filter impulse and first-sample analytic transient anchor |
| 15 / PC AUDIO | HP2; 100 Hz and 7 kHz bells/Q .7; peak compressor 10/120 ms; sample clip guard | Independent filtered sample-clipping curve with unity compressor ratio |

The Sub Pro low-pass is four poles made from two Butterworth second-order
sections, giving -6.0206 dB at its selected cutoff. It is not a single
fourth-order Butterworth with a -3.0103 dB cutoff. Digital FX's bit control
describes its quantization spacing, not a packed signed-PCM output format.

The common envelope law uses `c = exp(-1/(fs * timeSeconds))` and
`e[n] = c*e[n-1] + (1-c)*abs(x[n])`, choosing attack when the target exceeds
the previous envelope and release otherwise. A time setting is a one-time-
constant response (~63.2% toward a constant target), not time to settle.
Smart Compressor applies that law to squared samples before taking a square
root. For envelope `e`, threshold `T` and ratio `R`, compressor gain is unity
below threshold and `(T/e)^(1-1/R)` above it. Dynamics additionally multiplies
by `min(1,e/G)` for gate threshold `G`.

For positive drive, saturation is `tanh(x*d)/d`, where `d = 10^(driveDb/20)`;
zero drive is an exact nonlinear-stage bypass. This deliberately does not
normalize peak loudness. Vocal Clean's de-ess strength is dimensionless:
its high-band divisor is `1 + strength*10*max(0,HF-envelope-.06)`.
It is not a calibrated maximum reduction in dB. Smart EQ's strength controls
similarly describe bounded heuristic energy-dependent cuts.

Delay lengths use nearest-integer sample rounding. A comb's feedback is
`10^(-3*delaySamples/(fs*decaySeconds))`; its damping is a first-order low-pass.
The decay control defines a nominal low-frequency feedback decay, not a
measured broadband room RT60. The tests compare the actual causal transfer
function; they do not evaluate reverb diffusion, metallic coloration or taste.

## Independent test construction

`tools/independent_builtin_check.py` generates every input vector in Python and
sends it to `tools/lah_builtin_cli.cpp`. The bridge does not generate a reference
signal, export the engine's expected output, or compute reference coefficients.

Filter references are designed independently from analog transfer polynomials
with a prewarped [SciPy bilinear transform](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.bilinear.html).
Sample responses are evaluated by [SciPy lfilter](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.lfilter.html).
This provides a separate coefficient derivation from the direct trigonometric
biquad implementation. The [W3C Audio EQ Cookbook](https://www.w3.org/TR/audio-eq-cookbook/)
defines the filter conventions used for comparing corner frequency, Q and bell
gain; equality to that convention is a numerical claim, not subjective EQ QA.

Dynamics checks use the closed-form exponential solution on constant-amplitude
intervals and independent threshold/ratio anchors. Delay checks use explicitly
placed delayed impulses and geometric echo amplitudes. These references are
separate from the rack's ring buffers and per-sample envelope recurrence.

Additional invariants cover all 15 module types at 44.1, 48 and 96 kHz:

| Case | Required property | What it does not establish |
| --- | --- | --- |
| Disabled slot | Finite input is bit-exact at output | Click-free arbitrary automation |
| Inactive channel | Other channel stays bit-exact | All physical routing/device mappings |
| Reset/replay | Repeated vector after reset matches zero-state rendering | Every possible runtime reconfiguration |
| Disable transition | Exact bypass after the specified 64-sample transition; no wet tail continues afterward | Safe real-world switching during a show |
| Block partition | Same static controls at fixed and varying callback sizes | Bounded CPU time on all machines |
| Damaged input | NaN, infinity and extreme finite input never yield non-finite output | Audible recovery quality after corrupted input |

## Reproduction protocol

The CLI uses interleaved stereo little-endian IEEE float32 on stdin and stdout:

```text
lah_builtin_cli type rate p0 p1 p2 p3 p4 p5 [enabled=1] [resetFrame=-1] [disableFrame=-1] [blockSize=0]
lah_builtin_cli --catalog
```

Module IDs are 1 through 15 in the display order above. The catalog reports the
actual parameter names, ranges and defaults. All six parameter arguments are
present even when a module uses fewer controls. A block size of zero exercises
1, 17, 64, 127 and 513-frame callbacks. Reset and disable events split a callback
at the exact requested sample. No audio device is opened.

After building `lah_builtin_cli`, run Python with NumPy and SciPy available:

```text
python tools/independent_builtin_check.py path/to/lah_builtin_cli.exe
```

The command fails with a nonzero exit on an unmet assertion. Its final PASS
line applies only to the enumerated synthetic comparisons. Sample rate can be
restricted while diagnosing with `--rate 48000`.

## Execution record

Run: 2026-10-22, Windows Release `build-windows-v07/Release/lah_builtin_cli.exe`.
Runtime: Python 3.12.14, NumPy 2.2.6, SciPy 1.14.1. Executable SHA256 at that run:

```text
35EE779847DB804B1D0E327A41D62D761C299B7D3F6DC9FAA38FDA2E9F3C6199
```

Command (with local NumPy/SciPy on `PYTHONPATH`):

```text
python tools/independent_builtin_check.py build-windows-v07/Release/lah_builtin_cli.exe
PASS: 402 native processor render cases (synthetic offline checks only).
```

The 402 renders consist of 87 algorithm/reference vectors and 315 state and
bypass vectors, equally divided among 44.1, 48 and 96 kHz. The first-arrival
test uses a 1e-6 meaningful-echo threshold plus a separate 2e-7 pre-arrival
bound: float dry/wet cancellation can leave a ~1.5e-8 dry impulse residual at
100% wet. Whole-vector comparison remains required. Filter/nonlinear sample
tolerances are 2e-7 to 8e-7, dynamics up to 1.2e-4 for float detector rounding,
and delay/reverb up to 3e-6. Smart EQ's steady-tone comparison permits 2.5%
absolute-to-unity gain error for detector ripple; it is a steady behavioral
comparison, not full nonlinear time-varying equivalence.

Not independently established: arbitrary-signal Smart EQ/de-ess effectiveness,
voice/noise separation, optical-hardware equivalence, music/feedback
discrimination, no false positives, true-peak limiting, complete nonlinear
release trajectory equality, subjective FX quality, live-device latency and
dropout behavior. All 15 modules additionally passed the stated generic state
invariants; those invariants do not replace missing perceptual/acoustic tests.

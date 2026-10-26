# AI-FB LAB: synthetic feedback classifier model card

Recorded on **2026-10-23 (Asia/Saigon)**. Model version:
`synthetic-mlp12-v1`. This is an actual trained neural classifier with exported
fixed weights. It is a **LAB component trained only on generated signals**.
It is not an independently validated acoustic feedback eliminator, speaker
protection system, hearing protection system, or clone of a named product.

## Intended use and boundary

The model supplies one additional score to a deterministic feedback controller.
It does not modify audio, select notches, command hardware, or arm itself.
The controller must separately verify signal level, spectral evidence and
persistence, bound every cut, and require explicit operator arming for automatic
cuts. Default advisory operation remains appropriate. An operator must be able
to bypass/reset it and use manual EQ.

The sigmoid output is a **learned score, not calibrated confidence**. A display
of 0.95 must not be described as a 95% probability of real feedback. No measured
mic/PA dataset, M32R recording, room impulse response collection, field trial,
subjective music test, or acoustic stability margin was used here.

There is a fundamental input ambiguity: a growing synthesizer sine and a
microphone feedback tone can have the same observed samples. The separate
ambiguity test below produces many false positives. This limitation cannot be
removed by relabeling the score or by adding narrow notches after it.

## Source and architecture

- Reproducible generator/trainer: `tools/train_feedback_model.py`.
- Inference and streaming features: `src/dsp/FeedbackModelWeights.h`.
- Checked-in canonical float constants for numerical CI:
  `tests/data/feedback_model.json`.
- Architecture: 12 standardized features -> 12 tanh hidden units -> one sigmoid
  output, **169 learned scalar parameters**, plus 24 normalization constants.
- Optimization: weighted binary cross entropy, Adam, learning rate 0.003,
  minibatches of 512, L2 weight coefficient 1e-4, 4,000 steps. Fixed seed
  `20261023`; initialization/optimizer seed is that value +999.
- Training class weights give positive and negative frames equal total weight.
  The exported checkpoint has the lowest unweighted validation BCE among
  checkpoints every 100 steps: **step 3,500**. Test data did not select it.
- Feature means/standard deviations come from training data only; standardized
  values are clipped to +/-8. Header weights are float32; inference is C++17
  without an ML runtime, network connection, heap allocation or locks. Weights
  stay fixed during use; microphone audio is not uploaded or used for online
  retraining.

The actual validation BCE fell from 0.7186428 at step 1 to 0.0620894 at step
4,000; the selected checkpoint is recorded separately in `training-report.json`.
These values verify that numerical optimization ran, not that real microphone
classification is accurate.

## Dataset construction and split

Each split contains **650 separate 1.6-second generated clips** and **32,370
non-overlapping analysis frames**. Frames within a clip are correlated; 32,370
must not be presented as that many independent acoustic trials. Analysis rates
are 12, 16, 22.05, 24 and 44.1 kHz. Every clip has a seeded waveform SHA256 in
the exported `dataset_clips` manifest; the dataset digest covers the
training feature/label float32 arrays. No audio was downloaded.

Positive clips use an actual recursive, linear closed loop:

`y[n] = source[n] + g * BPF(y[n - D])`, with `g` between 1.12 and 1.85.

The BPF center is a delay mode (`k * fs / D`), bounded to 100 Hz..0.42 fs, and
its peak response is unity. Some paths add a second delayed return (18% of the
primary return, extra delay D/3). Excitation mixes broadband noise and an
unrelated five-harmonic voiced signal. Simulation proceeds in D-sample chunks
and carries the BPF state, so the returned signal really depends on earlier
loop output. It is **not a positive sine added by hand**. The generated output
is peak-normalized after the linear simulation and mixed with quieter program
excitation at varying level. Frames below -90 dBFS are labeled negative because
the growing signal is not useful observable evidence at that level. A separate
controller floor remains necessary; the model can violate that boundary.
Above the floor, unstable-loop frames retain the positive label even during
early buildup when excitation may dominate; these labels are generated loop
state labels, not human-annotated audible-onset times.

| Split | Delay families (ms) | BPF Q families | RNG seed |
| --- | --- | --- | --- |
| Training | 4, 9, 17, 28 | 5, 12, 24 | 20261023 |
| Validation | 6, 13, 22, 33 | 8, 17, 32 | 20361023 |
| Test | 7, 15, 24, 37 | 10, 20, 40 | 20461023 |

Thus an entire clip and its path settings belong to one split. Adjacent windows
of the same recording are never randomly divided between training and test.
All splits share the same broad synthesis assumptions; holding out delay/Q
families does not hold out all real-room behavior. The deliberately identical
all-zero silence baseline appears in each split; it is a guard case, not an
independent acoustic recording.

Per split, 200 clips use unstable closed loops. The other 450 comprise 50 clips
each of white/colored noise, held pure tones, logarithmic sweeps, harmonic
signals, synthetic voiced/formant signals, synthetic chords/program envelopes,
percussive noise, silence, and stable closed loops (gain 0.35..0.92). The
speech-like and music-like signals are generated approximations, **not real
speech or music recordings**. Some negatives have ordinary level rises/falls.

An additional **100 clips / 4,980 frames** use exponentially growing musical
sines and an independent seed (`20561023`). They are negative by source origin,
but intrinsically ambiguous from the waveform. They are excluded from fitting
and checkpoint selection and reported separately, without hiding their errors.

## Feature contract

`Analyzer::processSample` consumes analysis samples and returns true once per
`round(0.032 * analysisRate)` samples. It does not hold/delay the dry audio.
The first score requires a complete frame; temporal evidence and the enclosing
controller add further detection time. This is not a 32 ms feedback-removal
latency guarantee.

Input NaN/Inf becomes zero and finite samples are bounded to +/-16. Sums are
double precision. Positive zero-crossing intervals outside 60..12,000 Hz are
invalid. Interval state and the last two samples carry across frame boundaries.

| Index | Quantity | Definition/bounds |
| --- | --- | --- |
| 0 | Level | `20 log10(max(RMS,1e-6))`, -120..24 dBFS |
| 1 | Crest factor | `20 log10(max(peak,1e-6)/max(RMS,1e-6))`, 0..40 dB |
| 2 | Kurtosis | `N*sum(x^4)/max(sum(x^2)^2,1e-24)`, 0..50; not excess kurtosis |
| 3 | Crossing frequency | `fs/mean(valid intervals)/12000`, 0..1; zero when no valid interval |
| 4 | Lag-1 correlation | `sum(x[n]*x[n-1])/max(sum(x^2),1e-24)`, -1..1 |
| 5 | Lag-2 correlation | Same normalization for lag 2, -1..1 |
| 6 | Difference energy | `sum((x[n]-x[n-1])^2)/max(sum(x^2),1e-24)`, 0..4 |
| 7 | Crossing interval CV | Population standard deviation divided by mean, 0..2; 2 if fewer than two valid intervals |
| 8 | Frame growth | Current level minus previous frame level, -24..24 dB; zero for first frame |
| 9 | Growth EMA | `0.75*previous + 0.25*frameGrowth`, initialized to zero |
| 10 | Period change | Absolute `log2(meanPeriod/previousMeanPeriod)`, 0..4; zero if either missing |
| 11 | Adjacent agreement | Fraction of valid intervals within `max(1 sample,5% previous interval)` of the previous valid adjacent crossing interval |

The lag/difference features depend on analysis sample rate. The model was
trained at the five stated rates. API finite guards accept 8..192 kHz, but this
does **not** establish classifier validity at every accepted rate. The host
must pass its actual analysis rate after any decimation and must not feed
unfiltered aliased decimation as if it were validated input.

Public API in namespace `lah::feedback_model`:

```cpp
Analyzer analyzer;
analyzer.prepare(actualAnalysisRate);
if (analyzer.processSample(analysisSample)) {
    const float learnedScore = analyzer.score();
    const float crossingEstimate = analyzer.estimatedToneHz();
    const auto features = analyzer.features();
}
analyzer.reset();
float score = classify(std::array<float, 12>{ /* contract above */ });
```

The crossing estimate is not the host's verified FFT notch center and must not
replace that spectral estimator. `classify` rejects any non-finite feature with
score zero. `reset` immediately clears published score/frequency and all history.

## Actual evaluation results

Threshold **0.90**, frame-level results from the actual exported checkpoint:

| Population | TP | FN | FP | TN | Recall | Negative frame false-positive rate | ROC AUC |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Training | 7,766 | 613 | 33 | 23,958 | 92.68% | 0.138% | 0.99944 |
| Validation | 7,922 | 805 | 73 | 23,570 | 90.78% | 0.309% | 0.99669 |
| Held-out test paths/seeds | 7,314 | 1,354 | 40 | 23,662 | **84.38%** | **0.169%** | 0.99038 |
| Ambiguous growing musical tones only | — | — | **1,750** | 3,230 | — | **35.14%** | — |

The low ordinary synthetic negative rate and the high ambiguous-tone error
describe different populations. Neither is a real-world false-alarm rate.
Fourteen of the 40 held-out test false-positive frames are early closed-loop
frames below the -90 dBFS label boundary. Other test false positives: noise 8,
percussion 5, synthetic speech 8, stable loops 5. No threshold crossing occurred
in the enumerated held pure tones, sweeps, harmonic, synthetic chord or silence
test frames; this is not proof that other such signals cannot trigger.

The model can miss stationary feedback, clipped/limited feedback, multiple
simultaneous resonances, low frequency outside the generated coverage, and
feedback obscured by program material. Stationary limited feedback was not
trained as a positive class. It should not be the sole gate in a safety system.

## Reproduction and software checks

Use the project's local NumPy/SciPy environment and run:

```powershell
$env:PYTHONPATH = Join-Path (Get-Location) '.tools/python'
python tools/train_feedback_model.py
```

No ML framework is required. The actual run used Python 3.12.14, NumPy 2.2.6,
SciPy 1.14.1 on Windows. A second complete training run with the same defaults
and a separate output directory reproduced the **identical generated header
SHA256** and the same dataset digest/metrics.

| Provenance item | SHA256 |
| --- | --- |
| Training feature/label float32 arrays | `066fbd06948d043be19ee8ecbd9e342b26fbabfe3948a43fa03e4c8f6a056f69` |
| Exported `FeedbackModelWeights.h` | `9f2b50086c7d92c71f8f8b733cbebf0dd414313cef20ff6d0438d9637d9a8135` |
| Trainer source at this run | `57f6b9effb9884d1c6e847057b0899beacf349e1665ea6f4379e54b422903832` |
| Checked-in canonical `feedback_model.json` | `8690f3c2fcf68b60cba203bb25dea4c747f290c3c14046f97470791ad206ac57` |

Artifacts in `artifacts/feedback-model/` include the training report, float
model arrays, independent raw-sample/feature/score JSON fixtures, MSVC feature
probe source, and parity report. The probe compiled with **MSVC 19.44 x64
Release**. Five 16 kHz raw clips / 100 frames passed Python-versus-C++ comparison:
maximum feature error **2.9803e-8**, maximum score error **8.7371e-8** against
limits 3e-5 and 2e-5. The probe also checked non-finite feature rejection and
reset's published state. This checks implementation parity, not PA performance.

Clean-source CI does not need the artifacts directory. Build
`tools/lah_feedback_model_cli.cpp` and run:

```text
python tools/independent_feedback_model_check.py path/to/lah_feedback_model_cli.exe
```

The actual MSVC Release execution passed **25 independently generated clips /
495 frames** at all five analysis rates, plus NaN/Inf feature rejection,
NaN-to-silence input handling, reset state and finite extreme-input handling.
Maximum feature error was **3.8027e-6**, score error **5.7200e-7**, and crossing
frequency error **0.000410 Hz**, with limits 3e-5, 2e-5 and 0.002 Hz. Python
uses a separate batch feature calculation and canonical checked-in constants.
Its feedback fixture uses the closed-loop polynomial
`Y/U = A/(A - g*z^-D*B)` and SciPy filtering, independently from the trainer's
delay-chunk recurrence. Tones/noise/voiced/growing-tone fixtures are generated
by the check itself; it does not ask C++ to export expected answers.

Integration, notch response, persistence, CPU budget, no-allocation behavior,
host bypass, scene restore, physical feedback behavior and hardware latency
require their own tests. They are deliberately not inferred from model metrics.

## Remaining acceptance work

Collect consented, labeled real mic/PA loop recordings with program material,
hold out complete rooms, microphone/loudspeaker models, gain paths and sessions,
and measure false cuts per minute/event as well as detection delay and missed
feedback. Include sustained instruments, synthesizer swells, vibrato, clipping,
multiple feedback modes and changing positions. Evaluate the complete controller
in controlled low-level tests before any show use; retain supervised operation.
Only such measurements can support a real-system accuracy or autonomy claim.

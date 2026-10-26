# LiveAudioHost v0.4 — Engineering calculations & source-image assessment

Current Windows update: the expanded 12-mode calculator, Phase Engine and six coverage modes are built and tested with MSVC/JUCE. See [current feature status and remaining work](FEATURE_STATUS_AND_ROADMAP_v0.7.0.md). The v0.4 build status below is historical; mathematical assumptions remain documented here.

**Status: experimental, offline, mathematical calculations only.** Desktop GUI has NOT been built/tested on Windows, and no physical amplifier, microphone or M32R was connected. Not a certified SPL meter, PA-protection processor, or Smaart replacement.

## User-provided AcousticLab VN screenshots: mapping to original implementation

The user confirms that the screenshots contain results calculated by expert software. These are engineering reference cases for LiveAudioHost. A numerical comparison is accepted only when inputs, units, level definitions and model assumptions match; cases without legible or complete settings remain pending. Calculated predictions and microphone measurements are identified separately in the app.

| Screenshot topics | Code added to this version | Caveat |
|---|---|---|
| Ohm Wheel, dBu, V RMS, voltage/current/power | `EngineeringCore::ohmPower`, `dbu`, `voltsFromDbu`; native JUCE tab | Resistive equivalent only; speaker nominal impedance is not constant |
| Speaker sensitivity + power + listener distance + quantity | `idealSpl` with coherent/incoherent theoretical bounds | NOT calibrated dB SPL; excludes directional, room, compression |
| Speaker limiter based on AES/nominal watts, ohms, amplifier gain | `theoreticalRmsThreshold` | **Do not apply blindly**: no time constants, peak protection, crest factor, calibration, manufacturer thermal model |
| Cable loop length, copper gauge, amplifier damping factor | `cable` | Reference 20°C DC copper model, not a cable safety/rating test |
| Air humidity, temperature, travel, ISO 9613-1 | `iso9613AirAbsorptionDbPerM`, validated against independent cells | Pure tone air only, 50Hz..10kHz; no reflections or environmental acoustics outside scope |
| Phase/air-temperature drift, wavelength, quarter wavelength | `phaseForDelay`, `arrivalDriftMs` | Does not infer actual phase, source alignment, or system delay without measurements |
| Point-source/front-fill automatic count, nominal -6dB beams, arc | `CoverageCore` + native JUCE `Front-Fill / Coverage` 2D diagram | **RAY GEOMETRY ONLY**; no spatial summation/comb-filtering/polar calibration |
| Sub arrays temperature dependent propagation | Added temperature control to existing SubPanel | Sub array remains free-field omnidirectional, relative pressure only |
| Transfer function, RTA, speaker array heatmap, VST3 | Existing v0.3 modules remain | No new field validation; not licensed/certified for show-ready usage |

## Scientific formulas & assumptions

### 1. Electrical and speaker estimates

For sine voltage on a PURELY RESISTIVE load, `Vrms = sqrt(P * R)`, `Irms = Vrms / R`, `Vpeak = sqrt(2)*Vrms`, `dBu = 20*log10(Vrms/0.775)`.

Ideal free-field point-source SPL: `sensitivity_1W1m + 10 log10(P_W) - 20 log10(distance_m)`; optionally add `10 log10(N)` for strictly incoherent equal-energy sources or `20 log10(N)` for coincident perfectly coherent equal-amplitude sources. An actual distributed loudspeaker array cannot generally use those gains to claim coverage or maximum SPL; real positions, directivity and interference matter.

Theoretical DSP-side RMS input threshold: `20 log10(sqrt(P_cont*R_nom)/0.775) - amplifier_voltage_gain_dB`. An actual RMS/peak limiter is *not* this calculator; consult the manufacturer's tested settings and verify amplifier gain and DSP reference. **No automatic writes to speaker management or M32.**

Cable model: two conductor loop DC resistance `Rc = 2 * rho * oneWayLength / area_mm2`, default `rho=0.0175 ohm*mm2/m` (approx. 20°C); ideal amp source impedance `Ra = R_nom/DF_at_Rnom`; voltage ratio `Rnom/(Ra+Rc+Rnom)`; cable heat from calculated current squared * cable resistance. Actual frequency-domain speaker impedance and complex wire impedance are not modeled.

### 2. Atmosphere, pure-tone air attenuation

ISO 9613-1:1993 analytical attenuation coefficient, taking pressure default 101.325 kPa and water saturation using 273.16 K triple-point constant. In GUI, each point on frequency chart is calculated from the actual formulas; no fabricated curves. Speed of sound approximated as `343.2 * sqrt((T_C+273.15)/293.15)` m/s. Validated against the National Physical Laboratory published coefficient test values, subject to their stated precision.

**Reference comparison pending:** LiveAudioHost's ISO calculation at exactly 8 kHz, 20°C, 50% RH and 101.325 kPa gives about 105.29 dB/km. The screenshot contains a different displayed value; its complete settings, distance scaling and frequency-band definition have not yet been established. This is a comparison to resolve, not a finding that the expert software is incorrect. NPL validation cells are retained as a separate reproducible reference.

### 3. Front-fill geometry

Auto count is calculated for the requested **geometric target width** and nominal -6dB angle. Each source is represented as a point and a pair of 2D rays. Sample intersections at a target listener line y=d and report percentage of listeners *inside at least one nominal angle cone* and the maximum geometric uncovered segment. The colored segments are NOT an SPL map. Straight front-fill and a simplified curved arrangement with speaker azimuth oriented toward uniformly spaced listener targets are supported, but no measured loudspeaker polar response is loaded. A high visual percentage does NOT prove coherent summation, tonal balance, reflection behavior, or usable front-fill in a hall.

### 4. Reproducible test / comparison

Build only math core without JUCE:

```
cmake -S . -B build-core -DLAH_BUILD_GUI=OFF
cmake --build build-core --config Release
ctest --test-dir build-core --output-on-failure -C Release
```

For GUI on Windows (MSVC, JUCE 8.0.15):

```
.\scripts\build-windows.ps1 -JuceRoot "C:\SDK\JUCE"
```

Validation is NOT finished until Windows executable builds, is operated in a test environment, numerical results are compared against independent references, and audio measurements are matched to calibrated hardware/source materials.

## External references for implementation (not third-party code)

- ISO 9613-1:1993 overview: https://www.iso.org/standard/17426.html
- National Physical Laboratory ISO 9613-1 independently published validation data: https://resource.npl.co.uk/acoustics/techguides/absorption/validata.html
- Open acoustic formula example: https://python-acoustics.github.io/python-acoustics/standards.html
- Speaker-protection calculation examples in manufacturer manual: https://adn.harmanpro.com/product_documents/documents/1551_1352137490/fds336Tum_original.pdf
- JBL/HARMAN warning on amplifier gain and limiter calibration: https://pro.harman.com/insights/harman-pro/new-insights-into-the-dangers-of-using-power-amplifiers-that-are-too-small/

Other features shown in screenshots — proper speaker manufacturer data import, multi-band power compression, calibrated broadband SPL, manufacturer-rated amplifier/genset loading, ISO 9613-2 ground/weather/reflections and precise speaker limiter protection — remain future engineering work, not already implemented.

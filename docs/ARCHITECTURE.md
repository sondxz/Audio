# Architecture (v0.2 research prototype)

> Current Windows implementation is documented in `WINDOWS_DEVELOPMENT_REPORT_v0.7.0.md`: measurement/SPL workers, short FIR, bounded static reported-latency PDC, mute-generation invalidation and final output protection are present. This diagram is historical.

```
Windows ASIO/WASAPI (JUCE AudioDeviceManager)
    |
    +--> LiveAudioEngine audioDeviceIOCallbackWithContext() [real-time]
            |  Atomic params snapshot; peak meter and measurement capture
            +--> Mixer / 32x32 routing / HPF+PEQ+gate+compressor
            |    +--> FX send stereo delay/reverb --> return to OUT 1/2
            |    +--> output delay / gain / polarity / clip guard
            +--> optional VST3 single stereo insert on OUT 1/2
            +--> physical output callback slots
    |
    +--> SPSC atomic Capture (MIC + REFERENCE raw input)
           +--> MeasurementPanel 3Hz (non-real-time): FFT 8192, TF,
                magnitude/phase/coherence, GCC-PHAT, CSV export

UI message thread:
    Devices Tab     -> JUCE AudioDeviceSelectorComponent
    Mixer Tab       -> dsp::Controls atomics; 32x32 matrix
    Effects Tab     -> FX controls atomics
    Plugin Tab      -> worker thread VST3 scanner; callback detached for load
    Measurement Tab -> snapshot/analyze; plots and export
    Sub Array Tab   -> 3D free-field complex pressure model on GUI thread
    Scenes          -> XML saved in app user data; mute required for recall
```

DSP core, measurement/FFT, and sub simulation are framework-independent C++17 headers with automated tests. Engine uses no file I/O, GUI work, direct network calls or blocking locks in its own callback. **Third-party VST3 processors can violate realtime constraints**, and JUCE buffer view methods require Windows verification.

Measured channel count can differ from output channel count; only first 32 audio callback slots have DSP routing. Outputs >32 muted for safety.

No latency compensation across 32 outputs; delay is user-controlled per output. No actual Windows GUI verification and no certified show usage.

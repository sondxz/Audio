# Acceptance matrix / audit of ALL requested LiveAudioHost modules
`CORE TESTED` means selected offline C++ mathematical paths were executed — NOT that the entire module or UI has passed.

| Module | Source present | Verified here | Blocking gaps to release |
|---|---|---|---|
| Audio Engine + Device Manager | Yes | CORE TESTED | Windows JUCE build, USB/ASIO timing, disconnect, loopback |
| 32×32 routing/mixer | Yes | CORE TESTED | hardware 32ch routing, bus/group/PFL/AFL, solo, 1:1 safety |
| EQ + basic Dynamics | Yes | CORE TESTED | comprehensive response sweeps, true-peak limiter, compressor behavior on real signals |
| Delay + basic Reverb | Yes | CORE TESTED | phase/timing multichannel, UX and musical quality |
| VST3 host (one stereo) | Yes | NOT TESTED | third-party plugin validation, chain, editor, crash isolation, PDC |
| FFT/RTA/H1 Magnitude/Phase/Coherence | Yes | CORE TESTED | ASIO capture, referenced physical measurement, drift |
| Delay finder | Yes | CORE TESTED | impulse/room dataset comparison and loopback |
| Impulse/ETC/Spectrograph | No | NOT TESTED | full implementation |
| SPL calibration A/C/Z preview | Yes | CORE TESTED | traceable acoustic reference, IEC tolerances, mic and front-end |
| Short FIR 2-ch lab insert | Yes | CORE TESTED | windows audio runtime, actual output IR, PDC, CPU headroom |
| Long FIR/crossover/speaker limiter | Partial | NOT TESTED | partitioned FFT convolution, IIR crossover, limiter validation |
| Sub Array free field 2D/3D | Yes | CORE TESTED | real measured polar reference, test outdoors, ground reflections |
| Point-source/Front-fill geometry | Yes | CORE TESTED | measured coverage and wave interference |
| Room 3D acoustic propagation | No | NOT TESTED | room materials, directional reflections and 3D plots |
| Engineering electrical calculators | Yes | CORE TESTED | electrical fixture/current transient/impedance-vs-frequency validation |
| Project/scene local single file | Yes | CORE PARTIAL | scene migration, corruption recovery, multi-show, autosave |
| AI assist / MIDI / OSC | No | NOT TESTED | authorized automation with user confirmation, security |
| Packaging `.exe` and Windows long-run QA | No | NOT TESTED | requires Windows/MSVC/JUCE/hardware |

See `RELEASE_VALIDATION_REPORT_v0.6.md` for actual evidence. Marking all modules PASS or claiming professional equivalence to Smaart/VOS is unsupported.

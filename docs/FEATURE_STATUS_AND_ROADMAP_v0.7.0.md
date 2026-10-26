# LiveAudioHost — trạng thái tính năng và hướng xử lý, 2026-10-23

Đã build Windows x64 bằng MSVC/JUCE 8.0.15. Trạng thái sản phẩm: **bản LAB đã kiểm thử phần mềm, chưa nghiệm thu hệ thống PA thực**. “Đạt” bên dưới chỉ áp dụng phạm vi đã ghi, không có nghĩa đã hoàn thiện toàn bộ nhóm chức năng.

## Mốc hiện tại — AI-FB LAB/AUTO FB PRO và native rack

Giữ giao diện graphite/xanh ngọc, mixer fader dọc/bank, measurement phase/magnitude và SPL dễ đọc, sidebar/telemetry/Diagnostics; **15 native module, 32 input × 4 slot** với scene XML. **AI-FB LAB có neural model được huấn luyện trên tín hiệu tổng hợp; AUTO FB PRO có spectral/growth detector. Cả hai có cut tự động khi người vận hành ARM.** Chi tiết mã nguồn, workflow và kiểm chứng: [AI_FEEDBACK_IMPLEMENTATION_v0.7.0.md](AI_FEEDBACK_IMPLEMENTATION_v0.7.0.md), [model card](FEEDBACK_MODEL_CARD.md). [Báo cáo console/native rack trước AI](UI_AND_BUILTIN_RELEASE_REPORT_v0.7.0.md) là baseline lịch sử.

- Windows Release **30/30 CTest PASS**, 29,73 giây; MSVC ASan core **28/28 PASS**, 48,39 giây. Không suy rộng ASan core sang toàn GUI/JUCE hoặc VST3 bên thứ ba.
- Native DSP: **402 render đối chứng độc lập PASS** tại 44,1/48/96 kHz cho phần manual/unarmed; **30 feedback render cases** mới có filter, growth, negatives và một loop plant tổng hợp hồi tiếp thật. Model parity: **25 clip/495 frame**, chỉ kiểm số feature/score. Callback JUCE kiểm native/feedback và mute/PANIC, channel isolation, echo.
- GUI smoke: **73 PNG, 0 bounds warnings**, gồm AI/Auto ở kích thước thường và tối thiểu; đã xem trực tiếp bốn ảnh feedback. Device đóng, 0 callback, output mute; không có SPL giả trong smoke.
- Delay/history pool baseline khoảng 48 MB ở 48 kHz, 192 MB ở 192 kHz cho 128 slot; bản mới có thêm preallocated detector state. Các số pool không phải tổng memory ứng dụng. Tham số/loại module hiện chỉnh khi MUTE; chưa có automation mượt đầy đủ.
- Benchmark cuối 48 kHz/128 mẫu: **32 AUTO FB max 2.726 µs, 128 AI-FB max 4.873,2 µs và 128 Reverb max 3.997,5 µs đều vượt chu kỳ 2.666,67 µs**. Các lần trước nhanh hơn được giữ nguyên trong log; không chấp nhận cam kết realtime cho ba cấu hình này. 32 AI-FB max 659,5 µs và 8×2 Vocal/Dynamics max 301,5 µs trong lần cuối vẫn chưa thay thế thử driver/stream dài hạn. Xem báo cáo AI/feedback để biết toàn bộ p50/p99/max và ngữ cảnh tải máy.
- **Feedback mặc định disarmed; DISARM dừng cut mới và giữ filter cũ, Clear bỏ cut.** Model chỉ có dữ liệu tổng hợp, score không phải xác suất đã hiệu chuẩn. Tập tone nhạc tăng mức nhập nhằng có 35,14% false-positive frames của classifier; chưa có tỷ lệ cắt sai/controller trên recording mic/PA thật. Chưa đánh giá nghe hoặc nghiệm thu phần cứng.

XRun WASAPI, ASIO/M32R, SPL phần cứng và soak 24 giờ vẫn chưa đạt/chưa xác minh. Các bảng phạm vi dưới đây giữ các chức năng có trước, đồng thời bổ sung native rack; các con số ở mục lịch sử không phải kết quả tổng hợp mới nhất.

## Lịch sử — kết quả giai đoạn mở rộng acoustic/engineering (2026-10-22)

- Release: **24/24 CTest đạt**, có callback JUCE tổng hợp và native GUI smoke.
- MSVC AddressSanitizer core: **22/22 đạt**; không bao gồm ASan cho toàn bộ JUCE GUI/plugin.
- Đối chứng độc lập: 3 TF/RTA, 30 FIR, 21 DSP, thêm **90 phase + 90 sub-array** với NumPy; coverage có **5.000 phép so sánh độc lập bằng tích vô hướng**. Engineering bổ sung nghiệm Kirchhoff, voltage/headroom/slew, peak/RMS, gain chain, điện 1/3 pha và converter.
- GUI: **54 PNG**, 11 tab ở hai kích thước, các chế độ mới và hai trường hợp phase có kiểm tra bất biến; **0 bounds warnings**, device đóng, 0 callback. Đã xem trực tiếp ảnh phase quadrature, sub inverted-stack, circle coverage và gain chain.
- Benchmark offline bản cuối 32×32 + stereo 257-tap FIR + PDC, 48 kHz/128: p50 190,4 µs, p99 286 µs, max 855 µs so với chu kỳ 2.666,67 µs. Không bao gồm driver, OS, GUI, VST3; không phải chứng cứ worst-case realtime.
- Audio thực từ giai đoạn trước: **hai lần WASAPI mute khoảng 10 giây đều có 1 XRun**, chưa đạt nghiệm thu; chưa xác định nguyên nhân. Không xóa hoặc bỏ qua số này.
- Máy có M32-Edit nhưng chưa nối mixer. Chưa nghiệm thu ASIO M32R, round-trip, SPL phần cứng hay soak 24 giờ.

## Đã triển khai và kiểm thử trong phạm vi phần mềm

| Nhóm | Chức năng có thực | Giới hạn hiện tại |
|---|---|---|
| Windows | EXE Release, CMake/MSVC/JUCE, ASIO biên dịch bật, giao diện graphite/teal | Chưa kiểm máy sạch/installer và phần cứng M32R |
| Console/native rack | Sidebar, mixer fader dọc, phase/magnitude lớn, SPL calibration-aware; 15 loại DSP gốc, 32×4 insert, scene XML; AI-FB synthetic-trained và AUTO FB PRO tự cut sau ARM | Chưa kiểm phân biệt feedback/nhạc trên PA thật, chưa tham số automation đầy đủ, chưa rack graph/editor phổ riêng; tải realtime còn cần kiểm theo cấu hình |
| Mixer/engine | Routing 32×32, gain/mute/balance, HPF, PEQ, gate/compressor, output gain/delay/polarity, guard/meter cuối chuỗi, PANIC xóa hiệu lực tail | Chưa tương đương mixer FOH đầy đủ |
| Realtime | Scratch cấp phát trước, chunk plugin theo prepared block, kiểm rate/frame, telemetry CPU/deadline, mute khi lỗi, PDC tĩnh cho single stereo insert | Vẫn cần kiểm driver/latency/WCET/24h và plugin thật |
| FFT/RTA/TF | FFT/Hann, H1, phase/coherence theo bin, GCC-PHAT, worker phân tích, snapshots A/B, CSV | Đã đối chứng tín hiệu tổng hợp; chưa đối chiếu acoustic hardware/Smaart trên cùng recording |
| FIR | LP/HP 129–4097 taps offline, xuất CSV; insert realtime 129/257 taps OUT1/2 và PDC group delay | Chưa long FIR partitioned hoặc correction đa kênh |
| Phase Engine | 2–16 nguồn sin, frequency link/riêng, level/polarity/signed delay, wheel, waveform tổng, converter có nhánh chu kỳ | Chưa 3D phasor tương tác; không phải phép đo phase/coherence |
| Sub array | 7 preset: Broadside/Cardioid/End-fire/LR/Inverted-stack/Arc/Arc-delay; XYZ, gain/delay/polarity/on-off; 4 probe, magnitude/phase, reference thời gian hình học, focus-at-probe, snapshot A | Áp suất tương đối free-field, không SPL hiệu chuẩn; surface projection không phải room 3D |
| Coverage | Auto/manual line, parabolic arc, circular arc, outward ring, custom CSV; vùng phủ góc danh định, probe path/delay, xuất geometry CSV | Hình học beam, chưa tính giao thoa/SPL/directivity theo tần số |
| Engineering | 12 chế độ: Ohm, SPL lý tưởng, RMS threshold, cable/DF, ISO air loss, phase/nhiệt độ; mạng 4 tải series/parallel, amp headroom/slew, peak/RMS voltage, gain chain, điện 1/3 pha/voltage drop, unit conversion | Mô hình điện trở và tham số người dùng; không chứng nhận bảo vệ loa/điện |
| Handbook | Các calculator mới có công thức, ví dụ và ứng dụng trực tiếp trong kết quả | Chưa sổ tay đầy đủ có tìm kiếm/đồ thị cho mọi chủ đề |
| M32 scene | Đọc offline toàn bộ **2.117 bản ghi** của FNBOS2024, hash và xuất inventory/report | Chưa importer .scn vào app, chưa gửi OSC hoặc áp dụng lên mixer |

## Chưa hoàn thành hoặc chưa được xác minh — cách giải quyết

| Hạng mục | Còn thiếu/lỗi | Hướng xử lý và tiêu chí nghiệm thu |
|---|---|---|
| Dropout/audio thực — ưu tiên 1 | WASAPI có 1 XRun mỗi lần thử, nguyên nhân chưa rõ | Ghi timestamp discontinuity, thử input/output riêng và driver/buffer khác; tiếp theo ASIO thiết bị thực; chạy 30 phút có tải DSP rồi soak 24h. Không coi callback nhanh là bằng chứng không dropout. |
| M32R — ưu tiên 1 | Chưa nối USB/mixer; chưa biết I/O driver thực | Cài/kiểm driver phù hợp card thực, xác nhận fs/buffer/channels, dùng scene lập sơ đồ send/return, thử loopback điện có mute/bypass ngoài app. Đo round-trip và khôi phục khi rút/cắm. |
| Routing scene | Source ID output chưa đối chiếu enum firmware; CARD block 17–24 lấy CARD1–8 | Đối chiếu M32-Edit/mixer và vẽ tuyến signal trước khi map. Importer phải báo tham số không tương đương, giữ mute và không tự bật phantom/routing. |
| Digital Mixer FOH/BOH | Chưa bus/group/aux/monitor/PFL/AFL/solo/DCA đầy đủ | Thêm graph bus rõ, tap pre/post, gain smooth; test routing/cue/scene recall và chống vòng lặp theo sơ đồ M32R. |
| Native processor chuyên nghiệp | 15 module đã có chuỗi DSP và kiểm số; AI-FB/AUTO FB có detector/cut khi ARM, nhưng model chỉ train synthetic và tone nhạc tăng mức có thể kích hoạt nhầm; chưa perceptual QA/automation hoặc giới hạn tải được nghiệm thu | Thu speech/music/feedback recording, đánh giá false cuts theo event/phút và missed feedback/latency; kiểm model/controller ngoài tập tổng hợp, smoothing tham số, CPU/memory, driver và nghe đối chứng. Không gọi AI-FB/AUTO FB đã hoàn thiện cho show từ kết quả synthetic. |
| VST3 | Single insert in-process; thiếu multi-rack/editor/state và crash isolation | Test plugin cụ thể với block/latency/state/exception; thêm rack/state và scanner/host isolation. Chưa loại trừ plugin crash/hang/destructor lỗi. |
| PDC | Bù tĩnh FIR + latency plugin báo, giới hạn 65.536 mẫu | Phát triển graph latency cho bus/rack/parallel; kiểm impulse qua mọi tuyến và plugin đổi latency; đo hardware riêng. |
| FIR chuyên nghiệp | Chưa partitioned convolution dài/per-output/correction thực | Thêm convolution phân vùng, kiểm delta/impulse/sweep với SciPy và CPU worst-case; giữ headroom và đo latency. |
| SPL | Chưa hiệu chuẩn mic/calibrator thật và đối chứng reference meter; chưa IEC 61672 | Dùng mic/calibrator vật lý, ghi chuỗi gain, đo nhiều mức/tần số, lập uncertainty; kiểm weighting/time response/overload. Giữ không hiển thị SPL đo khi chưa calibration hợp lệ. |
| RTA/TF/phase/coherence/delay | Chưa validation acoustic, fraction-octave IEC và delay phức tạp ngoài fixtures | Cùng WAV/ref/mic, khớp window/averaging/time-reference với analyzer độc lập; test noise/reverb/polarity/negative delay và coherence mask. |
| Acoustic Simulation | Chưa room/ground/reflection, vật liệu, polar elevation/full3D, camera 3D tương tác | Tách direct/ground/reflected path, import dữ liệu loa có provenance; đối chứng các nghiệm analytic và impulse/đo ngoài thực địa trước tăng phạm vi công bố. |
| Power compression/matching | Chưa mô hình nhiệt/độ nhạy theo công suất, noise floor và protection timing | Nhập dữ liệu manufacturer hoặc đo độc lập; không suy đoán compression từ W danh định; bổ sung mô hình và kiểm chứng theo dữ liệu đó. |
| Điện/handbook | Assigned-phase core đã test nhưng chưa UI; chưa CB/ampacity theo điều kiện lắp đặt; handbook chưa đầy đủ | Thêm bảng tải/PF/phase và model phù hợp; CB/tiết diện cần derating, bảo vệ sự cố, cách lắp và quy chuẩn; mở rộng handbook có nguồn và test mỗi ví dụ. |
| Phát hành | Chưa installer, clean-machine/runtime/license review | Tạo package có runtime cần thiết, kiểm Windows sạch, code signing và điều khoản JUCE/ASIO/VST3 trước phân phối. |

## Scene của người dùng

File gốc không thay đổi: `C:/Users/NC/Downloads/FNBOS2024.scn`, SHA256 `405a34445b758e23c811ac5f9c1c13e7422a6e7d1fca356dce81e3395e21c30d`.

- Matrix tên Full L, Full R, Sub, Loa Bu, Mix L, Mix R.
- Output 1/2 delay ON 3,3 ms; 5/6 ON 64,4 ms; 7/8 ON 35,6 ms. Các output khác có `OFF 0.3`: 0,3 ms là giá trị lưu, không gọi là delay đang bật.
- Dynamics của 6 matrix lưu OFF. Đây không cho biết có hay không có processor/limiter ngoài mixer.
- Inventory nguyên bản và bảng routing nằm trong `artifacts/m32-scene/`; numeric source ID giữ nguyên, chưa gán tên khi thiếu xác nhận enum.

## Lịch sử — file thay đổi ở giai đoạn acoustic/engineering

| Nhóm | File |
|---|---|
| Tích hợp/build | CMakeLists.txt; src/ui/MainComponent.h/.cpp |
| Phase | src/engineering/PhaseCore.h; src/ui/PhasePanel.h/.cpp; tests/test_phase.cpp |
| Sub | src/simulation/SubArrayCore.h; src/ui/SubPanel.h/.cpp; tests/test_sub_array.cpp |
| Coverage | src/engineering/CoverageCore.h; src/ui/FrontFillPanel.h/.cpp; tests/test_coverage.cpp |
| Engineering | src/engineering/EngineeringCore.h; src/ui/EngineeringPanel.h/.cpp; src/ui/EngineeringExtended.cpp; tests/test_engineering_extended.cpp |
| Đối chứng | tools/lah_prediction_cli.cpp; tools/independent_prediction_check.py |
| Scene | tools/inspect_m32_scene.py; inventory/report trong artifacts/m32-scene |
| Tài liệu | README.md; docs/ENGINEERING_CALCULATORS.md; báo cáo phát triển, trạng thái và manifest |

Giai đoạn acoustic/engineering này không sửa lại Audio Engine. Giai đoạn native rack sau đó có tích hợp vào engine như báo cáo mới; không suy rộng câu này sang bản hiện tại. Binary baseline trước mở rộng acoustic được giữ tại `artifacts/windows-v07/baseline-before-acoustic-features/`.

## Đối chiếu các số trong ảnh chuyên gia

Các số được dùng làm tham chiếu với đầu vào/đơn vị/giả thiết đầy đủ. Đã có regression: **70 Vrms / 8 ohm = 612,5 W và 8,75 A**; **1000 W / 500 W = 2x và 3,0103 dB**; **500 W / 8 ohm / gain 32 dB = 6,2346 dBu RMS**. Peak voltage, sine-equivalent dBu và instantaneous peak watts là các định nghĩa riêng; trường hợp chưa đủ điều kiện để khớp ảnh vẫn ghi pending. Không kết luận phần mềm chuyên gia sai vì một số hiển thị chưa khớp.

Nguồn kiểm chứng: [NPL ISO absorption validation](https://resource.npl.co.uk/acoustics/techguides/absorption/validata.html), [TOA nominal coverage](https://www.toa.eu/service/soundcheck/loudspeakers-and-their-specifications). Các tài liệu nguồn khác được ghi trong kết quả từng calculator; không sao chép mã hoặc branding của phần mềm tham chiếu.

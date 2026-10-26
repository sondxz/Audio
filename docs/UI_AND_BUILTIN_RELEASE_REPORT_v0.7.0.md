# LiveAudioHost v0.7.0 — giao diện console và native processor rack

**BÁO CÁO BASELINE LỊCH SỬ — trước AI-FB/AUTO FB PRO mới.** Các số 27/27 Release, 25/25 ASan, 71 PNG và EXE/hash trong tài liệu này thuộc lần native rack trước đó, không mô tả binary hiện tại. Bản AI-FB mới đã có learned model tổng hợp và cut tự động sau ARM; xem [báo cáo cập nhật 30/30 Release, 28/28 ASan, 73 PNG](AI_FEEDBACK_IMPLEMENTATION_v0.7.0.md). Các mô tả “không AI/không tự cut” bên dưới chỉ là trạng thái baseline được giữ nguyên để đối chiếu.

Cập nhật tài liệu: **2026-10-23** (Asia/Saigon). Các lần chạy kiểm thử dưới đây thuộc phiên nâng cấp giao diện/native rack **2026-10-22**, được giữ theo log gốc; không mô tả chúng như một lần chạy phần cứng mới ngày 23. Đây là **bản LAB đã build và kiểm thử phần mềm**, chưa nghiệm thu dùng trên hệ thống PA hoặc show thật.

Đã triển khai giao diện desktop graphite/xanh ngọc mới và **15 loại bộ xử lý DSP tích hợp**, với 32 input × 4 insert nối tiếp. Đây là mã DSP gốc của dự án, không phải các file VST3 của sản phẩm trong ảnh. Có thể chỉnh và nghe qua engine của LiveAudioHost; một số yêu cầu thông minh/tự động trong danh sách ban đầu mới có phần nền tảng, như nêu rõ trong bảng dưới.

## File đã chỉnh sửa hoặc bổ sung trong giai đoạn này

| Nhóm | File |
|---|---|
| Build và cửa sổ ứng dụng | `CMakeLists.txt`; `src/app/Main.cpp`; `src/ui/MainComponent.h`; `src/ui/MainComponent.cpp` |
| Build/đóng gói | `scripts/build-windows-v07.ps1` — thêm native benchmark và bản sao báo cáo/log; kiểm cú pháp PowerShell đạt |
| Giao diện chung | `src/ui/StudioLookAndFeel.h`; `src/ui/MixerPanel.h/.cpp`; `src/ui/EffectsPanel.h/.cpp`; `src/ui/MeterBank.cpp` |
| Measurement/SPL | `src/ui/MeasurementPanel.h/.cpp`; `src/ui/SPLPanel.h/.cpp` |
| Công cụ mô phỏng/tính toán | `src/ui/SubPanel.h/.cpp`; `src/ui/EngineeringPanel.h/.cpp`; `src/ui/PhasePanel.h/.cpp`; `src/ui/FrontFillPanel.h/.cpp` |
| Native DSP và engine | `src/dsp/BuiltinRackCore.h`; `src/dsp/BuiltinSceneState.h`; `src/dsp/MixDspCore.h`; `src/audio/LiveAudioEngine.h` |
| Native rack UI | `src/ui/BuiltinRackPanel.h`; `src/ui/BuiltinRackPanel.cpp` |
| Kiểm thử | `tests/test_builtin_rack.cpp`; `tests/test_builtin_scene.cpp`; `tests/test_audio_engine.cpp` |
| Đối chứng/tải CPU | `tools/lah_builtin_cli.cpp`; `tools/independent_builtin_check.py`; `tools/lah_builtin_benchmark.cpp` |
| Tài liệu | `README.md`; báo cáo này; `docs/BUILTIN_PROCESSOR_VALIDATION.md`; `docs/FEATURE_STATUS_AND_ROADMAP_v0.7.0.md`; `docs/WINDOWS_DEVELOPMENT_REPORT_v0.7.0.md` |

Danh sách trên mô tả phạm vi giai đoạn giao diện/native rack. Danh sách tích lũy so với nguồn gốc được lưu riêng tại `docs/WINDOWS_CHANGED_FILES_v0.7.0.txt`; manifest/hash đóng gói là chứng cứ của từng lần phát hành, không thay thế kết quả kiểm thử.

## Giao diện đã triển khai

- Sidebar theo nhóm chức năng, header tên trang/trạng thái thiết bị, footer telemetry và ngăn Diagnostics có thể thu gọn; khởi động tại Mixer.
- Mixer có 8 strip mỗi bank, 4 bank cho 32 input, fader dọc có vạch dB, input/output meter lấy từ engine, gain/mute/balance và chọn output. Nút **INSERTS** mở native rack đúng input. Màn hình chi tiết giữ điều khiển DSP/routing 32×32 hiện có.
- Trang Measurement ưu tiên phase và magnitude lớn, có chọn Transfer/RTA/All, input reference/mic, snapshots/export và thông tin delay/coherence. Phase không được vẽ như đã xác minh khi chưa có time reference hợp lệ.
- Trang SPL có thẻ A/C/Z dễ đọc và chỉ số thống kê. Hiện `--` khi chưa có calibration hợp lệ; không điền số SPL từ ảnh mẫu hoặc dữ liệu giả.
- Native rack có chọn input, 4 slot theo thứ tự, thư viện 15 module, điều khiển tham số có đơn vị/range và thông tin giới hạn thuật toán. Các trang Sub, Phase, Coverage, Engineering, Effects và meter được bố trí lại theo không gian cửa sổ.
- Cửa sổ có giới hạn kích thước phù hợp màn hình; yêu cầu mở thêm instance thông thường đưa cửa sổ hiện có lên trước. Chế độ smoke-test có instance riêng để kết quả GUI không bị bỏ qua khi app đang mở.

Chưa triển khai các cửa sổ 3D vết đo, AVG EQ tự thiết kế/áp dụng filter, căn chỉnh hai loa hoặc giao diện đồ thị phổ riêng cho từng native module như ảnh tham chiếu. FIR/VST3 vẫn dùng chức năng hiện có, không coi việc đổi theme là hoàn thành rack/editor VST3 chuyên nghiệp.

## 15 native module: phần có thực và phần còn thiếu

Mỗi slot là một bộ xử lý **mono trên input**. Các slot khởi tạo `None`/bypass. Tên hiển thị là tên workflow; không tuyên bố sao chép âm thanh, thuật toán hay hiệu năng của phần mềm tham chiếu.

| Yêu cầu / tên trong app | Phần đã triển khai và kiểm thử số | Giới hạn/chưa hoàn thành |
|---|---|---|
| VOCAL CLEAN / Vocal Clean | High-pass, bell cut 250 Hz, de-ess dải cao theo envelope, peak compressor và makeup | Chưa tách giọng/tiếng ồn chồng nhau; chưa đánh giá nghe với giọng hát/mic thật; de-ess strength không phải độ giảm dB đã hiệu chuẩn |
| VOCAL COLOR / Vocal Color | High-pass, bell warmth/presence, soft saturation tanh, wet/output | Chưa đối chiếu phần cứng analog hoặc chất lượng cảm nhận |
| AI-FB / Feedback Guard | Bell cut hẹp chỉnh tay, high-pass, sample clip guard, wet mix | **Không có AI, không có detector feedback**; chưa hoàn thành yêu cầu AI-FB |
| AUTO FB PRO / Auto Feedback (advisory) | Gợi ý tần số tone tuần hoàn bền dựa trên zero-crossing/persistence; bell cut do người vận hành đặt | **Không tự áp dụng cut**; nhạc kéo dài có thể kích hoạt gợi ý; chưa xác minh false positive hoặc ổn định chống hú |
| EQ / Equalizer | High-pass, bell low/mid/high; mid frequency/gain/Q | Chưa editor đáp tuyến kéo thả hoặc rack EQ nhiều band tùy ý |
| SMART EQ / Smart EQ | Giảm dải low/high theo tỷ lệ năng lượng, presence EQ và output | Heuristic có giới hạn; không ML, không nhận diện nguồn, không target matching/room correction |
| DYNAMICS / Dynamics | Peak compressor feed-forward, downward expander, attack/release và makeup | Không true-peak limiter, không mô hình nhiệt/bảo vệ loa |
| SMART COMP / Smart Compressor | RMS detector, release theo mức, parallel wet và makeup | Không AI; chưa xác minh hành vi mọi nội dung/chất lượng nghe |
| OPTICAL COMP / Optical Compressor | Hai peak envelope có release khác nhau, gain law và saturation tùy chọn | Mô hình toán, chưa đo/khớp optocoupler hoặc thiết bị quang học cụ thể |
| FX / FX Delay | Mono feedback delay, damping, saturation tùy chọn, dry/wet | Chưa bộ FX đa hiệu ứng/stereo hoặc đồng bộ tempo |
| FX DIGITAL / Digital FX | Bit quantization, sample hold và feedback echo ngắn | Chưa bộ spatial FX/reverb theo ảnh tham chiếu |
| REVERB 96 / Reverb 96 | 4 damped comb + 2 allpass Schroeder, predelay, decay, room scale | Mono; “96” là tên, không phải cam kết 96 kHz nội bộ; chưa đánh giá diffusion/metallic tail bằng nghe |
| SUB PRO / Sub Pro | High-pass, 2 LP2 nối tiếp, bell 55 Hz, polarity, saturation/output | Low-pass tổng -6,0206 dB tại cutoff; chưa matching loa/amp hoặc protection thực |
| DRUM LIVE / Drum Live | High-pass, body/attack EQ, envelope gate, transient emphasis | Chưa multiband drum rack, bleed separation hay kiểm bộ trống thật |
| PC AUDIO / PC Audio | High-pass, bass/treble EQ, peak compressor, sample clip guard | Xử lý input đã route vào host; **chưa có tính năng mới capture toàn bộ âm thanh Windows/loopback** |

Định nghĩa công thức, tham số, phương pháp đối chứng và dung sai nằm trong [BUILTIN_PROCESSOR_VALIDATION.md](BUILTIN_PROCESSOR_VALIDATION.md). Các ca kiểm số chỉ xác minh phần thuật toán được nêu; không chứng minh tác dụng chống hú, chất lượng giọng hát hoặc hiệu quả bảo vệ phần cứng.

## Tích hợp engine, scene và realtime

Native rack chạy sau HPF/PEQ/gate/compressor của input và trước FX send/matrix routing. Test callback JUCE xác nhận xử lý input đã chọn, input khác không bị ảnh hưởng, bypass, echo đúng vị trí mẫu và xóa tail khi mute/PANIC.

- `prepare()` cấp phát delay/history trước callback. Kiểm thử bắt allocation cho `beginBlock`, xử lý sample và reset không phát hiện cấp phát trong phạm vi rack được test. Không suy rộng kết quả này sang driver hoặc plugin VST3 bên thứ ba.
- Reset global/channel dùng epoch O(1) để bỏ lịch sử. Input mute giữ output zero kể cả khi slot có tail; chuyển mute/bật lại nhanh không phát lại tail cũ trong ca regression.
- Bypass giữ sample hữu hạn bit-exact sau transition 64 mẫu; NaN/Inf chuyển zero. Guard biên độ sample của module không phải giới hạn true peak hay mức SPL.
- UI chỉ cho đổi loại module, tham số và reset khi audio đang **MUTE**, vì chưa có smoothing đầy đủ cho mọi thay đổi hệ số. Enable/bypass có transition; chưa nghiệm thu automation liên tục trong show.
- Scene XML của LiveAudioHost lưu/khôi phục 128 slot và kiểm giới hạn/giá trị hữu hạn. Scene cũ không có native rack sẽ xóa rack về bypass. Đây **không phải** importer scene `.scn` M32R.
- Rack cấp phát vùng delay tối đa cho cả 128 slot, kể cả khi bypass: khoảng **48 MB tại 48 kHz**, khoảng **192 MB tại 192 kHz**, chưa gồm JUCE/engine/GUI/VST3 và overhead. Đây là giới hạn thiết kế hiện tại; chưa tối ưu pool theo module đang dùng.

Core có kiểm biên sample rate đến 192 kHz; đối chứng tham chiếu độc lập của 15 module chạy tại **44,1/48/96 kHz**. Không gọi đó là toàn bộ ma trận thiết bị/sample rate đã nghiệm thu.

## Kết quả build và kiểm thử thực tế

Windows x64 Release, Visual Studio 2022/MSVC 19.44, CMake, JUCE 8.0.15; ASIO biên dịch bật. Build đã sửa lỗi kiểu rectangle của display bounds trong JUCE, build cuối và GUI smoke lại đạt.

EXE đóng gói: `release-v07/LiveAudioHost.exe`, **7.626.752 byte**, SHA256 `bc843b18d62dff5d83a7665e17561f7b7646b107936bc4f2b9d96a27502b78d7`. Đã mở qua desktop và quan sát mixer thực, output MUTE. Trạng thái thiết bị Windows Audio quan sát lúc mở là 44,1 kHz / 480 mẫu và vẫn báo **1 XRun**; việc mở được cửa sổ không xóa trạng thái lỗi audio hoặc chứng minh audio đã nghiệm thu.

| Kiểm tra | Kết quả quan sát | Phạm vi |
|---|---|---|
| CTest Windows Release | **27/27 PASS**, 39,15 giây | Gồm core, đối chứng, callback engine tổng hợp và native GUI smoke |
| MSVC AddressSanitizer core | **25/25 PASS**, 49,26 giây | Core/CLI; không instrument toàn JUCE GUI hoặc plugin bên thứ ba |
| Native DSP độc lập | **402 render cases PASS** | 87 vector thuật toán/tham chiếu + 315 state/bypass tại 3 sample rate; Python/NumPy/SciPy tạo reference độc lập |
| Native GUI smoke cuối | **71 PNG, 0 bounds warnings**, chạy lại riêng PASS 11,22 giây | Layout 1350×850 và 1150×720, các trang/chế độ; audio device đóng, 0 callback, output mute |
| Native scene/rack | PASS trong CTest | Roundtrip 128 slot, invalid state, legacy clear, ordering, channel isolation, reset/bypass/transition, allocation và biên sample rate |
| Callback native engine | PASS trong `lah_audio_engine` | Dynamics, bypass, echo 10 ms, input mute và PANIC/rearm tổng hợp |

Log: `artifacts/windows-v07/console-native-build.log`, `console-native-ctest.log`, `console-native-asan.log`, `ui-final-build.log`, `ui-final-ctest.log`; GUI report: `build-windows-v07/ui-smoke/smoke-report.txt`. GUI smoke không cung cấp calibration hoặc con số SPL. Test numerics không thay thế phép nghe/đo vật lý.

## CPU: tải thông thường đã quan sát, tải stress chưa đạt budget

Benchmark offline `lah_builtin_benchmark`, mixer + native rack, 48 kHz / 128 mẫu, routing 32 input/output 1:1. Chu kỳ callback là **2.666,67 µs**. Mỗi cấu hình có 50 block warm-up và 300 block tính thời gian; không mở driver, không gồm VST3/ADC/DAC.

| Cấu hình | p50 | p99 | Max |
|---|---:|---:|---:|
| 32 kênh, rack bypass | 151,9 µs | 202,6 µs | 248,5 µs |
| 8 kênh × 2 slot Vocal Clean + Dynamics | 190,3 µs | 267,6 µs | 340,1 µs |
| Stress: 32 kênh × 4 Reverb 96 | 1.249,7 µs | **4.179,6 µs** | **5.280,4 µs** |

Log gốc: `artifacts/windows-v07/native-benchmark.log`. **128 reverb đồng thời vượt budget buffer 128 mẫu** trong lần đo này; không chấp nhận cấu hình đó cho realtime. Cần tối ưu xử lý filter/delay và quản lý số instance đang hoạt động, kiểm profiling/WCET, sau đó đo lại có driver/GUI trước xác định giới hạn vận hành. Số p50 hoặc kết quả tải nhẹ không phải chứng cứ không dropout.

## Lỗi còn tồn tại và phạm vi chưa xác minh

- Hai lần chạy WASAPI mute khoảng 10 giây ở giai đoạn trước đều báo **1 XRun**. Chưa xác định nguyên nhân, chưa có phép chạy audio thực mới đủ để kết luận đã sửa. Cần timestamp discontinuity, thử driver/buffer, tải native DSP và chạy dài hạn.
- Chỉ có M32-Edit trên máy, **chưa nối M32R Live**. Chưa xác nhận ASIO card/driver/channel map, routing send/return, round-trip/loopback, hotplug hoặc soak 24 giờ.
- Chưa có mic/calibrator thật và reference meter: chưa nghiệm thu SPL phần cứng, uncertainty hoặc IEC 61672; không công bố độ chính xác âm học.
- Chưa hoàn thành FOH/BOH bus/group/aux/monitor/PFL/AFL/solo/DCA; native rack input không thay thế graph bus đầy đủ.
- VST3 vẫn thiếu multi-rack/editor/state chuyên nghiệp và crash/hang isolation; PDC vẫn tĩnh cho tuyến insert hiện có, chưa graph bus/parallel. Native processor tích hợp không giải quyết các giới hạn VST3 này.
- FIR dài đa output/partitioned convolution, acoustic room/ground/reflection/material/polar 3D, power compression/mô hình nhiệt/protection loa, điện/handbook đầy đủ và clean-machine installer vẫn chưa hoàn thành.
- Chưa có điều khiển tham số realtime mượt cho toàn rack, chưa kiểm mọi tổ hợp 128 slot hoặc mọi tham số bằng reference, chưa kiểm âm thanh bằng nghe với nội dung đa dạng. Feedback advisory chưa được phép gọi là hệ thống chống hú tự động đã hoàn thiện.

## Cách dùng bản đã nâng cấp

Mở `release-v07/LiveAudioHost.exe`, chọn **Mixer → INSERTS** trên input cần xử lý (hoặc trang native rack). Khi audio đang MUTE, chọn slot và processor, chỉnh tham số rồi enable slot. Kiểm đúng input/output và mức gain trước khi người vận hành chủ động bật audio. Bộ xử lý vừa nạp luôn bắt đầu bypass; mặc định không có module nào tự phát hoặc tự mở audio.

Khi nghiệm thu phần cứng, tiếp tục theo [Windows/ASIO acceptance](WINDOWS_ASIO_ACCEPTANCE_v07.md) và [trạng thái/roadmap](FEATURE_STATUS_AND_ROADMAP_v0.7.0.md). Không đánh dấu hoàn thành nghiệm thu live từ các log offline ở trên.

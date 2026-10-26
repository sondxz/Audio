# LiveAudioHost v0.7.0 — báo cáo phát triển Windows, cập nhật 2026-10-23

**Cập nhật mới nhất — AI-FB LAB/AUTO FB PRO:** Release **30/30** (29,73 giây), MSVC ASan core **28/28** (48,39 giây) đạt; GUI smoke **73 PNG/0 bounds warnings**; **402 native + 30 feedback render cases** và model parity **25 clip/495 frame** đối chứng độc lập. Giữ giao diện console/native rack 15 loại, 32 input × 4 slot. Xem [triển khai và kết quả AI/feedback](AI_FEEDBACK_IMPLEMENTATION_v0.7.0.md), [model card](FEEDBACK_MODEL_CARD.md) và [trạng thái/roadmap](FEATURE_STATUS_AND_ROADMAP_v0.7.0.md).

AI-FB LAB dùng mô hình neural đã huấn luyện trên tín hiệu tổng hợp, AUTO FB PRO dùng spectral/growth detector; **chỉ tạo cut mới sau ARM**. DISARM giữ cut hiện có, Clear bỏ cut. Model score không phải xác suất acoustic đã hiệu chuẩn; chưa nghiệm thu giọng hát/nhạc/PA thật. Tham số/loại module hiện chỉnh khi MUTE. Benchmark cuối 32 AUTO FB, 128 AI-FB và 128 Reverb có max vượt chu kỳ 128 mẫu; không cam kết realtime cho các cấu hình đó. Giữ cả log nhanh và chậm để thấy biến thiên. Trạng thái XRun/WASAPI, ASIO/M32R và SPL phần cứng vẫn chưa đạt/chưa xác minh.

**Baseline console/native rack trước AI:** [báo cáo 27/27 Release, 25/25 ASan, 71 PNG](UI_AND_BUILTIN_RELEASE_REPORT_v0.7.0.md) được giữ làm lịch sử; mô tả Feedback Guard/Auto Feedback trong báo cáo đó không phải phiên bản AI/automatic mới.

**Lịch sử mở rộng acoustic/engineering (2026-10-22):** 24/24 Release, 22/22 ASan core, 54 PNG/0 bounds; thêm 180 đối chứng NumPy phase/sub và 5.000 coverage comparisons. Các mục giai đoạn 1–4 bên dưới ghi mốc baseline trước mở rộng (19/19, 17/17), được giữ làm lịch sử; dùng báo cáo mới ở trên khi đánh giá binary console/native rack hiện tại.

Trạng thái: **Windows build / kiểm thử tổng hợp; LAB ONLY, chưa nghiệm thu hệ thống âm thanh thực**.

Báo cáo này cập nhật trạng thái Windows/PDC trong các báo cáo source-only v0.3–v0.7 trước đây. Các báo cáo cũ được giữ làm lịch sử. Kết quả với tín hiệu số tổng hợp không phải số đo SPL, latency vật lý, dropout ASIO hay chứng nhận vận hành show.

## Lịch sử — Giai đoạn 1: đọc và kiểm tra nguồn

Đã đọc CMakeLists.txt, README, toàn bộ src/tests/tools/scripts và tài liệu kỹ thuật của dự án trước khi sửa; công việc được chia thành audit Audio Engine, DSP/measurement và UI. Workspace ban đầu là source giải nén, không có `.git`.

Các lỗi tìm thấy và đã xử lý bằng thay đổi tập trung:

- JUCE 8.0.15 yêu cầu C compiler; project ban đầu chỉ bật CXX.
- `juceaide` đọc argv dạng UTF-8 nhưng Windows truyền đường dẫn theo ANSI code page: resource build lỗi `Unhandled exception` tại đường dẫn tiếng Việt. Hook CMake hiện dùng tham số resource tương đối, giữ nguyên SDK JUCE.
- VST3 có thể vượt output ceiling vì guard chỉ nằm trước insert; guard/meter cuối hiện ở sau FIR/VST/PDC.
- FIR và plugin có thể giữ đuôi âm thanh qua PANIC. Có invalidation/reset và mute generation cho cả trường hợp mute/bật lại giữa hai callback.
- Hai tab MIC dùng chung nguồn capture nhưng không đồng bộ hiển thị/hiệu chuẩn; đã đồng bộ và yêu cầu cửa sổ capture mới.
- A/B CSV dùng tần số của sample rate hiện tại cho snapshot cũ; nay mỗi snapshot có cột tần số riêng.
- NaN/Inf trong timebase, FIR và SPL statistics có thể làm sai kết quả hoặc cập nhật trạng thái dở. Đã thêm kiểm tra và regression.
- Capture chỉ kiểm tra committed count, chưa phát hiện ghi đè đang diễn ra; thêm write epoch/fences, từ chối snapshot chồng với producer.
- Calibration chỉ kiểm tra độ thuần 1 kHz ở đoạn đầu nhưng tính RMS toàn capture; nay kiểm tra các cửa sổ toàn đoạn và độ ổn định mức.
- `std::polar` được gọi với bán kính âm khi đảo polarity; nay áp dấu lên số phức có bán kính dương.
- GUI narrow-string literals không phải Unicode hợp lệ theo API JUCE: đã chuyển đổi UTF-8 tường minh.
- Log QA hai dòng cách nhau 24 giờ có thể được chấp nhận; nay kiểm tra tính liên tục, timestamp, tiến độ callback và tính hữu hạn của số liệu.

File thay đổi được kê đầy đủ trong `docs/WINDOWS_CHANGED_FILES_v0.7.0.txt`; manifest nguồn gốc vẫn được giữ nguyên để đối chiếu.

## Lịch sử — Giai đoạn 2: build Windows

Môi trường thực tế:

- Windows 11 Pro x64, version 10.0.26100.
- Visual Studio 2022 Community 17.14, MSVC 19.44.35222, Windows SDK 10.0.26100.0.
- CMake 3.31.6, generator Visual Studio 17 2022, x64, Release, C++17.
- JUCE 8.0.15; `LAH_ENABLE_ASIO=ON`, `JUCE_PLUGINHOST_VST3=1`.
- Python 3.12, NumPy 2.2.6, SciPy 1.14.1 chỉ dùng cho QA.
- Source JUCE ZIP SHA256: `72838128c730eba22c63dfdc8bf155304887a9f4e9d9f61ec569c7f8e4c3fab1`.

Ban đầu máy thiếu C++ headers/libraries và Windows SDK dù có cl.exe. Đã bổ sung các thành phần MSVC/SDK/CMake và ASan bằng Visual Studio Installer sau khi được cấp quyền. CMake/Ninja/SciPy và JUCE nằm trong `.tools`, không nằm trong EXE.

Build lại từ PowerShell thông thường:

```powershell
.\scripts\build-windows-v07.ps1 -Generator 'Visual Studio 17 2022' `
  -EnableSciPy -PythonExecutable 'duong-dan-toi-python.exe'
```

Script hỗ trợ CMake/JUCE cục bộ trong `.tools`, chọn Release cho generator nhiều cấu hình, lưu log và chỉ đóng gói khi build/CTest thành công. Không cài driver, không tự bật audio.

EXE build: `build-windows-v07/LiveAudioHost_artefacts/Release/LiveAudioHost.exe`.
EXE đóng gói: `release-v07/LiveAudioHost.exe`, checksum trong `release-v07/SHA256.txt`.

PE imports xác nhận EXE dùng VC++ x64 runtime (`MSVCP140.dll`, `VCRUNTIME140.dll`, `VCRUNTIME140_1.dll`). Đã chạy trên máy build hiện tại; chưa nghiệm thu trên một máy Windows sạch/installer.

## Lịch sử — Giai đoạn 3: Audio Engine và DSP

Đã triển khai/test tổng hợp:

- Mặc định/restart/error giữ mute; frame/rate không hợp lệ bị từ chối; output ngoài 32 kênh được đưa về zero.
- Khối VST không vượt block size đã prepare; scratch được cấp phát trước; xử lý denormal bị tắt trong callback.
- PANIC xóa hiệu lực lịch sử mixer delay/FX, FIR và PDC; không tái sử dụng đuôi plugin trước khi prepare/reset ngoài callback.
- Bắt C++ exception trong plugin processing/lifecycle và scan/load, giữ trạng thái mute/bypass. **Không cô lập access violation, hang, memory corruption hoặc destructor vi phạm noexcept**.
- Bù trễ tĩnh single stereo insert: OUT3–32 nhận độ trễ bằng reported VST3 latency + group delay FIR tuyến tính OUT1/2. Giới hạn 65.536 mẫu; lịch sử khoảng 7,5 MiB, cấp phát trước. Speaker delays có chủ ý vẫn cộng thêm. Plugin đổi reported latency khi chạy khiến block bị mute và yêu cầu rearm.
- Test impulse kiểm tra plugin 64 mẫu, FIR 64 mẫu, tổ hợp 128 mẫu, variable blocks và null channels. Đây là alignment số; không đo được latency mà plugin báo sai, ADC/DAC hoặc USB.
- Không có file I/O/FFT/GUI/khóa chặn trong callback của host; FFT/TF và SPL vẫn chạy worker. Heap/driver profiling thực tế của toàn ứng dụng và plugin bên thứ ba chưa hoàn tất.

Đối chứng độc lập tự động:

| Thuật toán | Dữ liệu/tham chiếu | Phạm vi |
|---|---|---|
| H1/phase/coherence/RTA | SciPy Welch/CSD, 3 ca gain/delay dương/âm, đảo polarity, đáp tuyến và noise độc lập | Tín hiệu số tổng hợp |
| FIR LP/HP | SciPy firwin, 30 cấu hình, 44.1/48/96 kHz, gồm 129/257 taps realtime | Coefficients và convolution số |
| HPF/PEQ | SciPy Butterworth/SOS, analog bell prototype + bilinear, impulse/anchor | 3 sample rates |
| Compressor/gate | Nghiệm exponential độc lập, ngưỡng/ratio/attack/release/plateau | Dynamics cơ bản hiện có |
| Routing/output delay/polarity | Tổng tuyến tính và dịch mẫu độc lập | Signed matrix, OUT32, delay 1/17 mẫu |
| Delay/reverb | Chuỗi xung feedback/comb tính trực tiếp | Timing/gain; không đánh giá chất lượng nghe |
| SPL/engineering/simulation | Tone tổng hợp và các giá trị/quan hệ giải tích trong C++ suites | Không thay thế hiệu chuẩn/đo thực |

21 ca đối chứng DSP ở 3 sample rates đã chạy đạt. Sai khác FIR lớn nhất quan sát khoảng `3.153e-14` trên coefficients double; con số này không phải độ chính xác âm học.

MSVC AddressSanitizer phát hiện stack overflow ở hai test chứa nhiều Result lớn trong stack 1 MiB. Đã chuyển fixtures lớn sang heap qua helper noinline, không tăng `/STACK`; FFT workspaces của analyzer cũng ở heap ngoài realtime. Full core ASan cuối: **17/17 PASS**. ASan này chưa instrument toàn GUI/JUCE/third-party plugin và không phải ThreadSanitizer.

Log chính: `artifacts/windows-v07/build-final.log`, `ctest.log`, `asan-build.log`, `asan-ctest.log`, `benchmark.log`. Benchmark chỉ là offline synthetic, không phải ASIO deadline/roundtrip evidence.

Kết quả cuối ghi nhận: **Release 19/19 CTest PASS** (bao gồm callback engine và GUI), **ASan core 17/17 PASS**, **15/15 Python QA cases PASS**. Benchmark dense 32×32 + stereo FIR 257 taps + PDC, 48 kHz/128 frames: p50 191,1 µs, p99 376,9 µs, max 1.467,6 µs so với period 2.666,67 µs. Đây là mẫu quan sát offline trên máy này, không phải WCET hoặc dự báo latency/dropout.

Tái chạy ASan bằng `scripts/test-asan-windows.ps1 -EnableSciPy -PythonExecutable <path>` sau khi có thành phần MSVC AddressSanitizer.

## Lịch sử — Giai đoạn 4: GUI desktop

Giữ theme graphite/teal. Các panel giữ bố cục hiện có, thêm viewport để truy cập controls ở cửa sổ nhỏ. Chế độ `--smoke-test <folder>` tạo 20 PNG của 10 tab ở hai kích thước, kiểm tra Unicode, bounds và trạng thái audio đóng/mute/zero callback. Không cung cấp dữ liệu SPL giả.

`--inspect-audio <file.json>` kê khai backend qua chính JUCE, không mở stream. Presence của ASIO/device không xác minh driver có thể chạy audio.

Kết quả render/CTest cuối và backend inventory được ghi cùng bản đóng gói; xem các log/PNG trực tiếp thay vì coi khởi tạo component là nghiệm thu thao tác toàn bộ ứng dụng.

Render cuối: **20/20 PNG**, **0 bounds warnings**, Unicode đúng, audio đóng/mute/zero callback. Checker dùng bounds đã áp affine transform của JUCE Drawable, tránh nhầm artwork nội bộ của nút tràn tab thành control bị cắt. Inventory thực từ EXE có Windows Audio/WASAPI shared/exclusive/low-latency, DirectSound và ASIO; ASIO kê khai M-Audio AIR 192|4, không có M32R. `audio_device_opened=false` trong file JSON.

## Lịch sử — Giai đoạn 5: quan sát driver Windows thực, lỗi vẫn chưa xử lý xong

Thêm `--muted-stream-check <1..30 giây> <file.json>`: chỉ cho phép backend WASAPI shared mặc định, luôn mute, không lưu waveform/settings, detach/close trước khi thoát. Lệnh này được gọi riêng với quyền rõ ràng; không chạy tự động trong CTest.

Đã chạy **hai lần 10 giây thực tế**, Mopad X3 input → Speakers output, 44.1 kHz, 480 frames, 2 input/2 output:

| Quan sát | Lần 1 | Lần chẩn đoán |
|---|---:|---:|
| Thời gian quan sát | 10,092 s | 10,075 s |
| Callback | 928 | 926 |
| Maximum callback | 0,0439 ms | 0,098 ms |
| Deadline overruns / device errors | 0 / 0 | 0 / 0 |
| Output peak theo engine / mute | 0 / giữ mute | 0 / giữ mute |
| Device/JUCE XRun counter cuối | 1 / 1 | 1 / 1 |
| XRun ban đầu → cuối | Chưa thu baseline | 0 → 1 |
| Tiêu chí quan sát | **KHÔNG ĐẠT** | **KHÔNG ĐẠT** |

Log nguyên bản: `artifacts/windows-v07/muted-wasapi-stream.json` và `muted-wasapi-diagnostic.json`. Giữ nguyên exit code 1 và `observation_criteria_met=false`; không trừ XRun để biến thành PASS.

XRun là blocker chưa xác định nguyên nhân. Source JUCE 8.0.15 WASAPI đếm input `AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY`; không đủ bằng chứng để quy nó cho DSP quá tải, kết luận chỉ do startup hoặc suy ra output dropout vật lý. Microsoft mô tả flag này có thể liên quan stream state transition hoặc timing glitch. Cần kiểm tra driver/clock và đo/trace độc lập trước khi chấp nhận audio thực. [Microsoft buffer flags](https://learn.microsoft.com/windows/win32/api/audioclient/ne-audioclient-_audclnt_bufferflags).

**19/19 kiểm thử tổng hợp đạt không làm bài driver thực trên đây trở thành đạt.** ASIO/M32R và workload đã bật DSP vẫn chưa được nghiệm thu.

## Trạng thái từng module và việc chưa xác minh

| Module | Đã có và đã test trong giai đoạn này | Còn thiếu/chưa xác minh |
|---|---|---|
| Windows app | EXE MSVC/JUCE build; render native | Phiên vận hành tương tác dài hạn, installer/signing |
| Audio Engine | Callback JUCE thật với device/processor giả; WASAPI mute thực có callback nhưng bài quan sát KHÔNG ĐẠT do XRun | XRun WASAPI chưa rõ nguyên nhân, ASIO thực, latency/dropout, heap profiling, 24h soak |
| Digital Mixer/Routing | 32×32 DSP, mute/balance/gain/output delay/polarity; đối chứng số | M32 physical slot mapping, buses/groups/PFL/AFL/DCA/solo chuyên nghiệp |
| DSP/FX | HPF/PEQ/dynamics/clip guard/delay/basic comb reverb và reference tests | Filter smoothing, crossover rack, true-peak/lookahead limiter, chất lượng reverb |
| VST3 | Format host build, stereo insert, block bounds, reset, C++ exception handling, reported-latency PDC | Plugin thật theo tên/version, rack/editor/state, process isolation/hang protection |
| FIR | Offline LP/HP, realtime 129/257 taps OUT1/2, PDC và PANIC tests | Partitioned long FIR, arbitrary measured correction, per-output FIR, loopback vật lý |
| FFT/RTA/TF/Phase/Coherence | Lõi H1/Hann/averaging/delay compensation, worker, CSV/A-B; SciPy tests | Venue/reference-app comparison, clock drift, real multi-mic/capture mapping |
| Delay Finder | GCC-PHAT linear, confidence, tonal rejection, signed/long delay tests | Dữ liệu phòng thật/low SNR/drift; không phải phép đo roundtrip mặc định |
| SPL | Calibration/input freshness, finite/clipping/purity gates, provisional A/C/Z/Leq/block F/S | Mic/calibrator/reference meter thật, IEC weighting/tolerances/uncertainty budget |
| Acoustic Simulation | Free-field monopoles, relative complex pressure, polar CSV, nominal coverage geometry | Room reflections/materials/ground, measured 3D directivity/GLL, absolute calibrated SPL |

Không module nào ở cột cuối được đánh dấu hoàn thành. Ứng dụng vẫn **LAB ONLY / NOT SHOW READY**.

## Nghiệm thu Midas M32R Live còn bắt buộc

Inventory Windows trong phiên này chỉ thấy registry ASIO M-Audio AIR 192|4; chưa phát hiện/stream được Midas M32R Live. Không tự cài driver âm thanh hoặc sửa routing console.

Thực hiện phiếu `WINDOWS_ASIO_ACCEPTANCE_v07.md`: driver/firmware/card/USB/clock; mapping từng callback slot bằng loopback điện mức thấp; 48 kHz và buffer thực hỗ trợ; measured wired roundtrip độc lập; full 32×32; PANIC/reconnect/rate change; impulse PDC; plugin đã xác định; mic/calibrator/SPL reference; soak 24 giờ với log liên tục và memory/dropout counters. Mọi kết quả vật lý hiện vẫn **CHƯA TEST**.

Sau khi có dữ liệu này mới quyết định phạm vi chạy qua PA. Bản build/đối chứng tổng hợp hiện tại không chứng minh độ tin cậy FOH/BOH hoặc độ chính xác SPL.

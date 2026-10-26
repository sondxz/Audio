# LiveAudioHost v0.7.0 — AI-FB LAB và AUTO FB PRO

Ngày cập nhật: **2026-10-23**, Asia/Saigon.

**Bản tích hợp Windows đã build: 30/30 CTest Release, 28/28 ASan core PASS; GUI smoke 73 PNG, 0 bounds warnings.** Đây là kết quả phần mềm/tín hiệu tổng hợp, chưa nghiệm thu acoustic feedback trên mic/PA thật. Các con số 27/27 và 25/25 của native rack trước đó là baseline lịch sử.

## Phạm vi thay đổi

Hai module giữ ID scene cũ: `FeedbackGuard = 3` nay hiển thị **AI-FB LAB**, `AutoFeedback = 4` nay hiển thị **AUTO FB PRO**. Các input vẫn có bốn insert mono nối tiếp; tổng 32 input. Giữ nguyên 13 loại xử lý khác và các tham số manual của hai module feedback.

| Module | Phần xử lý mới | Điều kiện áp dụng tự động |
| --- | --- | --- |
| AI-FB LAB | Mô hình neural nhỏ đã huấn luyện bằng dữ liệu tổng hợp, kết hợp spectral peaks, mức tăng và độ bền của tone; ngân hàng bell cut hẹp | Slot enabled, người vận hành ARM, tone qua các gate phổ/thời gian và model score đạt ngưỡng |
| AUTO FB PRO | Spectral peaks, mức tăng và độ bền của tone; ngân hàng bell cut hẹp | Slot enabled, người vận hành ARM và tone qua các gate phổ/thời gian; không dùng mô hình AI |

Đây là phân tích **acoustic feedback/tiếng hú**, không phải nhận diện hoặc điều khiển receiver micro không dây. Bộ xử lý không có liên kết điều khiển M32-Edit/mixer.

Mô hình có trọng số được học thật; không gọi các luật ngưỡng FFT là AI. Dữ liệu huấn luyện chỉ là mô phỏng, chưa có tập thu PA/micro thật để xác nhận khả năng phân biệt tiếng hú và nhạc trong vận hành.

## Luồng tín hiệu và phát hiện

Luồng input đi qua phần manual hiện có, sau đó qua các automatic cuts của slot. Bộ phân tích lấy tín hiệu dư sau những cut đã áp dụng. Manual EQ/HPF/clip guard vẫn xử lý khi slot enabled, kể cả lúc automatic cuts DISARM. Muốn bỏ toàn bộ xử lý của slot phải dùng **Bypass slot**.

- Phân tích được downsample về không quá 24 kHz, qua hai section low-pass chống alias. FFT 1024 mẫu, Hann, hop 256 mẫu; lịch phân tích được lệch giữa các slot để giảm việc nhiều FFT trùng một mẫu.
- Tìm tối đa bốn peak mỗi frame, theo dõi tối đa tám ứng viên. Miền tìm kiếm đặt khoảng 80 Hz đến giới hạn thấp hơn giữa 8 kHz và miền phổ phân tích; độ phân giải FFT và bộ lọc vẫn giới hạn khả năng phát hiện ở biên.
- Gate gồm năng lượng tập trung, độ nổi bật so với lân cận, mức tối thiểu, harmonic ratio, ổn định tần số, tăng mức và persistence. Một tone ổn định không tăng mức không phải lý do đủ để tự cắt.
- AI-FB yêu cầu thêm model score. Score là kết quả sigmoid của mô hình, **không phải xác suất tiếng hú đã hiệu chuẩn ngoài thực tế**.
- Tối đa sáu cut, mỗi cut 3–12 dB và tổng độ sâu tối đa 36 dB; tần số gần cut hiện có không tạo thêm một cut trùng. Với cut 12 dB, budget tổng chỉ đủ ba cut. Đây là bell cut hữu hạn, không phải notch lý tưởng có độ sâu vô hạn.
- Cut mới có transition 64 mẫu. Clear bỏ correction trong 64 mẫu; reset/PANIC xóa lịch sử trong tuyến đã mute. Regression xác nhận đầu ra hữu hạn và trở về dry sau Clear, không phải đánh giá nghe mọi thời điểm chuyển.

FFT được dùng cho detector; audio vẫn xử lý theo từng mẫu qua IIR và không chờ gom đủ frame FFT để phát ra. Thời gian chờ phát hiện/cắt khác với độ trễ tuyến audio. Persistence, thời gian lấp frame, growth và model gate có thể làm detector phản ứng chậm hoặc không tạo cut. Độ trễ round-trip phần cứng vẫn chưa đo.

## Mô hình và nguồn dữ liệu

`FeedbackModelWeights.h` khai báo model `synthetic-mlp12-v1`: 12 đặc trưng → 12 neuron tanh → một sigmoid. Đặc trưng tính theo frame khoảng 32 ms gồm mức, crest factor, kurtosis, zero-crossing/periodicity, tương quan lag, năng lượng sai phân và tăng mức theo thời gian. Chuẩn hóa dùng thống kê tập train và giới hạn giá trị chuẩn hóa.

`tools/train_feedback_model.py` sinh dữ liệu với seed cố định, tách train/validation/test theo các đường truyền delay/Q khác nhau, huấn luyện và xuất trọng số C++ cùng manifest. Các nhóm có closed-loop tổng hợp, stable loop, tone, harmonic, music-like, speech-like, noise, percussive, silence và sweep. Có một tập tone nhạc tăng mức gây nhập nhằng được báo cáo riêng; không được bỏ tập này để chỉ trình bày chỉ số thuận lợi.

Manifest tham chiếu: `artifacts/feedback-model/training-report.json`; dataset SHA256 được nhúng trong header. Các frame trong cùng clip có tương quan, vì vậy số frame không được trình bày như số phép thử độc lập. Các tỷ lệ synthetic không phải tỷ lệ lỗi trên mọi giọng hát hoặc hệ thống PA.

Ở ngưỡng score 0,9, báo cáo model ghi held-out synthetic test recall **84,38%**, false positive trên negative frames **0,169%**; riêng tone nhạc tăng mức nhập nhằng có **35,14% false-positive frames**. Đây là chỉ số **classifier theo frame**, không phải tỷ lệ phát hiện/cắt sai của toàn controller. Xem [model card](FEEDBACK_MODEL_CARD.md) để biết tập dữ liệu, nhãn, ma trận nhầm lẫn và giới hạn.

Có thể tái sinh mà không ghi đè header đang dùng:

```text
python tools/train_feedback_model.py --output artifacts/feedback-model/reproduction --header artifacts/feedback-model/reproduction/FeedbackModelWeights.h
```

Python/NumPy dùng cho training/QA offline; audio runtime sử dụng trọng số C++ nhúng và không gọi Python, mạng hoặc dịch vụ AI bên ngoài.

## Điều khiển và trạng thái trong giao diện

Vào **Mixer → INSERTS**, hoặc trang **Built-in processors**. Khi audio MUTE, chọn input/slot, chọn AI-FB LAB hoặc AUTO FB PRO, chỉnh tham số manual và enable slot. Module vừa nạp bắt đầu bypass; không có thao tác nào tự bật global audio.

Người vận hành chủ động bật audio sau khi xác minh routing và mức gain. Detector chạy ở chế độ monitor khi chưa ARM. Nút **ARM AUTO CUT** chỉ khả dụng khi slot enabled, thiết bị chạy và audio đang passing.

| Thao tác | Ý nghĩa |
| --- | --- |
| ARM AUTO CUT | Cho phép tạo cut mới khi detector đáp ứng điều kiện; không cam kết loại được tiếng hú |
| DISARM AUTO CUT | Dừng việc tạo cut mới; **giữ các filter đã cắt** |
| Clear notches | Xóa automatic cuts và trạng thái detector qua yêu cầu gửi audio thread; giữ tham số manual |
| Bypass slot | Bỏ toàn bộ xử lý slot, đồng thời disarm |
| MUTE / PANIC, reset stream hoặc recall scene | Không khôi phục trạng thái armed; lịch sử feedback phải được vô hiệu theo cơ chế reset của engine |

Card Feedback Control hiển thị candidate Hz, model score trên AI-FB, số cut/6 và tần số/độ sâu từng cut. `--` là chưa có candidate hợp lệ, không được thay bằng dữ liệu giả. UI có dòng LAB ngắn và thông báo DISARM giữ cut. Candidate cũng không phải phép đo acoustic đã được xác nhận.

Manual depth mặc định mới của AI-FB là **0 dB**; scene cũ vẫn giữ giá trị manual đã lưu. AUTO FB PRO giữ tham số `Period tolerance` của advisory zero-crossing cũ để tương thích; tham số đó **không chỉnh gate tracking FFT tự động mới**. Persistence trong AUTO điều khiển thời gian gate mới; AI-FB dùng persistence cố định 350 ms. Các model/growth gate vẫn có thể kéo dài hoặc ngăn hoàn toàn việc tạo cut.

## API và các file liên quan

| File | Trách nhiệm |
| --- | --- |
| `src/dsp/FeedbackModelWeights.h` | Trọng số và thống kê train, classifier, frame feature analyzer |
| `src/dsp/FeedbackSuppressionCore.h` | FFT detector, tracking/gates, ngân hàng cut và trạng thái; độc lập JUCE |
| `src/dsp/BuiltinRackCore.h` | Tích hợp hai detector vào slot 3/4, state độc lập, publication atomic, reset/disarm |
| `src/audio/LiveAudioEngine.h` | Getter `builtinFeedbackStatus(channel, slot)` và điều khiển native rack |
| `src/dsp/BuiltinSceneState.h` | Scene chỉ lưu loại/enable/tham số manual; armed/detector history không được recall |
| `src/ui/BuiltinRackPanel.h/.cpp` | ARM/DISARM/Clear, telemetry, editor và help text |
| `tools/train_feedback_model.py` | Dữ liệu tổng hợp, training, manifest và xuất header |
| `tools/lah_feedback_cli.cpp` | Bridge offline mono float32, telemetry JSON, plant loop thử nghiệm |
| `tests/test_feedback_suppression.cpp` | Kiểm thử detector/cut/state/finite/allocation và scene reset |
| `tools/independent_feedback_check.py` | Reference filter/closed-loop bằng NumPy/SciPy, dữ liệu do Python tạo |
| `tools/lah_feedback_model_cli.cpp`, `tools/independent_feedback_model_check.py` | Đối chứng feature/score C++ với fixtures và trọng số canonical độc lập |

Control mới trên `builtin::SlotControl`:

```cpp
std::atomic<bool> feedbackArmed {false};
std::atomic<std::uint32_t> feedbackResetGeneration {0};
```

Clear tăng `feedbackResetGeneration`; GUI không ghi vào ring buffer hoặc filter state của callback. `builtin::FeedbackStatus` là ảnh chụp gồm `candidateHz`, `modelScore`, `notchCount`, hai mảng `notchHz[6]`/`depthDb[6]`, và `armed`. Getter phải bỏ trạng thái cũ sau epoch reset, slot bypass hoặc transport mute. Không gọi getter này là một transaction đo lường nhiều trường đã đồng bộ ngoài các đảm bảo publication được kiểm thử.

## Kết quả kiểm thử bản tích hợp

| Hạng mục | Trạng thái tại thời điểm viết |
| --- | --- |
| MSVC/JUCE Windows Release toàn ứng dụng | **PASS**, Windows x64, MSVC 19.44/JUCE 8.0.15 |
| CTest Release gồm toàn bộ regression cũ và feedback mới | **30/30 PASS**, 29,73 giây |
| AddressSanitizer core của bản feedback mới | **28/28 PASS**, 48,39 giây; không instrument toàn GUI/JUCE/VST3 |
| Bridge feedback độc lập, sample rate/tần số/growth/closed-loop | **30 render cases PASS**, 44,1/48/96 kHz và các ca biên 8–192 kHz; không phải thử PA thật |
| Feature/model score độc lập | **25 clip / 495 frame PASS**, 5 analysis rates; max feature error 3,802604e-6, score error 5,719869e-7 |
| Native module regression cũ | **402 render cases PASS**, phần manual chạy unarmed; không thay thế 30 feedback cases |
| Allocation, bypass, reset, disarm-hold, Clear transition và scene recall | **PASS** trong core/rack/scene/callback tổng hợp; regression scene chạy lại riêng PASS 0,28 giây |
| GUI smoke AI/Auto tại kích thước thường và tối thiểu | **73 PNG / 0 bounds warnings**; đã xem bốn ảnh AI/Auto, không thấy cắt/đè control; device đóng, 0 callback, output mute |
| Benchmark mixer/native có nhiều detector/slot | **ĐÃ CHẠY; có deadline misses** ở 32 AUTO FB, 128 AI-FB và 128 Reverb; xem số liệu bên dưới |
| M32R ASIO, loopback/round-trip, thử mic/loa thật, soak | **CHƯA XÁC MINH — chưa nối mixer** |

Log kiểm thử: `artifacts/windows-v07/feedback-ctest.log`, `asan-ctest.log`, `feedback-asan.log`, `feedback-scene-retest.log`; ảnh và report ở `build-windows-v07/ui-smoke/`. Training/parity có manifest riêng trong `artifacts/feedback-model/`. EXE build cuối: **7.654.400 byte**, SHA256 `2e0a48c1c969214b19d21e47b9af269db8b9fcfc3e80abee812833da055d6168`; provenance đóng gói cần khớp binary này.

Fixture tone tăng mức off-bin cho thấy cut đầu khoảng **544–557 ms**, suy giảm tại tone khoảng **12 dB** ở các cấu hình được test; đây không phải latency audio hay cam kết phản ứng trên mọi tín hiệu. Transfer của notch đã học được đối chiếu với analog BLT/SciPy, max error khoảng 3,74e-9 trong fixture.

Đã thử **một vòng lặp tổng hợp thực sự hồi tiếp output vào input**: plant band-pass 1 kHz/Q20, delay 48 mẫu tại 48 kHz; gain 0,85 kiểm tuyến tính với đa thức transfer độc lập (max error 2,59e-10). Với gain 1,08 và seed ngừng sau 100 ms, cả controller tự động và AI-gated đều tạo cut trong ca đã nêu. Tail cuối của mô phỏng xuống dưới floor số của fixture; không công bố con số suy giảm cực lớn đó như hiệu quả acoustic hoặc dynamic range của thiết bị thật. Một plant không đại diện mọi room/loa/mic hoặc độ ổn định hệ PA.

## Tải CPU quan sát được và deadline misses

Benchmark `lah_builtin_benchmark` xử lý mixer/native offline, 32 input/output, 48 kHz / 128 mẫu: thời gian mỗi chu kỳ là **2.666,67 µs**. Mỗi cấu hình warm-up 50 block, đo 300 block. Process benchmark không mở driver và không xử lý VST3; app GUI mới đang mở MUTE trong lần chạy cuối, nên vẫn có tải máy/OS/GUI đồng thời. Đây không phải đo WCET, dropout driver hoặc độ trễ khứ hồi.

Log cuối: `artifacts/windows-v07/feedback-benchmark-final.log`.

| Cấu hình | p50 (µs) | p99 (µs) | Max (µs) |
| --- | ---: | ---: | ---: |
| 32 kênh, rack bypass | 157,5 | 292,0 | 766,5 |
| 8 kênh × Vocal Clean + Dynamics | 197,6 | 271,3 | 301,5 |
| 128 Reverb 96 | 1.066,8 | **3.191,9** | **3.997,5** |
| 32 kênh × AUTO FB PRO armed | 358,5 | 635,7 | **2.726,0** |
| 32 kênh × AI-FB LAB armed | 394,2 | 520,8 | 659,5 |
| 128 AI-FB LAB armed | 1.112,7 | **4.581,8** | **4.873,2** |

**Không chấp nhận cam kết realtime cho 32 AUTO FB, 128 AI-FB hoặc 128 Reverb ở buffer 128 mẫu:** đã có mẫu thời gian vượt chu kỳ, dù p50 thấp. Lần trước ở `feedback-benchmark.log` ghi max lần lượt 2.181,0 / 1.748,5 / 1.494,2 µs cho ba cấu hình này, đều thấp hơn lần cuối. Giữ cả hai log để thể hiện biến thiên; không chọn lần thuận lợi rồi kết luận không dropout. Cấu hình 32 AI-FB và 8×2 Vocal/Dynamics chưa vượt chu kỳ trong các mẫu đo được cũng **chưa phải nghiệm thu stream**, mọi nội dung/filter state hoặc chạy 24 giờ.

Cần tiếp tục profiling, giới hạn số instance theo tải thật, thử các buffer/driver phù hợp rồi chạy audio stream dài hạn. Chưa có ASIO M32R hoặc kiểm hardware/OS scheduling để xác định cấu hình vận hành được chấp nhận.

## Giới hạn còn tồn tại

- Tín hiệu đơn kênh không cung cấp đủ thông tin để luôn phân biệt tone nhạc và feedback có cùng đặc tính. Nhạc tăng mức, giọng hát kéo dài hoặc thay đổi gain có thể kích hoạt cut sai. Feedback không tăng mức, nhiều resonance yếu, transient quá nhanh hoặc detector gate không đạt có thể bị bỏ sót.
- Chưa có nhận dạng nguồn từ dữ liệu thu thực, microphone-array/reference cancellation, thuật toán adaptive acoustic feedback cancellation hoặc chứng cứ ổn định hệ kín cho hệ thống PA thực.
- Auto cut không thay thế EQ/positioning/gain trước feedback, true-peak limiter, SPL calibration hoặc bảo vệ loa. Không phát tín hiệu thử vào PA đang mở gain để làm minh chứng UI.
- Phân tích FFT tạo tải theo hop; số slot hoạt động và lịch driver/GUI ảnh hưởng deadline. Chưa chấp nhận mọi tổ hợp 128 detector là realtime đạt nếu chưa có benchmark và stream test tương ứng.
- Chưa sửa/xác minh xong XRun WASAPI đã ghi nhận ở giai đoạn trước. Có M32-Edit trên máy không tương đương kết nối/kiểm thử ASIO với M32R.
- Chỉnh tham số manual hoặc đổi module vẫn yêu cầu MUTE; chưa có automation mượt cho mọi hệ số. Reverb/FX/VST3/FIR và những phần còn thiếu khác của dự án giữ trạng thái riêng trong báo cáo tính năng, không được gọi hoàn tất nhờ bổ sung AI-FB.

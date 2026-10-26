# LiveAudioHost v0.7.0 — Windows Acceptance / Lab QA

**Release status:** SOURCE ONLY — NOT SHOW READY. Generated and inspected on Linux, not a Windows EXE. No M32R/ASIO hardware available here. No live acoustic calibrator, certified SPL meter, 24h operating system session or independent VST3 validation used. The six requested acceptance gates are not all satisfied.

## Chức năng mới hiện có trong source v0.7

- Thêm `reliability/StreamSafety.h` interlock: nếu xử lý callback vượt thời hạn ít nhất 8 lần liên tiếp, audio engine tự latches MUTE; yêu cầu người dùng kiểm tra và nhấn reset khi đang muted. Không nói đây là bằng chứng hardware XRun hoặc true-peak limiting. Phát hiện invalid frame count/sample rate.
- Ghi QA CSV mỗi ~1 s từ JUCE message-thread: sample rate, buffer, tên driver/device, channels, generation, deadline overrun, xrun do driver báo, CPU, lỗi thiết bị, MUTE, lock.
- `tools/qa_analyze.py`: phản hồi FAIL/INCONCLUSIVE/kết quả tiêu chí CSV tự động cho 24-hour session; không trả PASS nếu dữ liệu thiếu/driver không báo XRuns.
- Relative band-limited IR/energy envelope từ Transfer Function H1 (`ImpulseCore.h`) với gating coherence. **Chỉ là thử nghiệm DSP và chưa có đồ thị GUI riêng hoặc chuẩn IR/SPL**.
- 1 nhóm kiểm thử end-to-end tổng hợp mới: multi-source safety, dual-channel delay/gain/phase, relative IR, SPL calibrated tone, FIR 257 taps so sánh direct convolution, routing và PANIC.
- CMake sửa lỗi *đặc biệt nghiêm trọng*: Release `-DNDEBUG` làm `assert(...)` không thực thi ở test C++. Đã ép `-UNDEBUG` (GCC/Clang) và `/UNDEBUG` (MSVC) cho **mọi target kiểm thử**, giúp test Release có kiểm tra runtime thực sự. Chưa tự chứng minh trên MSVC.
- UI v0.7 giữ giao diện graphite / teal độc lập, thêm `QA LOG START/STOP` và status safety lock. **Chưa build JUCE GUI**.
- Script `scripts/build-windows-v07.ps1` build EXE ở máy Windows sau khi đã chuẩn bị MSVC/CMake/Ninja/JUCE; dừng ngay nếu compiler / automated test không đạt; không cài driver hay dependency tự động.

## Tests đã chạy *thực sự*

| Test host | Settings | Kết quả |
| --- | --- | --- |
| Linux g++ | C++17, Release, assertions ON, GUI OFF | 13/13 CTest PASS |
| Linux clang++ | C++17, Release, assertions ON, GUI OFF | 13/13 PASS |
| Linux clang++ | Debug + ASan/UBSan, GUI OFF | 13/13 PASS |
| Python stdlib | `tests/test_qa_analyze.py` | 8/8 PASS |
| Windows native executable | MSVC + JUCE 8 | **NOT RUN** |
| Midas M32R Live ASIO | Real driver, USB device | **NOT RUN** |
| 24-hour streaming | Hardware+OS endurance | **NOT RUN** |
| SPL with real calibrated mic | IEC reference instrument | **NOT RUN** |
| FIR physical impulse / latency | Wired low-level reference | **NOT RUN** |

Assertions in Release are now active; the 13/13 result is not a proof that UI, interface, audio or hardware is stable.

## Yêu cầu 6 điểm / Trạng thái

| Mục tiêu | v0.7 status |
| --- | --- |
| (1) File `LiveAudioHost.exe` trên Windows | Chưa tạo ở môi trường hiện tại; script phát hành và xác minh EXE cho Windows đã được bổ sung |
| (2) Dark/teal native desktop UI | Mã giao diện có sẵn, bổ sung logging/lock; chưa build thành GUI Windows để nghiệm thu bằng ảnh/interaction |
| (3) ASIO M32R Live USB | Cấu hình JUCE ASIO + hướng dẫn inventory/bench test; không có thiết bị nên chưa xác thực hardware |
| (4) FFT/RTA/TF/Phase | Lõi hiện có, validated synthetic; relative IR DSP đã bổ sung, chưa xác thực phần GUI/field measurement |
| (5) SPL/FIR/latency reference | Test tổng hợp SPL 94 dB @ 1kHz, FIR direct convolution và delay 173 samples đạt; không thay thế acoustic/loopback hardware |
| (6) Long-run/reconnect | Thêm callback deadline interlock, CSV, analyzer và test logic reconnect via session generations; chưa test hardware reconnect/24h |

## Kết luận và hạn chế

Không được gắn nhãn `Commercial`, `Certified`, `Show-ready`, `Smaart-equivalent`, `IEC 61672` hoặc `latency < 10 ms` nếu chưa có phép đo thực tế. Do thay đổi API JUCE mà chưa build trên Windows, khả năng compile bản GUI v0.7 vẫn chưa được xác nhận. ASIO direct monitoring, plugin sandbox, sample clock drift detection, loudspeaker-safe lookahead/true-peak limiter, SPL class compliance và 3D room simulation chính xác vẫn chưa đầy đủ.

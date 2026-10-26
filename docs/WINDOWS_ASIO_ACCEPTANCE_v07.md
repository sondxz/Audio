# LiveAudioHost v0.7 — Quy trình nghiệm thu Windows / M32R / SPL / FIR

**ĐÃ BUILD WINDOWS; CHƯA NGHIỆM THU M32R LIVE / AUDIO PHẦN CỨNG.** Kết quả build/core/callback/GUI tổng hợp xem `WINDOWS_DEVELOPMENT_REPORT_v0.7.0.md`. Đây là phiếu kiểm tra cần người vận hành thực hiện và điền kết quả, không phải giấy chứng nhận. Chỉ kiểm thử bằng tải an toàn/loopback kín, hệ loa PA và monitor tắt/cô lập. Không sử dụng trong show.

## A. Build file .exe

Mở **Developer PowerShell for Visual Studio** trong VS Code (Windows 11 x64), đảm bảo CMake, Ninja, MSVC, JUCE 8 (đúng thư mục chứa `CMakeLists.txt`) đã cài. Trong thư mục mã nguồn:

```powershell
.\scripts\build-windows-v07.ps1 -JuceRoot 'C:\SDK\JUCE'
```

Thành công **khi** script in ra `EXE BUILT LOCALLY`, và file `release-v07\LiveAudioHost.exe` tồn tại. CMake/CTest dừng ở lỗi đầu tiên, không được diễn giải bước build dở là EXE đã hoạt động. ASIO được bật trong build nếu không truyền `-DisableAsio`; cần tự rà soát giấy phép JUCE và Steinberg ASIO SDK trước phân phối.

## B. Kiểm thử audio device bằng driver ASIO của M32R Live

1. Kết nối USB từ card DN32-LIVE tới máy tính Windows, dùng driver chính thức tương thích OS/thiết bị. Trên mixer, kiểm tra định tuyến USB card bằng bản backup scene độc lập; test ban đầu với đầu ra vật lý không nối PA.
2. Chạy `.\scripts\inspect-windows-audio.ps1`. Đây **chỉ là kê khai driver Windows**, không xác minh ASIO có stream được.
3. Mở EXE; trong `Devices / ASIO`, chọn ASIO + driver đúng tên, sau đó thử 48 kHz với các buffer mà driver thực sự cho phép. Ghi số kênh nhận được. Không ép 32x32 khi hardware/driver không hỗ trợ.
4. Với loopback tín hiệu mức nhỏ đã cô lập khỏi loa, xác nhận `INPUT/OUTPUT meters` phản ánh tín hiệu thật; xác nhận `PANIC` làm output meter về 0, không có âm thanh không chủ đích. Chỉ bấm BẬT sau khi xác nhận routing/gain và mức an toàn. Khi replug đổi device, app phải giữ MUTE đến khi người vận hành bật lại.
5. Dùng `QA LOG: START` trên app, lưu CSV trong Documents/LiveAudioHost. Chạy bài test khởi đầu 15–60 phút để tìm lỗi trước; tiến hành 24 giờ chỉ trên bench setup và có người giám sát phù hợp. QA log ghi tiến trình qua *GUI timer*; không chứng minh tính độc lập hay tính đầy đủ của driver counters.

## C. Measurement/Phase/Delay

- Cùng một nguồn điện thử nghiệm đưa REF vào input 1 và bản sao sau đường thử tới input 2. Dùng tín hiệu broadband được bảo đảm phù hợp với thiết bị, KHÔNG lấy trực tiếp output công suất ampli vào input interface.
- Đối chứng amplitude, phase, coherence, delay bằng tín hiệu có gain/delay đã xác định độc lập. Dữ liệu dBFS/bin **không phải dB SPL**. Nếu delay finder không tin cậy thì không diễn giải phase đã bù.
- Báo cáo phải ghi: driver, clock, routing, buffer, cable path, REF/MIC, FFT length, averaging, delay finder confidence, điều kiện coherence, hình chụp dữ liệu.

## D. SPL & FIR

- SPL dB(A/C/Z): dùng microphone đo phù hợp cùng bộ hiệu chuẩn âm học 1 kHz tại 94/114 dB; giữ cố định gain, đường vào, clock và micro sau calibration. Không xem phép tính FFT-weighting đã được chứng nhận IEC 61672.
- So sánh với máy SPL tham chiếu được hiệu chuẩn. Không gán giá trị hoặc tiêu chí đạt chuẩn nếu thiếu bằng chứng về hiệu chuẩn/đáp ứng tần số/điều kiện môi trường.
- FIR: trước tiên chạy synthetic + loopback mức thấp, đo đáp tuyến và độ trễ xung. Phân biệt FIR group delay `(taps-1)/2/fs` với tổng roundtrip. FIR đang chỉ là 2 kênh 129/257 taps, mặc định BYPASS sau đổi device.

## E. Soát QA CSV

```powershell
python .\tools\qa_analyze.py 'C:\Users\<user>\Documents\LiveAudioHost\qa-session-YYYYMMDD-HHMMSS.csv' --min-hours 24
```

- `AUTOMATED LOG CRITERIA MET`: kiểm tra CSV đạt *các điều kiện tự động*; **không** chứng nhận độ chính xác âm học, không chứng nhận ổn định PA.
- `FAIL`: phát hiện ít nhất một vấn đề (drop deadline, errors, reconnect, thời lượng thiếu...).
- `INCONCLUSIVE`: driver XRuns không report hoặc audio luôn muted; không chấp nhận thay cho PASS.

## F. Phiếu kết quả (điền sau khi test thực địa)

| Bài nghiệm thu | Kết quả thực tế | Minh chứng | Đạt/Không đạt/Chưa test |
| --- | --- | --- | --- |
| Windows GUI compile với MSVC/JUCE | Đã build MSVC 19.44 / JUCE 8.0.15, render tổng hợp | EXE, CTest/log và SHA256 trong release-v07/artifacts | ĐẠT PHẠM VI BUILD; chưa nghiệm thu audio |
| WASAPI shared mặc định, luôn mute | 2×10 giây, 928/926 callbacks, mỗi lần 1 XRun | muted-wasapi-stream.json và muted-wasapi-diagnostic.json | KHÔNG ĐẠT; chưa xác định nguyên nhân XRun |
| M32R Live ASIO 48k, 2ch -> multichannel | CHƯA TEST | Driver, input/output channel screenshots | CHƯA TEST |
| Roundtrip wired loopback latency (ms) | CHƯA TEST | Trigger/recording calibrated | CHƯA TEST |
| Transfer function amplitude & phase | CHƯA TEST | Reference generator vs captured | CHƯA TEST |
| SPL with acoustic calibrator and meter | CHƯA TEST | Calibration report | CHƯA TEST |
| FIR impulse and latency on device | CHƯA TEST | Impulse response / block rates | CHƯA TEST |
| 24-hour stability / disconnect recovery | CHƯA TEST | QA CSV + device errors | CHƯA TEST |

## Bổ sung test PDC và tính liên tục QA

- PDC hiện chỉ bù reported VST3 latency + group delay FIR OUT1/2 sang OUT3–32, tối đa 65.536 mẫu. Đo impulse riêng từng tuyến; speaker delay có chủ ý vẫn cộng thêm. Thử plugin đổi latency: app phải mute và yêu cầu reset/rearm; không coi reported samples là latency đã đo.
- `--inspect-audio inventory.json` chỉ kê khai JUCE backend, không xác minh stream.
- QA analyzer từ chối khoảng trống log lớn hơn 5 giây, callback tiến triển không phù hợp rate/buffer, NaN/Inf và timestamp đi lùi. Khi thay đổi cấu hình, bắt đầu một session nghiệm thu mới. Audio mute trong session làm workload liên tục chưa được xác minh.

**Release gate:** Ứng dụng ở trạng thái `LAB ONLY / NOT SHOW READY` cho đến khi từng mục thực nghiệm đạt yêu cầu được duyệt độc lập. Plugin crash isolation và true-peak limiter chưa có; không nối thẳng tuyến PA chính.

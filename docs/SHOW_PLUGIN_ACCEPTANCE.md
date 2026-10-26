# Kiểm tra ngoại tuyến 17 plugin — 26/10/2026

**Trạng thái: đạt các kiểm tra chức năng dưới đây; chưa xác nhận đủ điều kiện chạy show.** Lần đo tải có một buffer vượt ngân sách. Không mở thiết bị âm thanh, không đo DN32/M32R thật, không chạy soak 24 giờ và không xác nhận chống hú trong không gian thực.

**Mốc phiên bản:** benchmark 30 giây trong tài liệu này là baseline **trước khi bổ sung Dynamics PRO/Digital FX PRO và các thay đổi phân tích/preset mới**. Không dùng số đo đó làm kết quả tải của bản mới. Các kiểm tra riêng đã chạy không đồng nghĩa toàn bộ bộ kiểm tra dự kiến 47 mục đã đạt; build/log CTest đầy đủ cho lần mở rộng này còn cần được xác nhận.

## Phạm vi đã chạy

Nguồn kiểm tra dùng trực tiếp `dsp::Mixer` và hai `builtin::Rack` của ứng dụng, tại **48.000 Hz, 128 mẫu, tám input/tám output**. Cả input insert và output insert đều được kiểm tra. Danh mục hiện tại gồm Vocal Clean, Vocal Color, AI-FB LAB, AUTO FB PRO, Equalizer, Smart EQ, Dynamics, Smart Compressor, Optical Compressor, FX Delay, Digital FX, Reverb 96, Sub Pro, Drum Live, PC Audio, Analog và 4K Line.

`lah_show_rack_acceptance_tests` đã chạy đạt với các điều kiện kiểm tra luôn bật trong Release:

- Mỗi module chạy trên cả tám kênh; so sánh render toàn bộ với CH1/CH8 chạy riêng qua routing hoán vị, hệ số dương/âm. Kênh được chọn khớp trong sai số 0,000002; các ngõ trả còn lại bằng 0 khi chỉ cấp một nguồn. Có kiểm tra năng lượng khác 0 để đường tín hiệu bị mất không thể vượt qua kiểm tra.
- Bypass thực sự trở về tín hiệu khô qua routing sau chuyển tiếp 64 mẫu.
- Năm cấu hình chuỗi bốn slot, đồng thời tám rack input và tám rack output: đối chiếu toàn bộ mẫu với cách tính đồ thị `input rack → routing → output rack` riêng. Các cấu hình này đi qua đủ 17 module. Đây là đối chiếu tích hợp routing/thứ tự insert, không phải một mô hình DSP độc lập của từng thuật toán.
- Chuỗi FX Delay/Reverb/Digital FX/Dynamics ở cả hai miền có đuôi âm đã đo được trước PANIC; callback tắt audio cho output bằng 0, rồi bật lại với đầu vào im lặng trong 0,853 giây vẫn bằng 0; cấp tín hiệu mới thì có âm trở lại. Phép kiểm tra PANIC không tự gọi reset rack để che hành vi mixer.
- Không ghi nhận lời gọi `operator new/new[]` trong phần xử lý đã giám sát; mọi cấp phát chuẩn bị buffer diễn ra trước đó. Không có mẫu output NaN/Inf.

Các kiểm tra chuyên biệt vẫn cần chạy cùng CTest: `lah_live_plugin_parameters` kiểm tra toàn bộ khoảng tham số/NaN/Inf tại 8–192 kHz, chuyển tiếp tham số và đổi cấu trúc reverb; `lah_insert_sound_changes` kiểm tra thay đổi âm của đủ 17 module; `lah_feedback_suppression` và các bài tham chiếu feedback kiểm tra detector/notch trên tập tín hiệu tổng hợp; `lah_builtin_telemetry`, `lah_audio_engine` kiểm tra meter/PANIC và callback JUCE. Bài kiểm tra mới bổ sung tải tám ngõ trả và tổ hợp hai rack, không thay thế các bài trên.

## Phạm vi mới sau baseline

Nguồn hiện tại bổ sung spectrum pre/post từ **mẫu audio thật** cho đủ 17 module, FFT ngoài callback, cùng A/B đầy đủ trạng thái và **51 preset khởi đầu** (ba trạng thái cho mỗi module). Đồ thị mô hình vẫn được phân biệt với spectrum đo được; khi không có audio/mẫu hợp lệ, analyzer không được xem như bằng chứng có tín hiệu. Preset là điểm bắt đầu cho thuật toán native, không bảo đảm phù hợp với một show cụ thể.

**Dynamics PRO** có 19 tham số gate/expander, compressor/soft knee, mix, output và final sample-peak limiter; meter GR lấy attenuation thật của từng tầng. Limiter không có lookahead, latency của bộ xử lý 0 mẫu, không phải true peak. **Digital FX PRO** có 18 tham số Echo + Reverb **mono**, gồm lọc, echo gửi sang reverb, ducking và master gain; echo tối đa 350 ms, predelay tối đa 120 ms. Đây là chế độ tùy chọn; chế độ sáu tham số và ID cũ vẫn được giữ cho scene cũ.

Các bài `test_digital_fx_pro.cpp` và `test_native_pro_rack.cpp` đã biên dịch MSVC `/O2 /W4` sạch và chạy đạt riêng. Bài rack xác nhận timing thật của echo/reverb; giữ echo đang chờ qua chỉnh gain; đổi cấu trúc/PRO↔legacy không diễn giải lại stamp cũ; compression 4:1, gate 30 dB, limiter vẫn giữ ceiling qua chuyển tiếp tham số 64 mẫu; GR đúng kênh/tầng và bị vô hiệu khi PANIC/bypass; snapshot/move/copy giữ trạng thái đầy đủ; tám kênh độc lập; tham số min/max/NaN/Inf tại 8/48/192 kHz và không cấp phát trong callback được giám sát. Limiter có guard cuối sau ramp để ramp không tạo lại mẫu vượt ceiling.

Những kết quả riêng này chưa xác nhận tải CPU của PRO, GUI hoàn chỉnh hay toàn bộ 47 kiểm tra. Cần chạy build/CTest và benchmark có cấu hình PRO mới rồi bổ sung log. [Ghi nhận video MR18](VIDEO_MR18_ANALYSIS.md) chỉ đối chiếu ba mốc đã quan sát; limiter trong video hiển thị lookahead 1 ms, khác thiết kế hiện tại, và video không chứng minh kết nối M32R hay trình diễn chi tiết đủ 17 plugin.

## Số đo baseline thực tế — trước PRO

Máy chạy: **Intel Core i7-13700KF, 24 logical processors, Windows build 26100**, MSVC x64 `/O2`, tiến trình ngoại tuyến ưu tiên bình thường. Dùng `steady_clock` quanh đúng `Mixer::process`; thời gian gồm cả khoảng tiến trình bị hệ điều hành ngắt. Không tách được chi phí DSP khỏi độ trễ lập lịch chỉ bằng phép đo này.

Benchmark baseline có **74 cấu hình**: bypass; 17 module × một/bốn slot × input/output; năm trường hợp đồng thời 8×4 input + 8×4 output. Equalizer bật đủ tám band; feedback được ARM sau reset; delay/reverb dùng feedback/decay cao. Digital FX/Dynamics trong lần đo này dùng thuật toán cũ, chưa có PRO. Cấu hình live edit thay đổi các atomic tham số khi đang xử lý. Mỗi cấu hình bỏ qua 16 buffer làm nóng trước phần thống kê.

Lần chạy 30,000281 giây đo **186.771 buffer**, tương ứng 498,056 giây audio cộng gộp giữa các cấu hình, **0 cấp phát xử lý, 0 mẫu NaN/Inf**. Đây là render chạy nhanh hơn thời gian thực, không phải stream thiết bị kéo dài 498 giây.

Ngân sách một buffer: **2.666,67 µs**.

| Cấu hình đồng thời, 64 insert | Buffer đo | p50 (µs) | p99 (µs) | Max (µs) | Vượt ngân sách |
|---|---:|---:|---:|---:|---:|
| Hỗn hợp đủ loại | 985 | 366,5 | 757,8 | 1.133,5 | 0 |
| Reverb 96 | 766 | 467,2 | 1.132,5 | 1.839,3 | 0 |
| AI-FB LAB đã ARM | 625 | 616,6 | 951,2 | 1.207,6 | 0 |
| AUTO FB PRO đã ARM | 699 | 522,2 | 1.058,4 | 1.329,2 | 0 |
| Chuỗi hỗn hợp chỉnh tham số trực tiếp | 1.078 | 335,0 | 724,7 | 1.698,0 | 0 |

**Có một lần vượt ngân sách trong toàn bộ phép đo:** Smart EQ ở input, tám kênh × một slot, **14.229,8 µs** (5,34 lần ngân sách); p50 110,4 µs, p99 320,8 µs trên 2.846 buffer của cấu hình đó. Giữ nguyên kết quả này; chưa đủ dữ liệu để quy nguyên nhân cho DSP hay lịch chạy của Windows. Kết quả không được diễn giải thành “không xrun”. File đầy đủ: [show-rack-benchmark.csv](../artifacts/windows-v07/show-rack-benchmark.csv).

Một số chuỗi cố ý cực đoan đạt peak **1,0** ở output (bốn Drum Live, 64 reverb, live edit). Guard output có cắt mẫu tại ngưỡng; cần chỉnh gain/headroom theo chương trình âm thanh. Peak hữu hạn không chứng minh âm thanh không clipping. Số notch học được trong CSV chỉ là hoạt động detector trên tone tổng hợp, không phải tỉ lệ chống hú thành công hay false-positive trong show.

## Chạy lại

```powershell
cmake --build build-windows-v07 --config Release --target lah_show_rack_acceptance_tests lah_show_rack_benchmark
ctest --test-dir build-windows-v07 -C Release -R '^lah_show_rack_acceptance$' --output-on-failure
build-windows-v07/Release/lah_show_rack_benchmark.exe --seconds 30
```

Benchmark chấp nhận `--seconds 30..60`, in số buffer vượt ngân sách và trả lỗi nếu có cấp phát xử lý/NaN/Inf. Vượt thời gian không làm CTest thất bại vì phụ thuộc máy/lịch chạy; phải đọc kết quả số. Bộ benchmark không tự mở ASIO/WASAPI.

Các số trên chưa bao gồm driver/USB, DPC, GUI, VST3, FIR hay tải nền thực tế. Chưa xác nhận vận hành lâu dài, thay driver nóng, chất lượng từng preset, gain trước hú, hoặc cáp USB dài. Cần kiểm tra lại cả chuỗi máy tính ↔ card USB của mixer với cấu hình show và buffer thực tế trước khi dùng hệ thống có khán giả. Các module native là thuật toán được mô tả trong ứng dụng; kết quả này không xác nhận chúng sao chép phần mềm trong video hoặc mọi tính năng của MASTER DEVELOPMENT PROMPT.

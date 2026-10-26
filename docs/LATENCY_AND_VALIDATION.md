# Độ trễ: phân biệt thông số và phép đo

> Windows update: đã có PDC tĩnh reported VST3 + FIR OUT1/2 sang OUT3–32, giới hạn 65.536 mẫu; xem `WINDOWS_DEVELOPMENT_REPORT_v0.7.0.md`. Giữ nguyên speaker delay có chủ ý. Đổi reported latency khi chạy khiến app mute/rearm. Các mục “chưa có PDC” bên dưới mô tả bản lịch sử; roundtrip M32R vẫn chưa đo.

Độ trễ end-to-end mic→USB→ASIO→DSP→USB→console→PA của Midas M32R Live **CHƯA ĐƯỢC ĐO**. Bản 0.3 không được tuyên bố '5ms' hoặc '10ms'.

## Giới hạn lý thuyết theo buffer

Công thức thời gian một audio block: `Tbuffer_ms = bufferSamples * 1000 / sampleRateHz`.

| Buffer | 48 kHz, thời gian 1 block | 48 kHz, 2 blocks (chỉ minh họa) |
|---:|---:|---:|
| 64 | 1.33 ms | 2.67 ms |
| 128 | 2.67 ms | 5.33 ms |
| 256 | 5.33 ms | 10.67 ms |
| 512 | 10.67 ms | 21.33 ms |

Cột 2 blocks **không là dự đoán chính xác round-trip**. Driver có thể báo input/output latency gồm nhiều buffers và đường phần cứng; không cộng hai lần các giá trị đã được tính trong driver. DAW plugin có thể thêm samples và fail to report latency. Digital crossover, look-ahead limiter, convolution tạo group delay riêng.

## Những nguồn latency trong bản mã

- Input/output reported latency: lấy từ `juce::AudioIODevice::getInputLatencyInSamples()` / `getOutputLatencyInSamples()`; hiển thị status nhưng chưa đo loopback.
- DSP built-in: biquad EQ, envelope compression/gate và matrix không dùng look-ahead; group delay phụ thuộc tần số, không đồng nhất với fixed added samples.
- Speaker delay: user-controlled 0–250ms trên từng output, xuất hiện trong monitor diagnostic.
- VST3 experimental: `getLatencySamples()` nếu plugin báo, không có PDC/bù trễ giữa các tuyến.
- Actual one-way/roundtrip: chưa có phép đo. USB M32 routing và mixer processing cũng có thể tác động; đo thực tế mới xác nhận.

## Quy trình xác nhận trước khi nghĩ tới sử dụng live

1. Build và mở GUI trên Windows, bảo đảm không crash với/không có ASIO; không lẫn sample rate.
2. Dùng sound card/loopback test bench với mức tín hiệu an toàn và monitoring độc lập; đo reference input và return, phân biệt delay do DAC/ADC/driver/app/USB. Không kết nối dây vào thiết bị đang bật gain lớn.
3. Test buffer 64,128,256,512 ở 48k với input/output đã chọn; ghi 99.9 percentile callback duration, xrun/dropout, driver input/output latency, round-trip đo thực tế qua thiết bị ngoài.
4. Test 32x32 routing, VST3 latency flags, mute/unmute, scene recall, device disconnect/reconnect; chạy soak test dài hạn; kiểm soát mức âm và clipping với audio calibration.
5. So sánh TF/coherence/phase/delay với Open Sound Meter / Smaart trên phép đo và điều kiện đồng nhất. Không xem khoảng phase delay trong đo phòng như tự động bằng latency phần mềm.
6. Với hệ thống có monitor wedges/in-ear, đánh giá đường feedback và phương án bypass tức thời ngoài máy tính trước buổi diễn.

## Cập nhật 0.3 — phân biệt delay đo TF và round-trip latency

- Ước lượng **TF delay** trong `lah_tf_cli`/measurement core là chênh thời gian *giữa hai kênh thu theo reference path*, không phải bản thân độ trễ app. Với mic không khí nó gồm khoảng cách/loa/processing; với loopback đúng nó có thể gồm đường thiết bị và app tùy chỗ tách reference.
- GCC-PHAT chạy trên 32768 mẫu và tìm lag tuyến tính, tối đa theo dữ liệu capture. Mọi phát hiện vẫn phải được kiểm chứng với measurement reference và kiểm tra phase/coherence.
- **Chưa có đo round-trip thực với M32R LIVE**; không dùng thời gian một buffer để khẳng định round-trip.

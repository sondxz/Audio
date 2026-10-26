# Thiết kế Audio Engine giai đoạn 1

> Trạng thái hiện tại: xem `WINDOWS_DEVELOPMENT_REPORT_v0.7.0.md`. Pipeline hiện là Mixer/DSP/output delay → short FIR OUT1/2 → stereo VST3 → reported-latency PDC OUT3–32 → final clip guard/meters. Analyzer/SPL chạy worker. Các mô tả pass-through/timer/PDC cũ dưới đây được giữ làm lịch sử; không phải trạng thái nghiệm thu audio thực.

- **An toàn đầu ra:** tắt pass-through khi mở app, khi ngừng thiết bị, khi lỗi được thông báo và khi thiết bị bắt đầu lại. `PANIC/MUTE` ghi atomic để audio callback tiếp theo xuất zero. Driver có thể đã giữ buffer âm thanh cũ, vì vậy panic **không phải** mute phần cứng tức thời.
- **Ánh xạ:** `output callback slot i` lấy từ `input callback slot i`. Mỗi driver có thể compact các kênh vật lý đang bật, do đó đây **chưa phải** routing matrix ánh xạ cổng vật lý tùy chọn. Callback nhận bao nhiêu channel tùy driver, phần xử lý không hard-code 32.
- **Không có limiter:** mẫu âm hữu hạn giữ nguyên mức; `NaN/Inf` được đưa về 0; signal >0 dBFS được theo dõi bởi meter nhưng không tự giới hạn. Mức an toàn cần thiết lập từ nguồn và amplifier/mixer ngoài.
- **Monitoring:** meter theo peak `max(abs(sample))` của từng block, dùng atomic exchange, refresh 20 Hz. Peak giữ một phần theo suy giảm visual, không phải peak hold chính xác theo chuẩn đo. Tối đa 64 kênh hiển thị mỗi bên.
- **Latency:** `device->getInputLatencyInSamples()`/`getOutputLatencyInSamples()` và `currentBufferSizeSamples`. Đây là các số driver báo cáo, không phải đo acoustic hay vòng in→out thực tế. Chưa có test loopback. DSP latency trong bản này bằng 0 sample theo thuật toán (không bao gồm driver hay transmission).
- **CPU:** lấy `AudioDeviceManager::getCpuUsage()`; XRun từ `getXRunCount()`. Chỉ phản ánh thông tin khả dụng của JUCE/backend.
- **Lưu settings:** `AudioDeviceManager::createStateXml()`, ghi file tạm rồi rename để hạn chế phá file cũ; file nằm trong application data. Nếu default setup không trả XML, không có settings riêng cần lưu.

---

## Addendum v0.2 — Mixer / DSP / FX / VST3

The active callback now invokes `dsp::Mixer`, not the original `PassThroughCore` (retained for standalone tests). Callback-slot-to-slot diagonal routing is still the initial setup, but any route gains in `dsp::Controls` are applied to the audio signal.

Mixer DSP state is preallocated in `Mixer::prepare()`: 32 output delay rings (max 250ms at current sample rate), stereo delay/reverb rings, biquad/envelope states and active route matrices. Atomic controls are read once at the start of a process block. Route gains and channel gain are interpolated linearly across the block; filter coefficient jumps are not smoothed, so rapid HPF/PEQ changes may click.

`measurement::Capture` uses an SPSC atomic floating-point ring so analyzer snapshots are obtained without locks and FFT work is done on the JUCE GUI timer, not inside the audio callback.

VST3 support is experimental single stereo slot after DSP outputs 1/2. Third-party processor callbacks can still allocate, block or crash despite the host's own callback design. JUCE plugin scratch logical size adjusts within preallocated 4096-sample capacity. Need Windows build and callback allocation profiling to establish real-time suitability.

**No API in this release measures round-trip latency**. The diagnostic shows driver-reported input/output samples, chosen buffer, speaker delay and plugin-reported latency; do not sum these blindly.

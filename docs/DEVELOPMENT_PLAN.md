# Kế hoạch nâng cấp LiveAudioHost

Cập nhật **26/10/2026**. Nâng cấp mã nguồn JUCE/MSVC đang hoạt động, giữ module ID và scene cũ, không thay DSP bằng hình minh họa. Tham khảo ảnh, MASTER DEVELOPMENT PROMPT và các keyframe của [TITAN LIVE HOST 1.0.10](https://www.youtube.com/watch?v=UiGUPirC3KY); không khẳng định tương đương thuật toán của nhà cung cấp.

## Mốc đã triển khai và kiểm thử

Bản Windows x64 Release hiện tại build thành công, **41/41 CTest đạt trong 48,55 giây**. GUI smoke đạt **173 PNG / 0 bounds warnings / 34 editor validations**; GUI → callback audio đạt **14 checks**, trong đó có **8 ca DSP chính** ở input/output. Xem [TEST_MATRIX.md](TEST_MATRIX.md) và các log `artifacts/windows-v07/completion-final-*`.

| Mốc | Đã có trong mã nguồn | Giới hạn còn lại |
|---|---|---|
| Audio và USB returns | Engine tối đa 32×32; profile USB 1:1 cho 1–8 kênh, bật audio chủ động, mute/PANIC, native input/output racks độc lập | Chưa mở được stream DN32-LIVE vật lý hoặc đo round-trip |
| Mixer workflow | Tên 32 input lưu scene, 8/16CH banks và viewport ngang, chain overview, move/copy/paste/remove slot, peak hold/clip reset từ meter thật | Chưa stereo-link/chain-enable chung; output column là physical USB OUTPUT |
| Bus / Master | Tám bus mono, send/gain/mute, constant-power pan, stereo Master L/R với fader/meter/output maps; sau input DSP, trước physical output racks | Chưa rack plugin riêng từng bus/master; chưa bus stereo link |
| Routing profiles | Master-only xóa matrix/FX return chung và đưa IN1–8 vào Master; USB 1:1 tắt bus/master và khôi phục các return riêng; cả hai giữ mute | Chưa phát hiện mọi loop/duplicate/parallel dry route tùy chỉnh |
| Scene / preset | 8 Quick Scenes save/recall/rename/import/export; native plugin và chain bốn slot, copy/paste, XML schema 2, tên/routing/bus/master/EQ state; restore saved scene khi mở app | Chưa VST3/FIR state, full channel/master preset riêng hoặc autosave/crash recovery toàn ứng dụng |
| EQ mở rộng | Equalizer tùy chọn tám bands, bell/shelves/HPF/LPF, band enable/frequency/gain/Q và kéo điểm; legacy sáu tham số/scene giữ nguyên | Response MODEL chưa phải FFT/spectrum pre/post |
| Native plugins | 17 thuật toán thực, editor nổi đúng target, meter slot, Smart Comp macro Boost/Balanced/Comp, live parameter smoothing | Nhiều module vẫn chỉ đáp ứng một phần đặc tả chi tiết |
| Chứng minh UI/DSP | GUI callback → audio callback cho gain, legacy EQ, delay và eight-band EQ; 17 module có Rack signal-change tests riêng | Không gộp thành mọi tham số của mọi module đều đã được nghiệm thu |

## Trình tự phần việc còn lại

1. **Giải quyết kết nối phần cứng:** hiện đã thấy tên DN32-LIVE ASIO, nhưng thiết bị chưa báo sample rate/kênh và chưa mở được stream. Thử đường USB ngắn trực tiếp, kiểm tra card/driver/Windows và chạy lại muted-check. Không đổi routing/đẩy tiếng trước khi có dữ liệu hợp lệ. Theo [báo cáo kết nối](M32_CONNECTION_CHECK_REPORT.md).
2. **Nghiệm thu USB thật:** bắt đầu 1 kênh, sau đó 8/16/32 nếu driver cung cấp; xác minh Card send/return, buffer 64/128/256/512 được hỗ trợ, loopback round-trip, XRuns, ngắt/nối và chạy dài.
3. **Bus/master graph hoàn chỉnh:** rack plugin riêng, stereo link, PDC và kiểm tra đường song song. Giữ mặc định USB 1:1 độc lập; không tự gộp physical outputs.
4. **Dynamics/Vocal:** limiter/knee/gate controls độc lập và GR thật; Vocal Clean clean macro/de-harsh/noise reduction/autogain/mix, FFT pre/post đúng instance; kiểm tra giọng/nhạc tham chiếu và artifact.
5. **Smart EQ/FX:** target analysis + đề xuất có Apply/Reject; chorus/modulation/pitch, stereo/ping-pong/tap delay và stereo Hall/Plate/Room reverb với test riêng.
6. **Các module còn lại:** PC AUDIO thành source capture utility có clock/SRC; Drum learn/sustain/preset; Sub crossover/phase/delay/limiter; Analog/4K aliasing/oversampling và VU phù hợp.
7. **VST3:** editor/fallback, per-channel chain/reorder, state save/load, PDC và scanner process. Cần plugin thật, missing-plugin/restart/crash tests.
8. **Recorder/Assistant/menu:** ghi multitrack từ tap thật và storage/status; radial menu; trợ lý có giải thích Preview/Apply/Reject. QA CSV hiện không phải ghi âm.
9. **Release có nghiệm thu:** native/hardware regression, CPU/RAM/latency/XRun workload cụ thể, soak test, giấy phép/dependencies, package và manual.

Các phần chưa triển khai trong [FEATURE_MATRIX.md](FEATURE_MATRIX.md) vẫn là công việc còn lại, không được coi hoàn thành chỉ vì build hoặc UI smoke đạt. [TEST_MATRIX.md](TEST_MATRIX.md) phân biệt PASSED offline với BLOCKED hardware. [APP_UPGRADE_20261026.md](APP_UPGRADE_20261026.md) mô tả bản đóng gói mới nhất. Audio callback tiếp tục không được làm file/network/load/scan hoặc chờ GUI; các biểu đồ MODEL phải được phân biệt với dữ liệu đo.

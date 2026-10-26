# Bản nâng cấp mixer, bus, EQ và scene — 26/10/2026

Ứng dụng Windows đã được nâng cấp trực tiếp trên mã nguồn hiện có. Đây là bước nâng cấp có DSP và state thật, chưa hoàn thành toàn bộ thuật toán/workflow nâng cao trong video. Đối chiếu đầy đủ tại [FEATURE_MATRIX.md](FEATURE_MATRIX.md).

## Chức năng mới và cách dùng

| Chức năng | Cách dùng và phạm vi |
|---|---|
| **8 software buses / Stereo Master** | Trang **BUS**: chọn input, chỉnh send vào từng bus hoặc trực tiếp Master; mỗi bus có gain/mute/pan/send Master và tùy chọn USB output trực tiếp. Master L/R có fader/mute/chọn output riêng. Pan dùng constant-power; nguồn là tín hiệu sau input plugins. Tổng bus/Master đi qua rack của USB output, output gain/delay/guard hiện có |
| **Hai profile routing** | **IN 1–8 > MASTER** xóa matrix trực tiếp, đưa CH1–8 vào Master OUT1/2; **USB 1:1** trả IN n → OUT n và tắt bus/Master. Hai thao tác giữ audio mute. Default và scene cũ vẫn dùng USB/matrix, bus mới mặc định off |
| **EQ 8 band** | Chèn Equalizer, mở editor, chọn **8 BAND EQ**. Mỗi band có bật/tắt, Bell/Low shelf/High shelf/HPF/LPF, frequency/gain/Q; kéo điểm để chỉnh frequency/gain, cuộn chuột chỉnh Q. Đường vẽ là đáp tuyến MODEL của bộ lọc thật, chưa có FFT overlay. Có thể quay về Legacy EQ; các giá trị tám band được giữ riêng |
| **Tên kênh / 16CH / peak** | Mixer có tên kênh chỉnh được, bank 8/16 kênh (16 kênh dùng cuộn ngang), chain summary kênh đang chọn, peak hold/clip latch và reset. Cột **USB OUTPUT** vẫn sửa một ngõ ra vật lý; Master stereo nằm ở trang BUS |
| **Thao tác insert** | Nhấp phải slot input/output: di chuyển lên/xuống, copy/paste hoặc remove. Thao tác cấu trúc cần mute; copy là snapshot đầy đủ, gồm EQ tám band. Không bật lại feedback ARM khi copy/recall |
| **8 Quick Scenes** | Trang **SCENES**: SAVE/RECALL từng ô, nhấp đúp tên để đổi, import/export XML. Scene lưu routing, gain/DSP, hai rack native, tên kênh và bus/Master; schema 1 cũ được đọc, schema 2 lưu state mở rộng. Recall giữ mute |
| **Preset plugin/chain** | Cùng trang SCENES: chọn INPUT/OUTPUT, channel và slot; lưu một plugin hoặc chuỗi bốn slot, copy/paste, import/export. Lưu cả bypass và EQ tám band. State VST3 ngoài chưa nằm trong preset |
| **Kiểm tra DN32-LIVE** | Script `scripts/check-m32-usb.ps1` liệt kê trước; `-ObserveMuted` yêu cầu phép thử ASIO im lặng, chọn đúng DN32/M32 và không fallback thiết bị khác. Không tự cài driver hay sửa routing phần cứng |

Bus/Master hiện chưa có rack insert độc lập riêng; output native vẫn xử lý sau khi các đường được cộng vào USB output đó. Không nên coi output rack này là tám bus racks. Chức năng stereo link và chain bypass chung chưa có.

## Bằng chứng kiểm thử

- **Build Windows x64 Release thành công; 41/41 CTest đạt**, 48,55 giây. Log `artifacts/windows-v07/completion-final-build.log`, `completion-final-ctest.log`. Lượt đầu 40/41 đã phát hiện JUCE chấp nhận một XML fragment thiếu đóng tag; codec đã thêm kiểm tra tài liệu đầy đủ và lượt cuối đã đạt.
- **14 GUI/audio checks**, gồm tám ca DSP chính gain/EQ/delay và EQ tám band ở input/output. Chỉnh band 1 kHz +6 dB bằng handler GUI thật làm audio callback thay đổi đúng +6 dB ở CH8/slot3, bảy kênh còn lại không đổi. Legacy knobs bị vô hiệu trong chế độ tám band; RESET khôi phục Legacy đúng state.
- **8-bus core** kiểm tra sum, pan, gain/mute, output maps, smoothing, meter epochs, invalid values, zero callback allocations và vị trí trong mixer. Actual JUCE callback kiểm tra CH8 → BUS8 → Master L/R, output insert/gain, cách ly, mute và PANIC/rearm.
- **EQ mở rộng** đối chiếu công thức bộ lọc độc lập cho cả năm loại tại 8–192 kHz, response/RMS, live edits, bypass, copy/scene/isolation và zero allocations. Test riêng đã chạy Release và MSVC AddressSanitizer; không phải ASan toàn bộ GUI/driver.
- **Scene/preset** có XML round-trip đủ state, scene cũ, bounds/schema/duplicate/corrupt/DTD/entity checks; decode hoàn tất trước commit. Import/export dùng file chooser bất đồng bộ. Không kiểm thử driver/VST3 bên thứ ba qua các bài này.
- **GUI smoke 173 PNG, 0 bounds warnings, 34 nhóm editor validation đạt**; thêm ảnh mixer 16CH và EQ tám band ở hai kích thước. Kiểm tra library dùng thư mục artifact, không đọc/ghi AppData của người dùng và không mở audio device.

## Kết nối mixer hiện tại

Driver DN32-LIVE đã xuất hiện, nhưng phép thử chưa mở được stream hoặc nhận kênh/sample rate. Người dùng đang dùng dây 10 m cộng đoạn nối dài; cần thử dây A–B dữ liệu ngắn trực tiếp để loại trừ đường dây. Chưa có bằng chứng USB send/return nghe được, round-trip latency hoặc XRun chạy live trên mixer.

Xem [báo cáo kiểm tra dây/driver](M32_CONNECTION_CHECK_REPORT.md). Bản nâng cấp không được mô tả là đã hoàn tất nghiệm thu M32R.

## Phần còn lại

Dynamics/Vocal Clean nhiều khối, FX Digital echo/reverb stereo, Reverb nhiều thuật toán, per-plugin FFT/GR, bus insert riêng, stereo link, PC Audio capture, VST3 chain/editor/state hoàn chỉnh, multitrack recorder và Assistant vẫn còn thiếu hoặc mới có DSP cơ bản. [Kế hoạch](DEVELOPMENT_PLAN.md) và [ma trận kiểm thử](TEST_MATRIX.md) ghi phạm vi cụ thể; các tên module không xác nhận đủ toàn bộ tính năng trong video.

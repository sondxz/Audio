# Báo cáo triển khai M32R USB và nâng cấp mixer

Cập nhật **26/10/2026**. Bản Windows x64 Release đã build thành công; **41/41 CTest đạt, 0 lỗi, 48,55 giây**. Log: [build](../artifacts/windows-v07/completion-final-build.log), [CTest](../artifacts/windows-v07/completion-final-ctest.log). Lượt đầu 40/41 phát hiện XML hỏng chưa bị từ chối đúng; parser đã được sửa và lượt tổng hợp cuối đạt. Mốc 37/37 trước đó là lịch sử tại `video-alignment-final-ctest.log`.

## Chức năng thực tế trong bản cập nhật

- **Mixer:** đặt tên 32 input bằng nhấp đúp label; 8CH/bốn bank hoặc 16CH/hai bank, cuộn ngang khi màn hình hẹp. Chain overview mở đúng slot. Menu chuột phải input/output có move up/down/copy/paste/remove; structural edits yêu cầu mute và giữ toàn bộ snapshot, gồm tám band EQ. Peak hold/clip latch/reset dùng peak âm thanh thật.
- **USB 1:1:** chọn 1–8 kênh, tìm DN32/M32 ASIO hoặc thiết bị được chọn, xác minh physical IN/OUT masks. Áp dụng profile đặt IN n → OUT n, xóa route khác, pan giữa, tắt FX chung và bus/master software; giữ plugin đã chọn và audio mute. USB không điều khiển fader/preamp/Card routing phần cứng.
- **Bus / Master:** tám software bus mono thật có send 32 input, gain/mute, pan/to-Master, direct physical output và meter. Stereo Master L/R có hai fader, mute, meter và output map riêng, dùng constant-power pan. Các nhánh lấy tín hiệu sau input DSP/native inserts và cộng trước physical output rack/gain/delay/clip guard. Chưa có rack plugin riêng từng bus/master.
- **Hai profile rõ ràng:** Master-only xóa matrix và FX return chung, đưa IN1–8 qua stereo Master OUT1/2; USB 1:1 khôi phục return riêng và tắt buses. Chuyển profile/output map giữ mute. Nếu tự bật bus cùng matrix, người vận hành vẫn phải kiểm tra đường cộng song song; chưa có bộ phát hiện mọi loop/parallel route.
- **Cột USB OUTPUT trong Mixer** chỉnh một ngõ ra vật lý và bốn insert của nó. Stereo Master thật nằm ở trang BUS/MASTER; hai phạm vi này không đồng nhất.
- **SCENES:** tám Quick Scenes save/recall/rename/import/export; preset native một plugin hoặc chain bốn slot, copy/paste sang target INPUT/OUTPUT khác. XML schema 2 lưu tên kênh, routing, DSP, hai native racks, EQ tám band và bus/master config; đọc scene cũ, validate/clamp trước commit. Recall/preset load giữ mute; saved scene có thể được khôi phục khi mở ứng dụng. Không lưu VST3/FIR state hoặc mọi trạng thái UI/crash recovery.
- **Equalizer:** legacy HPF/ba bell/sáu tham số vẫn giữ nguyên. Chế độ tùy chọn tám band có bell, low/high shelf, HPF/LPF; chỉnh enable/frequency/gain/Q hoặc kéo điểm. Lần đầu chuyển đổi giữ đáp tuyến legacy; quay lại không xóa các chỉnh tám band. Response ghi rõ MODEL, không phải FFT đo.
- **17 native processors:** cửa sổ nổi giữ đúng input/output/kênh/slot, nhiều cửa sổ đồng thời, meter pre/post riêng và tham số live. Smart Comp có macro Boost/Balanced/Comp map sáu tham số, advanced/CUSTOM và scene cũ được giữ. Các thuật toán cơ bản chưa tương đương đầy đủ 17 module trong video/đặc tả.

Đường tiếng vẫn có matrix và FX legacy, hai rack native bốn slot mỗi input/output, physical output gain/polarity/delay/clip guard; VST3/FIR stereo OUT1/2 hiện có là đường riêng. Plugin thay loại khi mute, mới nạp bắt đầu bypass. Native parameter transition dài 64 mẫu; đổi cấu trúc reverb loại bỏ tail cũ. Không khẳng định mọi chỉnh cực đoan đều không nghe thấy chuyển tiếp.

## Bằng chứng phần mềm

- **GUI smoke:** 173 PNG, 0 bounds warnings, 34 editor control validations; thêm tên/16CH/chain/peak, bus/master, tám quick scenes và preset transfers. Dữ liệu thử nằm trong `ui-smoke/session-validation`, không ghi AppData người dùng.
- **GUI → actual JUCE callback:** [report](../build-windows-v07/plugin-audio-audit/report.txt) đạt **14 checks**, gồm **8 ca DSP chính** Vocal Color gain, legacy EQ, delay và EQ tám band ở cả input/output; target CH8/insert3, 48 kHz/128, tám kênh synthetic. Đo −12/+6 dB và echo 480→1440 mẫu; kênh khác không đổi; legacy controls bị vô hiệu trong eight-band mode và queued edits cũ bị chặn. Đây không phải mở ASIO/WASAPI thật.
- **Cả 17 module đổi tín hiệu:** `test_insert_sound_changes.cpp` dùng sine/multitone/noise/impulse/bursts tại Rack, tham số live, difference RMS >0,0005, bypass residual =0, isolation; đã chạy standalone Release/ASan. Không suy ra mọi tham số đều đổi tiếng: Tone advisory tolerance là tham số phân tích.
- **Tests mới:** `test_bus_master.cpp`, `test_extended_eq.cpp`, `test_rack_operations.cpp`, `test_session_state.cpp` kiểm tra đường sum/pan/map/meter, đáp tuyến/state/legacy EQ, snapshot đầy đủ và XML/preset. Tất cả có trong lượt 41/41.
- Engine/worker/TF/FIR/feedback/model và các bộ NumPy/SciPy vẫn nằm trong regression. Không lặp lại ASan toàn GUI/VST3 hoặc nghiệm thu tải realtime 32 kênh.

## Kết nối phần cứng: có tên driver, chưa có stream

| Thời điểm 26/10/2026 (+07:00) | Quan sát |
|---|---|
| 17:53:54 | Inventory chỉ có M-Audio ASIO. PnP có Unknown USB Device / Port Reset Failed / Code43, `VID_0000&PID_0001`; chưa thể xác định là mixer. |
| 18:11:23 | Đã thấy **DN32-LIVE ASIO Driver** cùng M-Audio. Muted check chọn DN32 nhưng không có 44,1/48 kHz, không mở stream: 0 IN/OUT, 0 callbacks, audio mute. |
| 18:12:57 | Inventory vẫn có tên DN32 ASIO nhưng chưa có audio endpoint DN32. Lần lọc PnP mới không thấy DN32/VID26A0 hoặc PortResetFailed; Code43 trước đó là lịch sử, không mô tả lỗi đang hiện. |

Bằng chứng: `m32-connected-audio-inventory.json`, `m32-muted-check-blocked.json`, `m32-audio-inventory-after-driver.json`, `m32-usb-pnp-after-driver.txt` trong `artifacts/windows-v07/`. Đăng ký driver không chứng minh thiết bị/card đã giao tiếp.

Người dùng xác nhận cắm USB-B phía sau DN32-LIVE bằng cáp 10 m cộng dây nối dài. Đã đề nghị thử dây dữ liệu 1–2 m cắm trực tiếp; phép thử đó còn chờ, chưa kết luận dây là nguyên nhân. Agent đã tải driver chính hãng **5.72.0** và kiểm tra chữ ký EXE/MSI Valid, nhưng **không chạy installer hoặc chấp nhận EULA**. Không suy ra phiên bản driver đang đăng ký từ phiên bản ZIP đã tải.

Xem [báo cáo kiểm tra kết nối](M32_CONNECTION_CHECK_REPORT.md), `scripts/check-m32-usb.ps1` và [hướng dẫn Card routing](M32R_USB_INSERT_GUIDE.md). Script mặc định chỉ liệt kê; tùy chọn muted check chỉ nhắm đúng DN32/M32, không fallback sang driver khác và không phát tiếng. Kết quả hiện tại chưa đáp ứng điều kiện mở stream, chưa có processed USB return, số đo latency khứ hồi, dropout/endurance hoặc hardware failover. Những mục này vẫn BLOCKED.

## Phần chưa hoàn tất

Chưa có bus/master plugin racks, stereo link, PC AUDIO capture/clock-SRC utility, recorder đa track, EQ/vocal FFT pre-post và GR/VU đầy đủ, Dynamics limiter/knee/gate panels, Vocal Clean denoise/de-harsh/autogain, Smart EQ target/Apply, FX modulation/stereo delay/reverb nâng cao, Assistant/radial menu hoặc VST3 per-channel/editor/state. VST3 hiện vẫn một stereo insert OUT1/2 cùng process và chưa nghiệm thu plugin bên thứ ba.

Chi tiết: [APP_UPGRADE_20261026.md](APP_UPGRADE_20261026.md), [FEATURE_MATRIX.md](FEATURE_MATRIX.md), [TEST_MATRIX.md](TEST_MATRIX.md), [DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md), [hướng dẫn editor](PLUGIN_EDITORS_GUIDE.md). Build/test offline đạt không phải chứng nhận dùng live hoặc hoàn tất toàn bộ đặc tả.

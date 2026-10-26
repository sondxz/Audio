# Phân tích video và hướng triển khai LiveAudioHost

Ngày đối chiếu: **26/10/2026**. Nguồn chính: [video hướng dẫn TITAN LIVE HOST 1.0.10](https://www.youtube.com/watch?v=UiGUPirC3KY), dài **36:14**, trên kênh TITAN SPEAKER VIỆT NAM. Trang ghi ngày xuất bản 03/10/2026. Sau khi người dùng hoàn tất CAPTCHA, đã truy cập player và xem các đoạn thao tác/keyframe dưới đây. Transcript chưa tải xong; phân tích dựa trên hình và phụ đề nhìn thấy, không khẳng định đã nghe và đánh giá liên tục từng giây hoặc xác định thuật toán riêng của nhà cung cấp.

## Điều cần làm đúng

Ứng dụng trong video là mixer xử lý âm thanh thật, với channel strip, chuỗi insert, cửa sổ plugin, scene, Master stereo và các màn hình phụ. Muốn giống cách sử dụng đó, chỉ có danh sách 17 tên và sáu núm cho mỗi tên là chưa đủ. Từng plugin cần tham số, nhóm điều khiển và dữ liệu đo phù hợp với xử lý của nó. Đóng editor phải giữ xử lý; chỉnh một instance không được thay thông số instance khác.

Mẫu mixer chính vẫn là ảnh số 2 người dùng chọn: tám dải kênh nhìn đồng thời, insert phía trên, pan/mute ở giữa, fader dài, chọn ngõ ra phía dưới, Master bên phải. Menu vòng tròn là trình chọn plugin bổ sung, không thay màn hình mixer chính.

## Những gì đã nhìn thấy trực tiếp trong video

Thời điểm là vị trí quan sát của player, không phải mốc bắt đầu chính xác của toàn chức năng. Các con số là giá trị trong bản demo, **không phải preset khuyến nghị cho hệ thống của người dùng**.

| Thời điểm | Quan sát | Ý nghĩa triển khai |
|---|---|---|
| [02:46](https://www.youtube.com/watch?v=UiGUPirC3KY&t=166s) | AI-FB có núm Strength lớn, giá trị 63%; bên phải mixer thấy chuỗi MIC 1 → AI-FB → Smart Comp → Master | Editor đơn giản theo tác vụ vẫn phải điều khiển DSP thật; cần hiển thị đúng target và chuỗi |
| [03:37](https://www.youtube.com/watch?v=UiGUPirC3KY&t=217s), [07:14](https://www.youtube.com/watch?v=UiGUPirC3KY&t=434s) | Menu vòng tròn có 17 mục và Assistant ở giữa, phủ lên mixer | Cần bộ chọn plugin chung cho đúng channel/slot, thêm chế độ danh sách; không suy ra Assistant đã dùng một mô hình AI cụ thể |
| [04:03](https://www.youtube.com/watch?v=UiGUPirC3KY&t=243s) | AUTO FB PRO có đồ thị phổ/filter lớn, danh sách notch thật đang hiển thị tần số/gain/Q, bộ đếm 2/5, AUTO MIC/AUTO CẮT HÚ, A/B/preset/bypass/reset | Cần FFT, lịch sử/danh sách cut, cơ chế ARM/lock/clear và đo kiểm false positives; hình notch không chứng minh chất lượng detector |
| [11:56](https://www.youtube.com/watch?v=UiGUPirC3KY&t=716s), [12:32](https://www.youtube.com/watch?v=UiGUPirC3KY&t=752s) | Smart Comp có một núm chính, các vùng BOOST/BALANCED/COMP, Smart Gain và meter IN/OUT | Núm này nên là macro kết hợp nhiều tham số nén. Chỉ đổi makeup gain chưa tái hiện workflow |
| [14:52](https://www.youtube.com/watch?v=UiGUPirC3KY&t=892s) | Chuỗi MIC 1 → 4K Line → Reverb 96 → Smart Comp → Master; Master có hai fader L/R và tám ô scene; tooltip 4K Line ghi 8 samples/0,17 ms tại 48 kHz | Cần Master stereo và quick scenes riêng. Số samples trên một plugin không phải độ trễ USB khứ hồi |
| [19:05](https://www.youtube.com/watch?v=UiGUPirC3KY&t=1145s) | DRUM LIVE KICK PROCESSOR có Detect, Bleed, Velocity, Target, Range, Dynamics, Punch, Body, Output; Learn 10 Hits, tiến độ 2/10, Auto Match/Quality và trạng thái listening/trigger | Có workflow phân tích/học nhiều hit và nhiều nhóm tham số; EQ/gate/transient cơ bản chưa thay thế được phần này |
| [26:16](https://www.youtube.com/watch?v=UiGUPirC3KY&t=1576s) | VOCAL CLEAN có Clean 55%, phổ trước/sau và đường reduction, meter IN/GR/OUT; các nhóm Low Cut, De-ess, De-harsh, Comp, Tone; Listen/Auto Gain, Mix, Output, preset/A/B | Cần framework tham số lớn hơn sáu biến và telemetry GR/FFT lấy đúng vị trí; từng khối cần thuật toán và enable có nghĩa |
| [29:49](https://www.youtube.com/watch?v=UiGUPirC3KY&t=1789s), [30:10](https://www.youtube.com/watch?v=UiGUPirC3KY&t=1810s) | Mixer tám strip/four inserts, panel chuỗi kênh được chọn, số plugin hoạt động; cửa sổ plugin đã đóng trong khi mixer vẫn hiển thị tín hiệu | Editor phải tách vòng đời khỏi DSP; cần chain summary và thao tác rõ ràng trên slot |
| [34:09](https://www.youtube.com/watch?v=UiGUPirC3KY&t=2049s) | Màn hình ĐA KÊNH có arm kênh, 2/26 physical, PC Audio và Master taps, timer, disk/buffer/lost blocks; nhãn RAW BEFORE INSERT và MASTER AFTER MASTER/BEFORE OUTPUT BUS | Đây là multitrack recording có tap points, không phải QA CSV. LiveAudioHost hiện chưa triển khai recorder này |

Các đoạn đã quan sát hiển thị **SSL ASIO Driver 1, 26 kênh, 48 kHz, buffer 256 mẫu, 5,33 ms**. Ảnh mixer người dùng gửi hiển thị **DN32-LIVE, 32 kênh, 48 kHz, 128 mẫu, 2,67 ms**. Đây là hai cấu hình khác nhau. Thời gian một buffer không bao gồm toàn bộ đường ADC → USB → phần mềm → USB → DAC; video không xác nhận khả năng hoạt động với M32R của bản build này.

## Chi tiết từ ảnh đính kèm, tách khỏi quan sát video

Ảnh FX DIGITAL cho thấy hai phần Echo và Reverb 96, với time/feedback/mix, low/high cut/width, predelay/decay/size/diffusion/damping, ducking và master levels. Đây là yêu cầu workflow echo/reverb; Digital FX hiện tại của LiveAudioHost là bitcrusher/sample-hold/echo mono nên **chưa tương đương**.

Ảnh DYNAMICS cho thấy Gate và Compressor có enable/đồ thị/tham số riêng: gate threshold/range/attack/hold/release; compressor threshold/ratio/attack/release/knee/makeup. Engine hiện có compressor và gate/expander cơ bản, chưa đủ toàn bộ các điều khiển đó. Không nên vẽ thêm núm rồi gắn vào tham số có ý nghĩa khác.

Ảnh Vocal Clean và Smart Comp củng cố các nhóm điều khiển nêu trên. Không suy ra denoising, AI, chống hú hoặc bảo vệ loa chỉ từ hình và tên thương mại.

## Đã sửa trong bản cập nhật này

- Smart Comp có macro **BOOST → BALANCED → COMP**, điều khiển sáu thông số DSP threshold/ratio/attack/release/wet/makeup. Các núm nâng cao vẫn chỉnh riêng; trạng thái khác mapping hiển thị **CUSTOM**, không tự đổi scene khi refresh. Mapping do LiveAudioHost thiết kế, không cam kết giống thuật toán/chất âm của plugin trong video.
- Sửa rack cũ có thể ghi sai tham số sau khi một cửa sổ khác thay module. Sửa feedback/candidate readout có thể giữ dữ liệu trước PANIC nếu rearm nhanh. Chú thích đường tín hiệu ghi đúng vị trí input gain.
- Làm rõ **Tone advisory tolerance** của AUTO FB chỉ ảnh hưởng gợi ý tone, không phải độ nhạy FFT auto-cut hay núm đổi âm thanh.
- Bổ sung kiểm tra 17 thuật toán thật sự đổi mẫu âm thanh, bypass và cách ly kênh; thêm audit gọi chính callback điều khiển GUI rồi chạy audio callback tổng hợp. Build Release cuối đạt **37/37 CTest**; GUI/audio audit đạt **12 checks** trong phạm vi công bố tại [TEST_MATRIX.md](TEST_MATRIX.md).

## Thứ tự phát triển tiếp

1. Hoàn thiện nền mixer: tám software buses, stereo link, stereo Master L/R, chain summary, reorder/copy, tám quick scenes và preset có schema/migration. Cột MASTER hiện tại vẫn đang chọn một USB OUT, chưa phải bus Master stereo.
2. Mở rộng state/editor theo module, thêm telemetry FFT/GR thật; nâng EQ nhiều band, Dynamics từng khối và Vocal Clean đúng nhóm tham số. Giữ scene cũ và kiểm thử đường GUI → DSP.
3. Làm FX Digital/stereo Reverb theo echo/reverb workflow, Drum learning, PC Audio capture/cross-clock SRC và phần nâng cao của các module còn lại; mỗi mục cần test riêng.
4. Hoàn thiện VST3 editor/state/chain, menu vòng/Assistant và multitrack recorder. Danh sách preset, recorder hoặc Assistant chưa có chức năng thật phải ghi rõ là chưa triển khai.
5. Nghiệm thu với **M32R USB/driver đúng card**: xác minh patch send/return, kênh 1–8 độc lập, nghe bypass/processed, đo loopback/round-trip, buffer/XRun, mất kết nối và chạy dài. Có thể phát triển và test buffer trước khi cắm mixer; các phép nghiệm thu này cần thiết bị thật.

Xem [FEATURE_MATRIX.md](FEATURE_MATRIX.md) để biết từng chức năng có/thiếu; [DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md) ghi điều kiện nghiệm thu từng mốc. Bản hiện tại là nền tảng đã cải thiện và kiểm tra, **chưa hoàn thành toàn bộ ứng dụng trong video hoặc MASTER DEVELOPMENT PROMPT**.

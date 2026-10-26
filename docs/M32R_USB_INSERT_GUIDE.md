# M32R: nhận USB, chèn plugin và trả về mixer

**Bản nâng cấp 26/10/2026:** có EQ tám band, tên kênh/16CH, thao tác chuỗi, tám Quick Scene/preset và trang **BUS** với tám software buses/Master stereo riêng. Cột chọn output vật lý trên mixer nay ghi **USB OUTPUT**. Xem [cách dùng và phạm vi nâng cấp](APP_UPGRADE_20261026.md), [kiểm tra dây USB/driver hiện tại](M32_CONNECTION_CHECK_REPORT.md). Với đường insert riêng từng kênh M32R, dùng profile **USB 1:1**; profile này cũng tắt các tuyến bus/Master phần mềm để tránh cộng thêm đường khác.

Luồng sử dụng: nguồn trên mixer → USB IN 1–N → xử lý input → routing → plugin output → USB OUT 1–N → kênh Card return trên M32R. Chọn N từ 1 đến 8 trong phần mềm.

## Kết nối và routing trên M32R

Kết nối máy tính với cổng USB-B của card DN32 ở mặt sau mixer. Cài driver ASIO đúng card đang lắp: DN32-USB hoặc DN32-LIVE. Hai card có hướng dẫn riêng: [DN32-USB, mục 3](https://mediadl.musictribe.com/media/PLM/data/docs/P0BIF/DN32-USB_QSG_WW.pdf), [DN32-LIVE, mục 4](https://mediadl.musictribe.com/media/PLM/data/docs/0606-ACY/QSG_KT_0606-ACY_DN32-LIVE_WW.pdf). Nếu card đang đặt chế độ chỉ 2 kênh, cần chọn chế độ USB có đủ kênh trước khi mở 8 kênh trong ứng dụng.

Ví dụ dùng tám ngõ mic/line Local 1–8:

1. Trên mixer, đặt **ROUTING → Card Out → Output 1–8 = Local 1–8** để gửi nguồn vật lý sang máy tính.
2. Đặt **ROUTING → Inputs/Home → Input 17–24 = Card 1–8** để nhận tín hiệu máy tính về kênh 17–24.
3. Dùng kênh 17–24 để nghe hoặc đưa vào mix; với N nhỏ hơn 8, dùng N kênh đầu của bank này.

Đây là ví dụ patch dựa trên các bank nguồn Local/Card và trang Card Out của [M32R User Manual, mục 2.3, trang 28–29](https://mediadl.musictribe.com/media/PLM/data/docs/P0BI9/M32R_M_EN.pdf). Tên tab có thể khác theo firmware. Giữ Card Out lấy nguồn Local; gửi lại chính kênh Card return vào USB sẽ tạo vòng lặp. Khi nghe đường đã xử lý, bỏ đường nguồn khô khỏi mix để tránh cộng cả hai đường. Phần mềm không tự đổi routing trên mixer.

## Mở luồng USB trong phần mềm

1. Mở **THIET BI**, giữ audio **MUTE**.
2. Bấm **TÌM M32R / ASIO**. Nút này tìm tên driver ASIO có DN32/M32; nếu không tìm thấy, chọn đúng ASIO và thiết bị thủ công ở phần cấu hình bên dưới.
3. Chọn **1–8 kênh USB**, rồi bấm **ÁP DỤNG USB**. Ứng dụng mở các ngõ vào/ra liên tiếp 1–N và kiểm tra số kênh driver thực sự mở.
4. Khi thành công, ứng dụng chuyển sang **MIXER** và vẫn MUTE. Nếu driver không mở đúng các kênh yêu cầu, kiểm tra thông báo ở THIET BI; chưa bật audio.

**ÁP DỤNG USB thay thế routing bằng IN n → OUT n**, tắt những tuyến khác, đưa pan về giữa và tắt FX send/return chung. Các plugin và tham số DSP đang chọn được giữ. Chức năng này mở kênh qua driver đã cài; không cài driver tự động.

## Chèn plugin theo giao diện mixer

- **Plugin input:** bấm một trong bốn ô **PLUGIN CHÈN** trên dải CH1–CH8 để mở cửa sổ riêng. Chọn module khi MUTE, chỉnh các núm rồi bấm **BYPASS** để chuyển sang **ON**. Module mới nạp bắt đầu bypass. Tín hiệu đi qua slot 1 → 2 → 3 → 4.
- **Ngõ trả về:** chọn **NGÕ RA / USB** dưới mỗi dải kênh. Chọn một Out sẽ thay thế các tuyến gửi của input đó bằng một tuyến unity tới Out đã chọn. Mặc định sau ÁP DỤNG USB là CH1 → Out 1, …, CH8 → Out 8.
- **Plugin output:** tại cột **USB OUTPUT**, chọn **OUT n**, rồi bấm một trong bốn ô **PLUGIN CHÈN / OUT n**. Các slot này xử lý tổng tín hiệu được routing tới đúng output đó, trước output gain/delay. Cột USB OUTPUT điều khiển output được chọn, không phải một tổng stereo chung của cả tám output.
- Nút ON/BYPASS cho phép bật hoặc bypass từng slot. Có thể chỉnh tham số trực tiếp khi audio đang chạy; cần tắt audio trước khi thay loại module. Khi đã kiểm tra meter input, ngõ ra và Card return, giảm mức nghe trên mixer rồi bấm **BẬT AUDIO**. **PANIC / MUTE** ngắt các ngõ ra và loại bỏ đuôi hiệu ứng còn lưu.

Cả 17 bộ xử lý có cửa sổ chỉnh riêng với núm, ô nhập số và đồ thị theo chức năng. Xem [hướng dẫn cửa sổ plugin](PLUGIN_EDITORS_GUIDE.md). Đóng cửa sổ không xóa plugin; mở lại giữ nguyên tham số. Các cửa sổ input/output có thể mở đồng thời.

Có 17 bộ xử lý native: Vocal Clean, Vocal Color, AI-FB LAB, AUTO FB PRO, Equalizer, Smart EQ, Dynamics, Smart Compressor, Optical Compressor, FX Delay, Digital FX, Reverb 96, Sub Pro, Drum Live, PC Audio, Analog và 4K Line. Đây là các thuật toán tích hợp, không phải bản sao plugin thương mại trong ảnh. AI-FB/AUTO FB chỉ cho phép tạo cut tự động sau khi **ARM AUTO CUT**; nhận diện có thể nhầm âm nhạc với feedback.

Trang **VST3** hiện vẫn là insert stereo riêng trên **OUT 1/2**; không phải thư viện VST3 nhiều slot trên từng kênh.

## Lưu và kiểm tra

**Save Scene** lưu routing, điều khiển DSP và cả hai rack plugin input/output. **Load Scene** recall khi MUTE và không tự bật audio; trạng thái ARM feedback không được khôi phục. **Save Driver** lưu thiết bị/kênh audio riêng. Scene của ứng dụng không lưu routing/scene trên M32R và không lưu state VST3.

Nếu input có meter nhưng mixer không có tiếng trả về, kiểm tra lần lượt ON/BYP của slot, MUTE/fader, Out được chọn, output meter và nguồn Card của kênh 17–24 trên mixer. Nếu không có input meter, kiểm tra card/driver và Card Out trên mixer trước.

Các kiểm thử phần mềm dùng tín hiệu tổng hợp. Chưa xác minh được luồng phần cứng M32R, XRun hoặc độ trễ khứ hồi; chưa có chứng nhận dùng live. Người dùng đã cắm USB nhưng driver/thiết bị chưa mở được stream ở lượt kiểm tra gần nhất; xem báo cáo kết nối ở đầu tài liệu. Chỉ số thời gian một buffer không phải số đo độ trễ USB khứ hồi. Cần kiểm tra thực tế với đúng máy tính, card, driver và số plugin sẽ dùng.

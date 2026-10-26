# Kiểm tra kết nối M32R LIVE và dây USB

Ngày kiểm tra: **26/10/2026**. Người dùng xác nhận cắm PC vào cổng **USB-B trên card DN32-LIVE phía sau mixer**, bằng dây USB **10 m cộng thêm đoạn nối dài**.

## Kết quả đọc từ máy theo thời điểm

- Lúc 17:53, EXE liệt kê backend, không mở stream: ASIO đã được biên dịch nhưng chỉ thấy **M-Audio AIR 192 4 ASIO**, chưa thấy DN32/M32. Registry ASIO lúc đó cũng chỉ có M-Audio.
- PnP lúc 17:53 có **Unknown USB Device (Port Reset Failed), Code 43**, với ID `USB\VID_0000&PID_0001`. ID chưa xác định hãng, vì vậy chưa đủ cơ sở kết luận thiết bị lỗi đó chính là mixer.
- Lúc **18:11–18:12**, kiểm tra lại đã thấy **DN32-LIVE ASIO Driver** trong backend và registry. Tuy vậy driver không báo được 44,1/48 kHz hoặc kênh, nên phép thử không mở stream và không có callback. PnP USB ở lượt 18:12 không còn hiển thị lỗi Port Reset Failed trước đó, nhưng cũng chưa thấy thiết bị MIDAS/DN32 hoặc ID `VID_26A0` đang kết nối. Việc driver đã đăng ký không xác nhận mixer đã online.
- Chưa mở được ASIO DN32-LIVE; chưa kiểm tra mic vào, USB return nghe được, XRun khi chạy hoặc độ trễ round-trip của mixer.

Bằng chứng ban đầu: `artifacts/windows-v07/m32-connected-audio-inventory.json` và `m32-connected-usb-pnp.txt`; kiểm tra lại: `m32-audio-inventory-after-driver.json`, `m32-usb-pnp-after-driver.txt`, `m32-muted-check-blocked.json`. Đây là kết quả tại thời điểm lấy mẫu, không phải giám sát liên tục.

## Cần loại trừ đường dây trước

Thử **dây USB dữ liệu A–B ngắn khoảng 1–2 m**, nối trực tiếp PC → card DN32-LIVE, bỏ dây nối dài và hub. Nếu dây ngắn làm thiết bị xuất hiện ổn định còn dây dài không, mới có bằng chứng đường dây dài là nguyên nhân.

Tài liệu [USB-IF về kiểm tra tương thích](https://www.usb.org/sites/default/files/3.2%20Interoperability%20Testing%20v1.0%20w%20USB%20Type-C.pdf) nêu mức tối đa 5 m cho cáp USB 2.0 tiêu chuẩn. Dây thụ động 10 m nối thêm có thể vượt phạm vi này. Cáp active/repeater là trường hợp khác và phải kiểm tra khả năng hoạt động với USB audio; không suy ra mọi dây ghi 10 m đều có cùng thiết kế.

USB/driver chưa nhận không phải lỗi plugin. Việc sửa giao diện hay DSP không thể bù cho một kết nối USB không enumerate được.

## Driver đã chuẩn bị

Trang [DN32-LIVE chính hãng](https://www.klarkteknik.com/en/products/0606-ACY), mục Downloads, đang liệt kê **USB Audio Driver 5.72.0**. Đã tải [ZIP từ CDN của hãng](https://cdn-media.empowertribe.com/beae4262b794494d89810b5e70c1db84/KLARK_TEKNIK_DN32-LIVE_v5.72.0_setup.zip) vào `artifacts/drivers/` và kiểm tra tĩnh:

- EXE/MSI có chữ ký **Valid — MUSIC Tribe Brands DE GmbH**.
- INF trong gói ghi model DN32-LIVE và hardware ID `USB\VID_26A0&PID_0500`.
- Agent **chưa chạy installer và chưa chấp nhận EULA**. Driver đã xuất hiện ở lượt kiểm tra sau, nhưng không xác định ai/thao tác nào đã cài nó; không ghi nhận việc cài driver là công việc agent đã thực hiện.

Xem `artifacts/drivers/dn32-live-driver-verification.json` để biết hash, nguồn và phạm vi xác minh. Gói này dành cho **DN32-LIVE**, không dùng thay cho driver của card DN32-USB khác model.

## Kiểm tra lại bằng dự án

Sau khi nối bằng dây ngắn và cài đúng driver, chạy:

```powershell
.\scripts\check-m32-usb.ps1
.\scripts\check-m32-usb.ps1 -Channels 1 -Seconds 10 -ObserveMuted
.\scripts\check-m32-usb.ps1 -Channels 8 -Seconds 10 -ObserveMuted
```

Mặc định script chỉ liệt kê. Tùy chọn `-ObserveMuted` chỉ mở driver DN32/M32 đã nhận diện, xác minh mask IN/OUT 1–N và giữ output mute, không fallback sang M-Audio/WASAPI. Kết quả gồm rate/buffer, callback, input peak, output peak, lỗi và XRun driver nếu có. Không ghi file audio và không lưu cấu hình người dùng. Stream được dừng/đóng trước khi kết luận và ghi báo cáo.

Muted stream đạt chỉ xác nhận phạm vi đó. Để nghe hiệu ứng cần patch Card Send/Card Return trên mixer, chọn plugin, bật slot và bật audio; cần phép đo loopback riêng để xác nhận độ trễ khứ hồi. Phần mềm không tự sửa routing phần cứng qua USB.

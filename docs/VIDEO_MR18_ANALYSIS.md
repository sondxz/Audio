# Ghi nhận video tham chiếu MR18

Nguồn: [video “PAT2 MIXER MIDAS MR18…”](https://youtu.be/8EIRpSK7nMU), thời lượng hiển thị **19:07**. Các ghi nhận dưới đây chỉ dựa trên ba khung hình đã quan sát trong phiên làm việc. Đây là video dùng **MR18**, không phải minh chứng kết nối M32R/DN32 và không phải phần trình diễn chi tiết đủ 17 plugin.

## Những gì thấy trực tiếp

| Mốc | Ghi nhận trên giao diện | Giới hạn suy luận |
|---|---|---|
| [0:17](https://youtu.be/8EIRpSK7nMU?t=17) | Driver ASIO MR18, 18 kênh, 48 kHz, buffer 128 mẫu / khoảng 2,7 ms. Hiển thị input 4,17 ms + output 5,50 ms = 9,67 ms; bộ đếm XRUN là 7. AI-FB hiện strength 78%; đồng thời có macro Smart Compressor. Chuỗi insert thấy được: **PC Audio → AI-FB → Smart Comp**. | 2,7 ms là thời gian một buffer ở cấu hình hiển thị. 9,67 ms là con số giao diện báo, chưa phải phép đo loopback độc lập. XRUN 7 là giá trị nhìn thấy ở mốc đó; không đủ để đánh giá độ ổn định toàn video hoặc nguyên nhân. Strength 78% không chứng minh thuật toán AI-FB hay thang điều khiển của bản này tương đương. |
| [4:23](https://youtu.be/8EIRpSK7nMU?t=263) | PC AUDIO trên CH3, pan `L10`, chuỗi xử lý đi tới Master; Master hiển thị **+5,6 dB**. | Đây là một thiết lập trong phiên trình diễn, không phải mức gain/pan được khuyến nghị cho show hoặc cho M32R. Không suy ra headroom hay chất lượng âm từ ảnh. |
| [10:09](https://youtu.be/8EIRpSK7nMU?t=609) | Có **OUTPUT PROCESSOR** riêng: Sub 1 mono → ASIO 15; Sub 2 mono → ASIO 16; Master L/R → ASIO 17/18. Master Analog bật. Thấy vùng EQ lớn + RTA, crossover LR24, `LOW CUT 116 Hz` bật và `HIGH CUT 90 Hz` tắt; delay 0, polarity Normal, gain 0. Limiter hiển thị threshold −3 dB, ceiling −0,5 dB, lookahead 1 ms, attack 1 ms, release 200 ms. Một cửa sổ M-AIR với preset Snare Drum/EQ phủ lên giao diện. | Tần số/công tắc crossover được chép theo **nhãn UI**; không suy diễn chúng thành đáp tuyến LP/HP hay cấu hình đấu loa. Ánh xạ ASIO 15–18 của MR18 không tự áp dụng cho DN32/M32R. Không đủ dữ liệu để xác nhận cách mắc limiter/EQ ở bên trong. |

Chưa dùng các đoạn khác để kết luận toàn bộ nội dung, tính năng đủ 17 module, độ an toàn chống hú hoặc chất lượng âm. Cửa sổ M-AIR phủ lên video cũng không chứng minh đường audio nào thực sự đi qua plugin máy tính.

## Đối chiếu với bản đang phát triển

Luồng insert nối tiếp ở input/output, routing ngõ trả, Smart Compressor macro, EQ tám band và bus/master đã có mã xử lý thực. Spectrum mới lấy **mẫu pre/post thật của từng insert**; FFT được tính ngoài callback, tách biệt với đồ thị mô hình đáp tuyến. Editor có A/B đầy đủ trạng thái và **51 preset khởi đầu native** (17 module × ba trạng thái), không phải preset sao chép từ video. Các thay đổi mới cần được xác nhận bằng build và log kiểm tra toàn bộ hiện hành.

Hai chế độ bổ sung được viết riêng, giữ chế độ sáu tham số cũ để scene cũ tiếp tục hoạt động:

- **Dynamics PRO:** 19 tham số cho gate/expander, compressor với soft knee, parallel mix và **final sample-peak limiter**, cùng meter gain reduction thực cho từng tầng. Limiter không có lookahead, latency của bộ xử lý là 0 mẫu; đây không phải limiter true peak. Nó khác phần giao diện trong video hiển thị lookahead 1 ms và không được mô tả là bản sao limiter đó.
- **Digital FX PRO:** 18 tham số cho **Echo + Reverb mono**, lọc echo, echo gửi sang reverb, ducking và các master/output gain. Echo 5–350 ms, predelay reverb 0–120 ms. TAP/BPM đổi thời gian echo trong giới hạn này; không có thuật toán stereo/Width và không được quảng bá là stereo delay.

Kiểm tra riêng `native_pro_rack` đã chạy đạt trên rack DSP thật: thời điểm echo/reverb, thay đổi tham số làm đổi âm, giữ đuôi khi chỉnh gain, xóa stamp khi đổi cấu trúc/chế độ, compression/gate/limiter, meter GR, tám kênh độc lập, bypass/reset và không cấp phát trong phần xử lý được giám sát. Kết quả đó chưa thay thế build GUI, toàn bộ CTest hoặc kiểm tra thiết bị.

Khối OUTPUT PROCESSOR với mọi điều khiển crossover/limiter trong video chưa được xác nhận tương đương đầy đủ trong bản này. Không khẳng định đã sao chép toàn bộ phần mềm, toàn bộ MASTER DEVELOPMENT PROMPT, hoặc đã đủ điều kiện chạy show. Xem [báo cáo kiểm tra và giới hạn](SHOW_PLUGIN_ACCEPTANCE.md), trong đó giữ nguyên lần vượt ngân sách 14,23 ms của benchmark trước khi thêm các chế độ PRO.

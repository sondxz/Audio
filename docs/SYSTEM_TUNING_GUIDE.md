# Workflow tham khảo System Tuning / FOH / BOH

## Chuẩn bị

Dùng Midas M32R Live để quản lý input physical, mic preamp, monitor/PA riêng. App LiveAudioHost nhận audio qua ASIO/WASAPI *nếu Windows xác nhận driver hoạt động*. Vị trí kênh USB M32 phụ thuộc mixer routing; không coi callback ch1 là cổng vật lý cố định. Hạ gain trước khi thử signal.

## FOH / monitor

Tab **Devices**: chọn driver, rate, buffer, active channels. Mặc định toàn bộ outputs qua app MUTE cho tới khi xác nhận.

Tab **Mixer/DSP/Routing**: chọn input và output để xử lý gain, HPF, bell EQ, gate/comp; một output nhận tổng có trọng số từ nhiều inputs qua matrix 32x32. Click ô 8x8 để bật/tắt 0/1; chọn output/input ở combobox và dùng `Send Level` để nhập hệ số gain [-2,+2]. Trang 1-8 / 9-16 / 17-24 / 25-32 cho toàn bộ 32x32. Output delay/polarity/ceiling dành cho alignment bench; **không áp vào main PA live chỉ vì preset**. Những tính năng chưa có: bus chuyên biệt Monitor/Groups/Sub, fader motorized, MIDI/OSC, DCA, solo/PFL.

Tab **Delay/Reverb FX**: chọn input và FX send; wet delay/reverb hiện chỉ cộng vào output 1/2. Trước khi mở monitoring, kiểm tra không routing vòng. Reverb đơn giản để thử đường tín hiệu; không có đặc tính production-grade.

Tab **VST3 Experimental**: scan thư mục VST3 mặc định; chỉ load plugin stereo đầu tiên vào tuyến output callback slot 1/2. Load/unload làm engine tự mute; không bù delay plugin. Không dùng plugin mới chưa kiểm tra ngoài hiện trường.

## Transfer Function

- Đưa tín hiệu gốc tham chiếu cùng clock tới INPUT ref và mic đo tới INPUT measurement. Cùng input device nhưng hai input khác nhau; kiểm tra chống feedback. RTA có thể hiển thị chỉ cần mic; TF cần reference thật.
- FFT 8192 ở 48k ~5.86 Hz/bin, Hann, overlap 50%, 7 segments trên snapshot 32768 samples (~683ms).
- Kiểm tra RTA, magnitude dB, phase degrees và coherence theo frequency. Phase wrapping ±180° là bình thường. Delay estimate từ GCC-PHAT có thể không ổn định trong phòng nhiều phản xạ.
- `Export TF/RTA CSV`: tệp trong Documents/LiveAudioHost. `tf_valid=0` nghĩa là các cột TF không nên dùng phân tích.
- Đây không thay thế proper mic calibration, matched clock/reference, alignment workflow và phép kiểm chứng bằng thiết bị đo ngoài.

## Sub array

Tab **Sub Array 2D/3D** có preset Broadside, Gradient/Cardioid Front/Rear, End-Fire (+Y) và L/R. Chỉnh spacing, frequency, gain/delay/polarity từng sub, xem differential front/rear trên trường sóng free-field.

**Định nghĩa trục**: +Y là hướng khán giả, -Y phía sau sân khấu; x sang ngang, z là cao độ. Công thức: `p(r) = Σ (gain_i * polarity_i / distance_i) * exp[-j * 2π f (distance_i / c + delay_i)]`. Nguồn được coi monopole tại 1 tần số, lấy 1 sub unit-gain cách 1m làm tham chiếu. Loa thật có directivity, thùng/ports, EQ, crossover và gradient; mặt đất và venue chưa được mô phỏng.

Đừng xem trị `delayMs` của preset là setpoint để đưa vào DSP loa thật; đo và xác nhận phase trong khu vực phủ audience trước.

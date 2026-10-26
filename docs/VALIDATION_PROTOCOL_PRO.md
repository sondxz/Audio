# QUY TRÌNH KIỂM CHỨNG ĐỘ CHÍNH XÁC — LIVEAUDIOHOST v0.3 (VN)

**Mức phát hành:** nghiên cứu / experimental. **KHÔNG ĐƯỢC XEM ĐÂY LÀ CHỨNG NHẬN SẢN PHẨM, MÁY ĐO SPL HAY CAM KẾT SỬ DỤNG SHOW.**

## 1. Phân loại phép kiểm tra (không đánh đồng)

| Cấp | Loại thử | Có thể chứng minh | Không chứng minh |
|---|---|---|---|
| A | Synthetic golden reference, unit tests | Các công thức DSP/FFT và giới hạn logic với đầu vào xác định | Độ chính xác sound card, microphone, Windows ASIO |
| B | Kết quả C++ so với SciPy độc lập | Cross-spectrum H1, coherence, phase, FFT với window giống nhau | Tương đương toàn bộ Smaart / VOS / RITA |
| C | Loopback điện thực tế với giao diện audio | Điện áp tương đối, level, đáp tuyến đường loop, transport delay và ổn định driver | SPL tại tai khán giả / phase trong phòng |
| D | Đo âm học đối chứng tại hiện trường | Mô hình thực tế: delay phát ra không khí, đáp tuyến loa/phòng | Tính đúng tuyệt đối mọi chỗ ngồi, phản xạ 3D mô phỏng |
| E | Soak test/show readiness | Độ tin cậy theo thiết bị, OS, driver, plugin và workload đã thử | Hoàn toàn không bao giờ xảy ra lỗi |

Một **kết quả thuật toán đúng** không đảm bảo **app chuyên nghiệp an toàn**, và một đồ thị nhìn giống Smaart không chứng minh kết quả tương đương.

## 2. Giai đoạn A/B: chuẩn toán học, được tự động hóa

- **Known gain**: dùng 32768 mẫu broadband tín hiệu chuẩn, mic = 0.5×ref → -6.0206 dB, coherence gần 1, phase gần 0 sau bù delay.
- **Known delay**: 0, 120, 1024, 6000 và -120 samples, đo ở 48 kHz; kiểm tra tính chính xác ±1 sample với tín hiệu kiểm thử noise khô (bộ thử thực tế đang yêu cầu bằng 0 sample).
- **Polarity inversion**: mic = -ref → phase ≈180°; không được tự động kết luận polarity thiết bị từ một dải tần âm học.
- **Zero input** và **mic-only**: không được phát hiện Transfer Function hợp lệ nếu không có reference; RTA vẫn được tính từ MIC.
- **Periodic tone**: không được tự tin công bố một delay duy nhất dựa trên một sóng sin đơn tần (nhiều độ trễ khác nhau tạo ra pha lặp lại).
- **NaN / Inf**: không crash, không truyền số vô hạn vào kết quả (giới hạn có chứng cứ ở core, chưa xác thực GUI).
- **RTA parseval**: tổng năng lượng trong các dải FFT sau window normalization phải gần năng lượng RMS theo thời gian.
- **GCC-PHAT linear**: zero-padding để ngăn wrap aliasing cho delay lớn hơn ±4096 samples. Peak-to-sidelobe, độ rộng thông dải và giới hạn đầu vào phải đạt trước khi công bố delay.
- **Masked spectral bins**: mask REF không đủ năng lượng; đường magnitude/phase không vẽ những bins thiếu bằng chứng. Bins coherence < 0.6 được ẩn trên đồ thị magnitude/phase; coherence gốc vẫn xuất trong CSV.

### Lệnh trên Windows (mở Developer PowerShell/VS Code)

```powershell
cmake -S . -B build-core -DLAH_BUILD_GUI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-core --config Release
ctest --test-dir build-core --output-on-failure -C Release
```

Tùy chọn nếu đã có Python, NumPy và SciPy: chạy `python tools/independent_reference_check.py build-core/lah_tf_cli.exe` để đối chiếu độc lập. **Không bắt buộc Python để chạy app; Python là công cụ QA.**

## 3. Giai đoạn C: loopback điện với Midas M32R Live / sound card

**Kiểm tra driver thực tế và routing card trước.** M32R LIVE phổ biến có khả năng USB 32×32, console rate 44.1/48 kHz; không giả định các kênh đều đang được map/active hoặc có 96 kHz.

1. Tạo scene thử nghiệm riêng, không nối tới tuyến Main PA / wedges / IEM đang dùng cho khán giả. Chuẩn bị đường monitor mức thấp và hardware mute độc lập.
2. Chọn **cùng một clock** và sample rate cho hai kênh ghi (ví dụ 48 kHz). Không dùng hai sound cards không đồng bộ khi so sánh phase, trừ khi có phương pháp đồng bộ/compensation thích hợp.
3. Tạo tín hiệu tham chiếu đo broadband an toàn, bảo đảm không clip ở input hoặc output; thu **REF** và **RETURN** thành hai kênh waveform cùng sample clock. `REF` phải lấy ở điểm phù hợp sao cho các processing cần khảo sát nằm **giữa** REF và RETURN.
4. Xuất mỗi kênh thành dãy sample float CSV một cột hoặc dùng DAW chuyển sang định dạng này; dùng `lah_tf_cli 48000 ref.csv return.csv tf.csv` (các tệp phải có thời điểm bắt đầu tương ứng, cùng chiều dài 16384–65536 samples). Không chỉ dùng screenshot meter để xác nhận gain/phase.
5. So sánh kết quả với Open Sound Meter / Smaart / VOS trên **cùng lần thu** nếu có thể, hoặc cùng reference path, cùng window/FFT size, đúng spectral averaging, compensation delay và smoothing. So sánh ở bins có năng lượng REF và coherence đủ cao.
6. Lặp 5–10 lần mỗi cấu hình đã hỗ trợ: 44.1/48 kHz; buffer 64,128,256,512 nếu driver chấp nhận. Lưu driver version, firmware, routing map, CPU, plugin chain, ambient conditions và SHA của app.
7. Để đo độ trễ round-trip **riêng của đường ứng dụng**, reference phải ở vị trí trước chuỗi input+app+output và return sau output vật lý; biết rõ mức trễ cố ý từ speaker delay/plugin và nguồn trễ của mixer. Kết quả TF acoustic bao gồm thời gian truyền âm trong không khí và KHÔNG THỂ trực tiếp coi là software latency.

### Các ngưỡng đề xuất để điều tra (NOT tiêu chuẩn công bố)

- Gain loopback sau hiệu chỉnh vị trí đo: độ chênh với chuẩn tham chiếu **≤0.25 dB** ở vùng có SNR và coherence tốt. Không đòi đáp tuyến sound card tuyệt đối bằng 0 dB nếu chưa hiệu chuẩn.
- Phase loopback sau xác nhận delay: độ chênh so với phép đối chứng **≤2°** trên vùng đủ tin cậy, với điều kiện cùng FFT/window/smoothing và bù thời gian.
- Độ trễ synthetic **≤1 sample**; đường loopback thật báo cáo **phân bố kết quả đo và điều kiện đo**, không bịa ra một giá trị chung.
- Coherence kỳ vọng cao (vd **≥0.98** trong loopback broadband SNR tốt); **không áp nguyên xi** ngưỡng này cho phép đo phòng nhiều phản xạ hoặc tiếng ồn.
- Gặp clip, clock mismatch, ASIO dropout, REF quá nhỏ, 0 valid bins, peak delay mơ hồ → **FAIL / INCONCLUSIVE**, không vẽ hoặc xuất kết quả như đã kiểm định.

## 4. Giai đoạn D: acoustic measurements / calibrated SPL

- Mic đo có calibration file theo serial và phương hướng đo thích hợp; interface/phantom/gain được kiểm tra theo manual và dải mức an toàn.
- Muốn hiển thị **SPL dB** phải dùng quy trình hiệu chuẩn bằng acoustic calibrator tương ứng (thường 1 kHz theo thiết bị), kiểm tra tuyến đo và toàn bộ uncertainty budget. `dBFS` là mức số trong giao diện, tuyệt đối KHÔNG chuyển nhãn sang `dB SPL` nếu chưa có dữ liệu hiệu chuẩn.
- IEC 61672-1 là chuẩn liên quan máy đo mức âm, IEC 60268-21 liên quan phương pháp đo âm học thiết bị hệ thống; ứng dụng này **chưa được thử/chứng nhận phù hợp** các tiêu chuẩn đó.
- So sánh riêng main LR, sub, front fill, monitor; ghi vị trí 3D từng MIC, khoảng cách, nhiễu nền, temperature khi liên quan tới c trong tính phase/time-of-flight.
- Chụp các phép đo ở nhiều vị trí khán giả; đối chiếu delay/time alignment sau khi có reference có ý nghĩa. Đừng tự áp delay dự đoán từ mô phỏng sub vào DSP/PA nếu chưa kiểm chứng bằng measurement thật.

## 5. Giai đoạn E: acceptance test / an toàn show

### Blockers hiện tại (đều CHƯA PASS)

- [ ] GUI JUCE/MSVC Windows 11 build thành công và ký mã/phát hành installer nếu phân phối.
- [ ] ASIO thực tế M32R LIVE qua USB hoạt động đủ input/output cần thiết, không sai channel-slot map.
- [ ] Xác nhận input/output loopback latency, plugin latency và callback deadline bằng thiết bị đo độc lập.
- [ ] Chạy 8–24 giờ liên tục trên ít nhất 2 máy thử, ghi max/percentile callback runtime, xrun và memory trend.
- [ ] ASIO sample-rate/device disconnect/reconnect, USB lỗi, restart console và khôi phục trạng thái mute an toàn.
- [ ] Stress test routing 32×32, scene/preset A/B, bật/tắt plugin, buffer chuyển đột ngột, gain mạch thử nghiệm.
- [ ] VST3 crash isolation hoặc danh sách plugin đã được xác thực theo phiên bản cụ thể. Plugin không đáng tin không được trong tuyến show.
- [ ] PDC, crossover đo kiểm, output limiter thật với peak/true-peak verification và kiểm soát âm lượng vật lý.
- [ ] Phương án failover **ngoài máy tính**: bypass trực tiếp hardware console/DSP, monitor/wedges có đường backup.
- [ ] Kiểm thử từng thuật toán simulation với benchmark tán xạ/hướng tính thực, không coi free-field point-source tương đương EASE/ArrayCalc.
- [ ] Vận hành rehearsal đủ nội dung, có operator, quy trình restore scene đã xác minh.

Nếu bất cứ blocker nào chưa đạt, app phải giữ nhãn **Experimental / Not show-ready**.

## 6. Ghi nhận/đối chiếu dữ liệu

Mỗi cuộc đo lập một thư mục có: `environment.txt` (driver, firmware, OS build, interface, buffer, rate); `reference.csv`, `measured.csv` (đúng thời điểm), `result.csv`, bản xuất đối chứng từ ứng dụng tham khảo, `notes.txt` (reference point, mic, cable/routing, calibration), `test_outcome.txt` (PASS/FAIL/INCONCLUSIVE cùng sai số).

Đối với mỗi đồ thị, phân biệt rõ: **tính toán dự đoán**, **tín hiệu loopback electrical**, **đo acoustic**, **SPL đã hiệu chuẩn**, và **đánh giá chất lượng chủ quan**. Một giao diện không được làm các loại dữ liệu này giống hệt nhau.

# LiveAudioHost v0.7.0 — Windows build và kiểm thử Audio Engine

**Bản nâng cấp 26/10/2026:** build Windows x64 Release thành công, **41/41 CTest đạt trong 48,55 giây**; GUI smoke **173 PNG / 0 bounds warnings / 34 editor validations** và GUI → callback audio **14 checks**, gồm tám ca DSP chính input/output. Log cuối: `artifacts/windows-v07/completion-final-build.log`, `completion-final-ctest.log`. [Báo cáo triển khai](docs/M32R_USB_IMPLEMENTATION_REPORT.md).

Đã thêm **tám software bus mono và stereo Master L/R** với send/gain/mute/constant-power pan/output maps/meter thật; **8 Quick Scenes** save/recall/rename/import/export và preset native một plugin/chain bốn slot; **EQ tùy chọn tám band** Bell/Shelf/HPF/LPF với kéo điểm và state tương thích legacy. Mixer có tên 32 input lưu scene, 8/16CH viewport, chain overview, move/copy/paste/remove slot và peak-hold/clip-reset thật. Profile USB 1:1 giữ return riêng và tắt buses; Master-only xóa matrix trước khi sum. Chưa có plugin rack riêng từng bus/master; đồ thị EQ là MODEL, không phải FFT đo.

**Kết nối M32R hiện chưa chạy được audio:** lúc 18:11 Windows đã thấy **DN32-LIVE ASIO Driver**, nhưng muted-check không có rate 44,1/48 kHz, không mở thiết bị, **0 kênh / 0 callbacks**. Inventory 18:12 vẫn có tên driver, chưa có endpoint DN32. Code43 lúc 17:53 là lịch sử và chưa xác định thuộc mixer; lọc PnP 18:12 không còn ghi lỗi đó. Người dùng xác nhận USB-B với cáp 10 m cộng nối dài; thử dây dữ liệu ngắn trực tiếp còn chờ, chưa kết luận nguyên nhân. Agent đã tải/xác minh driver chính hãng 5.72.0 nhưng không chạy installer/EULA; không suy ra phiên bản đang đăng ký từ ZIP đã tải. Xem [báo cáo kết nối và script kiểm tra](docs/M32_CONNECTION_CHECK_REPORT.md).

Mở `release-v07/LiveAudioHost.exe`; xem [hướng dẫn bản nâng cấp/EXE](docs/APP_UPGRADE_20261026.md), [editor/mixer/scene](docs/PLUGIN_EDITORS_GUIDE.md), [Card send/return M32R](docs/M32R_USB_INSERT_GUIDE.md), [ma trận tính năng](docs/FEATURE_MATRIX.md), [ma trận kiểm thử](docs/TEST_MATRIX.md) và [phần việc tiếp theo](docs/DEVELOPMENT_PLAN.md). VST3 per-channel/editor/state, recorder, PC AUDIO capture, nhiều thuật toán Vocal/Dynamics/FX nâng cao và hardware/latency nghiệm thu vẫn còn thiếu; không coi 41 test offline đạt là hoàn tất toàn bộ đặc tả.

## Lịch sử các mốc trước bus/master, scene library và EQ tám band

**Cập nhật kiểm chứng và Smart Comp 2026-10-26:** build Windows x64 Release thành công; **37/37 CTest đạt trong 45,73 giây**. Audit callback GUI thật → callback audio tám kênh tổng hợp đạt **12 checks**, với sáu ca DSP chính gain Vocal Color, mid gain EQ và echo ở cả input/output: đo đúng −12/+6 dB và delay 480→1.440 mẫu. Cả **17 module** có bài riêng chứng minh thay đổi tín hiệu, bypass và cách ly kênh, đã chạy standalone Release và ASan. Đây không phải chứng minh cả 102 tham số đều đổi tiếng; **Tone advisory tolerance** chỉ phục vụ phân tích. GUI smoke vẫn **163 PNG / 34 editor validations / 0 bounds warnings**. [Báo cáo và log kiểm thử](docs/M32R_USB_IMPLEMENTATION_REPORT.md).

**SMART COMP** có thêm núm lớn **BOOST / BALANCED / COMP** điều chỉnh cùng sáu tham số DSP; sáu núm nâng cao, gồm Makeup, vẫn chỉnh riêng. Scene chỉnh tay hiển thị **CUSTOM** và được giữ nguyên khi mở lại; mapping là thiết kế của ứng dụng, không cam kết giống thuật toán trong video. [Cách chỉnh plugin](docs/PLUGIN_EDITORS_GUIDE.md). Các chức năng nâng cao trong video/đặc tả còn thiếu được ghi rõ ở [đối chiếu video](docs/VIDEO_ANALYSIS.md), [ma trận tính năng](docs/FEATURE_MATRIX.md), [ma trận kiểm thử](docs/TEST_MATRIX.md) và [kế hoạch phát triển](docs/DEVELOPMENT_PLAN.md). Chưa nghiệm thu USB/ASIO M32R thật; inventory ngày **25/10/2026** chỉ thấy M-Audio, không thấy DN32/M32.

**Cửa sổ plugin 2026-10-25:** cả **17 plugin native** có editor nổi riêng ngay trên mixer, gồm núm xoay/nhập số, ON/BYPASS/RESET, đồ thị theo chức năng và meter pre/post của đúng slot. Mở nhiều editor input/output không đổi target của cửa sổ đang chỉnh. Tham số điều chỉnh được khi audio chạy; thay loại plugin cần MUTE. Có chuyển tiếp 64 mẫu khi đổi tham số và xóa đuôi reverb khi đổi cấu trúc. [Hướng dẫn từng cửa sổ](docs/PLUGIN_EDITORS_GUIDE.md).

**Cập nhật M32R USB 2026-10-25:** giao diện mixer toàn chiều ngang theo mẫu 8 dải kênh, 4 ô plugin mỗi input, chọn USB OUT dưới mỗi dải và 4 ô plugin riêng trên từng output qua cột Master. Thư viện có **17 bộ xử lý native**, bổ sung Analog và 4K Line. Tab **THIET BI** có tìm ASIO DN32/M32 và mở **1–8 kênh USB hai chiều**; ÁP DỤNG USB đặt lại routing IN n → OUT n, pan giữa và tắt FX send/return chung. Scene lưu cả rack input và output. Các module là thuật toán tích hợp; chất âm không được khẳng định giống sản phẩm trong ảnh tham khảo.

Chạy `release-v07/LiveAudioHost.exe`. Xem [hướng dẫn USB/plugin và routing M32R](docs/M32R_USB_INSERT_GUIDE.md) và [kết quả kiểm thử bản USB console](docs/M32R_USB_IMPLEMENTATION_REPORT.md). Cần driver đúng card và cấu hình Card Out/Card return trên mixer; phần mềm không tự điều khiển routing M32R. Chưa nghiệm thu với mixer thật hoặc đo độ trễ USB khứ hồi. Các báo cáo và số liệu 15 module bên dưới là lịch sử trước bản cập nhật này.

**Cập nhật AI-FB/native rack 2026-10-23:** giao diện console graphite/xanh ngọc và **15 bộ xử lý DSP tích hợp**, 32 input × 4 slot. **30/30 Release, 28/28 ASan core đạt; 402 render native + 30 render feedback đối chứng; model parity 25 clip/495 frame; GUI smoke 73 PNG, 0 bounds warnings.** **AI-FB LAB có neural model đã huấn luyện trên dữ liệu tổng hợp; AUTO FB PRO có detector phổ/growth. Cả hai chỉ tạo cut tự động sau khi người vận hành ARM.** Xem [triển khai, kiểm thử và giới hạn AI/feedback](docs/AI_FEEDBACK_IMPLEMENTATION_v0.7.0.md) và [model card](docs/FEEDBACK_MODEL_CARD.md). Chưa xác minh phân biệt nhạc/feedback ngoài thực tế.

Đã build `LiveAudioHost.exe` với MSVC/JUCE 8.0.15 và ASIO bật. Mở `release-v07/LiveAudioHost.exe`, vào **Mixer → INSERTS** để chọn bộ xử lý trên input; chọn loại/chỉnh tham số khi audio MUTE, module vừa nạp bắt đầu bypass. **LAB ONLY / NOT SHOW READY**: chưa nghiệm thu M32R, SPL hiệu chuẩn phần cứng, VST3 bên thứ ba hay chạy 24 giờ. Benchmark cuối **32 AUTO FB, 128 AI-FB và 128 Reverb đều có thời điểm vượt chu kỳ buffer 128 mẫu**; không cam kết cấu hình đó realtime đạt dù lần trước nhanh hơn.

Feedback mặc định disarmed; **DISARM giữ cut đã có, Clear notches bỏ cut**. Model có thể nhầm tone nhạc tăng mức với tiếng hú; score không phải xác suất đã hiệu chuẩn. [Báo cáo giao diện/native rack 27/25/71 PNG](docs/UI_AND_BUILTIN_RELEASE_REPORT_v0.7.0.md) là baseline lịch sử trước bản AI-FB hiện tại.

Xem [báo cáo theo giai đoạn và trạng thái từng module](docs/WINDOWS_DEVELOPMENT_REPORT_v0.7.0.md). Chạy `scripts/build-windows-v07.ps1` để build/test và đóng gói trong `release-v07`. Các mục source-only bên dưới là lịch sử trước phiên Windows này.

Các chức năng Phase Engine, 7 sub-array presets/4 probes, coverage circle/arc/custom CSV và 12 calculator modes của giai đoạn trước vẫn được giữ. Xem [trạng thái tính năng và hướng xử lý](docs/FEATURE_STATUS_AND_ROADMAP_v0.7.0.md). Scene M32R FNBOS2024 đã đọc offline; chưa có importer .scn hoặc kết nối mixer. Quan sát WASAPI mute thực trước đó 2×10 giây có callback và output zero, nhưng **mỗi lần báo 1 XRun: chưa đạt nghiệm thu audio thực**; nguyên nhân chưa xác định. Các kết quả 24/22/54 PNG và 19/17 trong báo cáo cũ là mốc lịch sử, đã có mốc phần mềm mới ở trên.

---

# Lịch sử v0.7.0 — Windows Acceptance & QA Update

**Developer source for Windows, not a verified Windows build; NEVER use in a live PA show yet.** This source tree is based on v0.6, with an additional release-time safety latch, GUI-side QA CSV logger, 24-hour QA log analyzer, synthetic cross-module integration tests, relative impulse DSP, and a Windows PowerShell build/release script.

**Đọc trước:** [Windows / M32R acceptance](docs/WINDOWS_ASIO_ACCEPTANCE_v07.md), [Kết quả validation v0.7](docs/RELEASE_VALIDATION_REPORT_v0.7.md).

### Build từ Windows

Trong Developer PowerShell for VS, sau khi có JUCE8 / MSVC / Ninja / CMake:

```powershell
.\scripts\build-windows-v07.ps1 -JuceRoot 'C:\SDK\JUCE'
```

Nếu build+tests thật sự đạt, `release-v07/LiveAudioHost.exe` sẽ được tạo **trên máy Windows đó**. Chưa có .exe nào được tạo từ phiên làm việc Linux này. Dùng `QA LOG: START` trong UI để ghi nhận log trạng thái từ device thực rồi phân tích bằng `python tools/qa_analyze.py <path.csv> --min-hours 24`.

### Kiểm thử lõi (không cần JUCE)

```sh
cmake -S . -B build-core -DCMAKE_BUILD_TYPE=Release -DLAH_BUILD_GUI=OFF
cmake --build build-core --parallel
ctest --test-dir build-core --output-on-failure
```

**Cảnh báo:** QA log không thay thế hiệu chuẩn SPL, phép đo loopback, kiểm thử USB/ASIO và đo độ trễ thật. A/C/Z weighting vẫn là tính toán nghiên cứu, FIR 2 kênh experimental, VST3 không có crash isolation, hạn chế cấp phép JUCE/ASIO cần được rà soát.

---

# LiveAudioHost v0.6.0 — Engineering QA integration (C++17/JUCE source)

**STATUS: DEVELOPMENT SOURCE / NOT SHOW READY.** Không có EXE Windows đã được biên dịch, không có chứng cứ ASIO M32R, SPL hiệu chuẩn thiết bị thực, VST3 realtime hoặc vận hành PA an toàn. Phần bổ sung mới có kiểm thử core OFFLINE, không tương đương phần mềm đã được chứng nhận.

## v0.6 (2026-10-22) — QA upgrades carried out

- Chuyển **H1/FFT/GCC-PHAT** từ GUI timer sang `MeasurementWorker`, GUI chỉ lấy ảnh chụp kết quả đã xuất bản.
- `SplWorker` mới xử lý 8192-sample blocks liên tiếp (không chồng dữ liệu) và cho **indicative** A/C/Z, Leq A/C/Z, Fast/Slow block estimates; phát hiện capture gap, tự hủy calibration khi đổi input/device. **Không đạt IEC 61672**.
- **Realtime FIR experimental**: short 129/257-tap stereo OUT 1/2, nạp sau khi MUTE và tách callback; giữ bypass sau restart; kiểm tra kết quả với convolution độc lập, clipping guard; **chưa có latency PDC**.
- Mixer bổ sung **Input Mute** (smooth per-buffer) và Input Balance L/R OUT 1/2; lưu/khôi phục theo Scene.
- Khi PANIC/MUTE, delay/reverb bỏ tail cũ với epoch tags O(1), ngăn old echo khi bật lại.
- CMake bổ sung **11 nhóm test**; GCC/Clang/Clang ASan+UBSan đạt **11/11** trên Linux. Kết quả **chỉ dành cho core C++**. Offline benchmark có trường hợp gần đầy budget callback, phải đo trên Windows thật.
- Hướng dẫn đọc [Validation Report v0.6](docs/RELEASE_VALIDATION_REPORT_v0.6.md) và [module acceptance matrix](docs/MODULE_ACCEPTANCE_MATRIX_v0.6.md). Chạy `scripts/qa-v06-windows.ps1` để test trên máy bạn.

---
# LiveAudioHost v0.5.0 — Windows desktop engineering LAB source

> **CẢNH BÁO / STATUS: EXPERIMENTAL — NOT SHOW READY.** Ứng dụng native C++17/JUCE source; không có EXE Windows đã được build và xác minh. Không có thử nghiệm thực tế với Midas M32R Live, microphone calibrator, Smaart hoặc sound system. Không đưa vào tuyến PA chính cho show.

## v0.5 (2026-10-22) — Dark rack UI, calibrated SPL LAB, FIR designer, polar import

- **Theme:** dark graphite/teal kiểu audio rack chuyên nghiệp (màu lấy cảm hứng từ ảnh người dùng, không sao chép giao diện/thương hiệu).
- **SPL / Calibration:** chỉ hiển thị mức A/C/Z tính toán khi đã thu tone calibrator 1 kHz 94/114 dB hợp lệ. Từ chối clip/silence/noise, tự vô hiệu khi phát hiện thay đổi input/rate/driver. Đây là đo khối FFT có hiệu chuẩn tham chiếu **KHÔNG phải** sound level meter đạt IEC 61672.
- **FIR Designer:** LP/HP Blackman-sinc linear phase 129–4097 taps, plot magnitude, group delay, xuất coefficients CSV. **Offline only — CHƯA gắn FIR vào tín hiệu PA live.** Lõi FIR được đối chiếu SciPy độc lập.
- **System Measurement:** vẫn dùng H1/Coherence/Phase/Delay từ 0.3; bổ sung A/B snapshots + overlay, xuất CSV bao gồm A/B.
- **Acoustic Prediction:** Import CSV profile frequency/azimuth/magnitude/unwrapped phase cho sub grid (nếu dữ liệu hợp lệ, không extrapolate). Dữ liệu áp suất **tương đối**, mô hình direct wave; chưa có acoustic simulation theo room/GLL thực.
- **Realtime telemetry:** đếm callback over-deadline và max callback runtime, phân biệt với XRun driver và round-trip latency.
- **Offline tests:** GCC Linux 6/6; Clang Linux 6/6; Clang ASan/UBSan 6/6; FIR vs SciPy 24/24. **Không** được coi là chứng chỉ thử nghiệm phần cứng.

Xem [tài liệu kiểm chứng v0.5](docs/V05_ENGINEERING_VALIDATION.md), [báo cáo test](docs/RELEASE_VALIDATION_REPORT_v0.5.md), [cách build Windows](docs/BUILD_WINDOWS.md).

---

## NEW in v0.4 (2026-10-22): Engineering Calculators + Front-Fill designer

Nhận diện các ý tưởng kỹ thuật từ ảnh tham chiếu AcousticLab VN. Theo xác nhận của người dùng, các số trong ảnh là kết quả tính toán từ phần mềm chuyên gia và được dùng làm mốc đối chiếu khi xác định đầy đủ đầu vào, đơn vị và giả thiết. Bổ sung hai tab JUCE **Engineering Calculators** (Ohm/dBu, SPL lý tưởng, threshold RMS lý thuyết, cable & damping, ISO 9613-1 air loss, phase/delay nhiệt độ), **Front-Fill / Coverage** (tự tính số lượng loa và vùng phủ tia -6 dB, chế độ Manual và Arc; không phải bản đồ SPL). Subwoofer simulation đã thêm nhiệt độ để thay tốc độ truyền âm.

Các thuật toán là mã nguồn mới, không trích xuất/copy code giao diện AcousticLab. Các đối chiếu cần khớp tần số, nhiệt độ, độ ẩm, áp suất, khoảng cách và định nghĩa mức; chênh lệch với ảnh được ghi nhận để kiểm tra, không kết luận chất lượng phần mềm nguồn khi chưa khớp các điều kiện đó. Báo cáo kiểm thử lịch sử v0.4: 4/4 suite pure C++ đạt trên GCC & Clang/Linux; trạng thái Windows hiện tại xem báo cáo đầu README.

Xem: [Engineering Calculators](docs/ENGINEERING_CALCULATORS.md) | [v0.4 validation](docs/RELEASE_VALIDATION_REPORT_v0.4.md).

## v0.3 measurement accuracy validation update (2026-10-22)

**Đã bổ sung và chạy kiểm thử thực tế TRÊN LÕI C++ OFFLINE**: linear GCC-PHAT delay finder có kiểm tra spectral flatness, H1 Transfer Function bù delay, phân biệt bin thiếu reference / coherence thấp, RTA dBFS/bin RMS không gắn nhãn SPL, CLI offline để so sánh dữ liệu với bên thứ ba, independent cross-check với SciPy, 3/3 core tests và 3/3 ASan/UBSan tests trên Linux. Không đồng nghĩa app/show đã được chứng nhận. **Chưa có số đo độ trễ loopback M32R hoặc GUI đã build được trên Windows.**

Tài liệu thiết yếu: [VALIDATION_PROTOCOL_PRO.md](docs/VALIDATION_PROTOCOL_PRO.md) và [RELEASE_VALIDATION_REPORT_v0.3.md](docs/RELEASE_VALIDATION_REPORT_v0.3.md).

Lệnh CLI sau khi build core (trên Windows đặt `.exe`): `lah_tf_cli 48000 reference.csv measured.csv result.csv`. Mỗi file đầu vào là **một cột số mẫu float**, cùng sample rate và số frame, không có header; kết quả TF dùng reference đúng tuyến, nếu delay finder không đủ dữ liệu thì phase chưa được xác thực. Bộ kiểm tra đối chứng tùy chọn: `python tools/independent_reference_check.py build-core/lah_tf_cli` (cần Python/NumPy/SciPy riêng cho QA).

**Quan trọng:** v0.2 tính FFT TF từ các cặp REF/MIC chưa bù delay và GCC-PHAT chỉ trên FFT 8192 (có thể wrap). v0.3 sửa trong phạm vi các case có golden input; không tuyên bố mọi phép đo acoustic đã chính xác.

## Các mô-đun thực sự có mã nguồn trong bản 0.2.0

| Mô-đun | Hiện trạng / giới hạn |
|---|---|
| Windows native, ASIO/WASAPI, device/channel selection, meters, mute | Có code JUCE. Cần build Windows + thử với thiết bị. Mặc định audio mute. |
| 32×32 routing matrix | Dữ liệu matrix thực, mixer cộng tuyến tín hiệu theo gain cho từng cặp input→output. Bảng UI chia 4×4 trang, mỗi trang 8×8. | 
| Mono strips | Input gain; 1 HPF Butterworth 2nd order; 1 PEQ bell; expander gate; feed-forward compressor. Các controls cập nhật bằng atomic. Chưa có channel pan/group/solo. |
| Speaker outputs | Gain, delay 0–250 ms, đảo polarity, hard clip guard theo ceiling dBFS. **Hard clip guard không thay thế speaker true-peak limiter.** |
| Delay/reverb FX | Có stereo feedback delay / reverb comb đơn giản, FX send per input, wet return ra OUT 1/2. Không có convolution, de-esser, pitch correction. |
| VST3 experimental | Worker-thread scan VST3 mặc định, single stereo insert vào output slot 1/2, load/unload khi audio callback tách tạm thời, bypass mặc định. **Chưa được build trên JUCE/Windows, chưa kiểm tra với VST3 thật, không cô lập crash.** Không có multi-plugin rack, editor và state serialization. |
| FFT/RTA | 8192-point FFT với Hann, average 7 segments / 32768 sample snapshot tại 48 kHz (~5.86 Hz/bin). Lấy mic từ input thực; có thể xem RTA khi reference vắng. |
| Transfer Function | Cross spectrum H=S_yx/S_xx, phase, coherence, GCC-PHAT delay estimate khi coherence đủ cao. Không hiển thị TF hợp lệ khi thiếu reference. CSV export. **Chưa hiệu chuẩn delay của hệ thống đo hoặc benchmark cùng Smaart/Open Sound Meter.** |
| Sub array | Tính áp suất phức tại điểm nghe 3D từ monopole free-field, hiển thị 2D top-down và lưới perspective 3D; preset Broadside/Cardioid/End-Fire/L-R. SPL **tương đối** so với một monopole cách 1 m, không tính ground/room/manufacturer speaker data. |
| Scenes | Lưu/khôi phục 1 XML scene cục bộ (routing, DSP, FX, polarity/delay). Recall bắt buộc mute trước; không lưu VST plugin state. |

Các nút ở những màn hình kể trên có logic kết nối với core (không phải mock UI), nhưng **chưa được xác nhận hoạt động sau khi build JUCE trên Windows**. Các phép đo phải sử dụng thiết bị và đường tham chiếu thật để có kết quả có ý nghĩa.

## Cài đặt / chạy thử

Cần Windows 11, MSVC C++ Build Tools, CMake >= 3.22, Ninja và source **JUCE 8.0.15**. Mở Developer PowerShell trong thư mục dự án:

```powershell
.\scripts\build-windows.ps1 -JuceRoot "C:\SDK\JUCE"
```

Hoặc nếu chỉ muốn chạy kiểm thử lõi C++ mà chưa có JUCE:

```powershell
cmake -S . -B build-core -DLAH_BUILD_GUI=OFF
cmake --build build-core --config Release
ctest --test-dir build-core --output-on-failure -C Release
```

**Chi tiết:** [Build Windows](docs/BUILD_WINDOWS.md), [Kiến trúc](docs/ARCHITECTURE.md), [Hướng dẫn tuning](docs/SYSTEM_TUNING_GUIDE.md), [Độ trễ](docs/LATENCY_AND_VALIDATION.md), [Giới hạn](docs/KNOWN_LIMITATIONS.md).

### Thử tín hiệu với Midas M32R Live (không dùng show)

1. Cài driver M32R chính hãng trên Windows và kiểm tra device type ASIO cùng kênh USB thực tế. 32×32 DSP slots *không* tự động xác định chính xác số card channels vật lý M32.
2. **Để mute** và lựa chọn cặp input/output qua device selector. Mở tab Mixer, xem matrix: mặc định IN1→OUT1, IN2→OUT2 ...; mọi đường còn lại off. Lưu ý routing trên M32 là bước riêng biệt.
3. Nếu định phát qua loa/monitor, giảm mức gain tại mixer/ampli và chuẩn bị phương án bypass ngoài ứng dụng. **Không tạo vòng phản hồi input→output vô tình.**
4. Nhấn BẬT AUDIO chỉ khi đã xác minh routing; nhấn PANIC để ngắt audio phát từ app tức thời ở callback tiếp theo.
5. Để đo Transfer Function: cung cấp **hai input vật lý khác nhau**, một reference và một measurement mic. Kiểm tra coherence theo tần số, không dùng một trị trung bình để khẳng định việc tuning chính xác.
6. Sub Array tab là bài toán mô hình; **không lấy delay từ preset áp trực tiếp lên M32/processor** nếu chưa đo hệ thống thật và kiểm tra phase/polarity/delay bằng reference measurement.

## Độ trễ hiện tại (chưa được đo trên thiết bị)

**Chưa có số đo round-trip latency ASIO M32R Live.** Nếu chạy ở 48 kHz, 64/128/256 samples tương đương 1,33/2,67/5,33 ms mỗi buffer. Đó *không phải* latency khứ hồi: driver/hardware, DSP delay, VST3 latency, buffering và OS scheduling đều có thể tăng hoặc thay đổi kết quả. Speaker delay là thêm có chủ ý 0–250ms; VST3 báo số samples nhưng có plugin không báo đầy đủ. Xem [LATENCY_AND_VALIDATION.md](docs/LATENCY_AND_VALIDATION.md).

## Giấy phép

JUCE và thành phần VST3/ASIO tuân theo giấy phép của chủ sở hữu, **không tự giả định tự do phân phối binary thương mại**. Không đóng gói source JUCE hoặc plugin bên thứ ba trong ZIP này.

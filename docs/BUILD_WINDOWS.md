> **v0.7 (Windows):** đã build MSVC/JUCE trong phiên Windows; xem `WINDOWS_DEVELOPMENT_REPORT_v0.7.0.md`. Dùng `scripts/build-windows-v07.ps1` từ PowerShell thông thường với VS 2022 C++/SDK, hoặc từ Developer PowerShell với Ninja. Script nhận CMake/JUCE trong `.tools` khi có, hỗ trợ `-EnableSciPy -PythonExecutable <path>`, ghi log trong `artifacts/windows-v07`, và chỉ đóng gói sau khi CTest đạt. Không cần sửa execution policy của máy. Các hướng dẫn chi tiết cũ bên dưới là lịch sử.

# Build Windows 11 (VS Code + MSVC)

## Chuẩn bị (không tự cài đặt phần mềm)

1. Visual Studio Build Tools 2022/2026: cài **Desktop development with C++** và Windows SDK, MSVC.
2. CMake 3.22+, Ninja và Git (không bắt buộc nếu đã tải JUCE ZIP).
3. VS Code + các extension C/C++, CMake Tools nếu muốn.
4. Tải và giải nén **JUCE 8.0.15** từ trang release chính thức vào `C:\SDK\JUCE`. Tại đây phải thấy `CMakeLists.txt` và thư mục `modules`.
5. Kiểm tra `cmake --version`, `cl`, `ninja --version` trong **Developer PowerShell for VS** (chọn x64).

## Build nhanh

```powershell
cd C:\duong-dan\LiveAudioHost
.\scripts\build-windows.ps1 -JuceRoot "C:\SDK\JUCE"
```

Nếu PowerShell chặn script, sử dụng lệnh CMake bằng tay bên dưới; **không cần sửa chính sách bảo mật của máy**.

## CMake bằng tay (Ninja/MSVC)

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DJUCE_ROOT="C:/SDK/JUCE" -DLAH_ENABLE_ASIO=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

**Chỉ build test lõi** nếu JUCE chưa tải:

```powershell
cmake -S . -B build-core -G Ninja -DCMAKE_BUILD_TYPE=Release -DLAH_BUILD_GUI=OFF
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

## Tìm file EXE

Sau khi build GUI thành công, thường nằm tại:

`build\LiveAudioHost_artefacts\Release\LiveAudioHost.exe`

Vị trí cụ thể tùy JUCE/CMake; dùng `Get-ChildItem -Path build -Filter LiveAudioHost.exe -Recurse`.

## M32R Live / ASIO

- Mặc định Windows build có `-DLAH_ENABLE_ASIO=ON`. ASIO nguồn trong JUCE 8.0.15; không cần thêm code driver vào app. Nhà sản xuất mixer vẫn cần cấp driver M32R phù hợp Windows.
- Nếu build ASIO xảy ra lỗi cấp phép hay compiler khi phân phối: thử `-DLAH_ENABLE_ASIO=OFF`, dùng WASAPI (độ trễ/số kênh sẽ khác); xem lại nghĩa vụ giấy phép trước khi chia sẻ bản build.
- Đổi **sample rate / buffer** trong device selector theo đúng các lựa chọn mà thiết bị driver cung cấp. Không ép 32 input/output nếu driver không đưa ra.
- Không khẳng định 5–10ms; `Input/Output latency` chỉ lấy từ driver, không phải phép đo loopback.

## Khắc phục lỗi thường gặp

| Triệu chứng | Cách xử lý |
| --- | --- |
| `cl.exe not found` | Mở **Developer PowerShell for VS**, kiểm tra MSVC Build Tools. |
| `JUCE source missing` | Truyền `-DJUCE_ROOT=` với đường dẫn chứa `CMakeLists.txt` của JUCE. |
| `ninja not found` | Cài/đưa Ninja vào PATH theo hướng dẫn công cụ. |
| Không có `ASIO` trong app | Build ON, cài driver ASIO thật và khởi động lại ứng dụng. |
| Không có input signal | Kiểm tra M32 routing USB/card, chọn đúng cổng và input enable. |
| Nghe hú/feedback | **PANIC/MUTE** ngay, hạ gain / cô lập tuyến phản hồi rồi kiểm tra routing. |
| CPU/XRun cao | Tăng buffer, tắt ứng dụng chạy nền, kiểm tra driver và cổng USB. |
| Cấu hình không lưu được | Kiểm tra quyền thư mục dữ liệu ứng dụng Windows: `AppData\Roaming\LiveAudioHost\audio-device.xml`. |


## v0.5 additions

After launch, the new tabs are **SPL / Calibration** and **FIR Designer (offline)**. The Measurement tab also has `SAVE A`, `SAVE B` and `CLEAR A/B`; Sub Array has `IMPORT POLAR CSV` and `OMNI MODEL`.

On Windows verify mathematical components first:

```powershell
cmake -S . -B build-core -DLAH_BUILD_GUI=OFF
cmake --build build-core --config Release
ctest --test-dir build-core --output-on-failure -C Release
python tools/independent_fir_check.py build-core/lah_fir_cli.exe
```

SciPy checker requires separately installed NumPy/SciPy; it is used only for independent engineering QA, not required to run app. Build GUI requires JUCE exactly as above. No automatic driver installation, no requirement to connect PA to test the software.

## v0.6 offline QA
Run in **Developer PowerShell for Visual Studio**:

```powershell
.\scripts\qa-v06-windows.ps1
# Optional, only if Python NumPy/SciPy are installed:
.\scripts\qa-v06-windows.ps1 -EnableSciPy
```

This checks core algorithms only; NOT a Windows GUI, real ASIO device, calibrated SPL or third-party VST3. FIR realtime mode is a safety-gated lab insert on outputs 1/2 with 129/257 taps: it first forces MUTE and is automatically bypassed upon audio device restart. Explicitly enable while muted before globally enabling audio, and use independent hardware mute/bypass. Never treat the hard clip guard as a certified loudspeaker limiter.

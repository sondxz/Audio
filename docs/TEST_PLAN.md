# Test Plan v0.3 (2026-10-22)

**Current evidence:** GCC Release 3/3 core offline; GCC ASan+UBSan 3/3; Clang Release 3/3; independent SciPy check 1693 bins PASS. See [RELEASE_VALIDATION_REPORT_v0.3.md](RELEASE_VALIDATION_REPORT_v0.3.md) and [VALIDATION_PROTOCOL_PRO.md](VALIDATION_PROTOCOL_PRO.md).

**No evidence yet:** Windows JUCE GUI/ASIO/M32 hardware, live plugin chain, SPL calibration, round-trip latency, stress/soak tests.

## Historical v0.1/v0.2 testing notes (archived)

## Đã chạy trong container Linux (GCC 14.2 + CMake 3.31)

- `cmake -S . -B build-core -G Ninja -DLAH_BUILD_GUI=OFF -DCMAKE_BUILD_TYPE=Release`: thành công.
- `cmake --build build-core`: thành công.
- `ctest --test-dir build-core --output-on-failure`: **1/1 test passed**.
- Test cases trong `tests/test_core.cpp`: passthrough 1:1, meter peak, safe mute, input/output khác nhau, null channel pointers, NaN/Inf, 32 channel, 1/16/64/128/256/512/1024/2048 frames.

## Chưa được kiểm tra

- Build GUI với JUCE 8.0.15 trên Windows 11 / MSVC.
- Driver ASIO, sample rate 44.1/48/96k, thiết bị M32R Live thật.
- Null buffer timing bất thường, xrun thật, runtime liên tục 2–8h, plugging/unplugging USB.
- Độ trễ vòng loopback audio, latencies do driver báo cáo so với đo thực tế.
- Số kênh input 32/output 32, routing M32 thực, hồ sơ RAM/CPU trên từng máy.

## Quy trình test tại máy Windows

1. Build và chạy `ctest`.
2. Mở app, pass-through **OFF**; không được có âm từ input tới output do app gửi; đồng thời Input meter vẫn phản ánh âm thanh thực.
3. Chọn driver/device/rate/buffer, đóng/mở lại app, kiểm tra settings được khôi phục.
4. Kết nối monitor mức **rất thấp**, xác nhận bật passthrough; đưa tín hiệu nhẹ vào một channel → kiểm tra output tương ứng bằng meter, tai nghe/monitor phù hợp.
5. Nhấn PANIC → output meter xuống -inf; kiểm tra audio callback vẫn chạy nhưng xuất zero.
6. Thay đổi driver/thiết bị khi pass-through đang chạy → trở về OFF; không phát âm tự động khi device restart.
7. Lặp theo sample rates và buffer driver thực sự cho phép; ghi CPU/XRuns và dropout.
8. Test 2h/8h và thao tác disconnect/reconnect có kiểm soát trước khi dùng tại hiện trường.

---

## v0.2 tests now available

- `tests/test_extended.cpp`: diagonal 2-channel mixer, matrix route on/off, output gain/polarity, mute and delay, clip ceiling, bell EQ response energy, input NaN/Inf, 192k allocation/preparation, stereo FX wet return, FFT transfer magnitude / polarity/phase / coherence / 120-sample delay estimate, missing reference RTA, point-source inverse-distance and coherent/inverted sources, End-Fire direction and 2D finite heatmap.
- Existing `test_core.cpp`: pure basic pass-through regression.
- `ctest` Linux core: 2/2 passed; sanitizer build ASan/UBSan: 2/2 passed (see `TEST_RESULTS_LINUX.txt`).
- **Not available**: GUI automation, JUCE plugin host build on Windows, real 32-in/32-out USB audio run, CPU 99.9th percentile, room measurements, plugin recovery, actual loopback delay, full real-world show verification.

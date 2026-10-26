# Ma trận kiểm thử và giới hạn bằng chứng

Cập nhật **26/10/2026**. Trạng thái hợp lệ: **PASSED / FAILED / NOT RUN / BLOCKED**. Mỗi kết quả chỉ áp dụng đúng đường xử lý đã thử, không xác nhận toàn bộ đặc tả hay chất lượng thương mại.

**Kết quả cuối:** Windows x64 Release build thành công, **41/41 CTest PASSED, 0 FAILED, 48,55 giây**. Bằng chứng: `artifacts/windows-v07/completion-final-build.log`, `completion-final-ctest.log`. Lượt đầu 40/41 phát hiện một XML hỏng chưa bị từ chối đúng trong `lah_session_state`; parser đã sửa và toàn bộ rerun đạt. Mốc 37/37 tại `video-alignment-final-ctest.log` là lịch sử trước bus/master, scene library và EQ mở rộng.

## Kiểm thử phần mềm đã thực hiện

| Phạm vi | Trạng thái | Bằng chứng / tiêu chí | Giới hạn |
|---|---|---|---|
| Build Release bản nâng cấp | PASSED | `completion-final-build.log`: EXE có mixer workflow, bus/master, scene library và EQ tám band | Build không thay thế kiểm thử |
| CTest nâng cấp lượt đầu | FAILED, lịch sử | `completion-first-ctest.log`: 40/41, XML hỏng; parser đã sửa | Lỗi này không còn trong rerun cuối |
| CTest tổng hợp cuối 41 bài | PASSED | `completion-final-ctest.log`: 41/41, 0 failed | Phạm vi synthetic/offline, không xác nhận hardware |
| GUI smoke / editor / workflow | PASSED | `build-windows-v07/ui-smoke/smoke-report.txt`: **173 PNG, 0 bounds warnings, 34 editor validations**; names/16CH/chain/peak handlers, bus/master, 8 quick scenes/preset transfer | Không mở thiết bị; dùng `ui-smoke/session-validation`, không ghi scene người dùng |
| GUI → callback audio | PASSED | `plugin-audio-audit/report.txt`: **14 checks**, gồm **8 ca DSP chính** VocalColor gain, legacy EQ, FX delay, EQ tám band ở input/output; CH8/slot3, 48 kHz/128. Đo −12/+6 dB và 480→1440 mẫu, isolation; legacy controls bị vô hiệu/chặn queued edit khi tám band hoạt động | Không phải cả 17 module qua GUI/callback; không mở ASIO/WASAPI thật |
| Cả 17 module đổi âm thanh | PASSED | `test_insert_sound_changes.cpp`; `plugin-audit-sound-changes.log` và `plugin-audit-sound-changes-asan.log`: sine/multitone/noise/impulse/bursts, tham số đổi live, RMS difference >0.0005, bypass residual =0, isolation | Buffer tổng hợp tại Rack, không phải micro/ASIO stream; không suy ra mọi tham số đều đổi tiếng |
| Tham số live / smoothing | PASSED | `test_live_plugin_parameters.cpp`: 17 modules, 8–192 kHz, min/max/default/NaN/Inf, không cấp phát, isolation; gain ramp 64 mẫu, reverb đổi layout không phát lại tail cũ; ASan ở `plugin-editor-live-parameters-asan.log` | Không phải nội suy delay-time hoàn chỉnh hoặc đánh giá nghe mọi chuyển tiếp |
| Tám bus mono / stereo Master | PASSED | `test_bus_master.cpp`: send/sum, gain/mute, constant-power pan, output map, ramp và meter; callback integration và profile USB không cộng Master ngầm | Chưa có dedicated bus/master plugin rack hoặc phần cứng |
| EQ tám band | PASSED | `test_extended_eq.cpp`: năm loại filter, đáp tuyến độc lập, legacy, live changes, isolation/state, không cấp phát; smoke kiểm tra mode/controls/điểm kéo | MODEL không phải FFT đo |
| Move/copy/paste/remove native slot | PASSED | `test_rack_operations.cpp`: mọi field tám EQ bands và legacy-off state, clipboard cross-domain, muted guard, bounds và unrelated feedback-arm isolation | Không bao gồm VST3 reorder/state |
| Session XML / preset mới | PASSED | `test_session_state.cpp`: round-trip tên/routing/bus/master/EQ/preset, malformed/corrupt file rejection sau sửa parser; UI library round-trip đạt | Không có VST3/FIR state hoặc full crash-recovery |
| Smart Comp macro | PASSED | `test_smart_comp_macro.cpp`: mapping −100/0/+100, bounds/continuity/inference, CUSTOM/scene, isolation, nén phụ thuộc mức; smoke kiểm tra callback | Chưa có GR telemetry/kiểm thử nghe |
| JUCE engine / USB returns | PASSED | `test_audio_engine.cpp`: lifecycle, geometry, mute/PANIC, FIR/hosted-processor boundary, 8 returns/output racks, tail/restart; `test_usb_return_profile.cpp` kiểm tra N=1…8 | Thiết bị và processor double; không chứng minh DN32-LIVE thật |
| Meter slot / bus / Master | PASSED theo các test trên | Pre/post native slot, mute/reset/epoch, bus/master publication, independent console peaks | Không phải VU chuẩn, loudness hoặc gain-reduction meter |
| Toán DSP đối chứng | PASSED, phạm vi hiện có | Bảy independent suites; NumPy/SciPy so sánh RBJ, dynamics, delay/comb/allpass/predelay, TF/FIR/feedback/model; `test_console_modules.cpp` cho Analog/4K | Không chứng minh thuật toán chưa triển khai hoặc hardware emulation |
| Feedback detector/model | PASSED, dữ liệu tổng hợp | Notch/ARM/PANIC/model parity trong feedback suites | Chưa xác minh false positives trên nhạc/giọng live ngoài thực tế |

## Kiểm thử còn thiếu theo đặc tả

| Hạng mục | Trạng thái | Phần việc còn lại |
|---|---|---|
| Stereo link / bus-master plugin racks | NOT RUN | Tám bus mono và stereo Master sum đã có; stereo link/rack riêng chưa triển khai |
| Full channel/master preset, VST3 state, crash recovery | NOT RUN | Đã có native plugin/4-slot chain/8 scenes; các phạm vi còn lại chưa có |
| EQ / vocal pre-post spectrum, GR/VU | NOT RUN | Bổ sung capture đúng target và telemetry đúng đại lượng; không thay bằng MODEL |
| Vocal Clean denoise/de-harsh/autogain | NOT RUN | Thuật toán, giọng/noise tham chiếu, chất lượng, latency và CPU |
| Smart EQ target / Analyze / Apply / Reject | NOT RUN | Chưa có hệ thống đề xuất theo FFT/target |
| FX modulation / digital stereo delay / stereo reverb | NOT RUN | Thuật toán riêng, timing/phase/stereo mapping và chuyển delay-time |
| PC AUDIO source capture / clock / SRC | NOT RUN | DSP PC Audio hiện không phải source capture utility |
| Multitrack recorder / tap points | NOT RUN | Chưa ghi audio raw trước insert, PC Audio/Output/Master taps hoặc quản lý storage/status; QA CSV không phải recorder |
| Sub crossover / phase / limiter bảo vệ loa | NOT RUN | Triển khai topology/phase/limiter và dùng đúng thông số hệ thống |
| VST3 editor / per-channel chain / state | NOT RUN | Chưa có workflow đầy đủ hoặc thử plugin bên thứ ba được chọn |
| Crash-isolated scanner/hosting | NOT RUN | Chưa tách process và thử plugin lỗi/hang |
| Realtime stress 8/16/32 kênh, chạy 24 giờ | NOT RUN | Đo workload cụ thể, CPU/RAM/callback time/XRun/reconnect; offline pass không phải headroom thực |
| LIVE/HQ / oversampling / aliasing | NOT RUN | Chưa có các chế độ và xác nhận chất lượng tương ứng |
| Assistant / radial menu | NOT RUN | Chưa triển khai các workflow này |

## Kiểm thử cần M32R/DN32-LIVE

Lúc **17:53:54 +07:00**, inventory chỉ có M-Audio; PnP có Unknown USB/Code43 chưa xác định thuộc mixer. Lúc **18:11:23** đã liệt kê **DN32-LIVE ASIO Driver**, nhưng `m32-muted-check-blocked.json` ghi không có 44,1/48 kHz, **device_opened=false, 0 IN/OUT, 0 callbacks**, luôn mute. Inventory **18:12:57** vẫn có đăng ký DN32 nhưng không có endpoint DN32; lọc `m32-usb-pnp-after-driver.txt` không thấy DN32/VID26A0 hoặc PortResetFailed. Code43 là quan sát lịch sử, không khẳng định còn hiện.

Người dùng xác nhận USB-B, cáp 10 m cộng nối dài; thử dây dữ liệu ngắn trực tiếp còn chờ, chưa kết luận nguyên nhân. Driver chính hãng 5.72.0 đã tải/xác minh chữ ký EXE/MSI **Valid**; **agent không chạy installer/EULA**. Tên driver nay đăng ký không xác nhận phiên bản hay thiết bị đang giao tiếp. [Báo cáo kết nối](M32_CONNECTION_CHECK_REPORT.md) mô tả script `scripts/check-m32-usb.ps1`: mặc định chỉ liệt kê, muted stream check chỉ chọn DN32, không fallback M-Audio.

| Hardware acceptance | Trạng thái | Điều kiện thực hiện |
|---|---|---|
| Mở stream DN32-LIVE ASIO | BLOCKED | Có tên driver nhưng chưa có thiết bị/kênh/rate hợp lệ |
| USB send → DSP → USB return thực | BLOCKED | Patch hardware và thử từng tuyến độc lập |
| 1/8/16/32 kênh vật lý | BLOCKED | Driver mở đủ kênh, nguồn/điểm đo phù hợp |
| 48 kHz / 64,128,256,512 mẫu | BLOCKED | Chỉ thử các buffer driver đích hỗ trợ |
| Round-trip latency, mục tiêu <10 ms | BLOCKED | Loopback vật lý; buffer duration không phải round-trip |
| XRun/dropout/CPU/RAM, chạy dài | BLOCKED | Workload cụ thể và log đủ thời lượng |
| Ngắt/nối USB / recovery / hardware bypass | BLOCKED | Thử thiết bị thật, mute/tail/restart và đường dự phòng |
| Feedback/giọng/nhạc acoustic | BLOCKED | Dữ liệu/phòng/hệ thống đo và đánh giá false positives |

Không có nhãn PASSED nào cho hardware audio. Cắm dây, đăng ký/tải driver hoặc đọc log build không xác nhận M32R đã truyền audio.

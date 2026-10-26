# Known limitations — LiveAudioHost 0.3.0

> Cập nhật 2026-10-22: các giới hạn lịch sử về Windows chưa build và PDC chưa triển khai đã được cập nhật trong [báo cáo Windows hiện tại](WINDOWS_DEVELOPMENT_REPORT_v0.7.0.md). Các giới hạn phần cứng/SPL/plugin isolation/room simulation vẫn còn. Không dùng các báo cáo cũ để suy ra trạng thái nghiệm thu hiện tại.

## Điều quan trọng nhất

- **Chưa có chứng cứ app đã build thành công ở Windows 11, chưa có executable đã chạy, chưa có test với Midas M32R Live, chưa có đo latency round-trip.** Automated tests Linux chỉ xác nhận lõi C++ độc lập JUCE.
- Không khẳng định phần mềm này sẵn sàng dùng trên hệ thống FOH/BOH/monitor show có khán giả. Nếu phần mềm treo/crash có thể khiến tiếng im lặng hoặc méo. Cần tuyến bypass/mixer hardware độc lập.
- Plugin scanner chạy worker thread nhưng **cùng process**: plugin lỗi có thể làm cả app crash. Plugin loader có thể làm đứng message thread; plugin DSP có thể cấp phát bộ nhớ hoặc gây dropout trong callback. Chưa hỗ trợ multi-plugin chain, plugin GUI, lưu preset VST state, plugin delay compensation.
- Chưa có dedicated low-latency round-trip loopback measurement; CPU/XRun lấy từ JUCE/driver không đồng nhất trên mọi thiết bị. Không thể đưa ra con số latency thực tế cho M32.
- 32 audio channel tối đa cho mixer và DSP. Nếu driver mở >32, các output sau channel 32 được mute và input >32 chỉ hiển thị meter (nếu <=64). Driver callback slots có thể khác kênh cổng vật lý.
- 1 HPF và 1 bell EQ/input, basic feed-forward compressor/gate (không soft knee, makeup, compressor GR meter, sidechain). Output ceiling là sample clip guard, **không là true-peak/look-ahead/loudspeaker limiter chuẩn**. Không có crossover LR/BW output, FIR, system alignment automatic.
- FX chỉ stereo outputs 1/2, không bảo đảm chất lượng reverb chuyên nghiệp, không có de-esser/pitch correction.
- Mixer chưa có full group/bus/aux/solo/pan/monitor mixing model, subwoofer bus thực; matrix cơ bản và 1 FX send.
- TF bản 0.3 dùng delayed-aligned H1/Hann + overlap, per-bin coherence gates và linear GCC-PHAT. Delay vẫn có thể sai với reverberation, feedback, noise, sample-clock drift hoặc reference routing lỗi. Chưa có calibrated measurement mic/SPL absolute. CSV lưu dữ liệu khi có mic mạnh; giá trị phase và magnitude chỉ hợp lệ nếu flag tf_valid=1.
- Sub simulation là free-field omnidirectional point source, **không có phản xạ/tường/sàn, hướng tính loa thật, dự đoán SPL hiệu chuẩn, absorption vật liệu, wave-field synthesis**. 3D là projected surface tương đối, không mô phỏng phòng 3D đầy đủ.
- Scenes là một file XML cục bộ; không có nhiều show, autosave/versioning hoặc undo. Scene không lưu VST3 plugin state, headphone mix hay measurement snapshots.
- Tuning recommendations / AI assistant, true acoustic simulation, signal generator, automated loopback latency test, offline/real hardware soak test, full diagnostics vẫn chưa có.
- Safety: mọi routing thay đổi khi đang phát có thể gây tiếng lớn. PANIC ngắt trong callback kế tiếp, không phải điều khiển hardware mixer/PA.

## Phần bổ sung 0.3

- **Bổ sung toán học offline không thay thế test Windows.** GUI measurement timer hiện chạy FFT/delay finder ở message thread: có thể giật giao diện khi tải máy cao; cần chuyển analysis sang worker ưu tiên thấp và tách độc lập audio callback trước show release.
- Delay finder mặc định tìm tối đa min(250ms, half captured frame length), có thể từ chối tonal/poor-SNR reference; không đồng nghĩa group delay hoặc absolute acoustic TOF.
- RTA là window-normalized RMS dBFS mỗi FFT bin, **không phải SPL được hiệu chuẩn** và **không phải dBFS full-band meter**. Chưa có A/C weighting, time weighting IEC, mic-cal file và acoustic uncertainty budget.
- TF bins bị ẩn nếu không có reference floor/coherence tin cậy; phase không được hiển thị nếu finder chưa chứng thực delay. Khi lọc IIR/phòng phức tạp peak delay có thể không biểu diễn group delay của toàn phổ.
- Plugin host và output protection vẫn ở mức thử nghiệm, chưa được phê duyệt cho live sound reinforcement.

## v0.4 additional limits
- Original screen captures depict another program; they are not digitized reference measurements.
- Engineering estimates cannot certify electrical/speaker protection or calibrated SPL.
- Front-fill diagram shows only nominal -6dB ray intersection; not a verified acoustic array design.
- ISO air loss uses equation for pure tones in validated range; no multipath/weather, and screenshot ISO numbers were not adopted.
- v0.4 JUCE panels and controls are source-only; no confirmed Windows GUI build.


## v0.5 additional limits
- SPL numerical display is from session microphone acoustic-calibrator offset, not IEC-certified meter; frequency A/C weighting is approximated by offline spectral integration, not IEC tolerance-certified weighting network or F/S/Leq. Calibration can silently become wrong if analog preamp gain changes outside app; only some digital configuration changes trigger invalidation.
- FIR designers generate theoretical LP/HP taps for offline export/analysis; NOT a real-time convolution engine and NOT validated for speaker protection. Processor-specific file compatibility cannot be assumed. 2049-tap group delay 21.33ms at 48k; high order real-time deployment requires FFT convolution/CPU tests, per-output latency/PDC, crossovers/headroom and limiter verification.
- Polar CSV requires consistent unwrapped phase and referenced measurement origin, assumes same profile for all simulated sources; no real elevational polar data / full room propagation / manufacturer GLL. Relative SPL only.
- A/B TF snapshots are session-only and no standalone graph compare error metric; current FFT remains on GUI timer.
- Callback deadline overrun count measures elapsed software processing vs buffer duration, NOT exact ASIO dropout or real round-trip latency.
- Dark rack styling remains source only and has NOT passed GUI screenshot/UI tests on Win11.

## v0.6 source supersedes specific v0.5 limitations — BUT NOT HARDWARE VERIFICATION
- FFT/transfer analysis now runs in a C++ background `MeasurementWorker` instead of the GUI timer. GUI thread still renders plots and performs non-realtime file I/O; all paths require Windows runtime testing.
- SPL now has non-overlapping analysis blocks and indicative power-domain Leq/approximate Fast-Slow block averaging. Still **not an IEC 61672 time-weighting implementation**, certified meter or a legally traceable measurement chain.
- 129/257-tap FIR stereo insert is now present in the audio callback (OUT1/2), bypassed at device start and gated by device detachment/mute. **Long FIR, measured response, PDC, callback worst-case headroom and true-peak limiting remain incomplete.**
- Mixer gains input mute/L-R balance only. Real FOH/BOH separate monitor/cue buses, extensive DSP rack and robust solo routing remain missing.
- Offline source tests can pass with ASIO/MSVC GUI completely untested. See `MODULE_ACCEPTANCE_MATRIX_v0.6.md` and `RELEASE_VALIDATION_REPORT_v0.6.md`. Neither this nor prior versions may be called "100% perfect" or "show safe".

# Đối chiếu tính năng với yêu cầu mở rộng

Cập nhật: **26/10/2026**. Đối chiếu mã nguồn hiện tại với tài liệu “MASTER DEVELOPMENT PROMPT” và các keyframe đã truy cập được của video [TITAN LIVE HOST 1.0.10](https://www.youtube.com/watch?v=UiGUPirC3KY), dài 36:14. Người dùng đã hoàn tất CAPTCHA. Việc đối chiếu dựa trên các khung hình và chú thích nhìn thấy, không phải đánh giá từng giây của toàn video; transcript vẫn chưa tải xong.

**IMPLEMENTED**: đã có chức năng trong phạm vi mô tả ở cột hiện tại. **PARTIAL**: có nền tảng nhưng thiếu phần của yêu cầu. **NOT IMPLEMENTED**: chưa có chức năng tương ứng. Các nhãn này không thay thế kết quả kiểm thử; xem [TEST_MATRIX.md](TEST_MATRIX.md). Chưa nghiệm thu phần cứng M32R/DN32-LIVE.

Bản nâng cấp hiện tại đã build Windows x64 Release và **41/41 CTest đạt trong 48,55 giây**: `completion-final-build.log`, `completion-final-ctest.log` trong `artifacts/windows-v07`. GUI smoke đạt **173 PNG, 0 bounds warnings, 34 editor validations**; GUI/audio audit **14 checks**, gồm tám ca DSP chính. Lượt đầu 40/41 phát hiện XML hỏng chưa bị từ chối đúng; parser đã sửa và rerun đạt. Mốc 37/37 trước đó được giữ làm lịch sử. Kết quả offline không nâng các chức năng PARTIAL thành đầy đủ đặc tả.

## Audio, mixer và giao diện

| Hạng mục yêu cầu | Trạng thái | Có trong mã nguồn hiện tại | Phần còn thiếu / giới hạn |
|---|---|---|---|
| Ứng dụng Windows x64, JUCE, CMake, MSVC | IMPLEMENTED | Ứng dụng desktop và quy trình build Release; JUCE 8.0.15 | Dự án dùng C++17, chưa chuyển C++20; chưa có installer thương mại |
| ASIO input/output | PARTIAL | Backend JUCE ASIO, chọn driver/kênh/rate/buffer; theo dõi trạng thái và latency driver | Chưa mở stream DN32-LIVE thực; chưa chứng minh USB send/return M32R |
| Đường USB insert 1–8 kênh | IMPLEMENTED | Profile IN n → OUT n, xác minh mask kênh, giữ audio mute khi áp dụng; giữ rack đã chọn và tắt bus/master software | Chỉ là cấu hình phần mềm; Card send/return trên mixer phải patch riêng |
| Engine/routing tối đa 32 IN × 32 OUT | IMPLEMENTED | Matrix cộng tuyến theo gain, DSP độc lập từng kênh; phụ thuộc số kênh driver mở được | Không đồng nghĩa đã kiểm thử 32 kênh vật lý hoặc đủ CPU realtime |
| Chuỗi plugin input/output | IMPLEMENTED | 4 slot native theo thứ tự ở mỗi input và mỗi output; output rack xử lý sau tổng tuyến, trước gain/delay/clip output | Chuỗi cố định; chưa có graph tổng quát, drag reorder, nhánh tùy ý hoặc VST3 trong từng slot |
| Channel strip theo ảnh mixer | PARTIAL | Tên 32 input chỉnh được/lưu scene, 4 insert, chain overview, mute/balance/gain, peak hold/clip latch/reset, chọn output | Chưa có stereo link, chain-enable chung hoặc hai meter IN/OUT riêng trên mỗi strip; output meter có ở editor/output panel |
| Fader / mute / pan | IMPLEMENTED theo phạm vi | Gain/mute thật; physical OUT1/2 giữ balance cũ; đường input/bus → stereo Master dùng constant-power pan | Pan Master và balance USB 1:1 là hai đường khác nhau; chưa có stereo link |
| Điều hướng kênh | IMPLEMENTED | 8CH: bốn bank; 16CH: hai bank, viewport ngang khi không đủ chiều rộng; label/routing giữ chỉ số 1–32 | Không tự mở thêm kênh driver; các màn hình chuyên biệt khác của đặc tả vẫn còn thiếu |
| Master stereo L/R | PARTIAL | Trang BUS/MASTER có sum L/R thật, hai fader/meter, mute và chọn physical output riêng; sau input DSP, trước output rack | Chưa có rack plugin riêng của bus/master; cột USB OUTPUT trong Mixer chỉnh một physical USB OUT |
| Tối thiểu 8 software buses | PARTIAL | Tám bus mono thật với send 32 input, gain/mute, pan/to-Master, direct output map và meter độc lập; mặc định tắt | Chưa có bus stereo link hoặc rack DSP/plugin riêng; software bus khác bus vật lý M32R |
| Profile Master / USB | IMPLEMENTED | Master-only xóa matrix/FX return chung và đưa IN1–8 vào Master OUT1/2; USB 1:1 khôi phục tuyến riêng và tắt bus/master; giữ audio mute | Bật bus song song matrix tùy chỉnh có thể cộng cùng nguồn; chưa có bộ phát hiện toàn bộ đường dry song song/vòng feedback |
| FX send/return | PARTIAL | FX send chung sang delay/reverb stereo; bus mono có thể map ra physical output có native rack; native delay/reverb có wet 100% | Chưa có dedicated FX bus rack, stereo effect chain tùy ý, chorus/modulation hoặc kiểm tra đường dry song song |
| Thêm / bỏ / đổi thứ tự plugin | IMPLEMENTED | Click mở editor; menu chuột phải move up/down/copy/paste/remove cho input/output; giữ toàn bộ snapshot gồm EQ 8-band | Structural edits cần mute; chỉ bốn slot native, không bao gồm VST3 graph |
| Editor nổi độc lập | PARTIAL | Nhiều cửa sổ không modal, giữ đúng target; núm/nhập số/reset/bypass, meter thật, Smart Comp macro, EQ 8-band riêng; native presets qua SCENES | Nhiều module còn dùng framework sáu núm/model, chưa đủ toàn bộ layout/chức năng trong ảnh |
| Scene và preset | PARTIAL | 8 Quick Scenes save/recall/rename/import/export; preset native một plugin hoặc chain 4 slot, copy/paste; XML schema 2 giữ tên/routing/bus/master và EQ 8-band; mở lại saved session khi có file, vẫn mute | Chưa có VST3/FIR state, full channel/master preset riêng hoặc crash-recovery/autosave liên tục; không gọi là phục hồi mọi trạng thái ứng dụng |
| Multitrack recording | NOT IMPLEMENTED | Có QA CSV ghi trạng thái, CPU/XRun và callback; đây không phải ghi âm | Chưa có recorder raw trước insert, PC Audio/Output tap, Master tap sau master processing/trước output bus, quản lý file/dung lượng/trạng thái ghi như video |
| VST3 ngoài | PARTIAL | Scan trên worker thread, danh sách, load/unload, bypass; **một stereo insert OUT 1/2**, reported latency và compensation có giới hạn | Chưa có editor VST3, generic editor, search/category, per-channel chain/reorder, state save/load, scanner process riêng hoặc crash isolation hoàn toàn |
| Analyzer / đo lường | PARTIAL | Capture input thật, RTA, H1 transfer function, magnitude/phase/coherence, delay finder, snapshot A/B, CSV; worker riêng | Chưa có FFT trước/sau từng plugin, cursor/trace manager đầy đủ, multi-measurement/session; chưa hiệu chuẩn đo acoustic hoặc loopback M32R |
| Latency và hiệu năng | PARTIAL | Buffer duration, driver latency, VST3/FIR latency, callback timing, CPU/XRun; smoothing 64 mẫu khi đổi tham số native | Chưa có LIVE/HQ mode/oversampling; chưa có PDC cho graph tổng quát hoặc số đo round-trip thực |
| Diagnostics / phục hồi | PARTIAL | Default mute, PANIC, reset tail, bảo vệ NaN/Inf, device lifecycle, safety latch, QA CSV ngoài callback | Chưa có trình phục hồi/reconnect đầy đủ, async logger riêng, tự kiểm tra mọi routing; chưa có hardware failover |
| Routing safety | PARTIAL | Profile 1:1, giới hạn số kênh, label input/output rõ; không điều khiển routing phần cứng | Chưa phát hiện vòng feedback, output trùng, parallel dry/wet hoặc invalid graph tổng quát |
| TITAN Assistant | NOT IMPLEMENTED | Không có trung tâm trợ lý trong ứng dụng | Chưa có rule engine, đề xuất có giải thích, Preview/Apply/Reject hoặc chat |
| Menu plugin vòng tròn | NOT IMPLEMENTED | Danh sách chọn module trong editor/rack | Chưa có radial menu 17 biểu tượng và trung tâm Assistant |
| Điều khiển fader/preamp mixer qua USB | NOT IMPLEMENTED | USB/ASIO chỉ truyền audio | Muốn điều khiển mixer cần giao thức control riêng và thử phần cứng |

## Đối chiếu từng module trong danh sách 17 mục

Mỗi module có sáu tham số cơ bản, enable/bypass và state độc lập; Smart Comp có macro, Equalizer có thêm state tám band. Native preset riêng và preset chain đã có qua SCENES. Việc có đủ 17 tên và xử lý âm thanh thật **không có nghĩa đã đủ thuật toán, tham số và workflow của bản đặc tả mới**. CPU/latency theo từng instance, GR/VU và FFT riêng vẫn còn thiếu.

| Module yêu cầu → tên hiện tại | Trạng thái so với đặc tả | Xử lý / hiển thị đã có | Khoảng trống cụ thể |
|---|---|---|---|
| ANALOG → Analog | PARTIAL | HPF, warmth/air EQ, tanh soft saturation, wet/output; filter/drive models, peak IN/OUT | Chưa có input trim/character độc lập, harmonic spectrum thật, VU chuẩn hoặc kiểm thử aliasing/oversampling đầy đủ |
| DRUM LIVE → Drum Live | PARTIAL | HPF, body/attack EQ, gate-envelope, transient boost; transient model | Chưa có sustain riêng, compressor/peak-control riêng, output trim, preset Kick/Snare/Tom/Drum Bus hoặc learn 10 hits/auto-match như video |
| PC AUDIO → PC Audio | NOT IMPLEMENTED theo vai trò utility | Module hiện tại là **HPF + bass/treble EQ + compressor/clip guard** | Chưa có Windows audio capture/WASAPI loopback, source selector, cross-clock buffering/SRC hoặc source mapping; DSP hiện tại không thay thế tiện ích nguồn PC |
| 4K LINE → 4K Line | PARTIAL | HPF, 4 bell cố định 100/400/3k/10k Hz, line saturation; response model | Chưa có input/output trim riêng, tần số/Q chỉnh từng dải hoặc oversampling |
| AI-FB → AI-FB LAB | PARTIAL | FFT/peak/persistence detector và neural model huấn luyện tổng hợp; candidate, model score, manual notch và tối đa 6 auto cuts sau ARM | Chưa có live-spectrum/history UI, sensitivity/đề xuất Manual Apply như đặc tả; chưa xác minh phân biệt feedback với nhạc/giọng ngoài thực tế |
| AUTO FB PRO → AUTO FB PRO | PARTIAL | Spectral/growth detector, manual notch, ARM/DISARM/CLEAR; tối đa 6 notch và tổng cut budget 36 dB; hiển thị cut thật | Chưa có spectrum, lock từng notch, sensitivity/max-count tùy chỉnh và quản lý fixed/live filters đầy đủ |
| SMART COMP → Smart Compressor | PARTIAL | RMS compressor, level-dependent release; **macro BOOST / BALANCED / COMP** áp dụng mapping threshold/ratio/attack/release/wet/makeup; núm lớn riêng, advanced/CUSTOM, curve và native presets | Chưa có GR meter thực; mapping là thiết kế native có kiểm thử, không khẳng định sao chép thuật toán trong video |
| OPTICAL COMP → Optical Compressor | PARTIAL | Hai envelope/time constants, soft saturation, compression, makeup; compressor/release models | Chưa có Peak Reduction/mode workflow riêng, VU/GR meter, mô hình optical được đối chiếu phần cứng |
| DYNAMICS → Dynamics | PARTIAL | Compressor peak + downward expander/gate; attack/release, makeup; transfer/gate models | Chưa có limiter độc lập, knee, gate range/hold/attack/release riêng và enable từng khối; clip guard output không thay thế limiter này |
| EQ → Equalizer | PARTIAL | Legacy sáu tham số giữ nguyên; tùy chọn **8 bands** có bell/low shelf/high shelf/HPF/LPF, enable/frequency/gain/Q từng band và kéo điểm trên response model; state/preset đầy đủ | Chưa có FFT/spectrum overlay đo thật; model không thay spectrum; chưa có mọi kiểu slope/topology ngoài năm loại hiện có |
| SMART EQ → Smart EQ | PARTIAL | Các mức giảm low/high theo năng lượng, presence EQ, adaptation; band-gain model | Chưa có FFT averaging/target curve, Analyze/Suggest/Apply/Reject hoặc giải thích đề xuất theo dữ liệu |
| FX → FX Delay | PARTIAL, khác phạm vi | Mono feedback delay, damping, saturation, wet/output; echo-repeat model | Chưa có multi-effects chorus/flanger/phaser/pitch/modulation, effect selector/chain/reorder |
| FX DIGITAL → Digital FX | PARTIAL, khác phạm vi | Bit-depth quantizer, sample hold, mono echo, feedback/wet/output; quantizer/echo models | Chưa có stereo/ping-pong L/R delay, BPM/tap/tempo sync, stereo width, low/high-cut workflow; không có combined echo/reverb riêng |
| REVERB 96 → Reverb 96 | PARTIAL | Reverb **mono** Schroeder 4 comb + 2 allpass, predelay, decay/damping/room/wet/output; RT60/room models | Chưa có chọn Plate/Hall/Room, early reflections/diffusion/width riêng, stereo algorithm hoặc convolution |
| SUB PRO → Sub Pro | PARTIAL | HPF, LPF bậc 4, bell 55 Hz, polarity/saturation/output; filter model | Chưa có loại/slope crossover chọn được, LR đúng đáp tuyến, PEQ đa dải, delay/limiter bên trong module hoặc phase graph; output delay chung là tính năng riêng |
| VOCAL COLOR → Vocal Color | PARTIAL | HPF, warmth/presence bells, saturation, wet/output; response/drive models | Chưa có body/brightness/air độc lập, preset Natural/Warm/Bright/MC/Karaoke |
| VOCAL CLEAN → Vocal Clean | PARTIAL | HPF, cut 250 Hz, de-ess high-band, peak compressor/makeup; filter/de-ess models | Chưa có denoising, gate/expander riêng trong module, de-harsh chỉnh frequency, Clean macro, autogain giới hạn, dry/wet hoặc input/output spectrum |

Tên “4K”, “96”, “Analog”, “Optical” không xác nhận sample rate M32R, độ trung thực mô phỏng phần cứng hoặc chất lượng thương mại. Các đường MODEL hiện là mô hình tham số; meter và trạng thái notch lấy từ DSP thật. Kết quả offline không thay thế kiểm thử nhạc/giọng, aliasing, chất lượng nghe và phần cứng.

## Các điểm tham chiếu đã quan sát trong video

| Thời điểm | Nội dung nhìn thấy | Đối chiếu với LiveAudioHost hiện tại |
|---|---|---|
| 04:03 | AUTO FB có đồ thị, điều khiển và trạng thái 2/5 notch | Có notch thật/cut list; chưa có live-spectrum UI tương ứng |
| 12:32 | Smart Comp với macro Boost / Balanced / Comp và meter | Đã bổ sung macro nhiều tham số; meter IN/OUT thật đã có; GR còn thiếu |
| 14:52 | Chain 4K Line → Reverb96 → Smart Comp → Master; tooltip 8 samples | Có ordered native slots và stereo Master sum; chưa có dedicated Master plugin rack. Tooltip trong video không phải số đo latency của ứng dụng này |
| 19:05 | Drum Live Kick, learn 10 hits và auto-match | Chưa có learn/auto-match |
| 26:16 | Vocal Clean macro 55%, FFT trước/sau và GR; low-cut 100 Hz/12 dB/oct, de-ess 7 kHz, de-harsh 3.5 kHz, comp/tone/autogain/mix | Có một phần DSP cơ bản; chưa có Clean macro, FFT/GR và nhiều khối điều khiển tương ứng |
| 30:10 | Panel chain chính, 8 Quick Scene, Master L/R | Đã có chain overview, 8 Quick Scenes và stereo Master riêng; không khẳng định tương đương toàn bộ workflow của video |
| 34:09 | Ghi multitrack raw trước insert, PC Audio/Output, Master sau master processing/trước output bus; lưu trữ/trạng thái | Chưa có recorder và tap graph này; QA logging hiện tại chỉ ghi CSV trạng thái |

Header video hiển thị **SSL ASIO Driver 1, 26CH, 48 kHz, 256 samples**. Đây không phải bằng chứng kết nối DN32-LIVE/M32R hoặc khả năng phần cứng của bản build hiện tại.

## Trạng thái kết nối ngày 26/10/2026

Người dùng xác nhận USB-B phía sau DN32-LIVE, cáp 10 m và dây nối dài. Lúc **17:53:54 +07:00**, inventory chưa có DN32; PnP có Unknown USB Device / Port Reset Failed / Code43, không xác định được là mixer. **18:11:23** đã liệt kê **DN32-LIVE ASIO Driver**, nhưng muted-check không có 44,1/48 kHz, không mở thiết bị: **0 kênh / 0 callbacks** (`m32-muted-check-blocked.json`). Inventory **18:12:57** vẫn có tên driver nhưng chưa có endpoint DN32; lọc PnP mới không thấy DN32/VID26A0 hoặc PortResetFailed. Code43 trước đó là lịch sử, không mô tả lỗi hiện tại. Thử dây dữ liệu ngắn trực tiếp vẫn còn chờ; chưa kết luận nguyên nhân.

Driver chính hãng **5.72.0** đã tải vào `artifacts/drivers`, EXE/MSI có chữ ký hợp lệ của MUSIC Tribe Brands DE GmbH. **Agent không chạy bộ cài hoặc chấp nhận EULA**; việc hệ thống nay có đăng ký DN32 là quan sát riêng, không suy ra phiên bản từ ZIP đã tải. Nguồn: [Downloads DN32-LIVE](https://www.klarkteknik.com/en/products/0606-ACY), [kiểm tra tĩnh](../artifacts/drivers/dn32-live-driver-verification.json), [báo cáo kết nối](M32_CONNECTION_CHECK_REPORT.md). Đăng ký ASIO chưa xác nhận thiết bị/card hay round-trip đã hoạt động.

Thao tác và phạm vi bản đóng gói mới nhất: [APP_UPGRADE_20261026.md](APP_UPGRADE_20261026.md). VIDEO_ANALYSIS là mốc phân tích tham chiếu trước bản nâng cấp này, không phải danh sách trạng thái hiện tại.

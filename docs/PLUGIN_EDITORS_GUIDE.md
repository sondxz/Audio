# Chỉnh plugin trong LiveAudioHost

Cập nhật **26/10/2026**. Hướng dẫn này mô tả các bộ xử lý đã có; đối chiếu với phần còn thiếu trong yêu cầu mở rộng tại [ma trận tính năng](FEATURE_MATRIX.md), [ma trận kiểm thử](TEST_MATRIX.md) và [kế hoạch phát triển](DEVELOPMENT_PLAN.md). Các khung hình video đã xem và phạm vi đối chiếu được ghi tại [VIDEO_ANALYSIS.md](VIDEO_ANALYSIS.md).

Cả 17 plugin tích hợp đều có cửa sổ chỉnh riêng. Bạn có thể mở nhiều cửa sổ và tiếp tục dùng mixer khi chúng đang mở. Mỗi cửa sổ giữ đúng **INPUT hoặc OUTPUT, số kênh và số SLOT** ghi ở đầu cửa sổ; mở plugin khác không chuyển cửa sổ đang chỉnh sang kênh khác.

1. Trong **Mixer**, bấm tên plugin ở dải CH để mở plugin input. Để chỉnh plugin ngõ ra vật lý, chọn **OUT** ở cột **USB OUTPUT**, rồi bấm tên plugin trong phần **PLUGIN CHÈN / OUT**. Stereo Master thật nằm ở trang **BUS/MASTER**, không phải cột USB OUTPUT.
2. Bấm ô trống **+** để mở cửa sổ và chọn bộ xử lý. Cần tắt audio trước khi nạp hoặc thay loại plugin. Plugin mới bắt đầu ở **BYPASS**; bấm nút này để chuyển sang **ON**.
3. Chế độ cơ bản có **6 tham số xử lý/phân tích**. Kéo núm, bấm ô số để nhập rồi nhấn Enter, hoặc nhấp đúp núm để trả riêng tham số về mặc định. Có thể chỉnh khi audio chạy. Equalizer có thêm chế độ tám band bên dưới. **Tone advisory tolerance** của AUTO FB PRO chỉ điều chỉnh gợi ý tone, không đổi tiếng hoặc bộ cắt tự động.
4. **ON / BYPASS** bật/bỏ qua đúng slot; **RESET** trả tham số về mặc định. Với EQ, RESET còn xóa state tám band và trở về legacy. Đóng cửa sổ vẫn giữ DSP; bấm lại tên để mở lại.
5. Nhấp chuột phải ô insert hoặc chain overview để **Move up / Move down / Copy settings / Paste settings / Remove plugin**. Move/paste/remove cần audio mute. Copy/paste giữ toàn bộ tham số cài đặt, kể cả EQ tám band, và chỉ thay đúng target. Không copy bộ nhớ đuôi hiệu ứng hoặc ARM/cut tự động đang chạy.
6. Trang **SCENES** có tám Quick Scene: **SAVE**, **RECALL**, nhấp đúp tên để đổi, chọn scene rồi Import/Export. Preset library cho **Single plugin** hoặc **Four-slot chain**: chọn INPUT/OUTPUT, CH, SLOT, đặt tên và Save/Load hoặc Copy/Paste. Recall/preset load giữ audio mute; import chỉ đưa file vào thư viện, chưa tự phát tiếng.

Riêng **SMART COMP**, núm lớn **SMART MACRO** đi từ **BOOST −100%**, qua **BALANCED 0%**, đến **COMP +100%**. Khi xoay núm này, ứng dụng cùng thay đổi sáu tham số thật: threshold, ratio, attack, release, wet mix và makeup. Sáu núm nâng cao, bao gồm **MAKEUP**, vẫn chỉnh riêng được ở hàng dưới. Nhấp đúp macro trả về Balanced; thao tác này áp dụng lại cả sáu giá trị mặc định của Smart Comp.

Khi sáu tham số không còn khớp một vị trí macro, cửa sổ ghi **CUSTOM**. Mở lại cửa sổ hoặc đọc scene giữ nguyên các giá trị chỉnh riêng; vị trí macro chỉ được suy ra để hiển thị và không tự ghi đè scene. Scene tiếp tục lưu sáu tham số hiện có. Mapping Boost/Balanced/Comp là thiết kế của LiveAudioHost, không phải bằng chứng thuật toán hay chất âm giống plugin trong video.

Với **Equalizer**, bấm **8 BAND EQ** để dùng tám band độc lập. Chọn band 1–8, bật/tắt **BAND ON/OFF**, chọn Bell/Low Shelf/High Shelf/HPF/LPF, rồi chỉnh Frequency/Gain/Q. Có thể kéo điểm theo tần số/gain; HPF/LPF kéo ngang, lăn chuột đổi Q và nhấp đúp điểm bật/tắt band. Trong chế độ này các núm legacy bị vô hiệu hóa. Bấm **LEGACY EQ** để trở về sáu tham số cũ mà vẫn giữ các chỉnh tám band cho lần bật lại; **RESET** mới xóa state mở rộng. Scene/preset/copy/move lưu đầy đủ cả hai chế độ. Đường đáp ứng là **MODEL**, chưa có spectrum FFT overlay.

Trên Mixer, nhấp đúp tên để đổi tên input, dùng **8 CH / 16 CH** và bank để xem đủ 32 kênh; 16CH có thanh cuộn ngang khi cửa sổ hẹp. **PK / CLIP** giữ peak thật từ lần reset; bấm nó để xóa latch của kênh, hoặc **RESET PEAKS** để xóa tất cả. Số kênh nhìn thấy không tự mở thêm kênh driver.

Trang **BUS/MASTER** có tám bus mono với send từ input đang chọn, gain/mute, pan/to-Master và direct output; Stereo Master có L/R gain/meter/output map. Pan ở giữa theo constant-power, không phải unity ở cả hai bên. **IN 1–8 > MASTER** xóa matrix/FX chung để đưa tám input qua Master OUT1/2; **USB 1:1** tắt bus/master và khôi phục return riêng. Các thao tác đổi routing giữ mute. Chưa có plugin rack riêng của bus/master; plugin ở USB OUTPUT xử lý tổng thực sự đi vào ngõ ra vật lý đó.

Các plugin và phần hiển thị tương ứng:

| Plugin | Xử lý âm thanh | Hình và thông tin trong cửa sổ |
|---|---|---|
| **Vocal Clean** | Lọc trầm, giảm đục 250 Hz, giảm âm xì và nén giọng | Đường đáp ứng bộ lọc và mô hình mức giảm âm xì |
| **Vocal Color** | EQ độ ấm/độ rõ, tạo màu bằng soft saturation | Đường bộ lọc và đường cong soft drive |
| **AI-FB LAB** | Notch thủ công, lọc trầm, giới hạn mức và phát hiện feedback bằng mô hình huấn luyện từ dữ liệu tổng hợp | Đường bộ lọc, các cut tự động hiện có, tần số nghi ngờ và điểm model |
| **AUTO FB PRO** | Notch thủ công, phát hiện phổ/tăng mức và cắt feedback khi ARM | Đường bộ lọc, tần số nghi ngờ và danh sách cut đang áp dụng |
| **Equalizer** | Legacy sáu tham số hoặc tám band Bell/Shelf/HPF/LPF độc lập | Response MODEL có điểm kéo, band enable/type/frequency/gain/Q; chưa có FFT overlay |
| **Smart EQ** | Điều chỉnh dải thấp/cao theo năng lượng tương đối, EQ presence | Đường bộ lọc và mô hình mức giảm hai dải |
| **Dynamics** | Compressor và gate/expander, chỉnh attack/release | Đường cong nén và mô hình gate/expander |
| **Smart Compressor / SMART COMP** | Nén theo RMS, release thay đổi theo mức, trộn dry/wet; macro điều chỉnh đồng thời sáu tham số | Đường cong nén, núm lớn Boost / Balanced / Comp, trạng thái CUSTOM và sáu núm nâng cao riêng |
| **Optical Compressor** | Nén với hai tốc độ hồi phục, có soft drive | Đường cong nén và mô hình hai đường release |
| **FX Delay** | Echo với feedback, damping, soft drive và dry/wet | Các lần lặp echo theo thời gian và đường saturation |
| **Digital FX** | Giảm bit, giữ mẫu và echo ngắn | Mô hình các lần lặp, bậc lượng tử hóa và số mẫu giữ |
| **Reverb 96** | Reverb thuật toán mono, chỉnh decay, damping, predelay, room và dry/wet | Đường bao RT60, hình room scale và giá trị predelay |
| **Sub Pro** | Lọc trầm, low-pass 24 dB/oct, EQ 55 Hz, đảo cực và soft drive | Đường bộ lọc, tần số low-pass và trạng thái polarity |
| **Drum Live** | EQ body/attack, gate và tăng độ nổi đầu tiếng | Đường bộ lọc, mô hình transient và thông số gate |
| **PC Audio** | EQ bass/treble, compressor và clip guard | Đường bộ lọc và mô hình compressor/clip |
| **Analog** | EQ warmth/air, lọc trầm và soft saturation | Đường bộ lọc và đường soft drive |
| **4K Line** | Lọc trầm, bốn dải EQ 100 / 400 / 3k / 10k Hz và saturation | Đường bộ lọc và cột gain của bốn dải |

Hai cột **SIGNAL IN / OUT** lấy peak âm thanh thật ngay trước và sau **slot đang mở**. Chúng độc lập với meter tổng của mixer; không có tín hiệu thì không tạo mức giả. Các đồ thị có chữ **MODEL** mô tả tác động của tham số, không phải phổ FFT đo trực tiếp; riêng tần số và danh sách cut feedback lấy từ trạng thái detector đang chạy.

Với AI-FB LAB và AUTO FB PRO, **ARM AUTO CUT** cho phép tạo cut tự động; **DISARM CUTS** dừng tạo cut mới và giữ cut hiện có; **CLEAR CUTS** xóa các cut tự động. Nhạc có thể kích hoạt nhầm detector; điểm model không phải xác suất feedback đã hiệu chuẩn.

Các bộ xử lý này là DSP tích hợp của ứng dụng. Analog, 4K Line và Optical Compressor dùng mô hình giải tích, không cam kết mô phỏng phần cứng cụ thể; Smart EQ và Smart Compressor không dùng AI. Chức năng **VST3** nằm ở màn hình riêng, hiện ở mức thử nghiệm cho stereo **OUT 1/2**; không thuộc danh sách 17 cửa sổ plugin tích hợp này.

Build Release ngày 26/10/2026 đạt **41/41 CTest**; GUI smoke **173 PNG / 0 bounds warnings / 34 editor validations**. GUI → callback audio đạt **14 checks**, gồm tám ca DSP chính gain/EQ/delay/eight-band EQ ở input/output. Cả 17 module còn có signal-change tests riêng tại Rack; không suy ra mọi tham số đều đổi tiếng. Các test này dùng buffer/thiết bị tổng hợp. Hiện Windows đã liệt kê DN32-LIVE ASIO nhưng chưa mở được stream/kênh/rate; xem [báo cáo triển khai](M32R_USB_IMPLEMENTATION_REPORT.md) và [kiểm tra kết nối](M32_CONNECTION_CHECK_REPORT.md). Agent không chạy bộ cài hoặc EULA.

[Hướng dẫn bản nâng cấp và EXE đóng gói](APP_UPGRADE_20261026.md) tập hợp cách dùng các trang mới. VIDEO_ANALYSIS mô tả mốc đối chiếu video trước nâng cấp, không thay thế hướng dẫn hiện tại.

# AVOPSEMU PS1 v51 — CPU, GPU và âm thanh

## CPU và thanh ghi COP0

TAR (COP0 6) chỉ đọc qua MTC0; ngoại lệ trong delay slot của nhánh được lấy cập nhật TAR bằng đích nhánh. CAUSE phản ánh BD/BT riêng biệt. BPC/BPCM so khớp PC theo mặt nạ, DCIC kiểm tra DE/PCE và quyền kernel/user. TR bật sẽ vào vector breakpoint debug 0x80000040 hoặc 0xBFC00140, không dùng vector ngoại lệ thường. TR tắt chỉ ghi trạng thái. DCIC giữ các bit được phép ghi (0xFF80F03F).

Đây là hoàn thiện hành vi thanh ghi có thật của PS1, không thêm GPR ngoài kiến trúc. Data breakpoint BDA/BDAM, trace và cơ chế chuyển hướng jump của debug chưa được mô phỏng đầy đủ. BDA/BDAM vẫn giữ latch hiện có.

Trong tick thiết bị, giữ lần lấy mẫu ngắt đầu tiên và chỉ lấy mẫu lần hai khi CD-ROM, DMA hoặc SPU vừa chạy có thể thay đổi đường IRQ. Không thay nhịp CPU, timer, DMA hoặc CD-ROM.

Nguồn đối chiếu: https://psx-spx.consoledev.net/cpuspecifications/ (TAR, CAUSE, DCIC, BPC/BPCM và breakpoint vectors). Không sao chép mã lõi của giả lập khác.

## GPU

Với hình chữ nhật có texture diện tích ít nhất 128 pixel, tính trước địa chỉ cột và độ dịch lấy index. Texture window, texture page, độ sâu màu và CLUT được tách khỏi phép tính lặp từng pixel. Dữ liệu VRAM và màu bảng CLUT vẫn được đọc tại từng pixel, giữ hành vi tự ghi đè/feedback. Có đường ghi nhanh cho texture raw không bán trong suốt; vẫn kiểm tra mask.

Không thay thuật toán raster tam giác/GTE hoặc tuyên bố sửa toàn bộ lỗi đồ họa. 240 trường hợp seeded so sánh toàn bộ VRAM với vòng vẽ cũ, bao gồm wrap, flip, window, palette và mask.

## Âm thanh

WSOLA dùng bốn chuỗi tổng năng lượng tích lũy cho bước lấy mẫu 4. Tích vô hướng, thứ tự chọn ứng viên và crossfade không đổi; tránh tính lại năng lượng trên nhiều cửa sổ trùng nhau. Không thay SPU ADPCM/ADSR/reverb, sample rate, pitch, ngưỡng queue hoặc thời gian ngắt.

Mục tiêu là giảm tải xử lý âm thanh; nếu lõi không tạo đủ mẫu hoặc thiết bị Windows bị underrun thì vẫn có thể rè. Đối chiếu baseline/current tại bảy tỷ lệ tempo cho PCM, số mẫu còn lại và vị trí WSOLA giống nhau; kết quả trong VALIDATION_V51.txt. Không có kiểm chứng nghe trực tiếp trên Windows trong môi trường này.

## Build và kiểm chứng trên máy của bạn

Mở riêng AVOPSEMU_PS1 trong Visual Studio 2022, preset windows-release, startup AVOPSEMU.exe. Build lại; không dùng EXE hoặc cache CMake cũ. Có thể giữ avopsemu.ini cạnh EXE. Nạp cùng BIOS/đĩa, cùng save và cảnh chơi để so sánh. Nếu vẫn rè, lưu log và ghi tốc độ/FPS tại cùng thời điểm.

Kiểm thử ở đây dùng GCC C++17 trên Linux, không có BIOS/game thương mại hoặc MSVC/Windows SDK. Benchmark là tải tổng hợp, không tương đương mức tăng FPS GTA/GT2. PS2 giữ nguyên v50.

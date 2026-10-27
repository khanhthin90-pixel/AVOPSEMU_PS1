# AVOPSEMU PS1 v51

Dự án giả lập PlayStation viết bằng C++17, giao diện Win32 cho Visual Studio 2022. Tên trước đây: PSLab. Nhánh PS2 đã chuyển thành dự án AVOPSEMU_PS2 độc lập; không biên dịch hoặc liên kết PS2 trong dự án này.

## Cập nhật lõi v51

- CPU: hoàn thiện COP0 TAR, CAUSE.BT, mặt nạ DCIC và breakpoint BPC/BPCM với vector debug riêng; giảm lấy mẫu ngắt trùng khi thiết bị không thay đổi.
- GPU: giảm tính toán địa chỉ texture/CLUT lặp trong sprite lớn, vẫn đọc VRAM trực tiếp để giữ đúng thứ tự khi texture và framebuffer chồng nhau.
- Âm thanh: tối ưu tìm điểm ghép WSOLA bằng tổng năng lượng tích lũy; giữ nguyên mẫu PCM và nhịp SPU.
- Chi tiết phạm vi, đo tốc độ và giới hạn: `PS1_PERFORMANCE_CPU_UPDATE_V51.md`, `BENCHMARK_V51.txt`, `VALIDATION_V51.txt`.
- Nhánh PS2 v50 không thay đổi. Chưa xác nhận FPS game thật hoặc chất lượng thiết bị âm thanh Windows của bản này.

## Tách dự án v46

Mở riêng AVOPSEMU_PS1 để build PS1. Mã PS2, kiểm thử, ELF demo và tài liệu PS2 được chuyển sang AVOPSEMU_PS2; lõi PS2 được giữ nguyên. Không có CMake cha bắt buộc và không cần đặt hai thư mục cạnh nhau.

## Giao diện PS1 đã có từ v45

- Nhận diện AVOPSEMU, màn hình chào mới bằng GDI, tông tím than.
- Thanh trên: Nạp BIOS, Mở đĩa, Boot đĩa, Chạy, Tạm dừng, Bàn phím.
- Thanh bên: tên BIOS/đĩa, preset phím, tốc độ, âm thanh, chế độ rạp, lưu log + ảnh.
- Menu chính tiếng Việt; debugger bật/tắt riêng; vùng game giữ tỉ lệ 4:3.
- F5 chạy/tạm dừng, F9 debugger, F11 chế độ rạp. Phím đã gán cho tay cầm trong cấu hình riêng được ưu tiên.
- Tệp chạy GUI: **AVOPSEMU.exe**. Target CMake nội bộ vẫn là `pslab` để giữ tương thích cấu hình build.
- Cấu hình mới `avopsemu.ini` cạnh EXE. Tự sao chép `pslab.ini` nếu tệp mới chưa có; không ghi đè cấu hình mới. Nếu sao chép thất bại, đọc cấu hình cũ.

## Build và chạy

1. Cài Visual Studio 2022 với **Desktop development with C++**, **C++ CMake tools for Windows**.
2. Giải nén, mở thư mục `AVOPSEMU_PS1` chứa `CMakeLists.txt` bằng **File → Open → Folder**.
3. Chọn preset **windows-release**, Startup Item **AVOPSEMU.exe** (target `pslab`), Ctrl+F5.
4. Nạp BIOS PS1 512 KiB → Mở đĩa `.cue` / `.bin` / `.iso` → Boot đĩa → Chạy.
5. Với BIN/CUE nhiều track, chọn `.cue` và giữ tất cả BIN trong cùng thư mục theo đường dẫn trong CUE.

Không kèm BIOS hoặc game. Người dùng cung cấp tệp của mình. Homebrew EXE trực tiếp không thay thế quá trình BIOS khởi tạo cho game thương mại.

## Điều khiển mặc định

| Phím | Nút PS1 |
|---|---|
| Mũi tên | D-pad |
| Z / X / A / S | Cross / Circle / Square / Triangle |
| Enter / Shift | Start / Select |
| Q / W | L1 / R1 |
| E / R | L2 / R2 |

Bấm vào cửa sổ game trước khi chơi. Menu **Hiển thị** có preset WASD và thử giữ Cross/Start 1 giây. Alt-Tab nhả phím. Đây là digital pad, chưa có analog/rung.

## Trạng thái và kiểm tra

- V51 cập nhật lõi CPU/GPU và WSOLA trên nền v46. Các kết quả kiểm tra mới nằm trong VALIDATION_V51.txt; các số kiểm thử bên dưới thuộc lịch sử v45/v46.
- Trước v45, người dùng đã xác nhận GT2 tới replay 3D và menu My Home, có điều khiển. V44 sửa SIO cho GTA2 nhưng chưa có xác nhận game thật sau bản vá.
- **79 bộ kiểm thử Linux đạt** gồm bộ mới kiểm tra vùng màn hình/debugger/thanh trạng thái tại nhiều kích thước.
- Môi trường hiện tại không có Visual Studio/Windows SDK: chưa build hoặc kiểm tra hiển thị UI thực tế trên Windows. Không có BIOS/game thương mại để kiểm tra v45 trực tiếp.
- Chỉ số Hz trên giao diện là VBlank mô hình, không phải số khung hình game riêng biệt.

Xem `OPEN_IN_VISUAL_STUDIO.md`, `AVOPSEMU_UI_V45.md`, `VALIDATION_V45.txt`. Các tài liệu v44 trở về trước giữ tên PSLab như lịch sử dự án.

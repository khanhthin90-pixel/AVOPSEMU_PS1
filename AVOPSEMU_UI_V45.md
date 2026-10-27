# AVOPSEMU v45 — PS1 UI

Giao diện chính được tổ chức lại: toolbar sáu thao tác, sidebar phiên chơi, vùng game trung tâm, hướng dẫn bàn phím và status riêng. Debugger xuất hiện bên phải khi bật. Chế độ rạp ẩn sidebar nhưng giữ toolbar/menu để thoát. Đồ họa màn hình chào vẽ bằng mã GDI, không mô phỏng kết quả BIOS và không cần ảnh nền để hoạt động.

Menu Hiển thị có màn hình chào (tạm dừng, giữ trạng thái máy), trợ giúp, hai preset, kiểm tra Cross/Start. F5/F9/F11 xử lý khi phím chưa được gán cho digital pad. Điều khiển số và SIO FIFO v44 giữ nguyên.

Shell vẽ qua bitmap đệm để giảm chớp nền. Gợi ý phím cập nhật mỗi 250ms thay vì 16ms; các bộ đếm SIO nằm trong debugger/log. Màn hình game vẫn lấy snapshot VRAM dưới mutex rồi chuyển RGB bên ngoài mutex như trước.

Đổi project CMake thành AVOPSEMU, OUTPUT_NAME của GUI thành AVOPSEMU; giữ tên target pslab và CLI cũ để không phá script build. Cửa sổ PS2 đổi nhãn AVOPSEMU, chưa thiết kế lại nhánh PS2. Các tài liệu lịch sử không đổi tên hàng loạt.

Cấu hình phím: ưu tiên avopsemu.ini; nếu chưa có thì CopyFile từ pslab.ini với fail-if-exists. Nếu không sao chép được, tiếp tục đọc tệp cũ. Không xóa/ghi đè cấu hình có sẵn.

Kiểm tra tự động: 79 suite Linux, gồm 60 tổ hợp layout (5 chiều rộng × 3 chiều cao × rạp bật/tắt × debugger bật/tắt). Chưa kiểm tra UI/âm thanh/biên dịch native trên Windows; cần người dùng build Release x64 và xác nhận hiển thị, chuyển chế độ, nạp BIOS/đĩa, input sau Alt-Tab. Không có lời hứa tăng FPS hoặc sửa thêm GTA2 ở bản UI này.

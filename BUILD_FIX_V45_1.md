# AVOPSEMU v45 — hotfix build 1

Sửa C2672 tại winmain.cpp dòng 56: RECT::right có kiểu LONG, khác kiểu int của đối số 164 trong std::min. Chuyển tọa độ sang int trước phép tính chiều rộng toolbar. C2737 của button_width là lỗi kéo theo.

Đã biên dịch đoạn tái hiện với kiểu long/int và chạy kiểm thử layout trên GCC. Không có MSVC/Windows SDK để xác nhận build native toàn bộ; 79 suite của v45 là kết quả trước hotfix. Không đổi lõi giả lập.

Giải nén bản sửa, mở đúng thư mục mới, Build → Rebuild All, chạy AVOPSEMU.exe ở Release x64.

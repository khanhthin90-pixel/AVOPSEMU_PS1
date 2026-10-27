# v39 — Gran Turismo 2: unused timer-tail MMIO

Log người dùng dừng `Unsupported bus read @ 0x1f801130` sau 348829012 lệnh.
Lệnh cuối tại `80010910` là `94C20000` (LHU v0,0(a2)), a2=1F801130.
BIOS đã nạp `SCUS_944.88` vào RAM và gọi entry `8005D570`;
đây là lỗi khi mã game khởi tạo, không phải chưa tìm thấy đĩa.
Chưa có bằng chứng thay đổi PS2 đã gây ra lỗi này; decoder PS1 trước vá
không hỗ trợ địa chỉ đó.

Theo psx-spx, `1F80112A..1F80113F` nằm trong các địa chỉ I/O trống
không khóa: đọc không gây exception và ghi không có tác dụng.
Bản vá giới hạn ở phần đuôi `1F801130..1F80113F` đang gây lỗi.
Đọc byte/halfword/word trả zero ổn định, ghi bị bỏ qua, hỗ trợ các alias
KUSEG/KSEG0/KSEG1 hiện có. Zero là chính sách tương thích; chưa mô phỏng
giá trị bus không ổn định trên phần cứng. Không thêm Timer3.
Không chuyển mọi địa chỉ chưa biết thành zero; `1F801140` vẫn báo lỗi.
Không thay đổi timer0/1/2, CPU, CD-ROM, GPU hoặc bộ xử lý PS2 để vá GT2.

Test phát lại đúng 21 lệnh cuối trong log từ `800108C0` đến `80010910`,
với timer0/1 được seed từ trace, rồi dùng NOP tự viết để kiểm tra load delay.
LHU hoàn tất đến `80010914`; NOP commit v0=0 và tiến tới `80010918`.
Đây không phải phát lại toàn bộ 348 triệu lệnh, cũng chưa xác nhận GT2 vào
menu/đua xe. Không có BIOS/game Sony trong môi trường kiểm thử.

Build lại Release x64 trong Visual Studio 2022. Chạy đúng pslab.exe vừa build,
chọn BIOS PS1 và file CUE của GT2, reset/boot đĩa rồi Run. Save log mới phải
có dòng `PSLab v39 PS1 diagnostic (GT2 unused timer-tail MMIO fix)`.
Nếu gặp lỗi sau đó, log mới sẽ cho biết vị trí tiếp theo cần xử lý.

Nguồn: https://psx-spx.consoledev.net/unpredictablethings/
Mục Garbage Locations in I/O Area; không sao chép mã emulator ngoài.

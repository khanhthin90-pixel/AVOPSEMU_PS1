# v42 — PS1 audio continuity, GPU fixes and rendering performance

## Log đã nhận

Log PS1 v40 có 753563511 instructions, PC=8007C46C; không ghi bus/opcode
exception ở đầu. BIOS đã nạp SCUS_944.88 (GT2), nên đây không chỉ là log
startup BIOS. Trace đang lặp LBU từ RAM tại 801F0066, giá trị 2. Không sửa
RAM/ép bỏ vòng chờ vì chưa chứng minh ai phải cập nhật cờ này.

Các số đo được ghi trong log: MIPS=16.7976, VideoHz=28.5714,
audio_tempo=0.537248, underruns=44, host_drops=19857, SPU dropped_frames=1843.
VideoHz là số field/frame của mô hình trong thời gian thực, không nhất thiết
là FPS hình mới của game. GPU có 2170 rectangles; nhiều VSync timeout cũng
được in trong TTY. Chưa có ảnh/video/audio để xác nhận chính xác mọi lỗi hình
hoặc nguồn rè. `audio_mmcss=0` sau export/stop không chứng minh đăng ký MMCSS
khi đang phát đã thất bại: worker trả trạng thái này về false khi thoát.

## Âm thanh

- Nối từ mẫu âm thanh vừa phát sang PCM mới trong 128 frame khi phải cắt
  hàng đợi. Trước đây restart kéo gain đột ngột về gần zero, có thể tạo click.
- Khi thiết bị đã cạn hàng đợi, nối lại từ silence; không đưa lại mẫu cũ gây
  bước nhảy. Fade khi mute và che underrun về zero vẫn hoạt động.
- Hàng đợi SPU tăng trần từ 4410 lên 8192 stereo frames. Hàng đợi host chỉ
  cắt khi vượt 8192 frames, về 6144, thay vì 4410 về 3072. Đây là giới hạn
  chống burst/đứt quãng, không thêm độ trễ cố định vào mỗi lần phát.
  Đánh đổi: backlog có thể chứa nhiều âm thanh chờ hơn sau một lần host nghẽn.
- Tối ưu tìm đoạn khớp WSOLA: đọc deque một lần sang cửa sổ liên tục, tính
  năng lượng tail một lần, tích lũy tích/năng lượng bằng số nguyên 64-bit.
  Vẫn dùng căn chuẩn hóa double và cùng tiêu chí chọn điểm nối. Có thêm phần
  dư cho trường hợp làm tròn vị trí tìm kiếm; ASan kiểm tra ca biên này.
- Không đổi pitch SPU, ADSR, clocks 768 hoặc tốc độ CPU để che rè. Underrun
  vẫn có thể xảy ra khi máy không cấp dữ liệu đủ; chưa nghe thử WinMM thật.

## GPU và hiệu năng

- GP1(00) giữ tùy chọn GP1(09) và thiết lập các khoảng hiển thị mặc định theo
  tài liệu. Cold reset của core vẫn xóa tùy chọn; không thêm 2 MiB VRAM.
- Phân biệt khoảng hiển thị chưa được cấu hình với khoảng được ghi bằng 0.
  Zero horizontal range tạo HBlank liên tục, không sinh cạnh lặp; zero
  vertical range tạo VBlank liên tục. Presentation không hiển thị vùng giả.
- Vẽ rectangle không texture, không semi-transparency và không check-mask
  dùng fill theo hàng, tính màu một lần; vẫn giữ clipping/offset/force-mask.
  Các trường hợp texture, blend hoặc check-mask giữ đường vẽ cũ.
- Không sửa timer/CPU IRQ hoặc toàn bộ renderer dựa trên dòng VSync timeout.
  Không khẳng định mọi lỗi startup/GT2 đã hết. Cần log/ảnh mới sau chạy v42.

## Số đo

BENCHMARK_V42.txt có ba lượt xen kẽ trước/sau và tools/av_benchmark.cpp để
lặp lại. Median: workload rectangle đặc 1.63754 -> 0.157184 s (~10.4x);
workload WSOLA 0.54x 0.133508 -> 0.0860335 s (~1.55x). Hash PCM không đổi.
Đây là hai kernel tổng hợp, không phải toàn bộ emulator nhanh lên 10x hay
FPS BIOS/game thực tế. Chưa đo Windows/MSVC hoặc chất lượng âm thanh thiết bị.

## Cách thử

Build Release x64 từ mã nguồn mới, chạy pslab.exe v42. Giữ Speed:1x và
Audio:On, load BIOS/boot đĩa rồi Play. Nghe lại cùng đoạn startup và export
log sau 20–30 giây. So sánh underruns, host_drops, SPU dropped_frames, MIPS,
VideoHz giữa hai lần test cùng đoạn; log mới phải ghi PSLab v42 PS1.
Bản vá GT2, bàn phím v40 và PS2 v41 được giữ lại.

## Nguồn

https://psx-spx.consoledev.net/graphicsprocessingunitgpu/
GP1(00) reset defaults/GP1(09) persistence; GPU timing and zero-width HBlank.

https://learn.microsoft.com/en-us/windows/win32/multimedia/stopping-pausing-and-restarting-playback
https://learn.microsoft.com/en-us/windows/win32/multimedia/audio-data-blocks
WinMM playback/buffer lifecycle; không đổi API audio trong bản này.

Mã tối ưu/ghép âm thanh tự viết trong dự án, không chép mã emulator khác.

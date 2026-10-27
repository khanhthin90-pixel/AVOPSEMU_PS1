# AVOPSEMU PS1 — Visual Studio 2022

Cài workload **Desktop development with C++** và **C++ CMake tools for Windows**.

Mở thư mục có CMakeLists.txt → preset **windows-release** → Startup Item **AVOPSEMU.exe** (target `pslab`) → Ctrl+F5. Dùng Release x64 để chơi thử; Debug chậm hơn nhiều.

Hoặc mở Developer PowerShell tại thư mục dự án:

```powershell
cmake -S . -B build-vs -G "Visual Studio 17 2022" -A x64
cmake --build build-vs --config Release --target pslab
.\build-vs\Release\AVOPSEMU.exe
```

Solution tạo mới là `build-vs/AVOPSEMU_PS1.sln`. Target `pslab` xuất ra AVOPSEMU.exe. Không chạy EXE PSLab từ thư mục build cũ. Nếu CMake giữ tên solution cũ, tạo một thư mục build mới.

Nạp BIOS → Mở đĩa → Boot đĩa → Chạy. Dùng F5 để chạy/tạm dừng, F9 để xem debugger, F11 để ẩn/hiện thanh bên. Thoát chế độ rạp cũng có trong menu Hiển thị.

Kiểm thử lõi:

```powershell
cmake --build build-vs --config Release
ctest --test-dir build-vs -C Release --output-on-failure
```

Nguồn C++17/Win32, không cần Qt/SDL/NuGet. Chưa kiểm chứng build hoặc UI Windows tại môi trường tạo bản cập nhật này.

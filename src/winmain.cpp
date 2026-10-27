#include <windows.h>
#include <commdlg.h>
#include "ui_theme.hpp"
#include "ui_layout.hpp"
#include "core.hpp"
#include "display.hpp"
#include "diagnostic_capture.hpp"
#include "runner.hpp"
#include "audio_win32.hpp"
#include "keyboard_pad.hpp"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <memory>
#include <chrono>

namespace {
std::unique_ptr<ps1::Bus> bus;
std::unique_ptr<ps1::Cpu> cpu;
std::mutex machine_mutex;
std::unique_ptr<ps1::Runner> runner;
ps1::AudioOutput audio_output;
std::vector<uint32_t> presentation_pixels;
ps1::Gpu presentation; // UI-owned VRAM snapshot; convert RGB after releasing the core lock
bool limit_speed=true,muted=false;
uint64_t painted_words=~uint64_t(0),painted_display=~uint64_t(0),painted_video=~uint64_t(0);
HWND output,screen,pad_hint;
ps1::KeyboardPad keyboard;
ps1::HostPad host_pad;
std::filesystem::path keyboard_path;
bool wasd_profile=false;
uint64_t input_events=0,input_samples=0;
uint16_t last_host_buttons=0xffff,mouse_pad_buttons=0xffff;
ULONGLONG mouse_pad_until=0;
bool input_focused=false;
void save_keyboard(HWND w,bool clear_overrides=false){
 const bool saved=(!clear_overrides||WritePrivateProfileStringW(L"Keyboard",nullptr,nullptr,keyboard_path.c_str()))&&
  WritePrivateProfileStringW(L"Keyboard",L"Wasd",wasd_profile?L"1":L"0",keyboard_path.c_str())&&
  WritePrivateProfileStringW(L"Keyboard",L"NeutralOpposites",keyboard.neutral_opposites?L"1":L"0",keyboard_path.c_str());
 if(!saved)MessageBoxW(w,L"Dang dung cau hinh moi, nhung khong ghi duoc tep cau hinh. Kiem tra quyen ghi thu muc exe.",L"AVOPSEMU — Cấu hình phím",MB_OK|MB_ICONWARNING);
}
std::array<HWND,22> buttons{};
bool large_view=false,debug_visible=false,show_welcome=true;
HFONT ui_font=nullptr,title_font=nullptr,small_font=nullptr;
HBRUSH background_brush=nullptr,panel_brush=nullptr;
void paint_shell(HWND w,HDC dc);

std::wstring bios_name=L"Chưa nạp BIOS",disc_name=L"Chưa mở đĩa";
avopsui::Layout shell_layout;
void layout(HWND w){
 RECT r{};GetClientRect(w,&r);shell_layout=avopsui::layout(r.right,r.bottom,large_view,debug_visible);
 for(auto button:buttons)ShowWindow(button,SW_HIDE);
 const unsigned toolbar[]={1,11,15,4,5,21};
 const int button_width=std::min(164,(static_cast<int>(r.right)-40-5*8)/6);
 for(unsigned n=0;n<6;n++){auto button=buttons[toolbar[n]];ShowWindow(button,SW_SHOW);MoveWindow(button,20+int(n)*(button_width+8),94,button_width,40,TRUE);}
 const unsigned side[]={18,19,13,10};
 if(!large_view)for(unsigned n=0;n<4;n++){auto button=buttons[side[n]];ShowWindow(button,SW_SHOW);MoveWindow(button,36,shell_layout.sidebar.y+256+int(n)*42,192,34,TRUE);}
 auto move=[](HWND child,const avopsui::Rect& r){MoveWindow(child,r.x,r.y,r.width,r.height,TRUE);};
 ShowWindow(output,debug_visible?SW_SHOW:SW_HIDE);if(debug_visible)move(output,shell_layout.debug);
 move(screen,shell_layout.screen);move(pad_hint,shell_layout.hint);
 InvalidateRect(w,nullptr,FALSE);InvalidateRect(screen,nullptr,FALSE);
}

HFONT font;
uint32_t entry=0;
bool running=false;
ULONGLONG last_refresh=0;
bool exe_loaded=false,has_bios=false;
ULONGLONG perf_time=0;
uint64_t perf_frames=0,perf_instructions=0;
double video_hz=0,core_mips=0;
void start_perf(){std::lock_guard<std::mutex> lock(machine_mutex);perf_time=GetTickCount64();perf_frames=bus->gpu.video_frames;perf_instructions=cpu->instructions;video_hz=core_mips=0;}
void sample_perf(){
 const auto now=GetTickCount64();if(now-perf_time<1000)return;
 std::lock_guard<std::mutex> lock(machine_mutex);
 const double seconds=double(now-perf_time)/1000;
 if(bus->gpu.video_frames>=perf_frames&&cpu->instructions>=perf_instructions){video_hz=double(bus->gpu.video_frames-perf_frames)/seconds;core_mips=double(cpu->instructions-perf_instructions)/seconds/1000000;}
 perf_time=now;perf_frames=bus->gpu.video_frames;perf_instructions=cpu->instructions;
}
void update_pad(HWND w,bool sample_physical=false){
 const bool focused=GetAncestor(GetForegroundWindow(),GA_ROOT)==w;
 input_focused=focused;
 if(sample_physical){keyboard.sample([](unsigned key){return (GetAsyncKeyState(int(key))&0x8000)!=0;},focused);input_samples++;}
 if(!focused){keyboard.release();mouse_pad_buttons=0xffff;host_pad.clear();}
 if(GetTickCount64()>=mouse_pad_until)mouse_pad_buttons=0xffff;
 const auto host=uint16_t(keyboard.buttons(focused)&(focused?mouse_pad_buttons:0xffff));
 if(host!=0xffff)last_host_buttons=host;
 host_pad.publish(host);
}
std::array<uint32_t,32> initial_regs{};
std::wstring last_fault;
std::wstring status=L"Sẵn sàng • Nạp BIOS → Mở đĩa → Boot đĩa → Chạy. Z = Cross, Enter = Start.";
void refresh(){
 // Hidden diagnostics must not format every register or send Win32 messages under the core mutex.
 if(!debug_visible){InvalidateRect(GetParent(screen),nullptr,FALSE);if(screen&&!running)InvalidateRect(screen,nullptr,FALSE);return;}
 std::unique_lock<std::mutex> lock(machine_mutex);
 std::wostringstream s;
 s<<L"AVOPSEMU PS1 v51 | PS1 DEBUGGER\r\n"<<status<<L"\r\n\r\n";
 s<<L"Video Hz (model): "<<std::fixed<<std::setprecision(2)<<video_hz<<L" | CPU MIPS: "<<core_mips<<L"\r\n";
 s<<L"PC: "<<std::hex<<std::setfill(L'0')<<std::setw(8)<<cpu->pc<<L"    Next: "<<std::setw(8)<<cpu->next_pc<<L"\r\n\r\n";
 s<<L"Boot path: "<<(bus->direct_exe_boot?L"DIRECT EXE (no BIOS initialization)":L"Machine / BIOS reset")<<L"\r\n";
 s<<L"IRQ exceptions: "<<std::dec<<cpu->interrupts<<L" | Reserved: "<<cpu->reserved_exceptions<<L"\r\n";
 s<<L"GTE commands: "<<cpu->gte.commands<<L"\r\n";
 s<<L"MDEC blocks: "<<bus->mdec.macroblocks<<L" | Status: "<<std::hex<<bus->mdec.status()<<std::dec<<L"\r\n";
 if(cpu->first_fault.valid)s<<L"First CPU fault: "<<std::dec<<cpu->first_fault.code<<L" PC="<<std::hex<<cpu->first_fault.pc<<L" opcode="<<cpu->first_fault.opcode<<L"\r\n";
 s<<std::hex;
 s<<L"HI: "<<std::setw(8)<<cpu->hi<<L" LO: "<<std::setw(8)<<cpu->lo<<L"\r\n";
 s<<L"SR: "<<std::setw(8)<<cpu->cop0[12]<<L" Cause: "<<std::setw(8)<<cpu->cop0[13]<<L" EPC: "<<std::setw(8)<<cpu->cop0[14]<<L"\r\n\r\n";
 for(unsigned i=0;i<32;i++){
  s<<L"R"<<std::dec<<std::setfill(L'0')<<std::setw(2)<<i<<L"  "<<std::hex<<std::setw(8)<<cpu->r[i]<<((i%2==1)?L"\r\n":L"    ");
 }
 s<<L"\r\nInstructions: "<<std::dec<<cpu->instructions<<L" POST: "<<std::hex<<unsigned(bus->post)<<L"\r\n";
 s<<L"\r\nPAD1 host="<<std::hex<<host_pad.held_buttons()<<L" pending="<<host_pad.pending_presses()<<L" last_host="<<last_host_buttons<<L" reply="<<bus->sio[0].latched_buttons<<L" last_pressed_reply="<<bus->sio[0].last_pressed_buttons<<L"\r\n";
 s<<L"Host focus="<<input_focused<<L" events="<<std::dec<<input_events<<L" samples="<<input_samples<<L" | SIO polls="<<bus->sio[0].polls<<L" complete="<<bus->sio[0].completed_polls<<L" pressed="<<bus->sio[0].pressed_polls<<L" CTRL="<<std::hex<<bus->sio[0].control<<L" phase="<<std::dec<<bus->sio[0].phase<<L"\r\n";
 s<<L"\r\nDMA words: "<<std::dec<<bus->dma_words<<L"\r\n";
 const auto disc_info=bus->cd.disc.description();s<<L"Disc: "<<std::wstring(disc_info.begin(),disc_info.end())<<L"\r\n";
 s<<L"CD sectors: "<<std::dec<<bus->cd.sectors<<L" Read: "<<bus->cd.sectors_read<<L" Commands: "<<bus->cd.commands<<L"\r\n";
 s<<L"Video frames: "<<bus->gpu.video_frames<<L" Line: "<<bus->gpu.video_line<<L" VBlank: "<<bus->gpu.vblank_events<<L"\r\n";
 s<<L"GPU polygons: "<<bus->gpu.polygons<<L" Sprites: "<<bus->gpu.rectangles<<L"\r\n";
 s<<L"SPU writes: "<<bus->spu.writes<<L" | Voices: "<<bus->spu.active_voices()<<L" | Samples: "<<bus->spu.generated_frames<<L"\r\n";
 s<<L"Audio: "<<(audio_output.ready()?L"44100 Hz stereo / WSOLA":L"device closed")<<L" | MMCSS: "<<audio_output.multimedia_scheduled()<<L" | Underruns: "<<audio_output.underruns()<<L" | Tempo / WSOLA: "<<audio_output.ratio()<<L" | Buffered: "<<audio_output.buffered_frames()<<L" | Dropped: "<<(bus->spu.dropped_frames+audio_output.dropped())<<L" | Pace: "<<(limit_speed?L"1x model":L"Max")<<L"\r\n";
 s<<L"\r\nRAM[0x100]: "<<std::dec<<bus->read32(256)<<L"\r\n\r\n";
 const auto view=ps1::display_view(bus->gpu);
 s<<L"Display source: "<<view.x<<L","<<view.y<<L" "<<view.width<<L"x"<<view.height<<L" | Fit 4:3\r\n";
 s<<L"GPU: partial. SPU: ADPCM/ADSR stereo (partial). CD-ROM: ISO/BIN/CUE subset.\r\nBIOS co the dung o lenh hoac MMIO chua ho tro.\r\nPad: mui ten, Z/X/A/S, Enter/Shift. Homebrew direct boot; GTA1 da vao man choi tren may nguoi dung; tuong thich chua day du.";
 std::string tty=bus->tty;s<<L"\r\nTTY: "<<std::wstring(tty.begin(),tty.end());
 const auto text=s.str();lock.unlock();
 SetWindowTextW(output,text.c_str());
 InvalidateRect(GetParent(screen),nullptr,FALSE);
 if(screen&&!running)InvalidateRect(screen,nullptr,FALSE);
}
void stop(HWND w){host_pad.clear();mouse_pad_buttons=0xffff;mouse_pad_until=0;keyboard.release();update_pad(w);running=false;KillTimer(w,1);if(runner){runner->stop();runner.reset();}audio_output.close();}
void start_run(HWND w){
 show_welcome=false;
 start_perf();update_pad(w);
 {std::lock_guard<std::mutex> lock(machine_mutex);bus->spu.audio.clear();bus->sio[0].connected=true;bus->sio[0].host_input=&host_pad;}
 audio_output.set_muted(muted);
 const bool audio_ok=audio_output.open(bus->spu,machine_mutex);
 try{runner=std::make_unique<ps1::Runner>(*cpu,machine_mutex);runner->start(limit_speed);}
 catch(const std::exception& e){stop(w);std::string message=e.what();status=L"Run: "+std::wstring(message.begin(),message.end());refresh();return;}
 running=SetTimer(w,1,16,nullptr)!=0;
 if(!running){stop(w);status=L"Khong tao duoc UI timer.";}
 else{SetFocus(screen);status=audio_ok?L"Đang chạy • Âm thanh stereo 44,1 kHz.":L"Đang chạy • Không mở được thiết bị âm thanh Windows.";}
 refresh();
}
void advance(HWND w,unsigned n){
 show_welcome=false;
 try{for(unsigned i=0;i<n;i++){cpu->step();if(cpu->bios_system_error)throw std::runtime_error(cpu->failure_summary());}status=L"Da thuc thi lenh.";}
 catch(const std::exception& e){stop(w);std::string message=e.what();status=L"Dừng: "+std::wstring(message.begin(),message.end());last_fault=status;}
 refresh();
}
void loadBios(HWND w){
 wchar_t file[32768]{};OPENFILENAMEW o{};o.lStructSize=sizeof(o);o.hwndOwner=w;o.lpstrFile=file;o.nMaxFile=32768;
 o.lpstrFilter=L"BIOS (*.bin)\0*.bin\0All files\0*.*\0";o.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
 if(!GetOpenFileNameW(&o))return;
 stop(w);
 try {
  std::ifstream f(std::filesystem::path(file),std::ios::binary|std::ios::ate);
  if(!f||f.tellg()!=std::streamoff(512*1024))throw std::runtime_error("BIOS must be exactly 512 KiB");
  std::vector<uint8_t> image(512*1024);f.seekg(0);
  if(!f.read(reinterpret_cast<char*>(image.data()),image.size()))throw std::runtime_error("Read failed");
  show_welcome=false;ps1::boot_bios(*bus,*cpu,image);bios_name=std::filesystem::path(file).filename().wstring();exe_loaded=false;has_bios=true;entry=0xbfc00000;last_fault.clear();
  status=bus->cd.sectors?L"Đã nạp BIOS, giữ nguyên đĩa. Chọn Boot đĩa → Chạy.":L"Đã nạp BIOS. Bấm Chạy để xem BIOS hoặc Mở đĩa → Boot đĩa → Chạy.";
 }catch(const std::exception&){status=L"Không nạp được BIOS. Cần tệp BIOS PS1 hợp lệ, đúng 512 KiB.";}
 refresh();
}
void openExe(HWND w){
 wchar_t file[32768]{};OPENFILENAMEW o{};o.lStructSize=sizeof(o);o.hwndOwner=w;o.lpstrFile=file;o.nMaxFile=32768;
 o.lpstrFilter=L"PS-X EXE (*.exe;*.ps-exe)\0*.exe;*.ps-exe\0All files\0*.*\0";o.Flags=OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR;
 if(!GetOpenFileNameW(&o))return;
 stop(w);
 try{
  std::ifstream f(std::filesystem::path(file),std::ios::binary|std::ios::ate);
  auto size=f.tellg();if(!f||size<0x800||size>4*1024*1024)throw std::runtime_error("EXE file size invalid (max 4 MiB)");
  std::vector<uint8_t> bytes(static_cast<size_t>(size));f.seekg(0);
  if(!f.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))throw std::runtime_error("EXE read failed");
  auto fresh=std::make_unique<ps1::Bus>();auto freshCpu=std::make_unique<ps1::Cpu>(*fresh);
  show_welcome=false;ps1::load_exe(*fresh,*freshCpu,bytes);
  cpu.reset();bus=std::move(fresh);cpu=std::move(freshCpu);entry=cpu->pc;initial_regs=cpu->r;exe_loaded=true;has_bios=false;bios_name=L"Homebrew trực tiếp";disc_name=std::filesystem::path(file).filename().wstring();
  status=L"PS-X EXE loaded. Bam Run. Khong co BIOS services; chi homebrew tu khoi tao.";
 }catch(const std::exception& e){std::string text=e.what();status=L"EXE: "+std::wstring(text.begin(),text.end());}
 refresh();
}
void mountIso(HWND w){
 wchar_t path[32768]{};OPENFILENAMEW o{};o.lStructSize=sizeof(o);o.hwndOwner=w;o.lpstrFile=path;o.nMaxFile=32768;o.lpstrFilter=L"PS1 disc (*.cue;*.bin;*.iso)\0*.cue;*.bin;*.iso\0All files\0*.*\0";o.Flags=OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR;
 if(!GetOpenFileNameW(&o))return;stop(w);
 try{bus->cd.mount(std::filesystem::path(path));disc_name=std::filesystem::path(path).filename().wstring();status=L"Đã mở đĩa. Nạp BIOS nếu chưa có → Boot đĩa → Chạy.";}
 catch(const std::exception&e){std::string msg=e.what();status=std::wstring(msg.begin(),msg.end());}refresh();
}
void paint_shell(HWND w,HDC dc){
 RECT client{};GetClientRect(w,&client);FillRect(dc,&client,background_brush);
 ps1ui::label(dc,L"AVOPSEMU",RECT{20,12,380,62},title_font,ps1ui::text);
 ps1ui::label(dc,L"PLAYSTATION EMULATION  /  v51",RECT{22,62,400,82},small_font,ps1ui::accent);
 std::wostringstream metrics;metrics<<std::fixed<<std::setprecision(1)<<video_hz<<L" Hz mô hình   ·   "<<core_mips<<L" MIPS   ·   "<<(running?L"ĐANG CHẠY":L"TẠM DỪNG");
 ps1ui::label(dc,metrics.str().c_str(),RECT{420,26,client.right-20,62},ui_font,running?ps1ui::accent:ps1ui::muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
 if(!large_view){
  const auto& p=shell_layout.sidebar;const int y=p.y;
  ps1ui::box(dc,RECT{p.x,y,p.x+p.width,y+p.height},ps1ui::panel,ps1ui::border);
  ps1ui::label(dc,L"PHIÊN CHƠI PS1",RECT{36,y+12,228,y+40},small_font,ps1ui::accent);
  ps1ui::label(dc,L"BIOS  /  512 KiB",RECT{36,y+48,228,y+72},small_font,ps1ui::muted);
  ps1ui::label(dc,bios_name.c_str(),RECT{36,y+76,228,y+102},ui_font,ps1ui::text,DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
  ps1ui::label(dc,L"ĐĨA / CHƯƠNG TRÌNH",RECT{36,y+112,228,y+138},small_font,ps1ui::muted);
  ps1ui::label(dc,disc_name.c_str(),RECT{36,y+142,228,y+174},ui_font,ps1ui::text,DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
  ps1ui::label(dc,L"DIGITAL PAD 1",RECT{36,y+184,228,y+210},small_font,ps1ui::accent);
  ps1ui::label(dc,wasd_profile?L"WASD + J / K / U / I":L"Mũi tên + Z / X / A / S",RECT{36,y+214,228,y+244},small_font,ps1ui::text);
 }
 const auto& r=shell_layout.status;
 ps1ui::box(dc,RECT{r.x,r.y,r.x+r.width,r.y+r.height},ps1ui::panel,ps1ui::border,8);
 ps1ui::label(dc,status.c_str(),RECT{r.x+10,r.y,r.x+r.width-10,r.y+r.height},small_font,ps1ui::muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
}

LRESULT CALLBACK proc(HWND w,UINT m,WPARAM wp,LPARAM lp){
 switch(m){
 case WM_CREATE:{
  const wchar_t* labels[]={L"Demo CPU",L"1. Nạp BIOS",L"Đặt lại CPU (giữ RAM)",L"Step",L"4. Chạy",L"Tạm dừng",L"Demo GPU",L"Polygon",L"Open EXE",L"Chạy 10.000 lệnh",L"Lưu log + ảnh",L"2. Mở đĩa",L"Homebrew EXE",L"Chế độ rạp",L"Demo texture",L"3. Boot đĩa",L"Region CD",L"Tháo đĩa",L"Tốc độ: 1x",L"Âm thanh: Bật",L"SPU test",L"Bàn phím"};
  for(int i=0;i<22;i++){auto b=CreateWindowW(L"BUTTON",labels[i],WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,16+(i%8)*120,16+(i/8)*42,110,36,w,reinterpret_cast<HMENU>(INT_PTR(101+i)),nullptr,nullptr);buttons[i]=b;SendMessageW(b,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);}
  pad_hint=CreateWindowW(L"STATIC",L"Mũi tên: di chuyển   Z/X/A/S: Cross/Circle/Square/Triangle\nEnter: Start   Shift: Select   Q/W: L1/R1   E/R: L2/R2",WS_CHILD|WS_VISIBLE,16,90,1300,24,w,nullptr,nullptr,nullptr);
  output=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY,16,108,740,400,w,nullptr,nullptr,nullptr);
  screen=CreateWindowW(L"PS1Vram",L"",WS_CHILD|WS_VISIBLE,780,108,640,480,w,nullptr,GetModuleHandleW(nullptr),nullptr);
  font=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,FIXED_PITCH,L"Consolas");
  for(auto button:buttons)SendMessageW(button,WM_SETFONT,reinterpret_cast<WPARAM>(ui_font),TRUE);
  if(wasd_profile)SetWindowTextW(pad_hint,L"WASD: di chuyển   J/K/U/I: Cross/Circle/Square/Triangle\nEnter: Start   Tab: Select   Q/E: L1/R1   1/3: L2/R2");
  SendMessageW(pad_hint,WM_SETFONT,reinterpret_cast<WPARAM>(small_font),TRUE);
  HMENU menu=CreateMenu(),file=CreatePopupMenu(),view=CreatePopupMenu(),tools=CreatePopupMenu();
  AppendMenuW(file,MF_STRING,102,L"Nạp BIOS PS1...");AppendMenuW(file,MF_STRING,112,L"Mở đĩa BIN / CUE / ISO...");AppendMenuW(file,MF_STRING,116,L"Boot đĩa qua BIOS");AppendMenuW(file,MF_STRING,109,L"Mở homebrew EXE...");AppendMenuW(file,MF_STRING,111,L"Lưu log + ảnh VRAM...");AppendMenuW(file,MF_STRING,124,L"Thoát");
  AppendMenuW(view,MF_STRING,123,L"Bật / tắt debugger   F9");AppendMenuW(view,MF_STRING,114,L"Bật / tắt chế độ rạp   F11");AppendMenuW(view,MF_STRING,122,L"Hướng dẫn bàn phím");
  AppendMenuW(view,MF_STRING,129,L"Thử giữ Cross trong 1 giây");
  AppendMenuW(view,MF_STRING,130,L"Thử giữ Start trong 1 giây");
  AppendMenuW(view,MF_STRING|(!wasd_profile?MF_CHECKED:0),126,L"Bàn phím: Mũi tên + Z/X/A/S");
  AppendMenuW(view,MF_STRING|(wasd_profile?MF_CHECKED:0),127,L"Bàn phím: WASD + J/K/U/I");
  AppendMenuW(view,MF_STRING|(keyboard.neutral_opposites?MF_CHECKED:0),128,L"Trung hòa hai hướng đối nghịch");
  const unsigned ids[]={103,104,110,117,118,101,107,108,115,121,113};
  const wchar_t* names[]={L"Đặt lại CPU (giữ RAM)",L"Chạy 1 lệnh",L"Chạy 10.000 lệnh",L"Đổi vùng CD",L"Tháo đĩa",L"Demo CPU",L"Demo GPU",L"Demo đa giác",L"Demo texture",L"Kiểm tra âm thanh",L"Boot homebrew trực tiếp"};
  for(unsigned n=0;n<11;n++)AppendMenuW(tools,MF_STRING,ids[n],names[n]);
  
  AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(file),L"Tệp");AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(view),L"Hiển thị");AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(tools),L"Công cụ");AppendMenuW(view,MF_STRING,131,L"Màn hình chào");AppendMenuW(view,MF_STRING,132,L"Về AVOPSEMU");SetMenu(w,menu);
  SendMessageW(output,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);SetTimer(w,2,8,nullptr);layout(w);refresh();return 0;}
 case WM_ERASEBKGND:return 1;
 case WM_PAINT:{PAINTSTRUCT paint{};auto dc=BeginPaint(w,&paint);RECT r{};GetClientRect(w,&r);
  auto memory=CreateCompatibleDC(dc);auto bitmap=CreateCompatibleBitmap(dc,std::max(1,int(r.right)),std::max(1,int(r.bottom)));
  if(memory&&bitmap){auto old=SelectObject(memory,bitmap);paint_shell(w,memory);BitBlt(dc,0,0,r.right,r.bottom,memory,0,0,SRCCOPY);SelectObject(memory,old);}
  else paint_shell(w,dc);
  if(bitmap)DeleteObject(bitmap);if(memory)DeleteDC(memory);EndPaint(w,&paint);return 0;}
 case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:{auto dc=reinterpret_cast<HDC>(wp);SetTextColor(dc,ps1ui::muted);SetBkColor(dc,ps1ui::background);return reinterpret_cast<LRESULT>(background_brush);}
 case WM_DRAWITEM:{auto item=reinterpret_cast<DRAWITEMSTRUCT*>(lp);if(item->CtlType!=ODT_BUTTON)break;
  const bool primary=item->CtlID==105||item->CtlID==116,pressed=item->itemState&ODS_SELECTED;
  RECT r=item->rcItem;ps1ui::box(item->hDC,r,primary?(pressed?RGB(88,80,185):ps1ui::accent):(pressed?RGB(47,45,66):ps1ui::panel),primary?ps1ui::accent:ps1ui::border,10);
  wchar_t text[128]{};GetWindowTextW(item->hwndItem,text,128);ps1ui::label(item->hDC,text,r,ui_font,primary?ps1ui::background:ps1ui::text,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
  if(item->itemState&ODS_FOCUS){InflateRect(&r,-4,-4);DrawFocusRect(item->hDC,&r);}return TRUE;}
 case WM_ACTIVATEAPP:if(!wp){keyboard.release();host_pad.clear();mouse_pad_buttons=0xffff;}return 0;
 case WM_SIZE:if(output&&screen)layout(w);return 0;
 case WM_GETMINMAXINFO:{auto p=reinterpret_cast<MINMAXINFO*>(lp);p->ptMinTrackSize={1040,780};return 0;}
 case WM_COMMAND:
  switch(LOWORD(wp)){
   case 129:case 130:
    if(!running)start_run(w);
    if(running){SetFocus(screen);mouse_pad_buttons=uint16_t(0xffffu^(LOWORD(wp)==129?0x4000u:8u));mouse_pad_until=GetTickCount64()+1000;update_pad(w);}
    break;
   case 126:case 127:wasd_profile=LOWORD(wp)==127;keyboard.preset(wasd_profile);host_pad.clear();update_pad(w);save_keyboard(w,true);CheckMenuItem(GetMenu(w),126,MF_BYCOMMAND|(!wasd_profile?MF_CHECKED:MF_UNCHECKED));CheckMenuItem(GetMenu(w),127,MF_BYCOMMAND|(wasd_profile?MF_CHECKED:MF_UNCHECKED));InvalidateRect(w,nullptr,FALSE);
    SetWindowTextW(pad_hint,wasd_profile?L"KEYBOARD   WASD D-pad | J Cross / K Circle / U Square / I Triangle | Enter Start / Tab Select | Q/E L1/R1 | 1/3 L2/R2":L"KEYBOARD   Arrows D-pad | Z Cross / X Circle / A Square / S Triangle | Enter Start / Shift Select | Q/W L1/R1 | E/R L2/R2");SetFocus(screen);break;
   case 128:host_pad.clear();keyboard.release();keyboard.neutral_opposites=!keyboard.neutral_opposites;update_pad(w);save_keyboard(w);CheckMenuItem(GetMenu(w),128,MF_BYCOMMAND|(keyboard.neutral_opposites?MF_CHECKED:MF_UNCHECKED));break;
   case 123:debug_visible=!debug_visible;CheckMenuItem(GetMenu(w),123,MF_BYCOMMAND|(debug_visible?MF_CHECKED:MF_UNCHECKED));layout(w);refresh();break;
   case 124:DestroyWindow(w);break;
   case 131:stop(w);show_welcome=true;status=L"Màn hình chào • Phiên chơi được giữ lại. Bấm Chạy để tiếp tục.";refresh();break;
   case 132:MessageBoxW(w,L"AVOPSEMU PS1 v51\nGiả lập PlayStation — C++17 / Win32\n\nPS1: CPU, GPU, CD-ROM, SPU và digital pad.\nPS2 đã được tách thành dự án AVOPSEMU_PS2 riêng.\n\nF5: Chạy / Tạm dừng\nF9: Debugger\nF11: Chế độ rạp\n\nKhả năng tương thích game vẫn đang được phát triển.",L"Về AVOPSEMU",MB_OK|MB_ICONINFORMATION);break;
   case 122:{keyboard.release();update_pad(w);MessageBoxW(w,L"Digital Pad 1 - ban phim\n\nHiển thị > Bàn phím để đổi preset. WASD preset: WASD di chuyen; J/K/U/I = Cross/Circle/Square/Triangle; Q/E = L1/R1; 1/3 = L2/R2; Tab = Select.\n\nPreset mac dinh:\nMui ten: D-pad\nZ: Cross (X) - chức năng tùy game\nX: Circle (O)\nA: Square\nS: Triangle\nEnter: Start / Pause trong game\nShift: Select\nQ / W: L1 / R1\nE / R: L2 / R2\n\nBam vao man hinh game truoc khi choi. Giu nhieu phim cung luc duoc.\nAlt-Tab se nha tat ca nut. Day la digital pad, chua co analog/rung.",L"AVOPSEMU — Bàn phím PS1",MB_OK);SetFocus(screen);break;}

   case 115:{stop(w);exe_loaded=false;entry=0;const auto n=ps1::install_texture_demo(*bus);cpu->reset(0);advance(w,n);status=L"Texture demo: CPU upload -> textured quads, modulation, sprite va blend.";refresh();break;}
   case 114:large_view=!large_view;SetWindowTextW(buttons[13],large_view?L"Hiện thanh bên":L"Chế độ rạp");layout(w);break;
   case 101:stop(w);exe_loaded=false;entry=0;ps1::install_demo(*bus);cpu->reset(entry);advance(w,4);status=L"Demo hoan tat: 7 + 5 = 12. Xem RAM[0x100].";refresh();break;
   case 112:mountIso(w);break;
   case 113:{stop(w);try{ps1::boot_disc(*bus,*cpu,true);entry=cpu->pc;initial_regs=cpu->r;exe_loaded=true;last_fault.clear();start_perf();status=L"Homebrew EXE loaded. KHONG co kernel BIOS. Game thuong mai: Nap BIOS -> Boot Game CD -> Run.";}catch(const std::exception&e){std::string msg=e.what();status=std::wstring(msg.begin(),msg.end());}refresh();break;}
   case 116:{stop(w);try{if(!has_bios)throw std::runtime_error("Nap BIOS truoc, sau do Mount CD");ps1::boot_disc(*bus,*cpu,false);exe_loaded=false;entry=cpu->pc;last_fault.clear();start_perf();status=L"Đã reset BIOS với đĩa đã mở. Bấm Chạy để khởi động game.";}catch(const std::exception&e){std::string msg=e.what();status=std::wstring(msg.begin(),msg.end());}refresh();break;}
   case 117:{stop(w);bus->cd.region=(bus->cd.region+1)%5;const wchar_t* names[]={L"Auto (BOOT prefix)",L"USA",L"Europe",L"Japan",L"Unlicensed"};status=L"GetID region emulated: "+std::wstring(names[bus->cd.region]);refresh();break;}
   case 121:stop(w);exe_loaded=false;entry=0;ps1::install_spu_demo(*bus);cpu->reset(0);start_run(w);status=L"Original SPU test: hai tan so trai/phai; khong can BIOS.";refresh();break;
   case 119:{bool resume=running;stop(w);limit_speed=!limit_speed;SetWindowTextW(buttons[18],limit_speed?L"Tốc độ: 1x":L"Tốc độ: Max");if(resume)start_run(w);else refresh();break;}
   case 120:muted=!muted;audio_output.set_muted(muted);SetWindowTextW(buttons[19],muted?L"Âm thanh: Tắt":L"Âm thanh: Bật");refresh();break;
   case 118:stop(w);bus->cd.eject();disc_name=L"Chưa mở đĩa";status=L"Đã tháo đĩa.";refresh();break;
   case 111:{
    stop(w);wchar_t path[32768]=L"avopsemu_ps1_log.txt";OPENFILENAMEW o{};o.lStructSize=sizeof(o);o.hwndOwner=w;o.lpstrFile=path;o.nMaxFile=32768;o.lpstrFilter=L"Text log\0*.txt\0";o.lpstrDefExt=L"txt";o.Flags=OFN_OVERWRITEPROMPT|OFN_NOCHANGEDIR;
    if(GetSaveFileNameW(&o)){std::ofstream f(std::filesystem::path(path),std::ios::binary);std::string fault;for(wchar_t ch:last_fault)fault.push_back(ch<=127?static_cast<char>(ch):'?');f<<fault<<"\nHOST_INPUT last_pressed="<<std::hex<<last_host_buttons<<" held="<<host_pad.held_buttons()<<" pending_presses="<<host_pad.pending_presses()<<" events="<<std::dec<<input_events<<" samples="<<input_samples<<" focused_before_export="<<input_focused<<"\nMIPS="<<core_mips<<" VideoHz="<<video_hz<<" paced="<<limit_speed<<" audio_underruns="<<audio_output.underruns()<<" audio_tempo="<<audio_output.ratio()<<" audio_host_drops="<<audio_output.dropped()<<" audio_buffered="<<audio_output.buffered_frames()<<" audio_mmcss="<<audio_output.multimedia_scheduled()<<" audio_error="<<audio_output.error()<<"\n"<<cpu->diagnostic();const bool snapshots=ps1::save_debug_images(*bus,std::filesystem::path(path));f<<"\nSnapshot sidecars: "<<(snapshots?"VRAM binary/BMP + selected framebuffer BMP saved":"snapshot write failed")<<"\n";status=f?(snapshots?L"Đã lưu log + ảnh VRAM và framebuffer BMP.":L"Đã lưu log; không lưu được đầy đủ ảnh."):L"Không lưu được log.";refresh();}break;}
   case 109:openExe(w);break;
   case 110:stop(w);advance(w,10000);break;
   case 108:{stop(w);exe_loaded=false;entry=0;unsigned n=ps1::install_polygon_demo(*bus);cpu->reset(0);advance(w,n);status=L"Polygon demo: tam giac va tu giac mau phang qua CPU -> GP0.";refresh();break;}
   case 107:{stop(w);exe_loaded=false;entry=0;unsigned n=ps1::install_gpu_demo(*bus);cpu->reset(0);advance(w,n);status=L"GPU demo: CPU -> SW -> GP0 -> VRAM. Khung xem 320x240.";refresh();break;}
   case 102:loadBios(w);break;
   case 103:stop(w);cpu->reset(entry);if(exe_loaded)cpu->r=initial_regs;status=L"Đã đặt lại CPU; RAM được giữ lại.";refresh();break;
   case 104:stop(w);advance(w,1);break;
   case 105:if(!running)start_run(w);break;
   case 106:stop(w);status=L"Đã tạm dừng. Bấm Chạy hoặc F5 để tiếp tục.";refresh();break;
  }return 0;
 case WM_TIMER:if(wp==2){update_pad(w,true);return 0;}if(wp==1&&running){
  sample_perf();
  if(GetTickCount64()-last_refresh>=250){std::wstring hint=wasd_profile?L"WASD: di chuyển   J/K/U/I: Cross/Circle/Square/Triangle":L"Mũi tên: di chuyển   Z/X/A/S: Cross/Circle/Square/Triangle";
   hint+=L"\nEnter: Start   ·   F5: Chạy / Tạm dừng   ·   ";hint+=input_focused?L"Bàn phím đang hoạt động":L"Bấm vào màn hình game để nhận phím";
   SetWindowTextW(pad_hint,hint.c_str());}

  if(!runner->active()){
   const auto error=runner->error();stop(w);status=L"Dừng: "+std::wstring(error.begin(),error.end());last_fault=status;refresh();return 0;
  }
  if(audio_output.error()!=MMSYSERR_NOERROR&&audio_output.ready()==false){audio_output.close();status=L"Loi phat am thanh Windows; CPU van chay.";}
  bool changed=false;
  {std::lock_guard<std::mutex> lock(machine_mutex);changed=(bus->gpu.words!=painted_words&&bus->gpu.video_frames!=painted_video)||bus->gpu.display_revision!=painted_display;}
  if(changed&&!IsIconic(w))InvalidateRect(screen,nullptr,FALSE);
  if(GetTickCount64()-last_refresh>=250){refresh();last_refresh=GetTickCount64();}
 }return 0;
 case WM_DESTROY:KillTimer(w,2);stop(w);if(font)DeleteObject(font);PostQuitMessage(0);return 0;
 }return DefWindowProcW(w,m,wp,lp);
}
}
LRESULT CALLBACK vramProc(HWND w,UINT m,WPARAM wp,LPARAM lp){
 if(m==WM_LBUTTONDOWN){SetFocus(w);return 0;}
 if(m==WM_KEYDOWN&&wp==VK_ESCAPE){SendMessageW(GetParent(w),WM_COMMAND,106,0);return 0;}
 if(m==WM_PAINT){
  PAINTSTRUCT paint{};HDC dc=BeginPaint(w,&paint);
  if(show_welcome){
   RECT area{};GetClientRect(w,&area);FillRect(dc,&area,background_brush);
   const int cx=area.right/2,cy=area.bottom/2;
   ps1ui::box(dc,RECT{cx-88,cy-138,cx+88,cy-42},ps1ui::panel,ps1ui::accent,28);
   ps1ui::label(dc,L"AV",RECT{cx-88,cy-138,cx+88,cy-42},title_font,ps1ui::accent,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
   ps1ui::label(dc,L"AVOPSEMU",RECT{20,cy-24,area.right-20,cy+36},title_font,ps1ui::text,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
   ps1ui::label(dc,L"Trở lại thế giới PlayStation.",RECT{20,cy+40,area.right-20,cy+72},ui_font,ps1ui::muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
   ps1ui::label(dc,L"Nạp BIOS → Mở đĩa → Boot đĩa → Chạy",RECT{12,area.bottom-58,area.right-12,area.bottom-20},small_font,ps1ui::accent,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
   EndPaint(w,&paint);return 0;
  }
  std::unique_lock<std::mutex> lock(machine_mutex);
  const auto view=ps1::display_view(bus->gpu);
  std::copy(bus->gpu.vram.begin(),bus->gpu.vram.end(),presentation.vram.begin());
  painted_words=bus->gpu.words;painted_display=bus->gpu.display_revision;painted_video=bus->gpu.video_frames;lock.unlock();
  presentation_pixels.resize(view.width*view.height);
  auto& pixels=presentation_pixels;
  for(unsigned y=0;y<view.height;y++)for(unsigned x=0;x<view.width;x++)pixels[y*view.width+x]=ps1::display_pixel(presentation,view,x,y);
  BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=static_cast<LONG>(view.width);info.bmiHeader.biHeight=-static_cast<LONG>(view.height);
  info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
  RECT r{};GetClientRect(w,&r);SetStretchBltMode(dc,COLORONCOLOR);
  FillRect(dc,&r,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
  const auto dest=ps1::fit_display(r.right,r.bottom);
  if(dest.width>0&&dest.height>0)StretchDIBits(dc,dest.x,dest.y,dest.width,dest.height,0,0,static_cast<int>(view.width),static_cast<int>(view.height),pixels.data(),&info,DIB_RGB_COLORS,SRCCOPY);
  EndPaint(w,&paint);return 0;
 }
 return DefWindowProcW(w,m,wp,lp);
}
int WINAPI wWinMain(HINSTANCE h,HINSTANCE,PWSTR,int show){
 background_brush=CreateSolidBrush(ps1ui::background);panel_brush=CreateSolidBrush(ps1ui::panel);
 ui_font=CreateFontW(-16,0,0,0,FW_MEDIUM,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
 title_font=CreateFontW(-34,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
 small_font=CreateFontW(-13,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
 wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
 keyboard_path=std::filesystem::path(executable).parent_path()/L"avopsemu.ini";
 const auto legacy_config=keyboard_path.parent_path()/L"pslab.ini";
 if(GetFileAttributesW(keyboard_path.c_str())==INVALID_FILE_ATTRIBUTES&&GetFileAttributesW(legacy_config.c_str())!=INVALID_FILE_ATTRIBUTES)
  CopyFileW(legacy_config.c_str(),keyboard_path.c_str(),TRUE);
 // If migration cannot write the new file, continue reading the existing bindings.
 if(GetFileAttributesW(keyboard_path.c_str())==INVALID_FILE_ATTRIBUTES&&GetFileAttributesW(legacy_config.c_str())!=INVALID_FILE_ATTRIBUTES)keyboard_path=legacy_config;
 wasd_profile=GetPrivateProfileIntW(L"Keyboard",L"Wasd",0,keyboard_path.c_str())!=0;
 keyboard.preset(wasd_profile);
 keyboard.neutral_opposites=GetPrivateProfileIntW(L"Keyboard",L"NeutralOpposites",0,keyboard_path.c_str())!=0;
 // Optional Key0..Key15 overrides use decimal Win32 virtual-key codes. Invalid or duplicate keys retain the preset.
 for(unsigned bit=0;bit<16;bit++){const auto name=L"Key"+std::to_wstring(bit);const auto key=GetPrivateProfileIntW(L"Keyboard",name.c_str(),keyboard.keys[bit],keyboard_path.c_str());keyboard.bind(bit,key);}
 bus=std::make_unique<ps1::Bus>();cpu=std::make_unique<ps1::Cpu>(*bus);ps1::install_demo(*bus);cpu->reset(0);
 WNDCLASSW c{};c.lpfnWndProc=proc;c.hInstance=h;c.lpszClassName=L"AVOPSEMUWindow";c.hIcon=LoadIconW(h,MAKEINTRESOURCEW(1));c.hCursor=LoadCursor(nullptr,IDC_ARROW);c.hbrBackground=background_brush;
 WNDCLASSW v{};v.lpfnWndProc=vramProc;v.hInstance=h;v.lpszClassName=L"PS1Vram";v.hCursor=LoadCursor(nullptr,IDC_ARROW);
 if(!RegisterClassW(&v))return 1;
 if(!RegisterClassW(&c))return 1;
 HWND w=CreateWindowW(c.lpszClassName,L"AVOPSEMU PS1 v51 — PlayStation / PS1",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1440,900,nullptr,nullptr,h,nullptr);
 if(!w)return 1;ShowWindow(w,show);MSG msg{};int result;
 while((result=GetMessageW(&msg,nullptr,0,0))>0){
  if((msg.message==WM_KEYDOWN||msg.message==WM_KEYUP)&&GetAncestor(msg.hwnd,GA_ROOT)==w){
   if(keyboard.event(unsigned(msg.wParam),msg.message==WM_KEYDOWN)){input_events++;update_pad(w);continue;}
  }
  if(msg.message==WM_KEYDOWN&&GetAncestor(msg.hwnd,GA_ROOT)==w&&!(msg.lParam&(LPARAM(1)<<30))){
   const unsigned command=msg.wParam==VK_F5?(running?106:105):msg.wParam==VK_F9?123:msg.wParam==VK_F11?114:0;
   if(command){SendMessageW(w,WM_COMMAND,command,0);continue;}
  }
  TranslateMessage(&msg);DispatchMessageW(&msg);
 }
 DeleteObject(ui_font);DeleteObject(title_font);DeleteObject(small_font);DeleteObject(background_brush);DeleteObject(panel_brush);
 return result==-1?1:static_cast<int>(msg.wParam);
}

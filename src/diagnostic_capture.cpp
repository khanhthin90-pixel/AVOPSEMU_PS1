#include "diagnostic_capture.hpp"
#include "display.hpp"
#include <fstream>
#include <sstream>
namespace ps1 {
void Bus::watch_ram_write(uint32_t address,uint32_t value,unsigned width){
 const uint32_t base=(address&0x1fffffff)&0x1fffff;
 if(watched_byte<base||watched_byte-base>=width)return;
 const auto after=uint8_t(value>>((watched_byte-base)*8));
 watch_writes[watch_total++%watch_writes.size()]={cd.clocks,writer_instruction,writer_cpu?writer_pc:0,writer_cpu?writer_opcode:0,address,ram[watched_byte],after,uint8_t(width),writer_dma,writer_cpu};
}
std::string Bus::watch_diagnostic()const{
 std::ostringstream o;o<<"WRITE_WATCH 801f0066 physical=001f0066 value="<<std::hex<<unsigned(ram[watched_byte])<<std::dec<<" writes="<<watch_total<<" (RAM aliases; diagnostic only, never clears the byte)\n";
 o<<"CD_FIFO accepted_lba="<<std::dec<<cd.data_lba<<" bytes="<<cd.data.size()<<" next_lba="<<cd.lba<<" sectors_with_old_fifo="<<cd.sectors_with_fifo_tail<<"\n";
 const auto count=std::min<uint64_t>(watch_total,watch_writes.size());
 for(uint64_t i=watch_total-count;i<watch_total;i++){
  const auto& w=watch_writes[i%watch_writes.size()];
  o<<"watch clock="<<std::dec<<w.clock<<" ins="<<w.instruction<<" source=";
  if(w.dma>=0)o<<"DMA"<<w.dma;else o<<(w.cpu?"CPU":"host");
  o<<" trigger_pc="<<std::hex<<w.pc<<" opcode="<<w.opcode<<" address="<<w.address<<" width="<<std::dec<<unsigned(w.width)<<" value="<<std::hex<<unsigned(w.before)<<"->"<<unsigned(w.after)<<"\n";
 }
 return o.str();
}
namespace {
template<class Pixel>bool bmp(const std::filesystem::path& path,unsigned width,unsigned height,Pixel pixel){
 std::ofstream f(path,std::ios::binary);if(!f)return false;
 auto put=[&](uint32_t v,unsigned n){for(unsigned i=0;i<n;i++)f.put(char(v>>(8*i)));};
 const unsigned row=(width*3+3)&~3u,size=row*height;
 f.put('B');f.put('M');put(54+size,4);put(0,4);put(54,4);put(40,4);put(width,4);put(height,4);put(1,2);put(24,2);put(0,4);put(size,4);put(0,4);put(0,4);put(0,4);put(0,4);
 for(unsigned y=height;y>0;y--){for(unsigned x=0;x<width;x++){auto c=pixel(x,y-1);f.put(char(c));f.put(char(c>>8));f.put(char(c>>16));}for(unsigned n=width*3;n<row;n++)f.put(0);}
 f.flush();return bool(f);
}
}
bool save_debug_images(const Bus& b,const std::filesystem::path& log){
 auto raw=log;raw+=".vram.bin";std::ofstream f(raw,std::ios::binary);
 for(auto p:b.gpu.vram){f.put(char(p));f.put(char(p>>8));}f.flush();const bool raw_ok=bool(f);
 auto vp=log;vp+=".vram.bmp";auto fp=log;fp+=".frame.bmp";
 const auto v=display_view(b.gpu);
 const bool vr=bmp(vp,1024,512,[&](unsigned x,unsigned y){return b.gpu.rgb(x,y);});
 const bool frame=bmp(fp,v.width,v.height,[&](unsigned x,unsigned y){return display_pixel(b.gpu,v,x,y);});
 return raw_ok&&vr&&frame;
}
}

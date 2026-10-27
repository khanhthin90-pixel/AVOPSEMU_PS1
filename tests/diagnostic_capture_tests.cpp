#include "diagnostic_capture.hpp"
#include <fstream>
#include <memory>
#include <iostream>
#include <stdexcept>
static void ck(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int main(){try{auto b=std::make_unique<ps1::Bus>();
 b->write32(0x801f0064,0x12345678);ck(b->watch_total==1&&b->watch_writes[0].after==0x34,"watch word byte-lane");
 b->write16(0xa07f0066,0xabcd);ck(b->watch_total==2&&b->watch_writes[1].before==0x34&&b->watch_writes[1].after==0xcd,"watch RAM aliases and halfword");
 b->cd.requested=true;b->cd.data={1,2,3,4};b->dpcr=0x8000;
 b->write32(0x1f8010b0,0x1f0064);b->write32(0x1f8010b4,1);b->write32(0x1f8010b8,0x11000000);
 ck(b->watch_total==3&&b->watch_writes[2].dma==3&&b->watch_writes[2].after==3&&b->writer_dma==-1,"DMA source attribution and scope restoration");
 const auto n=b->watch_total;b->watch_diagnostic();ck(b->watch_total==n&&b->read8(0x801f0066)==3,"watch snapshot never mutates target");
 for(unsigned i=0;i<20;i++){b->write8(0x801f0066,uint8_t(i));}ck(b->watch_total==23&&b->watch_diagnostic().find("writes=23")!=std::string::npos,"bounded chronological watch ring");
 b->gpu.vram[0]=31;auto path=std::filesystem::temp_directory_path()/"pslab_capture_test.txt";
 ck(ps1::save_debug_images(*b,path),"capture files saved");
 for(auto suffix:{".vram.bin",".vram.bmp",".frame.bmp"}){auto p=path;p+=suffix;std::ifstream f(p,std::ios::binary);char h[2]{};f.read(h,2);ck(bool(f),"capture readable");if(std::string(suffix)!=".vram.bin")ck(h[0]=='B'&&h[1]=='M',"standard BMP header");f.close();std::filesystem::remove(p);}
 std::cout<<"PASS: watch CPU/DMA lane/alias/source ring, read-only snapshots and raw VRAM/BMP/framebuffer exports\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

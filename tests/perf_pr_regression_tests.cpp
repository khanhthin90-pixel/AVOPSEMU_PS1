#include "core.hpp"
#include "audio_resampler.hpp"
#include "audio_stretcher.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>
namespace {
void check(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}
uint32_t xy(unsigned x,unsigned y){return (y<<16)|x;}
void verify_fill(){
 ps1::Gpu gpu;
 std::mt19937 rand(0x50912026);
 for(unsigned trial=0;trial<160;trial++){
  // Compare against the pre-optimization per-pixel fill, including horizontal
  // and vertical wrap, zero-size packets and GP0(02) mask semantics.
  for(auto& p:gpu.vram)p=uint16_t(rand());
  std::vector<uint16_t> reference=gpu.vram;
  const unsigned x=rand()&1023,y=rand()&511;
  const unsigned in_w=rand()&1023,in_h=rand()&511;
  const uint32_t color=rand()&0xffffff;
  const uint16_t pixel=uint16_t(((color>>3)&31)|(((color>>11)&31)<<5)|(((color>>19)&31)<<10));
  const unsigned left=x&~15u,w=((in_w+15)&~15u),h=in_h;
  gpu.gp0(0xe6000000|(trial&3)); // GP0(02) ignores mask flags.
  gpu.gp0(0x02000000|color);gpu.gp0(xy(x,y));gpu.gp0(xy(in_w,in_h));
  for(unsigned j=0;j<h;j++)for(unsigned i=0;i<w;i++)reference[((y+j)&511)*1024+((left+i)&1023)]=pixel;
  check(gpu.vram==reference,"GP0(02) reference-equivalence failed");
 }
}
void verify_flat_lines(){
 ps1::Gpu gpu;
 std::mt19937 rand(0x31415926);
 gpu.left=0;gpu.top=0;gpu.right=1023;gpu.bottom=511;
 for(unsigned trial=0;trial<240;trial++){
  const int x0=int(rand()%1000),x1=int(rand()%1000);
  const int y0=int(rand()%500),y1=int(rand()%500);
  const uint32_t color=rand()&0xffffff;
  // Shaded line with equal endpoint colors must render exactly like flat line.
  std::fill(gpu.vram.begin(),gpu.vram.end(),0);
  gpu.render_line({x0,y0,0,0,color},{x1,y1,0,0,color},false);
  const auto baseline=gpu.vram;
  std::fill(gpu.vram.begin(),gpu.vram.end(),0);
  gpu.gp0(0x40000000|color);gpu.gp0(xy(x0,y0));gpu.gp0(xy(x1,y1));
  check(gpu.vram==baseline,"flat line/GP0 same-color mismatch");
 }
}
void verify_muted_resampler(){
 std::mt19937 rand(0x12345);
 for(double ratio:{0.25,0.5,0.75,1.0,1.25,1.5,2.0}){
  for(unsigned trial=0;trial<30;trial++){
   ps1::AudioResampler resampler;
   std::deque<int16_t> input;
   for(unsigned i=0;i<300;i++){input.push_back(int16_t(rand()));input.push_back(int16_t(rand()));}
   std::array<int16_t,128> muted{};
   muted.fill(77);
   const unsigned before=unsigned(input.size());
   const bool ok=resampler.fill(input,muted.data(),64,ratio,true);
   check(ok,"muted resampler should accept buffered input");
   check(std::all_of(muted.begin(),muted.end(),[](int16_t v){return v==0;}),"mute zero fill");
   const unsigned consumed=unsigned(64*ratio);
   check(input.size()==before-consumed*2,"muted resampler must consume the right frames");
   check(std::abs(resampler.fraction-((64*ratio)-consumed))<1e-9,"muted resampler phase");
  }
 }
}
void verify_cpu_bus(){
 ps1::Bus bus;
 for(uint32_t addr:{0u,0x10000u,0x1ffffcu,0x80012340u,0xa0012340u}){
  bus.write32(addr,0x89abcdef);
  check(bus.read8(addr)==0xef&&bus.read8(addr+1)==0xcd&&bus.read8(addr+2)==0xab&&bus.read8(addr+3)==0x89,"CPU byte lanes");
  check(bus.read32(addr)==0x89abcdef,"CPU 32-bit mirror read");
 }
 bus.write8(0x1f800010,0x52);
 check(bus.read8(0xbf800010)==0x52,"CPU scratchpad mirror");
}
}
int main(){try{
 verify_cpu_bus();verify_fill();verify_flat_lines();verify_muted_resampler();
 std::cout<<"PASS: CPU memory mirrors, 160 reference GPU fill cases, 240 flat lines, 210 muted audio cases\n";
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}

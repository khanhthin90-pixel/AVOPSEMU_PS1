#include "gpu.hpp"
#include <chrono>
#include <iostream>
#include <memory>
int main(){auto p=std::make_unique<ps1::Gpu>();auto& g=*p;g.right=319;g.bottom=239;g.draw_mode=0x100;g.vram[256*1024+640]=0x7fff;
for(unsigned mode=0;mode<3;mode++){auto start=std::chrono::steady_clock::now();
for(unsigned n=0;n<500;n++){const uint32_t color=(n*3571u)&0xffffff;ps1::Gpu::Vertex a{0,0,0,0,color},b{319,0,0,0,mode==1?color^0xffffff:color},c{0,239,0,0,color};g.render_triangle(a,b,c,0,mode==2,false,mode==2,false);}
uint64_t sum=1469598103934665603ull;for(auto x:g.vram){sum^=x;sum*=1099511628211ull;}
std::cout<<mode<<" "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<" ms hash="<<sum<<"\n";}}

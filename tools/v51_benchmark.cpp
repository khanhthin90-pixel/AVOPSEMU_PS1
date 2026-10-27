#include "core.hpp"
#include "audio_stretcher.hpp"
#include <chrono>
#include <ctime>
#include <iostream>
#include <memory>
#include <cmath>
struct Clock { static std::chrono::duration<double> now(){return std::chrono::duration<double>(double(std::clock())/CLOCKS_PER_SEC);} };
int main(){auto b=std::make_unique<ps1::Bus>();ps1::Cpu c(*b);c.reset(0x1000);const uint32_t code[]={0x24210001,0x08000400,0};for(unsigned i=0;i<3;i++)b->write32(0x1000+i*4,code[i]);auto t=Clock::now();for(unsigned i=0;i<3000000;i++)c.step();std::cout<<"cpu="<<std::chrono::duration<double>(Clock::now()-t).count()<<" checksum="<<c.r[1]<<":"<<c.pc<<":"<<b->gpu.video_frames<<"\n";
 auto g=std::make_unique<ps1::Gpu>();g->left=g->top=0;g->right=1023;g->bottom=511;g->draw_mode=0x10a;for(unsigned i=0;i<g->vram.size();i++)g->vram[i]=uint16_t((i*71)^(i>>3));t=Clock::now();for(unsigned n=0;n<2000;n++){g->gp0(0x65000000);g->gp0((n%200)|((n%100)<<16));g->gp0(0);g->gp0(128|(128<<16));}uint64_t hash=1469598103934665603ULL;for(auto p:g->vram){hash^=p;hash*=1099511628211ULL;}std::cout<<"gpu="<<std::chrono::duration<double>(Clock::now()-t).count()<<" hash="<<hash<<"\n";
 ps1::AudioStretcher a;std::deque<int16_t> q;int16_t pcm[1024];hash=1469598103934665603ULL;t=Clock::now();for(unsigned n=0;n<1000;n++){while(q.size()<6000){auto i=q.size()+n*257;q.push_back(int16_t(std::sin(i*.09)*14000));}if(!a.fill(q,pcm,512,.54))return 1;for(auto p:pcm){hash^=uint16_t(p);hash*=1099511628211ULL;}}std::cout<<"audio="<<std::chrono::duration<double>(Clock::now()-t).count()<<" hash="<<hash<<"\n";
}

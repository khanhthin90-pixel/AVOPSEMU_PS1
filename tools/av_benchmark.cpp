#include "gpu.hpp"
#include "audio_stretcher.hpp"
#include <chrono>
#include <iostream>
#include <cmath>
#include <memory>
int main(){using C=std::chrono::steady_clock;auto g=std::make_unique<ps1::Gpu>();g->left=g->top=0;g->right=319;g->bottom=239;auto t=C::now();for(unsigned i=0;i<10000;i++){g->gp0(0x60000000|(i&255));g->gp0(0);g->gp0(320|(240<<16));}std::cout<<"rect_seconds="<<std::chrono::duration<double>(C::now()-t).count()<<" final="<<g->vram[0]<<'\n';
ps1::AudioStretcher s;std::deque<int16_t> q;uint64_t hash=1469598103934665603ull;int16_t out[1024];t=C::now();for(unsigned n=0;n<2000;n++){while(q.size()<6000){auto i=q.size()+n*257;q.push_back(int16_t(std::sin(i*.09)*14000));}if(!s.fill(q,out,512,.54))return 1;for(auto v:out){hash^=uint16_t(v);hash*=1099511628211ull;}}std::cout<<"stretch_seconds="<<std::chrono::duration<double>(C::now()-t).count()<<" hash="<<hash<<'\n';}

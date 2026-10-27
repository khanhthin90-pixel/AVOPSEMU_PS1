#include "core.hpp"
#include "display.hpp"
#include "audio_playback.hpp"
#include <memory>
#include <iostream>
#include <stdexcept>
#include <cmath>
static void ck(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int main(){try{
 auto g=std::make_unique<ps1::Gpu>();
 g->gp1(0x09000001);g->gp1(0);ck(g->allow_texture_y512&&g->display[1]==0xc00200&&g->display[2]==0x40010,"GP1 reset range defaults and option retained");
 g->gp1(0x03000000);ck(!ps1::display_view(*g).disabled,"default display range is valid");
 g->gp1(0x06000000);ck(ps1::display_view(*g).disabled,"explicit zero horizontal range blanks output");
 for(unsigned i=0;i<10000;i++){g->tick_video(2);ck(g->video_hblank&&!g->timer_hblank_rise,"zero horizontal range is endless HBlank without repeated edges");}
 g->gp1(0);g->gp1(0x03000000);g->gp1(0x07000000);ck(ps1::display_view(*g).disabled&&g->video_vblank,"explicit zero vertical range blanks output");
 // Compare accelerated opaque rectangles with the original per-pixel formula,
 // clipping, signed offsets, forced/check masks, and background preservation.
 uint32_t rng=13;auto next=[&]{rng=rng*1664525+1013904223;return rng;};
 for(unsigned trial=0;trial<160;trial++){
  g->reset();g->left=7;g->top=9;g->right=48;g->bottom=45;g->offset_x=-4;g->offset_y=3;g->force_mask=trial&1;g->check_mask=trial&2;
  std::fill(g->vram.begin(),g->vram.end(),0x8001);const uint32_t color=next()&0xffffff;
  const unsigned x=next()%60,y=next()%50,w=next()%25,h=next()%25;
  g->gp0(0x60000000|color);g->gp0(x|(y<<16));g->gp0(w|(h<<16));
  const auto value=uint16_t(((color>>3)&31)|(((color>>11)&31)<<5)|(((color>>19)&31)<<10)|(g->force_mask?0x8000:0));
  for(unsigned py=0;py<64;py++)for(unsigned px=0;px<64;px++){
   const bool inside=int(px)>=int(x)-4&&int(px)<int(x)-4+int(w)&&int(py)>=int(y)+3&&int(py)<int(y)+3+int(h)&&px>=7&&px<=48&&py>=9&&py<=45;
   ck(g->vram[py*1024+px]==(inside&&!g->check_mask?value:0x8001),"rectangle reference pixels");
  }
 }
 ps1::AudioPlayback p;std::deque<int16_t> incoming(6000,12000);p.append(incoming);int16_t output[1024];
 ck(p.render(output,512,false)&&p.last_sample[0]==12000,"steady source before queue recovery");
 incoming.assign(40000,-12000);p.append(incoming);ck(p.dropped>0,"force bounded overflow recovery");
 ck(p.render(output,512,false)&&output[0]>11000,"overflow starts from preceding audible sample, not sudden zero/opposite sign");
 for(unsigned i=1;i<512;i++){ck(std::abs(int(output[i*2])-output[(i-1)*2])<600,"recovery crossfade limits artificial discontinuity");}
 p.restart(true);ck(p.render(output,512,false)&&std::abs(int(output[0]))<100,"restart after device silence fades from zero");
 p.conceal(output,512);ck(output[1022]==0&&output[1023]==0,"underrun concealment ends at silence");
 ps1::AudioPlayback burst;incoming.assign(12000,5000);burst.append(incoming);ck(!burst.dropped&&burst.input.size()==12000,"short producer backlog no longer forces a queue cut");
 std::cout<<"PASS: GPU reset/zero ranges, 160 reference rectangle cases, audio trim crossfade/silence recovery and burst tolerance\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

#include "core.hpp"
#include <iostream>
#include <memory>
#include <random>
#include <algorithm>
#include <stdexcept>
void ck(bool b,const char* why){if(!b)throw std::runtime_error(why);}
int sx11(uint32_t w){int n=w&2047;return n&1024?n-2048:n;}
void reference_rectangle(ps1::Gpu& g){
 unsigned op=g.packet[0]>>24;bool raw=op&1,semi=op&2;int x=sx11(g.packet[1])+g.offset_x,y=sx11(g.packet[1]>>16)+g.offset_y;
 unsigned w=g.packet[3]&1023,h=(g.packet[3]>>16)&511,u=g.packet[2]&255,v=(g.packet[2]>>8)&255;uint16_t clut=uint16_t(g.packet[2]>>16);
 int du=g.draw_mode&0x1000?-1:1,dv=g.draw_mode&0x2000?-1:1;
 for(int py=std::max({0,g.top,y});py<=std::min({511,g.bottom,y+int(h)-1});py++)for(int px=std::max({0,g.left,x});px<=std::min({1023,g.right,x+int(w)-1});px++)g.fragment(px,py,g.packet[0],g.texel(unsigned(int(u)+(px-x)*du),unsigned(int(v)+(py-y)*dv),clut),true,raw,semi,false);
}
int main(){try{
 auto b=std::make_unique<ps1::Bus>();ps1::Cpu c(*b);
 auto start=[&](uint32_t ins){c.reset(0x1000);c.cop0[12]=0;b->write32(0x1000,ins);b->write32(0x1004,12);};
 start(0x08000800);c.step();c.step();ck(c.cop0[6]==0x2000&&(c.cop0[13]&0xc0000000)==0xc0000000&&c.cop0[14]==0x1000,"taken jump exception TAR/BD/BT");
 start(0x10000003);c.step();c.step();ck(c.cop0[6]==0x1010&&(c.cop0[13]&0x40000000),"taken branch TAR");
 start(0x14200003);c.cop0[6]=0x1234;c.step();c.step();ck(c.cop0[6]==0x1234&&!(c.cop0[13]&0x40000000)&&(c.cop0[13]&0x80000000),"untaken branch keeps TAR and clears BT");
 start(0x40823000);c.cop0[6]=0x7788;c.r[2]=0xffffffff;c.step();ck(c.cop0[6]==0x7788,"TAR read-only");
 start(0x40823800);c.r[2]=~0u;c.step();ck(c.cop0[7]==0xff80f03f,"DCIC writable mask");
 start(0);c.cop0[3]=0x1000;c.cop0[11]=~0u;c.cop0[7]=0xa1800000;c.step();ck(c.pc==0x80000040&&((c.cop0[13]>>2)&31)==9&&c.cop0[14]==0x1000&&(c.cop0[7]&3)==3&&!c.instructions,"BPC debug vector before execution");
 start(0);c.cop0[3]=0x1000;c.cop0[11]=~0u;c.cop0[7]=0x21800000;c.step();ck(c.pc==0x1004&&(c.cop0[7]&3)==3,"BPC status-only mode");
 start(0);c.cop0[3]=0x1010;c.cop0[11]=~0u;c.cop0[7]=0xa1800000;c.step();ck(c.pc==0x1004,"BPCM mismatch does not trap");
 start(0);c.cop0[3]=0x1000;c.cop0[11]=~0u;c.cop0[7]=0x81800000;c.step();ck(c.pc==0x1004,"kernel mode gate");
 // Rectangle equivalence includes live VRAM/texture/CLUT feedback, wrapping, masks and blend modes.
 auto a=std::make_unique<ps1::Gpu>(),g=std::make_unique<ps1::Gpu>();std::mt19937 random(0x515151);
 for(unsigned test=0;test<240;test++){
  for(auto& p:a->vram){p=uint16_t(random());}
  a->left=0;a->right=1023;a->top=0;a->bottom=511;
  a->draw_mode=random()&0x3fff;a->texture_window=random()&0xfffff;a->allow_texture_y512=test&16;a->force_mask=test&1;a->check_mask=test&2;a->offset_x=int(test%7)-3;a->offset_y=int(test%5)-2;
  a->packet[0]=0x64000000|(random()&0xffffff)|((test&3)<<24);a->packet[1]=(test%4==0?0:random()%800)|((test%4==0?0:random()%450)<<16);a->packet[2]=random();a->packet[3]=(16+random()%160)|((8+random()%80)<<16);*g=*a;
  a->rectangle_packet();reference_rectangle(*g);ck(a->vram==g->vram,"textured rectangle differs from reference");
 }
 std::cout<<"PASS: PS1 TAR/BT/DCIC/BPC and 240 textured rectangle equivalence cases\n";
 }catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}

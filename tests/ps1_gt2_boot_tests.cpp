#include "core.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
static void ck(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){try{
 auto b=std::make_unique<ps1::Bus>();ps1::Cpu c(*b);
 for(auto alias:{0u,0x80000000u,0xa0000000u}){
  for(unsigned i=0;i<16;i++){
   auto a=(0x1f801130u+i)|alias;b->write8(a,0xff);ck(b->read8(a)==0,"unused byte read/write");
   if(!(i&1)){b->write16(a,0xffff);ck(b->read16(a)==0,"unused halfword read/write");}
   if(!(i&3)){b->write32(a,0xffffffff);ck(b->read32(a)==0,"unused word read/write");}
  }
 }
 b->timers[2].count=0x4567;b->timers[2].target=0xabcd;
 b->timers[2].mode=0x1c00;
 ck(b->read16(0x1f801130)==0&&b->timers[2].mode==0x1c00,"unused read cannot clear timer flags");
 ck(b->read16(0x1f801120)==0x4567&&b->read16(0x1f801128)==0xabcd,"real timer registers preserved");
 for(auto a:{0x1f801140u,0x1f801828u}){
  bool failed=false;try{b->read16(a);}catch(const std::runtime_error&){failed=true;}ck(failed,"neighbor remains unsupported");
 }
 bool failed=false;try{b->read16(0x1f801131);}catch(const std::runtime_error&){failed=true;}ck(failed,"alignment still enforced");
 // Exact final 21 instruction attempts from the uploaded GT2 log, then an authored NOP
 // to observe the MIPS-I load delay. This is not a full game replay.
 const uint32_t code[]={0x3c041f80,0x34841120,0x3c031f80,0x34631100,0x3c051f80,0x34a51110,0x3c061f80,0x34c61130,0x3402ffff,0xa4800004,0xa4820008,0x24020248,0xa4820004,0x94630000,0x94a20000,0x3c05801d,0x94840000,0x00021100,0x00621826,0x00042200,0x94c20000,0};
 c.reset(0x800108c0);b->timers[0].count=0x12f3;b->timers[1].count=0x9b0;b->timers[1].mode=0x100; // HBlank clock: no edge in this short trace.
 for(unsigned i=0;i<22;i++){b->write32(0x800108c0+4*i,code[i]);}
 for(unsigned i=0;i<21;i++){c.step();}
 ck(c.pc==0x80010914&&c.pending_reg==2&&c.pending_value==0&&c.r[2]==0x9b00,"reported GT2 LHU completes with load delay intact");
 c.step();ck(c.r[2]==0&&c.pc==0x80010918&&!c.first_fault.valid,"GT2 continuation commits unused read");
 std::cout<<"PASS: GT2 timer-tail MMIO aliases/widths/guards, timer isolation and recorded 21-instruction tail\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

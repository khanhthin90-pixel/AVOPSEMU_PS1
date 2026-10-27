#include "core.hpp"
#include <memory>
#include <iostream>
#include <stdexcept>
static void ck(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int main(int argc,char** argv){try{
 ck(argc==2,"authored CUE required");auto b=std::make_unique<ps1::Bus>();b->cd.mount(argv[1]);
 auto w=[&](unsigned p,uint8_t v){b->write8(0x1f801800+p,v);};
 auto ack=[&]{w(0,1);w(3,7);w(0,0);b->write32(0x1f801070,~4u);};
 auto wait=[&]{for(unsigned i=0;i<1000000;i++){b->tick_timers();if(b->cd.flag)return;}throw std::runtime_error("CD raw tail stalled next INT1");};
 auto cmd=[&](uint8_t op,std::initializer_list<uint8_t> p){for(auto x:p)w(2,x);w(1,op);wait();ck(b->cd.flag==3,"INT3 command response");ack();};
 cmd(0xe,{0xa0});cmd(2,{0,2,0x16});cmd(6,{});wait();ck(b->cd.flag==1,"first INT1");
 w(3,0);w(3,0x80);b->dpcr=0x8000;
 auto dma=[&](uint32_t address,unsigned words){b->write32(0x1f8010b0,address);b->write32(0x1f8010b4,words);b->write32(0x1f8010b8,0x11000000);};
 dma(0x10000,3);dma(0x11000,512);ck(b->cd.data.size()==280,"exact GT2 pattern leaves 280 raw tail bytes");
 const auto tail=b->cd.data;const auto first=b->cd.last_lba;ack();wait();
 ck(b->cd.flag==1&&b->cd.last_lba==first+1&&b->cd.data==tail&&b->cd.data_lba==first,"next INT1 arrives without overwriting locked old FIFO");
 const auto read=b->cd.sectors_read;for(unsigned i=0;i<300000;i++){b->tick_timers();}ck(b->cd.sectors_read==read,"pending INT1 is not overwritten");
 w(3,0);w(3,0x80);ck(b->cd.data.size()==2340&&b->cd.data_lba==first+1,"BFRD accepts new sector after disc advances");
 auto raw=b->cd.disc.raw(first+1);dma(0x12000,3);dma(0x13000,512);
 for(unsigned i=0;i<2048;i++){ck(b->ram[0x13000+i]==raw[24+i],"second payload has correct bytes, no repeated sector");}
 ck(b->cd_dma_history[2].lba==first+1&&b->cd_dma_history[3].lba==first+1,"DMA trace reports accepted FIFO sector");
 // An authored interrupt callback clears the same byte naturally. No emulator
 // special-case clears RAM or skips the recorded GT2 loop.
 ack();ps1::Cpu c(*b);c.reset(0x8007c468);c.cop0[12]=0x401;b->irq_mask=4;
 c.r[4]=0x801eff00;c.r[8]=0x1f801800;c.r[9]=0x801f0066;c.r[10]=1;c.r[11]=7;c.r[12]=0x1f801070;
 b->write8(0x801f0066,1);
 const uint32_t loop[]={0x90820166,0,0x1440fffd,0,0x24030055};for(unsigned i=0;i<5;i++){b->write32(0x8007c468+i*4,loop[i]);}
 const uint32_t handler[]={0xa10a0000,0xa10b0003,0xad800000,0xa1200000,0x401a7000,0,0x03400008,0x42000010};
 for(unsigned i=0;i<8;i++){b->write32(0x80000080+i*4,handler[i]);}
 for(unsigned i=0;i<400000&&c.r[3]!=0x55;i++){c.step();}
 ck(c.r[3]==0x55&&c.interrupts==1&&b->read8(0x801f0066)==0,"real CD IRQ + authored callback releases recorded wait loop");
 const auto& last=b->watch_writes[(b->watch_total-1)%16];ck(last.cpu&&last.dma==-1&&last.pc==0x8000008c&&last.before==1&&last.after==0,"write-watch identifies actual callback instruction");
 std::cout<<"PASS: raw 12+2048-byte DMA leaves280, next INT1/FIFO isolation/LBA and CD interrupt callback releases exact wait loop\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

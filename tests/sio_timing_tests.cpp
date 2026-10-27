#include "core.hpp"
#include "keyboard_pad.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
void ck(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(){try{
 auto b=std::make_unique<ps1::Bus>();auto& p=b->sio[0];p.connected=true;p.buttons=0xbedf;
 auto ticks=[&](unsigned n){while(n--)b->tick_timers();};
 auto tx=[&](uint8_t v){b->write8(0x1f801040,v);return b->read8(0x1f801040);};
 b->write16(0x1f801048,0xd);b->write16(0x1f80104e,0x88);b->write16(0x1f80104a,0x1003);
 ck(tx(1)==255&&!p.ack&&!p.irq,"ACK must not occur in the data write");
 ticks(100);b->write32(0x1f801070,~128u);ck(!(b->irq_status&128),"BIOS clears old I_STAT before new ACK");
 ticks(1137);ck(!p.irq,"ACK not due before baud*8+150");ticks(1);ck(p.ack&&p.irq&&(b->irq_status&128),"delayed ACK raises IRQ7");
 b->write16(0x1f80104a,0x1013);ck(p.irq,"cannot acknowledge while DSR is asserted");
 ticks(100);ck(!p.ack,"ACK pulse releases without another TX");b->write16(0x1f80104a,0x1013);ticks(1);ck(!p.irq,"ACK IRQ can clear after pulse");b->write32(0x1f801070,~128u);
 const uint8_t send[]={0x42,0,0,0};const uint8_t want[]={0x41,0x5a,0xdf,0xbe};
 for(unsigned i=0;i<4;i++){
  ck(tx(send[i])==want[i],"digital active-low response");if(i==0)p.buttons=0xffff;
  ticks(1338);ck(bool(p.irq)==(i!=3),"only non-final bytes generate ACK IRQ");
  b->write16(0x1f80104a,0x1013);ticks(1);b->write32(0x1f801070,~128u);
 }
 ck(p.polls==1&&p.ack_events==4,"poll and ACK counters");ck(tx(1)==255&&p.phase==5&&!p.ack_delay,"finished pad waits for CS release");
 b->write16(0x1f80104a,1);b->write16(0x1f80104a,0x1003);tx(1);ck(p.ack_delay>0,"new CS restarts transaction");b->write16(0x1f80104a,1);ticks(2000);ck(!p.irq&&!p.ack_delay,"deselect cancels pending ACK");
 b->write16(0x1f80104a,0x1003);tx(1);b->write16(0x1f80104a,0x3003);ck(p.phase==0&&!p.ack_delay,"switching ports cancels old transaction");ck(tx(1)==255&&!p.ack_delay,"absent second controller has no ACK");
 b->write16(0x1f80104a,0x40);ck(!p.irq&&!p.ack&&!p.ack_width&&!p.ack_delay,"SIO reset cancels all ACK state");
 ps1::KeyboardPad k;k.preset(true);k.event('W',true);k.event('J',true);ck(k.buttons()==uint16_t(0xffff^0x4010),"WASD profile combo");
 k.event('S',true);k.neutral_opposites=true;ck((k.buttons()&0x50)==0x50,"opposite vertical input neutralization");k.event('A',true);k.event('D',true);ck((k.buttons()&0xa0)==0xa0,"opposite horizontal input neutralization");
 ck(!k.bind(14,'W')&&!k.bind(1,'B')&&!k.bind(14,256),"invalid and duplicate mapping rejected");ck(k.bind(14,'B')&&k.buttons()==0xffff,"remap clears held state");k.event('B',true);ck(!(k.buttons()&0x4000),"custom Cross binding");k.preset(false);ck(k.keys==k.defaults&&k.buttons()==0xffff,"default restore releases all buttons");
 std::cout<<"PASS: BIOS ACK spacing, pulse/IRQ ack ordering, digital poll bytes, final byte, CS/slot/reset cancellation, keyboard presets/remapping/SOCD\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

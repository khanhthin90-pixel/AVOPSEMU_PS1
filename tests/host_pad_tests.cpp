#include "core.hpp"
#include "keyboard_pad.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
static void ck(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int main(){try{
 ps1::HostPad host;ps1::KeyboardPad keys;
 auto send=[&]{host.publish(keys.buttons());};
 keys.event('Z',true);send();keys.event('Z',false);send();
 ck(host.held_buttons()==0xffff&&host.pending_presses()==0x4000,"tap survives release before guest poll");
 auto b=std::make_unique<ps1::Bus>();auto& p=b->sio[0];p.connected=true;p.host_input=&host;
 // An absent slot and memory-card probe must not consume a pending pad press.
 b->write16(0x1f80104a,0x3003);b->write8(0x1f801040,1);
 b->write16(0x1f80104a,1);b->write16(0x1f80104a,3);b->write8(0x1f801040,0x81);
 ck(host.pending_presses()==0x4000,"non-pad probes do not consume keyboard input");
 b->write16(0x1f80104a,0x40); // Flush probe replies before a new transaction.
 // Actual MIPS-I MMIO instructions: select pad, issue 01/42/00/00/00,
 // read the button high byte, observe the load delay, store guest result.
 ps1::Cpu c(*b);c.reset(0x1000);c.r[1]=0x1f801040;c.r[2]=3;c.r[3]=1;c.r[4]=0x42;
 const uint32_t code[]={0xa422000a,0xa0230000,0xa0240000,0xa0200000,0xa0200000,0xa0200000,0x8c260000,0,0x90250000,0,0xac050200};
 for(unsigned i=0;i<11;i++){b->write32(0x1000+i*4,code[i]);}
 for(unsigned i=0;i<11;i++){c.step();}
 ck(b->read32(0x200)==0xbf&&p.last_pressed_buttons==0xbfff&&p.completed_polls==1,"quick keyboard Cross reaches guest CPU through SIO bytes");
 ck(host.pending_presses()==0&&host.consume_poll()==0xffff,"tap consumed once and released next poll");
 // Held buttons remain held through multiple polls; key repeat does not queue extras.
 keys.event(0x0d,true);send();ck(host.consume_poll()==0xfff7,"Start press");send();
 ck(host.pending_presses()==0&&host.consume_poll()==0xfff7,"held Start persists without repeat edges");
 keys.event(0x0d,false);send();ck(host.consume_poll()==0xffff,"Start releases after keyup");
 // Stable per-transfer snapshot, even if focus is lost between response bytes.
 keys.event('Z',true);keys.event(0x26,true);send();
 b->write16(0x1f80104a,1);b->write16(0x1f80104a,3);
 auto tx=[&](uint8_t v){b->write8(0x1f801040,v);return b->read8(0x1f801040);};
 ck(tx(1)==255&&tx(0x42)==0x41,"digital header");host.clear();keys.release();
 ck(tx(0)==0x5a&&tx(0)==0xef&&tx(0)==0xbf,"in-flight packet remains consistent after focus loss");
 ck(host.consume_poll()==0xffff&&!host.pending_presses(),"focus clear cancels future held and pending input");
 // Concurrent publisher/consumer must leave the final release intact, with
 // at most one coalesced pending edge (never a torn 16-bit button packet).
 std::thread writer([&]{for(unsigned i=0;i<20000;i++){host.publish(0xbfff);host.publish(0xffff);}});
 for(unsigned i=0;i<20000;i++){auto v=host.consume_poll();ck(v==0xffff||v==0xbfff,"atomic input snapshot");}
 writer.join();ck(host.held_buttons()==0xffff,"final concurrent keyup preserved");host.clear();
 p.host_input=nullptr;p.buttons=0xfff7;
 b->write16(0x1f80104a,1);b->write16(0x1f80104a,3);tx(1);tx(0x42);tx(0);
 ck(tx(0)==0xf7&&tx(0)==0xff,"legacy direct button injection remains available");
 std::cout<<"PASS: short taps/held/release, guest CPU SIO polling, absent probes, focus/packet snapshot and atomic UI handoff\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

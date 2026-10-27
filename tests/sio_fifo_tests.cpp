#include "core.hpp"
#include "keyboard_pad.hpp"
#include <memory>
#include <iostream>
#include <stdexcept>
static void ck(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(){try{auto b=std::make_unique<ps1::Bus>();auto& p=b->sio[0];p.connected=true;ps1::HostPad host;p.host_input=&host;ps1::KeyboardPad keyboard;
 keyboard.event('Z',true);host.publish(keyboard.buttons());
 auto control=[&](uint16_t v){b->write16(0x1f80104a,v);};auto tx=[&](uint8_t v){b->write8(0x1f801040,v);};
 control(0x40);control(0x903); // RX IRQ when two bytes are available, not on the first byte.
 tx(1);ck(p.rx_count==1&&!p.irq,"RX interrupt waits for selected threshold");tx(0x42);
 ck(p.rx_count==2&&p.irq&&p.polls==1,"second RX byte satisfies two-byte threshold");
 ck(b->read16(0x1f801040)==0x41ff&&p.rx_count==1,"halfword snapshots two FIFO bytes and pops one");
 ck(b->read8(0x1f801040)==0x41&&!p.rx_count&&!p.rx,"next byte read sees previewed ID");
 tx(0);tx(0);tx(0);ck(p.completed_polls==1&&p.last_pressed_buttons==0xbfff,"Z/Cross reaches full digital reply");
 ck((b->read32(0x1f801040)&0xffffff)==0xbfff5a&&p.rx_count==0,"word consumes up to four FIFO entries");
 // Authored MIPS poll batches TX writes, then reads JOY_DATA with LHU and LBU.
 // This is a regression pattern, not GTA2 executable code.
 control(0x40);control(3);ps1::Cpu c(*b);c.reset(0x1000);c.r[1]=0x1f801040;c.r[2]=1;c.r[3]=0x42;
 const uint32_t code[]={0xa0220000,0xa0230000,0x94240000,0,0x90250000,0};
 for(unsigned i=0;i<6;i++){b->write32(0x1000+4*i,code[i]);}for(unsigned i=0;i<6;i++){c.step();}
 ck(c.r[4]==0x41ff&&c.r[5]==0x41&&!p.rx_count,"actual CPU LHU preview and LBU pop preserve controller ID");
 bool seen=false;for(const auto& h:p.history)if(h.event==std::string("read-data")&&h.pc==0x1008)seen=true;ck(seen,"protocol trace includes read PC");
 control(0);ck(p.abort_phases[2]>0,"early deselect identifies abort after accepted command");
 // Hardware FIFO overflow overwrites the last slot, preserving oldest bytes.
 ps1::Sio s;for(unsigned i=0;i<9;i++){s.receive(uint8_t(i));}
 ck(s.rx_count==8&&s.rx_overflows==1&&s.read_data(4)==0x03020100&&s.read_data(4)==0x08060504,"eight-byte FIFO and last-entry overwrite");
 // RX IRQ level reasserts after acknowledge if unread threshold is still met.
 control(0x40);control(0x903);tx(1);tx(0x42);b->tick_timers();ck(b->irq_status&128,"RX FIFO interrupt reaches IRQ7");
 b->write32(0x1f801070,~128u);control(0x913);ck(!p.irq,"RX acknowledge initially lowers source");
 b->tick_timers();ck(!p.irq,"source-low observation before reassert");b->tick_timers();ck(p.irq&&(b->irq_status&128),"unread threshold reasserts source and IRQ latch");
 const auto entries=p.history_count;const auto count=p.rx_count;auto report=p.diagnostic();ck(p.history_count==entries&&p.rx_count==count&&report.find("abort phases")!=std::string::npos,"diagnostic never pops RX data");
 control(0x40);ck(!p.rx_count&&!p.rx&&!p.irq&&!p.irq_reassert,"reset clears FIFO and scheduled interrupt");
 control(5);tx(0);ck(!(p.control&4)&&p.rx_count==1,"forced RX enable clears after one byte");
 std::cout<<"PASS: SIO FIFO widths/pop rules, RX IRQ thresholds/reassert, forced RX, overflow, CPU batched poll/Cross reply and abort trace\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

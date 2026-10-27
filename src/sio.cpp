#include "sio.hpp"
#include <sstream>
namespace ps1 {
void Sio::record(const char* event,uint8_t value){
 history[history_count++%history.size()]={event,clocks,context_pc,control,baud,last_tx,value,uint8_t(phase),uint8_t(rx_count)};
}
void Sio::receive(uint8_t value){
 if(rx_count<rx_fifo.size()){rx_fifo[(rx_head+rx_count)%8]=value;rx_count++;}
 else {rx_fifo[(rx_head+7)%8]=value;rx_overflows++;}
 rx=rx_count!=0;control&=~4u; // SIO0 forced RX receives only one byte.
 if(rx_irq_condition())irq=true;
}
uint32_t Sio::read_data(unsigned bytes){
 uint32_t value=0;for(unsigned i=0;i<bytes;i++)value|=uint32_t(i<rx_count?rx_fifo[(rx_head+i)%8]:data)<<(i*8);
 const unsigned remove=bytes==4?4:1;const unsigned count=remove<rx_count?remove:rx_count;
 rx_head=(rx_head+count)%8;rx_count-=count;rx=rx_count!=0;
 record("read-data",uint8_t(value));return value;
}
void Sio::tick(uint64_t clock){
 clocks=clock?clock:clocks+1;
 if(irq_reassert&&!--irq_reassert&&((control&0x400)||rx_irq_condition()||((control&0x1000)&&ack))){irq=true;record("irq-reassert");}
 if(ack_width&&!--ack_width)ack=false;
 if(ack_delay&&!--ack_delay){ack=true;ack_width=100;ack_events++;if(control&0x1000)irq=true;record("ack-pulse");}
}
uint8_t Sio::read(unsigned o){
 if(o==0)return uint8_t(read_data(1));
 if(o<4)return o<rx_count?rx_fifo[(rx_head+o)%8]:data;
 uint32_t status=5|(rx?2:0)|(irq?512:0)|(ack?128:0);
 if(o>=4&&o<8)return uint8_t(status>>((o-4)*8));
 if(o==8||o==9)return uint8_t(mode>>((o-8)*8));
 if(o==10||o==11)return uint8_t(control>>((o-10)*8));
 if(o==14||o==15)return uint8_t(baud>>((o-14)*8));
 return 0;
}
void Sio::write(unsigned o,uint8_t v){
 auto put=[&](uint16_t& r,unsigned lane){r=uint16_t((r&~(255u<<(lane*8)))|(uint16_t(v)<<(lane*8)));};
 if(o==0){
  if(!(control&1))return;
  transfers++;last_tx=v;
  ack=false;ack_width=ack_delay=0;data=255;bool more=false;
  if(connected&&(control&2)&&!(control&0x2000)){
   switch(phase){
    case 0:if(v==1){phase=1;more=true;}else phase=5;break;
    case 1:if(v==0x42){data=0x41;phase=2;more=true;if(host_input){buttons=host_input->held_buttons();latched_buttons=host_input->consume_poll();}else latched_buttons=buttons;polls++;}else phase=5;break;
    case 2:data=0x5a;phase=3;more=true;break;
    case 3:data=uint8_t(latched_buttons);phase=4;more=true;break;
    case 4:data=uint8_t(latched_buttons>>8);phase=5;completed_polls++;if(latched_buttons!=0xffff){pressed_polls++;last_pressed_buttons=latched_buttons;}break;
    default:break; // Wait for /CS release before accepting another address.
   }
  }else phase=0;
  if(more){const unsigned factor=(mode&3)==2?16:(mode&3)==3?64:1;ack_delay=unsigned(baud)*factor*8+150;}
  if(control&6)receive(data);
  if(control&0x400)irq=true;
  record("tx",data);
  return;
 }
 if(o==8||o==9){put(mode,o-8);mode&=0x1ff;record("mode");return;}
 if(o==14||o==15){put(baud,o-14);record("baud");return;}
 if(o==10||o==11){
  const auto old=control;put(control,o-10);
  const bool reset=control&0x40,deselect=!(control&2)||((old^control)&0x2000);
  if(reset||deselect){
   if(phase>=2&&phase<5)abort_phases[phase]++;
   record(reset?"reset":"deselect");phase=0;ack=false;ack_delay=ack_width=0;
  }
  if(reset){mode=control=baud=0;rx=irq=ack=false;ack_delay=ack_width=irq_reassert=0;rx_head=rx_count=0;return;}
  if(control&0x10){
   if(!(control&0x1000)||!ack){irq=false;irq_reassert=((control&0x400)||rx_irq_condition())?2:0;}
  }
  control&=0x3faf;record("control");
 }
}
std::string Sio::diagnostic()const{
 std::ostringstream o;o<<"SIO RX FIFO count="<<rx_count<<" overflows="<<rx_overflows<<" threshold="<<(1u<<((control>>8)&3))<<" abort phases:";
 for(unsigned i=2;i<5;i++)o<<' '<<i<<'='<<abort_phases[i];
 o<<"\nSIO protocol history (last128; PC=triggering I/O context, immediate RX model):\n";
 const auto count=history_count<history.size()?history_count:history.size();
 for(uint64_t i=history_count-count;i<history_count;i++){
  const auto& h=history[i%history.size()];o<<std::dec<<h.clock<<' '<<h.event<<" pc="<<std::hex<<h.pc<<" ctrl="<<h.control<<" baud="<<h.baud<<" tx="<<unsigned(h.tx)<<" rx="<<unsigned(h.rx)<<" phase="<<std::dec<<unsigned(h.phase)<<" fifo="<<unsigned(h.count)<<'\n';
 }
 return o.str();
}
}

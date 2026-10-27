#pragma once
#include <atomic>
#include <cstdint>
namespace ps1 {
// Single atomic snapshot: low16 held (active high), high16 unconsumed press edges.
// UI publishes; emulation thread consumes only when a pad poll is accepted.
// Multiple taps of the same button before a poll coalesce into one press.
class HostPad {
 std::atomic<uint32_t> state{0};
public:
 void publish(uint16_t active_low){
  const auto held=uint32_t(uint16_t(~active_low));auto old=state.load();
  for(;;){const auto edges=held&~(old&0xffffu);
   const auto next=held|((old&0xffff0000u)|(edges<<16));
   if(state.compare_exchange_weak(old,next))return;
  }
 }
 void clear(){state.store(0);}
 uint16_t held_buttons()const{return uint16_t(~state.load());}
 uint16_t pending_presses()const{return uint16_t(state.load()>>16);}
 uint16_t consume_poll(){const auto s=state.fetch_and(0xffffu);return uint16_t(~((s&0xffffu)|(s>>16)));}
};
}

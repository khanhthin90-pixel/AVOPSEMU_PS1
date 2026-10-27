#pragma once
#include <cstdint>
#include <array>
#include <string>
#include "host_pad.hpp"
namespace ps1 {
// Functional digital pad model: immediate RX, scheduled ACK (not a full shifter).
struct Sio {
 // Non-owning: attach only while the emulation worker is stopped.
 HostPad* host_input=nullptr;
 uint16_t mode=0,control=0,baud=0;
 bool rx=false,irq=false,connected=false,ack=false;
 unsigned phase=0;
 std::array<uint8_t,8> rx_fifo{};unsigned rx_head=0,rx_count=0;
 unsigned irq_reassert=0;uint64_t rx_overflows=0,clocks=0;
 uint32_t context_pc=0;
 struct Trace {const char* event="";uint64_t clock=0;uint32_t pc=0;uint16_t control=0,baud=0;uint8_t tx=0,rx=0,phase=0,count=0;};
 std::array<Trace,128> history{};uint64_t history_count=0;
 std::array<uint64_t,6> abort_phases{};
 void record(const char* event,uint8_t value=0);
 void receive(uint8_t value);
 uint32_t read_data(unsigned bytes);
 bool rx_irq_condition()const{return (control&0x800)&&rx_count>=(1u<<((control>>8)&3));}
 std::string diagnostic()const;
 uint16_t buttons=0xffff,latched_buttons=0xffff;
 uint8_t data=255;
 unsigned ack_delay=0,ack_width=0;
 uint64_t polls=0,ack_events=0;
 uint64_t completed_polls=0,pressed_polls=0,transfers=0;
 uint16_t last_pressed_buttons=0xffff;
 uint8_t last_tx=255;
 void tick(uint64_t clock=0);
 uint8_t read(unsigned offset);
 void write(unsigned offset,uint8_t value);
};
}

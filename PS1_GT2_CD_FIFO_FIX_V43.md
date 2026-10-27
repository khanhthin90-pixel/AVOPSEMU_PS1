# v43 — GT2 wait loop: CD raw FIFO and write tracing

## Evidence

The supplied analysis identifies `8007C468: LBU v0,166(a0); NOP;
BNE v0,zero,-3; NOP`, with a0=801EFF00. This reads RAM byte 801F0066.
The newer v42 log ps1_bios_log(20261027-044001).txt confirms value 1,
260 CD commands, mode A0, raw sector_buffer=2340, data_fifo=280,
reading status22, no pending response/error, sector_delay=0, next LBA473.
It also has Setloc472 staged without a new Read: this must not itself seek.

DMA history shows two operations for a sector: 3 words (12 bytes), then
512 words (2048 bytes), leaving 2340-12-2048=280 bytes. The old CD tick
required data.empty() before receiving the next sector. Therefore a host
that intentionally leaves raw ECC tail bytes would never get the next
INT1: a reproducible emulator defect matching this logged state.
This does not establish that it is the only remaining GT2 problem.

## Fix

CD reception no longer requires the *host* FIFO to be empty. Its next
sector remains separate in sector_buffer while the previous FIFO keeps
its bytes intact. INT1 announces the next sector after the existing IRQ,
response and cadence gates. BFRD=0/1 discards the old tail and accepts the
new sector. A pending sector/INT1 is not overwritten by another delivery.

FIFO data now has its own accepted LBA for DMA diagnostics, so a transfer
from the old FIFO is not mislabeled with the newer drive head position.
CD_FIFO logs accepted LBA, FIFO bytes, next LBA and the count of sectors
received with an older FIFO still present. No new DMA-completion rules,
IRQ priorities, hard-coded GT2 RAM clears or forced GPU display changes.

This remains a bounded functional buffer model. It does not implement all
HC05 SRAM slots, overrun mechanics or cycle-accurate drive behavior.

## Write-watch and image evidence

Always-on narrow watch at physical 001F0066 (all current RAM aliases):
last 16 writes, old/new byte, address/width, CPU instruction/PC/opcode,
clock, and CPU/DMA/host source. DMA trigger_pc names the CPU instruction
active when DMA was serviced; it is not a supposed DMA instruction.
Host-initiated operations have PC/opcode0. Direct host memory copies used
by loaders are not instruction-level watch events. Watch history survives
CPU-only reset with retained RAM, but fresh Bus/boot clears it.

Export diagnostic log pauses the worker and writes sidecars next to it:
- .vram.bin: raw little-endian 1024x512 16-bit VRAM.
- .vram.bmp: full VRAM as 15-bit RGB, useful for texture/palette inspection.
- .frame.bmp: currently selected display region, including 24-bit decoding
  or black output when the display/range is disabled.

The log and both BMPs let us distinguish a stopped draw stream, wrong
framebuffer choice and incorrect texture/VRAM content. No image is invented
when the real framebuffer is black. Snapshots do not alter the machine.
If a filesystem error occurs, the UI reports that snapshots were incomplete.

## Validation

An original CUE fixture reproduces raw modeA0 and the exact 12+2048 DMA
pattern. Against the previous v42 CD source it fails with
`CD raw tail stalled next INT1`. With v43 it receives the next INT1,
keeps the old tail unchanged, accepts the next sector on BFRD and validates
all 2048 payload bytes and their DMA LBA labels.

A separate *authored* MIPS interrupt handler clears the watched byte on a
real CD IRQ. The exact reported four-instruction wait loop then exits;
the watch identifies SB at 8000008C, value1->0. This is not GT2's callback
and is not a full BIOS/game replay. The emulator never clears the busy
byte on its own; only the guest handler in this test does so.

## Try on Windows

Build Release x64, run pslab.exe v43, reset/reboot the GT2 CUE from BIOS,
then Play. Reboot matters: a prior stopped machine cannot reconstruct
missing earlier callbacks from an already wedged state.

If it still goes black after copyright, Export log and send:
the new text log plus its .frame.bmp and .vram.bmp sidecars. Inspect
WRITE_WATCH and CD_FIFO; do not manually zero 801F0066.
Actual GT2 menu/racing and native Win32 capture remain unverified here.
No Sony BIOS or GT2 image is available in this test environment.
PS1 keyboard/audio/GPU and PS2 changes from v40-v42 are retained.

## Primary reference

https://psx-spx.consoledev.net/cdromdrive/
INT1/data request/IRQ acknowledgement ordering; data accepted into the
host buffer remains intact even when later drive sectors arrive. This
supports separating the host-facing FIFO from drive reception. The local
model is simpler than the full physical buffering described there.

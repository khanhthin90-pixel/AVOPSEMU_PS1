# v44 — PS1 SIO RX FIFO / GTA2 controller diagnosis

## What the screenshot establishes

The v43 GTA2 menu screenshot shows last_host=BFFF (Cross), 1996 host key
events and 1880 started SIO polls, but completed/pressed polls both zero.
Windows input reached the host mapping; no full digital pad reply was
completed in that snapshot. This differs from the confirmed GT2 menu/replay
screenshots with complete and pressed replies. The screenshot alone does
not show the GTA2 driver's access widths, control writes or abort reason.

## Verified emulator defects fixed

The old SIO held only the most recent reply byte. Sending another byte before
reading DATA replaced the previous reply. A batched 01h/42h exchange thus
lost FFh and returned only 41h. The new RX FIFO holds eight bytes; overflow
replaces the last slot while retaining the oldest unread entries.

DATA reads now snapshot the FIFO before consumption: byte reads remove one,
halfword reads preview two but remove one, word reads remove four (up to
what is available). Bus helpers dispatch wide DATA reads as one operation,
rather than assembling bytes while changing the FIFO between lanes.
Unused/empty preview lanes use the last reply as a functional approximation;
the physical replicated empty FIFO behavior is not fully emulated.

RX interrupt thresholds follow CTRL bits8..9: 1/2/4/8 received bytes. The old
implementation asserted RX IRQ on every received byte irrespective of the
threshold. ACK can lower RX/TX IRQ, which reasserts after a short modeled
source-low gap if its enabled condition is still present. DSR only prevents
ACK clearing while DSR IRQ is enabled and the ACK line is active. SIO0 forced
RX bit2 clears after one received byte. Reset clears FIFO and reassert state.

ACK byte timing is retained from v43 (baud*factor*8+150, pulse100 clocks).
RX is still immediate; TX-ready/busy and full serial shifter timing remain
functional approximations. No analog pad, memory card or SIO1 UART support
is claimed. No GTA2-specific RAM patch or game-specific key remapping.

## New trace

Export log includes the last128 SIO events: transmitted byte, received low
byte, control, baud, phase, FIFO count, clock and last port-I/O PC context.
Events include TX, DATA read, control/reset/deselect, ACK and IRQ reassert.
Abort counters identify deselection/reset after phases2,3,4 (after command
42h accepted but before both button bytes were generated). Counters retain
history across SIO reset; the controller/Bus fresh boot resets them.
Snapshots do not read/pop the FIFO. PC on a scheduled event is the most
recent I/O context, not a claimed exact CPU instruction causing the event.

## Validation and limits

New tests cover batched digital polling, Cross=BFFF, actual CPU LHU/LBU
preview/pop, word reads, FIFO overflow, IRQ thresholds/reassert, forced RX,
reset and abort counters. Existing host mailbox test now drains buffered
probe replies and explicitly reads the first four reply bytes before the
high button byte: it no longer depends on the old last-byte overwrite bug.

This tests real emulator defects and a plausible reason for early GTA2
poll aborts. It does NOT reproduce GTA2's actual driver or prove that the
menu will now accept Cross on the user's machine. No GTA2 or Sony BIOS
image is available here; no MSVC/Windows SDK/runtime for native testing.
GT2 controller compatibility needs a real-game recheck after this shared
SIO change; existing protocol/input/CD/GPU/PS2 regression suites are used.

## Try

Extract/build the complete v44 source as Release x64. Run the new pslab.exe,
load BIOS/GTA2 CUE, reboot and Play. Click the game and press Z. Do not only
resume a previously running v43 process. Mapping remains Z=Cross.

If it still does not accept: press Z, wait a few seconds, then Export log
and send the v44 text log. It now contains the missing TX/RX/control history
and abort phase, so diagnosis can target the actual GTA2 exchange instead
of guessing from the Windows key mask. VRAM/framebuffer sidecars remain
available from v43; they are optional for this controller issue.

## Primary source

https://psx-spx.consoledev.net/serialinterfacessio/
DATA FIFO preview/pop widths, eight-byte storage/last-entry overflow,
CTRL RX interrupt thresholds and forced receive, IRQ acknowledgement.
Implementation written locally; no external emulator source copied.

# v40 — buffered PS1 keyboard input

Previous input path overwrote the current SIO button mask on every key event.
A down/up occurring between two emulated 42h pad polls disappeared entirely.
At low emulation speed a normal human tap could therefore be invisible to the
game. UI input writes also waited for the machine mutex held by the CPU worker.
These are code-level problems; without a new failing log they are not proof
of the sole cause of the user's keyboard failure.

## Changes

- HostPad atomically stores held buttons plus pending press edges, separately
  from the machine mutex. The UI does not write to a running SIO device.
- An accepted port1 digital pad command 42h consumes pending presses once and
  combines them with currently held buttons. A short released tap reaches one
  poll; subsequent polls see release. Held directions stay held.
- Multiple taps of one button before a poll coalesce into one press. This is a
  host input buffering policy, not a change to the real PS1 protocol.
- Absent port2 and memory-card probes do not consume pending pad presses.
- Focus loss, pause/reset/load, profile changes clear pending input. A response
  packet already latched remains consistent through its last byte.
- Separate UI input timer requests 8ms sampling, with key messages retained.
  Actual Windows timer resolution/scheduling varies. No background input.
- Running PS1 attaches the mailbox while the worker is stopped. Its lifetime
  exceeds the worker's. Existing direct SIO button injection remains supported.
- Live footer/export adds held and pending masks, plus existing SIO poll/reply
  counts. FFFF means no buttons; Cross is BFFF, Start is FFF7 (active low).

## Test on Windows

1. Extract this complete source ZIP; build Release x64 in Visual Studio 2022.
2. Run the newly built pslab.exe. Window/PS1 diagnostic must show v40.
3. Select View > Keyboard: Arrows + Z/X/A/S to reset old INI bindings.
4. Load PS1 BIOS and GT2 CUE, Boot disc and Play. Click the game screen.
5. Z=Cross, Enter=Start, arrows=D-pad. WASD profile instead uses J=Cross.
6. The footer should show Host=BFFF while Z is held. A quick tap may show
   pending=4000 until the next game poll, then last pressed reply=BFFF.
7. View > Test pad: hold Cross for 1 second also uses this buffered path.
8. If controls still fail, export a new diagnostic after trying Z and Enter.
   HOST_INPUT events/held/pending distinguish host reception; completed SIO
   polls and pressed replies distinguish emulated controller communication.
   If the CPU stopped with an error, input cannot resume game execution.

Core tests use actual MIPS MMIO store/load instructions to verify that a
released Cross tap reaches the guest as BF in the high button byte. They also
check repeat/hold/release, focus clearing, packet snapshots, absent probes,
legacy injection and concurrent producer/consumer snapshots.
No MSVC/Windows SDK/runtime here; native keyboard/focus behavior and GT2
menu/racing still require testing on the user's machine. No new SIO timing,
analog controller, CPU/GPU compatibility or PS2 changes in this update.

Protocol reference (digital controller ID, command and active-low buttons):
https://psx-spx.consoledev.net/controllersandmemorycards/
New original input-bridge implementation; no external emulator source copied.

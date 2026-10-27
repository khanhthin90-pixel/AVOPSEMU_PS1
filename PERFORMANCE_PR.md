# AVOPSEMU_PS1 v51 – performance-focused pull request

## Summary

This patch reduces redundant work in the PS1 CPU bus, GPU and audio playback paths while preserving the functional semantics of the local v51 reference source.

- **CPU/Bus:** Decode physical addresses once in byte read/write paths. Retain mirrored RAM, BIOS, scratchpad and MMIO behavior.
- **GPU flat lines:** Skip per-pixel Gouraud interpolation when both endpoint colors are identical; preserve the existing per-pixel fragment path and masks.
- **GPU GP0(02) fills:** Fill contiguous VRAM row spans instead of redoing wrap calculations for each pixel; preserve horizontal/vertical wrap and the command's mask-independent behavior.
- **Audio resampler:** Write silence without interpolation when muted; still validate source availability, consume the same frames and maintain fractional timing.
- **Audio queues:** Batch removal of consumed samples from deque containers in resampler and stretcher paths.
- **Regression coverage:** Reference-loop comparison for 160 seeded GP0(02) fills, 240 flat-line comparisons, 210 muted resampler cases and CPU memory mirror checks.

## Validation performed

- Build: GCC C++17, Linux; CMake Debug with `-O1` (no Windows/MSVC build verification).
- `ctest --output-on-failure -j 8`: **51/51 tests passed** on local v51 source, including the new performance regression test.
- `ps1_v51_benchmark`: baseline vs patched output checksums/hashes matched in CPU, GPU and audio runs. These synthetic runs are **not** a guarantee of improved in-game FPS.
- No Sony BIOS, commercial game images, keys or proprietary firmware are included.

## Limitations / before merging

This is based on the **locally supplied AVOPSEMU_PS1 v51 snapshot**, not on a freshly fetched GitHub checkout. Confirm the upstream repository version, review the diff, build and run the same tests after integration, then benchmark games on Windows. The scope is a handful of targeted CPU/GPU/audio optimizations, **not** every subsystem.

## Windows developer validation

Open the PS1 project in Visual Studio 2022 using its documented CMake preset; rebuild the PS1 executable and its tests. Compare before/after behavior using the same BIOS and games that you own. Test audio, BIOS boot, menus, GT2, line and polygon rendering, CD-ROM DMA, and gamepad input. Avoid merging if gameplay regressions appear.

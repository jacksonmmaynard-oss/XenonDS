# Roadmap

## Current baseline: v0.6.1

- Real-hardware ROM boot and dual-screen output
- DeSmuME interpreter
- Controller and touch input
- About 14.8 FPS in the measured Pokémon Black test scene

## Next

1. Profile ARM interpreter dispatch and memory access hotspots.
2. Add a PowerPC-oriented JIT or block translator.
3. Improve multicore scheduling without changing emulation order.
4. Add audio and persistent cartridge saves.
5. Add a ROM browser and configurable screen layouts.
6. Build a repeatable compatibility and performance test suite.

Alternative emulator cores may be evaluated later, but experimental core
swaps are not part of the current public release line.

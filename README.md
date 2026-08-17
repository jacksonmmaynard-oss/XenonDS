# XenonDS

<p align="center">
  <img src="branding/social-preview.png" alt="XenonDS: Nintendo DS emulation for Xbox 360 homebrew" width="100%">
</p>

XenonDS is an experimental Nintendo DS emulator port for Xbox 360 homebrew.
The current build uses the DeSmuME interpreter with a LibXenon frontend and a
second Xbox hardware thread for framebuffer conversion.

> **Current milestone:** v0.6.1 boots a legally dumped Nintendo DS ROM and
> displays both screens on real Xbox 360 hardware. Pokémon Black measured about
> 14.8 FPS in the current test scene. Audio, saves, a ROM browser, and broad
> compatibility testing are not implemented yet.

## What works

- XeLL launch from FAT32 USB storage
- FAT device and `.nds` discovery
- Nintendo DS ROM-header validation
- DeSmuME ARM7/ARM9 interpreter execution
- Dual-screen software framebuffer output
- Xbox 360 controller and right-stick touch mapping
- On-console performance profiling
- Framebuffer conversion on a second physical Xbox core
- Host-side parser, layout, and controller tests

## USB layout

Download the v0.6.1 runtime package and preserve this layout:

```text
xenon.elf
XenonDS/
  xenonds-core.elf32
  game.nds
```

`game.nds` is not included. Use a ROM dumped from a cartridge you own.

Boot XeLL, press **A** at the loader prompt, photograph the profile if you are
testing performance, then press **A** again to enter the game.

## Build host tools

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Build the Xbox target

Docker Desktop is required on Windows:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\build_xenon_desmume.ps1
```

The USB-ready result is written to `platform/xenon/release`.

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Xbox testing](docs/XBOX_TESTING.md)
- [Roadmap](docs/ROADMAP.md)
- [Contributing](CONTRIBUTING.md)

## Legal

XenonDS does not include ROMs, Nintendo BIOS or firmware files, encryption
keys, Microsoft SDK files, console keys, or NAND data. DeSmuME integration is
GPL-2.0 licensed. Derived binary releases must provide corresponding source.

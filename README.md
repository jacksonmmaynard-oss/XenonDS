# XenonDS

<p align="center">
  <img src="branding/social-preview.png" alt="XenonDS: Nintendo DS emulation for Xbox 360 homebrew" width="100%">
</p>

XenonDS is an open-source Nintendo DS emulator port for Xbox 360 homebrew.
It combines a portable frontend with LibXenon platform support and the
DeSmuME emulation core.

> **Project status:** Experimental. The v0.4 performance checkpoint boots a
> commercial DS game through the DeSmuME interpreter and renders playable
> frames on real Xbox 360 hardware. Performance, audio, and compatibility are
> still early and it is not yet a general release.

## Implemented

- Nintendo DS header parsing and CRC16 validation
- ARM7 and ARM9 executable-range validation
- Backend-neutral emulator lifecycle
- Dual-screen video, stereo audio, controller, and touch interfaces
- Vertical, horizontal, and single-screen layouts with integer scaling
- Xbox-position controller mapping and right-stick touch input
- BGR555-to-XRGB8888 conversion and nearest-neighbor composition
- Host-side tests and ROM information utility
- Initial DeSmuME adapter and LibXenon hardware probe
- On-console FAT device discovery and Nintendo DS ROM-header validation
- Statically linked DeSmuME interpreter checkpoint for LibXenon
- Direct software-framebuffer output for the two Nintendo DS screens
- On-console emulation and video-stage profiler

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Inspect a legally dumped Nintendo DS image:

```bash
./build/xenonds-rominfo /path/to/game.nds
```

Generate a synthetic dual-screen compositor image:

```bash
./build/xenonds-layout-demo xenonds-layout-demo.ppm
```

![Synthetic XenonDS compositor output](docs/images/layout-demo.png)

## Xbox 360 hardware probe

The LibXenon runtime probe in `platform/xenon` initializes video, USB,
controller, ATA, and FAT services, then validates the first `.nds` header it
finds in `XenonDS/`, `xenonds/`, or the root of a mounted FAT device. A
Docker-based Windows build script is included:

```powershell
./scripts/build_xenon_probe.ps1
```

Running the resulting `.elf32` file requires a homebrew-capable Xbox 360 and
XeLL. Copy only a legally dumped `.nds` image to the USB drive; this milestone
inspects metadata and does not execute the game yet.

## Experimental DeSmuME performance checkpoint

The v0.4.1 build uses a small staged loader plus the pinned DeSmuME interpreter
core. The loader shows file-read progress and a moving memory-preparation bar,
then the core loads the first legal `.nds` image found on FAT storage and runs
it on the Xbox 360. Before gameplay it reports separate input, ARM interpreter,
frame-copy, and Xbox-video timings measured on the console.

```powershell
./scripts/build_xenon_desmume.ps1
```

The USB-ready output is `platform/xenon/release/`: `xenon.elf` belongs in the
drive root and `XenonDS/xenonds-core.elf32` stays inside the included folder.
This checkpoint uses the scalar interpreter and has no audio, save persistence,
or ROM browser yet. It keeps the proven v0.3.7 framebuffer path while measuring
where each frame spends its time. See [Xbox 360 runtime testing](docs/XBOX_TESTING.md) for the
complete layout and expected on-screen result.

## Upstream dependencies

```bash
./scripts/fetch_upstreams.sh
```

The script checks out the DeSmuME and LibXenon revisions recorded in
`upstream.lock`. Upstream source is not vendored in this repository.

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Roadmap](docs/ROADMAP.md)
- [Xbox 360 runtime testing](docs/XBOX_TESTING.md)
- [Contributing](CONTRIBUTING.md)
- [Brand assets](branding/README.md)

## Legal

XenonDS does not include games, Nintendo BIOS or firmware files, encryption
keys, Microsoft SDK files, or proprietary Xbox software. Use ROM images and
firmware dumped from hardware and games you own. DeSmuME integration makes
XenonDS GPL-2.0 licensed, so derived releases must provide corresponding
source code.

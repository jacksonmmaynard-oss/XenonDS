# XenonDS

<p align="center">
  <img src="branding/social-preview.png" alt="XenonDS: Nintendo DS emulation for Xbox 360 homebrew" width="100%">
</p>

XenonDS is an open-source Nintendo DS emulator port for Xbox 360 homebrew.
It combines a portable frontend with LibXenon platform support and Nintendo DS
emulation cores adapted for the Xbox 360.

> **Project status:** Experimental. XenonDS now boots a retail Nintendo DS game
> on real Xbox 360 hardware with animated dual-screen video, controller input,
> touch input, and persistent cartridge saves. The v0.9.1 release candidate
> fixes the white-screen/0-FPS timing regression, adds validated frame-boundary
> Turbo rendering, separates emulation and video-rate reporting, keeps live
> display controls, and improves Xenon scheduling, presentation, and multicore
> software 3D.
> Audio, a ROM browser, and broad compatibility testing are not included yet.

## Hardware demo

https://github.com/user-attachments/assets/14ba5767-a1ef-4ce8-8817-85949b17969b

The video shows the first v0.7.0 hardware run at roughly 17 FPS. It is included
as a real-console milestone, not a full-speed compatibility claim.

## Implemented

- Nintendo DS header parsing and CRC16 validation
- ARM7 and ARM9 executable-range validation
- Backend-neutral emulator lifecycle
- Dual-screen video, stereo audio, controller, and touch interfaces
- Vertical, horizontal, and single-screen layouts with integer scaling
- Label-matched Xbox controller mapping and right-stick touch input
- BGR555-to-XRGB8888 conversion and nearest-neighbor composition
- Host-side tests and ROM information utility
- Initial DeSmuME adapter and LibXenon hardware probe
- On-console FAT device discovery and Nintendo DS ROM-header validation
- Statically linked DeSmuME interpreter checkpoint for LibXenon
- Direct software-framebuffer output for the two Nintendo DS screens
- On-console emulation and video-stage profiler
- NooDS direct-boot core with no proprietary BIOS requirement
- Persistent `.sav` loading and periodic/exit-time save flushing
- Asynchronous Xbox framebuffer presentation on a second hardware thread
- Separate on-screen emulated-frame, displayed-video, and real-time DS-speed
  counters
- Xenon-specific direct THUMB/branch dispatch and reverse-sorted event queue
- Three-worker NooDS software-3D renderer on hardware contexts 2/3/4, with
  ordered scanline polygon bins and video presentation isolated on context 5
- Stable complete-frame delivery with no internal scheduler slices counted as video
- Persistent brightness, contrast, and color controls opened with R3
- Left-trigger Turbo mode with an uncapped one-in-four video handoff and an
  on-screen indicator
- Capture-aware Turbo rendering that keeps guest VRAM video/cutscene captures current
- Nonblocking framebuffer submission with bounded worker-failure diagnostics

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

## NooDS v0.9.1 validated Turbo build

The current Xbox target uses a pinned and patched NooDS core. It boots the first valid
`.nds` file found in `XenonDS/`, `xenonds/`, or the root of a mounted FAT
device, renders both DS screens, maps all DS controls, and stores a `.sav` file
beside the ROM. It uses direct boot, so DS BIOS and firmware files are optional.

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\build_xenon_noods.ps1
```

The USB-ready output is `platform/xenon/release-noods/`. Preserve its folder
layout, add one legally dumped ROM as `XenonDS/game.nds`, boot XeLL, and press
**A** at the staged-loader prompt. See [Xbox 360 runtime testing](docs/XBOX_TESTING.md)
for controls and limitations.

The build prints ROM-preload progress for large games. After `Core ready`, it
presents and paces only exact NooDS end-of-frame events. Internal CPU halt/resume
scheduler returns are never mistaken for frames. Invalid-instruction and
stopped-video watchdogs show ARM9/ARM7 diagnostics instead of silently hanging.
Solid-color frames remain valid game output and are never treated as a failure.
The overlay reports completed emulated frames, displayed-video frames, and
actual game-clock speed separately; SPEED 100% means the emulated Nintendo DS
is running in real time.

The v0.7.1 optimization pass replaced expensive indirect dispatch for common
THUMB instructions and ARM branches, uses native endian-correct mapped-memory
accesses, removes unused audio scheduling, enables link-time optimization, and
keeps the scheduler's next event at the end of its vector for constant-time
removal. On the deterministic host interpreter benchmark, the same frame hash
improved from 354.95 to 528.13 FPS (48.8%). Actual console performance varies
by game; the on-screen counter is the authoritative hardware measurement.

The v0.8.0 pass targets the next measured bottleneck: software 3D. Four LibXenon
worker contexts now split NooDS scanlines across the two non-main physical CPU
cores, while video presentation uses the main core's sibling context. Ordered
per-scanline polygon bins also avoid rescanning the complete polygon list for
every row. A deterministic 384-polygon renderer benchmark produced the same
frame hash in serial and threaded modes and improved from 1141.54 to 2717.16
FPS on the host (2.38x). The real-console result still depends on scene mix and
must be measured with the overlay.

Real-console testing of v0.8.0 improved Pokemon Black's 3D-heavy scenes from
about 9 FPS to a stable 22-23 FPS, with brief peaks near 30 FPS. Controller
input, action scenes, and persistent cartridge saves were also verified. The
v0.8.3 build established a working full-frame baseline. A later frontend change
incorrectly paced every return from `runCore()`, including internal CPU
halt/resume boundaries; that could throttle scheduler slices to 60 per second
before a real DS frame completed, producing two white screens and 0 FPS.

v0.9.0 made `Core::endFrame()` the single owner of cadence, statistics, and
save timing. Normal mode renders every completed frame. Left-trigger Turbo
switches only at a frame boundary while DS CPU, input, timer, and game logic
execution continue continuously. The presenter also copies only changed Xbox
framebuffer tiles after its first frame. These paths pass deterministic
frame-cadence, scheduler-return, framebuffer-hash, display-capture, and
release-LTO tests. Capture-heavy games still receive fresh internal 2D/3D
pixels when required, so Turbo cannot corrupt video or cutscene data stored in
guest VRAM.

v0.9.1 keeps that exact completed-frame ownership and replaces the ambiguous
speed presets with two honest modes. Normal is capped at 60 Hz and presents
every completed frame. Turbo is uncapped and hands one complete rendered frame
in four to the television; it does not claim a fixed 2x multiplier. The overlay
reports EMU, VID, and real DS-clock SPEED separately. Its Xenon scheduler calls
the fixed task table directly, the optimized presenter uses precomputed tiled
framebuffer offsets, and hardware context 5 is reserved exclusively for video.
Deterministic replay tests cover scheduler-only returns, 602 completed frames,
Turbo toggles, capture freshness, legitimate all-white output, 100 display-menu
open/close cycles, and exact framebuffer hashes. Real-console performance still
depends on the game and scene.

GitHub Actions publishes three v0.9.1 artifacts from the same build: the
USB-ready runtime, unstripped debug symbols, and a corresponding-source archive
containing the exact pinned NooDS source used by the executable.

## Legacy DeSmuME performance checkpoint

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

The script checks out the legacy DeSmuME and LibXenon revisions recorded in
`upstream.lock`. `scripts/fetch_noods.sh` checks out the pinned NooDS revision
and applies the Xbox compatibility patch without overwriting a modified local
checkout.

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Roadmap](docs/ROADMAP.md)
- [Xbox 360 runtime testing](docs/XBOX_TESTING.md)
- [Contributing](CONTRIBUTING.md)
- [Brand assets](branding/README.md)

## Legal

XenonDS does not include games, Nintendo BIOS or firmware files, encryption
keys, Microsoft SDK files, or proprietary Xbox software. Use ROM images and
firmware dumped from hardware and games you own. The legacy DeSmuME executable
is GPL-2.0; the NooDS executable is GPL-3.0-or-later. Shared XenonDS code is
GPL-2.0-or-later. Derived releases must preserve the applicable license and
provide corresponding source code.

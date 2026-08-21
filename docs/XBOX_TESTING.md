# Xbox 360 runtime test

XenonDS provides a hardware probe, a legacy DeSmuME performance checkpoint,
and the v0.9.1 NooDS validated Turbo build. The NooDS build enters the live
game loop with controller/touch input and persistent cartridge saves.

## Prepare the USB drive

1. Use a FAT32-formatted USB drive.
2. Create a folder named `XenonDS`.
3. Copy one legally dumped `.nds` image into that folder.
4. For either emulator build, copy `xenon.elf` to the drive root and
   `xenonds-core.elf32` into the `XenonDS` folder.

The probe scans every FAT device reported by LibXenon. On each device it checks
`XenonDS/`, `xenonds/`, and then the drive root, selecting the first valid
filename ending in `.nds`.

## Build

From PowerShell in the repository root:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\build_xenon_probe.ps1
```

The output is `platform\xenon\xenonds-probe.elf32`.

To build the current NooDS target:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\build_xenon_noods.ps1
```

Its USB-ready output is `platform\xenon\release-noods`.

To build the legacy DeSmuME performance checkpoint instead:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\build_xenon_desmume.ps1
```

Both packages use this layout:

```text
xenon.elf
XenonDS/
  xenonds-core.elf32
  game.nds
```

The ROM is deliberately excluded from builds and release archives.

## Expected result

A successful test prints:

- `FAT: OK`
- the mounted device being scanned
- ROM title, game code, maker, version, and file size
- matching header CRC
- ARM9 and ARM7 offsets, RAM addresses, and sizes
- `PASS: portable XenonDS ROM parser is running on Xbox 360.`

Press **A** to rescan after changing storage. Press the **Guide** button to
return to XeLL.

## Expected v0.4.1 interpreter result

Boot XeLL with the staged `xenon.elf` in the USB root, then press **A** at the
loader prompt. It reports FAT and core discovery, prints core-read percentages,
and then shows a memory-preparation bar at the bottom of the screen. After the
handoff, a successful first-frame test
prints `PASS: first DeSmuME software frame completed` followed by a frame hash.
Press **A** to run the four-frame profile. Photograph the resulting input, ARM,
frame-copy, core-total, and Xbox-video timings, then press **A** again to display
the DS screens and continue emulation. Press the **Guide** button to exit.

The v0.4.1 checkpoint deliberately uses DeSmuME's scalar interpreter. It has no
audio, persistent saves, ROM browser, or performance guarantees yet. If it
stops or displays incorrectly, photograph the complete on-screen message,
profile, and frame hash when filing an issue.

Do not publish ROM images, console keys, BIOS/firmware dumps, or NAND files in
the repository or in bug reports.

## NooDS controls and saves

Boot XeLL and press **A** at the staged-loader prompt. The Xbox face buttons
match their printed DS labels: A/B/X/Y become DS A/B/X/Y. The D-pad, bumpers,
Menu/View buttons become the DS D-pad, L/R, and Start/Select. Move the touch
cursor with the right stick and touch with the right trigger.

Click the right stick (R3) to open display settings. Use D-pad up/down to select
Brightness, Contrast, or Color; left/right changes it immediately; A restores
the defaults; B or R3 saves and closes. Settings persist beside the ROM in
`xenonds-display.cfg`. Left trigger toggles Turbo mode. Turbo removes the normal
60 Hz cap and sends one complete rendered frame in four to the television while
the DS CPUs, input, timers, and game logic continue running continuously.

The emulator creates `game.sav` beside `game.nds`, writes changed save data
periodically, and flushes it again when **Guide** exits. The current target has
no audio, ROM browser, save-state UI, microphone input, or network UI. Direct
boot does not require Nintendo DS BIOS or firmware files.

For a large ROM, wait for the `ROM preload` line to reach `100%`; do not power
off while the USB drive is being read. A successful v0.9.1 startup then prints
`Core ready. Running game...` and switches to the two DS screens. The frontend
updates and paces the television only at exact NooDS end-of-frame events.
Internal CPU halt/resume scheduler returns are neither counted nor delayed as
frames. The upper-left overlay shows EMU for completed emulated frames per
second, VID for frames sent to the television, and SPEED for real-time DS-clock
progress. SPEED 100% is full DS speed; 33% means the game is advancing at
roughly one third of normal speed regardless of EMU or VID. Values begin at
zero during the first sample.

Normal mode renders every completed frame and applies the normal 60 Hz cap.
Turbo is uncapped and hands one complete rendered frame in four to the
television, reducing renderer and presentation work without skipping DS CPU,
input, timer, or game-logic execution. Turbo can increase SPEED when rendering
is the bottleneck; it does not guarantee 2x speed in a CPU-bound scene, and VID
is intentionally lower than EMU. If the game captures a DS screen or 3D output
into its own VRAM, XenonDS still renders the required internal source. This
preserves capture-based cutscenes and effects while leaving the external
one-in-four handoff cadence unchanged.

Uniform white or black frames are valid game output and do not trigger an
error. If completed DS frames stop producing framebuffer handoffs, the display
worker stops accepting frames, or the emulated CPUs hit the invalid-opcode
limit, XenonDS returns to a diagnostic screen containing the ARM9 and ARM7
program counters and last invalid opcodes. Photograph that whole screen for a
bug report. A XeLL crash screen should be reported with its full stack dump and
the matching unstripped v0.9.1 symbols artifact.

The release is considered hardware-certified only after the exact runtime
artifact reaches animated game graphics, responds to a controller input, and
creates or updates `game.sav` on a real console.

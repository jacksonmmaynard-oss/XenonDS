# Xbox 360 runtime test

XenonDS provides a hardware probe, a legacy DeSmuME performance checkpoint,
and the v0.8.1 NooDS color and pacing preview. The NooDS build enters the live game loop
with controller/touch input and persistent cartridge saves.

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

The emulator creates `game.sav` beside `game.nds`, writes changed save data
periodically, and flushes it again when **Guide** exits. The current target has
no audio, ROM browser, save-state UI, microphone input, or network UI. Direct
boot does not require Nintendo DS BIOS or firmware files.

For a large ROM, wait for the `ROM preload` line to reach `100%`; do not power
off while the USB drive is being read. A successful v0.8.1 startup then prints
`Core ready. Running game...` and switches to the two DS screens. The frontend
updates the television only when NooDS completes a new DS frame, so startup is
no longer delayed once per internal scheduler slice. The upper-left overlay
separates EMU game-clock throughput from VID displayed-frame throughput. The
default one-frame performance skip reduces expensive rendering without skipping
DS CPU or input execution. Both counters initially display `00.0`.

If startup remains blank for 600 completed DS frames or the emulated CPUs hit
the invalid-opcode limit, XenonDS returns to a diagnostic screen containing the
ARM9 and ARM7 program counters and last invalid opcodes. Photograph that whole
screen for a bug report. A XeLL crash screen should be reported with its full
stack dump and the matching unstripped v0.8.1 symbols artifact.

The release is considered hardware-certified only after the exact runtime
artifact reaches animated game graphics, responds to a controller input, and
creates or updates `game.sav` on a real console.

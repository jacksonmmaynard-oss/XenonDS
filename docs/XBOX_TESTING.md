# Xbox 360 runtime test

XenonDS currently provides two Xbox executables: the v0.2 hardware probe and
the v0.3 experimental DeSmuME interpreter checkpoint. The probe only validates
the ROM header. The interpreter build attempts to execute and display the first
software-rendered Nintendo DS frame.

## Prepare the USB drive

1. Use a FAT32-formatted USB drive.
2. Create a folder named `XenonDS`.
3. Copy one legally dumped `.nds` image into that folder.
4. For the interpreter checkpoint, copy `xenon.elf` to the drive root and
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

To build the v0.3 interpreter checkpoint instead:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\build_xenon_desmume.ps1
```

Its USB-ready output is `platform\xenon\release` with this layout:

```text
xenon.elf
XenonDS/
  xenonds-core.elf32
  game.nds
```

GitHub Actions also builds the same file on every push to `main` and every pull
request. Open the successful **Core CI** run and download the
`xenonds-runtime-probe` artifact if you prefer not to build locally.

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

## Expected v0.3 interpreter result

Boot XeLL with the staged `xenon.elf` in the USB root, then press **A** at the
loader prompt. It reports FAT and core discovery, prints core-read percentages,
and then shows a memory-preparation bar at the bottom of the screen. After the
handoff, a successful first-frame test
prints `PASS: first DeSmuME software frame completed` followed by a frame hash.
Press **A** to display the DS screens and continue emulation. Press the **Guide**
button to exit.

The v0.3 checkpoint deliberately uses DeSmuME's scalar interpreter. It has no
audio, persistent saves, ROM browser, or performance guarantees yet. If it
stops or displays incorrectly, photograph the complete on-screen message and
include the reported frame hash when filing an issue.

Do not publish ROM images, console keys, BIOS/firmware dumps, or NAND files in
the repository or in bug reports.

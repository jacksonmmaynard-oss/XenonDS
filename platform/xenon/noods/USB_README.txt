XenonDS v0.8.1 - NooDS color and pacing preview

Copy this package's contents to the root of a FAT32 USB drive, preserving the
folders:

  xenon.elf
  XenonDS/xenonds-core.elf32

Add exactly one legally dumped Nintendo DS image:

  XenonDS/game.nds

Boot XeLL and press A at the loader prompt. XenonDS boots the game directly;
Nintendo DS BIOS and firmware files are not required for this mode. A save is
stored beside the ROM as XenonDS/game.sav. The emulator flushes changed save
data periodically and again when you exit with the Guide button.

Large ROMs show a `ROM preload` line. Wait for it to reach 100%, followed by
`Core ready. Running game...`. The two DS screens then replace the text console.
The upper-left overlay reports EMU (emulated DS frames per second) and VID
(frames displayed per second). Both read 00.0 during initial sampling. The
default performance mode renders every other frame while continuing to execute
the DS CPU and game clock every frame.

Controls:

  Xbox A/B/X/Y       DS A/B/X/Y (matching printed button labels)
  D-pad              DS D-pad
  LB/RB              DS L/R
  Menu/View          DS Start/Select
  Right stick        Move the touchscreen cursor
  Right trigger      Touch the touchscreen
  Guide              Save and exit

This NooDS build uses native-resolution software rendering and has no
audio, ROM browser, save states, microphone, or network emulation UI yet.

If XenonDS stops at a diagnostic screen, photograph the complete ARM9/ARM7
message and keep the matching v0.8.1 symbols artifact. The build is considered
verified only after animated graphics, controller input, and `game.sav` have
all been tested on the console.

Do not publish ROMs, console keys, BIOS/firmware dumps, or NAND files.
XenonDS' NooDS executable is distributed under GPL-3.0-or-later. Corresponding
source and build scripts are in the XenonDS repository.

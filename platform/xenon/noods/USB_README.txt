XenonDS v0.9.1 - validated Turbo build

Copy this package's contents to the root of a FAT32 USB drive, preserving this
layout:

  xenon.elf
  XenonDS/xenonds-core.elf32

Add exactly one legally dumped Nintendo DS image:

  XenonDS/game.nds

Boot XeLL and press A at the loader prompt. XenonDS boots the game directly;
Nintendo DS BIOS and firmware files are not required. A save is stored beside
the ROM as XenonDS/game.sav and is flushed periodically and when Guide exits.

Large ROMs show ROM-preload progress. Wait for 100%, then for
`Core ready. Running game...`. EMU is the completed emulated-frame rate; VID is
the rate of frames sent to the television. SPEED is real DS game-clock speed:
100% is full speed, while 33% means the game is advancing at about one third of
normal speed. The counters begin at zero while collecting their first sample.

Controls:

  Xbox A/B/X/Y       DS A/B/X/Y (matching printed button labels)
  D-pad              DS D-pad
  LB/RB              DS L/R
  Menu/View          DS Start/Select
  Right stick        Move the touchscreen cursor
  Right trigger      Touch the touchscreen
  Left trigger       Toggle Turbo rendering mode (yellow TURBO indicator)
  Right-stick click  Open / close display settings
  Guide              Save and exit

Display settings:

  D-pad up/down      Select Brightness, Contrast, or Color
  D-pad left/right   Change the selected value immediately
  A                  Restore the defaults
  B                  Save settings and close

Settings are stored beside the ROM as XenonDS/xenonds-display.cfg. Normal mode
renders every completed DS frame and applies the normal 60 Hz cap. Turbo removes
that cap and sends one complete rendered frame in four to the television while
still executing DS CPU, input, timers, and game logic continuously. Turbo does
not promise a fixed 2x multiplier; its SPEED counter shows the real result.
Guest display captures still receive fresh internal 2D/3D pixels when required,
preserving capture-based videos and effects.

The v0.9.1 frontend owns pacing, statistics, and save timing at exact NooDS
end-of-frame boundaries. Internal CPU halt/resume scheduler returns are never
counted or delayed as frames. This fixes the v0.8.4-v0.8.5 failure that could
leave both DS screens white with the overlay frozen at 0 FPS. If completed DS
frames continue without a video handoff, the build stops with a diagnostic
instead of hanging indefinitely. Frame submission is nonblocking, presentation
owns hardware context 5 exclusively, and ordinary video-worker waits are
bounded so the frontend cannot silently deadlock during normal presentation.

This preview has no audio, ROM browser, save states, microphone input, or
network-emulation UI. Do not publish ROMs, console keys, BIOS/firmware dumps,
or NAND files. XenonDS' NooDS executable is GPL-3.0-or-later; corresponding
source and build scripts are in the XenonDS repository.

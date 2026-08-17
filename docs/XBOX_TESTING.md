# Xbox 360 testing

## Requirements

- JTAG or RGH Xbox 360 capable of booting XeLL
- FAT32 USB drive
- v0.6.1 runtime package
- one legally dumped Nintendo DS ROM

## USB layout

```text
xenon.elf
XenonDS/
  xenonds-core.elf32
  game.nds
```

Boot XeLL with the USB drive connected. Press **A** at the staged-loader prompt.
The core reads the ROM, executes a 30-frame performance sample, and displays
the timing result. Press **A** again to enter the game. Press **Guide** to leave.

The Xbox face buttons map by physical position: Xbox A/B/X/Y become DS
B/A/Y/X. The D-pad, bumpers, Menu, and View map to the DS D-pad, L/R, Start,
and Select. The right stick controls the touch cursor and the right trigger
touches the screen.

Current limitations include low performance, no audio, no persistent saves,
no save states, no ROM browser, and limited game testing.

Never attach ROMs, BIOS or firmware dumps, console keys, CPU/DVD keys, or NAND
files to releases or bug reports.

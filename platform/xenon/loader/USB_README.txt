XenonDS v0.4.1 safe performance checkpoint

Copy this package's contents to the root of a FAT32 USB drive:

  xenon.elf
  XenonDS/xenonds-core.elf32

Add one legally dumped Nintendo DS image:

  XenonDS/game.nds

Boot XeLL and press A at the loader prompt. The loader will show core-read
percentages and a progress bar while preparing DeSmuME's runtime memory. After
a crash/reload, it waits for A instead of automatically relaunching the core.
The core prints five first-frame checkpoints after loading the ROM. Press A
after PASS to run a four-frame hardware profile. Photograph the timing screen,
then press A again to start the game. This checkpoint keeps the
hardware-proven v0.3.7 presenter while collecting timings, avoiding the
experimental v0.4.0 framebuffer regression.

Do not publish ROMs, console keys, BIOS/firmware dumps, or NAND files.

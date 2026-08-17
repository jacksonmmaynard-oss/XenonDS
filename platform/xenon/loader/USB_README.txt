XenonDS v0.6.1 fast-timing pipeline test

Copy this package's contents to the root of a FAT32 USB drive:

  xenon.elf
  XenonDS/xenonds-core.elf32

Add one legally dumped Nintendo DS image:

  XenonDS/game.nds

Boot XeLL and press A at the loader prompt. The loader will show core-read
percentages and a progress bar while preparing DeSmuME's runtime memory. After
a crash/reload, it waits for A instead of automatically relaunching the core.
The core prints five first-frame checkpoints after loading the ROM. Press A
after PASS to run the 30-frame performance sample. Photograph the timing screen,
then press A again to start the game. "Serial total" shows the old one-core
cost. "Pipelined" shows steady-state throughput with framebuffer conversion on
a second physical Xbox core. This build also removes unused scheduler debugger
branches and disables DeSmuME's expensive advanced bus/cache timing mode. It
keeps the proven O2 interpreter layout and does not fake speed with frame skip.

Do not publish ROMs, console keys, BIOS/firmware dumps, or NAND files.

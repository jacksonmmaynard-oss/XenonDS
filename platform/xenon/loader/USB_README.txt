XenonDS v0.3.7 execution-resume checkpoint

Copy this package's contents to the root of a FAT32 USB drive:

  xenon.elf
  XenonDS/xenonds-core.elf32

Add one legally dumped Nintendo DS image:

  XenonDS/game.nds

Boot XeLL and press A at the loader prompt. The loader will show core-read
percentages and a progress bar while preparing DeSmuME's runtime memory. After
a crash/reload, it waits for A instead of automatically relaunching the core.
The core prints five first-frame checkpoints after loading the ROM. Press A
after PASS to display the first DS frame. The Xbox frame is assembled off-screen
and published at once to prevent the visible upward redraw from v0.3.4. Video
MMIO fields are read as aligned 32-bit words to avoid the v0.3.5 DSI exception.
The Xenon frontend resumes DeSmuME after ROM reset so each NDS_exec() reaches
VBlank instead of returning after a single hardware event.

Do not publish ROMs, console keys, BIOS/firmware dumps, or NAND files.

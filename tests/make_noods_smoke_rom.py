#!/usr/bin/env python3
"""Create a tiny, redistributable NDS ROM for frontend/core smoke tests."""

from pathlib import Path
import struct
import sys


def put32(image: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<I", image, offset, value)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: make_noods_smoke_rom.py OUTPUT.nds", file=sys.stderr)
        return 2

    image = bytearray(0x400)
    image[0:12] = b"XENONDS TEST"
    image[12:16] = b"XNDS"
    image[16:18] = b"01"

    # The ARM9 configures VRAM-A for LCD display mode, fills it with red, then
    # idles. This verifies actual ARM execution, MMIO, VRAM and 2D output while
    # remaining a tiny, clean-room and redistributable test program. ARM7 idles.
    arm9 = [
        0xE59F002C,  # ldr  r0, =VRAMCNT_A
        0xE3A01080,  # mov  r1, #0x80 (VRAM-A enabled in LCDC mode)
        0xE5C01000,  # strb r1, [r0]
        0xE59F0024,  # ldr  r0, =DISPCNT_A
        0xE59F1024,  # ldr  r1, =VRAM_DISPLAY_MODE
        0xE5801000,  # str  r1, [r0]
        0xE59F0020,  # ldr  r0, =VRAM_A
        0xE59F1020,  # ldr  r1, =TWO_RED_PIXELS
        0xE59F2020,  # ldr  r2, =WORD_COUNT
        0xE4801004,  # fill: str r1, [r0], #4
        0xE2522001,  # subs r2, r2, #1
        0x1AFFFFFC,  # bne  fill
        0xEAFFFFFE,  # idle: b idle
        0x04000240,  # VRAMCNT_A
        0x04000000,  # DISPCNT_A
        0x00020000,  # display mode 2: show VRAM-A directly
        0x06800000,  # VRAM-A LCDC mapping
        0x001F001F,  # two BGR555 red pixels
        256 * 192 // 2,
    ]
    arm9_bytes = struct.pack("<" + "I" * len(arm9), *arm9)

    put32(image, 0x20, 0x200)
    put32(image, 0x24, 0x02000000)
    put32(image, 0x28, 0x02000000)
    put32(image, 0x2C, len(arm9_bytes))
    put32(image, 0x30, 0x200 + len(arm9_bytes))
    put32(image, 0x34, 0x03800000)
    put32(image, 0x38, 0x03800000)
    put32(image, 0x3C, 4)
    put32(image, 0x80, len(image))
    image[0x200:0x200 + len(arm9_bytes)] = arm9_bytes
    arm7_offset = 0x200 + len(arm9_bytes)
    image[arm7_offset:arm7_offset + 4] = struct.pack("<I", 0xEAFFFFFE)

    Path(sys.argv[1]).write_bytes(image)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

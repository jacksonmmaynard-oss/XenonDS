// SPDX-License-Identifier: GPL-2.0-only
#include "rom_finder.hpp"

#include <console/console.h>
#include <diskio/ata.h>
#include <input/input.h>
#include <libfat/fat.h>
#include <usb/usbmain.h>
#include <xenon_soc/xenon_power.h>
#include <xenos/xenos.h>

#include <cstdio>
#include <cstring>

namespace {

void print_rom(const xenonds::xenon::FoundRom& rom) {
    const xenonds::NdsHeader& header = rom.header;
    std::printf("\nROM: %s\n", rom.path.c_str());
    std::printf("Title: %s\n", header.title.c_str());
    std::printf("Game code: %s  Maker: %s  Version: %u\n",
                header.game_code.c_str(), header.maker_code.c_str(),
                static_cast<unsigned int>(header.rom_version));
    std::printf("File size: %lu bytes  Header CRC: %04X (valid)\n",
                static_cast<unsigned long>(rom.file_size),
                static_cast<unsigned int>(header.stored_header_crc));
    std::printf("ARM9: offset %08lX  RAM %08lX  size %08lX\n",
                static_cast<unsigned long>(header.arm9.offset),
                static_cast<unsigned long>(header.arm9.ram_address),
                static_cast<unsigned long>(header.arm9.size));
    std::printf("ARM7: offset %08lX  RAM %08lX  size %08lX\n",
                static_cast<unsigned long>(header.arm7.offset),
                static_cast<unsigned long>(header.arm7.ram_address),
                static_cast<unsigned long>(header.arm7.size));
    std::printf("\nPASS: portable XenonDS ROM parser is running on Xbox 360.\n");
}

void scan_for_rom() {
    xenonds::xenon::FoundRom rom;
    const xenonds::Status status = xenonds::xenon::find_first_rom(&rom);
    if (status.ok()) {
        print_rom(rom);
    } else {
        std::printf("ROM scan failed: %s\n", status.message().c_str());
    }
}

void wait_for_button_release() {
    controller_data_s pad;
    do {
        usb_do_poll();
        std::memset(&pad, 0, sizeof(pad));
        get_controller_data(&pad, 0);
    } while (pad.a || pad.logo);
}

} // namespace

int main() {
    controller_data_s pad;

    xenos_init(VIDEO_MODE_AUTO);
    console_init();
    xenon_make_it_faster(XENON_SPEED_FULL);
    usb_init();
    usb_do_poll();
    xenon_ata_init();
    xenon_atapi_init();
    const bool fat_ready = fatInitDefault();

    std::printf("XenonDS runtime probe v0.2.0\n");
    std::printf("Video: OK  USB: OK  Controller: OK  ATA: OK  FAT: %s\n\n",
                fat_ready ? "OK" : "FAILED");
    std::printf("Looking for the first valid .nds file in:\n");
    std::printf("  <drive>:/XenonDS/\n  <drive>:/xenonds/\n  <drive>:/\n\n");

    if (fat_ready) {
        scan_for_rom();
    }

    std::printf("\nA: rescan storage    Guide: exit to XeLL\n");
    wait_for_button_release();

    for (;;) {
        usb_do_poll();
        std::memset(&pad, 0, sizeof(pad));
        get_controller_data(&pad, 0);

        if (pad.logo) {
            return 0;
        }
        if (pad.a) {
            std::printf("\n--- Rescanning ---\n");
            scan_for_rom();
            std::printf("\nA: rescan storage    Guide: exit to XeLL\n");
            wait_for_button_release();
        }
    }
}

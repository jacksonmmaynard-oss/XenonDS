// SPDX-License-Identifier: GPL-2.0-only
#include "progress_loader.h"

#include <console/console.h>
#include <diskio/ata.h>
#include <input/input.h>
#include <libfat/fat.h>
#include <sys/stat.h>
#include <usb/usbmain.h>
#include <xenon_soc/xenon_power.h>
#include <xenos/xenos.h>

#include <stdio.h>
#include <string.h>

extern int bdev_enum(int handle, const char** name);

static void wait_for_launch(void) {
    struct controller_data_s pad;
    printf("Press A to launch the core.\n");
    printf("After a crash/reload, the loader will wait here instead of looping.\n");

    do {
        usb_do_poll();
        memset(&pad, 0, sizeof(pad));
        get_controller_data(&pad, 0);
    } while (pad.a);

    do {
        usb_do_poll();
        memset(&pad, 0, sizeof(pad));
        get_controller_data(&pad, 0);
    } while (!pad.a);
}

static int find_payload(char* output, size_t output_size) {
    const char* device = NULL;
    int handle = -1;

    while ((handle = bdev_enum(handle, &device)) >= 0) {
        const char* locations[] = {
            "%s:/XenonDS/xenonds-core.elf32",
            "%s:/xenonds/xenonds-core.elf32",
            "%s:/xenonds-core.elf32",
        };

        for (size_t i = 0; i < sizeof(locations) / sizeof(locations[0]); ++i) {
            struct stat info;
            snprintf(output, output_size, locations[i], device);
            if (stat(output, &info) == 0 && info.st_size > 0) {
                return 1;
            }
        }
    }

    output[0] = '\0';
    return 0;
}

int main(void) {
    xenos_init(VIDEO_MODE_AUTO);
    console_init();
    xenon_make_it_faster(XENON_SPEED_FULL);
    usb_init();
    usb_do_poll();
    xenon_ata_init();
    xenon_atapi_init();

    printf("XenonDS staged loader v0.3.7\n");
    printf("Stage 1/3: initializing FAT storage...\n");
    if (!fatInitDefault()) {
        printf("FAILED: no FAT storage device was mounted.\n");
        printf("Correct the USB drive and reboot XeLL.\n");
        for (;;) {}
    }

    char payload_path[256];
    if (!find_payload(payload_path, sizeof(payload_path))) {
        printf("FAILED: xenonds-core.elf32 was not found.\n\n");
        printf("Required USB layout:\n");
        printf("  xenon.elf\n");
        printf("  XenonDS/xenonds-core.elf32\n");
        printf("  XenonDS/game.nds\n");
        for (;;) {}
    }

    printf("Core: %s\n", payload_path);
    wait_for_launch();
    printf("Stage 2/3: reading and validating the emulator core...\n");
    const int result = xenonds_run_payload(payload_path);
    printf("FAILED: staged loader error %d.\n", result);
    printf("Photograph this complete screen for the XenonDS issue.\n");
    for (;;) {}
}

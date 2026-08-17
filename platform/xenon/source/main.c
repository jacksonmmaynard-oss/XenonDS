// SPDX-License-Identifier: GPL-2.0-only
#include <stdio.h>

#include <console/console.h>
#include <diskio/ata.h>
#include <input/input.h>
#include <libfat/fat.h>
#include <usb/usbmain.h>
#include <xenon_soc/xenon_power.h>
#include <xenos/xenos.h>

int main(void) {
    struct controller_data_s pad;

    xenos_init(VIDEO_MODE_AUTO);
    console_init();
    xenon_make_it_faster(XENON_SPEED_FULL);
    usb_init();
    usb_do_poll();
    xenon_ata_init();
    xenon_atapi_init();
    fatInitDefault();

    printf("XenonDS hardware probe v0.1.0\n");
    printf("Video, USB, controller, ATA and FAT initialization completed.\n");
    printf("Press the Xbox Guide button to exit.\n");

    for (;;) {
        usb_do_poll();
        get_controller_data(&pad, 0);
        if (pad.logo) {
            return 0;
        }
    }
}


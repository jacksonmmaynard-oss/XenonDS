// SPDX-License-Identifier: GPL-2.0-only
#include "rom_finder.hpp"
#include "xenon_video.hpp"
#include "xenonds/backends/desmume_backend.hpp"
#include "xenonds/controller_mapper.hpp"
#include "xenonds/session.hpp"

#include <console/console.h>
#include <diskio/ata.h>
#include <input/input.h>
#include <libfat/fat.h>
#include <ppc/timebase.h>
#include <time/time.h>
#include <usb/usbmain.h>
#include <xenon_soc/xenon_power.h>
#include <xenos/xenos.h>

#include <cstdio>
#include <cstring>

namespace {

const unsigned long kTargetFrameMicroseconds = 16715;

xenonds::ControllerSnapshot snapshot_from_pad(const controller_data_s& pad) {
    xenonds::ControllerSnapshot snapshot;
    snapshot.south = pad.a != 0;
    snapshot.east = pad.b != 0;
    snapshot.west = pad.x != 0;
    snapshot.north = pad.y != 0;
    snapshot.dpad_up = pad.up != 0;
    snapshot.dpad_down = pad.down != 0;
    snapshot.dpad_left = pad.left != 0;
    snapshot.dpad_right = pad.right != 0;
    snapshot.left_shoulder = pad.lb != 0;
    snapshot.right_shoulder = pad.rb != 0;
    snapshot.start = pad.start != 0;
    snapshot.back = pad.back != 0;
    snapshot.right_x = pad.s2_x;
    snapshot.right_y = pad.s2_y;
    snapshot.right_trigger = pad.rt;
    return snapshot;
}

std::uint32_t frame_hash(const xenonds::FrameOutput& frame) {
    std::uint32_t hash = 2166136261u;
    for (std::size_t i = 0; i < frame.pixels.size(); ++i) {
        hash ^= frame.pixels[i];
        hash *= 16777619u;
    }
    return hash;
}

bool wait_to_start() {
    controller_data_s pad;
    for (;;) {
        usb_do_poll();
        std::memset(&pad, 0, sizeof(pad));
        get_controller_data(&pad, 0);
        if (pad.logo) {
            return false;
        }
        if (pad.a) {
            do {
                usb_do_poll();
                std::memset(&pad, 0, sizeof(pad));
                get_controller_data(&pad, 0);
            } while (pad.a);
            return true;
        }
    }
}

void show_failure(const char* stage, const xenonds::Status& status) {
    std::printf("\nFAILED during %s: %s\n", stage, status.message().c_str());
    std::printf("Guide: return to XeLL\n");
    controller_data_s pad;
    do {
        usb_do_poll();
        std::memset(&pad, 0, sizeof(pad));
        get_controller_data(&pad, 0);
    } while (!pad.logo);
}

} // namespace

int main() {
    xenos_init(VIDEO_MODE_AUTO);
    console_init();
    xenon_make_it_faster(XENON_SPEED_FULL);
    usb_init();
    usb_do_poll();
    xenon_ata_init();
    xenon_atapi_init();

    std::printf("XenonDS DeSmuME interpreter checkpoint v0.3.7\n");
    if (!xenonds::xenon::initialize_video_presenter()) {
        std::printf("FAILED: Xbox framebuffer information is invalid.\n");
        return 1;
    }
    if (!fatInitDefault()) {
        show_failure("FAT initialization",
                     xenonds::Status(xenonds::ErrorCode::io_error,
                                     "No FAT device could be mounted"));
        return 1;
    }

    xenonds::xenon::FoundRom rom;
    xenonds::Status status = xenonds::xenon::find_first_rom(&rom);
    if (!status.ok()) {
        show_failure("ROM discovery", status);
        return 1;
    }

    std::printf("ROM: %s\n", rom.path.c_str());
    std::printf("Title: %s  Code: %s  CRC: %04X\n",
                rom.header.title.c_str(), rom.header.game_code.c_str(),
                static_cast<unsigned int>(rom.header.stored_header_crc));
    std::printf("Initializing interpreter and software renderer...\n");

    xenonds::DesmumeBackend backend;
    xenonds::Session session(&backend);
    xenonds::CoreConfig config;
    config.enable_audio = false;
    config.enable_jit = false;

    status = session.initialize(config);
    if (!status.ok()) {
        show_failure("DeSmuME initialization", status);
        return 1;
    }

    std::printf("Core initialization: PASS\n");
    std::printf("Loading ROM through DeSmuME...\n");

    status = session.load_rom_header(rom.header_bytes.data(), rom.header_bytes.size(),
                                     rom.file_size, rom.path);
    if (!status.ok()) {
        show_failure("ROM loading", status);
        return 1;
    }

    status = session.start();
    if (!status.ok()) {
        show_failure("session start", status);
        return 1;
    }

    xenonds::FrameOutput frame;
    xenonds::InputState input;
    std::printf("Executing first emulated frame...\n");
    status = session.run_frame(input, &frame);
    if (!status.ok()) {
        show_failure("first emulated frame", status);
        return 1;
    }

    std::printf("PASS: first DeSmuME software frame completed. Hash: %08lX\n",
                static_cast<unsigned long>(frame_hash(frame)));
    std::printf("A: display and continue    Guide: return to XeLL\n");
    if (!wait_to_start()) {
        return 0;
    }

    xenonds::ControllerMapper mapper;
    controller_data_s pad;

    // Show the already-completed first frame immediately. The presenter builds
    // a tiled frame off-screen before publishing it to avoid visible sweeps.
    xenonds::xenon::present_ds_frame(frame, input.touch);

    for (;;) {
        const std::uint64_t frame_start = mftb();
        usb_do_poll();
        std::memset(&pad, 0, sizeof(pad));
        get_controller_data(&pad, 0);
        if (pad.logo) {
            return 0;
        }

        input = mapper.map(snapshot_from_pad(pad));
        status = session.run_frame(input, &frame);
        if (!status.ok()) {
            console_init();
            show_failure("emulation loop", status);
            return 1;
        }

        xenonds::xenon::present_ds_frame(frame, input.touch);
        const unsigned long elapsed = tb_diff_usec(mftb(), frame_start);
        if (elapsed < kTargetFrameMicroseconds) {
            udelay(static_cast<int>(kTargetFrameMicroseconds - elapsed));
        }
    }
}

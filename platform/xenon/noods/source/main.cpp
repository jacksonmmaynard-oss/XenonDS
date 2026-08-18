// SPDX-License-Identifier: GPL-3.0-or-later
#include "noods_video.hpp"
#include "rom_finder.hpp"
#include "xenonds/controller_mapper.hpp"

#include "core.h"
#include "settings.h"

#include <console/console.h>
#include <diskio/ata.h>
#include <input/input.h>
#include <libfat/fat.h>
#include <ppc/timebase.h>
#include <time/time.h>
#include <usb/usbmain.h>
#include <xenon_soc/xenon_power.h>
#include <xenos/xenos.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>

namespace {

const unsigned long kTargetFrameMicroseconds = 16715;
const unsigned int kBlankFrameLimit = 600;
const std::uint32_t kUnknownOpcodeLimit = 4096;
std::uint32_t ds_frame[xenonds::kCombinedPixelCount];

bool frame_has_content(const std::uint32_t* frame) {
    const std::uint32_t first = frame[0];
    for (std::size_t i = 1; i < xenonds::kCombinedPixelCount; ++i) {
        if (frame[i] != first)
            return true;
    }
    return false;
}

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

void wait_for_guide() {
    controller_data_s pad;
    do {
        usb_do_poll();
        std::memset(&pad, 0, sizeof(pad));
        get_controller_data(&pad, 0);
    } while (!pad.logo);
}

void fail(const char* message) {
    std::printf("\nFAILED: %s\nGuide: return to XeLL\n", message);
    wait_for_guide();
}

std::string parent_path(const std::string& path) {
    const std::string::size_type slash = path.find_last_of("/\\");
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

void configure_noods(const std::string& rom_path) {
    Settings::basePath = parent_path(rom_path);
    Settings::directBoot = 1;
    // Commercial games continuously stream small blocks from the cartridge.
    // LibXenon FAT I/O is both slower and less predictable than desktop stdio,
    // so preload the image once and execute from a verified in-memory copy.
    Settings::romInRam = 1;
    Settings::fpsLimiter = 0;
    Settings::frameskip = 0;
    Settings::threaded2D = 0;
    Settings::threaded3D = 0;
    Settings::highRes3D = 0;
    Settings::screenGhost = 0;
    Settings::emulateAudio = 0;
    Settings::savesFolder = 0;
    Settings::statesFolder = 0;
    Settings::cheatsFolder = 0;
    Settings::screenFilter = 0;
    Settings::dsiMode = 0;
    Settings::arm7Hle = 0;

    Settings::gbaBiosPath = Settings::basePath + "/gba_bios.bin";
    Settings::ndsBios9Path = Settings::basePath + "/bios9.bin";
    Settings::ndsBios7Path = Settings::basePath + "/bios7.bin";
    Settings::ndsFirmPath = Settings::basePath + "/firmware.bin";
}

void apply_input(Core& core, const xenonds::InputState& input) {
    for (int key = 0; key < 12; ++key) {
        if (input.buttons & (1u << key))
            core.input.pressKey(key);
        else
            core.input.releaseKey(key);
    }

    if (input.touch.pressed) {
        core.spi.setTouch(input.touch.x, input.touch.y);
        core.input.pressScreen();
    }
    else {
        core.spi.clearTouch();
        core.input.releaseScreen();
    }
}

const char* core_error_message(int error) {
    switch (error) {
    case ERROR_NDS_BIOS: return "DS BIOS could not be initialized";
    case ERROR_NDS_FIRM: return "DS firmware could not be initialized";
    case ERROR_DSI_BIOS: return "DSi BIOS is unavailable";
    case ERROR_DSI_FIRM: return "DSi firmware is unavailable";
    case ERROR_DSI_NAND: return "DSi NAND is unavailable";
    case ERROR_ROM: return "NooDS could not open or parse the ROM";
    default: return "NooDS initialization failed";
    }
}

} // namespace

int main() {
    xenos_init(VIDEO_MODE_AUTO);
    console_init();
    xenon_make_it_faster(XENON_SPEED_FULL);
    xenon_thread_startup();
    usb_init();
    usb_do_poll();
    xenon_ata_init();
    xenon_atapi_init();

    std::printf("XenonDS NooDS playable integration v0.7.0\n");
    if (!xenonds::xenon::initialize_noods_video()) {
        fail("Xbox framebuffer information is invalid");
        return 1;
    }
    if (!fatInitDefault()) {
        fail("No FAT device could be mounted");
        return 1;
    }

    xenonds::xenon::FoundRom rom;
    const xenonds::Status status = xenonds::xenon::find_first_rom(&rom);
    if (!status.ok()) {
        fail(status.message().c_str());
        return 1;
    }

    std::printf("ROM: %s\n", rom.path.c_str());
    std::printf("Title: %s  Code: %s\n",
                rom.header.title.c_str(), rom.header.game_code.c_str());
    std::printf("Starting NooDS (direct boot, software renderer)...\n");
    configure_noods(rom.path);

    Core* core = nullptr;
    try {
        core = new Core(rom.path);
    }
    catch (int error) {
        fail(core_error_message(error));
        return 1;
    }
    catch (const std::bad_alloc&) {
        fail("Not enough memory to initialize NooDS");
        return 1;
    }
    catch (...) {
        fail("Unexpected exception while initializing NooDS");
        return 1;
    }

    std::printf("Core ready. Running game...\n");
    xenonds::ControllerMapper mapper;
    xenonds::InputState input;
    controller_data_s pad;
    unsigned int frames_since_save = 0;
    unsigned int blank_frames = 0;
    bool saw_content = false;
    std::uint64_t last_frame_tick = mftb();

    for (;;) {
        usb_do_poll();
        std::memset(&pad, 0, sizeof(pad));
        get_controller_data(&pad, 0);
        if (pad.logo) {
            xenonds::xenon::wait_for_noods_video();
            core->cartridgeNds.writeSave();
            delete core;
            return 0;
        }

        input = mapper.map(snapshot_from_pad(pad));
        apply_input(*core, input);
        try {
            core->runCore();
        }
        catch (int error) {
            xenonds::xenon::wait_for_noods_video();
            delete core;
            fail(core_error_message(error));
            return 1;
        }
        catch (const std::bad_alloc&) {
            xenonds::xenon::wait_for_noods_video();
            delete core;
            fail("Memory allocation failed while running the game");
            return 1;
        }
        catch (...) {
            xenonds::xenon::wait_for_noods_video();
            delete core;
            fail("Unexpected exception while running the game");
            return 1;
        }

        const bool frame_ready = core->gpu.getFrame(ds_frame, false);
        if (frame_ready) {
            if (!saw_content) {
                saw_content = frame_has_content(ds_frame);
                if (!saw_content)
                    ++blank_frames;
            }
            xenonds::xenon::present_noods_frame(ds_frame, input.touch);

            // Pace actual DS video frames, not internal scheduler slices.
            // NooDS may require many runCore() calls before a frame is ready.
            const unsigned long elapsed = tb_diff_usec(mftb(), last_frame_tick);
            if (elapsed < kTargetFrameMicroseconds)
                udelay(static_cast<int>(kTargetFrameMicroseconds - elapsed));
            last_frame_tick = mftb();

            if (++frames_since_save >= 300) {
                core->cartridgeNds.writeSave();
                frames_since_save = 0;
            }
        }

        const std::uint32_t unknown_opcodes =
            core->interpreter[0].unknownOpcodeCount +
            core->interpreter[1].unknownOpcodeCount;
        if (unknown_opcodes >= kUnknownOpcodeLimit ||
            (!saw_content && blank_frames >= kBlankFrameLimit)) {
            xenonds::xenon::wait_for_noods_video();
            std::printf("\nCore startup diagnostic:\n");
            std::printf("ARM9 PC=%08X opcode=%08X unknown=%u\n",
                        core->interpreter[0].getPC(),
                        core->interpreter[0].lastUnknownOpcode,
                        core->interpreter[0].unknownOpcodeCount);
            std::printf("ARM7 PC=%08X opcode=%08X unknown=%u\n",
                        core->interpreter[1].getPC(),
                        core->interpreter[1].lastUnknownOpcode,
                        core->interpreter[1].unknownOpcodeCount);
            core->cartridgeNds.writeSave();
            delete core;
            fail(unknown_opcodes >= kUnknownOpcodeLimit
                     ? "The emulated CPUs encountered invalid instructions"
                     : "The game produced only blank frames during startup");
            return 1;
        }

    }
}

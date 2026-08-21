// SPDX-License-Identifier: GPL-3.0-or-later
#include "noods_video.hpp"
#include "rom_finder.hpp"
#include "speed_mode.hpp"
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

const unsigned long kFrameProgressTimeoutMicroseconds = 10000000UL;
const unsigned long kSaveIntervalMicroseconds = 5000000UL;
const unsigned int kSchedulerReturnLimit = 1024;
const unsigned int kVideoHandoffLimit = 8;
const unsigned int kVideoSubmissionLimit = 240;
const std::uint32_t kUnknownOpcodeLimit = 4096;
std::uint16_t ds_frame[xenonds::kCombinedPixelCount];

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

template <typename T>
T clamp_value(T value, T minimum, T maximum) {
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

bool load_picture_settings(const std::string& path,
                           xenonds::xenon::PictureSettings* settings) {
    FILE* file = std::fopen(path.c_str(), "r");
    if (!file) return false;

    int brightness = 0;
    unsigned int contrast = 0;
    unsigned int saturation = 0;
    const int fields = std::fscanf(
        file, "brightness=%d\ncontrast=%u\nsaturation=%u",
        &brightness, &contrast, &saturation);
    std::fclose(file);
    if (fields != 3) return false;

    settings->brightness = clamp_value(brightness, -64, 64);
    settings->contrast = clamp_value(contrast, 50u, 150u);
    settings->saturation = clamp_value(saturation, 0u, 150u);
    return true;
}

bool save_picture_settings(const std::string& path,
                           const xenonds::xenon::PictureSettings& settings) {
    FILE* file = std::fopen(path.c_str(), "w");
    if (!file) return false;
    const int written = std::fprintf(
        file, "brightness=%d\ncontrast=%u\nsaturation=%u\n",
        settings.brightness, settings.contrast, settings.saturation);
    const int closed = std::fclose(file);
    return written > 0 && closed == 0;
}

bool menu_controls_released(const controller_data_s& pad) {
    return !pad.s2_z && !pad.a && !pad.b && !pad.up && !pad.down &&
           !pad.left && !pad.right;
}

void configure_noods(const std::string& rom_path) {
    Settings::basePath = parent_path(rom_path);
    Settings::directBoot = 1;
    // Commercial games continuously stream small blocks from the cartridge.
    // LibXenon FAT I/O is both slower and less predictable than desktop stdio,
    // so preload the image once and execute from a verified in-memory copy.
    Settings::romInRam = 1;
    Settings::fpsLimiter = 0;
    // Preserve the exact full-frame path proven by the working v0.8.3 build.
    // Turbo changes this at runtime only after the user requests it.
    Settings::frameskip = 0;
    Settings::threaded2D = 0;
    // Leave the emulation thread alone on physical core 0. Three software-3D
    // workers use contexts 2/3/4, while presentation uses context 5.
    Settings::threaded3D = 3;
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

    std::printf("XenonDS NooDS validated Turbo v0.9.1\n");
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
    const std::string picture_path =
        parent_path(rom.path) + "/xenonds-display.cfg";
    xenonds::xenon::PictureSettings picture;
    if (load_picture_settings(picture_path, &picture))
        std::printf("Loaded picture settings: %s\n", picture_path.c_str());
    if (!xenonds::xenon::set_noods_picture_settings(picture)) {
        fail("The video worker did not accept the initial picture settings");
        return 1;
    }
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
    unsigned int completed_without_video = 0;
    unsigned int completed_without_submission = 0;
    std::uint64_t last_frame_tick = mftb();
    std::uint64_t last_completed_tick = last_frame_tick;
    std::uint64_t last_save_tick = last_frame_tick;
    std::uint64_t stats_window_tick = last_frame_tick;
    unsigned int scheduler_returns_without_frame = 0;
    unsigned long pending_core_microseconds = 0;
    unsigned long stats_core_microseconds = 0;
    unsigned int stats_emulated_frames = 0;
    unsigned int stats_video_frames = 0;
    xenonds::xenon::PerformanceStats performance;
    performance.picture = picture;
    xenonds::xenon::SpeedMode speed_mode =
        xenonds::xenon::SpeedMode::Normal;
    bool picture_menu_open = false;
    unsigned int picture_menu_item = 0;
    bool suppress_game_input = false;
    bool previous_left_trigger = false;
    bool previous_menu = false;
    bool previous_up = false;
    bool previous_down = false;
    bool previous_left = false;
    bool previous_right = false;
    bool previous_a = false;
    bool previous_b = false;
    bool have_frame = false;
    bool display_dirty = false;
    bool video_frame_pending = false;
    bool picture_settings_dirty = false;

    for (;;) {
        usb_do_poll();
        std::memset(&pad, 0, sizeof(pad));
        get_controller_data(&pad, 0);
        if (pad.logo) {
            xenonds::xenon::wait_for_noods_video();
            save_picture_settings(picture_path, picture);
            core->cartridgeNds.writeSave();
            delete core;
            return 0;
        }

        const bool left_trigger = pad.lt > 32;
        const bool menu_button = pad.s2_z != 0;
        const bool up = pad.up != 0;
        const bool down = pad.down != 0;
        const bool left = pad.left != 0;
        const bool right = pad.right != 0;
        const bool a = pad.a != 0;
        const bool b = pad.b != 0;

        if (left_trigger && !previous_left_trigger) {
            speed_mode = xenonds::xenon::toggle_speed_mode(speed_mode);
            // Rendering cadence is independent from emulated time. Turbo is
            // uncapped and hands off one complete rendered frame in four.
            core->gpu.requestFrameSkip(
                xenonds::xenon::speed_mode_config(speed_mode).frame_skip);

            // Begin a fresh measurement window with the new cadence. Mixing
            // pre- and post-switch frames can make Turbo appear slower for up
            // to a second even when completed-frame throughput increased.
            stats_window_tick = mftb();
            stats_core_microseconds = 0;
            stats_emulated_frames = 0;
            stats_video_frames = 0;
            performance.emulation_fps_tenths = 0;
            performance.video_fps_tenths = 0;
            performance.game_speed_percent = 0;
            performance.core_microseconds = 0;
            display_dirty = true;
        }

        if (menu_button && !previous_menu) {
            picture_menu_open = !picture_menu_open;
            suppress_game_input = true;
            display_dirty = true;
            if (!picture_menu_open)
                save_picture_settings(picture_path, picture);
        }

        if (picture_menu_open) {
            if (up && !previous_up)
                picture_menu_item = (picture_menu_item + 2) % 3;
            if (down && !previous_down)
                picture_menu_item = (picture_menu_item + 1) % 3;

            bool changed = false;
            if ((left && !previous_left) || (right && !previous_right)) {
                const int direction = right ? 1 : -1;
                if (picture_menu_item == 0) {
                    picture.brightness = clamp_value(
                        picture.brightness + direction * 4, -64, 64);
                }
                else if (picture_menu_item == 1) {
                    picture.contrast = static_cast<unsigned int>(clamp_value(
                        static_cast<int>(picture.contrast) + direction * 5,
                        50, 150));
                }
                else {
                    picture.saturation = static_cast<unsigned int>(clamp_value(
                        static_cast<int>(picture.saturation) + direction * 5,
                        0, 150));
                }
                changed = true;
            }
            if (a && !previous_a) {
                picture = xenonds::xenon::PictureSettings();
                changed = true;
            }
            if (changed)
                picture_settings_dirty = true;
            display_dirty = display_dirty || changed;
            if (b && !previous_b) {
                picture_menu_open = false;
                suppress_game_input = true;
                display_dirty = true;
                save_picture_settings(picture_path, picture);
            }
        }

        const xenonds::xenon::SpeedModeConfig mode =
            xenonds::xenon::speed_mode_config(speed_mode);
        performance.turbo_enabled = mode.turbo_enabled;
        performance.picture_menu_open = picture_menu_open;
        performance.picture_menu_item = picture_menu_item;
        performance.picture = picture;

        // A presentation may still be reading the active color table. Never
        // block emulation waiting for it: keep the update dirty and retry as
        // soon as the worker is idle.
        if (picture_settings_dirty &&
            xenonds::xenon::set_noods_picture_settings(picture, 0)) {
            picture_settings_dirty = false;
            display_dirty = true;
        }

        if (picture_menu_open || suppress_game_input) {
            input = xenonds::InputState();
            if (!picture_menu_open && menu_controls_released(pad))
                suppress_game_input = false;
        }
        else {
            input = mapper.map(snapshot_from_pad(pad));
        }

        previous_left_trigger = left_trigger;
        previous_menu = menu_button;
        previous_up = up;
        previous_down = down;
        previous_left = left;
        previous_right = right;
        previous_a = a;
        previous_b = b;

        apply_input(*core, input);
        const std::uint32_t completed_before = core->completedFrames;
        const std::uint64_t core_start_tick = mftb();
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
        const unsigned long core_elapsed =
            tb_diff_usec(mftb(), core_start_tick);
        pending_core_microseconds += core_elapsed;
        const std::uint32_t completed_count =
            core->completedFrames - completed_before;
        const bool frame_ready = core->gpu.getFrameXenon(ds_frame, false);
        if (frame_ready) {
            have_frame = true;
            completed_without_video = 0;
            display_dirty = true;
            video_frame_pending = true;
        }

        // Keep only the newest completed image when presentation overlaps the
        // next emulation slice. A busy worker is never waited on and never has
        // its queued frame overwritten; display_dirty makes this retry on the
        // next scheduler return.
        const bool submitted_frame =
            display_dirty && have_frame && !picture_settings_dirty &&
            xenonds::xenon::try_present_noods_frame(
                ds_frame, input.touch, performance);
        if (submitted_frame) {
            display_dirty = false;
            completed_without_submission = 0;
            if (video_frame_pending) {
                video_frame_pending = false;
                ++stats_video_frames;
            }
        }

        if (completed_count != 0) {
            // runCore() also returns when an emulated CPU halts or resumes.
            // Only Core::endFrame() owns cadence, statistics, and save timing.
            stats_core_microseconds += pending_core_microseconds;
            pending_core_microseconds = 0;
            scheduler_returns_without_frame = 0;
            last_completed_tick = mftb();
            stats_emulated_frames += completed_count;
            if (!frame_ready)
                completed_without_video += completed_count;
            if (!submitted_frame && display_dirty && have_frame)
                completed_without_submission += completed_count;

            const unsigned long window_microseconds =
                tb_diff_usec(mftb(), stats_window_tick);
            if (window_microseconds >= 1000000UL) {
                const unsigned int emulation_fps_tenths =
                    static_cast<unsigned int>(
                        (static_cast<unsigned long long>(stats_emulated_frames) *
                         10000000ULL + window_microseconds / 2) /
                        window_microseconds);
                performance.emulation_fps_tenths = emulation_fps_tenths;
                performance.video_fps_tenths = static_cast<unsigned int>(
                    (static_cast<unsigned long long>(stats_video_frames) *
                     10000000ULL + window_microseconds / 2) /
                    window_microseconds);
                performance.game_speed_percent = clamp_value(
                    (emulation_fps_tenths + 3u) / 6u, 0u, 999u);
                performance.core_microseconds = stats_emulated_frames != 0 ?
                    stats_core_microseconds / stats_emulated_frames : 0;

                stats_window_tick = mftb();
                stats_core_microseconds = 0;
                stats_emulated_frames = 0;
                stats_video_frames = 0;
            }

            const unsigned long elapsed =
                tb_diff_usec(mftb(), last_frame_tick);
            if (mode.frame_cap_microseconds != 0 &&
                elapsed < mode.frame_cap_microseconds) {
                udelay(static_cast<int>(
                    mode.frame_cap_microseconds - elapsed));
            }
            last_frame_tick = mftb();

            // Save against wall time, not emulated frames. Fast modes must not
            // turn a five-second checkpoint into FAT writes every 1.25s.
            if (tb_diff_usec(last_frame_tick, last_save_tick) >=
                kSaveIntervalMicroseconds) {
                core->cartridgeNds.writeSave();
                last_save_tick = last_frame_tick;
            }
        }
        else if (scheduler_returns_without_frame < kSchedulerReturnLimit) {
            ++scheduler_returns_without_frame;
        }

        const std::uint32_t unknown_opcodes =
            core->interpreter[0].unknownOpcodeCount +
            core->interpreter[1].unknownOpcodeCount;
        const bool scheduler_stalled =
            scheduler_returns_without_frame >= kSchedulerReturnLimit &&
            tb_diff_usec(mftb(), last_completed_tick) >=
                kFrameProgressTimeoutMicroseconds;
        if (unknown_opcodes >= kUnknownOpcodeLimit ||
            scheduler_stalled ||
            completed_without_video > kVideoHandoffLimit ||
            completed_without_submission > kVideoSubmissionLimit) {
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
                     : (scheduler_stalled
                            ? "The DS frame scheduler stopped making progress"
                        : (completed_without_video > kVideoHandoffLimit
                            ? "Video handoffs stopped while DS frames continued"
                            : "The video worker stopped accepting frames")));
            return 1;
        }

    }
}

// SPDX-License-Identifier: GPL-3.0-or-later

// Compile and execute the production frontend loop against a scripted core.
// Renaming main lets this file supply the host test entry point below.
#define main xenonds_noods_frontend_main
#include "../platform/xenon/noods/source/main.cpp"
#undef main

#include <cstdio>
#include <cstring>
#include <string>

namespace {

std::string rom_path;
unsigned int controller_reads = 0;
unsigned int pace_calls = 0;
unsigned long long paced_microseconds = 0;
unsigned int presentation_attempts = 0;
unsigned int nonuniform_presentations = 0;
unsigned int wait_calls = 0;
std::uint64_t mock_time = 0;

bool uniform_white(const std::uint16_t* pixels) {
    for (std::size_t i = 0; i < xenonds::kCombinedPixelCount; ++i) {
        if (pixels[i] != 0x7FFFu) return false;
    }
    return true;
}

int check(bool condition, const char* message) {
    if (condition) return 0;
    std::fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

} // namespace

extern "C" void xenos_init(int) {}
extern "C" void console_init(void) {}
extern "C" void xenon_make_it_faster(int) {}
extern "C" void xenon_thread_startup(void) {}
extern "C" int usb_init(void) { return 0; }
extern "C" void usb_do_poll(void) {}
extern "C" int xenon_ata_init(void) { return 0; }
extern "C" int xenon_atapi_init(void) { return 0; }
extern "C" int fatInitDefault(void) { return 1; }

extern "C" int get_controller_data(controller_data_s* data, int) {
    ++controller_reads;
    std::memset(data, 0, sizeof(*data));
    data->logo = controller_reads >
        noods_frontend_test::kSchedulerOnlyReturns +
        noods_frontend_test::kUniformFrames;
    // After two scheduler-only returns, run 200 normal frames, 200 Turbo
    // frames, then restore normal for the final 202 frames.
    data->lt = controller_reads == 203 || controller_reads == 403 ? 255 : 0;
    return 0;
}

extern "C" void udelay(int microseconds) {
    ++pace_calls;
    paced_microseconds += static_cast<unsigned int>(microseconds);
    mock_time += static_cast<unsigned int>(microseconds);
}

extern "C" std::uint64_t xenonds_frontend_test_mftb(void) {
    return mock_time;
}

namespace xenonds {
namespace xenon {

Status find_first_rom(FoundRom* output) {
    output->path = rom_path;
    output->file_size = 0x400;
    output->header.title = "UNIFORM TEST";
    output->header.game_code = "XNDS";
    return Status::Ok();
}

bool initialize_noods_video() { return true; }

bool set_noods_picture_settings(const PictureSettings&, unsigned long) {
    return true;
}

bool try_present_noods_frame(const std::uint16_t* pixels,
                             const TouchState&,
                             const PerformanceStats&) {
    ++presentation_attempts;
    if (!uniform_white(pixels)) ++nonuniform_presentations;
    return true;
}

bool wait_for_noods_video(unsigned long) {
    ++wait_calls;
    return true;
}

} // namespace xenon
} // namespace xenonds

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: noods_frontend_liveness ROM.nds\n");
        return 2;
    }
    rom_path = argv[1];

    int failures = 0;
    using xenonds::xenon::SpeedMode;
    using xenonds::xenon::toggle_speed_mode;
    using xenonds::xenon::speed_mode_config;
    const xenonds::xenon::SpeedModeConfig normal =
        speed_mode_config(SpeedMode::Normal);
    const xenonds::xenon::SpeedModeConfig turbo =
        speed_mode_config(SpeedMode::Turbo);
    failures += check(normal.frame_cap_microseconds == 16715 &&
                          normal.frame_skip == 0 && !normal.turbo_enabled,
                      "1X mode configuration changed");
    failures += check(turbo.frame_cap_microseconds == 0 &&
                          turbo.frame_skip == 3 && turbo.turbo_enabled,
                      "Turbo mode configuration changed");
    failures += check(
        toggle_speed_mode(SpeedMode::Normal) == SpeedMode::Turbo &&
            toggle_speed_mode(SpeedMode::Turbo) == SpeedMode::Normal,
        "LT normal/Turbo toggle changed");
    const int result = xenonds_noods_frontend_main();
    failures += check(result == 0,
                      "601+ legitimate uniform frames triggered a fatal exit");
    failures += check(
        noods_frontend_test::run_calls ==
            noods_frontend_test::kSchedulerOnlyReturns +
            noods_frontend_test::kUniformFrames,
        "frontend ran an unexpected number of scheduler events");
    failures += check(
        noods_frontend_test::completed_frames ==
            noods_frontend_test::kUniformFrames,
        "scheduler-only returns were counted as completed frames");
    failures += check(
        pace_calls == 402,
        "scheduler-only returns were paced as completed frames");
    const unsigned long long expected_pacing = 402ULL * 16715ULL;
    failures += check(paced_microseconds == expected_pacing,
                      "1X/2X/4X frame pacing duration changed unexpectedly");
    failures += check(
        noods_frontend_test::skip_request_count == 2 &&
            noods_frontend_test::skip_requests[0] == 3 &&
            noods_frontend_test::skip_requests[1] == 0,
        "LT did not request the exact normal/Turbo 0->3->0 cadence");
    failures += check(
        noods_frontend_test::frame_handoffs == 452,
        "Turbo did not hand off exactly one complete frame in four");
    failures += check(
        presentation_attempts == 452 &&
            nonuniform_presentations == 0,
        "frontend did not present the exact scripted white-frame cadence");
    failures += check(
        noods_frontend_test::save_calls == 2 &&
            noods_frontend_test::save_completed_frames[0] == 500 &&
            noods_frontend_test::save_completed_frames[1] ==
                noods_frontend_test::kUniformFrames,
        "save cadence was not based on five seconds of wall time");
    failures += check(wait_calls == 1,
                      "clean exit did not wait for the final video task");

    if (failures != 0) return 1;
    std::printf(
        "PASS frontend-liveness uniform=%u scheduler_only=%u paced=%u "
        "modes=NORMAL,TURBO handoffs=%u saves=%u\n",
        noods_frontend_test::kUniformFrames,
        noods_frontend_test::kSchedulerOnlyReturns,
        pace_calls, noods_frontend_test::frame_handoffs,
        noods_frontend_test::save_calls);
    return 0;
}

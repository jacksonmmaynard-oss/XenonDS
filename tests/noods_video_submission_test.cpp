// SPDX-License-Identifier: GPL-3.0-or-later

// Include the implementation so this host test can verify that a Busy result
// leaves the anonymous-namespace producer/consumer queue completely intact.
#include "../platform/xenon/noods/source/noods_video.cpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

int failures = 0;
int worker_busy = 0;
int launch_result = 0;
unsigned int running_queries = 0;
unsigned int launch_calls = 0;
std::uint64_t mock_time = 0;
std::size_t flushed_bytes = 0;

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "%s:%d CHECK failed: %s\n", \
                     __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

void reset_calls() {
    running_queries = 0;
    launch_calls = 0;
    mock_time = 0;
}

std::uint64_t hash_pixels(const std::vector<std::uint32_t>& pixels) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (std::uint32_t value : pixels) {
        hash ^= value;
        hash *= 1099511628211ULL;
    }
    return hash;
}

void test_metric_formatting() {
    using namespace xenonds::xenon;
    char text[10];
    format_metric(text, sizeof(text), 'E', 'M', 'U', 450);
    CHECK(std::strcmp(text, "EMU 045.0") == 0);
    format_metric(text, sizeof(text), 'E', 'M', 'U', 1197);
    CHECK(std::strcmp(text, "EMU 119.7") == 0);
    format_metric(text, sizeof(text), 'E', 'M', 'U', 2393);
    CHECK(std::strcmp(text, "EMU 239.3") == 0);
}

bool queued_stats_equal(const xenonds::xenon::PerformanceStats& expected) {
    using xenonds::xenon::queued_stats;
    return queued_stats.emulation_fps_tenths ==
               expected.emulation_fps_tenths &&
           queued_stats.video_fps_tenths == expected.video_fps_tenths &&
           queued_stats.game_speed_percent == expected.game_speed_percent &&
           queued_stats.core_microseconds == expected.core_microseconds &&
           queued_stats.turbo_enabled == expected.turbo_enabled &&
           queued_stats.picture_menu_open == expected.picture_menu_open &&
           queued_stats.picture_menu_item == expected.picture_menu_item &&
           queued_stats.picture.brightness == expected.picture.brightness &&
           queued_stats.picture.contrast == expected.picture.contrast &&
           queued_stats.picture.saturation == expected.picture.saturation;
}

void test_wait_is_bounded() {
    using namespace xenonds::xenon;

    initialized = false;
    worker_busy = 1;
    reset_calls();
    CHECK(wait_for_noods_video(0));
    CHECK(running_queries == 0);

    initialized = true;
    reset_calls();
    CHECK(!wait_for_noods_video(0));
    CHECK(running_queries == 1);

    worker_busy = 0;
    reset_calls();
    CHECK(wait_for_noods_video(0));
    CHECK(running_queries == 1);

    worker_busy = 1;
    reset_calls();
    CHECK(!wait_for_noods_video(3));
    CHECK(running_queries == 3);
}

void test_busy_submission_preserves_queue() {
    using namespace xenonds;
    using namespace xenonds::xenon;

    initialized = true;
    std::fill(queued_frame, queued_frame + kCombinedPixelCount, 0x11223344u);
    queued_touch.pressed = true;
    queued_touch.x = 17;
    queued_touch.y = 29;
    queued_stats.emulation_fps_tenths = 654;
    queued_stats.video_fps_tenths = 321;
    queued_stats.game_speed_percent = 54;
    queued_stats.core_microseconds = 9876;
    queued_stats.turbo_enabled = true;
    queued_stats.picture_menu_open = true;
    queued_stats.picture_menu_item = 2;
    queued_stats.picture.brightness = -12;
    queued_stats.picture.contrast = 137;
    queued_stats.picture.saturation = 41;

    const std::vector<std::uint16_t> queue_before(
        queued_frame, queued_frame + kCombinedPixelCount);
    const TouchState touch_before = queued_touch;
    const PerformanceStats stats_before = queued_stats;

    std::vector<std::uint16_t> incoming(kCombinedPixelCount);
    for (std::size_t i = 0; i < incoming.size(); ++i)
        incoming[i] = static_cast<std::uint16_t>(i & 0x7FFFu);
    TouchState incoming_touch;
    incoming_touch.pressed = false;
    incoming_touch.x = 222;
    incoming_touch.y = 111;
    PerformanceStats incoming_stats;
    incoming_stats.emulation_fps_tenths = 1197;
    incoming_stats.video_fps_tenths = 599;
    incoming_stats.game_speed_percent = 100;
    incoming_stats.core_microseconds = 1234;
    incoming_stats.picture.brightness = 24;

    worker_busy = 1;
    reset_calls();
    CHECK(!try_present_noods_frame(
        incoming.data(), incoming_touch, incoming_stats));
    CHECK(running_queries == 1);
    CHECK(launch_calls == 0);
    CHECK(std::equal(queue_before.begin(), queue_before.end(), queued_frame));
    CHECK(queued_touch.pressed == touch_before.pressed);
    CHECK(queued_touch.x == touch_before.x);
    CHECK(queued_touch.y == touch_before.y);
    CHECK(queued_stats_equal(stats_before));

    worker_busy = 0;
    launch_result = 0;
    reset_calls();
    CHECK(try_present_noods_frame(
        incoming.data(), incoming_touch, incoming_stats));
    CHECK(running_queries == 1);
    CHECK(launch_calls == 1);
    CHECK(std::equal(incoming.begin(), incoming.end(), queued_frame));
    CHECK(queued_touch.pressed == incoming_touch.pressed);
    CHECK(queued_touch.x == incoming_touch.x);
    CHECK(queued_touch.y == incoming_touch.y);
    CHECK(queued_stats_equal(incoming_stats));

    launch_result = 7;
    reset_calls();
    CHECK(!try_present_noods_frame(
        incoming.data(), incoming_touch, incoming_stats));
    CHECK(running_queries == 1);
    CHECK(launch_calls == 1);

    initialized = false;
    reset_calls();
    CHECK(!try_present_noods_frame(
        incoming.data(), incoming_touch, incoming_stats));
    CHECK(!try_present_noods_frame(nullptr, incoming_touch, incoming_stats));
    CHECK(running_queries == 0);
    CHECK(launch_calls == 0);
}

void test_busy_settings_preserve_lut() {
    using namespace xenonds::xenon;

    PictureSettings settings;
    settings.brightness = 44;
    settings.contrast = 123;
    settings.saturation = 67;
    std::fill(color_lut, color_lut + 32u * 32u * 32u, 0xDEADBEEFu);

    initialized = false;
    reset_calls();
    CHECK(!set_noods_picture_settings(settings, 0));
    CHECK(running_queries == 0);
    CHECK(std::all_of(color_lut, color_lut + 32u * 32u * 32u,
                      [](std::uint32_t value) {
                          return value == 0xDEADBEEFu;
                      }));

    initialized = true;
    worker_busy = 1;
    reset_calls();
    CHECK(!set_noods_picture_settings(settings, 0));
    CHECK(running_queries == 1);
    CHECK(std::all_of(color_lut, color_lut + 32u * 32u * 32u,
                      [](std::uint32_t value) {
                          return value == 0xDEADBEEFu;
                      }));

    worker_busy = 0;
    reset_calls();
    CHECK(set_noods_picture_settings(settings, 0));
    CHECK(running_queries == 1);
    CHECK(std::any_of(color_lut, color_lut + 32u * 32u * 32u,
                      [](std::uint32_t value) {
                          return value != 0xDEADBEEFu;
                      }));
}

void test_presenter_sequence_is_exact() {
    using namespace xenonds;
    using namespace xenonds::xenon;

    std::vector<std::uint32_t> pixels(640u * 480u);
    std::vector<std::uint16_t> source(kCombinedPixelCount);
    for (std::size_t i = 0; i < source.size(); ++i)
        source[i] = static_cast<std::uint16_t>((i * 761u) & 0x7FFFu);

    output = {pixels.data(), 640, 480, 640, 480};
    PictureSettings picture;
    rebuild_color_lut(picture);
    TouchState touch;
    PerformanceStats stats;
    stats.emulation_fps_tenths = 220;
    stats.video_fps_tenths = 218;
    stats.game_speed_percent = 37;

    const auto check_present = [&](std::uint64_t expected_hash,
                                   std::size_t expected_flush) {
        flushed_bytes = 0;
        present_immediate(source.data(), touch, stats);
        CHECK(hash_pixels(pixels) == expected_hash);
        CHECK(flushed_bytes == expected_flush);
    };

    check_present(0x3FF16FE21ED40083ULL, 1228800u);
    source[0] ^= 0x7FFFu;
    check_present(0x4D60081721C70A83ULL, 516096u);

    stats.emulation_fps_tenths = 231;
    stats.video_fps_tenths = 229;
    stats.game_speed_percent = 39;
    check_present(0xAD883BC4FC5B4183ULL, 589824u);

    stats.picture_menu_open = true;
    stats.picture_menu_item = 1;
    check_present(0x3D731B30D5D00083ULL, 835584u);
    source[70000] ^= 0x1234u;
    check_present(0xF431F65722C09B83ULL, 835584u);

    stats.picture.brightness = 20;
    stats.picture.contrast = 95;
    stats.picture.saturation = 80;
    rebuild_color_lut(stats.picture);
    check_present(0x22CC6E8A607E1383ULL, 835584u);

    // Closing the menu must restore the matte and both DS screens instead of
    // leaving the overlay hidden behind a later screen draw.
    stats.picture_menu_open = false;
    check_present(0x6AA52DE822481583ULL, 835584u);

    stats.turbo_enabled = true;
    stats.emulation_fps_tenths = 440;
    stats.video_fps_tenths = 110;
    stats.game_speed_percent = 73;
    check_present(0x6CE1CF5BA04CEF83ULL, 589824u);

    // Repeated open/close cycles must restore the complete framebuffer, not
    // merely hide the menu underneath a later DS screen draw.
    const std::vector<std::uint32_t> clean = pixels;
    for (unsigned int cycle = 0; cycle < 100; ++cycle) {
        stats.picture_menu_open = true;
        stats.picture_menu_item = cycle % 3;
        present_immediate(source.data(), touch, stats);
        CHECK(pixels != clean);

        stats.picture_menu_open = false;
        present_immediate(source.data(), touch, stats);
        CHECK(pixels == clean);
    }

    // A legitimate all-white game frame is content, not a stall signal.
    std::fill(source.begin(), source.end(), static_cast<std::uint16_t>(0x7FFFu));
    present_immediate(source.data(), touch, stats);
    CHECK(pixels[tiled_index(56, 144, 640)] == xenos_color(0x7FFFu));
    CHECK(pixels[tiled_index(311, 335, 640)] == xenos_color(0x7FFFu));
    // The inactive touch cursor intentionally marks (328,144); sample away
    // from its crosshair while checking the second screen.
    CHECK(pixels[tiled_index(340, 144, 640)] == xenos_color(0x7FFFu));
    CHECK(pixels[tiled_index(583, 335, 640)] == xenos_color(0x7FFFu));
}

} // namespace

extern "C" std::uint64_t noods_video_test_mftb(void) {
    return mock_time++;
}

extern "C" int xenon_is_thread_task_running(int thread) {
    CHECK(thread == 5);
    ++running_queries;
    return worker_busy;
}

extern "C" int xenon_run_thread_task(int thread, void* stack, void* task) {
    CHECK(thread == 5);
    CHECK(stack != nullptr);
    CHECK(task != nullptr);
    ++launch_calls;
    return launch_result;
}

extern "C" void memdcbst(void*, int bytes) {
    if (bytes > 0)
        flushed_bytes += static_cast<std::size_t>(bytes);
}

int main() {
    test_metric_formatting();
    test_wait_is_bounded();
    test_busy_submission_preserves_queue();
    test_busy_settings_preserve_lut();
    test_presenter_sequence_is_exact();
    if (failures != 0) return 1;
    std::printf("PASS video-submission busy-preserves-queue bounded-waits\n");
    return 0;
}

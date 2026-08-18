// SPDX-License-Identifier: GPL-3.0-or-later
#include "noods_video.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

extern "C" {
#include <ppc/cache.h>
#include <xenon_soc/xenon_power.h>
}

namespace xenonds {
namespace xenon {
namespace {

struct Framebuffer {
    std::uint32_t* pixels;
    int visible_width;
    int visible_height;
    int padded_width;
    int padded_height;
};

Framebuffer output = {};
bool initialized = false;
const int kVideoThread = 2;
alignas(16) unsigned char worker_stack[64 * 1024];
std::uint32_t queued_frame[kCombinedPixelCount];
TouchState queued_touch;
std::vector<std::uint32_t> staging;

std::size_t tiled_index(int x, int y, int width) {
    return static_cast<std::size_t>(
        ((y >> 5) * 32 * width + ((x >> 5) << 10) +
         (x & 3) + ((y & 1) << 2) + (((x & 31) >> 2) << 3) +
         (((y & 31) >> 1) << 6)) ^ ((y & 8) << 2));
}

void put_pixel(const Framebuffer& framebuffer, int x, int y, std::uint32_t color) {
    if (x < 0 || y < 0 || x >= framebuffer.visible_width ||
        y >= framebuffer.visible_height) {
        return;
    }
    framebuffer.pixels[tiled_index(x, y, framebuffer.padded_width)] = color;
}

std::uint32_t xenos_color(std::uint32_t rgb8) {
    // NooDS returns 0xAABBGGRR. Xenos' tiled framebuffer uses 0xBBGGRR00.
    return rgb8 << 8u;
}

void draw_screen(const Framebuffer& framebuffer,
                 const std::uint32_t* source,
                 int destination_x,
                 int destination_y,
                 int scale) {
    for (std::size_t source_y = 0; source_y < kScreenHeight; ++source_y) {
        for (std::size_t source_x = 0; source_x < kScreenWidth; ++source_x) {
            const std::uint32_t color = xenos_color(
                source[source_y * kScreenWidth + source_x]);
            const int base_x = destination_x + static_cast<int>(source_x) * scale;
            const int base_y = destination_y + static_cast<int>(source_y) * scale;
            for (int y = 0; y < scale; ++y) {
                for (int x = 0; x < scale; ++x) {
                    put_pixel(framebuffer, base_x + x, base_y + y, color);
                }
            }
        }
    }
}

void draw_touch_cursor(const Framebuffer& framebuffer,
                       const TouchState& touch,
                       int destination_x,
                       int destination_y,
                       int scale) {
    const int center_x = destination_x + static_cast<int>(touch.x) * scale;
    const int center_y = destination_y + static_cast<int>(touch.y) * scale;
    const std::uint32_t color = touch.pressed ? 0x0000FF00u : 0x00FFFF00u;
    for (int offset = -5; offset <= 5; ++offset) {
        put_pixel(framebuffer, center_x + offset, center_y, color);
        put_pixel(framebuffer, center_x, center_y + offset, color);
    }
}

void present_immediate(const std::uint32_t* source, const TouchState& touch) {
    std::fill(staging.begin(), staging.end(), 0x090B1000u);
    Framebuffer framebuffer = output;
    framebuffer.pixels = staging.data();

    const int gap = 16;
    const int maximum_scale_x = (framebuffer.visible_width - gap) /
                                static_cast<int>(kScreenWidth * 2);
    const int maximum_scale_y = framebuffer.visible_height /
                                static_cast<int>(kScreenHeight);
    const int scale = std::max(1, std::min(3, std::min(maximum_scale_x, maximum_scale_y)));
    const int total_width = static_cast<int>(kScreenWidth * 2) * scale + gap;
    const int total_height = static_cast<int>(kScreenHeight) * scale;
    const int top_x = (framebuffer.visible_width - total_width) / 2;
    const int top_y = (framebuffer.visible_height - total_height) / 2;
    const int touch_x = top_x + static_cast<int>(kScreenWidth) * scale + gap;

    draw_screen(framebuffer, source, top_x, top_y, scale);
    draw_screen(framebuffer, source + kScreenWidth * kScreenHeight,
                touch_x, top_y, scale);
    draw_touch_cursor(framebuffer, touch, touch_x, top_y, scale);

    const std::size_t byte_count = staging.size() * sizeof(std::uint32_t);
    std::memcpy(output.pixels, staging.data(), byte_count);
    memdcbst(output.pixels, static_cast<int>(byte_count));
}

extern "C" void xenonds_noods_video_worker() {
    __sync_synchronize();
    present_immediate(queued_frame, queued_touch);
    __sync_synchronize();
}

} // namespace

bool initialize_noods_video() {
    volatile const std::uint32_t* info =
        reinterpret_cast<volatile const std::uint32_t*>(0xec806100ULL);
    const std::uint32_t base = info[4];
    const std::uint32_t width = info[13];
    const std::uint32_t height = info[14];
    if (base == 0 || width < 160u || height < 100u) {
        return false;
    }

    output.pixels = reinterpret_cast<std::uint32_t*>(
        static_cast<unsigned long>(base | 0x80000000u));
    output.visible_width = static_cast<int>(width);
    output.visible_height = static_cast<int>(height);
    output.padded_width = (output.visible_width + 31) & ~31;
    output.padded_height = (output.visible_height + 31) & ~31;
    staging.resize(static_cast<std::size_t>(output.padded_width) *
                   output.padded_height);
    initialized = true;
    return true;
}

void wait_for_noods_video() {
    if (!initialized) return;
    while (xenon_is_thread_task_running(kVideoThread) != 0) {
    }
    __sync_synchronize();
}

void present_noods_frame(const std::uint32_t* pixels, const TouchState& touch) {
    if (!initialized || !pixels) return;

    wait_for_noods_video();
    std::memcpy(queued_frame, pixels, sizeof(queued_frame));
    queued_touch = touch;
    __sync_synchronize();

    void* stack_top = worker_stack + sizeof(worker_stack) - 256;
    while (xenon_run_thread_task(
               kVideoThread, stack_top,
               reinterpret_cast<void*>(&xenonds_noods_video_worker)) != 0) {
    }
}

} // namespace xenon
} // namespace xenonds

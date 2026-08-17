// SPDX-License-Identifier: GPL-2.0-only
#include "xenon_video.hpp"

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

Framebuffer cached_output = {};
bool output_is_initialized = false;
const int kVideoThread = 2; // A different physical core from the main thread.
alignas(16) unsigned char video_worker_stack[64 * 1024];
FrameOutput queued_frame;
TouchState queued_touch;
std::vector<std::uint32_t> staging;

std::size_t tiled_index(int x, int y, int width) {
    return static_cast<std::size_t>(
        ((y >> 5) * 32 * width + ((x >> 5) << 10) +
         (x & 3) + ((y & 1) << 2) + (((x & 31) >> 2) << 3) +
         (((y & 31) >> 1) << 6)) ^ ((y & 8) << 2));
}

std::uint8_t expand_five_bits(std::uint16_t value) {
    const std::uint8_t component = static_cast<std::uint8_t>(value & 0x1Fu);
    return static_cast<std::uint8_t>((component << 3u) | (component >> 2u));
}

std::uint32_t framebuffer_color(std::uint16_t bgr555) {
    const std::uint8_t red = expand_five_bits(bgr555);
    const std::uint8_t green = expand_five_bits(static_cast<std::uint16_t>(bgr555 >> 5u));
    const std::uint8_t blue = expand_five_bits(static_cast<std::uint16_t>(bgr555 >> 10u));
    return (static_cast<std::uint32_t>(blue) << 24u) |
           (static_cast<std::uint32_t>(green) << 16u) |
           (static_cast<std::uint32_t>(red) << 8u);
}

void put_pixel(const Framebuffer& framebuffer, int x, int y, std::uint32_t color) {
    if (x < 0 || y < 0 || x >= framebuffer.visible_width ||
        y >= framebuffer.visible_height) {
        return;
    }
    framebuffer.pixels[tiled_index(x, y, framebuffer.padded_width)] = color;
}

void draw_screen(const Framebuffer& framebuffer,
                 const std::uint16_t* source,
                 int destination_x,
                 int destination_y,
                 int scale) {
    for (std::size_t source_y = 0; source_y < kScreenHeight; ++source_y) {
        for (std::size_t source_x = 0; source_x < kScreenWidth; ++source_x) {
            const std::uint32_t color = framebuffer_color(
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
    for (int offset = -6; offset <= 6; ++offset) {
        put_pixel(framebuffer, center_x + offset, center_y, color);
        put_pixel(framebuffer, center_x, center_y + offset, color);
    }
}

void present_ds_frame_immediate(const std::uint16_t* source,
                                const TouchState& touch) {
    const Framebuffer output = cached_output;
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

extern "C" void xenonds_video_worker() {
    // The main thread does not touch queued_frame until this task returns.
    __sync_synchronize();
    present_ds_frame_immediate(queued_frame.pixels.data(), queued_touch);
    __sync_synchronize();
}

} // namespace

bool initialize_video_presenter() {
    // These MMIO fields sit at offsets that are valid for 32-bit reads but not
    // for a combined 64-bit read. Volatile word accesses stop GCC from merging
    // width and height into the unaligned load that caused the v0.3.5 DSI.
    volatile const std::uint32_t* info =
        reinterpret_cast<volatile const std::uint32_t*>(0xec806100ULL);
    const std::uint32_t base = info[4];
    const std::uint32_t width = info[13];
    const std::uint32_t height = info[14];
    if (base == 0 || width < 160u || height < 100u) {
        return false;
    }

    cached_output.pixels = reinterpret_cast<std::uint32_t*>(
        static_cast<unsigned long>(base | 0x80000000u));
    cached_output.visible_width = static_cast<int>(width);
    cached_output.visible_height = static_cast<int>(height);
    cached_output.padded_width = (cached_output.visible_width + 31) & ~31;
    cached_output.padded_height = (cached_output.visible_height + 31) & ~31;
    staging.resize(static_cast<std::size_t>(cached_output.padded_width) *
                   cached_output.padded_height);
    output_is_initialized = true;
    return true;
}

void wait_for_video_presenter() {
    if (!output_is_initialized) {
        return;
    }
    while (xenon_is_thread_task_running(kVideoThread) != 0) {
        // A frame conversion is only ~7 ms and normally completes while the
        // next DS frame is executing on the main core.
    }
    __sync_synchronize();
}

void present_ds_frame(const FrameOutput& frame, const TouchState& touch) {
    if (!output_is_initialized || frame.pixels.size() < kCombinedPixelCount) {
        return;
    }

    wait_for_video_presenter();
    std::memcpy(queued_frame.pixels.data(), frame.pixels.data(),
                kCombinedPixelCount * sizeof(std::uint16_t));
    queued_touch = touch;
    __sync_synchronize();

    void* stack_top = video_worker_stack + sizeof(video_worker_stack) - 256;
    while (xenon_run_thread_task(
               kVideoThread, stack_top,
               reinterpret_cast<void*>(&xenonds_video_worker)) != 0) {
    }
}

} // namespace xenon
} // namespace xenonds

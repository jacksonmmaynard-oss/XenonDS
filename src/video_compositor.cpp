// SPDX-License-Identifier: GPL-2.0-only
#include "xenonds/video_compositor.hpp"

#include <algorithm>
#include <cstddef>

namespace xenonds {
namespace {

std::uint8_t expand_five_bits(std::uint16_t value) {
    const std::uint8_t component = static_cast<std::uint8_t>(value & 0x1Fu);
    return static_cast<std::uint8_t>((component << 3u) | (component >> 2u));
}

void draw_screen(const std::uint16_t* source,
                 const ScreenRect& destination,
                 std::uint32_t output_width,
                 std::uint32_t output_height,
                 std::vector<std::uint32_t>* output) {
    if (destination.width <= 0 || destination.height <= 0) {
        return;
    }

    for (int y = 0; y < destination.height; ++y) {
        const int output_y = destination.y + y;
        if (output_y < 0 || output_y >= static_cast<int>(output_height)) {
            continue;
        }
        const std::size_t source_y = static_cast<std::size_t>(y) * kScreenHeight /
                                     static_cast<std::size_t>(destination.height);
        for (int x = 0; x < destination.width; ++x) {
            const int output_x = destination.x + x;
            if (output_x < 0 || output_x >= static_cast<int>(output_width)) {
                continue;
            }
            const std::size_t source_x = static_cast<std::size_t>(x) * kScreenWidth /
                                         static_cast<std::size_t>(destination.width);
            const std::size_t source_index = source_y * kScreenWidth + source_x;
            const std::size_t output_index = static_cast<std::size_t>(output_y) * output_width +
                                             static_cast<std::size_t>(output_x);
            (*output)[output_index] = bgr555_to_xrgb8888(source[source_index]);
        }
    }
}

} // namespace

std::uint32_t bgr555_to_xrgb8888(std::uint16_t pixel) {
    const std::uint8_t red = expand_five_bits(pixel);
    const std::uint8_t green = expand_five_bits(static_cast<std::uint16_t>(pixel >> 5u));
    const std::uint8_t blue = expand_five_bits(static_cast<std::uint16_t>(pixel >> 10u));
    return 0xFF000000u |
           (static_cast<std::uint32_t>(red) << 16u) |
           (static_cast<std::uint32_t>(green) << 8u) |
           static_cast<std::uint32_t>(blue);
}

Status compose_frame_nearest(const FrameOutput& frame,
                             const ScreenPlacement& placement,
                             std::uint32_t output_width,
                             std::uint32_t output_height,
                             std::uint32_t background_color,
                             std::vector<std::uint32_t>* output_pixels) {
    if (output_pixels == 0 || output_width == 0 || output_height == 0) {
        return Status(ErrorCode::invalid_argument, "A non-empty compositor output is required");
    }
    if (frame.pixels.size() < kCombinedPixelCount) {
        return Status(ErrorCode::invalid_argument, "The frame does not contain two complete DS screens");
    }

    const std::uint64_t pixel_count = static_cast<std::uint64_t>(output_width) * output_height;
    if (pixel_count > static_cast<std::uint64_t>(output_pixels->max_size())) {
        return Status(ErrorCode::invalid_argument, "The requested compositor output is too large");
    }
    output_pixels->assign(static_cast<std::size_t>(pixel_count), background_color);

    if (placement.show_top) {
        draw_screen(frame.pixels.data(), placement.top,
                    output_width, output_height, output_pixels);
    }
    if (placement.show_bottom) {
        draw_screen(frame.pixels.data() + kScreenWidth * kScreenHeight,
                    placement.bottom, output_width, output_height, output_pixels);
    }
    return Status::Ok();
}

} // namespace xenonds

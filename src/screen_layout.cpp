// SPDX-License-Identifier: GPL-2.0-only
#include "xenonds/screen_layout.hpp"

#include "xenonds/types.hpp"

#include <algorithm>

namespace xenonds {
namespace {

ScreenRect centered_rect(std::uint32_t canvas_width,
                         std::uint32_t canvas_height,
                         std::uint32_t width,
                         std::uint32_t height) {
    ScreenRect rect;
    rect.x = static_cast<int>((canvas_width - width) / 2);
    rect.y = static_cast<int>((canvas_height - height) / 2);
    rect.width = static_cast<int>(width);
    rect.height = static_cast<int>(height);
    return rect;
}

} // namespace

Status calculate_screen_placement(std::uint32_t output_width,
                                  std::uint32_t output_height,
                                  ScreenLayoutMode mode,
                                  std::uint32_t gap,
                                  ScreenPlacement* output) {
    if (output == 0 || output_width == 0 || output_height == 0) {
        return Status(ErrorCode::invalid_argument, "A non-empty output surface is required");
    }

    const std::uint32_t native_width = static_cast<std::uint32_t>(kScreenWidth);
    const std::uint32_t native_height = static_cast<std::uint32_t>(kScreenHeight);
    std::uint32_t scale = 0;
    ScreenPlacement placement;

    if (mode == ScreenLayoutMode::vertical) {
        if (output_height <= gap) {
            return Status(ErrorCode::invalid_argument, "The screen gap is larger than the output surface");
        }
        scale = std::min(output_width / native_width,
                         (output_height - gap) / (native_height * 2u));
        if (scale == 0) {
            return Status(ErrorCode::invalid_argument, "The output surface is too small for two DS screens");
        }
        const std::uint32_t width = native_width * scale;
        const std::uint32_t height = native_height * scale;
        const std::uint32_t total_height = height * 2u + gap;
        const int x = static_cast<int>((output_width - width) / 2u);
        const int y = static_cast<int>((output_height - total_height) / 2u);
        placement.top.x = x;
        placement.top.y = y;
        placement.top.width = static_cast<int>(width);
        placement.top.height = static_cast<int>(height);
        placement.bottom = placement.top;
        placement.bottom.y += static_cast<int>(height + gap);
        placement.show_top = true;
        placement.show_bottom = true;
    } else if (mode == ScreenLayoutMode::horizontal) {
        if (output_width <= gap) {
            return Status(ErrorCode::invalid_argument, "The screen gap is larger than the output surface");
        }
        scale = std::min((output_width - gap) / (native_width * 2u),
                         output_height / native_height);
        if (scale == 0) {
            return Status(ErrorCode::invalid_argument, "The output surface is too small for two DS screens");
        }
        const std::uint32_t width = native_width * scale;
        const std::uint32_t height = native_height * scale;
        const std::uint32_t total_width = width * 2u + gap;
        const int x = static_cast<int>((output_width - total_width) / 2u);
        const int y = static_cast<int>((output_height - height) / 2u);
        placement.top.x = x;
        placement.top.y = y;
        placement.top.width = static_cast<int>(width);
        placement.top.height = static_cast<int>(height);
        placement.bottom = placement.top;
        placement.bottom.x += static_cast<int>(width + gap);
        placement.show_top = true;
        placement.show_bottom = true;
    } else {
        scale = std::min(output_width / native_width, output_height / native_height);
        if (scale == 0) {
            return Status(ErrorCode::invalid_argument, "The output surface is too small for a DS screen");
        }
        const ScreenRect rect = centered_rect(output_width,
                                              output_height,
                                              native_width * scale,
                                              native_height * scale);
        placement.top = rect;
        placement.bottom = rect;
        placement.show_top = mode == ScreenLayoutMode::top_only;
        placement.show_bottom = mode == ScreenLayoutMode::bottom_only;
    }

    placement.integer_scale = scale;
    *output = placement;
    return Status::Ok();
}

} // namespace xenonds

// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "xenonds/status.hpp"

#include <cstdint>

namespace xenonds {

enum class ScreenLayoutMode {
    vertical,
    horizontal,
    top_only,
    bottom_only
};

struct ScreenRect {
    int x;
    int y;
    int width;
    int height;

    ScreenRect() : x(0), y(0), width(0), height(0) {}
};

struct ScreenPlacement {
    ScreenRect top;
    ScreenRect bottom;
    bool show_top;
    bool show_bottom;
    std::uint32_t integer_scale;

    ScreenPlacement() : show_top(false), show_bottom(false), integer_scale(0) {}
};

Status calculate_screen_placement(std::uint32_t output_width,
                                  std::uint32_t output_height,
                                  ScreenLayoutMode mode,
                                  std::uint32_t gap,
                                  ScreenPlacement* output);

} // namespace xenonds

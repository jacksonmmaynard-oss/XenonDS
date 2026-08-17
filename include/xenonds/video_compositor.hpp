// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "xenonds/screen_layout.hpp"
#include "xenonds/status.hpp"
#include "xenonds/types.hpp"

#include <cstdint>
#include <vector>

namespace xenonds {

std::uint32_t bgr555_to_xrgb8888(std::uint16_t pixel);

Status compose_frame_nearest(const FrameOutput& frame,
                             const ScreenPlacement& placement,
                             std::uint32_t output_width,
                             std::uint32_t output_height,
                             std::uint32_t background_color,
                             std::vector<std::uint32_t>* output_pixels);

} // namespace xenonds

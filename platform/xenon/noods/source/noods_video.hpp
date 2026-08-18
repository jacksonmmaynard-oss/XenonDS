// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xenonds/types.hpp"

#include <cstdint>

namespace xenonds {
namespace xenon {

struct PerformanceStats {
    unsigned int video_fps_tenths;
    unsigned int emulation_fps_tenths;
    unsigned long core_microseconds;

    PerformanceStats(): video_fps_tenths(0), emulation_fps_tenths(0),
                        core_microseconds(0) {}
};

bool initialize_noods_video();
void present_noods_frame(const std::uint32_t* pixels, const TouchState& touch,
                         const PerformanceStats& stats);
void wait_for_noods_video();

} // namespace xenon
} // namespace xenonds

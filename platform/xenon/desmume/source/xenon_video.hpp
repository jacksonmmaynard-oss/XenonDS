// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "xenonds/types.hpp"

namespace xenonds {
namespace xenon {

bool initialize_video_presenter();
void present_ds_frame(const FrameOutput& frame, const TouchState& touch);

} // namespace xenon
} // namespace xenonds

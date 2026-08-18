// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xenonds/types.hpp"

#include <cstdint>

namespace xenonds {
namespace xenon {

bool initialize_noods_video();
void present_noods_frame(const std::uint32_t* pixels, const TouchState& touch);
void wait_for_noods_video();

} // namespace xenon
} // namespace xenonds

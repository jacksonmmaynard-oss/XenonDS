// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <cstddef>
#include <cstdint>

namespace xenonds {

std::uint16_t crc16_nintendo(const std::uint8_t* data,
                             std::size_t size,
                             std::uint16_t seed = 0xFFFFu);

} // namespace xenonds


// SPDX-License-Identifier: GPL-2.0-or-later
#include "xenonds/checksum.hpp"

namespace xenonds {

std::uint16_t crc16_nintendo(const std::uint8_t* data,
                             std::size_t size,
                             std::uint16_t seed) {
    std::uint16_t crc = seed;
    if (data == 0) {
        return crc;
    }

    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            const bool carry = (crc & 1u) != 0;
            crc = static_cast<std::uint16_t>(crc >> 1u);
            if (carry) {
                crc ^= 0xA001u;
            }
        }
    }
    return crc;
}

} // namespace xenonds

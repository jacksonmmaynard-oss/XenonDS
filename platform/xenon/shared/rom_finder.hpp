// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "xenonds/nds_header.hpp"
#include "xenonds/status.hpp"

#include <array>
#include <cstddef>
#include <string>

namespace xenonds {
namespace xenon {

struct FoundRom {
    std::string path;
    std::size_t file_size;
    NdsHeader header;
    std::array<std::uint8_t, kNdsMinimumHeaderSize> header_bytes;

    FoundRom() : file_size(0) {}
};

Status find_first_rom(FoundRom* output);

} // namespace xenon
} // namespace xenonds

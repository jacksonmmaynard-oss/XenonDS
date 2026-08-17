// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "xenonds/status.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace xenonds {

struct RomRegion {
    std::uint32_t offset;
    std::uint32_t entry_address;
    std::uint32_t ram_address;
    std::uint32_t size;

    RomRegion() : offset(0), entry_address(0), ram_address(0), size(0) {}
};

struct NdsHeader {
    std::string title;
    std::string game_code;
    std::string maker_code;
    std::uint8_t unit_code;
    std::uint8_t device_capacity;
    std::uint8_t rom_version;
    RomRegion arm9;
    RomRegion arm7;
    std::uint32_t banner_offset;
    std::uint32_t declared_rom_size;
    std::uint32_t header_size;
    std::uint16_t stored_header_crc;
    std::uint16_t calculated_header_crc;

    NdsHeader();
};

Status parse_nds_header(const std::uint8_t* rom, std::size_t rom_size, NdsHeader* output);

} // namespace xenonds


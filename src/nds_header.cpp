// SPDX-License-Identifier: GPL-2.0-only
#include "xenonds/nds_header.hpp"

#include "xenonds/checksum.hpp"

#include <cctype>

namespace xenonds {
namespace {

std::uint16_t read_u16_le(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[1]) << 8u);
}

std::uint32_t read_u32_le(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8u) |
           (static_cast<std::uint32_t>(p[2]) << 16u) |
           (static_cast<std::uint32_t>(p[3]) << 24u);
}

std::string clean_ascii(const std::uint8_t* p, std::size_t length) {
    std::string result;
    result.reserve(length);
    for (std::size_t i = 0; i < length; ++i) {
        const unsigned char value = p[i];
        if (value == 0) {
            break;
        }
        result.push_back(std::isprint(value) ? static_cast<char>(value) : '?');
    }
    while (!result.empty() && result[result.size() - 1] == ' ') {
        result.erase(result.size() - 1);
    }
    return result;
}

RomRegion read_region(const std::uint8_t* p) {
    RomRegion region;
    region.offset = read_u32_le(p + 0x00);
    region.entry_address = read_u32_le(p + 0x04);
    region.ram_address = read_u32_le(p + 0x08);
    region.size = read_u32_le(p + 0x0C);
    return region;
}

bool region_fits(const RomRegion& region, std::size_t rom_size) {
    if (region.size == 0 || region.offset < kNdsMinimumHeaderSize) {
        return false;
    }
    const std::uint64_t end = static_cast<std::uint64_t>(region.offset) + region.size;
    return end <= rom_size;
}

bool valid_game_code(const std::string& game_code) {
    if (game_code.size() != 4) {
        return false;
    }
    for (std::size_t i = 0; i < game_code.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(game_code[i]);
        // Commercial titles normally use A-Z/0-9, while legitimate homebrew
        // commonly uses placeholders such as "####". Keep the structural
        // check without rejecting those images.
        if (!std::isprint(c) || std::isspace(c)) {
            return false;
        }
    }
    return true;
}

} // namespace

NdsHeader::NdsHeader()
    : unit_code(0),
      device_capacity(0),
      rom_version(0),
      banner_offset(0),
      declared_rom_size(0),
      header_size(0),
      stored_header_crc(0),
      calculated_header_crc(0) {}

Status parse_nds_header(const std::uint8_t* rom, std::size_t rom_size, NdsHeader* output) {
    return parse_nds_header_prefix(rom, rom_size, rom_size, output);
}

Status parse_nds_header_prefix(const std::uint8_t* header,
                               std::size_t header_size,
                               std::size_t rom_size,
                               NdsHeader* output) {
    if (header == 0 || output == 0) {
        return Status(ErrorCode::invalid_argument, "ROM data and output header are required");
    }
    if (header_size < kNdsMinimumHeaderSize || rom_size < kNdsMinimumHeaderSize) {
        return Status(ErrorCode::rom_too_small, "File is smaller than the 0x160-byte Nintendo DS header");
    }

    NdsHeader parsed;
    parsed.title = clean_ascii(header + 0x00, 12);
    parsed.game_code = clean_ascii(header + 0x0C, 4);
    parsed.maker_code = clean_ascii(header + 0x10, 2);
    parsed.unit_code = header[0x12];
    parsed.device_capacity = header[0x14];
    parsed.rom_version = header[0x1E];
    parsed.arm9 = read_region(header + 0x20);
    parsed.arm7 = read_region(header + 0x30);
    parsed.banner_offset = read_u32_le(header + 0x68);
    parsed.declared_rom_size = read_u32_le(header + 0x80);
    parsed.header_size = read_u32_le(header + 0x84);
    parsed.stored_header_crc = read_u16_le(header + 0x15E);
    parsed.calculated_header_crc = crc16_nintendo(header, 0x15E);

    if (!valid_game_code(parsed.game_code)) {
        return Status(ErrorCode::invalid_header, "Nintendo DS game code is missing or invalid");
    }
    if (parsed.header_size < kNdsMinimumHeaderSize || parsed.header_size > rom_size) {
        return Status(ErrorCode::invalid_header, "Declared header size is outside the ROM image");
    }
    if (parsed.stored_header_crc != parsed.calculated_header_crc) {
        return Status(ErrorCode::header_crc_mismatch, "Nintendo DS header CRC16 does not match");
    }
    if (!region_fits(parsed.arm9, rom_size) || !region_fits(parsed.arm7, rom_size)) {
        return Status(ErrorCode::rom_region_out_of_bounds, "ARM7 or ARM9 executable region is outside the ROM image");
    }

    *output = parsed;
    return Status::Ok();
}

} // namespace xenonds

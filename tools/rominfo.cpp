// SPDX-License-Identifier: GPL-2.0-only
#include "xenonds/nds_header.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: xenonds-rominfo <legally-dumped-game.nds>\n";
        return 2;
    }

    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::cerr << "Could not open: " << argv[1] << "\n";
        return 2;
    }

    const std::vector<std::uint8_t> rom((std::istreambuf_iterator<char>(file)),
                                         std::istreambuf_iterator<char>());
    xenonds::NdsHeader header;
    const xenonds::Status status = xenonds::parse_nds_header(rom.data(), rom.size(), &header);
    if (!status.ok()) {
        std::cerr << "Invalid Nintendo DS image: " << status.message() << "\n";
        return 1;
    }

    std::cout << "Title:       " << header.title << "\n"
              << "Game code:   " << header.game_code << "\n"
              << "Maker code:  " << header.maker_code << "\n"
              << "ROM bytes:   " << rom.size() << "\n"
              << "ARM9:        offset 0x" << std::hex << header.arm9.offset
              << ", size 0x" << header.arm9.size << "\n"
              << "ARM7:        offset 0x" << header.arm7.offset
              << ", size 0x" << header.arm7.size << "\n"
              << "Header CRC:  0x" << std::setw(4) << std::setfill('0')
              << header.stored_header_crc << " (verified)\n";
    return 0;
}


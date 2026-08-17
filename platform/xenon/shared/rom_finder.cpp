// SPDX-License-Identifier: GPL-2.0-only
#include "rom_finder.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <dirent.h>

extern "C" int bdev_enum(int handle, const char** name);

namespace xenonds {
namespace xenon {
namespace {

const char* const kSearchDirectories[] = {"XenonDS", "xenonds", ""};

bool has_nds_extension(const char* name) {
    if (name == 0) {
        return false;
    }

    const std::size_t length = std::strlen(name);
    if (length < 4) {
        return false;
    }

    const char* extension = name + length - 4;
    return extension[0] == '.' &&
           std::tolower(static_cast<unsigned char>(extension[1])) == 'n' &&
           std::tolower(static_cast<unsigned char>(extension[2])) == 'd' &&
           std::tolower(static_cast<unsigned char>(extension[3])) == 's';
}

Status inspect_rom(const char* path, FoundRom* output) {
    std::FILE* file = std::fopen(path, "rb");
    if (file == 0) {
        return Status(ErrorCode::io_error, "Could not open the ROM file");
    }

    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        return Status(ErrorCode::io_error, "Could not seek to the end of the ROM file");
    }

    const long end = std::ftell(file);
    if (end < 0 || std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        return Status(ErrorCode::io_error, "Could not determine the ROM file size");
    }

    std::uint8_t header_bytes[kNdsMinimumHeaderSize];
    const std::size_t bytes_read = std::fread(header_bytes, 1, sizeof(header_bytes), file);
    std::fclose(file);
    if (bytes_read != sizeof(header_bytes)) {
        return Status(ErrorCode::rom_too_small, "Could not read a complete Nintendo DS header");
    }

    NdsHeader header;
    const Status status = parse_nds_header_prefix(
        header_bytes, sizeof(header_bytes), static_cast<std::size_t>(end), &header);
    if (!status.ok()) {
        return status;
    }

    output->path = path;
    output->file_size = static_cast<std::size_t>(end);
    output->header = header;
    std::memcpy(output->header_bytes.data(), header_bytes, sizeof(header_bytes));
    return Status::Ok();
}

Status scan_directory(const char* device, const char* directory, FoundRom* output) {
    char directory_path[256];
    if (directory[0] == '\0') {
        std::snprintf(directory_path, sizeof(directory_path), "%s:/", device);
    } else {
        std::snprintf(directory_path, sizeof(directory_path), "%s:/%s/", device, directory);
    }

    DIR* open_directory = opendir(directory_path);
    if (open_directory == 0) {
        return Status(ErrorCode::io_error, "Directory is not available");
    }

    Status last_error(ErrorCode::io_error, "No .nds file was found");
    for (dirent* entry = readdir(open_directory); entry != 0; entry = readdir(open_directory)) {
        if (!has_nds_extension(entry->d_name)) {
            continue;
        }

        char rom_path[512];
        std::snprintf(rom_path, sizeof(rom_path), "%s%s", directory_path, entry->d_name);
        last_error = inspect_rom(rom_path, output);
        if (last_error.ok()) {
            closedir(open_directory);
            return last_error;
        }
    }

    closedir(open_directory);
    return last_error;
}

} // namespace

Status find_first_rom(FoundRom* output) {
    if (output == 0) {
        return Status(ErrorCode::invalid_argument, "A ROM result is required");
    }

    const char* device = 0;
    int handle = -1;
    bool saw_device = false;
    Status last_error(ErrorCode::io_error, "No FAT storage device was mounted");

    while ((handle = bdev_enum(handle, &device)) >= 0) {
        saw_device = true;
        for (std::size_t i = 0;
             i < sizeof(kSearchDirectories) / sizeof(kSearchDirectories[0]);
             ++i) {
            last_error = scan_directory(device, kSearchDirectories[i], output);
            if (last_error.ok()) {
                return last_error;
            }
        }
    }

    if (saw_device && last_error.message() == "Directory is not available") {
        return Status(ErrorCode::io_error, "No .nds file was found on mounted storage");
    }
    return last_error;
}

} // namespace xenon
} // namespace xenonds

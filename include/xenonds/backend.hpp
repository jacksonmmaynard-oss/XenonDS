// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "xenonds/nds_header.hpp"
#include "xenonds/status.hpp"
#include "xenonds/types.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace xenonds {

struct RomView {
    const std::uint8_t* data;
    std::size_t data_size;
    // Size of the complete file, which can exceed data_size for file-backed ROMs.
    std::size_t size;
    std::string source_path;
    NdsHeader header;

    RomView() : data(0), data_size(0), size(0) {}
};

class CoreBackend {
public:
    virtual ~CoreBackend() {}

    virtual const char* name() const = 0;
    virtual Status initialize(const CoreConfig& config) = 0;
    virtual Status load_rom(const RomView& rom) = 0;
    virtual Status reset() = 0;
    virtual Status run_frame(const InputState& input, FrameOutput* output) = 0;
    virtual void unload_rom() = 0;
    virtual void shutdown() = 0;
};

} // namespace xenonds

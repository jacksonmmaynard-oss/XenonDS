// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "xenonds/backend.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace xenonds {

enum class SessionState {
    created,
    initialized,
    rom_loaded,
    running,
    paused,
    stopped,
    faulted
};

class Session {
public:
    explicit Session(CoreBackend* backend);
    ~Session();

    Status initialize(const CoreConfig& config);
    Status load_rom(const std::uint8_t* data, std::size_t size, const std::string& source_path);
    Status load_rom_header(const std::uint8_t* header_data,
                           std::size_t header_size,
                           std::size_t rom_size,
                           const std::string& source_path);
    Status start();
    Status pause();
    Status reset();
    Status run_frame(const InputState& input, FrameOutput* output);
    void stop();

    SessionState state() const { return state_; }
    const NdsHeader& rom_header() const { return rom_.header; }

private:
    Status fail(ErrorCode code, const std::string& message);
    Status load_validated_rom(const std::uint8_t* data,
                              std::size_t data_size,
                              std::size_t rom_size,
                              const std::string& source_path,
                              const NdsHeader& header);

    CoreBackend* backend_;
    SessionState state_;
    RomView rom_;
    bool backend_initialized_;
    bool rom_is_loaded_;
};

} // namespace xenonds

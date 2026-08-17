// SPDX-License-Identifier: GPL-2.0-only
#include "xenonds/session.hpp"

namespace xenonds {

Session::Session(CoreBackend* backend)
    : backend_(backend),
      state_(SessionState::created),
      backend_initialized_(false),
      rom_is_loaded_(false) {}

Session::~Session() {
    stop();
}

Status Session::fail(ErrorCode code, const std::string& message) {
    state_ = SessionState::faulted;
    return Status(code, message);
}

Status Session::initialize(const CoreConfig& config) {
    if (backend_ == 0) {
        return fail(ErrorCode::invalid_argument, "A core backend is required");
    }
    if (state_ != SessionState::created && state_ != SessionState::stopped) {
        return Status(ErrorCode::invalid_state, "Session has already been initialized");
    }

    const Status status = backend_->initialize(config);
    if (!status.ok()) {
        return fail(status.code(), status.message());
    }
    backend_initialized_ = true;
    state_ = SessionState::initialized;
    return Status::Ok();
}

Status Session::load_rom(const std::uint8_t* data,
                         std::size_t size,
                         const std::string& source_path) {
    if (state_ != SessionState::initialized) {
        return Status(ErrorCode::invalid_state, "Initialize the session before loading a ROM");
    }

    NdsHeader header;
    const Status parse_status = parse_nds_header(data, size, &header);
    if (!parse_status.ok()) {
        return parse_status;
    }

    return load_validated_rom(data, size, size, source_path, header);
}

Status Session::load_rom_header(const std::uint8_t* header_data,
                                std::size_t header_size,
                                std::size_t rom_size,
                                const std::string& source_path) {
    if (state_ != SessionState::initialized) {
        return Status(ErrorCode::invalid_state, "Initialize the session before loading a ROM");
    }

    NdsHeader header;
    const Status parse_status = parse_nds_header_prefix(
        header_data, header_size, rom_size, &header);
    if (!parse_status.ok()) {
        return parse_status;
    }

    // The backend receives the full file size and path, but only a header-sized
    // memory view. File-backed backends should reopen source_path as needed.
    return load_validated_rom(header_data, header_size, rom_size, source_path, header);
}

Status Session::load_validated_rom(const std::uint8_t* data,
                                   std::size_t data_size,
                                   std::size_t rom_size,
                                   const std::string& source_path,
                                   const NdsHeader& header) {

    rom_.data = data;
    rom_.data_size = data_size;
    rom_.size = rom_size;
    rom_.source_path = source_path;
    rom_.header = header;

    const Status load_status = backend_->load_rom(rom_);
    if (!load_status.ok()) {
        return fail(load_status.code(), load_status.message());
    }
    rom_is_loaded_ = true;
    state_ = SessionState::rom_loaded;
    return Status::Ok();
}

Status Session::start() {
    if (state_ != SessionState::rom_loaded && state_ != SessionState::paused) {
        return Status(ErrorCode::invalid_state, "Load a ROM before starting emulation");
    }
    state_ = SessionState::running;
    return Status::Ok();
}

Status Session::pause() {
    if (state_ != SessionState::running) {
        return Status(ErrorCode::invalid_state, "Only a running session can be paused");
    }
    state_ = SessionState::paused;
    return Status::Ok();
}

Status Session::reset() {
    if (!rom_is_loaded_) {
        return Status(ErrorCode::invalid_state, "Load a ROM before resetting emulation");
    }
    const Status status = backend_->reset();
    if (!status.ok()) {
        return fail(status.code(), status.message());
    }
    state_ = SessionState::rom_loaded;
    return Status::Ok();
}

Status Session::run_frame(const InputState& input, FrameOutput* output) {
    if (state_ != SessionState::running) {
        return Status(ErrorCode::invalid_state, "Session must be running to execute a frame");
    }
    if (output == 0) {
        return Status(ErrorCode::invalid_argument, "Frame output is required");
    }

    InputState sanitized = input;
    if (sanitized.touch.x >= kScreenWidth) {
        sanitized.touch.x = static_cast<std::uint16_t>(kScreenWidth - 1);
    }
    if (sanitized.touch.y >= kScreenHeight) {
        sanitized.touch.y = static_cast<std::uint16_t>(kScreenHeight - 1);
    }

    const Status status = backend_->run_frame(sanitized, output);
    if (!status.ok()) {
        return fail(status.code(), status.message());
    }
    return Status::Ok();
}

void Session::stop() {
    if (backend_ != 0 && rom_is_loaded_) {
        backend_->unload_rom();
    }
    rom_is_loaded_ = false;
    if (backend_ != 0 && backend_initialized_) {
        backend_->shutdown();
    }
    backend_initialized_ = false;
    state_ = SessionState::stopped;
}

} // namespace xenonds

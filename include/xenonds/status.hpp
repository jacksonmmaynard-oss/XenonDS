// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <string>

namespace xenonds {

enum class ErrorCode {
    ok = 0,
    invalid_argument,
    invalid_state,
    io_error,
    rom_too_small,
    invalid_header,
    header_crc_mismatch,
    rom_region_out_of_bounds,
    backend_error,
    unsupported
};

class Status {
public:
    Status() : code_(ErrorCode::ok) {}
    Status(ErrorCode code, const std::string& message) : code_(code), message_(message) {}

    static Status Ok() { return Status(); }

    bool ok() const { return code_ == ErrorCode::ok; }
    ErrorCode code() const { return code_; }
    const std::string& message() const { return message_; }

private:
    ErrorCode code_;
    std::string message_;
};

} // namespace xenonds


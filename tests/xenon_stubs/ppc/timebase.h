// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
static inline std::uint64_t mftb(void) { return 0; }
static inline unsigned long tb_diff_usec(std::uint64_t end, std::uint64_t start) {
    return static_cast<unsigned long>(end - start);
}

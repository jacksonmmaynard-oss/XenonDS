// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

extern "C" std::uint64_t noods_video_test_mftb(void);

static inline std::uint64_t mftb(void) {
    return noods_video_test_mftb();
}

static inline unsigned long tb_diff_usec(std::uint64_t end,
                                         std::uint64_t start) {
    return static_cast<unsigned long>(end - start);
}

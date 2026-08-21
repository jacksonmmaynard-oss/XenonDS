// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace xenonds {
namespace xenon {

enum class SpeedMode : unsigned int {
    Normal = 0,
    Turbo = 1
};

struct SpeedModeConfig {
    unsigned long frame_cap_microseconds;
    int frame_skip;
    bool turbo_enabled;
};

inline SpeedModeConfig speed_mode_config(SpeedMode mode) {
    if (mode == SpeedMode::Turbo) {
        // Turbo removes the frontend cap and renders one frame in four. It
        // promises maximum effort, not an impossible fixed multiplier when
        // DS CPU emulation itself is the bottleneck.
        return {0UL, 3, true};
    }
    return {16715UL, 0, false};
}

inline SpeedMode toggle_speed_mode(SpeedMode mode) {
    return mode == SpeedMode::Normal ? SpeedMode::Turbo : SpeedMode::Normal;
}

} // namespace xenon
} // namespace xenonds

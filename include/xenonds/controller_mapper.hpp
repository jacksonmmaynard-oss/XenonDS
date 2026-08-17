// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "xenonds/types.hpp"

#include <cstdint>

namespace xenonds {

// Platform adapters translate their native controller structure into this
// stable snapshot. Directional face names describe physical button position,
// which keeps Xbox and Nintendo label differences out of the platform layer.
struct ControllerSnapshot {
    bool south;
    bool east;
    bool west;
    bool north;
    bool dpad_up;
    bool dpad_down;
    bool dpad_left;
    bool dpad_right;
    bool left_shoulder;
    bool right_shoulder;
    bool start;
    bool back;
    std::int16_t right_x;
    std::int16_t right_y;
    std::uint8_t right_trigger;

    ControllerSnapshot();
};

class ControllerMapper {
public:
    ControllerMapper();

    InputState map(const ControllerSnapshot& controller);
    void set_touch_cursor(std::uint16_t x, std::uint16_t y);
    void set_dead_zone(std::uint16_t dead_zone);
    void set_cursor_speed(std::uint8_t pixels_per_frame);

    std::uint16_t touch_x() const { return touch_x_; }
    std::uint16_t touch_y() const { return touch_y_; }

private:
    int axis_delta(std::int16_t value) const;

    std::uint16_t touch_x_;
    std::uint16_t touch_y_;
    std::uint16_t dead_zone_;
    std::uint8_t cursor_speed_;
};

} // namespace xenonds

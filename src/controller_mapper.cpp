// SPDX-License-Identifier: GPL-2.0-or-later
#include "xenonds/controller_mapper.hpp"

#include <algorithm>
#include <cstdlib>

namespace xenonds {
namespace {

void set_button(bool pressed, Button button, std::uint16_t* buttons) {
    if (pressed) {
        *buttons |= static_cast<std::uint16_t>(button);
    }
}

std::uint16_t clamp_coordinate(int value, int maximum) {
    return static_cast<std::uint16_t>(std::max(0, std::min(value, maximum)));
}

} // namespace

ControllerSnapshot::ControllerSnapshot()
    : south(false),
      east(false),
      west(false),
      north(false),
      dpad_up(false),
      dpad_down(false),
      dpad_left(false),
      dpad_right(false),
      left_shoulder(false),
      right_shoulder(false),
      start(false),
      back(false),
      right_x(0),
      right_y(0),
      right_trigger(0) {}

ControllerMapper::ControllerMapper()
    : touch_x_(static_cast<std::uint16_t>(kScreenWidth / 2)),
      touch_y_(static_cast<std::uint16_t>(kScreenHeight / 2)),
      dead_zone_(8000),
      cursor_speed_(6) {}

void ControllerMapper::set_touch_cursor(std::uint16_t x, std::uint16_t y) {
    touch_x_ = clamp_coordinate(x, static_cast<int>(kScreenWidth - 1));
    touch_y_ = clamp_coordinate(y, static_cast<int>(kScreenHeight - 1));
}

void ControllerMapper::set_dead_zone(std::uint16_t dead_zone) {
    dead_zone_ = std::min<std::uint16_t>(dead_zone, 32766u);
}

void ControllerMapper::set_cursor_speed(std::uint8_t pixels_per_frame) {
    cursor_speed_ = std::max<std::uint8_t>(pixels_per_frame, 1u);
}

int ControllerMapper::axis_delta(std::int16_t value) const {
    const int magnitude = std::abs(static_cast<int>(value));
    if (magnitude <= dead_zone_) {
        return 0;
    }
    const int range = 32767 - dead_zone_;
    const int scaled = 1 + ((magnitude - dead_zone_) * (cursor_speed_ - 1)) / range;
    return value < 0 ? -scaled : scaled;
}

InputState ControllerMapper::map(const ControllerSnapshot& controller) {
    InputState input;

    // Match the printed Xbox labels so A confirms and B cancels naturally.
    set_button(controller.south, button_a, &input.buttons);
    set_button(controller.east, button_b, &input.buttons);
    set_button(controller.west, button_x, &input.buttons);
    set_button(controller.north, button_y, &input.buttons);
    set_button(controller.dpad_up, button_up, &input.buttons);
    set_button(controller.dpad_down, button_down, &input.buttons);
    set_button(controller.dpad_left, button_left, &input.buttons);
    set_button(controller.dpad_right, button_right, &input.buttons);
    set_button(controller.left_shoulder, button_l, &input.buttons);
    set_button(controller.right_shoulder, button_r, &input.buttons);
    set_button(controller.start, button_start, &input.buttons);
    set_button(controller.back, button_select, &input.buttons);

    touch_x_ = clamp_coordinate(static_cast<int>(touch_x_) + axis_delta(controller.right_x),
                                static_cast<int>(kScreenWidth - 1));
    // Positive stick Y means up on LibXenon, while touch coordinates grow down.
    touch_y_ = clamp_coordinate(static_cast<int>(touch_y_) - axis_delta(controller.right_y),
                                static_cast<int>(kScreenHeight - 1));
    input.touch.x = touch_x_;
    input.touch.y = touch_y_;
    input.touch.pressed = controller.right_trigger > 32u;
    return input;
}

} // namespace xenonds

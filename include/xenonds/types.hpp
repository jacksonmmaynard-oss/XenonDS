// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace xenonds {

static const std::size_t kScreenWidth = 256;
static const std::size_t kScreenHeight = 192;
static const std::size_t kScreenCount = 2;
static const std::size_t kCombinedPixelCount = kScreenWidth * kScreenHeight * kScreenCount;

enum Button : std::uint16_t {
    button_a      = 1u << 0,
    button_b      = 1u << 1,
    button_select = 1u << 2,
    button_start  = 1u << 3,
    button_right  = 1u << 4,
    button_left   = 1u << 5,
    button_up     = 1u << 6,
    button_down   = 1u << 7,
    button_r      = 1u << 8,
    button_l      = 1u << 9,
    button_x      = 1u << 10,
    button_y      = 1u << 11
};

struct TouchState {
    bool pressed;
    std::uint16_t x;
    std::uint16_t y;

    TouchState() : pressed(false), x(0), y(0) {}
};

struct InputState {
    std::uint16_t buttons;
    TouchState touch;
    bool lid_closed;

    InputState() : buttons(0), lid_closed(false) {}
};

struct FrameOutput {
    // Native DS BGR555 pixels. Main screen first, touch screen second.
    std::vector<std::uint16_t> pixels;
    // Interleaved signed 16-bit stereo samples.
    std::vector<std::int16_t> audio;
    std::uint32_t audio_sample_rate;

    FrameOutput() : pixels(kCombinedPixelCount, 0), audio_sample_rate(32768) {}
};

struct CoreConfig {
    bool enable_audio;
    bool enable_jit;
    std::uint32_t audio_sample_rate;

    CoreConfig() : enable_audio(true), enable_jit(false), audio_sample_rate(32768) {}
};

} // namespace xenonds


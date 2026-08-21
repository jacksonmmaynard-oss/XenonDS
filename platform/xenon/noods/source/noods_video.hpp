// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xenonds/types.hpp"

#include <cstdint>

namespace xenonds {
namespace xenon {

struct PictureSettings {
    int brightness;
    unsigned int contrast;
    unsigned int saturation;

    PictureSettings(): brightness(8), contrast(78), saturation(100) {}
};

struct PerformanceStats {
    unsigned int emulation_fps_tenths;
    unsigned int video_fps_tenths;
    unsigned int game_speed_percent;
    unsigned long core_microseconds;
    bool turbo_enabled;
    bool picture_menu_open;
    unsigned int picture_menu_item;
    PictureSettings picture;

    PerformanceStats(): emulation_fps_tenths(0), video_fps_tenths(0),
                        game_speed_percent(0), core_microseconds(0),
                        turbo_enabled(false),
                        picture_menu_open(false), picture_menu_item(0) {}
};

// Long enough for an ordinary presentation to drain, but finite so a wedged
// hardware context cannot hang settings changes or shutdown.
const unsigned long kDefaultNooDSVideoWaitMicroseconds = 100000UL;

bool initialize_noods_video();
bool set_noods_picture_settings(
    const PictureSettings& settings,
    unsigned long timeout_microseconds = kDefaultNooDSVideoWaitMicroseconds);

// Single-producer, nonblocking submission. A false result means the caller
// must retain its dirty frame and retry later. If the worker is busy, none of
// the queued frame state is changed. An idle worker gets exactly one launch
// attempt; a launch failure also returns false.
bool try_present_noods_frame(const std::uint16_t* pixels,
                             const TouchState& touch,
                             const PerformanceStats& stats);

// Waits at most timeout_microseconds for the current presentation. A zero
// timeout performs one state check only. The return value reports whether the
// worker is idle; callers may therefore handle a bounded shutdown timeout.
bool wait_for_noods_video(
    unsigned long timeout_microseconds = kDefaultNooDSVideoWaitMicroseconds);

} // namespace xenon
} // namespace xenonds

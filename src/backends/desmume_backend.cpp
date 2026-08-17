// SPDX-License-Identifier: GPL-2.0-only
#include "xenonds/backends/desmume_backend.hpp"

#include "GPU.h"
#include "NDSSystem.h"

#include <algorithm>
#include <cstdio>

#ifdef XENON
#include <ppc/timebase.h>
#endif

namespace xenonds {
namespace {

bool pressed(const InputState& input, Button button) {
    return (input.buttons & static_cast<std::uint16_t>(button)) != 0;
}

} // namespace

DesmumeBackend::DesmumeBackend()
    : initialized_(false), loaded_(false), first_frame_(true) {}
DesmumeBackend::~DesmumeBackend() { shutdown(); }

const char* DesmumeBackend::name() const { return "DeSmuME"; }

Status DesmumeBackend::initialize(const CoreConfig&) {
    std::printf("[core] Entering NDS_Init\n");
    if (NDS_Init() != 0) {
        return Status(ErrorCode::backend_error, "DeSmuME NDS_Init failed");
    }
#ifdef XENON
    // Advanced bus timing simulates sequential wait states and the ARM9 cache
    // controller on every guest fetch. DeSmuME supports disabling it as a
    // faster, less cycle-exact mode; this is the default for the Xenon port.
    CommonSettings.advanced_timing = false;
    std::printf("[core] Fast bus timing enabled\n");
#endif
    std::printf("[core] NDS_Init returned successfully\n");
    initialized_ = true;
    return Status::Ok();
}

Status DesmumeBackend::load_rom(const RomView& rom) {
    if (!initialized_) {
        return Status(ErrorCode::invalid_state, "DeSmuME is not initialized");
    }
    if (rom.source_path.empty()) {
        return Status(ErrorCode::invalid_argument, "The initial DeSmuME adapter requires a ROM file path");
    }
    std::printf("[core] Entering NDS_LoadROM: %s\n", rom.source_path.c_str());
    if (NDS_LoadROM(rom.source_path.c_str()) < 0) {
        return Status(ErrorCode::backend_error, "DeSmuME could not load the ROM");
    }
    // NDS_LoadROM() resets the core and leaves the frontend-owned execution
    // flag paused. Desktop frontends call desmume_resume(); the Xenon adapter
    // owns the same flag directly so NDS_exec() can run through VBlank.
    execute = true;
    std::printf("[core] NDS_LoadROM returned successfully\n");
    std::printf("[core] ARM execution resumed\n");
    loaded_ = true;
    return Status::Ok();
}

Status DesmumeBackend::reset() {
    if (!loaded_) {
        return Status(ErrorCode::invalid_state, "No ROM is loaded");
    }
    NDS_Reset();
    execute = true;
    return Status::Ok();
}

Status DesmumeBackend::run_frame(const InputState& input, FrameOutput* output) {
    if (!loaded_ || output == 0) {
        return Status(ErrorCode::invalid_state, "A loaded ROM and frame output are required");
    }

    last_frame_profile_ = DesmumeFrameProfile();
#ifdef XENON
    std::uint64_t stage_start = mftb();
#endif

    if (first_frame_) {
        std::printf("[frame 1/5] applying raw controller state\n");
    }

    // DeSmuME requires frontends to update raw input before opening its
    // processing window. NDS_beginProcessingInput() copies raw input into the
    // mutable processing buffer and deliberately rejects later raw updates.
    NDS_setPad(pressed(input, button_right),
               pressed(input, button_left),
               pressed(input, button_down),
               pressed(input, button_up),
               pressed(input, button_select),
               pressed(input, button_start),
               pressed(input, button_b),
               pressed(input, button_a),
               pressed(input, button_y),
               pressed(input, button_x),
               pressed(input, button_l),
               pressed(input, button_r),
               false,
               input.lid_closed);
    if (input.touch.pressed) {
        NDS_setTouchPos(input.touch.x, input.touch.y);
    } else {
        NDS_releaseTouch();
    }

    if (first_frame_) {
        std::printf("[frame 2/5] beginning input processing\n");
    }
    NDS_beginProcessingInput();
    NDS_endProcessingInput();

#ifdef XENON
    std::uint64_t stage_end = mftb();
    last_frame_profile_.input_microseconds = tb_diff_usec(stage_end, stage_start);
    stage_start = stage_end;
#endif

    if (first_frame_) {
        std::printf("[frame 3/5] running ARM interpreters\n");
    }
    NDS_exec<false>();

#ifdef XENON
    stage_end = mftb();
    last_frame_profile_.arm_microseconds = tb_diff_usec(stage_end, stage_start);
    stage_start = stage_end;
#endif

    if (first_frame_) {
        std::printf("[frame 4/5] interpreter returned; reading video\n");
    }
    const NDSDisplayInfo& display = GPU->GetDisplayInfo();
    const std::uint16_t* source = display.masterNativeBuffer16;
    if (source == 0) {
        return Status(ErrorCode::backend_error, "DeSmuME returned an empty framebuffer");
    }
    if (output->pixels.size() != kCombinedPixelCount) {
        output->pixels.resize(kCombinedPixelCount);
    }
    std::copy(source, source + kCombinedPixelCount, output->pixels.begin());

    // Audio wiring is intentionally deferred until the LibXenon sound ring
    // buffer is in place. The emulation core still advances its SPU per frame.
    output->audio.clear();
#ifdef XENON
    stage_end = mftb();
    last_frame_profile_.copy_microseconds = tb_diff_usec(stage_end, stage_start);
#endif
    if (first_frame_) {
        std::printf("[frame 5/5] first framebuffer copied\n");
        first_frame_ = false;
    }
    return Status::Ok();
}

const DesmumeFrameProfile& DesmumeBackend::last_frame_profile() const {
    return last_frame_profile_;
}

void DesmumeBackend::request_frame_skip() {
    NDS_SkipNextFrame();
}

void DesmumeBackend::cancel_frame_skip() {
    NDS_OmitFrameSkip(2);
}

void DesmumeBackend::unload_rom() {
    execute = false;
    loaded_ = false;
    first_frame_ = true;
}

void DesmumeBackend::shutdown() {
    if (initialized_) {
        execute = false;
        NDS_DeInit();
    }
    initialized_ = false;
    loaded_ = false;
    first_frame_ = true;
}

} // namespace xenonds

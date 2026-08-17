// SPDX-License-Identifier: GPL-2.0-only
#include "xenonds/backends/desmume_backend.hpp"

#include "GPU.h"
#include "NDSSystem.h"

#include <algorithm>

namespace xenonds {
namespace {

bool pressed(const InputState& input, Button button) {
    return (input.buttons & static_cast<std::uint16_t>(button)) != 0;
}

} // namespace

DesmumeBackend::DesmumeBackend() : initialized_(false), loaded_(false) {}
DesmumeBackend::~DesmumeBackend() { shutdown(); }

const char* DesmumeBackend::name() const { return "DeSmuME"; }

Status DesmumeBackend::initialize(const CoreConfig&) {
    if (NDS_Init() != 0) {
        return Status(ErrorCode::backend_error, "DeSmuME NDS_Init failed");
    }
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
    if (NDS_LoadROM(rom.source_path.c_str()) < 0) {
        return Status(ErrorCode::backend_error, "DeSmuME could not load the ROM");
    }
    loaded_ = true;
    return Status::Ok();
}

Status DesmumeBackend::reset() {
    if (!loaded_) {
        return Status(ErrorCode::invalid_state, "No ROM is loaded");
    }
    NDS_Reset();
    return Status::Ok();
}

Status DesmumeBackend::run_frame(const InputState& input, FrameOutput* output) {
    if (!loaded_ || output == 0) {
        return Status(ErrorCode::invalid_state, "A loaded ROM and frame output are required");
    }

    NDS_beginProcessingInput();
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
    NDS_endProcessingInput();

    NDS_exec<false>();

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
    return Status::Ok();
}

void DesmumeBackend::unload_rom() {
    loaded_ = false;
}

void DesmumeBackend::shutdown() {
    if (initialized_) {
        NDS_DeInit();
    }
    initialized_ = false;
    loaded_ = false;
}

} // namespace xenonds


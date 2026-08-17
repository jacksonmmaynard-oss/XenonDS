// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "xenonds/backend.hpp"

namespace xenonds {

struct DesmumeFrameProfile {
    unsigned long input_microseconds;
    unsigned long arm_microseconds;
    unsigned long copy_microseconds;

    DesmumeFrameProfile()
        : input_microseconds(0), arm_microseconds(0), copy_microseconds(0) {}
};

// First integration adapter for the upstream DeSmuME core. The host tests do
// not build this target yet; it is enabled with XENONDS_WITH_DESMUME once the
// pinned upstream source and LibXenon platform layer are present.
class DesmumeBackend : public CoreBackend {
public:
    DesmumeBackend();
    virtual ~DesmumeBackend();

    virtual const char* name() const;
    virtual Status initialize(const CoreConfig& config);
    virtual Status load_rom(const RomView& rom);
    virtual Status reset();
    virtual Status run_frame(const InputState& input, FrameOutput* output);
    virtual void unload_rom();
    virtual void shutdown();

    const DesmumeFrameProfile& last_frame_profile() const;

private:
    bool initialized_;
    bool loaded_;
    bool first_frame_;
    DesmumeFrameProfile last_frame_profile_;
};

} // namespace xenonds

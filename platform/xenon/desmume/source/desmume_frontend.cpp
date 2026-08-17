// SPDX-License-Identifier: GPL-2.0-only
#include <cstring>
#include <map>
#include <sys/times.h>

#include "frontend/interface/interface.h"
#include "rasterize.h"
#include "render3D.h"
#include "SPU.h"

// DeSmuME deliberately leaves these frontend-owned globals undefined. The
// Xenon runtime has no SDL frontend, so it supplies the minimal interpreter
// state and the null/software 3D renderer list here.
volatile bool execute = false;
TieredRegion hooked_regions[HOOK_COUNT];
std::map<unsigned int, memory_cb_fnc> hooks[HOOK_COUNT];

GPU3DInterface* core3DList[] = {
    &gpu3DNull,
    &gpu3DRasterize,
    nullptr,
};

SoundInterface_struct* SNDCoreList[] = {
    &SNDDummy,
    nullptr,
};

// Newlib in the current LibXenon toolchain declares these POSIX functions but
// does not provide implementations. DeSmuME only uses realpath() to normalize
// optional frontend paths, and the v0.3 runtime does not use process timing.
extern "C" char* realpath(const char* path, char* resolved_path) {
    if (path == nullptr || resolved_path == nullptr) {
        return nullptr;
    }

    std::strcpy(resolved_path, path);
    return resolved_path;
}

extern "C" clock_t times(struct tms* buffer) {
    if (buffer != nullptr) {
        std::memset(buffer, 0, sizeof(*buffer));
    }
    return 0;
}

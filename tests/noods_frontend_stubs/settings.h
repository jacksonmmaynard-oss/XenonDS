// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

// Minimal deterministic Settings replacement for exercising the actual Xenon
// frontend loop without constructing the full emulator core.
class Settings {
public:
    inline static std::string basePath;
    inline static std::string gbaBiosPath;
    inline static std::string ndsBios9Path;
    inline static std::string ndsBios7Path;
    inline static std::string ndsFirmPath;

    inline static int directBoot = 0;
    inline static int romInRam = 0;
    inline static int fpsLimiter = 0;
    inline static int frameskip = 0;
    inline static int threaded2D = 0;
    inline static int threaded3D = 0;
    inline static int highRes3D = 0;
    inline static int screenGhost = 0;
    inline static int emulateAudio = 0;
    inline static int savesFolder = 0;
    inline static int statesFolder = 0;
    inline static int cheatsFolder = 0;
    inline static int screenFilter = 0;
    inline static int dsiMode = 0;
    inline static int arm7Hle = 0;
};

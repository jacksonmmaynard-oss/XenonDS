// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

enum {
    ERROR_NDS_BIOS = 1,
    ERROR_NDS_FIRM,
    ERROR_DSI_BIOS,
    ERROR_DSI_FIRM,
    ERROR_DSI_NAND,
    ERROR_ROM
};

namespace noods_frontend_test {

inline constexpr unsigned int kSchedulerOnlyReturns = 2;
inline constexpr unsigned int kUniformFrames = 602;
inline constexpr std::size_t kCombinedPixelCount = 256u * 192u * 2u;

inline unsigned int run_calls = 0;
inline unsigned int completed_frames = 0;
inline unsigned int frame_handoffs = 0;
inline unsigned int save_calls = 0;
inline unsigned int save_completed_frames[8] = {};
inline unsigned int skip_request_count = 0;
inline int skip_requests[8] = {};

} // namespace noods_frontend_test

class FakeInterpreter {
public:
    std::uint32_t unknownOpcodeCount = 0;
    std::uint32_t lastUnknownOpcode = 0;

    std::uint32_t getPC() const { return 0x02000000u; }
};

class FakeInput {
public:
    void pressKey(int) {}
    void releaseKey(int) {}
    void pressScreen() {}
    void releaseScreen() {}
};

class FakeSpi {
public:
    void setTouch(std::uint16_t, std::uint16_t) {}
    void clearTouch() {}
};

class FakeGpu {
public:
    void requestFrameSkip(int skip) {
        const unsigned int index = noods_frontend_test::skip_request_count++;
        if (index < 8) noods_frontend_test::skip_requests[index] = skip;
        frame_skip_ = skip;
        phase_ = 0;
    }

    bool getFrame(std::uint32_t* output, bool) {
        if (!ready_) return false;
        ready_ = false;
        std::fill(output,
                  output + noods_frontend_test::kCombinedPixelCount,
                  0xFFFFFFFFu);
        ++noods_frontend_test::frame_handoffs;
        return true;
    }

    bool getFrameXenon(std::uint16_t* output, bool) {
        if (!ready_) return false;
        ready_ = false;
        std::fill(output,
                  output + noods_frontend_test::kCombinedPixelCount,
                  static_cast<std::uint16_t>(0x7FFFu));
        ++noods_frontend_test::frame_handoffs;
        return true;
    }

private:
    friend class Core;
    void completeFrame() {
        ready_ = phase_ == 0;
        phase_ = phase_ >= frame_skip_ ? 0 : phase_ + 1;
    }

    bool ready_ = false;
    int frame_skip_ = 0;
    int phase_ = 0;
};

class FakeCartridge {
public:
    void writeSave() {
        const unsigned int index = noods_frontend_test::save_calls++;
        if (index < 8) {
            noods_frontend_test::save_completed_frames[index] =
                noods_frontend_test::completed_frames;
        }
    }
};

class Core {
public:
    explicit Core(const std::string&) {}

    void runCore() {
        ++noods_frontend_test::run_calls;
        if (noods_frontend_test::run_calls <=
            noods_frontend_test::kSchedulerOnlyReturns) {
            // Model the UPDATE_RUN exits caused by one CPU halting and then
            // resuming. They are scheduler boundaries, not DS frames.
            return;
        }

        ++completedFrames;
        noods_frontend_test::completed_frames = completedFrames;
        gpu.completeFrame();
    }

    std::uint32_t completedFrames = 0;
    FakeInterpreter interpreter[2];
    FakeInput input;
    FakeSpi spi;
    FakeGpu gpu;
    FakeCartridge cartridgeNds;
};

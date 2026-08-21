// SPDX-License-Identifier: GPL-3.0-or-later
#include "core.h"
#include "settings.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <vector>

namespace {

std::uint32_t framebuffer[256 * 192 * 2];

std::uint32_t hash_frame() {
    std::uint32_t hash = 2166136261u;
    for (std::size_t i = 0; i < 256u * 192u * 2u; ++i) {
        hash ^= framebuffer[i];
        hash *= 16777619u;
    }
    return hash;
}

std::size_t count_red_pixels() {
    std::size_t count = 0;
    for (std::size_t i = 0; i < 256u * 192u * 2u; ++i) {
        const std::uint32_t rgb = framebuffer[i] & 0x00FFFFFFu;
        if ((rgb & 0x000000FFu) >= 0x000000F0u &&
            (rgb & 0x00FFFF00u) == 0)
            ++count;
    }
    return count;
}

std::uint32_t hash_3d_frame(Core& core) {
    std::uint32_t hash = 2166136261u;
    for (int y = 0; y < 192; ++y) {
        const std::uint32_t* line = core.gpu3DRenderer.getLine(y);
        for (int x = 0; x < 256; ++x) {
            hash ^= line[x];
            hash *= 16777619u;
        }
    }
    return hash;
}

void prepare_3d_benchmark(Core& core) {
    const int columns = 24;
    const int rows = 16;
    const int polygon_count = columns * rows;
    core.gpu3D.polygonCountOut = polygon_count;
    core.gpu3D.vertexCountOut = polygon_count * 4;

    for (int i = 0; i < polygon_count; ++i) {
        const int column = i % columns;
        const int row = i / columns;
        const int left = column * 256 / columns;
        const int right = (column + 1) * 256 / columns;
        const int top = row * 192 / rows;
        const int bottom = (row + 1) * 192 / rows;
        Vertex* vertices = &core.gpu3D.verticesOut[i * 4];
        vertices[0].x = left;  vertices[0].y = top;
        vertices[1].x = right; vertices[1].y = top;
        vertices[2].x = right; vertices[2].y = bottom;
        vertices[3].x = left;  vertices[3].y = bottom;
        for (int vertex = 0; vertex < 4; ++vertex) {
            vertices[vertex].z = 0x1000 + i;
            vertices[vertex].w = 0x1000;
            vertices[vertex].color =
                ((i * 13) & 0x3F) |
                (((i * 29) & 0x3F) << 6) |
                (((i * 47) & 0x3F) << 12);
        }

        _Polygon& polygon = core.gpu3D.polygonsOut[i];
        polygon = _Polygon();
        polygon.vertices = i * 4;
        polygon.size = 4;
        polygon.alpha = (i % 7 == 0) ? 0x28 : 0x3F;
        polygon.id = i & 0x3F;
    }

    core.gpu3DRenderer.writeClearColor(0xFFFFFFFFu, 0);
    core.gpu3DRenderer.writeClearDepth(0xFFFFu, 0x7FFFu);
}

void set_3d_clear_color(Core& core, std::uint32_t color) {
    core.gpu3DRenderer.writeClearColor(0xFFFFFFFFu, color);
    core.gpu.invalidate3D();
}

double benchmark_3d(Core& core, int frames, int threads,
                    std::uint32_t* output_hash) {
    Settings::threaded3D = threads;
    const std::chrono::steady_clock::time_point start =
        std::chrono::steady_clock::now();
    std::uint32_t hash = 0;
    for (int frame = 0; frame < frames; ++frame) {
        for (int line = 0; line < 192; ++line)
            core.gpu3DRenderer.drawScanline(line);
        hash = hash_3d_frame(core);
    }
    const std::chrono::duration<double> elapsed =
        std::chrono::steady_clock::now() - start;
    *output_hash = hash;
    return frames / elapsed.count();
}

bool run_to_next_completed_frame(Core& core, int* scheduler_returns) {
    const std::uint32_t before = core.completedFrames;
    for (int attempt = 0; attempt < 256; ++attempt) {
        core.runCore();
        ++*scheduler_returns;
        const std::uint32_t advanced = core.completedFrames - before;
        if (advanced != 0)
            return advanced == 1;
    }
    return false;
}

bool run_capture_frame(Core& core, bool* handoff, const char* stage) {
    int scheduler_returns = 0;
    if (!run_to_next_completed_frame(core, &scheduler_returns)) {
        std::fprintf(stderr, "%s: no completed frame\n", stage);
        return false;
    }
    *handoff = core.gpu.getFrame(framebuffer, false);
    return true;
}

std::uint16_t read_capture_sample(Core& core, unsigned int x,
                                  unsigned int y) {
    return core.memory.read<std::uint16_t>(
        0, 0x06800000u + 2u * (y * 256u + x));
}

bool verify_turbo_display_capture_case(const char* rom_path, bool hidden_3d,
                                       bool late_enable, int renderer_threads,
                                       int frame_skip) {
    Settings::frameskip = 0;
    Settings::threaded2D = 0;
    Settings::threaded3D = renderer_threads;
    Core* core = new Core(rom_path);
    bool handoff = false;

    if (!run_capture_frame(*core, &handoff, "capture warmup") || !handoff) {
        std::fprintf(stderr, "capture warmup did not produce a handoff\n");
        delete core;
        return false;
    }

    core->gpu2D[0].writeDispCnt(1u << 3, 1u << 3);
    core->gpu.writePowCnt1(1u << 0, 1u << 0);

    // Establish a known rendered frame, then enter Turbo on a rendered phase.
    // The following phase is skipped by the frontend, so a fresh capture from
    // that phase proves internal rendering is independent of handoff cadence.
    set_3d_clear_color(*core, 0x001F03E0u); // Opaque green.
    if (!run_capture_frame(*core, &handoff, "normal green") || !handoff) {
        std::fprintf(stderr, "normal capture setup did not hand off\n");
        delete core;
        return false;
    }

    core->gpu.requestFrameSkip(frame_skip);
    set_3d_clear_color(*core, 0x001F03FFu); // Opaque yellow; RGB555=03FF.
    if (!run_capture_frame(*core, &handoff, "turbo yellow") || !handoff) {
        std::fprintf(stderr, "Turbo capture setup did not hand off\n");
        delete core;
        return false;
    }

    // Direct 3D capture must render even when engine A is not displaying 3D.
    // This catches implementations that only widen the frame-skip gate while
    // retaining the normal DISPCNT bit-3 renderer requirement.
    if (hidden_3d)
        core->gpu2D[0].writeDispCnt(1u << 3, 0);

    set_3d_clear_color(*core, 0x001F7C00u); // Opaque blue; RGB555=7C00.
    const std::uint32_t capture =
        (1u << 31) | // Enable.
        (1u << 24) | // Source A is direct 3D.
        (3u << 20);  // Full 256x192 capture.

    int armed_vcount = 192;
    if (late_enable) {
        // Pause immediately after scanline256(215), after the normal render
        // latch but before visible line 0. Games can legally arm capture here.
        const std::uint32_t pause_after_line_215 =
            23u * 355u * 6u + 256u * 6u + 1u;
        const std::uint32_t completed_before_pause = core->completedFrames;
        core->schedule(UPDATE_RUN, pause_after_line_215);
        core->runCore();
        armed_vcount = core->gpu.readVCount();
        if (core->completedFrames != completed_before_pause ||
            armed_vcount != 215) {
            std::fprintf(stderr,
                         "late capture pause missed: vCount=%d delta=%u\n",
                         armed_vcount,
                         core->completedFrames - completed_before_pause);
            delete core;
            return false;
        }
    }
    core->gpu.writeDispCapCnt(0xFFFFFFFFu, capture);

    bool capture_handoff = false;
    if (!run_capture_frame(*core, &capture_handoff, "capture phase")) {
        delete core;
        return false;
    }
    const std::uint16_t first = read_capture_sample(*core, 0, 0);
    const std::uint16_t middle = read_capture_sample(*core, 128, 96);
    const std::uint16_t last = read_capture_sample(*core, 255, 191);
    const bool enable_cleared =
        (core->gpu.readDispCapCnt() & (1u << 31)) == 0;

    bool next_handoff = false;
    if (!run_capture_frame(*core, &next_handoff, "post-capture phase")) {
        delete core;
        return false;
    }

    // Rearm a full-frame capture on every emulated frame. Every capture must
    // contain that frame's color, while the selected cadence exposes only its
    // intended share of the twelve frames to the frontend.
    int repeated_handoffs = 0;
    int fresh_full_frames = 0;
    int captures_cleared = 0;
    for (unsigned int frame = 0; frame < 12; ++frame) {
        const std::uint16_t rgb = static_cast<std::uint16_t>(
            ((frame + 1u) & 31u) |
            (((frame * 3u + 2u) & 31u) << 5) |
            (((frame * 5u + 3u) & 31u) << 10));
        const std::uint16_t expected = static_cast<std::uint16_t>(
            0x8000u | rgb);
        set_3d_clear_color(*core, (31u << 16) | rgb);
        core->gpu.writeDispCapCnt(0xFFFFFFFFu, capture);
        bool repeated_handoff = false;
        if (!run_capture_frame(*core, &repeated_handoff,
                               "repeated capture")) {
            delete core;
            return false;
        }
        repeated_handoffs += repeated_handoff;
        const bool full_frame_fresh =
            read_capture_sample(*core, 0, 0) == expected &&
            read_capture_sample(*core, 128, 96) == expected &&
            read_capture_sample(*core, 255, 191) == expected;
        fresh_full_frames += full_frame_fresh;
        captures_cleared +=
            (core->gpu.readDispCapCnt() & (1u << 31)) == 0;
    }

    const int expected_handoffs = frame_skip == 1 ? 6 :
        (frame_skip == 2 ? 4 : 3);
    const bool expected_next_handoff = frame_skip == 1;
    const bool passed = !capture_handoff &&
        next_handoff == expected_next_handoff &&
        first == 0xFC00u && middle == 0xFC00u && last == 0xFC00u &&
        enable_cleared && repeated_handoffs == expected_handoffs &&
        fresh_full_frames == 12 && captures_cleared == 12;
    std::printf("CAPTURE_TURBO skip=%d late=%d threads=%d hidden_3d=%d "
                "capture_handoff=%d next_handoff=%d "
                "samples=%04X,%04X,%04X enable_cleared=%d "
                "handoffs=%d/12 fresh_full_frames=%d/12 "
                "cleared=%d/12 result=%s\n",
                frame_skip, late_enable, renderer_threads, hidden_3d,
                capture_handoff, next_handoff, first, middle, last,
                enable_cleared, repeated_handoffs, fresh_full_frames,
                captures_cleared, passed ? "PASS" : "FAIL");

    delete core;
    return passed;
}

bool verify_turbo_display_capture_matrix(const char* rom_path) {
    for (int frame_skip : { 1, 2, 3 }) {
        for (int renderer_threads : { 0, 3, 4 }) {
            for (int hidden_3d = 0; hidden_3d <= 1; ++hidden_3d) {
                for (int late_enable = 0; late_enable <= 1; ++late_enable) {
                    if (!verify_turbo_display_capture_case(
                            rom_path, hidden_3d != 0, late_enable != 0,
                            renderer_threads, frame_skip))
                        return false;
                }
            }
        }
    }
    std::printf("PASS capture-turbo matrix=36 handoffs=6/12,4/12,3/12 "
                "fresh_full_frames=12/12\n");
    return true;
}

bool verify_capture_bank_alignment_matrix(const char* rom_path) {
    Settings::frameskip = 0;
    Settings::threaded2D = 0;
    Settings::threaded3D = 0;
    Core* core = new Core(rom_path);
    bool handoff = false;
    if (!run_capture_frame(*core, &handoff, "bank matrix warmup")) {
        delete core;
        return false;
    }
    core->gpu2D[0].writeDispCnt(1u << 3, 1u << 3);
    core->gpu.writePowCnt1(1u << 0, 1u << 0);
    for (int block = 0; block < 4; ++block)
        core->memory.writeVramCnt(block, 0x80);

    static const int widths[4] = {128, 256, 256, 256};
    static const int heights[4] = {128, 64, 128, 192};
    std::uint32_t hash = 2166136261u;
    int cases = 0;
    for (unsigned int bank = 0; bank < 4; ++bank) {
        const std::uint32_t base = 0x06800000u + bank * 0x20000u;
        for (unsigned int offset = 0; offset < 4; ++offset) {
            for (unsigned int size_mode = 0; size_mode < 4; ++size_mode) {
                const std::uint16_t rgb = static_cast<std::uint16_t>(
                    1u + bank | ((3u + offset) << 5) |
                    ((7u + size_mode) << 10));
                const std::uint16_t expected = 0x8000u | rgb;
                set_3d_clear_color(*core, (31u << 16) | rgb);
                const std::uint32_t capture =
                    (1u << 31) | (1u << 24) |
                    (bank << 16) | (offset << 18) |
                    (size_mode << 20);
                core->gpu.writeDispCapCnt(0xFFFFFFFFu, capture);
                if (!run_capture_frame(*core, &handoff, "bank matrix")) {
                    delete core;
                    return false;
                }
                const int width = widths[size_mode];
                const int height = heights[size_mode];
                const std::uint32_t write_offset = offset * 0x8000u;
                const int sample_x[3] = {0, width / 2, width - 1};
                const int sample_y[3] = {0, height / 2, height - 1};
                for (int sample = 0; sample < 3; ++sample) {
                    const std::uint32_t address = base +
                        ((write_offset +
                          (sample_y[sample] * width + sample_x[sample]) * 2u) &
                         0x1FFFFu);
                    const std::uint16_t actual =
                        core->memory.read<std::uint16_t>(0, address);
                    if (actual != expected) {
                        std::fprintf(stderr,
                            "capture bank matrix mismatch bank=%u offset=%u "
                            "size=%u sample=%d address=%08X expected=%04X "
                            "actual=%04X\n",
                            bank, offset, size_mode, sample, address,
                            expected, actual);
                        delete core;
                        return false;
                    }
                    hash = (hash ^ actual) * 16777619u;
                }
                ++cases;
            }
        }
    }

    // Source-B and blended captures use both direct read and write mappings.
    // A 256x64 capture is exactly 32KB; a source two slots away is disjoint.
    for (unsigned int bank = 0; bank < 4; ++bank) {
        const std::uint32_t base = 0x06800000u + bank * 0x20000u;
        for (unsigned int offset = 0; offset < 4; ++offset) {
            const unsigned int read_offset = (offset + 2u) & 3u;
            const std::uint16_t source_b = static_cast<std::uint16_t>(
                0x8000u | (2u + bank) | ((9u + offset) << 5) |
                ((17u + bank) << 10));
            for (unsigned int pixel = 0; pixel < 256u * 64u; ++pixel) {
                core->memory.write<std::uint16_t>(
                    0, base + ((read_offset * 0x8000u + pixel * 2u) &
                               0x1FFFFu), source_b);
            }

            const std::uint32_t common =
                (1u << 31) | (bank << 16) | (offset << 18) |
                (1u << 20) | (read_offset << 26);
            core->gpu.writeDispCapCnt(
                0xFFFFFFFFu, common | (1u << 29));
            if (!run_capture_frame(*core, &handoff, "source-B matrix")) {
                delete core;
                return false;
            }
            const std::uint32_t destination = base + offset * 0x8000u;
            const std::uint16_t copied =
                core->memory.read<std::uint16_t>(0, destination);
            if (copied != source_b) {
                std::fprintf(stderr,
                    "source-B matrix mismatch bank=%u offset=%u "
                    "expected=%04X actual=%04X\n",
                    bank, offset, source_b, copied);
                delete core;
                return false;
            }
            hash = (hash ^ copied) * 16777619u;
            ++cases;

            for (unsigned int pixel = 0; pixel < 256u * 64u; ++pixel) {
                core->memory.write<std::uint16_t>(
                    0, base + ((read_offset * 0x8000u + pixel * 2u) &
                               0x1FFFFu), source_b);
            }
            const std::uint16_t source_a = static_cast<std::uint16_t>(
                (21u - bank) | ((5u + offset) << 5) |
                ((3u + offset) << 10));
            set_3d_clear_color(*core, (31u << 16) | source_a);
            core->gpu.writeDispCapCnt(
                0xFFFFFFFFu,
                common | (1u << 30) | (1u << 24) | 8u | (8u << 8));
            if (!run_capture_frame(*core, &handoff, "blend matrix")) {
                delete core;
                return false;
            }
            const std::uint16_t blended =
                core->memory.read<std::uint16_t>(0, destination);
            const unsigned int expected_r =
                (((source_a >> 0) & 31u) * 8u +
                 ((source_b >> 0) & 31u) * 8u) / 16u;
            const unsigned int expected_g =
                (((source_a >> 5) & 31u) * 8u +
                 ((source_b >> 5) & 31u) * 8u) / 16u;
            const unsigned int expected_b =
                (((source_a >> 10) & 31u) * 8u +
                 ((source_b >> 10) & 31u) * 8u) / 16u;
            const std::uint16_t expected_blend = static_cast<std::uint16_t>(
                0x8000u | (expected_b << 10) |
                (expected_g << 5) | expected_r);
            if (blended != expected_blend) {
                std::fprintf(stderr,
                    "blend matrix mismatch bank=%u offset=%u "
                    "expected=%04X actual=%04X\n",
                    bank, offset, expected_blend, blended);
                delete core;
                return false;
            }
            hash = (hash ^ blended) * 16777619u;
            ++cases;
        }
    }
    std::printf("CAPTURE_BANK_MATRIX cases=%d banks=4 offsets=4 sizes=4 "
                "source_b=16 blend=16 hash=%08X PASS\n", cases, hash);
    delete core;
    return true;
}

bool verify_frame_boundaries_and_cadence(const char* rom_path) {
    Settings::frameskip = 0;
    Core* core = new Core(rom_path);

    // Force the exact UPDATE_RUN scheduler exits that regressed v0.8.4/5.
    // They are internal boundaries, not completed DS frames.
    std::uint32_t completed = core->completedFrames;
    core->interpreter[1].halt(0);
    core->runCore();
    if (core->completedFrames != completed) {
        std::fprintf(stderr, "halt UPDATE_RUN was counted as a frame\n");
        delete core;
        return false;
    }
    core->interpreter[1].unhalt(0);
    core->runCore();
    if (core->completedFrames != completed) {
        std::fprintf(stderr, "unhalt UPDATE_RUN was counted as a frame\n");
        delete core;
        return false;
    }

    int scheduler_returns = 0;
    int normal_handoffs = 0;
    for (int frame = 0; frame < 12; ++frame) {
        if (!run_to_next_completed_frame(*core, &scheduler_returns)) {
            std::fprintf(stderr, "normal frame %d did not complete\n", frame);
            delete core;
            return false;
        }
        normal_handoffs += core->gpu.getFrame(framebuffer, false);
    }
    if (normal_handoffs != 12) {
        std::fprintf(stderr,
                     "normal cadence produced %d/12 framebuffer handoffs\n",
                     normal_handoffs);
        delete core;
        return false;
    }

    core->gpu.requestFrameSkip(1);
    int turbo_handoffs = 0;
    for (int frame = 0; frame < 12; ++frame) {
        if (!run_to_next_completed_frame(*core, &scheduler_returns)) {
            std::fprintf(stderr, "turbo frame %d did not complete\n", frame);
            delete core;
            return false;
        }
        turbo_handoffs += core->gpu.getFrame(framebuffer, false);
    }
    if (turbo_handoffs != 6) {
        std::fprintf(stderr,
                     "turbo cadence produced %d/6 framebuffer handoffs\n",
                     turbo_handoffs);
        delete core;
        return false;
    }

    core->gpu.requestFrameSkip(2);
    int adaptive_handoffs = 0;
    for (int frame = 0; frame < 12; ++frame) {
        if (!run_to_next_completed_frame(*core, &scheduler_returns)) {
            std::fprintf(stderr, "adaptive frame %d did not complete\n", frame);
            delete core;
            return false;
        }
        adaptive_handoffs += core->gpu.getFrame(framebuffer, false);
    }
    if (adaptive_handoffs != 4) {
        std::fprintf(stderr,
                     "adaptive cadence produced %d/4 framebuffer handoffs\n",
                     adaptive_handoffs);
        delete core;
        return false;
    }

    core->gpu.requestFrameSkip(3);
    int quadruple_handoffs = 0;
    for (int frame = 0; frame < 12; ++frame) {
        if (!run_to_next_completed_frame(*core, &scheduler_returns)) {
            std::fprintf(stderr, "quadruple frame %d did not complete\n", frame);
            delete core;
            return false;
        }
        quadruple_handoffs += core->gpu.getFrame(framebuffer, false);
    }
    if (quadruple_handoffs != 3) {
        std::fprintf(stderr,
                     "quadruple cadence produced %d/3 framebuffer handoffs\n",
                     quadruple_handoffs);
        delete core;
        return false;
    }

    core->gpu.requestFrameSkip(0);
    int restored_handoffs = 0;
    for (int frame = 0; frame < 8; ++frame) {
        if (!run_to_next_completed_frame(*core, &scheduler_returns)) {
            std::fprintf(stderr, "restored frame %d did not complete\n", frame);
            delete core;
            return false;
        }
        restored_handoffs += core->gpu.getFrame(framebuffer, false);
    }
    delete core;
    if (restored_handoffs != 8) {
        std::fprintf(stderr,
                     "restored cadence produced %d/8 framebuffer handoffs\n",
                     restored_handoffs);
        return false;
    }

    std::printf("CADENCE normal=12/12 skip1=6/12 skip2=4/12 "
                "skip3=3/12 restored=8/8 "
                "scheduler_returns=%d\n", scheduler_returns);
    return true;
}

bool stress_live_cadence(const char* rom_path, int frame_count) {
    Settings::frameskip = 0;
    Core* core = new Core(rom_path);
    std::uint32_t random = 0x58444E53u;
    int requested = 0;
    int active = 0;
    int phase = 0;
    int scheduler_returns = 0;
    int handoffs = 0;
    int maximum_gap = 0;
    int current_gap = 0;

    for (int frame = 0; frame < frame_count; ++frame) {
        random = random * 1664525u + 1013904223u;
        if ((random & 0x0Fu) == 0) {
            static const int skips[] = { 0, 1, 2, 3 };
            requested = skips[(random >> 8) % 4];
            core->gpu.requestFrameSkip(requested);
        }

        if (requested != active) {
            active = requested;
            phase = 0;
        }
        const bool expected_handoff = phase == 0;
        if (phase >= active)
            phase = 0;
        else
            ++phase;

        if (!run_to_next_completed_frame(*core, &scheduler_returns)) {
            std::fprintf(stderr, "cadence stress frame %d did not complete\n", frame);
            delete core;
            return false;
        }
        const bool handoff = core->gpu.getFrame(framebuffer, false);
        if (handoff != expected_handoff) {
            std::fprintf(stderr,
                         "cadence mismatch at frame %d: requested=%d "
                         "expected=%d actual=%d\n",
                         frame, requested, expected_handoff, handoff);
            delete core;
            return false;
        }
        if (handoff) {
            ++handoffs;
            current_gap = 0;
        }
        else {
            ++current_gap;
            maximum_gap = std::max(maximum_gap, current_gap);
        }
    }

    std::printf("CADENCE_STRESS frames=%d handoffs=%d max_gap=%d "
                "scheduler_returns=%d hash=%08X\n",
                frame_count, handoffs, maximum_gap,
                scheduler_returns, hash_frame());
    delete core;
    return maximum_gap <= 3;
}

std::uint16_t quantize_rgb8_to_rgb555(std::uint32_t color) {
    return static_cast<std::uint16_t>(
        ((color >> 3) & 0x001Fu) |
        ((color >> 6) & 0x03E0u) |
        ((color >> 9) & 0x7C00u));
}

std::uint32_t hash_rgb555(const std::uint16_t* pixels) {
    std::uint32_t hash = 2166136261u;
    for (std::size_t i = 0; i < 256u * 192u * 2u; ++i) {
        hash ^= pixels[i];
        hash *= 16777619u;
    }
    return hash;
}

bool next_generic_frame(Core& core, std::uint32_t* output) {
    for (int attempt = 0; attempt < 256; ++attempt) {
        core.runCore();
        if (core.gpu.getFrame(output, false)) return true;
    }
    return false;
}

bool next_xenon_frame(Core& core, std::uint16_t* output) {
    for (int attempt = 0; attempt < 256; ++attempt) {
        core.runCore();
        if (core.gpu.getFrameXenon(output, false)) return true;
    }
    return false;
}

bool verify_xenon_frame_path(const char* rom_path) {
    const int frame_count = 20;
    const std::size_t pixel_count = 256u * 192u * 2u;
    std::vector<std::uint32_t> generic(pixel_count);
    std::vector<std::uint16_t> reduced(pixel_count);
    std::uint32_t expected[frame_count] = {};
    std::uint32_t actual[frame_count] = {};

    Core* generic_core = new Core(rom_path);
    for (int frame = 0; frame < frame_count; ++frame) {
        if (!next_generic_frame(*generic_core, generic.data())) {
            std::fprintf(stderr,
                         "generic framebuffer path stalled at frame %d\n",
                         frame);
            delete generic_core;
            return false;
        }
        for (std::size_t pixel = 0; pixel < pixel_count; ++pixel)
            reduced[pixel] = quantize_rgb8_to_rgb555(generic[pixel]);
        expected[frame] = hash_rgb555(reduced.data());
    }
    delete generic_core;

    Core* xenon_core = new Core(rom_path);
    for (int frame = 0; frame < frame_count; ++frame) {
        if (!next_xenon_frame(*xenon_core, reduced.data())) {
            std::fprintf(stderr,
                         "Xenon framebuffer path stalled at frame %d\n",
                         frame);
            delete xenon_core;
            return false;
        }
        actual[frame] = hash_rgb555(reduced.data());
    }
    delete xenon_core;

    for (int frame = 0; frame < frame_count; ++frame) {
        if (actual[frame] != expected[frame]) {
            std::fprintf(stderr,
                         "frame path mismatch frame=%d expected=%08X "
                         "actual=%08X\n",
                         frame, expected[frame], actual[frame]);
            return false;
        }
    }
    std::printf("PASS framepath frames=%d hash=%08X\n",
                frame_count, actual[frame_count - 1]);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 4) {
        std::fprintf(stderr,
                     "usage: noods_core_smoke ROM.nds "
                     "[--boot | --benchmark FRAMES | --renderer-benchmark FRAMES "
                     "| --cadence-stress FRAMES | --capture-regression "
                     "| --capture-bank-matrix | --framepath-regression]\n");
        return 2;
    }
    const bool boot_probe = argc == 3 && std::strcmp(argv[2], "--boot") == 0;
    const bool benchmark = argc == 4 &&
        std::strcmp(argv[2], "--benchmark") == 0;
    const bool renderer_benchmark = argc == 4 &&
        std::strcmp(argv[2], "--renderer-benchmark") == 0;
    const bool cadence_stress = argc == 4 &&
        std::strcmp(argv[2], "--cadence-stress") == 0;
    const bool capture_regression = argc == 3 &&
        std::strcmp(argv[2], "--capture-regression") == 0;
    const bool capture_bank_matrix = argc == 3 &&
        std::strcmp(argv[2], "--capture-bank-matrix") == 0;
    const bool framepath_regression = argc == 3 &&
        std::strcmp(argv[2], "--framepath-regression") == 0;
    const int benchmark_frames = benchmark ? std::atoi(argv[3]) : 0;
    const int renderer_frames = renderer_benchmark ? std::atoi(argv[3]) : 0;
    const int cadence_frames = cadence_stress ? std::atoi(argv[3]) : 0;
    if ((argc == 3 && !boot_probe && !capture_regression &&
         !capture_bank_matrix && !framepath_regression) ||
        (argc == 4 && ((!benchmark && !renderer_benchmark && !cadence_stress) ||
                       (benchmark && benchmark_frames < 1) ||
                       (renderer_benchmark && renderer_frames < 1) ||
                       (cadence_stress && cadence_frames < 1)))) {
        std::fprintf(stderr, "unknown mode: %s\n", argv[2]);
        return 2;
    }

    Settings::directBoot = 1;
    Settings::romInRam = 1;
    Settings::fpsLimiter = 0;
    Settings::frameskip = 0;
    Settings::threaded2D = 0;
    Settings::threaded3D = 0;
    Settings::highRes3D = 0;
    Settings::screenGhost = 0;
    Settings::emulateAudio = 0;
    Settings::savesFolder = 0;
    Settings::statesFolder = 0;
    Settings::cheatsFolder = 0;
    Settings::screenFilter = 0;
    Settings::dsiMode = 0;
    Settings::arm7Hle = 0;

    try {
        if (!boot_probe && !benchmark && !renderer_benchmark &&
            !cadence_stress && !capture_regression &&
            !framepath_regression) {
            if (!verify_frame_boundaries_and_cadence(argv[1])) {
                return 1;
            }
            Settings::frameskip = 0;
        }

        if (capture_regression)
            return verify_turbo_display_capture_matrix(argv[1]) ? 0 : 1;

        if (capture_bank_matrix)
            return verify_capture_bank_alignment_matrix(argv[1]) ? 0 : 1;

        if (framepath_regression)
            return verify_xenon_frame_path(argv[1]) ? 0 : 1;

        Core* core = new Core(argv[1]);

        if (cadence_stress) {
            delete core;
            return stress_live_cadence(argv[1], cadence_frames) ? 0 : 1;
        }

        if (renderer_benchmark) {
            prepare_3d_benchmark(*core);
            std::uint32_t serial_hash = 0;
            std::uint32_t three_thread_hash = 0;
            std::uint32_t threaded_hash = 0;
            const double serial_fps = benchmark_3d(
                *core, renderer_frames, 0, &serial_hash);
            const double three_thread_fps = benchmark_3d(
                *core, renderer_frames, 3, &three_thread_hash);
            const double threaded_fps = benchmark_3d(
                *core, renderer_frames, 4, &threaded_hash);
            if (serial_hash != three_thread_hash ||
                serial_hash != threaded_hash) {
                std::fprintf(stderr,
                             "3D thread mismatch: serial=%08X three=%08X "
                             "four=%08X\n",
                             serial_hash, three_thread_hash, threaded_hash);
                delete core;
                return 1;
            }
            std::printf("RENDER_BENCH frames=%d serial_fps=%.2f "
                        "three_fps=%.2f four_fps=%.2f three_speedup=%.2fx "
                        "four_speedup=%.2fx hash=%08X\n",
                        renderer_frames, serial_fps, three_thread_fps,
                        threaded_fps, three_thread_fps / serial_fps,
                        threaded_fps / serial_fps, serial_hash);
            delete core;
            return 0;
        }

        if (benchmark) {
            // Warm caches and one-time core paths before measuring steady-state
            // emulation. Frame retrieval remains in the loop because it is part
            // of the Xbox frontend's per-frame core cost.
            for (int frame = 0; frame < 10; ++frame) {
                do {
                    core->runCore();
                } while (!core->gpu.getFrame(framebuffer, false));
            }

            const std::chrono::steady_clock::time_point start =
                std::chrono::steady_clock::now();
            for (int frame = 0; frame < benchmark_frames; ++frame) {
                do {
                    core->runCore();
                } while (!core->gpu.getFrame(framebuffer, false));
            }
            const std::chrono::duration<double> elapsed =
                std::chrono::steady_clock::now() - start;
            const double fps = benchmark_frames / elapsed.count();
            std::printf("BENCH frames=%d seconds=%.6f fps=%.2f hash=%08X\n",
                        benchmark_frames, elapsed.count(), fps, hash_frame());
            delete core;
            return 0;
        }

        const int frame_limit = boot_probe ? 300 : 3;
        bool saw_content = false;
        std::uint32_t first_hash = 0;
        bool saw_change = false;
        int frames_received = 0;
        for (int frame = 0; frame < frame_limit; ++frame) {
            int attempts = 0;
            while (attempts < 64) {
                core->runCore();
                ++attempts;
                if (core->gpu.getFrame(framebuffer, false))
                    break;
            }
            if (attempts == 64) {
                std::fprintf(stderr,
                             "frame %d was not queued after 64 scheduler calls\n",
                             frame);
                delete core;
                return 1;
            }

            ++frames_received;
            const std::uint32_t hash = hash_frame();
            if (frames_received == 1)
                first_hash = hash;
            else if (hash != first_hash)
                saw_change = true;
            const std::uint32_t first = framebuffer[0];
            for (std::size_t i = 1; i < 256u * 192u * 2u; ++i) {
                if (framebuffer[i] != first) {
                    saw_content = true;
                    break;
                }
            }
        }

        if (boot_probe) {
            if (frames_received == 0 || !saw_content || !saw_change) {
                std::fprintf(stderr,
                             "boot probe stayed blank/static: frames=%d content=%d changed=%d "
                             "hash=%08X ARM9=%08X ARM7=%08X\n",
                             frames_received, saw_content, saw_change, hash_frame(),
                             core->interpreter[0].getPC(),
                             core->interpreter[1].getPC());
                delete core;
                return 1;
            }
            if (core->interpreter[0].unknownOpcodeCount != 0 ||
                core->interpreter[1].unknownOpcodeCount != 0) {
                std::fprintf(stderr,
                             "unexpected opcodes: ARM9=%u ARM7=%u\n",
                             core->interpreter[0].unknownOpcodeCount,
                             core->interpreter[1].unknownOpcodeCount);
                delete core;
                return 1;
            }
            std::printf("PASS boot frames=%d/%d hash=%08X ARM9=%08X ARM7=%08X\n",
                        frames_received, frame_limit, hash_frame(),
                        core->interpreter[0].getPC(),
                        core->interpreter[1].getPC());
            delete core;
            return 0;
        }

        core->input.pressKey(0);
        if (core->input.readKeyInput() & 1u) {
            std::fprintf(stderr, "A press was not registered\n");
            return 1;
        }
        core->input.releaseKey(0);
        if (!(core->input.readKeyInput() & 1u)) {
            std::fprintf(stderr, "A release was not registered\n");
            return 1;
        }
        core->spi.setTouch(128, 96);
        core->input.pressScreen();
        if (core->input.readExtKeyIn() & (1u << 6)) {
            std::fprintf(stderr, "touch press was not registered\n");
            return 1;
        }

        const std::size_t red_pixels = count_red_pixels();
        if (red_pixels < 256u * 190u) {
            std::fprintf(stderr,
                         "rendered VRAM test was incomplete: %zu red pixels "
                         "samples=%08X,%08X,%08X,%08X\n",
                         red_pixels, framebuffer[0], framebuffer[256u * 96u],
                         framebuffer[256u * 192u], framebuffer[256u * 288u]);
            delete core;
            return 1;
        }

        if (core->interpreter[0].unknownOpcodeCount != 0 ||
            core->interpreter[1].unknownOpcodeCount != 0) {
            std::fprintf(stderr,
                         "unexpected opcodes: ARM9=%u ARM7=%u\n",
                         core->interpreter[0].unknownOpcodeCount,
                         core->interpreter[1].unknownOpcodeCount);
            delete core;
            return 1;
        }

        std::printf("PASS frames=3 hash=%08X red=%zu touch=%03X,%03X\n",
                    hash_frame(), red_pixels, core->spi.touchX, core->spi.touchY);
        delete core;
        return 0;
    }
    catch (int error) {
        std::fprintf(stderr, "NooDS error %d\n", error);
        return 1;
    }
    catch (const std::bad_alloc&) {
        std::fprintf(stderr, "allocation failed\n");
        return 1;
    }
}

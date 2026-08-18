// SPDX-License-Identifier: GPL-3.0-or-later
#include "core.h"
#include "settings.h"

#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

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

} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 4) {
        std::fprintf(stderr,
                     "usage: noods_core_smoke ROM.nds "
                     "[--boot | --benchmark FRAMES | --renderer-benchmark FRAMES]\n");
        return 2;
    }
    const bool boot_probe = argc == 3 && std::strcmp(argv[2], "--boot") == 0;
    const bool benchmark = argc == 4 &&
        std::strcmp(argv[2], "--benchmark") == 0;
    const bool renderer_benchmark = argc == 4 &&
        std::strcmp(argv[2], "--renderer-benchmark") == 0;
    const int benchmark_frames = benchmark ? std::atoi(argv[3]) : 0;
    const int renderer_frames = renderer_benchmark ? std::atoi(argv[3]) : 0;
    if ((argc == 3 && !boot_probe) ||
        (argc == 4 && ((!benchmark && !renderer_benchmark) ||
                       (benchmark && benchmark_frames < 1) ||
                       (renderer_benchmark && renderer_frames < 1)))) {
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
        Core* core = new Core(argv[1]);

        if (renderer_benchmark) {
            prepare_3d_benchmark(*core);
            std::uint32_t serial_hash = 0;
            std::uint32_t threaded_hash = 0;
            const double serial_fps = benchmark_3d(
                *core, renderer_frames, 0, &serial_hash);
            const double threaded_fps = benchmark_3d(
                *core, renderer_frames, 4, &threaded_hash);
            if (serial_hash != threaded_hash) {
                std::fprintf(stderr,
                             "3D thread mismatch: serial=%08X threaded=%08X\n",
                             serial_hash, threaded_hash);
                delete core;
                return 1;
            }
            std::printf("RENDER_BENCH frames=%d serial_fps=%.2f "
                        "threaded_fps=%.2f speedup=%.2fx hash=%08X\n",
                        renderer_frames, serial_fps, threaded_fps,
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
            core->runCore();
            if (!core->gpu.getFrame(framebuffer, false)) {
                if (boot_probe || frames_received == 0)
                    continue;
                std::fprintf(stderr, "frame %d was not queued\n", frame);
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

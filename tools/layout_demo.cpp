// SPDX-License-Identifier: GPL-2.0-only
#include "xenonds/screen_layout.hpp"
#include "xenonds/types.hpp"
#include "xenonds/video_compositor.hpp"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::uint16_t bgr555(std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
    return static_cast<std::uint16_t>((red >> 3u) |
           ((green >> 3u) << 5u) |
           ((blue >> 3u) << 10u));
}

void make_test_frame(xenonds::FrameOutput* frame) {
    for (std::size_t y = 0; y < xenonds::kScreenHeight; ++y) {
        for (std::size_t x = 0; x < xenonds::kScreenWidth; ++x) {
            const std::uint8_t lime = static_cast<std::uint8_t>(80u + x * 175u / xenonds::kScreenWidth);
            const std::uint8_t cyan = static_cast<std::uint8_t>(70u + y * 185u / xenonds::kScreenHeight);
            frame->pixels[y * xenonds::kScreenWidth + x] = bgr555(lime, cyan, 20);

            const bool checker = ((x / 24u) + (y / 24u)) % 2u == 0u;
            const std::size_t bottom = xenonds::kScreenWidth * xenonds::kScreenHeight +
                                       y * xenonds::kScreenWidth + x;
            frame->pixels[bottom] = checker ? bgr555(20, 200, 255) : bgr555(10, 25, 40);
        }
    }
}

bool write_ppm(const std::string& path,
               std::uint32_t width,
               std::uint32_t height,
               const std::vector<std::uint32_t>& pixels) {
    std::ofstream output(path.c_str(), std::ios::binary);
    if (!output) {
        return false;
    }
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        const char rgb[3] = {
            static_cast<char>((pixels[i] >> 16u) & 0xFFu),
            static_cast<char>((pixels[i] >> 8u) & 0xFFu),
            static_cast<char>(pixels[i] & 0xFFu)
        };
        output.write(rgb, sizeof(rgb));
    }
    return output.good();
}

} // namespace

int main(int argc, char** argv) {
    const std::string output_path = argc > 1 ? argv[1] : "xenonds-layout-demo.ppm";
    const std::uint32_t width = 1280;
    const std::uint32_t height = 720;

    xenonds::FrameOutput frame;
    make_test_frame(&frame);

    xenonds::ScreenPlacement placement;
    xenonds::Status status = xenonds::calculate_screen_placement(
        width, height, xenonds::ScreenLayoutMode::horizontal, 32, &placement);
    if (!status.ok()) {
        std::cerr << status.message() << '\n';
        return EXIT_FAILURE;
    }

    std::vector<std::uint32_t> composed;
    status = xenonds::compose_frame_nearest(frame, placement, width, height,
                                            0xFF090B10u, &composed);
    if (!status.ok()) {
        std::cerr << status.message() << '\n';
        return EXIT_FAILURE;
    }
    if (!write_ppm(output_path, width, height, composed)) {
        std::cerr << "Could not write " << output_path << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "Wrote synthetic dual-screen preview to " << output_path << '\n';
    return EXIT_SUCCESS;
}

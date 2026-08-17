// SPDX-License-Identifier: GPL-2.0-only
#include "xenonds/checksum.hpp"
#include "xenonds/controller_mapper.hpp"
#include "xenonds/screen_layout.hpp"
#include "xenonds/session.hpp"
#include "xenonds/video_compositor.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::cerr << __FILE__ << ':' << __LINE__ << " CHECK failed: " #condition "\n"; \
        ++failures; \
    } \
} while (0)

void write_u16_le(std::vector<std::uint8_t>* data, std::size_t offset, std::uint16_t value) {
    (*data)[offset] = static_cast<std::uint8_t>(value & 0xFFu);
    (*data)[offset + 1] = static_cast<std::uint8_t>((value >> 8u) & 0xFFu);
}

void write_u32_le(std::vector<std::uint8_t>* data, std::size_t offset, std::uint32_t value) {
    (*data)[offset] = static_cast<std::uint8_t>(value & 0xFFu);
    (*data)[offset + 1] = static_cast<std::uint8_t>((value >> 8u) & 0xFFu);
    (*data)[offset + 2] = static_cast<std::uint8_t>((value >> 16u) & 0xFFu);
    (*data)[offset + 3] = static_cast<std::uint8_t>((value >> 24u) & 0xFFu);
}

std::vector<std::uint8_t> make_test_rom() {
    std::vector<std::uint8_t> rom(0x8000, 0);
    const char title[] = "XENONDS TEST";
    const char code[] = "XNDE";
    const char maker[] = "00";
    for (std::size_t i = 0; i < 12; ++i) rom[i] = static_cast<std::uint8_t>(title[i]);
    for (std::size_t i = 0; i < 4; ++i) rom[0x0C + i] = static_cast<std::uint8_t>(code[i]);
    for (std::size_t i = 0; i < 2; ++i) rom[0x10 + i] = static_cast<std::uint8_t>(maker[i]);

    write_u32_le(&rom, 0x20, 0x4000);
    write_u32_le(&rom, 0x24, 0x02000000);
    write_u32_le(&rom, 0x28, 0x02000000);
    write_u32_le(&rom, 0x2C, 0x1000);
    write_u32_le(&rom, 0x30, 0x5000);
    write_u32_le(&rom, 0x34, 0x03800000);
    write_u32_le(&rom, 0x38, 0x03800000);
    write_u32_le(&rom, 0x3C, 0x1000);
    write_u32_le(&rom, 0x80, static_cast<std::uint32_t>(rom.size()));
    write_u32_le(&rom, 0x84, 0x4000);
    write_u16_le(&rom, 0x15E, xenonds::crc16_nintendo(rom.data(), 0x15E));
    return rom;
}

class RecordingBackend : public xenonds::CoreBackend {
public:
    RecordingBackend()
        : initialized(false), loaded(false), frames(0), last_touch_x(0), last_touch_y(0) {}

    virtual const char* name() const { return "test"; }
    virtual xenonds::Status initialize(const xenonds::CoreConfig&) {
        initialized = true;
        return xenonds::Status::Ok();
    }
    virtual xenonds::Status load_rom(const xenonds::RomView&) {
        loaded = true;
        return xenonds::Status::Ok();
    }
    virtual xenonds::Status reset() { return xenonds::Status::Ok(); }
    virtual xenonds::Status run_frame(const xenonds::InputState& input, xenonds::FrameOutput*) {
        ++frames;
        last_touch_x = input.touch.x;
        last_touch_y = input.touch.y;
        return xenonds::Status::Ok();
    }
    virtual void unload_rom() { loaded = false; }
    virtual void shutdown() { initialized = false; }

    bool initialized;
    bool loaded;
    int frames;
    std::uint16_t last_touch_x;
    std::uint16_t last_touch_y;
};

void test_crc_known_vector() {
    const std::uint8_t input[] = {'1','2','3','4','5','6','7','8','9'};
    CHECK(xenonds::crc16_nintendo(input, sizeof(input)) == 0x4B37u);
}

void test_header_parser() {
    std::vector<std::uint8_t> rom = make_test_rom();
    xenonds::NdsHeader header;
    xenonds::Status status = xenonds::parse_nds_header(rom.data(), rom.size(), &header);
    CHECK(status.ok());
    CHECK(header.title == "XENONDS TEST");
    CHECK(header.game_code == "XNDE");
    CHECK(header.arm9.offset == 0x4000u);

    rom[0x00] ^= 1u;
    status = xenonds::parse_nds_header(rom.data(), rom.size(), &header);
    CHECK(status.code() == xenonds::ErrorCode::header_crc_mismatch);
}

void test_homebrew_game_code() {
    std::vector<std::uint8_t> rom = make_test_rom();
    rom[0x0C] = '#';
    rom[0x0D] = '#';
    rom[0x0E] = '#';
    rom[0x0F] = '#';
    write_u16_le(&rom, 0x15E, xenonds::crc16_nintendo(rom.data(), 0x15E));

    xenonds::NdsHeader header;
    const xenonds::Status status = xenonds::parse_nds_header(rom.data(), rom.size(), &header);
    CHECK(status.ok());
    CHECK(header.game_code == "####");
}

void test_region_bounds() {
    std::vector<std::uint8_t> rom = make_test_rom();
    write_u32_le(&rom, 0x2C, 0x5000);
    write_u16_le(&rom, 0x15E, xenonds::crc16_nintendo(rom.data(), 0x15E));
    xenonds::NdsHeader header;
    const xenonds::Status status = xenonds::parse_nds_header(rom.data(), rom.size(), &header);
    CHECK(status.code() == xenonds::ErrorCode::rom_region_out_of_bounds);
}

void test_session_lifecycle() {
    std::vector<std::uint8_t> rom = make_test_rom();
    RecordingBackend backend;
    xenonds::Session session(&backend);
    xenonds::CoreConfig config;
    CHECK(session.initialize(config).ok());
    CHECK(session.load_rom(rom.data(), rom.size(), "test.nds").ok());
    CHECK(session.start().ok());

    xenonds::InputState input;
    input.touch.pressed = true;
    input.touch.x = 999;
    input.touch.y = 999;
    xenonds::FrameOutput frame;
    CHECK(session.run_frame(input, &frame).ok());
    CHECK(backend.frames == 1);
    CHECK(backend.last_touch_x == 255);
    CHECK(backend.last_touch_y == 191);
    CHECK(session.pause().ok());
    session.stop();
    CHECK(!backend.initialized);
}

void test_screen_layouts() {
    xenonds::ScreenPlacement placement;
    CHECK(xenonds::calculate_screen_placement(1280, 720,
                                              xenonds::ScreenLayoutMode::horizontal,
                                              12, &placement).ok());
    CHECK(placement.integer_scale == 2u);
    CHECK(placement.top.width == 512);
    CHECK(placement.bottom.x - placement.top.x == 524);
    CHECK(placement.show_top && placement.show_bottom);

    CHECK(xenonds::calculate_screen_placement(640, 480,
                                              xenonds::ScreenLayoutMode::vertical,
                                              4, &placement).ok());
    CHECK(placement.integer_scale == 1u);
    CHECK(placement.bottom.y - placement.top.y == 196);

    CHECK(xenonds::calculate_screen_placement(255, 191,
                                              xenonds::ScreenLayoutMode::top_only,
                                              0, &placement).code() == xenonds::ErrorCode::invalid_argument);
}

void test_controller_mapper() {
    xenonds::ControllerMapper mapper;
    xenonds::ControllerSnapshot controller;
    controller.south = true;
    controller.east = true;
    controller.left_shoulder = true;
    controller.right_x = 32767;
    controller.right_y = -32767;
    controller.right_trigger = 255;

    const xenonds::InputState input = mapper.map(controller);
    CHECK((input.buttons & xenonds::button_b) != 0);
    CHECK((input.buttons & xenonds::button_a) != 0);
    CHECK((input.buttons & xenonds::button_l) != 0);
    CHECK(input.touch.pressed);
    CHECK(input.touch.x > xenonds::kScreenWidth / 2);
    CHECK(input.touch.y > xenonds::kScreenHeight / 2);

    mapper.set_touch_cursor(255, 191);
    mapper.map(controller);
    CHECK(mapper.touch_x() == 255);
    CHECK(mapper.touch_y() == 191);
}

void test_video_compositor() {
    CHECK(xenonds::bgr555_to_xrgb8888(0x0000u) == 0xFF000000u);
    CHECK(xenonds::bgr555_to_xrgb8888(0x001Fu) == 0xFFFF0000u);
    CHECK(xenonds::bgr555_to_xrgb8888(0x03E0u) == 0xFF00FF00u);
    CHECK(xenonds::bgr555_to_xrgb8888(0x7C00u) == 0xFF0000FFu);

    xenonds::FrameOutput frame;
    std::fill(frame.pixels.begin(),
              frame.pixels.begin() + xenonds::kScreenWidth * xenonds::kScreenHeight,
              0x001Fu);
    std::fill(frame.pixels.begin() + xenonds::kScreenWidth * xenonds::kScreenHeight,
              frame.pixels.end(), 0x7C00u);

    xenonds::ScreenPlacement placement;
    CHECK(xenonds::calculate_screen_placement(512, 192,
                                              xenonds::ScreenLayoutMode::horizontal,
                                              0, &placement).ok());
    std::vector<std::uint32_t> composed;
    CHECK(xenonds::compose_frame_nearest(frame, placement, 512, 192,
                                         0xFF090B10u, &composed).ok());
    CHECK(composed.size() == 512u * 192u);
    CHECK(composed[0] == 0xFFFF0000u);
    CHECK(composed[511] == 0xFF0000FFu);
}

} // namespace

int main() {
    test_crc_known_vector();
    test_header_parser();
    test_homebrew_game_code();
    test_region_bounds();
    test_session_lifecycle();
    test_screen_layouts();
    test_controller_mapper();
    test_video_compositor();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All XenonDS core tests passed\n";
    return EXIT_SUCCESS;
}

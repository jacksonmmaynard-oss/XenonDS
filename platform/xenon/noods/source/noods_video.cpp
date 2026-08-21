// SPDX-License-Identifier: GPL-3.0-or-later
#include "noods_video.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" {
#include <ppc/cache.h>
#include <ppc/timebase.h>
#include <xenon_soc/xenon_power.h>
}

namespace xenonds {
namespace xenon {
namespace {

struct Framebuffer {
    std::uint32_t* pixels;
    int visible_width;
    int visible_height;
    int padded_width;
    int padded_height;
};

Framebuffer output = {};
bool initialized = false;
// Keep the main emulation context (PIR 0) alone on its physical core. NooDS
// software 3D uses contexts 2, 3, and 4; presentation shares the final core on
// context 5 only after a completed image has been queued.
const int kVideoThread = 5;
// This secondary bound guarantees progress even if the timebase stops
// advancing. Under normal operation the requested time limit wins first.
const unsigned long kMaximumVideoWaitPolls = 10000000UL;
alignas(16) unsigned char worker_stack[64 * 1024];
std::uint16_t queued_frame[kCombinedPixelCount];
TouchState queued_touch;
PerformanceStats queued_stats;
std::uint32_t color_lut[32 * 32 * 32];
const std::uint32_t kMatteColor = 0x090B1000u;

int clamp_channel(int value) {
    return value < 0 ? 0 : (value > 255 ? 255 : value);
}

void rebuild_color_lut(const PictureSettings& settings) {
    for (unsigned int blue5 = 0; blue5 < 32; ++blue5) {
        for (unsigned int green5 = 0; green5 < 32; ++green5) {
            for (unsigned int red5 = 0; red5 < 32; ++red5) {
                int red = static_cast<int>((red5 << 3) | (red5 >> 2));
                int green = static_cast<int>((green5 << 3) | (green5 >> 2));
                int blue = static_cast<int>((blue5 << 3) | (blue5 >> 2));

                // Contrast is intentionally anchored at black. The default
                // 78% plus +8 brightness reproduces the safer 8..207 analog
                // output range used by v0.8.1 without clipping pale colors.
                red = clamp_channel(
                    settings.brightness + red * static_cast<int>(settings.contrast) / 100);
                green = clamp_channel(
                    settings.brightness + green * static_cast<int>(settings.contrast) / 100);
                blue = clamp_channel(
                    settings.brightness + blue * static_cast<int>(settings.contrast) / 100);

                const int luma = (77 * red + 150 * green + 29 * blue) >> 8;
                red = clamp_channel(
                    luma + (red - luma) * static_cast<int>(settings.saturation) / 100);
                green = clamp_channel(
                    luma + (green - luma) * static_cast<int>(settings.saturation) / 100);
                blue = clamp_channel(
                    luma + (blue - luma) * static_cast<int>(settings.saturation) / 100);

                const std::size_t index = red5 | (green5 << 5) | (blue5 << 10);
                color_lut[index] = (static_cast<std::uint32_t>(blue) << 24) |
                                   (static_cast<std::uint32_t>(green) << 16) |
                                   (static_cast<std::uint32_t>(red) << 8);
            }
        }
    }
}

std::size_t tiled_index(int x, int y, int width) {
    return static_cast<std::size_t>(
        ((y >> 5) * 32 * width + ((x >> 5) << 10) +
         (x & 3) + ((y & 1) << 2) + (((x & 31) >> 2) << 3) +
         (((y & 31) >> 1) << 6)) ^ ((y & 8) << 2));
}

void put_pixel(const Framebuffer& framebuffer, int x, int y, std::uint32_t color) {
    if (x < 0 || y < 0 || x >= framebuffer.visible_width ||
        y >= framebuffer.visible_height) {
        return;
    }
    framebuffer.pixels[tiled_index(x, y, framebuffer.padded_width)] = color;
}

void put_pixel_unchecked(const Framebuffer& framebuffer, int x, int y,
                         std::uint32_t color) {
    framebuffer.pixels[tiled_index(x, y, framebuffer.padded_width)] = color;
}

std::uint32_t xenos_color(std::uint16_t rgb555) {
    // The Xenon-specific NooDS handoff is already quantized to the exact
    // RGB555 index consumed by this lookup. This removes a redundant RGB6 to
    // RGB8 expansion, three per-pixel masks, and half of the queue traffic.
    return color_lut[rgb555 & 0x7FFFu];
}

void draw_screen(const Framebuffer& framebuffer,
                 const std::uint16_t* source,
                 int destination_x,
                 int destination_y,
                 int scale) {
    // The 640x480 output used by retail Xenon consoles is scale 1. Keep that
    // overwhelmingly common path free of per-pixel bounds checks and nested
    // scale loops; the placement calculation guarantees the rectangle fits.
    if (scale == 1 && destination_x >= 0 && destination_y >= 0 &&
        destination_x + static_cast<int>(kScreenWidth) <= framebuffer.visible_width &&
        destination_y + static_cast<int>(kScreenHeight) <= framebuffer.visible_height) {
        // Four neighboring Xenos pixels are contiguous when the first X is
        // four-pixel aligned. Both 640x480 screen placements satisfy this, so
        // calculate the tiled address once per group instead of once per pixel.
        if ((destination_x & 3) == 0) {
            // Xenos' horizontal swizzle is identical on every source row.
            // Precompute its 64 four-pixel group offsets once, then combine it
            // with the much smaller vertical term below. This avoids repeating
            // the full tiling equation 12,288 times per DS screen.
            std::uint32_t x_offsets[kScreenWidth / 4];
            for (std::size_t group = 0; group < kScreenWidth / 4; ++group) {
                const int x = destination_x + static_cast<int>(group * 4);
                x_offsets[group] = static_cast<std::uint32_t>(
                    ((x >> 5) << 10) + (((x & 31) >> 2) << 3));
            }

            const std::uint16_t* source_pixel = source;
            for (std::size_t source_y = 0; source_y < kScreenHeight; ++source_y) {
                const int y = destination_y + static_cast<int>(source_y);
                const std::size_t row_base = static_cast<std::size_t>(
                    (y >> 5) * 32 * framebuffer.padded_width +
                    ((y & 1) << 2) + (((y & 31) >> 1) << 6));
                const std::size_t xor_mask =
                    static_cast<std::size_t>((y & 8) << 2);
                for (std::size_t group = 0; group < kScreenWidth / 4; ++group) {
                    std::uint32_t* destination = framebuffer.pixels + row_base +
                        (static_cast<std::size_t>(x_offsets[group]) ^ xor_mask);
                    destination[0] = xenos_color(source_pixel[0]);
                    destination[1] = xenos_color(source_pixel[1]);
                    destination[2] = xenos_color(source_pixel[2]);
                    destination[3] = xenos_color(source_pixel[3]);
                    source_pixel += 4;
                }
            }
            return;
        }
        for (std::size_t source_y = 0; source_y < kScreenHeight; ++source_y) {
            for (std::size_t source_x = 0; source_x < kScreenWidth; ++source_x) {
                put_pixel_unchecked(
                    framebuffer,
                    destination_x + static_cast<int>(source_x),
                    destination_y + static_cast<int>(source_y),
                    xenos_color(source[source_y * kScreenWidth + source_x]));
            }
        }
        return;
    }

    for (std::size_t source_y = 0; source_y < kScreenHeight; ++source_y) {
        for (std::size_t source_x = 0; source_x < kScreenWidth; ++source_x) {
            const std::uint32_t color = xenos_color(
                source[source_y * kScreenWidth + source_x]);
            const int base_x = destination_x + static_cast<int>(source_x) * scale;
            const int base_y = destination_y + static_cast<int>(source_y) * scale;
            for (int y = 0; y < scale; ++y) {
                for (int x = 0; x < scale; ++x) {
                    put_pixel(framebuffer, base_x + x, base_y + y, color);
                }
            }
        }
    }
}

void draw_touch_cursor(const Framebuffer& framebuffer,
                       const TouchState& touch,
                       int destination_x,
                       int destination_y,
                       int scale) {
    const int center_x = destination_x + static_cast<int>(touch.x) * scale;
    const int center_y = destination_y + static_cast<int>(touch.y) * scale;
    const int right = destination_x + static_cast<int>(kScreenWidth) * scale;
    const int bottom = destination_y + static_cast<int>(kScreenHeight) * scale;
    const std::uint32_t color = touch.pressed ? 0x0000FF00u : 0x00FFFF00u;
    for (int offset = -5; offset <= 5; ++offset) {
        const int horizontal_x = center_x + offset;
        const int vertical_y = center_y + offset;
        if (horizontal_x >= destination_x && horizontal_x < right &&
            center_y >= destination_y && center_y < bottom) {
            put_pixel(framebuffer, horizontal_x, center_y, color);
        }
        if (center_x >= destination_x && center_x < right &&
            vertical_y >= destination_y && vertical_y < bottom) {
            put_pixel(framebuffer, center_x, vertical_y, color);
        }
    }
}

std::uint16_t glyph_bits(char glyph) {
    // Compact 3x5 font. Bits are stored left-to-right, top-to-bottom.
    switch (glyph) {
    case '0': return 0x7B6Fu; // 111 101 101 101 111
    case '1': return 0x2C97u; // 010 110 010 010 111
    case '2': return 0x73E7u; // 111 001 111 100 111
    case '3': return 0x73CFu; // 111 001 111 001 111
    case '4': return 0x5BC9u; // 101 101 111 001 001
    case '5': return 0x79CFu; // 111 100 111 001 111
    case '6': return 0x79EFu; // 111 100 111 101 111
    case '7': return 0x7292u; // 111 001 010 010 010
    case '8': return 0x7BEFu; // 111 101 111 101 111
    case '9': return 0x7BCFu; // 111 101 111 001 111
    case 'F': return 0x79E4u; // 111 100 111 100 100
    case 'G': return 0x72EFu; // 111 100 101 101 111
    case 'H': return 0x5BEDu; // 101 101 111 101 101
    case 'P': return 0x7BE4u; // 111 101 111 100 100
    case 'S': return 0x79CFu; // 111 100 111 001 111
    case 'E': return 0x79A7u; // 111 100 110 100 111
    case 'M': return 0x5FEDu; // 101 111 111 101 101
    case 'U': return 0x5B6Fu; // 101 101 101 101 111
    case 'V': return 0x5B6Au; // 101 101 101 101 010
    case 'I': return 0x7497u; // 111 010 010 010 111
    case 'D': return 0x6B6Eu; // 110 101 101 101 110
    case 'A': return 0x2BEDu; // 010 101 111 101 101
    case 'B': return 0x6BAEu; // 110 101 110 101 110
    case 'C': return 0x7247u; // 111 100 100 100 111
    case 'N': return 0x5FEDu; // 101 111 111 101 101
    case 'O': return 0x7B6Fu; // 111 101 101 101 111
    case 'R': return 0x7BE5u; // 111 101 111 100 101
    case 'T': return 0x7492u; // 111 010 010 010 010
    case 'L': return 0x4927u; // 100 100 100 100 111
    case 'W': return 0x5BFDu; // 101 101 111 111 101
    case 'X': return 0x5AABu; // 101 101 010 101 101
    case '-': return 0x01C0u; // center row
    case '>': return 0x2A2Au; // 010 101 010 101 010
    case '%': return 0x5294u; // 101 001 010 100 101
    case '.': return 0x0002u; // bottom-center pixel
    default: return 0;
    }
}

void fill_rect(const Framebuffer& framebuffer, int left, int top,
               int width, int height, std::uint32_t color);
void draw_glyph(const Framebuffer& framebuffer, char glyph, int left, int top,
                int scale, std::uint32_t color);

void draw_text(const Framebuffer& framebuffer, const char* text,
               int left, int top, int scale, std::uint32_t color) {
    const int advance = 4 * scale;
    for (std::size_t i = 0; text[i] != '\0'; ++i) {
        draw_glyph(framebuffer, text[i],
                   left + static_cast<int>(i) * advance,
                   top, scale, color);
    }
}

void draw_number(const Framebuffer& framebuffer, int value,
                 int left, int top, int scale, std::uint32_t color) {
    char text[8];
    std::snprintf(text, sizeof(text), "%d", value);
    draw_text(framebuffer, text, left, top, scale, color);
}

void fill_rect(const Framebuffer& framebuffer, int left, int top,
               int width, int height, std::uint32_t color) {
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            put_pixel(framebuffer, left + x, top + y, color);
}

void flush_tiled_rect(const Framebuffer& framebuffer,
                      int left, int top, int width, int height) {
    const int clipped_left = std::max(0, left);
    const int clipped_top = std::max(0, top);
    const int clipped_right = std::min(framebuffer.visible_width, left + width);
    const int clipped_bottom = std::min(framebuffer.visible_height, top + height);
    if (clipped_left >= clipped_right || clipped_top >= clipped_bottom)
        return;

    const int first_tile_x = clipped_left >> 5;
    const int last_tile_x = (clipped_right - 1) >> 5;
    const int first_tile_y = clipped_top >> 5;
    const int last_tile_y = (clipped_bottom - 1) >> 5;
    const std::size_t row_pixels =
        static_cast<std::size_t>(last_tile_x - first_tile_x + 1) * 1024u;
    const std::size_t row_bytes = row_pixels * sizeof(std::uint32_t);

    for (int tile_y = first_tile_y; tile_y <= last_tile_y; ++tile_y) {
        const std::size_t first =
            static_cast<std::size_t>(tile_y) * 32u * framebuffer.padded_width +
            static_cast<std::size_t>(first_tile_x) * 1024u;
        memdcbst(framebuffer.pixels + first, static_cast<int>(row_bytes));
    }
}

void draw_glyph(const Framebuffer& framebuffer, char glyph, int left, int top,
                int scale, std::uint32_t color) {
    const std::uint16_t bits = glyph_bits(glyph);
    for (int row = 0; row < 5; ++row) {
        for (int column = 0; column < 3; ++column) {
            const int bit = 14 - (row * 3 + column);
            if (!(bits & (1u << bit))) continue;
            fill_rect(framebuffer, left + column * scale, top + row * scale,
                      scale, scale, color);
        }
    }
}

void format_metric(char* output_text, std::size_t output_size,
                   char first, char second, char third, unsigned int value) {
    value = std::min(value, 9999u);
    std::snprintf(output_text, output_size, "%c%c%c %03u.%u",
                  first, second, third, value / 10u, value % 10u);
}

void draw_metric(const Framebuffer& framebuffer, char first, char second,
                 char third, unsigned int value, int left, int top,
                 int text_scale, std::uint32_t color) {
    char text[10];
    format_metric(text, sizeof(text), first, second, third, value);
    draw_text(framebuffer, text, left, top, text_scale, color);
}

void draw_fps_counter(const Framebuffer& framebuffer,
                      const PerformanceStats& stats) {
    const int text_scale = framebuffer.visible_width >= 640 ? 3 : 2;
    const int advance = 4 * text_scale;
    const int left = 12;
    const int top = 12;
    const int height = 5 * text_scale;
    const int line_gap = 3 * text_scale;

    fill_rect(framebuffer, left - 5, top - 5,
              22 * advance + 6, height * 2 + line_gap + 10, 0x00000000u);
    draw_metric(framebuffer, 'E', 'M', 'U', stats.emulation_fps_tenths,
                left, top, text_scale, 0x00E6A900u);
    draw_metric(framebuffer, 'V', 'I', 'D', stats.video_fps_tenths,
                left + 10 * advance, top, text_scale, 0x00E6A900u);
    char speed[24];
    std::snprintf(speed, sizeof(speed), "SPEED %u%% %s",
                  stats.game_speed_percent,
                  stats.turbo_enabled ? "TURBO" : "NORMAL");
    draw_text(framebuffer, speed, left, top + height + line_gap,
              text_scale, 0x00E6A900u);
}

bool same_counter_values(const PerformanceStats& left,
                         const PerformanceStats& right) {
    return left.emulation_fps_tenths == right.emulation_fps_tenths &&
           left.video_fps_tenths == right.video_fps_tenths &&
           left.game_speed_percent == right.game_speed_percent &&
           left.turbo_enabled == right.turbo_enabled;
}

void clear_picture_menu_backing(const Framebuffer& framebuffer) {
    const int menu_scale = framebuffer.visible_width >= 640 ? 3 : 2;
    const int menu_top = framebuffer.visible_height >= 400 ? 72 : 58;
    const int menu_line = 7 * menu_scale;
    fill_rect(framebuffer, 6, menu_top - 7,
              34 * 4 * menu_scale, 8 * menu_line + 15, kMatteColor);
}

void draw_picture_menu(const Framebuffer& framebuffer,
                       const PerformanceStats& stats) {
    if (!stats.picture_menu_open) return;

    const int scale = framebuffer.visible_width >= 640 ? 3 : 2;
    const int left = 12;
    const int top = framebuffer.visible_height >= 400 ? 72 : 58;
    const int line = 7 * scale;
    const std::uint32_t normal = 0x00E6A900u;
    const std::uint32_t selected = 0x00E6E600u;
    fill_rect(framebuffer, left - 6, top - 7,
              34 * 4 * scale, 8 * line + 15, 0x00000000u);
    draw_text(framebuffer, "PICTURE SETTINGS", left, top, scale, normal);

    const char* labels[] = {"BRIGHTNESS", "CONTRAST", "COLOR"};
    const int values[] = {
        stats.picture.brightness,
        static_cast<int>(stats.picture.contrast),
        static_cast<int>(stats.picture.saturation)
    };
    for (unsigned int item = 0; item < 3; ++item) {
        const int y = top + static_cast<int>(item + 1) * line;
        const std::uint32_t color =
            stats.picture_menu_item == item ? selected : normal;
        if (stats.picture_menu_item == item)
            draw_text(framebuffer, ">", left, y, scale, color);
        draw_text(framebuffer, labels[item], left + 5 * scale, y, scale, color);
        draw_number(framebuffer, values[item], left + 49 * scale,
                    y, scale, color);
    }
    draw_text(framebuffer, "UP DOWN SELECT", left, top + 4 * line,
              scale, normal);
    draw_text(framebuffer, "LEFT RIGHT CHANGE", left, top + 5 * line,
              scale, normal);
    draw_text(framebuffer, "A DEFAULT", left, top + 6 * line,
              scale, normal);
    draw_text(framebuffer, "B SAVE CLOSE", left, top + 7 * line,
              scale, normal);
}

void present_immediate(const std::uint16_t* source, const TouchState& touch,
                       const PerformanceStats& stats) {
    static bool first_present = true;
    static bool menu_was_open = false;
    static bool have_counter_values = false;
    static PerformanceStats last_counter_values;
    Framebuffer framebuffer = output;
    if (first_present) {
        std::fill(framebuffer.pixels,
                  framebuffer.pixels +
                      static_cast<std::size_t>(framebuffer.padded_width) *
                          framebuffer.padded_height,
                  kMatteColor);
    }

    const int gap = 16;
    const int maximum_scale_x = (framebuffer.visible_width - gap) /
                                static_cast<int>(kScreenWidth * 2);
    const int maximum_scale_y = framebuffer.visible_height /
                                static_cast<int>(kScreenHeight);
    const int scale = std::max(1, std::min(3, std::min(maximum_scale_x, maximum_scale_y)));
    const int total_width = static_cast<int>(kScreenWidth * 2) * scale + gap;
    const int total_height = static_cast<int>(kScreenHeight) * scale;
    const int top_x = (framebuffer.visible_width - total_width) / 2;
    const int top_y = (framebuffer.visible_height - total_height) / 2;
    const int touch_x = top_x + static_cast<int>(kScreenWidth) * scale + gap;

    const bool menu_just_closed = menu_was_open && !stats.picture_menu_open;
    const int counter_scale = framebuffer.visible_width >= 640 ? 3 : 2;
    const int counter_left = 7;
    const int counter_top = 7;
    const int counter_right = counter_left + 22 * 4 * counter_scale + 6;
    const int counter_bottom = counter_top + 10 * counter_scale +
        3 * counter_scale + 10;
    const int screen_width = static_cast<int>(kScreenWidth) * scale;
    const int screen_height = static_cast<int>(kScreenHeight) * scale;
    const bool counter_overlaps_screen =
        top_y < counter_bottom && top_y + screen_height > counter_top &&
        ((top_x < counter_right && top_x + screen_width > counter_left) ||
         (touch_x < counter_right && touch_x + screen_width > counter_left));
    const bool counter_changed = counter_overlaps_screen ||
        !have_counter_values || !same_counter_values(stats, last_counter_values);
    if (menu_just_closed)
        clear_picture_menu_backing(framebuffer);
    draw_screen(framebuffer, source, top_x, top_y, scale);
    draw_screen(framebuffer, source + kScreenWidth * kScreenHeight,
                touch_x, top_y, scale);
    draw_touch_cursor(framebuffer, touch, touch_x, top_y, scale);
    // The counter lives entirely in the matte and its values change at most
    // once per statistics window. Preserve it between frames rather than
    // redrawing and flushing the same 18 Xenos tiles on every handoff.
    if (counter_changed)
        draw_fps_counter(framebuffer, stats);
    draw_picture_menu(framebuffer, stats);

    const std::size_t byte_count =
        static_cast<std::size_t>(framebuffer.padded_width) *
        framebuffer.padded_height * sizeof(std::uint32_t);
    if (first_present) {
        memdcbst(output.pixels, static_cast<int>(byte_count));
        first_present = false;
    }
    else {
        flush_tiled_rect(framebuffer, top_x, top_y,
                         screen_width, screen_height);
        flush_tiled_rect(framebuffer, touch_x, top_y,
                         screen_width, screen_height);

        if (counter_changed) {
            const int text_scale = framebuffer.visible_width >= 640 ? 3 : 2;
            const int advance = 4 * text_scale;
            const int text_height = 5 * text_scale;
            const int line_gap = 3 * text_scale;
            flush_tiled_rect(framebuffer, 7, 7, 22 * advance + 6,
                             text_height * 2 + line_gap + 10);
        }

        if (stats.picture_menu_open || menu_just_closed) {
            const int menu_scale = framebuffer.visible_width >= 640 ? 3 : 2;
            const int menu_top = framebuffer.visible_height >= 400 ? 72 : 58;
            const int menu_line = 7 * menu_scale;
            flush_tiled_rect(framebuffer, 6, menu_top - 7,
                             34 * 4 * menu_scale, 8 * menu_line + 15);
        }
    }
    menu_was_open = stats.picture_menu_open;
    if (counter_changed) {
        last_counter_values = stats;
        have_counter_values = true;
    }
}

extern "C" void xenonds_noods_video_worker() {
    __sync_synchronize();
    present_immediate(queued_frame, queued_touch, queued_stats);
    __sync_synchronize();
}

} // namespace

bool initialize_noods_video() {
    volatile const std::uint32_t* info =
        reinterpret_cast<volatile const std::uint32_t*>(0xec806100ULL);
    const std::uint32_t base = info[4];
    const std::uint32_t width = info[13];
    const std::uint32_t height = info[14];
    if (base == 0 || width < 160u || height < 100u) {
        return false;
    }

    output.pixels = reinterpret_cast<std::uint32_t*>(
        static_cast<unsigned long>(base | 0x80000000u));
    output.visible_width = static_cast<int>(width);
    output.visible_height = static_cast<int>(height);
    output.padded_width = (output.visible_width + 31) & ~31;
    output.padded_height = (output.visible_height + 31) & ~31;
    rebuild_color_lut(PictureSettings());
    initialized = true;
    return true;
}

bool set_noods_picture_settings(const PictureSettings& settings,
                                unsigned long timeout_microseconds) {
    if (!initialized) return false;
    if (!wait_for_noods_video(timeout_microseconds)) return false;
    rebuild_color_lut(settings);
    __sync_synchronize();
    return true;
}

bool wait_for_noods_video(unsigned long timeout_microseconds) {
    if (!initialized) return true;

    const std::uint64_t start = mftb();
    unsigned long poll_count = 0;
    for (;;) {
        if (xenon_is_thread_task_running(kVideoThread) == 0) {
            // Acquire the frame worker's writes after observing completion.
            __sync_synchronize();
            return true;
        }

        ++poll_count;
        if (timeout_microseconds == 0 ||
            poll_count >= kMaximumVideoWaitPolls ||
            tb_diff_usec(mftb(), start) >= timeout_microseconds) {
            return false;
        }
    }
}

bool try_present_noods_frame(const std::uint16_t* pixels,
                             const TouchState& touch,
                             const PerformanceStats& stats) {
    if (!initialized || !pixels) return false;

    // With one producer, observing idle reserves the shared queue until this
    // function's single launch attempt. Most importantly, a busy result exits
    // before touching any queued state.
    if (xenon_is_thread_task_running(kVideoThread) != 0) return false;

    __sync_synchronize();
    std::memcpy(queued_frame, pixels, sizeof(queued_frame));
    queued_touch = touch;
    queued_stats = stats;
    __sync_synchronize();

    void* stack_top = worker_stack + sizeof(worker_stack) - 256;
    return xenon_run_thread_task(
               kVideoThread, stack_top,
               reinterpret_cast<void*>(&xenonds_noods_video_worker)) == 0;
}

} // namespace xenon
} // namespace xenonds

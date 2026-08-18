// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

struct controller_data_s {
    signed short s1_x, s1_y, s2_x, s2_y;
    int s1_z, s2_z, lb, rb, start, back, a, b, x, y;
    int up, down, left, right;
    unsigned char lt, rt;
    int logo;
};

extern "C" int get_controller_data(controller_data_s* data, int port);

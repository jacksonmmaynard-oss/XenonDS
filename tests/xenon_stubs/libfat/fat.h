// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Host-only declaration used by the Xenon frontend syntax check. The real
// implementation is supplied by LibXenon's libfat package.
extern "C" int fatInitDefault(void);

#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cxx="${CXX:-g++}"

if [[ ! -f "$repo_root/third_party/noods/src/core/core.h" ]]; then
    printf 'NooDS source is missing; run scripts/fetch_noods.sh first.\n' >&2
    exit 1
fi

"$cxx" -std=gnu++11 -Wall -Wextra -Wpedantic -Werror \
    -Wno-sign-compare -fsyntax-only \
    -DENDIAN_BIG=1 -DNO_FDOPEN=1 -DLOG_LEVEL=0 -DNOODS_XENON=1 \
    -I"$repo_root/tests/xenon_stubs" \
    -I"$repo_root/platform/xenon/noods/compat" \
    -I"$repo_root/platform/xenon/noods/source" \
    -I"$repo_root/platform/xenon/shared" \
    -I"$repo_root/include" \
    -I"$repo_root/third_party/noods/src/core" \
    -I"$repo_root/third_party/noods/src/core/arm" \
    -I"$repo_root/third_party/noods/src/core/gpu" \
    -I"$repo_root/third_party/noods/src/core/hle" \
    -I"$repo_root/third_party/noods/src/core/io" \
    -I"$repo_root/third_party/noods/src/core/memory" \
    "$repo_root/platform/xenon/noods/source/main.cpp" \
    "$repo_root/platform/xenon/noods/source/thread_compat.cpp" \
    "$repo_root/platform/xenon/noods/source/noods_video.cpp" \
    "$repo_root/platform/xenon/shared/rom_finder.cpp"

printf 'PASS Xenon NooDS frontend syntax\n'

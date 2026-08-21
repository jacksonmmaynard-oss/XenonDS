#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${TMPDIR:-/tmp}/xenonds-noods-video-test"
binary="$build_dir/noods-video-submission-test"
cxx="${CXX:-g++}"

mkdir -p "$build_dir"
"$cxx" -std=gnu++17 -Wall -Wextra -Wpedantic -Werror \
    -I"$repo_root/tests/noods_video_stubs" \
    -I"$repo_root/tests/xenon_stubs" \
    -I"$repo_root/include" \
    -I"$repo_root/platform/xenon/noods/source" \
    "$repo_root/tests/noods_video_submission_test.cpp" \
    -o "$binary"

"$binary"

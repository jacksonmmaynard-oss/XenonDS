#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${TMPDIR:-/tmp}/xenonds-noods-frontend-test"
binary="$build_dir/noods-frontend-liveness"
rom_path="$build_dir/uniform-test.nds"
cxx="${CXX:-g++}"

mkdir -p "$build_dir"
# The scripted Core does not read this image, but the production frontend owns
# the surrounding directory and writes its display settings there on exit.
python3 "$repo_root/tests/make_noods_smoke_rom.py" "$rom_path"

"$cxx" -std=gnu++17 -Wall -Wextra -Wpedantic -Werror \
    -I"$repo_root/tests/noods_frontend_stubs" \
    -I"$repo_root/tests/xenon_stubs" \
    -I"$repo_root/include" \
    -I"$repo_root/platform/xenon/noods/source" \
    -I"$repo_root/platform/xenon/shared" \
    "$repo_root/tests/noods_frontend_liveness.cpp" \
    "$repo_root/src/checksum.cpp" \
    "$repo_root/src/controller_mapper.cpp" \
    "$repo_root/src/nds_header.cpp" \
    -o "$binary"

"$binary" "$rom_path"

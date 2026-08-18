#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${TMPDIR:-/tmp}/xenonds-noods-smoke-build"
rom_path="$build_dir/render-test.nds"
binary="$build_dir/noods-core-smoke"
cxx="${CXX:-g++}"
optimization="${NOODS_TEST_OPTIMIZATION:--Ofast}"
read -r -a optimization_flags <<< "$optimization"

mkdir -p "$build_dir"
python3 "$repo_root/tests/make_noods_smoke_rom.py" "$rom_path"

mapfile -t sources < <(
    sed -n '/^NOODS_CPP_SOURCES :=/,/^$/p' "$repo_root/platform/xenon/noods_sources.mk" |
        sed -e '1d' -e 's/^[[:space:]]*//' -e 's/[[:space:]]*\\$//' -e '/^$/d' |
        sed "s#^#$repo_root/third_party/noods/src/core/#"
)

"$cxx" -w -std=gnu++17 "${optimization_flags[@]}" -fstrict-aliasing -pthread \
    -DLOG_LEVEL=0 -DNOODS_XENON=1 \
    -I"$repo_root/third_party/noods/src/core" \
    "$repo_root/tests/noods_core_smoke.cpp" "${sources[@]}" -o "$binary"

"$binary" "$rom_path"
"$binary" "$rom_path" --benchmark "${NOODS_BENCHMARK_FRAMES:-600}"

# Optionally exercise a larger redistributable homebrew ROM supplied by the
# caller. This is useful for release validation without checking ROM binaries
# into the repository.
if [[ $# -gt 0 ]]; then
    "$binary" "$1" --boot
fi

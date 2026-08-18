#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
release_dir="$repo_root/platform/xenon/release-noods"

for required in \
    "$repo_root/platform/xenon/loader/xenonds-loader.elf32" \
    "$repo_root/platform/xenon/xenonds-noods.elf32" \
    "$repo_root/platform/xenon/noods/USB_README.txt" \
    "$repo_root/platform/xenon/noods/VERSION.txt" \
    "$repo_root/third_party/noods/LICENSE" \
    "$repo_root/LICENSE"; do
    if [[ ! -f "$required" ]]; then
        printf 'Required release file is missing: %s\n' "$required" >&2
        exit 1
    fi
done

core_binary="$repo_root/platform/xenon/xenonds-noods.elf32"
stale_source="$(find \
    "$repo_root/platform/xenon/noods/source" \
    "$repo_root/platform/xenon/shared" \
    "$repo_root/include" \
    "$repo_root/src" \
    "$repo_root/third_party/noods/src/core" \
    -type f -newer "$core_binary" -print -quit)"
if [[ -n "$stale_source" ]]; then
    printf 'Refusing to package a stale Xbox core. Newer source: %s\n' \
        "$stale_source" >&2
    printf 'Rebuild the NooDS target before packaging.\n' >&2
    exit 1
fi

# This is a fixed generated-output directory, never a caller-provided path.
rm -rf -- "$release_dir"
mkdir -p "$release_dir/XenonDS" "$release_dir/licenses"
cp "$repo_root/platform/xenon/loader/xenonds-loader.elf32" "$release_dir/xenon.elf"
cp "$repo_root/platform/xenon/xenonds-noods.elf32" "$release_dir/XenonDS/xenonds-core.elf32"
cp "$repo_root/platform/xenon/noods/USB_README.txt" "$release_dir/README.txt"
cp "$repo_root/platform/xenon/noods/VERSION.txt" "$release_dir/VERSION.txt"
cp "$repo_root/third_party/noods/LICENSE" "$release_dir/licenses/NooDS-GPL-3.0.txt"
cp "$repo_root/LICENSE" "$release_dir/licenses/XenonDS-GPL-2.0.txt"

printf 'Packaged USB-ready runtime at %s\n' "$release_dir"

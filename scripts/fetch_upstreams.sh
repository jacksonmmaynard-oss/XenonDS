#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
third_party="$project_root/third_party"

desmume_commit="2f3383d139cc77ae6b28ff3e8beb868a708f2fe1"
libxenon_commit="a6fe4c1f14921b5512e3cc6e400971e4d977b75e"

mkdir -p "$third_party"

fetch_repo() {
    local url="$1"
    local destination="$2"
    local commit="$3"
    if [[ ! -d "$destination/.git" ]]; then
        git clone --filter=blob:none --no-checkout "$url" "$destination"
    fi
    git -C "$destination" fetch --depth 1 origin "$commit"
    git -C "$destination" checkout --detach "$commit"
}

fetch_repo "https://github.com/TASEmulators/desmume.git" "$third_party/desmume" "$desmume_commit"
fetch_repo "https://github.com/X360Tools/libxenon.git" "$third_party/libxenon" "$libxenon_commit"

desmume_patch="$project_root/patches/desmume-xenon.patch"
if git -C "$third_party/desmume" apply --ignore-space-change --check "$desmume_patch"; then
    git -C "$third_party/desmume" apply --ignore-space-change "$desmume_patch"
elif ! git -C "$third_party/desmume" apply --ignore-space-change --reverse --check "$desmume_patch"; then
    printf 'Unable to apply Xenon compatibility patch: %s\n' "$desmume_patch" >&2
    exit 1
fi

printf 'Pinned upstream sources are ready in %s\n' "$third_party"

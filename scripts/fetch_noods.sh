#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
destination="$project_root/third_party/noods"
commit="4cb1439c330248b26cdf5f8dbcfa4e5ac53dcda0"
patch="$project_root/patches/noods-xenon.patch"

mkdir -p "$project_root/third_party"
created_checkout=0
if [[ -e "$destination" && ! -d "$destination/.git" ]]; then
    printf 'Refusing to replace non-git path: %s\n' "$destination" >&2
    exit 1
fi
if [[ ! -d "$destination/.git" ]]; then
    git clone --filter=blob:none --no-checkout \
        https://github.com/Hydr8gon/NooDS.git "$destination"
    created_checkout=1
fi

current_commit="$(git -C "$destination" rev-parse HEAD 2>/dev/null || true)"
if [[ "$created_checkout" == 0 && "$current_commit" == "$commit" ]] &&
   git -C "$destination" apply --reverse --check "$patch"; then
    printf 'Pinned NooDS source is already patched at %s\n' "$destination"
    exit 0
fi

if [[ "$created_checkout" == 0 &&
      -n "$(git -C "$destination" status --porcelain --untracked-files=normal)" ]]; then
    printf 'Refusing to overwrite modified NooDS checkout: %s\n' "$destination" >&2
    printf 'Move it aside or restore it, then run this script again.\n' >&2
    exit 1
fi

git -C "$destination" fetch --depth 1 origin "$commit"
git -C "$destination" checkout --detach "$commit"

if ! git -C "$destination" apply --check "$patch"; then
    printf 'Unable to apply Xenon compatibility patch: %s\n' "$patch" >&2
    exit 1
fi
git -C "$destination" apply "$patch"

printf 'Pinned NooDS source is ready at %s\n' "$destination"

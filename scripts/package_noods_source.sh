#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
output="$repo_root/platform/xenon/XenonDS-v0.8.1-source.tar.gz"
staging="$(mktemp -d "${TMPDIR:-/tmp}/xenonds-source.XXXXXX")"
trap 'rm -rf -- "$staging"' EXIT

if [[ ! -d "$repo_root/third_party/noods/.git" ]]; then
    printf 'NooDS source is missing; run scripts/fetch_noods.sh first.\n' >&2
    exit 1
fi
if [[ -n "$(git -c "safe.directory=$repo_root" -C "$repo_root" status --porcelain --untracked-files=all)" ]]; then
    printf 'Refusing to package source from a dirty repository.\n' >&2
    printf 'Commit the exact release source first, then run this script again.\n' >&2
    exit 1
fi

mkdir -p "$staging/XenonDS-v0.8.1"
git -c "safe.directory=$repo_root" -C "$repo_root" archive --format=tar HEAD | \
    tar -xf - -C "$staging/XenonDS-v0.8.1"
mkdir -p "$staging/XenonDS-v0.8.1/third_party/noods"
tar -C "$repo_root/third_party/noods" --exclude=.git -cf - . | \
    tar -xf - -C "$staging/XenonDS-v0.8.1/third_party/noods"
tar -czf "$output" -C "$staging" XenonDS-v0.8.1

printf 'Packaged corresponding source at %s\n' "$output"

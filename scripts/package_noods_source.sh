#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
version="0.9.1"
noods_revision="4cb1439c330248b26cdf5f8dbcfa4e5ac53dcda0"
noods_dir="$repo_root/third_party/noods"
patch_file="$repo_root/patches/noods-xenon.patch"
archive_root="XenonDS-v${version}"
output="$repo_root/platform/xenon/${archive_root}-source.tar.gz"
staging="$(mktemp -d "${TMPDIR:-/tmp}/xenonds-source.XXXXXX")"
trap 'rm -rf -- "$staging"' EXIT

if [[ ! -d "$noods_dir/.git" ]]; then
    printf 'NooDS source is missing; run scripts/fetch_noods.sh first.\n' >&2
    exit 1
fi
if [[ "$(git -C "$noods_dir" rev-parse HEAD)" != "$noods_revision" ]]; then
    printf 'NooDS is not at the pinned release revision %s.\n' \
        "$noods_revision" >&2
    exit 1
fi
if [[ -n "$(git -C "$noods_dir" ls-files --others --exclude-standard)" ]]; then
    printf 'Refusing to package untracked files from third_party/noods.\n' >&2
    git -C "$noods_dir" ls-files --others --exclude-standard >&2
    exit 1
fi
if ! git -C "$noods_dir" diff --binary --no-ext-diff \
        --src-prefix=a/ --dst-prefix=b/ | cmp -s - "$patch_file"; then
    printf 'The active NooDS changes do not exactly match %s.\n' \
        "$patch_file" >&2
    printf 'Regenerate and replay-test the patch before packaging.\n' >&2
    exit 1
fi
if [[ -n "$(git -c "safe.directory=$repo_root" -C "$repo_root" status --porcelain --untracked-files=all)" ]]; then
    printf 'Refusing to package source from a dirty repository.\n' >&2
    printf 'Commit the exact release source first, then run this script again.\n' >&2
    exit 1
fi

source_date_epoch="$(git -c "safe.directory=$repo_root" -C "$repo_root" log -1 --format=%ct HEAD)"
root_revision="$(git -c "safe.directory=$repo_root" -C "$repo_root" rev-parse HEAD)"
patch_sha256="$(sha256sum "$patch_file" | awk '{print $1}')"
toolchain_image="${LIBXENON_IMAGE:-free60/libxenon:latest (digest unavailable)}"

mkdir -p "$staging/$archive_root"
git -c "safe.directory=$repo_root" -C "$repo_root" archive --format=tar HEAD | \
    tar -xf - -C "$staging/$archive_root"
mkdir -p "$staging/$archive_root/third_party/noods"
git -C "$noods_dir" ls-files -z | \
    tar -C "$noods_dir" --null -T - -cf - | \
    tar -xf - -C "$staging/$archive_root/third_party/noods"

printf '%s\n' \
    "XenonDS version: v${version}" \
    "XenonDS commit: ${root_revision}" \
    "NooDS commit: ${noods_revision}" \
    "NooDS patch SHA-256: ${patch_sha256}" \
    "LibXenon toolchain image: ${toolchain_image}" \
    "Source date epoch: ${source_date_epoch}" \
    > "$staging/$archive_root/BUILD_PROVENANCE.txt"

LC_ALL=C tar \
    --sort=name \
    --mtime="@${source_date_epoch}" \
    --owner=0 \
    --group=0 \
    --numeric-owner \
    -C "$staging" \
    -cf - "$archive_root" | gzip -n > "$output"
gzip -t "$output"

printf 'Packaged corresponding source at %s\n' "$output"

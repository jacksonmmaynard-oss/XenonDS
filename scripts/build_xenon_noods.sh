#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if ! command -v docker >/dev/null 2>&1; then
    echo "Docker is required. Install Docker Desktop, then run this script again." >&2
    exit 1
fi

bash "$repo_root/scripts/fetch_noods.sh"

docker run --rm \
    -v "${repo_root}:/app" \
    -w /app \
    free60/libxenon:latest \
    bash -lc "make -C platform/xenon clean WITH_NOODS=1 && make -C platform/xenon WITH_NOODS=1 && make -C platform/xenon/loader clean && make -C platform/xenon/loader && bash scripts/package_noods_runtime.sh"

echo "Built USB-ready NooDS files in ${repo_root}/platform/xenon/release-noods"

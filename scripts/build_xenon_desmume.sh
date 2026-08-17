#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if ! command -v docker >/dev/null 2>&1; then
    echo "Docker is required. Install Docker Desktop, then run this script again." >&2
    exit 1
fi

docker run --rm \
    -v "${repo_root}:/app" \
    -w /app \
    free60/libxenon:latest \
    bash -lc "bash scripts/fetch_upstreams.sh && make -C platform/xenon WITH_DESMUME=1 && make -C platform/xenon/loader && mkdir -p platform/xenon/release/XenonDS && cp platform/xenon/loader/xenonds-loader.elf32 platform/xenon/release/xenon.elf && cp platform/xenon/xenonds-desmume.elf32 platform/xenon/release/XenonDS/xenonds-core.elf32 && cp platform/xenon/loader/USB_README.txt platform/xenon/release/README.txt"

echo "Built USB-ready files in ${repo_root}/platform/xenon/release"

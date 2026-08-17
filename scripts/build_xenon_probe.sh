#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if ! command -v docker >/dev/null 2>&1; then
    echo "Docker is required. Install Docker Desktop, then run this script again." >&2
    exit 1
fi

docker run --rm \
    -v "${repo_root}:/app" \
    -w /app/platform/xenon \
    free60/libxenon:latest \
    bash -lc make

echo "Built ${repo_root}/platform/xenon/xenonds-probe.elf32"

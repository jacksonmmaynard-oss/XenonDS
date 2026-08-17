$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    Write-Error "Docker Desktop is required. Install it, start it, then run this script again."
    exit 1
}

docker run --rm `
    -v "${RepoRoot}:/app" `
    -w /app `
    free60/libxenon:latest `
    bash -lc "bash scripts/fetch_upstreams.sh && make -C platform/xenon WITH_DESMUME=1 && make -C platform/xenon/loader && mkdir -p platform/xenon/release/XenonDS && cp platform/xenon/loader/xenonds-loader.elf32 platform/xenon/release/xenon.elf && cp platform/xenon/xenonds-desmume.elf32 platform/xenon/release/XenonDS/xenonds-core.elf32 && cp platform/xenon/loader/USB_README.txt platform/xenon/release/README.txt"

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Built USB-ready files in $RepoRoot/platform/xenon/release"

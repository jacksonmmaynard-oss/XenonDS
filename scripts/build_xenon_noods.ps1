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
    bash -lc "bash scripts/fetch_noods.sh && make -C platform/xenon clean WITH_NOODS=1 && make -C platform/xenon WITH_NOODS=1 && make -C platform/xenon/loader clean && make -C platform/xenon/loader && bash scripts/package_noods_runtime.sh"

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Built USB-ready NooDS files in $RepoRoot/platform/xenon/release-noods"

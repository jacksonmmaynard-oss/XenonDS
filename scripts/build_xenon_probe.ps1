$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    Write-Error "Docker Desktop is required. Install it, start it, then run this script again."
    exit 1
}

docker run --rm `
    -v "${RepoRoot}:/app" `
    -w /app/platform/xenon `
    free60/libxenon:latest `
    bash -lc make

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Built $RepoRoot/platform/xenon/xenonds-probe.elf32"

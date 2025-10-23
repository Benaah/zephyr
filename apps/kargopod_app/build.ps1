# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2025 Kargo Chain
# PowerShell build script for KargoPod firmware

param(
    [string]$Board = "kargopod_esp32s3",
    [string]$BuildType = "release",
    [switch]$Clean,
    [switch]$Flash,
    [switch]$Help
)

function Show-Usage {
    Write-Host ""
    Write-Host "KargoPod Firmware Build Script" -ForegroundColor Green
    Write-Host ""
    Write-Host "Usage: .\build.ps1 [OPTIONS]"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "  -Board <BOARD>      Target board (kargopod_esp32s3 or kargopod_nrf9160)"
    Write-Host "  -BuildType <TYPE>   Build type (debug or release)"
    Write-Host "  -Clean              Clean build"
    Write-Host "  -Flash              Flash after build"
    Write-Host "  -Help               Show this help message"
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  .\build.ps1 -Board kargopod_esp32s3"
    Write-Host "  .\build.ps1 -Board kargopod_nrf9160 -Clean -Flash"
    exit 0
}

if ($Help) {
    Show-Usage
}

Write-Host "========================================" -ForegroundColor Green
Write-Host "  KargoPod Firmware Build" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host "Board:      $Board" -ForegroundColor Yellow
Write-Host "Build type: $BuildType" -ForegroundColor Yellow
Write-Host ""

# Check if west is installed
try {
    $null = Get-Command west -ErrorAction Stop
} catch {
    Write-Host "Error: west is not installed" -ForegroundColor Red
    Write-Host "Install with: pip install west"
    exit 1
}

# Navigate to app directory
Set-Location $PSScriptRoot

# Clean if requested
if ($Clean) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    if (Test-Path "build") {
        Remove-Item -Recurse -Force "build"
    }
}

# Build
Write-Host "Building firmware..." -ForegroundColor Green
if ($BuildType -eq "debug") {
    west build -b $Board -- -DCMAKE_BUILD_TYPE=Debug
} else {
    west build -b $Board -- -DCMAKE_BUILD_TYPE=MinSizeRel
}

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build successful!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Build artifacts:" -ForegroundColor Green
    Get-ChildItem "build\zephyr\zephyr.*" | Format-Table Name, Length
    Write-Host ""
    
    # Flash if requested
    if ($Flash) {
        Write-Host "Flashing firmware..." -ForegroundColor Yellow
        west flash
    }
} else {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

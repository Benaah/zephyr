# Build Script for Kargo IoT Sensor Application (PowerShell)
# Usage: .\build.ps1 [-Board <board>] [-Type <type>] [-Flash] [-Monitor] [-Clean] [-Help]

param(
    [string]$Board = "esp32s3_devkitc/esp32s3/procpu",
    [ValidateSet("standard", "size", "speed", "debug")]
    [string]$Type = "standard",
    [switch]$Flash,
    [switch]$Monitor,
    [switch]$Clean,
    [switch]$Help
)

# Colors
$Green = "Green"
$Yellow = "Yellow"
$Red = "Red"

function Show-Usage {
    Write-Host ""
    Write-Host "Kargo IoT Sensor Build Script" -ForegroundColor $Green
    Write-Host "=============================" -ForegroundColor $Green
    Write-Host ""
    Write-Host "Usage: .\build.ps1 [OPTIONS]"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "  -Board <board>     Target board (default: esp32s3_devkitc/esp32s3/procpu)"
    Write-Host "  -Type <type>       Build type: standard|size|speed|debug (default: standard)"
    Write-Host "  -Flash             Flash after successful build"
    Write-Host "  -Monitor           Open serial monitor after flashing"
    Write-Host "  -Clean             Clean before build"
    Write-Host "  -Help              Show this help message"
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  .\build.ps1                    # Standard build"
    Write-Host "  .\build.ps1 -Type size         # Size-optimized build"
    Write-Host "  .\build.ps1 -Flash -Monitor    # Build, flash, and monitor"
    Write-Host "  .\build.ps1 -Clean -Type debug -Flash -Monitor    # Clean, debug build, flash, and monitor"
    Write-Host ""
}

if ($Help) {
    Show-Usage
    exit 0
}

Write-Host ""
Write-Host "=== Kargo IoT Sensor Build Script ===" -ForegroundColor $Green
Write-Host ""

# Check if we're in the right directory
if (-not (Test-Path "CMakeLists.txt")) {
    Write-Host "Error: CMakeLists.txt not found. Please run from the project directory." -ForegroundColor $Red
    exit 1
}

# Clean if requested
if ($Clean) {
    Write-Host "Cleaning previous build..." -ForegroundColor $Yellow
    if (Test-Path "build") {
        Remove-Item -Recurse -Force build
    }
}

# Set build options based on type
$BuildOpts = ""
switch ($Type) {
    "size" {
        Write-Host "Build type: Size-optimized" -ForegroundColor $Yellow
        $BuildOpts = "-DCONFIG_SIZE_OPTIMIZATIONS=y"
    }
    "speed" {
        Write-Host "Build type: Speed-optimized" -ForegroundColor $Yellow
        $BuildOpts = "-DCONFIG_SPEED_OPTIMIZATIONS=y"
    }
    "debug" {
        Write-Host "Build type: Debug" -ForegroundColor $Yellow
        $BuildOpts = "-DCONFIG_DEBUG=y -DCONFIG_DEBUG_OPTIMIZATIONS=y"
    }
    "standard" {
        Write-Host "Build type: Standard" -ForegroundColor $Yellow
    }
}

# Build
Write-Host "Building for board: $Board" -ForegroundColor $Green
Write-Host ""

if ($BuildOpts) {
    west build -b $Board -- $BuildOpts
} else {
    west build -b $Board
}

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "✓ Build successful!" -ForegroundColor $Green

    # Show binary size
    if (Test-Path "build\zephyr\zephyr.bin") {
        $Size = (Get-Item "build\zephyr\zephyr.bin").Length
        $SizeKB = [math]::Round($Size / 1KB, 2)
        Write-Host "Binary size: $SizeKB KB" -ForegroundColor $Green
    }
} else {
    Write-Host "✗ Build failed!" -ForegroundColor $Red
    exit 1
}

# Flash if requested
if ($Flash) {
    Write-Host ""
    Write-Host "Flashing firmware..." -ForegroundColor $Yellow
    west flash

    if ($LASTEXITCODE -eq 0) {
        Write-Host "✓ Flash successful!" -ForegroundColor $Green
    } else {
        Write-Host "✗ Flash failed!" -ForegroundColor $Red
        exit 1
    }
}

# Open monitor if requested
if ($Monitor) {
    Write-Host ""
    Write-Host "Opening serial monitor..." -ForegroundColor $Yellow
    Write-Host "Press Ctrl+C to exit monitor" -ForegroundColor $Yellow
    Start-Sleep -Seconds 2
    west espressif monitor
}

Write-Host ""
Write-Host "Done!" -ForegroundColor $Green

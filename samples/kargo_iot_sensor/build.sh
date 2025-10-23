#!/bin/bash
# Build Script for Kargo IoT Sensor Application

set -e  # Exit on error

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Default values
BOARD="esp32s3_devkitc/esp32s3/procpu"
BUILD_TYPE="standard"
FLASH_AFTER_BUILD=false
MONITOR_AFTER_FLASH=false

# Print usage
usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -b, --board BOARD        Target board (default: esp32s3_devkitc/esp32s3/procpu)"
    echo "  -t, --type TYPE          Build type: standard|size|speed|debug (default: standard)"
    echo "  -f, --flash              Flash after successful build"
    echo "  -m, --monitor            Open serial monitor after flashing"
    echo "  -c, --clean              Clean before build"
    echo "  -h, --help               Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                       # Standard build"
    echo "  $0 -t size               # Size-optimized build"
    echo "  $0 -f -m                 # Build, flash, and monitor"
    echo "  $0 -c -t debug -f -m     # Clean, debug build, flash, and monitor"
}

# Parse arguments
CLEAN=false
while [[ $# -gt 0 ]]; do
    case $1 in
        -b|--board)
            BOARD="$2"
            shift 2
            ;;
        -t|--type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        -f|--flash)
            FLASH_AFTER_BUILD=true
            shift
            ;;
        -m|--monitor)
            MONITOR_AFTER_FLASH=true
            shift
            ;;
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo -e "${RED}Error: Unknown option $1${NC}"
            usage
            exit 1
            ;;
    esac
done

echo -e "${GREEN}=== Kargo IoT Sensor Build Script ===${NC}"
echo ""

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo -e "${RED}Error: CMakeLists.txt not found. Please run from the project directory.${NC}"
    exit 1
fi

# Clean if requested
if [ "$CLEAN" = true ]; then
    echo -e "${YELLOW}Cleaning previous build...${NC}"
    west build -t clean 2>/dev/null || rm -rf build
fi

# Set build options based on type
BUILD_OPTS=""
case $BUILD_TYPE in
    size)
        echo -e "${YELLOW}Build type: Size-optimized${NC}"
        BUILD_OPTS="-DCONFIG_SIZE_OPTIMIZATIONS=y"
        ;;
    speed)
        echo -e "${YELLOW}Build type: Speed-optimized${NC}"
        BUILD_OPTS="-DCONFIG_SPEED_OPTIMIZATIONS=y"
        ;;
    debug)
        echo -e "${YELLOW}Build type: Debug${NC}"
        BUILD_OPTS="-DCONFIG_DEBUG=y -DCONFIG_DEBUG_OPTIMIZATIONS=y"
        ;;
    standard)
        echo -e "${YELLOW}Build type: Standard${NC}"
        ;;
    *)
        echo -e "${RED}Error: Invalid build type: $BUILD_TYPE${NC}"
        exit 1
        ;;
esac

# Build
echo -e "${GREEN}Building for board: $BOARD${NC}"
echo ""

if [ -n "$BUILD_OPTS" ]; then
    west build -b $BOARD -- $BUILD_OPTS
else
    west build -b $BOARD
fi

if [ $? -eq 0 ]; then
    echo ""
    echo -e "${GREEN}✓ Build successful!${NC}"

    # Show binary size
    if [ -f "build/zephyr/zephyr.bin" ]; then
        SIZE=$(du -h build/zephyr/zephyr.bin | cut -f1)
        echo -e "${GREEN}Binary size: $SIZE${NC}"
    fi
else
    echo -e "${RED}✗ Build failed!${NC}"
    exit 1
fi

# Flash if requested
if [ "$FLASH_AFTER_BUILD" = true ]; then
    echo ""
    echo -e "${YELLOW}Flashing firmware...${NC}"
    west flash

    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Flash successful!${NC}"
    else
        echo -e "${RED}✗ Flash failed!${NC}"
        exit 1
    fi
fi

# Open monitor if requested
if [ "$MONITOR_AFTER_FLASH" = true ]; then
    echo ""
    echo -e "${YELLOW}Opening serial monitor...${NC}"
    echo -e "${YELLOW}Press Ctrl+C to exit monitor${NC}"
    sleep 2
    west espressif monitor || screen /dev/ttyUSB0 115200
fi

echo ""
echo -e "${GREEN}Done!${NC}"

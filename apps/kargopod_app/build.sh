#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2025 Kargo Chain
# Build script for KargoPod firmware

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default values
BOARD="${BOARD:-kargopod_esp32s3}"
BUILD_TYPE="${BUILD_TYPE:-release}"
CLEAN="${CLEAN:-0}"

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -b, --board BOARD       Target board (kargopod_esp32s3 or kargopod_nrf9160)"
    echo "  -t, --type TYPE         Build type (debug or release)"
    echo "  -c, --clean             Clean build"
    echo "  -f, --flash             Flash after build"
    echo "  -h, --help              Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 -b kargopod_esp32s3"
    echo "  $0 -b kargopod_nrf9160 -c -f"
    exit 1
}

# Parse arguments
FLASH=0
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
        -c|--clean)
            CLEAN=1
            shift
            ;;
        -f|--flash)
            FLASH=1
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            usage
            ;;
    esac
done

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  KargoPod Firmware Build${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "Board:      ${YELLOW}${BOARD}${NC}"
echo -e "Build type: ${YELLOW}${BUILD_TYPE}${NC}"
echo ""

# Check if west is installed
if ! command -v west &> /dev/null; then
    echo -e "${RED}Error: west is not installed${NC}"
    echo "Install with: pip install west"
    exit 1
fi

# Navigate to app directory
cd "$(dirname "$0")"

# Clean if requested
if [ "$CLEAN" -eq 1 ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf build
fi

# Build
echo -e "${GREEN}Building firmware...${NC}"
if [ "$BUILD_TYPE" == "debug" ]; then
    west build -b $BOARD -- -DCMAKE_BUILD_TYPE=Debug
else
    west build -b $BOARD -- -DCMAKE_BUILD_TYPE=MinSizeRel
fi

if [ $? -eq 0 ]; then
    echo -e "${GREEN}Build successful!${NC}"
    echo ""
    echo -e "${GREEN}Build artifacts:${NC}"
    ls -lh build/zephyr/zephyr.*
    echo ""
    
    # Flash if requested
    if [ "$FLASH" -eq 1 ]; then
        echo -e "${YELLOW}Flashing firmware...${NC}"
        west flash
    fi
else
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

#!/bin/bash

# Build script for Glimmer static library (Linux only)

set -e  # Exit on error

# Enable newer GCC if available (needed for C++20 support)
if [ -f /opt/rh/gcc-toolset-11/enable ]; then
    echo "Enabling GCC Toolset 11..."
    source /opt/rh/gcc-toolset-11/enable
elif [ -f /opt/rh/devtoolset-11/enable ]; then
    echo "Enabling DevToolset 11..."
    source /opt/rh/devtoolset-11/enable
elif [ -f /opt/rh/gcc-toolset-12/enable ]; then
    echo "Enabling GCC Toolset 12..."
    source /opt/rh/gcc-toolset-12/enable
else
    echo "Warning: No newer GCC toolset found. Using system GCC $(gcc --version | head -1)"
    echo "Build may fail. Install with: sudo yum install gcc-toolset-11"
fi

gcc --version | head -1
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if running on Linux
if [[ "$OSTYPE" != "linux-gnu"* ]]; then
    echo -e "${RED}Error: This build script is for Linux only${NC}"
    exit 1
fi

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Build directory
BUILD_DIR="${SCRIPT_DIR}/build"

# Parse command line arguments
BUILD_TYPE="Release"
CLEAN_BUILD=false
ENABLE_SVG=true
ENABLE_IMAGES=true
ENABLE_RICHTEXT=true
ENABLE_PLOTS=true
ENABLE_NFDEXT=false
ENABLE_BLEND2D=false

print_usage() {
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -h, --help              Show this help message"
    echo "  -r, --release           Build in Release mode (optimized, default)"
    echo "  -d, --debug             Build in Debug mode (with debug symbols)"
    echo "  -c, --clean             Clean build directory before building"
    echo "  --no-svg                Disable SVG support"
    echo "  --no-images             Disable image support"
    echo "  --no-richtext           Disable rich text support"
    echo "  --no-plots              Disable plots support"
    echo "  --enable-nfdext         Enable nfd-extended library"
    echo "  --enable-blend2d        Enable Blend2D renderer (requires libblend2d.a)"
    echo ""
}

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            print_usage
            exit 0
            ;;
        -r|--release)
            BUILD_TYPE="Release"
            shift
            ;;
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        --no-svg)
            ENABLE_SVG=false
            shift
            ;;
        --no-images)
            ENABLE_IMAGES=false
            shift
            ;;
        --no-richtext)
            ENABLE_RICHTEXT=false
            shift
            ;;
        --no-plots)
            ENABLE_PLOTS=false
            shift
            ;;
        --enable-nfdext)
            ENABLE_NFDEXT=true
            shift
            ;;
        --enable-blend2d)
            ENABLE_BLEND2D=true
            shift
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            print_usage
            exit 1
            ;;
    esac
done

# Clean build if requested
if [ "$CLEAN_BUILD" = true ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Prepare CMake options
CMAKE_OPTIONS="-DCMAKE_BUILD_TYPE=$BUILD_TYPE"

if [ "$ENABLE_SVG" = false ]; then
    CMAKE_OPTIONS="$CMAKE_OPTIONS -DGLIMMER_DISABLE_SVG=ON"
fi

if [ "$ENABLE_IMAGES" = false ]; then
    CMAKE_OPTIONS="$CMAKE_OPTIONS -DGLIMMER_DISABLE_IMAGES=ON"
fi

if [ "$ENABLE_RICHTEXT" = false ]; then
    CMAKE_OPTIONS="$CMAKE_OPTIONS -DGLIMMER_DISABLE_RICHTEXT=ON"
fi

if [ "$ENABLE_PLOTS" = false ]; then
    CMAKE_OPTIONS="$CMAKE_OPTIONS -DGLIMMER_DISABLE_PLOTS=ON"
fi

if [ "$ENABLE_NFDEXT" = true ]; then
    CMAKE_OPTIONS="$CMAKE_OPTIONS -DGLIMMER_ENABLE_NFDEXT=ON"
fi

if [ "$ENABLE_BLEND2D" = true ]; then
    CMAKE_OPTIONS="$CMAKE_OPTIONS -DGLIMMER_ENABLE_BLEND2D=ON"
fi

# Run CMake
echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Configuring Glimmer Build${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "${YELLOW}Build Type: $BUILD_TYPE${NC}"
if [ "$BUILD_TYPE" = "Debug" ]; then
    echo -e "${YELLOW}Using dependencies from: src/libs/lib/linux/debug/${NC}"
else
    echo -e "${YELLOW}Using dependencies from: src/libs/lib/linux/release/${NC}"
fi
echo -e "${GREEN}========================================${NC}"
echo ""
cmake $CMAKE_OPTIONS ..

# Build
echo -e "${GREEN}Building Glimmer static library...${NC}"
make -j$(nproc)

# Check if build was successful
if [ -f "${SCRIPT_DIR}/staticlib/libglimmer.a" ]; then
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}Build Successful! ($BUILD_TYPE)${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo -e "Static library: ${GREEN}${SCRIPT_DIR}/staticlib/libglimmer.a${NC}"
    
    # Show library size
    LIB_SIZE=$(du -h "${SCRIPT_DIR}/staticlib/libglimmer.a" | cut -f1)
    echo -e "Library size: ${YELLOW}${LIB_SIZE}${NC}"
    echo -e "Build type: ${YELLOW}${BUILD_TYPE}${NC}"
    echo ""
else
    echo -e "${RED}Build failed - library not found${NC}"
    exit 1
fi




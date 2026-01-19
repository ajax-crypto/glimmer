#!/bin/bash

# Build script for Glimmer static library (Linux)
# Supports multiple platform targets with automatic dependency building

set -e  # Exit on error

# Enable newer GCC if available (needed for C++20 support)
if [ -f /opt/rh/gcc-toolset-12/enable ]; then
    echo "Enabling GCC Toolset 12..."
    source /opt/rh/gcc-toolset-12/enable
elif [ -f /opt/rh/gcc-toolset-11/enable ]; then
    echo "Enabling GCC Toolset 11..."
    source /opt/rh/gcc-toolset-11/enable
elif [ -f /opt/rh/devtoolset-11/enable ]; then
    echo "Enabling DevToolset 11..."
    source /opt/rh/devtoolset-11/enable
else
    echo "Warning: No newer GCC toolset found. Using system GCC $(gcc --version | head -1)"
    echo "Build may fail. Install with: sudo yum install gcc-toolset-12"
fi

gcc --version | head -1
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
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
PLATFORM="sdl3"  # Default platform
BUILD_DEPS=true  # Auto-build dependencies
UPDATE_ALL=false # Force re-download/rebuild/copy of dependencies
ENABLE_SVG=""
ENABLE_IMAGES=""
ENABLE_RICHTEXT=""
ENABLE_PLOTS=""
ENABLE_NFDEXT=""
ENABLE_BLEND2D=""

print_usage() {
    echo "Usage: $0 [options]"
    echo ""
    echo "Platform Options:"
    echo "  --platform=<type>       Target platform (default: sdl3)"
    echo "                          Valid: test, tui, sdl3, glfw"
    echo ""
    echo "Build Options:"
    echo "  -h, --help              Show this help message"
    echo "  -r, --release           Build in Release mode (optimized, default)"
    echo "  -d, --debug             Build in Debug mode (with debug symbols)"
    echo "  -c, --clean             Clean build directory before building"
    echo "  --no-deps               Don't auto-build dependencies"
    echo "  --update                Force re-download/rebuild/copy deps"
    echo ""
    echo "Feature Overrides (skip cloning/building if disabled):"
    echo "  --disable-svg           Disable SVG (skip plutovg/lunasvg)"
    echo "  --disable-images        Disable images (skip stb_image headers)"
    echo "  --disable-richtext      Disable rich text support"
    echo "  --disable-plots         Disable plots (skip implot)"
    echo "  --disable-icon-font     Disable icon fonts (skip fontawesome headers)"
    echo "  --enable-nfdext         Enable nfd-extended library"
    echo "  --enable-blend2d        Enable Blend2D renderer"
    echo ""
    echo "Platform Defaults:"
    echo "  test   - Minimal (imgui + freetype + yoga)"
    echo "         Output: libglimmer_test.a"
    echo "  tui    - Terminal UI (+ pdcurses + json)"
    echo "         Output: libglimmer_tui.a"
    echo "  sdl3   - Full (+ implot + svg + images + icons + blend2d)"
    echo "         Output: libglimmer_sdl3.a"
    echo "  glfw   - GUI (+ implot + svg + images + icons + nfd-ext)"
    echo "         Output: libglimmer_glfw.a"
    echo ""
    echo "Examples:"
    echo "  $0                                # Build SDL3 platform (default)"
    echo "  $0 --platform=glfw                # Build GLFW platform"
    echo "  $0 --platform=sdl3 --disable-svg  # SDL3 without SVG"
    echo "  $0 --platform=glfw --disable-plots # GLFW without plotting"
    echo ""
}

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            print_usage
            exit 0
            ;;
        --platform=*)
            PLATFORM="${1#*=}"
            shift
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
        --no-deps)
            BUILD_DEPS=false
            shift
            ;;
        --update)
            UPDATE_ALL=true
            shift
            ;;
        --disable-svg)
            ENABLE_SVG="false"
            shift
            ;;
        --disable-images)
            ENABLE_IMAGES="false"
            shift
            ;;
        --disable-icon-font)
            ENABLE_ICON_FONT="false"
            shift
            ;;
        --disable-richtext)
            ENABLE_RICHTEXT="false"
            shift
            ;;
        --disable-plots)
            ENABLE_PLOTS="false"
            shift
            ;;
        --enable-nfdext)
            ENABLE_NFDEXT="true"
            shift
            ;;
        --enable-blend2d)
            ENABLE_BLEND2D="true"
            shift
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            print_usage
            exit 1
            ;;
    esac
done

# Validate platform
case $PLATFORM in
    test|tui|sdl3|glfw)
        ;;
    *)
        echo -e "${RED}Invalid platform: $PLATFORM${NC}"
        echo -e "${RED}Valid platforms: test, tui, sdl3, glfw${NC}"
        exit 1
        ;;
esac

# Set library name based on platform
case $PLATFORM in
    test)
        LIBRARY_NAME="glimmer_test"
        ;;
    tui)
        LIBRARY_NAME="glimmer_tui"
        ;;
    sdl3)
        LIBRARY_NAME="glimmer_sdl3"
        ;;
    glfw)
        LIBRARY_NAME="glimmer_glfw"
        ;;
esac

# Build dependencies if requested
if [ "$BUILD_DEPS" = true ]; then
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}Building Dependencies First${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    
    # Prepare dependency build flags
    DEP_FLAGS="--platform=$PLATFORM"
    
    if [ "$BUILD_TYPE" = "Debug" ]; then
        DEP_FLAGS="$DEP_FLAGS --debug"
    else
        DEP_FLAGS="$DEP_FLAGS --release"
    fi
    
    # Pass feature disable flags to dependency build
    if [ "$ENABLE_SVG" = "false" ]; then
        DEP_FLAGS="$DEP_FLAGS --disable-svg"
    fi
    
    if [ "$ENABLE_PLOTS" = "false" ]; then
        DEP_FLAGS="$DEP_FLAGS --disable-plots"
    fi
    
    if [ "$ENABLE_IMAGES" = "false" ]; then
        DEP_FLAGS="$DEP_FLAGS --disable-images"
    fi
    
    if [ "$UPDATE_ALL" = true ]; then
        DEP_FLAGS="$DEP_FLAGS --update"
    fi

    # Check for icon font disable (need to add this flag)
    if [[ "$*" == *"--disable-icon-font"* ]]; then
        DEP_FLAGS="$DEP_FLAGS --disable-icon-font"
    fi
    
    echo -e "${YELLOW}Running: build_dependencies.sh $DEP_FLAGS${NC}"
    "$SCRIPT_DIR/build_dependencies.sh" $DEP_FLAGS
    
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}Dependencies Built, Continuing...${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
fi

# Clean build if requested
if [ "$CLEAN_BUILD" = true ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Prepare CMake options (pass platform to CMake)
CMAKE_OPTIONS="-DCMAKE_BUILD_TYPE=$BUILD_TYPE"
CMAKE_OPTIONS="$CMAKE_OPTIONS -DGLIMMER_PLATFORM=$PLATFORM"
if [ "$UPDATE_ALL" = true ]; then
    CMAKE_OPTIONS="$CMAKE_OPTIONS -DGLIMMER_FORCE_UPDATE=ON"
fi

# Set platform-specific defaults if not overridden
case $PLATFORM in
    test)
        # Minimal: disable everything by default
        [ -z "$ENABLE_SVG" ] && ENABLE_SVG="false"
        [ -z "$ENABLE_IMAGES" ] && ENABLE_IMAGES="false"
        [ -z "$ENABLE_PLOTS" ] && ENABLE_PLOTS="false"
        [ -z "$ENABLE_RICHTEXT" ] && ENABLE_RICHTEXT="false"
        [ -z "$ENABLE_NFDEXT" ] && ENABLE_NFDEXT="false"
        [ -z "$ENABLE_BLEND2D" ] && ENABLE_BLEND2D="false"
        ;;
    tui)
        # Terminal: disable graphics features
        [ -z "$ENABLE_SVG" ] && ENABLE_SVG="false"
        [ -z "$ENABLE_IMAGES" ] && ENABLE_IMAGES="false"
        [ -z "$ENABLE_PLOTS" ] && ENABLE_PLOTS="false"
        [ -z "$ENABLE_NFDEXT" ] && ENABLE_NFDEXT="false"
        [ -z "$ENABLE_BLEND2D" ] && ENABLE_BLEND2D="false"
        ;;
    sdl3)
        # Full SDL3: enable all except nfd-ext
        [ -z "$ENABLE_SVG" ] && ENABLE_SVG="true"
        [ -z "$ENABLE_IMAGES" ] && ENABLE_IMAGES="true"
        [ -z "$ENABLE_PLOTS" ] && ENABLE_PLOTS="true"
        [ -z "$ENABLE_RICHTEXT" ] && ENABLE_RICHTEXT="true"
        [ -z "$ENABLE_NFDEXT" ] && ENABLE_NFDEXT="false"
        [ -z "$ENABLE_BLEND2D" ] && ENABLE_BLEND2D="true"
        ;;
    glfw)
        # Full GLFW: enable all including nfd-ext, no blend2d
        [ -z "$ENABLE_SVG" ] && ENABLE_SVG="true"
        [ -z "$ENABLE_IMAGES" ] && ENABLE_IMAGES="true"
        [ -z "$ENABLE_PLOTS" ] && ENABLE_PLOTS="true"
        [ -z "$ENABLE_RICHTEXT" ] && ENABLE_RICHTEXT="true"
        [ -z "$ENABLE_NFDEXT" ] && ENABLE_NFDEXT="true"
        [ -z "$ENABLE_BLEND2D" ] && ENABLE_BLEND2D="false"
        ;;
esac

# Apply feature flags to CMake
if [ "$ENABLE_SVG" = "false" ]; then
    CMAKE_OPTIONS="$CMAKE_OPTIONS -DGLIMMER_DISABLE_SVG=ON"
fi

if [ "$ENABLE_IMAGES" = "false" ]; then
    CMAKE_OPTIONS="$CMAKE_OPTIONS -DGLIMMER_DISABLE_IMAGES=ON"
fi

if [ "$ENABLE_RICHTEXT" = "false" ]; then
    CMAKE_OPTIONS="$CMAKE_OPTIONS -DGLIMMER_DISABLE_RICHTEXT=ON"
fi

if [ "$ENABLE_PLOTS" = "false" ]; then
    CMAKE_OPTIONS="$CMAKE_OPTIONS -DGLIMMER_DISABLE_PLOTS=ON"
fi

if [ "$ENABLE_NFDEXT" = "true" ]; then
    CMAKE_OPTIONS="$CMAKE_OPTIONS -DGLIMMER_ENABLE_NFDEXT=ON"
fi

if [ "$ENABLE_BLEND2D" = "true" ]; then
    CMAKE_OPTIONS="$CMAKE_OPTIONS -DGLIMMER_ENABLE_BLEND2D=ON"
fi

# Run CMake
echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Configuring Glimmer Build${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "${YELLOW}Platform: $PLATFORM${NC}"
echo -e "${YELLOW}Library: lib${LIBRARY_NAME}.a${NC}"
echo -e "${YELLOW}Build Type: $BUILD_TYPE${NC}"
if [ "$BUILD_TYPE" = "Debug" ]; then
    echo -e "${YELLOW}Dependencies: src/libs/lib/linux/debug/$PLATFORM/${NC}"
else
    echo -e "${YELLOW}Dependencies: src/libs/lib/linux/release/$PLATFORM/${NC}"
fi
echo -e "${GREEN}========================================${NC}"
echo ""
cmake $CMAKE_OPTIONS ..

# Check if CMake succeeded
if [ $? -ne 0 ]; then
    echo -e "${RED}CMake configuration failed!${NC}"
    echo ""
    echo -e "${YELLOW}Possible issues:${NC}"
    echo -e "  1. Missing dependencies - run: ./build_dependencies.sh --platform=$PLATFORM"
    echo -e "  2. Missing system packages (X11, OpenGL, etc.)"
    echo ""
    exit 1
fi

# Build
echo ""
echo -e "${GREEN}Building Glimmer static library...${NC}"
make -j$(nproc)

# Check if build was successful
if [ -f "${SCRIPT_DIR}/staticlib/lib${LIBRARY_NAME}.a" ]; then
    echo ""
    echo -e "${BLUE}================================================================================${NC}"
    echo -e "${BLUE}                    BUILD COMPLETE - GLIMMER LIBRARY                           ${NC}"
    echo -e "${BLUE}================================================================================${NC}"
    echo ""
    echo -e "${GREEN}✓ Build Successful!${NC}"
    echo ""
    
    # Show main library info
    LIB_SIZE=$(ls -lh "${SCRIPT_DIR}/staticlib/lib${LIBRARY_NAME}.a" | awk '{print $5}')
    echo -e "${YELLOW}Library Name:${NC}    lib${LIBRARY_NAME}.a"
    echo -e "${YELLOW}Full Path:${NC}       ${SCRIPT_DIR}/staticlib/lib${LIBRARY_NAME}.a"
    echo -e "${YELLOW}Library Size:${NC}    ${LIB_SIZE}"
    echo -e "${YELLOW}Platform:${NC}        ${PLATFORM}"
    echo -e "${YELLOW}Build Type:${NC}      ${BUILD_TYPE}"
    echo ""
    
    # Show dependencies location
    if [ "$BUILD_TYPE" = "Debug" ]; then
        DEP_PATH="${SCRIPT_DIR}/src/libs/lib/linux/debug/${PLATFORM}"
    else
        DEP_PATH="${SCRIPT_DIR}/src/libs/lib/linux/release/${PLATFORM}"
    fi
    
    echo -e "${BLUE}================================================================================${NC}"
    echo -e "${BLUE}                           DEPENDENCIES USED                                   ${NC}"
    echo -e "${BLUE}================================================================================${NC}"
    echo ""
    echo -e "${YELLOW}Dependencies from:${NC} $DEP_PATH"
    echo ""
    
    if [ -d "$DEP_PATH" ]; then
        # Count and show dependencies
        DEP_COUNT=$(ls -1 "$DEP_PATH"/*.a 2>/dev/null | wc -l)
        if [ "$DEP_COUNT" -gt 0 ]; then
            printf "${YELLOW}%-25s %-10s %-15s${NC}\n" "DEPENDENCY" "SIZE" "STATUS"
            echo "--------------------------------------------------------------------------------"
            for lib in "$DEP_PATH"/*.a; do
                if [ -f "$lib" ]; then
                    libname=$(basename "$lib")
                    libsize=$(ls -lh "$lib" | awk '{print $5}')
                    printf "${GREEN}%-25s${NC} %-10s ${GREEN}%-15s${NC}\n" "$libname" "$libsize" "✓ Found"
                fi
            done
            echo ""
            TOTAL_DEP_SIZE=$(du -sh "$DEP_PATH" 2>/dev/null | awk '{print $1}')
            echo -e "${YELLOW}Total Dependencies:${NC}      $DEP_COUNT libraries"
            echo -e "${YELLOW}Total Dependencies Size:${NC} $TOTAL_DEP_SIZE"
            echo ""
            echo -e "${GREEN}✓ All required dependencies are present${NC}"
        else
            echo -e "${YELLOW}⚠ No dependency libraries found${NC}"
            echo ""
            echo "This may be normal if this is a minimal build."
            echo "To build dependencies, run: ./build_dependencies.sh --platform=$PLATFORM"
        fi
    else
        echo -e "${YELLOW}⚠ Dependency directory not found: $DEP_PATH${NC}"
        echo ""
        echo "Dependencies need to be built first."
        echo "Run: ./build_dependencies.sh --platform=$PLATFORM"
    fi
    
    echo ""
    echo -e "${BLUE}================================================================================${NC}"
    echo -e "${BLUE}                            ENABLED FEATURES                                   ${NC}"
    echo -e "${BLUE}================================================================================${NC}"
    echo ""
    
    # Show feature summary
    [ "$ENABLE_SVG" = "true" ] && echo -e "  ${GREEN}✓${NC} SVG Support (PlutoVG + LunaSVG)" || echo -e "  ${YELLOW}✗${NC} SVG Support (disabled)"
    [ "$ENABLE_IMAGES" = "true" ] && echo -e "  ${GREEN}✓${NC} Image Support (stb_image)" || echo -e "  ${YELLOW}✗${NC} Image Support (disabled)"
    [ "$ENABLE_PLOTS" = "true" ] && echo -e "  ${GREEN}✓${NC} Plot Support (ImPlot)" || echo -e "  ${YELLOW}✗${NC} Plot Support (disabled)"
    [ "$ENABLE_RICHTEXT" = "true" ] && echo -e "  ${GREEN}✓${NC} Rich Text (FreeType)" || echo -e "  ${YELLOW}✗${NC} Rich Text (disabled)"
    [ "$ENABLE_NFDEXT" = "true" ] && echo -e "  ${GREEN}✓${NC} File Dialogs (nfd-extended)" || echo -e "  ${YELLOW}✗${NC} File Dialogs (not in platform)"
    [ "$ENABLE_BLEND2D" = "true" ] && echo -e "  ${GREEN}✓${NC} Blend2D Renderer" || echo -e "  ${YELLOW}✗${NC} Blend2D Renderer (not in platform)"
    
    # Platform-specific features
    case $PLATFORM in
        sdl3)
            echo -e "  ${GREEN}✓${NC} SDL3 Backend"
            echo -e "  ${GREEN}✓${NC} Hardware Acceleration"
            ;;
        glfw)
            echo -e "  ${GREEN}✓${NC} GLFW Backend"
            echo -e "  ${GREEN}✓${NC} Hardware Acceleration"
            ;;
        tui)
            echo -e "  ${GREEN}✓${NC} Terminal UI (PDCurses)"
            echo -e "  ${GREEN}✓${NC} JSON Support"
            ;;
        test)
            echo -e "  ${GREEN}✓${NC} Minimal Build (Testing Only)"
            ;;
    esac
    
    echo ""
    echo -e "${BLUE}================================================================================${NC}"
    echo -e "${BLUE}                              HOW TO USE                                       ${NC}"
    echo -e "${BLUE}================================================================================${NC}"
    echo ""
    echo -e "${YELLOW}1. Link against the library in your project:${NC}"
    echo ""
    echo "   g++ your_app.cpp -o your_app \\"
    echo "       -I${SCRIPT_DIR}/src \\"
    echo "       -I${SCRIPT_DIR}/src/libs/inc \\"
    echo "       -L${SCRIPT_DIR}/staticlib \\"
    echo "       -l${LIBRARY_NAME} \\"
    case $PLATFORM in
        sdl3)
            echo "       -lX11 -lGL -lpthread -ldl -lm"
            ;;
        glfw)
            echo "       -lX11 -lGL -lpthread -ldl -lm"
            ;;
        tui)
            echo "       -lpthread"
            ;;
        test)
            echo "       -lpthread -lm"
            ;;
    esac
    echo ""
    echo -e "${YELLOW}2. Include in your C++ code:${NC}"
    echo ""
    echo "   #include <glimmer.h>"
    echo ""
    echo -e "${YELLOW}3. Check installed dependency versions:${NC}"
    echo ""
    echo "   cat ${SCRIPT_DIR}/src/libs/inc/DEPENDENCY_VERSIONS.txt"
    echo ""
    echo -e "${BLUE}================================================================================${NC}"
    echo -e "${GREEN}✓ Build Complete! Ready to use lib${LIBRARY_NAME}.a in your projects!${NC}"
    echo -e "${BLUE}================================================================================${NC}"
    echo ""
else
    echo ""
    echo -e "${RED}================================================================================${NC}"
    echo -e "${RED}                              BUILD FAILED                                     ${NC}"
    echo -e "${RED}================================================================================${NC}"
    echo ""
    echo -e "${RED}✗ Library not found: ${SCRIPT_DIR}/staticlib/lib${LIBRARY_NAME}.a${NC}"
    echo ""
    echo "Please check the build log above for errors."
    echo ""
    exit 1
fi

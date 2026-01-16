#!/bin/bash

# Build script for Glimmer Linux dependencies
# Uses specific release versions (wget) instead of cloning master branch
# Supports multiple platform targets with conditional dependency building
# Supports feature disable flags to skip unnecessary dependencies
#
# SYSTEM DEPENDENCIES REQUIRED:
# - libXrandr-devel (for SDL3 XRANDR multi-monitor support)
# - libXrender-devel (dependency of libXrandr-devel)
# - gcc-toolset-12 (for C++20 support in Yoga)
# - wget or curl (for downloading release tarballs)
#
# Install on Rocky Linux/RHEL 8:
#   sudo yum install -y libXrender-devel libXrandr-devel gcc-toolset-12 wget

set -e  # Exit on error

# Get the directory where this script is located (portable)
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$SCRIPT_DIR"

# Enable newer GCC if available (needed for C++20 support in Yoga)
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
    echo "Yoga build may fail. Install with: sudo yum install gcc-toolset-12"
fi

gcc --version | head -1

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

#==============================================================================
# Release versions and download URLs - Update these to use specific stable releases
#==============================================================================

# Font rendering
FREETYPE_VERSION="2.14.1"
FREETYPE_URL="https://gitlab.freedesktop.org/freetype/freetype/-/archive/VER-2-14-1/freetype-VER-2-14-1.tar.gz?ref_type=tags"

# ImGui core library
IMGUI_VERSION="1.92.5"
IMGUI_URL="https://github.com/ocornut/imgui/archive/refs/tags/v${IMGUI_VERSION}.tar.gz"

# ImGui plotting extension
IMPLOT_VERSION="0.17"
IMPLOT_URL="https://github.com/epezent/implot/archive/refs/tags/v${IMPLOT_VERSION}.tar.gz"

# SVG libraries
PLUTOVG_VERSION="1.3.2"
PLUTOVG_URL="https://github.com/sammycage/plutovg/archive/refs/tags/v${PLUTOVG_VERSION}.tar.gz"

LUNASVG_VERSION="3.5.0"
LUNASVG_URL="https://github.com/sammycage/lunasvg/archive/refs/tags/v${LUNASVG_VERSION}.tar.gz"

# Layout engine
YOGA_VERSION="3.2.1"
YOGA_URL="https://github.com/facebook/yoga/archive/refs/tags/v${YOGA_VERSION}.tar.gz"

# Platform backends
SDL3_VERSION="3.4.0"
SDL3_URL="https://github.com/libsdl-org/SDL/archive/refs/tags/release-${SDL3_VERSION}.tar.gz"

GLFW_VERSION="3.4"
GLFW_URL="https://github.com/glfw/glfw/archive/refs/tags/${GLFW_VERSION}.tar.gz"

# 2D rendering engine
BLEND2D_VERSION="0.21.2"
BLEND2D_URL="https://blend2d.com/download/blend2d-${BLEND2D_VERSION}.tar.gz"

ASMJIT_VERSION="2024-11-18"
ASMJIT_URL="https://github.com/asmjit/asmjit/archive/refs/tags/${ASMJIT_VERSION}.tar.gz"

# File dialogs
NFD_VERSION="1.3.0"
NFD_URL="https://github.com/btzy/nativefiledialog-extended/archive/refs/tags/v${NFD_VERSION}.tar.gz"

# Terminal UI
PDCURSES_VERSION="3.9"
PDCURSES_URL="https://github.com/wmcbrine/PDCurses/archive/refs/tags/${PDCURSES_VERSION}.tar.gz"

# JSON support (full archive, not just header)
JSON_VERSION="3.12.0"
JSON_URL="https://github.com/nlohmann/json/archive/refs/tags/v${JSON_VERSION}.tar.gz"

# Header-only libraries
ICONFONT_URL="https://github.com/juliettef/IconFontCppHeaders/archive/refs/heads/main.zip"
STB_IMAGE_URL="https://raw.githubusercontent.com/nothings/stb/master/stb_image.h"

#==============================================================================
# Parse command line arguments
#==============================================================================
BUILD_TYPE="Release"
LIB_SUBDIR="release"
PLATFORM="sdl3"  # Default platform
UPDATE_ALL=false # Force re-download/rebuild/copy

# Feature flags (auto-detected from platform, or explicitly set)
ENABLE_PLOTS=""
ENABLE_SVG=""
ENABLE_IMAGES=""
ENABLE_ICON_FONT=""

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
    echo "  --update                Force re-download/rebuild/copy even if present"
    echo ""
    echo "Feature Options (override platform defaults):"
    echo "  --disable-plots         Skip implot (no downloading/building)"
    echo "  --disable-svg           Skip plutovg/lunasvg (no downloading/building)"
    echo "  --disable-images        Skip stb_image headers (no downloading)"
    echo "  --disable-icon-font     Skip fontawesome headers (no downloading)"
    echo ""
    echo "Platform Defaults:"
    echo "  test   - Minimal (imgui + freetype + yoga only)"
    echo "  tui    - Terminal UI (+ pdcurses + json)"
    echo "  sdl3   - Full (+ implot + svg + images + icons + blend2d)"
    echo "  glfw   - GUI (+ implot + svg + images + icons + nfd-ext)"
    echo ""
    echo "Release Versions Used:"
    echo "  PlutoVG:   $PLUTOVG_VERSION"
    echo "  LunaSVG:   $LUNASVG_VERSION"
    echo "  SDL3:      $SDL3_VERSION"
    echo "  GLFW:      $GLFW_VERSION"
    echo "  FreeType:  $FREETYPE_VERSION"
    echo "  Blend2D:   $BLEND2D_VERSION"
    echo "  AsmJit:    $ASMJIT_VERSION"
    echo "  NFD-ext:   $NFD_VERSION"
    echo "  PDCurses:  $PDCURSES_VERSION"
    echo "  JSON:      $JSON_VERSION"
    echo ""
    echo "Examples:"
    echo "  $0 --platform=sdl3                  # Full SDL3 with all features"
    echo "  $0 --platform=sdl3 --disable-plots  # SDL3 without plotting"
    echo "  $0 --platform=glfw --disable-svg    # GLFW without SVG support"
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
            LIB_SUBDIR="release"
            shift
            ;;
        -d|--debug)
            BUILD_TYPE="Debug"
            LIB_SUBDIR="debug"
            shift
            ;;
        --update)
            UPDATE_ALL=true
            shift
            ;;
        --disable-plots)
            ENABLE_PLOTS="false"
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

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Building Glimmer Dependencies${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}Platform: $PLATFORM${NC}"
echo -e "${GREEN}Build Type: $BUILD_TYPE${NC}"
echo -e "${GREEN}Update Mode: $UPDATE_ALL${NC}"
echo -e "${GREEN}Output Dir: $PROJECT_ROOT/src/libs/lib/linux/$LIB_SUBDIR/$PLATFORM/${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Tracking arrays for summary
declare -a LIBS_BUILT=()
declare -a LIBS_SKIPPED=()
declare -a HEADERS_COPIED=()
declare -a HEADERS_SKIPPED=()

# Create directory structure
echo -e "${YELLOW}Creating directory structure...${NC}"
LIB_OUTPUT_DIR="$PROJECT_ROOT/src/libs/lib/linux/$LIB_SUBDIR/$PLATFORM"
mkdir -p "$LIB_OUTPUT_DIR"
BUILD_DIR="$PROJECT_ROOT/../glimmer_deps_build_${LIB_SUBDIR}_${PLATFORM}"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Header directories
HEADER_DIR="$PROJECT_ROOT/src/libs/inc"
mkdir -p "$HEADER_DIR"

# Version tracking file
VERSION_FILE="$HEADER_DIR/DEPENDENCY_VERSIONS.txt"

# Detect download tool
if command -v wget &> /dev/null; then
    DOWNLOAD_CMD="wget -O"
elif command -v curl &> /dev/null; then
    DOWNLOAD_CMD="curl -L -o"
else
    echo -e "${RED}Error: Neither wget nor curl found. Please install one.${NC}"
    exit 1
fi

# ==============================================================================
# Dependency flags based on platform + explicit overrides
# ==============================================================================
BUILD_IMGUI=true          # Always required (core)
BUILD_FREETYPE=true       # Always required (fonts)
BUILD_YOGA=true           # Always required (layout)
BUILD_IMPLOT=false        # Optional: plotting
BUILD_PLUTOVG=false       # Optional: SVG rendering
BUILD_LUNASVG=false       # Optional: SVG parsing
BUILD_SDL3=false          # Platform: SDL3 backend
BUILD_GLFW=false          # Platform: GLFW backend
BUILD_BLEND2D=false       # Optional: 2D renderer
BUILD_NFDEXT=false        # Optional: file dialogs
BUILD_PDCURSES=false      # Platform: terminal UI
BUILD_JSON=false          # Platform: JSON config
COPY_STB_IMAGE=false      # Header-only: image loading
COPY_FONTAWESOME=false    # Header-only: icon fonts

# Set defaults based on platform
case $PLATFORM in
    test)
        # Minimal: imgui + freetype + yoga only
        ;;
    tui)
        # Terminal: + pdcurses + json
        BUILD_PDCURSES=true
        BUILD_JSON=true
        ;;
    sdl3)
        # Full SDL3: + implot + svg + images + icons + blend2d + SDL3
        BUILD_IMPLOT=true
        BUILD_PLUTOVG=true
        BUILD_LUNASVG=true
        BUILD_SDL3=true
        BUILD_BLEND2D=true
        COPY_STB_IMAGE=true
        COPY_FONTAWESOME=true
        ;;
    glfw)
        # GLFW: + implot + svg + images + icons + GLFW + nfd-ext
        BUILD_IMPLOT=true
        BUILD_PLUTOVG=true
        BUILD_LUNASVG=true
        BUILD_GLFW=true
        BUILD_NFDEXT=true
        COPY_STB_IMAGE=true
        COPY_FONTAWESOME=true
        ;;
esac

# Apply explicit overrides (disable flags)
if [ "$ENABLE_PLOTS" = "false" ]; then
    BUILD_IMPLOT=false
fi

if [ "$ENABLE_SVG" = "false" ]; then
    BUILD_PLUTOVG=false
    BUILD_LUNASVG=false
fi

if [ "$ENABLE_IMAGES" = "false" ]; then
    COPY_STB_IMAGE=false
fi

if [ "$ENABLE_ICON_FONT" = "false" ]; then
    COPY_FONTAWESOME=false
fi

# Count libraries to build
LIB_COUNT=0
[[ "$BUILD_IMGUI" == true ]] && ((LIB_COUNT++)) || true
[[ "$BUILD_FREETYPE" == true ]] && ((LIB_COUNT++)) || true
[[ "$BUILD_YOGA" == true ]] && ((LIB_COUNT++)) || true
[[ "$BUILD_IMPLOT" == true ]] && ((LIB_COUNT++)) || true
[[ "$BUILD_PLUTOVG" == true ]] && ((LIB_COUNT++)) || true
[[ "$BUILD_LUNASVG" == true ]] && ((LIB_COUNT++)) || true
[[ "$BUILD_SDL3" == true ]] && ((LIB_COUNT++)) || true
[[ "$BUILD_GLFW" == true ]] && ((LIB_COUNT++)) || true
[[ "$BUILD_BLEND2D" == true ]] && ((LIB_COUNT++)) || true
[[ "$BUILD_NFDEXT" == true ]] && ((LIB_COUNT++)) || true
[[ "$BUILD_PDCURSES" == true ]] && ((LIB_COUNT++)) || true
[[ "$BUILD_JSON" == true ]] && ((LIB_COUNT++)) || true
[[ "$COPY_STB_IMAGE" == true ]] && ((LIB_COUNT++)) || true
[[ "$COPY_FONTAWESOME" == true ]] && ((LIB_COUNT++)) || true

echo -e "${GREEN}Processing $LIB_COUNT items for $PLATFORM platform${NC}"
echo ""

# Initialize version tracking file
cat > "$VERSION_FILE" << EOF
================================================================================
Glimmer Dependency Versions
================================================================================
Platform: $PLATFORM
Build Type: $BUILD_TYPE
Build Date: $(date '+%Y-%m-%d %H:%M:%S')
================================================================================

EOF

CURRENT_LIB=0

# ==============================================================================
# BUILD ORDER (Optimized Dependency Chain):
#   1. FreeType + Yoga          (Core dependencies - fonts & layout)
#   2. PlutoVG                  (SVG vector rendering)
#   3. LunaSVG                  (SVG parsing - depends on PlutoVG)
#   4. Platform Backends:
#      - SDL3 + Blend2D         (for --platform=sdl3)
#      - GLFW + nfd-ext         (for --platform=glfw)
#      - PDCurses               (for --platform=tui)
#   5. ImGui + Backend          (GUI after platform is ready)
#   6. ImPlot                   (Plotting after ImGui)
# ==============================================================================

# ==============================================================================
# 1. Build FreeType FIRST (font rendering - ALWAYS REQUIRED)
# ==============================================================================
if [ "$BUILD_FREETYPE" = true ]; then
    CURRENT_LIB=$((CURRENT_LIB+1))
    echo -e "${GREEN}[$CURRENT_LIB/$LIB_COUNT] Building freetype v${FREETYPE_VERSION}... (BUILD FIRST)${NC}"
cd "$BUILD_DIR"
if [ -f "$LIB_OUTPUT_DIR/libfreetype.a" ] && [ "$UPDATE_ALL" != true ]; then
    echo -e "${YELLOW}✓ libfreetype.a already exists, skipping build${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libfreetype.a"
        LIBS_SKIPPED+=("FreeType v${FREETYPE_VERSION}")
    else
        LIBS_BUILT+=("FreeType v${FREETYPE_VERSION}")
        FREETYPE_DIR="freetype-VER-${FREETYPE_VERSION//./-}"
        if [ "$UPDATE_ALL" = true ]; then
            rm -rf "$FREETYPE_DIR"
        fi
        if [ ! -d "$FREETYPE_DIR" ]; then
            echo -e "${YELLOW}Downloading freetype v${FREETYPE_VERSION}...${NC}"
            $DOWNLOAD_CMD freetype.tar.gz $FREETYPE_URL
            tar -xzf freetype.tar.gz
            rm freetype.tar.gz
        else
            echo -e "${YELLOW}freetype v${FREETYPE_VERSION} source found, building...${NC}"
        fi
        cd "$FREETYPE_DIR"
    rm -rf build
    mkdir build && cd build
    cmake .. \
        -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
        -DBUILD_SHARED_LIBS=OFF \
        -DFT_DISABLE_ZLIB=ON \
        -DFT_DISABLE_BZIP2=ON \
        -DFT_DISABLE_PNG=OFF \
        -DFT_DISABLE_HARFBUZZ=ON \
        -DFT_DISABLE_BROTLI=ON
    make -j$(nproc)
    if [ "$BUILD_TYPE" = "Debug" ]; then
        cp libfreetyped.a "$LIB_OUTPUT_DIR/libfreetype.a"
    else
        cp libfreetype.a "$LIB_OUTPUT_DIR/"
    fi
        
        # Copy headers (check if already copied for this version)
        if [ "$UPDATE_ALL" != true ] && grep -q "FreeType: v${FREETYPE_VERSION}" "$VERSION_FILE" 2>/dev/null && [ -d "$HEADER_DIR/freetype2" ] && [ "$(ls -A $HEADER_DIR/freetype2 2>/dev/null)" ]; then
            echo -e "${YELLOW}✓ FreeType v${FREETYPE_VERSION} headers already installed, skipping copy${NC}"
            HEADERS_SKIPPED+=("FreeType v${FREETYPE_VERSION}")
        else
            echo -e "${YELLOW}Copying freetype v${FREETYPE_VERSION} headers...${NC}"
            HEADERS_COPIED+=("FreeType v${FREETYPE_VERSION}")
            make install DESTDIR="$BUILD_DIR/freetype-install" > /dev/null 2>&1 || true
            mkdir -p "$HEADER_DIR/freetype2"
            cp -r "$BUILD_DIR/freetype-install"/usr/local/include/freetype2/* "$HEADER_DIR/freetype2/" 2>/dev/null || \
            cp -r ../include/* "$HEADER_DIR/freetype2/" 2>/dev/null || true
            echo -e "${GREEN}✓ Headers copied${NC}"
        fi
        
        # Track version
        grep -q "FreeType: v${FREETYPE_VERSION}" "$VERSION_FILE" 2>/dev/null || echo "FreeType: v${FREETYPE_VERSION}" >> "$VERSION_FILE"
        
        echo -e "${GREEN}✓ freetype built successfully (headers copied)${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libfreetype.a"
    cd "$BUILD_DIR"
fi
echo ""
fi

# ==============================================================================
# 2. Build Yoga (flexbox layout engine - ALWAYS REQUIRED)
# ==============================================================================
if [ "$BUILD_YOGA" = true ]; then
    CURRENT_LIB=$((CURRENT_LIB+1))
    echo -e "${GREEN}[$CURRENT_LIB/$LIB_COUNT] Building Yoga v${YOGA_VERSION}... (BUILD SECOND - LAYOUT ENGINE)${NC}"
if [ -f "$LIB_OUTPUT_DIR/libyoga.a" ] && [ "$UPDATE_ALL" != true ]; then
    echo -e "${YELLOW}✓ libyoga.a already exists, skipping build${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libyoga.a"
        LIBS_SKIPPED+=("Yoga v${YOGA_VERSION}")
    else
        LIBS_BUILT+=("Yoga v${YOGA_VERSION}")
        cd "$BUILD_DIR"
        
        YOGA_DIR="yoga-${YOGA_VERSION}"
        if [ "$UPDATE_ALL" = true ]; then
            rm -rf "$YOGA_DIR"
        fi
        if [ ! -d "$YOGA_DIR" ]; then
            echo -e "${YELLOW}Downloading Yoga v${YOGA_VERSION}...${NC}"
            $DOWNLOAD_CMD yoga.tar.gz "$YOGA_URL"
            tar -xzf yoga.tar.gz
            rm yoga.tar.gz
        else
            echo -e "${YELLOW}Yoga v${YOGA_VERSION} source found, building...${NC}"
        fi
        
        cd "$YOGA_DIR"
        
        # Build Yoga (only core library files, exclude bindings and tests)
        echo -e "${YELLOW}Building Yoga from source...${NC}"
        find . -name "*.cpp" \
            -not -path "*/tests/*" \
            -not -path "*/benchmark/*" \
            -not -path "*/capture/*" \
            -not -path "*/fuzz/*" \
            -not -path "*/java/*" \
            -not -path "*/javascript/*" \
            -exec g++ -c -std=c++20 $CFLAGS {} -I. -Iyoga \;
        ar rcs libyoga.a *.o
        cp libyoga.a "$LIB_OUTPUT_DIR/"
        rm -f *.o
        
        # Copy headers
        if [ "$UPDATE_ALL" != true ] && grep -q "Yoga: v${YOGA_VERSION}" "$VERSION_FILE" 2>/dev/null && [ -d "$HEADER_DIR/yoga" ] && [ "$(ls -A $HEADER_DIR/yoga 2>/dev/null)" ]; then
            echo -e "${YELLOW}✓ Yoga v${YOGA_VERSION} headers already installed, skipping copy${NC}"
            HEADERS_SKIPPED+=("Yoga v${YOGA_VERSION}")
        else
            echo -e "${YELLOW}Copying Yoga v${YOGA_VERSION} headers...${NC}"
            HEADERS_COPIED+=("Yoga v${YOGA_VERSION}")
            mkdir -p "$HEADER_DIR/yoga"
            cp -r yoga/*.h "$HEADER_DIR/yoga/" 2>/dev/null || true
            echo -e "${GREEN}✓ Headers copied${NC}"
        fi
        
        # Track version
        grep -q "Yoga: v${YOGA_VERSION}" "$VERSION_FILE" 2>/dev/null || echo "Yoga: v${YOGA_VERSION}" >> "$VERSION_FILE"
        
        echo -e "${GREEN}✓ Yoga built successfully (headers copied)${NC}"
        ls -lh "$LIB_OUTPUT_DIR/libyoga.a"
        cd "$BUILD_DIR"
fi
echo ""
fi

# ==============================================================================
# 3. Build PlutoVG (SVG vector rendering)
# ==============================================================================
if [ "$BUILD_PLUTOVG" = true ]; then
    CURRENT_LIB=$((CURRENT_LIB+1))
    echo -e "${GREEN}[$CURRENT_LIB/$LIB_COUNT] Building plutosvg v${PLUTOVG_VERSION}... (BUILD THIRD - SVG RENDERING)${NC}"
if [ -f "$LIB_OUTPUT_DIR/libplutovg.a" ] && [ "$UPDATE_ALL" != true ]; then
    echo -e "${YELLOW}✓ libplutovg.a already exists, skipping build${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libplutovg.a"
        LIBS_SKIPPED+=("PlutoVG v${PLUTOVG_VERSION}")
    else
        LIBS_BUILT+=("PlutoVG v${PLUTOVG_VERSION}")
        PLUTOVG_DIR="plutovg-${PLUTOVG_VERSION}"
        if [ "$UPDATE_ALL" = true ]; then
            rm -rf "$PLUTOVG_DIR"
        fi
        if [ ! -d "$PLUTOVG_DIR" ]; then
            echo -e "${YELLOW}Downloading plutovg v${PLUTOVG_VERSION}...${NC}"
            $DOWNLOAD_CMD plutovg.tar.gz "$PLUTOVG_URL"
            tar -xzf plutovg.tar.gz
            rm plutovg.tar.gz
        else
            echo -e "${YELLOW}plutovg v${PLUTOVG_VERSION} source found, building...${NC}"
        fi
        cd "$PLUTOVG_DIR"
    rm -rf build
    mkdir build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DBUILD_SHARED_LIBS=OFF
    make -j$(nproc)
    cp libplutovg.a "$LIB_OUTPUT_DIR/"
        
        # Copy headers (check if already copied for this version)
        if [ "$UPDATE_ALL" != true ] && grep -q "PlutoVG: v${PLUTOVG_VERSION}" "$VERSION_FILE" 2>/dev/null && [ -d "$HEADER_DIR/plutovg" ] && [ "$(ls -A $HEADER_DIR/plutovg 2>/dev/null)" ]; then
            echo -e "${YELLOW}✓ PlutoVG v${PLUTOVG_VERSION} headers already installed, skipping copy${NC}"
            HEADERS_SKIPPED+=("PlutoVG v${PLUTOVG_VERSION}")
        else
            echo -e "${YELLOW}Copying plutovg v${PLUTOVG_VERSION} headers...${NC}"
            HEADERS_COPIED+=("PlutoVG v${PLUTOVG_VERSION}")
            cd ..
            mkdir -p "$HEADER_DIR/plutovg"
            cp -r include/* "$HEADER_DIR/plutovg/" 2>/dev/null || cp *.h "$HEADER_DIR/plutovg/" 2>/dev/null || true
            echo -e "${GREEN}✓ Headers copied${NC}"
        fi
        
        # Track version
        grep -q "PlutoVG: v${PLUTOVG_VERSION}" "$VERSION_FILE" 2>/dev/null || echo "PlutoVG: v${PLUTOVG_VERSION}" >> "$VERSION_FILE"
        
        echo -e "${GREEN}✓ plutovg built successfully (headers copied)${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libplutovg.a"
    cd "$BUILD_DIR"
fi
echo ""
fi

# ==============================================================================
# 4. Build LunaSVG (SVG parsing - depends on PlutoVG)
# ==============================================================================
if [ "$BUILD_LUNASVG" = true ]; then
    CURRENT_LIB=$((CURRENT_LIB+1))
    echo -e "${GREEN}[$CURRENT_LIB/$LIB_COUNT] Building lunasvg v${LUNASVG_VERSION}... (BUILD FOURTH - SVG PARSING)${NC}"
if [ -f "$LIB_OUTPUT_DIR/liblunasvg.a" ] && [ "$UPDATE_ALL" != true ]; then
    echo -e "${YELLOW}✓ liblunasvg.a already exists, skipping build${NC}"
    ls -lh "$LIB_OUTPUT_DIR/liblunasvg.a"
        LIBS_SKIPPED+=("LunaSVG v${LUNASVG_VERSION}")
    else
        LIBS_BUILT+=("LunaSVG v${LUNASVG_VERSION}")
        LUNASVG_DIR="lunasvg-${LUNASVG_VERSION}"
        if [ "$UPDATE_ALL" = true ]; then
            rm -rf "$LUNASVG_DIR"
        fi
        if [ ! -d "$LUNASVG_DIR" ]; then
            echo -e "${YELLOW}Downloading lunasvg v${LUNASVG_VERSION}...${NC}"
            $DOWNLOAD_CMD lunasvg.tar.gz "$LUNASVG_URL"
            tar -xzf lunasvg.tar.gz
            rm lunasvg.tar.gz
        else
            echo -e "${YELLOW}lunasvg v${LUNASVG_VERSION} source found, building...${NC}"
        fi
        cd "$LUNASVG_DIR"
    rm -rf build
    mkdir build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DBUILD_SHARED_LIBS=OFF -DLUNASVG_BUILD_EXAMPLES=OFF
    make -j$(nproc)
    cp liblunasvg.a "$LIB_OUTPUT_DIR/"
        
        # Copy headers (check if already copied for this version)
        if [ "$UPDATE_ALL" != true ] && grep -q "LunaSVG: v${LUNASVG_VERSION}" "$VERSION_FILE" 2>/dev/null && [ -d "$HEADER_DIR/lunasvg" ] && [ "$(ls -A $HEADER_DIR/lunasvg 2>/dev/null)" ]; then
            echo -e "${YELLOW}✓ LunaSVG v${LUNASVG_VERSION} headers already installed, skipping copy${NC}"
            HEADERS_SKIPPED+=("LunaSVG v${LUNASVG_VERSION}")
        else
            echo -e "${YELLOW}Copying lunasvg v${LUNASVG_VERSION} headers...${NC}"
            HEADERS_COPIED+=("LunaSVG v${LUNASVG_VERSION}")
            cd ..
            mkdir -p "$HEADER_DIR/lunasvg"
            cp -r include/lunasvg.h "$HEADER_DIR/lunasvg/" 2>/dev/null || cp -r include/* "$HEADER_DIR/lunasvg/" 2>/dev/null || true
            echo -e "${GREEN}✓ Headers copied${NC}"
        fi
        
        # Track version
        grep -q "LunaSVG: v${LUNASVG_VERSION}" "$VERSION_FILE" 2>/dev/null || echo "LunaSVG: v${LUNASVG_VERSION}" >> "$VERSION_FILE"
        
        echo -e "${GREEN}✓ lunasvg built successfully (headers copied)${NC}"
    ls -lh "$LIB_OUTPUT_DIR/liblunasvg.a"
    cd "$BUILD_DIR"
fi
echo ""
fi

# ==============================================================================
# 5. Build Platform Backends (conditional based on --platform option)
# ==============================================================================

# ==============================================================================
# Build SDL3 (if platform=sdl3)
# ==============================================================================
if [ "$BUILD_SDL3" = true ]; then
    CURRENT_LIB=$((CURRENT_LIB+1))
    echo -e "${GREEN}[$CURRENT_LIB/$LIB_COUNT] Building SDL3 v${SDL3_VERSION} (BUILD FIFTH - PLATFORM BACKEND)...${NC}"
    cd "$BUILD_DIR"
    if [ -f "$LIB_OUTPUT_DIR/libSDL3.a" ] && [ "$UPDATE_ALL" != true ]; then
        echo -e "${YELLOW}✓ libSDL3.a already exists, skipping build${NC}"
        ls -lh "$LIB_OUTPUT_DIR/libSDL3.a"
        LIBS_SKIPPED+=("SDL3 v${SDL3_VERSION}")
    else
        LIBS_BUILT+=("SDL3 v${SDL3_VERSION}")
        SDL3_DIR="SDL-release-${SDL3_VERSION}"
        if [ "$UPDATE_ALL" = true ]; then
            rm -rf "$SDL3_DIR"
        fi
        if [ ! -d "$SDL3_DIR" ]; then
            echo -e "${YELLOW}Downloading SDL3 v${SDL3_VERSION}...${NC}"
            $DOWNLOAD_CMD sdl3.tar.gz "$SDL3_URL"
            tar -xzf sdl3.tar.gz
            rm sdl3.tar.gz
        else
            echo -e "${YELLOW}SDL3 v${SDL3_VERSION} source found, building...${NC}"
        fi
        cd "$SDL3_DIR"
        rm -rf build
        mkdir build && cd build
        
        # Build with X11 support (XTEST disabled - only needed for input automation)
        if cmake .. \
           -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
           -DBUILD_SHARED_LIBS=OFF \
           -DSDL_STATIC=ON \
           -DSDL_SHARED=OFF \
           -DSDL_TEST=OFF \
           -DSDL_X11=ON \
           -DSDL_X11_XRANDR=ON \
           -DSDL_X11_XSCRNSAVER=ON \
           -DSDL_X11_XTEST=OFF \
           -DSDL_WAYLAND=OFF \
           -DSDL_OPENGL=ON && \
           make -j$(nproc) && \
           cp libSDL3.a "$LIB_OUTPUT_DIR/"; then
            
            # Copy headers (check if already copied for this version)
            if [ "$UPDATE_ALL" != true ] && grep -q "SDL3: v${SDL3_VERSION}" "$VERSION_FILE" 2>/dev/null && [ -d "$HEADER_DIR/SDL3" ] && [ "$(ls -A $HEADER_DIR/SDL3 2>/dev/null)" ]; then
                echo -e "${YELLOW}✓ SDL3 v${SDL3_VERSION} headers already installed, skipping copy${NC}"
                HEADERS_SKIPPED+=("SDL3 v${SDL3_VERSION}")
            else
                echo -e "${YELLOW}Copying SDL3 v${SDL3_VERSION} headers...${NC}"
                HEADERS_COPIED+=("SDL3 v${SDL3_VERSION}")
                cd ..
                mkdir -p "$HEADER_DIR/SDL3"
                cp -r include/SDL3/* "$HEADER_DIR/SDL3/" 2>/dev/null || true
                cp -r build/include/SDL3/* "$HEADER_DIR/SDL3/" 2>/dev/null || true
                echo -e "${GREEN}✓ Headers copied${NC}"
            fi
            
            # Track version
            grep -q "SDL3: v${SDL3_VERSION}" "$VERSION_FILE" 2>/dev/null || echo "SDL3: v${SDL3_VERSION}" >> "$VERSION_FILE"
            
            echo -e "${GREEN}✓ SDL3 built successfully (headers copied)${NC}"
        else
            echo -e "${RED}SDL3 build failed${NC}"
            exit 1
        fi
        
        cd "$BUILD_DIR"
    fi
    echo ""
fi

# ==============================================================================
# Build GLFW (if platform=glfw)
# ==============================================================================
if [ "$BUILD_GLFW" = true ]; then
    CURRENT_LIB=$((CURRENT_LIB+1))
    echo -e "${GREEN}[$CURRENT_LIB/$LIB_COUNT] Building GLFW v${GLFW_VERSION} (BUILD FIFTH - PLATFORM BACKEND)...${NC}"
    cd "$BUILD_DIR"
    if [ -f "$LIB_OUTPUT_DIR/libglfw3.a" ] && [ "$UPDATE_ALL" != true ]; then
        echo -e "${YELLOW}✓ libglfw3.a already exists, skipping build${NC}"
        ls -lh "$LIB_OUTPUT_DIR/libglfw3.a"
        LIBS_SKIPPED+=("GLFW v${GLFW_VERSION}")
    else
        LIBS_BUILT+=("GLFW v${GLFW_VERSION}")
        GLFW_DIR="glfw-${GLFW_VERSION}"
        if [ "$UPDATE_ALL" = true ]; then
            rm -rf "$GLFW_DIR"
        fi
        if [ ! -d "$GLFW_DIR" ]; then
            echo -e "${YELLOW}Downloading GLFW v${GLFW_VERSION}...${NC}"
            $DOWNLOAD_CMD glfw.tar.gz "$GLFW_URL"
            tar -xzf glfw.tar.gz
            rm glfw.tar.gz
        else
            echo -e "${YELLOW}GLFW v${GLFW_VERSION} source found, building...${NC}"
        fi
        cd "$GLFW_DIR"
        rm -rf build
        mkdir build && cd build
        cmake .. \
            -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
            -DBUILD_SHARED_LIBS=OFF \
            -DGLFW_BUILD_EXAMPLES=OFF \
            -DGLFW_BUILD_TESTS=OFF \
            -DGLFW_BUILD_DOCS=OFF
        make -j$(nproc)
        cp src/libglfw3.a "$LIB_OUTPUT_DIR/"
        
        # Copy headers (check if already copied for this version)
        if [ "$UPDATE_ALL" != true ] && grep -q "GLFW: v${GLFW_VERSION}" "$VERSION_FILE" 2>/dev/null && [ -d "$HEADER_DIR/GLFW" ] && [ "$(ls -A $HEADER_DIR/GLFW 2>/dev/null)" ]; then
            echo -e "${YELLOW}✓ GLFW v${GLFW_VERSION} headers already installed, skipping copy${NC}"
            HEADERS_SKIPPED+=("GLFW v${GLFW_VERSION}")
        else
            echo -e "${YELLOW}Copying GLFW v${GLFW_VERSION} headers...${NC}"
            HEADERS_COPIED+=("GLFW v${GLFW_VERSION}")
            cd ..
            mkdir -p "$HEADER_DIR/GLFW"
            cp include/GLFW/* "$HEADER_DIR/GLFW/" 2>/dev/null || true
            echo -e "${GREEN}✓ Headers copied${NC}"
        fi
        
        # Track version
        grep -q "GLFW: v${GLFW_VERSION}" "$VERSION_FILE" 2>/dev/null || echo "GLFW: v${GLFW_VERSION}" >> "$VERSION_FILE"
        
        echo -e "${GREEN}✓ GLFW built successfully (headers copied)${NC}"
        cd "$BUILD_DIR"
    fi
    echo ""
fi

# ==============================================================================
# 3. Build EVERYTHING ELSE (ImGui, ImPlot, Yoga, SVG libraries, etc.)
# ==============================================================================

# ==============================================================================
# 6. Build ImGui (immediate mode GUI + FreeType - AFTER PLATFORM READY)
# ==============================================================================
if [ "$BUILD_IMGUI" = true ]; then
    CURRENT_LIB=$((CURRENT_LIB+1))
    echo -e "${GREEN}[$CURRENT_LIB/$LIB_COUNT] Building ImGui v${IMGUI_VERSION}... (BUILD SIXTH - GUI + FREETYPE)${NC}"
    cd "$BUILD_DIR"
    
    if [ -f "$LIB_OUTPUT_DIR/libimgui.a" ] && [ "$UPDATE_ALL" != true ]; then
        echo -e "${YELLOW}✓ libimgui.a already exists, skipping build${NC}"
        ls -lh "$LIB_OUTPUT_DIR/libimgui.a"
        LIBS_SKIPPED+=("ImGui v${IMGUI_VERSION}")
    else
        LIBS_BUILT+=("ImGui v${IMGUI_VERSION}")
        
        IMGUI_DIR="imgui-${IMGUI_VERSION}"
        if [ "$UPDATE_ALL" = true ]; then
            rm -rf "$IMGUI_DIR"
        fi
        if [ ! -d "$IMGUI_DIR" ]; then
            echo -e "${YELLOW}Downloading ImGui v${IMGUI_VERSION}...${NC}"
            $DOWNLOAD_CMD imgui.tar.gz "$IMGUI_URL"
            tar -xzf imgui.tar.gz
            rm imgui.tar.gz
        else
            echo -e "${YELLOW}ImGui v${IMGUI_VERSION} source found, building...${NC}"
        fi
        
        cd "$IMGUI_DIR"
        
        # Build ImGui with FreeType support
        echo -e "${YELLOW}Building ImGui from source...${NC}"
        g++ -c -std=c++17 $CFLAGS \
            -I. \
            -I"$HEADER_DIR/freetype2" \
            -DIMGUI_ENABLE_FREETYPE \
            imgui.cpp \
            imgui_demo.cpp \
            imgui_draw.cpp \
            imgui_tables.cpp \
            imgui_widgets.cpp \
            misc/freetype/imgui_freetype.cpp
        
        ar rcs libimgui.a imgui.o imgui_demo.o imgui_draw.o imgui_tables.o imgui_widgets.o imgui_freetype.o
        cp libimgui.a "$LIB_OUTPUT_DIR/"
        rm -f *.o
        
        # Copy headers (and required backend sources)
        if [ "$UPDATE_ALL" != true ] && grep -q "ImGui: v${IMGUI_VERSION}" "$VERSION_FILE" 2>/dev/null && [ -d "$HEADER_DIR/imgui" ] && [ "$(ls -A $HEADER_DIR/imgui 2>/dev/null)" ]; then
            echo -e "${YELLOW}✓ ImGui v${IMGUI_VERSION} headers already installed, skipping copy${NC}"
            HEADERS_SKIPPED+=("ImGui v${IMGUI_VERSION}")
        else
            echo -e "${YELLOW}Copying ImGui v${IMGUI_VERSION} headers...${NC}"
            HEADERS_COPIED+=("ImGui v${IMGUI_VERSION}")
            mkdir -p "$HEADER_DIR/imgui"
            cp *.h "$HEADER_DIR/imgui/" 2>/dev/null || true
            mkdir -p "$HEADER_DIR/imgui/backends"
            cp backends/*.h "$HEADER_DIR/imgui/backends/" 2>/dev/null || true
            cp backends/*.cpp "$HEADER_DIR/imgui/backends/" 2>/dev/null || true
            if [ "$PLATFORM" = "sdl3" ]; then
                mkdir -p "$HEADER_DIR/imguisdl3"
                cp backends/imgui_impl_sdl3.* "$HEADER_DIR/imguisdl3/" 2>/dev/null || true
                cp backends/imgui_impl_sdlrenderer3.* "$HEADER_DIR/imguisdl3/" 2>/dev/null || true
                cp backends/imgui_impl_sdlgpu3.* "$HEADER_DIR/imguisdl3/" 2>/dev/null || true
                cp backends/imgui_impl_sdlgpu3_shaders.h "$HEADER_DIR/imguisdl3/" 2>/dev/null || true
            fi
            cp -r misc "$HEADER_DIR/imgui/" 2>/dev/null || true
            echo -e "${GREEN}✓ Headers copied${NC}"
        fi
        
        # Track version
        grep -q "ImGui: v${IMGUI_VERSION}" "$VERSION_FILE" 2>/dev/null || echo "ImGui: v${IMGUI_VERSION}" >> "$VERSION_FILE"
        
        echo -e "${GREEN}✓ ImGui built successfully (with FreeType)${NC}"
        ls -lh "$LIB_OUTPUT_DIR/libimgui.a"
        cd "$BUILD_DIR"
    fi
    echo ""
fi

# ==============================================================================
# 7. Build ImPlot (plotting library - AFTER IMGUI)
# ==============================================================================
if [ "$BUILD_IMPLOT" = true ]; then
    CURRENT_LIB=$((CURRENT_LIB+1))
    echo -e "${GREEN}[$CURRENT_LIB/$LIB_COUNT] Building ImPlot v${IMPLOT_VERSION}... (BUILD SEVENTH - PLOTTING)${NC}"
    cd "$BUILD_DIR"
    
    if [ -f "$LIB_OUTPUT_DIR/libimplot.a" ] && [ "$UPDATE_ALL" != true ]; then
        echo -e "${YELLOW}✓ libimplot.a already exists, skipping build${NC}"
        ls -lh "$LIB_OUTPUT_DIR/libimplot.a"
        LIBS_SKIPPED+=("ImPlot v${IMPLOT_VERSION}")
    else
        LIBS_BUILT+=("ImPlot v${IMPLOT_VERSION}")
        
        IMPLOT_DIR="implot-${IMPLOT_VERSION}"
        if [ "$UPDATE_ALL" = true ]; then
            rm -rf "$IMPLOT_DIR"
        fi
        if [ ! -d "$IMPLOT_DIR" ]; then
            echo -e "${YELLOW}Downloading ImPlot v${IMPLOT_VERSION}...${NC}"
            $DOWNLOAD_CMD implot.tar.gz "$IMPLOT_URL"
            tar -xzf implot.tar.gz
            rm implot.tar.gz
        else
            echo -e "${YELLOW}ImPlot v${IMPLOT_VERSION} source found, building...${NC}"
        fi
        
        cd "$IMPLOT_DIR"
        
        # Build ImPlot
        echo -e "${YELLOW}Building ImPlot from source...${NC}"
        g++ -c -std=c++17 $CFLAGS \
            -I. \
            -I"$HEADER_DIR/imgui" \
            implot.cpp \
            implot_items.cpp \
            implot_demo.cpp
        
        ar rcs libimplot.a implot.o implot_items.o implot_demo.o
        cp libimplot.a "$LIB_OUTPUT_DIR/"
        rm -f *.o
        
        # Copy headers
        if [ "$UPDATE_ALL" != true ] && grep -q "ImPlot: v${IMPLOT_VERSION}" "$VERSION_FILE" 2>/dev/null && [ -d "$HEADER_DIR/implot" ] && [ "$(ls -A $HEADER_DIR/implot 2>/dev/null)" ]; then
            echo -e "${YELLOW}✓ ImPlot v${IMPLOT_VERSION} headers already installed, skipping copy${NC}"
            HEADERS_SKIPPED+=("ImPlot v${IMPLOT_VERSION}")
        else
            echo -e "${YELLOW}Copying ImPlot v${IMPLOT_VERSION} headers...${NC}"
            HEADERS_COPIED+=("ImPlot v${IMPLOT_VERSION}")
            mkdir -p "$HEADER_DIR/implot"
            cp *.h "$HEADER_DIR/implot/" 2>/dev/null || true
            echo -e "${GREEN}✓ Headers copied${NC}"
        fi
        
        # Track version
        grep -q "ImPlot: v${IMPLOT_VERSION}" "$VERSION_FILE" 2>/dev/null || echo "ImPlot: v${IMPLOT_VERSION}" >> "$VERSION_FILE"
        
        echo -e "${GREEN}✓ ImPlot built successfully${NC}"
        ls -lh "$LIB_OUTPUT_DIR/libimplot.a"
        cd "$BUILD_DIR"
    fi
    echo ""
fi

if false; then  # DISABLED - duplicate of earlier PlutoVG build
# ==============================================================================
# Build PlutoVG (SVG vector rendering) - ALREADY BUILT EARLIER, SKIP DUPLICATE
# ==============================================================================
if [ "$BUILD_PLUTOVG" = true ]; then
    CURRENT_LIB=$((CURRENT_LIB+1))
    echo -e "${GREEN}[$CURRENT_LIB/$LIB_COUNT] Building plutovg v${PLUTOVG_VERSION}...${NC}"
if [ -f "$LIB_OUTPUT_DIR/libplutovg.a" ] && [ "$UPDATE_ALL" != true ]; then
    echo -e "${YELLOW}✓ libplutovg.a already exists, skipping build${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libplutovg.a"
        LIBS_SKIPPED+=("PlutoVG v${PLUTOVG_VERSION}")
    else
        LIBS_BUILT+=("PlutoVG v${PLUTOVG_VERSION}")
        PLUTOVG_DIR="plutovg-${PLUTOVG_VERSION}"
        if [ "$UPDATE_ALL" = true ]; then
            rm -rf "$PLUTOVG_DIR"
        fi
        if [ ! -d "$PLUTOVG_DIR" ]; then
            echo -e "${YELLOW}Downloading plutovg v${PLUTOVG_VERSION}...${NC}"
            $DOWNLOAD_CMD plutovg.tar.gz "$PLUTOVG_URL"
            tar -xzf plutovg.tar.gz
            rm plutovg.tar.gz
        else
            echo -e "${YELLOW}plutovg v${PLUTOVG_VERSION} source found, building...${NC}"
        fi
        cd "$PLUTOVG_DIR"
    rm -rf build
    mkdir build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DBUILD_SHARED_LIBS=OFF
    make -j$(nproc)
    cp libplutovg.a "$LIB_OUTPUT_DIR/"
        
        # Copy headers (check if already copied for this version)
        if [ "$UPDATE_ALL" != true ] && grep -q "PlutoVG: v${PLUTOVG_VERSION}" "$VERSION_FILE" 2>/dev/null && [ -d "$HEADER_DIR/plutovg" ] && [ "$(ls -A $HEADER_DIR/plutovg 2>/dev/null)" ]; then
            echo -e "${YELLOW}✓ PlutoVG v${PLUTOVG_VERSION} headers already installed, skipping copy${NC}"
            HEADERS_SKIPPED+=("PlutoVG v${PLUTOVG_VERSION}")
        else
            echo -e "${YELLOW}Copying plutovg v${PLUTOVG_VERSION} headers...${NC}"
            HEADERS_COPIED+=("PlutoVG v${PLUTOVG_VERSION}")
            cd ..
            mkdir -p "$HEADER_DIR/plutovg"
            cp -r include/* "$HEADER_DIR/plutovg/" 2>/dev/null || cp *.h "$HEADER_DIR/plutovg/" 2>/dev/null || true
            echo -e "${GREEN}✓ Headers copied${NC}"
        fi
        
        # Track version
        grep -q "PlutoVG: v${PLUTOVG_VERSION}" "$VERSION_FILE" 2>/dev/null || echo "PlutoVG: v${PLUTOVG_VERSION}" >> "$VERSION_FILE"
        
        echo -e "${GREEN}✓ plutovg built successfully (headers copied)${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libplutovg.a"
    cd "$BUILD_DIR"
fi
fi
echo ""
fi

if false; then  # DISABLED - duplicate of earlier LunaSVG build
# ==============================================================================
# Build LunaSVG (SVG parsing)
# ==============================================================================
if [ "$BUILD_LUNASVG" = true ]; then
    CURRENT_LIB=$((CURRENT_LIB+1))
    echo -e "${GREEN}[$CURRENT_LIB/$LIB_COUNT] Building lunasvg v${LUNASVG_VERSION}...${NC}"
cd "$BUILD_DIR"
if [ -f "$LIB_OUTPUT_DIR/liblunasvg.a" ] && [ "$UPDATE_ALL" != true ]; then
    echo -e "${YELLOW}✓ liblunasvg.a already exists, skipping build${NC}"
    ls -lh "$LIB_OUTPUT_DIR/liblunasvg.a"
        LIBS_SKIPPED+=("LunaSVG v${LUNASVG_VERSION}")
    else
        LIBS_BUILT+=("LunaSVG v${LUNASVG_VERSION}")
        LUNASVG_DIR="lunasvg-${LUNASVG_VERSION}"
        if [ "$UPDATE_ALL" = true ]; then
            rm -rf "$LUNASVG_DIR"
        fi
        if [ ! -d "$LUNASVG_DIR" ]; then
            echo -e "${YELLOW}Downloading lunasvg v${LUNASVG_VERSION}...${NC}"
            $DOWNLOAD_CMD lunasvg.tar.gz "$LUNASVG_URL"
            tar -xzf lunasvg.tar.gz
            rm lunasvg.tar.gz
        else
            echo -e "${YELLOW}lunasvg v${LUNASVG_VERSION} source found, building...${NC}"
        fi
        cd "$LUNASVG_DIR"
    rm -rf build
    mkdir build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DBUILD_SHARED_LIBS=OFF -DLUNASVG_BUILD_EXAMPLES=OFF
    make -j$(nproc)
    cp liblunasvg.a "$LIB_OUTPUT_DIR/"
        
        # Copy headers (check if already copied for this version)
        if [ "$UPDATE_ALL" != true ] && grep -q "LunaSVG: v${LUNASVG_VERSION}" "$VERSION_FILE" 2>/dev/null && [ -d "$HEADER_DIR/lunasvg" ] && [ "$(ls -A $HEADER_DIR/lunasvg 2>/dev/null)" ]; then
            echo -e "${YELLOW}✓ LunaSVG v${LUNASVG_VERSION} headers already installed, skipping copy${NC}"
            HEADERS_SKIPPED+=("LunaSVG v${LUNASVG_VERSION}")
        else
            echo -e "${YELLOW}Copying lunasvg v${LUNASVG_VERSION} headers...${NC}"
            HEADERS_COPIED+=("LunaSVG v${LUNASVG_VERSION}")
            cd ..
            mkdir -p "$HEADER_DIR/lunasvg"
            cp -r include/* "$HEADER_DIR/lunasvg/" 2>/dev/null || cp *.h "$HEADER_DIR/lunasvg/" 2>/dev/null || true
            echo -e "${GREEN}✓ Headers copied${NC}"
        fi
        
        # Track version
        grep -q "LunaSVG: v${LUNASVG_VERSION}" "$VERSION_FILE" 2>/dev/null || echo "LunaSVG: v${LUNASVG_VERSION}" >> "$VERSION_FILE"
        
        echo -e "${GREEN}✓ lunasvg built successfully (headers copied)${NC}"
    ls -lh "$LIB_OUTPUT_DIR/liblunasvg.a"
    cd "$BUILD_DIR"
fi
fi
echo ""
fi

if false; then  # DISABLED - duplicate of earlier SDL3 build
if [ "$BUILD_SDL3" = true ]; then
    CURRENT_LIB=$((CURRENT_LIB+1))
    echo -e "${GREEN}[$CURRENT_LIB/$LIB_COUNT] Building SDL3 v${SDL3_VERSION} (this may take a while)...${NC}"
cd "$BUILD_DIR"
if [ -f "$LIB_OUTPUT_DIR/libSDL3.a" ] && [ "$UPDATE_ALL" != true ]; then
    echo -e "${YELLOW}✓ libSDL3.a already exists, skipping build${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libSDL3.a"
        LIBS_SKIPPED+=("SDL3 v${SDL3_VERSION}")
    else
        LIBS_BUILT+=("SDL3 v${SDL3_VERSION}")
        SDL3_DIR="SDL-release-${SDL3_VERSION}"
        if [ "$UPDATE_ALL" = true ]; then
            rm -rf "$SDL3_DIR"
        fi
        if [ ! -d "$SDL3_DIR" ]; then
            echo -e "${YELLOW}Downloading SDL3 v${SDL3_VERSION}...${NC}"
            $DOWNLOAD_CMD SDL3.tar.gz "$SDL3_URL"
            tar -xzf SDL3.tar.gz
            rm SDL3.tar.gz
        else
            echo -e "${YELLOW}SDL3 v${SDL3_VERSION} source found, building...${NC}"
        fi
        cd "$SDL3_DIR"
    rm -rf build
    mkdir build && cd build

    if cmake .. \
        -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
        -DBUILD_SHARED_LIBS=OFF \
        -DSDL_STATIC=ON \
        -DSDL_SHARED=OFF \
        -DSDL_TEST=OFF \
        -DSDL_X11_XCURSOR=OFF \
        -DSDL_X11_XINERAMA=OFF \
        -DSDL_X11_XRANDR=ON \
        -DSDL_X11_XSCRNSAVER=OFF \
        -DSDL_X11_XSHAPE=OFF \
        -DSDL_X11_XTEST=OFF && \
       make -j$(nproc) && \
       cp libSDL3.a "$LIB_OUTPUT_DIR/"; then
            
            # Copy headers (check if already copied for this version)
            if [ "$UPDATE_ALL" != true ] && grep -q "SDL3: v${SDL3_VERSION}" "$VERSION_FILE" 2>/dev/null && [ -d "$HEADER_DIR/SDL3" ] && [ "$(ls -A $HEADER_DIR/SDL3 2>/dev/null)" ]; then
                echo -e "${YELLOW}✓ SDL3 v${SDL3_VERSION} headers already installed, skipping copy${NC}"
                HEADERS_SKIPPED+=("SDL3 v${SDL3_VERSION}")
            else
                echo -e "${YELLOW}Copying SDL3 v${SDL3_VERSION} headers...${NC}"
                HEADERS_COPIED+=("SDL3 v${SDL3_VERSION}")
                cd ..
                mkdir -p "$HEADER_DIR/SDL3"
                cp -r include/* "$HEADER_DIR/SDL3/" 2>/dev/null || true
                echo -e "${GREEN}✓ Headers copied${NC}"
            fi
            
            # Track version
            grep -q "SDL3: v${SDL3_VERSION}" "$VERSION_FILE" 2>/dev/null || echo "SDL3: v${SDL3_VERSION}" >> "$VERSION_FILE"
            
            echo -e "${GREEN}✓ SDL3 built successfully (headers copied)${NC}"
        ls -lh "$LIB_OUTPUT_DIR/libSDL3.a"
    else
            echo -e "${RED}SDL3 build failed!${NC}"
            echo -e "${YELLOW}Install missing X11 packages:${NC}"
            echo -e "${YELLOW}  libXcursor-devel libXrandr-devel libXScrnSaver-devel${NC}"
            exit 1
    fi
    cd "$BUILD_DIR"
    fi
    echo ""
fi
fi

if false; then  # DISABLED - duplicate of earlier GLFW build
# ==============================================================================
# Build GLFW3 (windowing library)
# ==============================================================================
if [ "$BUILD_GLFW" = true ]; then
    CURRENT_LIB=$((CURRENT_LIB+1))
    echo -e "${GREEN}[$CURRENT_LIB/$LIB_COUNT] Building GLFW v${GLFW_VERSION}...${NC}"
    cd "$BUILD_DIR"
if [ -f "$LIB_OUTPUT_DIR/libglfw3.a" ] && [ "$UPDATE_ALL" != true ]; then
        echo -e "${YELLOW}✓ libglfw3.a already exists, skipping build${NC}"
        ls -lh "$LIB_OUTPUT_DIR/libglfw3.a"
        LIBS_SKIPPED+=("GLFW v${GLFW_VERSION}")
    else
        LIBS_BUILT+=("GLFW v${GLFW_VERSION}")
        GLFW_DIR="glfw-${GLFW_VERSION}"
        if [ "$UPDATE_ALL" = true ]; then
            rm -rf "$GLFW_DIR"
        fi
        if [ ! -d "$GLFW_DIR" ]; then
            echo -e "${YELLOW}Downloading GLFW v${GLFW_VERSION}...${NC}"
            $DOWNLOAD_CMD glfw.tar.gz "$GLFW_URL"
            tar -xzf glfw.tar.gz
            rm glfw.tar.gz
        else
            echo -e "${YELLOW}GLFW v${GLFW_VERSION} source found, building...${NC}"
        fi
        cd "$GLFW_DIR"
        rm -rf build
        mkdir build && cd build
        cmake .. \
            -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
            -DBUILD_SHARED_LIBS=OFF \
            -DGLFW_BUILD_EXAMPLES=OFF \
            -DGLFW_BUILD_TESTS=OFF \
            -DGLFW_BUILD_DOCS=OFF \
            -DGLFW_BUILD_WAYLAND=OFF
        make -j$(nproc)
        cp src/libglfw3.a "$LIB_OUTPUT_DIR/"
        
        # Copy headers (check if already copied for this version)
        if [ "$UPDATE_ALL" != true ] && grep -q "GLFW: v${GLFW_VERSION}" "$VERSION_FILE" 2>/dev/null && [ -d "$HEADER_DIR/GLFW" ] && [ "$(ls -A $HEADER_DIR/GLFW 2>/dev/null)" ]; then
            echo -e "${YELLOW}✓ GLFW v${GLFW_VERSION} headers already installed, skipping copy${NC}"
            HEADERS_SKIPPED+=("GLFW v${GLFW_VERSION}")
        else
            echo -e "${YELLOW}Copying GLFW v${GLFW_VERSION} headers...${NC}"
            HEADERS_COPIED+=("GLFW v${GLFW_VERSION}")
            cd ..
            mkdir -p "$HEADER_DIR/GLFW"
            cp -r include/GLFW/* "$HEADER_DIR/GLFW/" 2>/dev/null || true
            echo -e "${GREEN}✓ Headers copied${NC}"
        fi
        
        # Track version
        grep -q "GLFW: v${GLFW_VERSION}" "$VERSION_FILE" 2>/dev/null || echo "GLFW: v${GLFW_VERSION}" >> "$VERSION_FILE"
        
        echo -e "${GREEN}✓ GLFW3 built successfully (headers copied)${NC}"
        ls -lh "$LIB_OUTPUT_DIR/libglfw3.a"
        cd "$BUILD_DIR"
    fi
echo ""
fi
fi

# ==============================================================================
# DUPLICATE SECTION DISABLED - FreeType now built earlier (BUILD FIRST)
# ==============================================================================
if false; then  # DISABLED - duplicate of earlier FreeType build
    CURRENT_LIB=$((CURRENT_LIB+1))
    echo -e "${GREEN}[$CURRENT_LIB/$LIB_COUNT] Building freetype v${FREETYPE_VERSION}...${NC}"
cd "$BUILD_DIR"
if [ -f "$LIB_OUTPUT_DIR/libfreetype.a" ]; then
    echo -e "${YELLOW}✓ libfreetype.a already exists, skipping build${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libfreetype.a"
        LIBS_SKIPPED+=("FreeType v${FREETYPE_VERSION}")
    else
        LIBS_BUILT+=("FreeType v${FREETYPE_VERSION}")
        FREETYPE_DIR="freetype-VER-${FREETYPE_VERSION//./-}"
        if [ ! -d "$FREETYPE_DIR" ]; then
            echo -e "${YELLOW}Downloading freetype v${FREETYPE_VERSION}...${NC}"
            $DOWNLOAD_CMD freetype.tar.gz "$FREETYPE_URL"
            tar -xzf freetype.tar.gz
            rm freetype.tar.gz
        else
            echo -e "${YELLOW}freetype v${FREETYPE_VERSION} source found, building...${NC}"
        fi
        cd "$FREETYPE_DIR"
    rm -rf build
    mkdir build && cd build
    cmake .. \
        -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
        -DBUILD_SHARED_LIBS=OFF \
        -DFT_DISABLE_ZLIB=ON \
        -DFT_DISABLE_BZIP2=ON \
        -DFT_DISABLE_PNG=OFF \
        -DFT_DISABLE_HARFBUZZ=ON \
        -DFT_DISABLE_BROTLI=ON
    make -j$(nproc)
    if [ "$BUILD_TYPE" = "Debug" ]; then
        cp libfreetyped.a "$LIB_OUTPUT_DIR/libfreetype.a"
    else
        cp libfreetype.a "$LIB_OUTPUT_DIR/"
    fi
        
        # Copy headers (check if already copied for this version)
        if grep -q "FreeType: v${FREETYPE_VERSION}" "$VERSION_FILE" 2>/dev/null && [ -d "$HEADER_DIR/freetype2" ] && [ "$(ls -A $HEADER_DIR/freetype2 2>/dev/null)" ]; then
            echo -e "${YELLOW}✓ FreeType v${FREETYPE_VERSION} headers already installed, skipping copy${NC}"
            HEADERS_SKIPPED+=("FreeType v${FREETYPE_VERSION}")
        else
            echo -e "${YELLOW}Copying freetype v${FREETYPE_VERSION} headers...${NC}"
            HEADERS_COPIED+=("FreeType v${FREETYPE_VERSION}")
            make install DESTDIR="$BUILD_DIR/freetype-install" > /dev/null 2>&1 || true
            mkdir -p "$HEADER_DIR/freetype2"
            cp -r "$BUILD_DIR/freetype-install"/usr/local/include/freetype2/* "$HEADER_DIR/freetype2/" 2>/dev/null || \
            cp -r ../include/* "$HEADER_DIR/freetype2/" 2>/dev/null || true
            echo -e "${GREEN}✓ Headers copied${NC}"
        fi
        
        # Track version
        grep -q "FreeType: v${FREETYPE_VERSION}" "$VERSION_FILE" 2>/dev/null || echo "FreeType: v${FREETYPE_VERSION}" >> "$VERSION_FILE"
        
        echo -e "${GREEN}✓ freetype built successfully (headers copied)${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libfreetype.a"
    cd "$BUILD_DIR"
fi
echo ""
fi

# ==============================================================================
# DUPLICATE SECTION DISABLED - ImGui now built earlier from downloaded tarball
# ==============================================================================
if false; then  # DISABLED - duplicate of earlier ImGui build
    CURRENT_LIB=$((CURRENT_LIB+1))
    echo -e "${GREEN}[$CURRENT_LIB/$LIB_COUNT] Building ImGui...${NC}"
if [ -f "$LIB_OUTPUT_DIR/libimgui.a" ]; then
    echo -e "${YELLOW}✓ libimgui.a already exists, skipping build${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libimgui.a"
        LIBS_SKIPPED+=("ImGui (bundled)")
else
        LIBS_BUILT+=("ImGui (bundled)")
    echo -e "${YELLOW}Building ImGui from source...${NC}"
    cd "$PROJECT_ROOT/src/libs/src"
        
        # Build with FreeType support
    g++ -c -O3 -std=c++17 -fPIC \
        -I../inc \
        -I../inc/imgui \
            -I../inc/freetype2 \
            -DIMGUI_ENABLE_FREETYPE \
        imgui.cpp \
        imgui_demo.cpp \
        imgui_draw.cpp \
        imgui_tables.cpp \
            imgui_widgets.cpp \
            ../inc/imgui/misc/freetype/imgui_freetype.cpp
        
        ar rcs libimgui.a imgui.o imgui_demo.o imgui_draw.o imgui_tables.o imgui_widgets.o imgui_freetype.o
    cp libimgui.a "$LIB_OUTPUT_DIR/"
    rm -f *.o
        
        # Headers already in src/libs/inc/imgui, just track version
        IMGUI_VERSION=$(grep "IMGUI_VERSION " ../inc/imgui/imgui.h | head -1 | awk '{print $3}' | tr -d '"')
        grep -q "ImGui: ${IMGUI_VERSION:-bundled}" "$VERSION_FILE" 2>/dev/null || echo "ImGui: ${IMGUI_VERSION:-bundled}" >> "$VERSION_FILE"
        
    echo -e "${GREEN}✓ ImGui built successfully${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libimgui.a"
fi
echo ""
fi

# ==============================================================================
# DUPLICATE SECTION DISABLED - ImPlot now built earlier from downloaded tarball
# ==============================================================================
if false; then  # DISABLED - duplicate of earlier ImPlot build
    CURRENT_LIB=$((CURRENT_LIB+1))
    echo -e "${GREEN}[$CURRENT_LIB/$LIB_COUNT] Building ImPlot...${NC}"
if [ -f "$LIB_OUTPUT_DIR/libimplot.a" ]; then
    echo -e "${YELLOW}✓ libimplot.a already exists, skipping build${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libimplot.a"
        LIBS_SKIPPED+=("ImPlot (bundled)")
else
        LIBS_BUILT+=("ImPlot (bundled)")
    echo -e "${YELLOW}Building ImPlot from source...${NC}"
    cd "$PROJECT_ROOT/src/libs/inc/implot"
    g++ -c -O3 -std=c++17 -fPIC \
        -I../../inc \
        -I../../inc/imgui \
        implot.cpp \
        implot_items.cpp
    ar rcs libimplot.a implot.o implot_items.o
    cp libimplot.a "$LIB_OUTPUT_DIR/"
    rm -f *.o
        
        # Headers already in src/libs/inc/implot, just track version
        IMPLOT_VERSION=$(grep "IMPLOT_VERSION " implot.h | head -1 | awk '{print $3}' | tr -d '"')
        grep -q "ImPlot: ${IMPLOT_VERSION:-bundled}" "$VERSION_FILE" 2>/dev/null || echo "ImPlot: ${IMPLOT_VERSION:-bundled}" >> "$VERSION_FILE"
        
    echo -e "${GREEN}✓ ImPlot built successfully${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libimplot.a"
fi
echo ""
fi

# ==============================================================================
# DUPLICATE SECTION DISABLED - Yoga now built earlier from downloaded tarball
# ==============================================================================
if false; then  # DISABLED - duplicate of earlier Yoga build
    CURRENT_LIB=$((CURRENT_LIB+1))
    echo -e "${GREEN}[$CURRENT_LIB/$LIB_COUNT] Building Yoga...${NC}"
if [ -f "$LIB_OUTPUT_DIR/libyoga.a" ]; then
    echo -e "${YELLOW}✓ libyoga.a already exists, skipping build${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libyoga.a"
        LIBS_SKIPPED+=("Yoga (bundled)")
else
        LIBS_BUILT+=("Yoga (bundled)")
    echo -e "${YELLOW}Building Yoga from source...${NC}"
    cd "$PROJECT_ROOT/src/libs/inc/yoga"
    rm -f *.o *.a
        
        # Build all Yoga source files (C++20 required)
        find . -name "*.cpp" -exec g++ -c -O3 -std=c++20 -fPIC -I.. -I. {} \;
        find . -name "*.o" -exec ar rcs libyoga.a {} +
        
    cp libyoga.a "$LIB_OUTPUT_DIR/"
    rm -f *.o
        find . -name "*.o" -delete
        
        # Headers already in src/libs/inc/yoga, just track version
        YOGA_VERSION=$(grep "constexpr int32_t YGVersion" Yoga.h 2>/dev/null | awk '{print $5}' | tr -d ';' || echo "bundled")
        grep -q "Yoga: ${YOGA_VERSION}" "$VERSION_FILE" 2>/dev/null || echo "Yoga: ${YOGA_VERSION}" >> "$VERSION_FILE"
        
    echo -e "${GREEN}✓ Yoga built successfully${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libyoga.a"
fi
echo ""
fi

# ==============================================================================
# Build Blend2D (2D rendering engine)
# ==============================================================================
if [ "$BUILD_BLEND2D" = true ]; then
    CURRENT_LIB=$((CURRENT_LIB+1))
    echo -e "${GREEN}[$CURRENT_LIB/$LIB_COUNT] Building blend2d v${BLEND2D_VERSION} (this may take a while)...${NC}"
cd "$BUILD_DIR"
if [ -f "$LIB_OUTPUT_DIR/libblend2d.a" ] && [ "$UPDATE_ALL" != true ]; then
    echo -e "${YELLOW}✓ libblend2d.a already exists, skipping build${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libblend2d.a"
        LIBS_SKIPPED+=("Blend2D v${BLEND2D_VERSION}")
        if [ ! -d "$HEADER_DIR/blend2d" ] || [ -z "$(ls -A "$HEADER_DIR/blend2d" 2>/dev/null)" ]; then
            echo -e "${YELLOW}Blend2D headers missing, restoring...${NC}"
            BLEND2D_DIR="blend2d-${BLEND2D_VERSION}"
            if [ ! -d "$BLEND2D_DIR" ]; then
                echo -e "${YELLOW}Downloading blend2d v${BLEND2D_VERSION} for headers...${NC}"
                $DOWNLOAD_CMD blend2d.tar.gz "$BLEND2D_URL"
                tar -xzf blend2d.tar.gz
                if [ -d "blend2d" ]; then
                    mv blend2d "$BLEND2D_DIR"
                fi
                rm blend2d.tar.gz
            fi
            mkdir -p "$HEADER_DIR/blend2d"
            if [ -d "$BLEND2D_DIR/blend2d" ]; then
                cp -r "$BLEND2D_DIR/blend2d/blend2d.h" "$HEADER_DIR/blend2d/" 2>/dev/null || true
                cp -r "$BLEND2D_DIR/blend2d" "$HEADER_DIR/" 2>/dev/null || true
            else
                cp -r "$BLEND2D_DIR/src/blend2d.h" "$HEADER_DIR/blend2d/" 2>/dev/null || true
                cp -r "$BLEND2D_DIR/src/blend2d" "$HEADER_DIR/" 2>/dev/null || true
            fi
        fi
    else
        LIBS_BUILT+=("Blend2D v${BLEND2D_VERSION}")
        BLEND2D_DIR="blend2d-${BLEND2D_VERSION}"
        if [ "$UPDATE_ALL" = true ]; then
            rm -rf "$BLEND2D_DIR"
        fi
        if [ ! -d "$BLEND2D_DIR" ]; then
            echo -e "${YELLOW}Downloading blend2d v${BLEND2D_VERSION}...${NC}"
            $DOWNLOAD_CMD blend2d.tar.gz "$BLEND2D_URL"
            # Blend2D tarball extracts flat, so extract then rename
            tar -xzf blend2d.tar.gz
            # The tarball may extract to 'blend2d' or current directory
            if [ -d "blend2d" ]; then
                mv blend2d "$BLEND2D_DIR"
            fi
            rm blend2d.tar.gz
        else
            echo -e "${YELLOW}blend2d v${BLEND2D_VERSION} source found${NC}"
        fi
        
        # Build Blend2D (CMake will fetch AsmJit automatically)
        cd "$BLEND2D_DIR"
        rm -rf build
    mkdir build && cd build
        echo -e "${YELLOW}Building blend2d...${NC}"
    cmake .. \
        -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
        -DBUILD_SHARED_LIBS=OFF \
        -DBLEND2D_STATIC=ON \
        -DBLEND2D_TEST=OFF \
        -DBL_BUILD_OPT_AVX512=ON \
        -DASMJIT_NO_FOREIGN=ON \
        -DASMJIT_NO_STDCXX=ON \
        -DASMJIT_ABI_NAMESPACE=abi_bl \
        -DASMJIT_STATIC=ON \
        -DASMJIT_NO_AARCH64=ON
    make -j$(nproc)
    cp libblend2d.a "$LIB_OUTPUT_DIR/"
        
        # Copy headers (check if already copied for this version)
        if [ "$UPDATE_ALL" != true ] && grep -q "Blend2D: v${BLEND2D_VERSION}" "$VERSION_FILE" 2>/dev/null && [ -d "$HEADER_DIR/blend2d" ] && [ "$(ls -A $HEADER_DIR/blend2d 2>/dev/null)" ]; then
            echo -e "${YELLOW}✓ Blend2D v${BLEND2D_VERSION} headers already installed, skipping copy${NC}"
            HEADERS_SKIPPED+=("Blend2D v${BLEND2D_VERSION}")
        else
            echo -e "${YELLOW}Copying blend2d v${BLEND2D_VERSION} headers...${NC}"
            HEADERS_COPIED+=("Blend2D v${BLEND2D_VERSION}")
            cd ..
            mkdir -p "$HEADER_DIR/blend2d"
            if [ -d "blend2d" ]; then
                cp -r blend2d/blend2d.h "$HEADER_DIR/blend2d/" 2>/dev/null || true
                cp -r blend2d "$HEADER_DIR/" 2>/dev/null || true
            else
                cp -r src/blend2d.h "$HEADER_DIR/blend2d/" 2>/dev/null || true
                cp -r src/blend2d "$HEADER_DIR/" 2>/dev/null || true
            fi
            echo -e "${GREEN}✓ Headers copied${NC}"
        fi
        
        # Track version
        grep -q "Blend2D: v${BLEND2D_VERSION}" "$VERSION_FILE" 2>/dev/null || echo "Blend2D: v${BLEND2D_VERSION}" >> "$VERSION_FILE"
        grep -q "AsmJit: ${ASMJIT_VERSION}" "$VERSION_FILE" 2>/dev/null || echo "AsmJit: ${ASMJIT_VERSION}" >> "$VERSION_FILE"
        
        echo -e "${GREEN}✓ blend2d built successfully (headers copied)${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libblend2d.a"
    cd "$BUILD_DIR"
    fi
    echo ""
fi

# ==============================================================================
# Build NFD-Extended (native file dialogs)
# ==============================================================================
if [ "$BUILD_NFDEXT" = true ]; then
    CURRENT_LIB=$((CURRENT_LIB+1))
    echo -e "${GREEN}[$CURRENT_LIB/$LIB_COUNT] Building nfd-extended ${NFD_VERSION}...${NC}"
    cd "$BUILD_DIR"
    if [ -f "$LIB_OUTPUT_DIR/libnfd.a" ] && [ "$UPDATE_ALL" != true ]; then
        echo -e "${YELLOW}✓ libnfd.a already exists, skipping build${NC}"
        ls -lh "$LIB_OUTPUT_DIR/libnfd.a"
        LIBS_SKIPPED+=("NFD-extended ${NFD_VERSION}")
    else
        LIBS_BUILT+=("NFD-extended ${NFD_VERSION}")
        NFD_DIR="nativefiledialog-extended-${NFD_VERSION#v}"
        if [ "$UPDATE_ALL" = true ]; then
            rm -rf "$NFD_DIR"
        fi
        if [ ! -d "$NFD_DIR" ]; then
            echo -e "${YELLOW}Downloading nfd-extended ${NFD_VERSION}...${NC}"
            $DOWNLOAD_CMD nfd.tar.gz "$NFD_URL"
            tar -xzf nfd.tar.gz
            rm nfd.tar.gz
        else
            echo -e "${YELLOW}nfd-extended ${NFD_VERSION} source found, building...${NC}"
        fi
        cd "$NFD_DIR"
        rm -rf build
        mkdir build && cd build
        cmake .. \
            -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
            -DBUILD_SHARED_LIBS=OFF \
            -DNFD_BUILD_TESTS=OFF
        make -j$(nproc)
        cp src/libnfd.a "$LIB_OUTPUT_DIR/"
        
        # Copy headers (check if already copied for this version)
        if [ "$UPDATE_ALL" != true ] && grep -q "NFD-extended: ${NFD_VERSION}" "$VERSION_FILE" 2>/dev/null && [ -d "$HEADER_DIR/nfd-ext" ] && [ "$(ls -A $HEADER_DIR/nfd-ext 2>/dev/null)" ]; then
            echo -e "${YELLOW}✓ NFD-extended ${NFD_VERSION} headers already installed, skipping copy${NC}"
            HEADERS_SKIPPED+=("NFD-extended ${NFD_VERSION}")
        else
            echo -e "${YELLOW}Copying nfd-extended ${NFD_VERSION} headers...${NC}"
            HEADERS_COPIED+=("NFD-extended ${NFD_VERSION}")
            cd ..
            mkdir -p "$HEADER_DIR/nfd-ext"
            cp -r src/include/* "$HEADER_DIR/nfd-ext/" 2>/dev/null || true
            echo -e "${GREEN}✓ Headers copied${NC}"
        fi
        
        # Track version
        grep -q "NFD-extended: ${NFD_VERSION}" "$VERSION_FILE" 2>/dev/null || echo "NFD-extended: ${NFD_VERSION}" >> "$VERSION_FILE"
        
        echo -e "${GREEN}✓ nfd-extended built successfully (headers copied)${NC}"
        ls -lh "$LIB_OUTPUT_DIR/libnfd.a"
        cd "$BUILD_DIR"
    fi
    echo ""
fi

# ==============================================================================
# Build PDCurses (terminal UI)
# ==============================================================================
if [ "$BUILD_PDCURSES" = true ]; then
    CURRENT_LIB=$((CURRENT_LIB+1))
    echo -e "${GREEN}[$CURRENT_LIB/$LIB_COUNT] Building PDCurses v${PDCURSES_VERSION}...${NC}"
    cd "$BUILD_DIR"
    if [ -f "$LIB_OUTPUT_DIR/libpdcurses.a" ] && [ "$UPDATE_ALL" != true ]; then
        echo -e "${YELLOW}✓ libpdcurses.a already exists, skipping build${NC}"
        ls -lh "$LIB_OUTPUT_DIR/libpdcurses.a"
        LIBS_SKIPPED+=("PDCurses v${PDCURSES_VERSION}")
    else
        LIBS_BUILT+=("PDCurses v${PDCURSES_VERSION}")
        PDCURSES_DIR="PDCurses-${PDCURSES_VERSION}"
        if [ "$UPDATE_ALL" = true ]; then
            rm -rf "$PDCURSES_DIR"
        fi
        if [ ! -d "$PDCURSES_DIR" ]; then
            echo -e "${YELLOW}Downloading PDCurses v${PDCURSES_VERSION}...${NC}"
            $DOWNLOAD_CMD pdcurses.tar.gz "$PDCURSES_URL"
            tar -xzf pdcurses.tar.gz
            rm pdcurses.tar.gz
        else
            echo -e "${YELLOW}PDCurses v${PDCURSES_VERSION} source found, building...${NC}"
        fi
        cd "$PDCURSES_DIR"
        
        # Build X11 variant for Linux
        cd x11
        make clean || true
        make -j$(nproc)
        cp libXCurses.a "$LIB_OUTPUT_DIR/libpdcurses.a"
        
        # Copy headers (check if already copied for this version)
        if [ "$UPDATE_ALL" != true ] && grep -q "PDCurses: v${PDCURSES_VERSION}" "$VERSION_FILE" 2>/dev/null && [ -f "$HEADER_DIR/pdcurses/curses.h" ]; then
            echo -e "${YELLOW}✓ PDCurses v${PDCURSES_VERSION} headers already installed, skipping copy${NC}"
            HEADERS_SKIPPED+=("PDCurses v${PDCURSES_VERSION}")
        else
            echo -e "${YELLOW}Copying PDCurses v${PDCURSES_VERSION} headers...${NC}"
            HEADERS_COPIED+=("PDCurses v${PDCURSES_VERSION}")
            mkdir -p "$HEADER_DIR/pdcurses"
            cp curses.h "$HEADER_DIR/pdcurses/" 2>/dev/null || true
            cp panel.h "$HEADER_DIR/pdcurses/" 2>/dev/null || true
            echo -e "${GREEN}✓ Headers copied${NC}"
        fi
        
        # Track version
        grep -q "PDCurses: v${PDCURSES_VERSION}" "$VERSION_FILE" 2>/dev/null || echo "PDCurses: v${PDCURSES_VERSION}" >> "$VERSION_FILE"
        
        echo -e "${GREEN}✓ PDCurses built successfully (headers copied)${NC}"
        ls -lh "$LIB_OUTPUT_DIR/libpdcurses.a"
        cd "$BUILD_DIR"
    fi
    echo ""
fi

# ==============================================================================
# Setup nlohmann-json (header-only library)
# ==============================================================================
if [ "$BUILD_JSON" = true ]; then
    CURRENT_LIB=$((CURRENT_LIB+1))
    echo -e "${GREEN}[$CURRENT_LIB/$LIB_COUNT] Setting up nlohmann-json v${JSON_VERSION} (header-only)...${NC}"
    cd "$BUILD_DIR"
    
    JSON_HEADER_DIR="$PROJECT_ROOT/src/libs/inc/nlohmann"
    if [ "$UPDATE_ALL" != true ] && grep -q "nlohmann-json: v${JSON_VERSION}" "$VERSION_FILE" 2>/dev/null && [ -f "$JSON_HEADER_DIR/json.hpp" ]; then
        echo -e "${YELLOW}✓ nlohmann-json v${JSON_VERSION} already installed, skipping download${NC}"
        HEADERS_SKIPPED+=("nlohmann-json v${JSON_VERSION} (header-only)")
    else
        echo -e "${YELLOW}Downloading nlohmann-json v${JSON_VERSION}...${NC}"
        HEADERS_COPIED+=("nlohmann-json v${JSON_VERSION} (header-only)")
        if [ "$UPDATE_ALL" = true ]; then
            rm -rf "json-${JSON_VERSION}" json.tar.gz
        fi
        $DOWNLOAD_CMD json.tar.gz "$JSON_URL"
        tar -xzf json.tar.gz
        mkdir -p "$JSON_HEADER_DIR"
        cp json-${JSON_VERSION}/single_include/nlohmann/json.hpp "$JSON_HEADER_DIR/"
        echo -e "${GREEN}✓ nlohmann-json header extracted and copied${NC}"
    fi
    
    # Track version
    grep -q "nlohmann-json: v${JSON_VERSION}" "$VERSION_FILE" 2>/dev/null || echo "nlohmann-json: v${JSON_VERSION} (header-only)" >> "$VERSION_FILE"
    echo ""
fi

# ==============================================================================
# Copy stb_image headers (header-only library)
# ==============================================================================
if [ "$COPY_STB_IMAGE" = true ]; then
    CURRENT_LIB=$((CURRENT_LIB+1))
    echo -e "${GREEN}[$CURRENT_LIB/$LIB_COUNT] Copying stb_image headers...${NC}"
    
    STB_HEADER_DIR="$PROJECT_ROOT/src/libs/inc/stb_image"
    
    # Check if already installed (stb_image doesn't have release versions, so just check if exists)
    if [ "$UPDATE_ALL" != true ] && [ -f "$STB_HEADER_DIR/stb_image.h" ] && grep -q "stb_image:" "$VERSION_FILE" 2>/dev/null; then
        STB_VERSION=$(grep "stb_image.*v" "$STB_HEADER_DIR/stb_image.h" | head -1 | grep -oP 'v[\d.]+' || echo "latest")
        echo -e "${YELLOW}✓ stb_image ${STB_VERSION} already installed, skipping download${NC}"
        HEADERS_SKIPPED+=("stb_image (header-only)")
    else
        HEADERS_COPIED+=("stb_image (header-only)")
        mkdir -p "$STB_HEADER_DIR"
        echo -e "${YELLOW}Downloading stb_image.h (latest)...${NC}"
        $DOWNLOAD_CMD "$STB_HEADER_DIR/stb_image.h" "$STB_IMAGE_URL"
        echo -e "${GREEN}✓ stb_image.h downloaded${NC}"
    fi
    
    # Try to extract version from header and track it
    STB_VERSION=$(grep "stb_image.*v" "$STB_HEADER_DIR/stb_image.h" | head -1 | grep -oP 'v[\d.]+' || echo "latest")
    grep -q "stb_image:" "$VERSION_FILE" 2>/dev/null || echo "stb_image: ${STB_VERSION} (header-only)" >> "$VERSION_FILE"
    echo ""
fi

# ==============================================================================
# Copy FontAwesome headers (header-only library)
# ==============================================================================
if [ "$COPY_FONTAWESOME" = true ]; then
    CURRENT_LIB=$((CURRENT_LIB+1))
    echo -e "${GREEN}[$CURRENT_LIB/$LIB_COUNT] Copying FontAwesome C++ headers...${NC}"
    
    ICON_HEADER_DIR="$PROJECT_ROOT/src/libs/inc/iconfonts"
    
    # Check if already installed (FontAwesome doesn't have release versions, so just check if exists)
    if [ "$UPDATE_ALL" != true ] && [ -f "$ICON_HEADER_DIR/IconsFontAwesome6.h" ] && grep -q "IconFontCppHeaders:" "$VERSION_FILE" 2>/dev/null; then
        FA_VERSION=$(grep "Font Awesome" "$ICON_HEADER_DIR/IconsFontAwesome6.h" | grep -oP '[\d.]+' | head -1 || echo "6.x")
        echo -e "${YELLOW}✓ IconFontCppHeaders (FontAwesome ${FA_VERSION}) already installed, skipping download${NC}"
        HEADERS_SKIPPED+=("IconFontCppHeaders (header-only)")
    else
        HEADERS_COPIED+=("IconFontCppHeaders (header-only)")
        mkdir -p "$ICON_HEADER_DIR"
        cd "$BUILD_DIR"
        
        # Download IconFontCppHeaders (no releases, use specific commit or latest)
        if [ "$UPDATE_ALL" = true ]; then
            rm -rf "IconFontCppHeaders" "IconFontCppHeaders-main" "IconFontCppHeaders.zip"
        fi
        if [ ! -d "IconFontCppHeaders" ]; then
            echo -e "${YELLOW}Downloading IconFontCppHeaders (latest)...${NC}"
            $DOWNLOAD_CMD IconFontCppHeaders.zip "$ICONFONT_URL"
            unzip -q IconFontCppHeaders.zip
            mv IconFontCppHeaders-main IconFontCppHeaders
            rm IconFontCppHeaders.zip
        fi
        
        # Copy FontAwesome headers
        cp IconFontCppHeaders/IconsFontAwesome6.h "$ICON_HEADER_DIR/"
        cp IconFontCppHeaders/IconsFontAwesome6Brands.h "$ICON_HEADER_DIR/" 2>/dev/null || true
        
        echo -e "${GREEN}✓ FontAwesome headers copied${NC}"
    fi
    
    # Try to extract FontAwesome version from header and track it
    FA_VERSION=$(grep "Font Awesome" "$ICON_HEADER_DIR/IconsFontAwesome6.h" | grep -oP '[\d.]+' | head -1 || echo "6.x")
    grep -q "IconFontCppHeaders:" "$VERSION_FILE" 2>/dev/null || echo "IconFontCppHeaders: latest (FontAwesome ${FA_VERSION}, header-only)" >> "$VERSION_FILE"
    echo ""
fi

# ==============================================================================
# Summary
# ==============================================================================
# Finalize version file
cat >> "$VERSION_FILE" << EOF

================================================================================
Header Locations:
================================================================================
All headers installed in: $HEADER_DIR/
Version tracking: $VERSION_FILE

To see installed versions: cat $VERSION_FILE
================================================================================
EOF

echo ""
echo -e "${BLUE}================================================================================${NC}"
echo -e "${BLUE}                         BUILD SUMMARY - DEPENDENCIES                          ${NC}"
echo -e "${BLUE}================================================================================${NC}"
echo ""
echo -e "${GREEN}✓ All dependencies processed successfully!${NC}"
echo ""
echo -e "${YELLOW}Platform:${NC}        $PLATFORM"
echo -e "${YELLOW}Build Type:${NC}      $BUILD_TYPE"
echo -e "${YELLOW}Output Path:${NC}     $LIB_OUTPUT_DIR"
echo -e "${YELLOW}Headers Path:${NC}    $HEADER_OUTPUT_DIR"
echo -e "${YELLOW}Version File:${NC}    $VERSION_FILE"
echo ""

# Show what was built vs what was skipped
echo -e "${BLUE}================================================================================${NC}"
echo -e "${BLUE}                           BUILD VS SKIP SUMMARY                               ${NC}"
echo -e "${BLUE}================================================================================${NC}"
echo ""

# Show newly built libraries
if [ ${#LIBS_BUILT[@]} -gt 0 ]; then
    echo -e "${GREEN}Libraries Built (${#LIBS_BUILT[@]}):${NC}"
    for lib in "${LIBS_BUILT[@]}"; do
        echo -e "  ${GREEN}✓${NC} $lib"
    done
    echo ""
else
    echo -e "${YELLOW}Libraries Built: None (all already present)${NC}"
    echo ""
fi

# Show skipped libraries
if [ ${#LIBS_SKIPPED[@]} -gt 0 ]; then
    echo -e "${YELLOW}Libraries Skipped - Already Present (${#LIBS_SKIPPED[@]}):${NC}"
    for lib in "${LIBS_SKIPPED[@]}"; do
        echo -e "  ${YELLOW}↻${NC} $lib"
    done
    echo ""
else
    echo -e "${YELLOW}Libraries Skipped: None (all newly built)${NC}"
    echo ""
fi

# Show newly copied headers
if [ ${#HEADERS_COPIED[@]} -gt 0 ]; then
    echo -e "${GREEN}Headers Copied (${#HEADERS_COPIED[@]}):${NC}"
    for lib in "${HEADERS_COPIED[@]}"; do
        echo -e "  ${GREEN}✓${NC} $lib"
    done
    echo ""
else
    echo -e "${YELLOW}Headers Copied: None (all already present)${NC}"
    echo ""
fi

# Show skipped headers
if [ ${#HEADERS_SKIPPED[@]} -gt 0 ]; then
    echo -e "${YELLOW}Headers Skipped - Already Present (${#HEADERS_SKIPPED[@]}):${NC}"
    for lib in "${HEADERS_SKIPPED[@]}"; do
        echo -e "  ${YELLOW}↻${NC} $lib"
    done
    echo ""
else
    echo -e "${YELLOW}Headers Skipped: None (all newly copied)${NC}"
    echo ""
fi

# Helper function to check if library was built or skipped
was_built() {
    local lib_name="$1"
    for built in "${LIBS_BUILT[@]}"; do
        if [[ "$built" == *"$lib_name"* ]]; then
            return 0
        fi
    done
    return 1
}

# Show detailed .a files created with paths and sizes
echo -e "${BLUE}================================================================================${NC}"
echo -e "${BLUE}                        STATIC LIBRARIES (.a files)                           ${NC}"
echo -e "${BLUE}================================================================================${NC}"
echo ""
printf "${YELLOW}%-20s %-12s %-10s %-15s${NC}\n" "LIBRARY" "VERSION" "SIZE" "STATUS"
echo "--------------------------------------------------------------------------------"

# Check and display each library with status
if [[ "$BUILD_IMGUI" == true ]] && [ -f "$LIB_OUTPUT_DIR/libimgui.a" ]; then
    SIZE=$(ls -lh "$LIB_OUTPUT_DIR/libimgui.a" | awk '{print $5}')
    if was_built "ImGui"; then
        printf "${GREEN}%-20s${NC} %-12s %-10s ${GREEN}%-15s${NC}\n" "ImGui" "bundled" "$SIZE" "✓ Built"
    else
        printf "${GREEN}%-20s${NC} %-12s %-10s ${YELLOW}%-15s${NC}\n" "ImGui" "bundled" "$SIZE" "↻ Already present"
    fi
fi

if [[ "$BUILD_FREETYPE" == true ]] && [ -f "$LIB_OUTPUT_DIR/libfreetype.a" ]; then
    SIZE=$(ls -lh "$LIB_OUTPUT_DIR/libfreetype.a" | awk '{print $5}')
    if was_built "FreeType"; then
        printf "${GREEN}%-20s${NC} %-12s %-10s ${GREEN}%-15s${NC}\n" "FreeType" "v${FREETYPE_VERSION}" "$SIZE" "✓ Built"
    else
        printf "${GREEN}%-20s${NC} %-12s %-10s ${YELLOW}%-15s${NC}\n" "FreeType" "v${FREETYPE_VERSION}" "$SIZE" "↻ Already present"
    fi
fi

if [[ "$BUILD_YOGA" == true ]] && [ -f "$LIB_OUTPUT_DIR/libyoga.a" ]; then
    SIZE=$(ls -lh "$LIB_OUTPUT_DIR/libyoga.a" | awk '{print $5}')
    if was_built "Yoga"; then
        printf "${GREEN}%-20s${NC} %-12s %-10s ${GREEN}%-15s${NC}\n" "Yoga" "bundled" "$SIZE" "✓ Built"
    else
        printf "${GREEN}%-20s${NC} %-12s %-10s ${YELLOW}%-15s${NC}\n" "Yoga" "bundled" "$SIZE" "↻ Already present"
    fi
fi

if [[ "$BUILD_IMPLOT" == true ]] && [ -f "$LIB_OUTPUT_DIR/libimplot.a" ]; then
    SIZE=$(ls -lh "$LIB_OUTPUT_DIR/libimplot.a" | awk '{print $5}')
    if was_built "ImPlot"; then
        printf "${GREEN}%-20s${NC} %-12s %-10s ${GREEN}%-15s${NC}\n" "ImPlot" "bundled" "$SIZE" "✓ Built"
    else
        printf "${GREEN}%-20s${NC} %-12s %-10s ${YELLOW}%-15s${NC}\n" "ImPlot" "bundled" "$SIZE" "↻ Already present"
    fi
fi

if [[ "$BUILD_PLUTOVG" == true ]] && [ -f "$LIB_OUTPUT_DIR/libplutovg.a" ]; then
    SIZE=$(ls -lh "$LIB_OUTPUT_DIR/libplutovg.a" | awk '{print $5}')
    if was_built "PlutoVG"; then
        printf "${GREEN}%-20s${NC} %-12s %-10s ${GREEN}%-15s${NC}\n" "PlutoVG" "v${PLUTOVG_VERSION}" "$SIZE" "✓ Built"
    else
        printf "${GREEN}%-20s${NC} %-12s %-10s ${YELLOW}%-15s${NC}\n" "PlutoVG" "v${PLUTOVG_VERSION}" "$SIZE" "↻ Already present"
    fi
fi

if [[ "$BUILD_LUNASVG" == true ]] && [ -f "$LIB_OUTPUT_DIR/liblunasvg.a" ]; then
    SIZE=$(ls -lh "$LIB_OUTPUT_DIR/liblunasvg.a" | awk '{print $5}')
    if was_built "LunaSVG"; then
        printf "${GREEN}%-20s${NC} %-12s %-10s ${GREEN}%-15s${NC}\n" "LunaSVG" "v${LUNASVG_VERSION}" "$SIZE" "✓ Built"
    else
        printf "${GREEN}%-20s${NC} %-12s %-10s ${YELLOW}%-15s${NC}\n" "LunaSVG" "v${LUNASVG_VERSION}" "$SIZE" "↻ Already present"
    fi
fi

if [[ "$BUILD_SDL3" == true ]] && [ -f "$LIB_OUTPUT_DIR/libSDL3.a" ]; then
    SIZE=$(ls -lh "$LIB_OUTPUT_DIR/libSDL3.a" | awk '{print $5}')
    if was_built "SDL3"; then
        printf "${GREEN}%-20s${NC} %-12s %-10s ${GREEN}%-15s${NC}\n" "SDL3" "v${SDL3_VERSION}" "$SIZE" "✓ Built"
    else
        printf "${GREEN}%-20s${NC} %-12s %-10s ${YELLOW}%-15s${NC}\n" "SDL3" "v${SDL3_VERSION}" "$SIZE" "↻ Already present"
    fi
fi

if [[ "$BUILD_GLFW" == true ]] && [ -f "$LIB_OUTPUT_DIR/libglfw3.a" ]; then
    SIZE=$(ls -lh "$LIB_OUTPUT_DIR/libglfw3.a" | awk '{print $5}')
    if was_built "GLFW"; then
        printf "${GREEN}%-20s${NC} %-12s %-10s ${GREEN}%-15s${NC}\n" "GLFW" "v${GLFW_VERSION}" "$SIZE" "✓ Built"
    else
        printf "${GREEN}%-20s${NC} %-12s %-10s ${YELLOW}%-15s${NC}\n" "GLFW" "v${GLFW_VERSION}" "$SIZE" "↻ Already present"
    fi
fi

if [[ "$BUILD_BLEND2D" == true ]] && [ -f "$LIB_OUTPUT_DIR/libblend2d.a" ]; then
    SIZE=$(ls -lh "$LIB_OUTPUT_DIR/libblend2d.a" | awk '{print $5}')
    if was_built "Blend2D"; then
        printf "${GREEN}%-20s${NC} %-12s %-10s ${GREEN}%-15s${NC}\n" "Blend2D" "v${BLEND2D_VERSION}" "$SIZE" "✓ Built"
    else
        printf "${GREEN}%-20s${NC} %-12s %-10s ${YELLOW}%-15s${NC}\n" "Blend2D" "v${BLEND2D_VERSION}" "$SIZE" "↻ Already present"
    fi
fi

if [[ "$BUILD_NFDEXT" == true ]] && [ -f "$LIB_OUTPUT_DIR/libnfd.a" ]; then
    SIZE=$(ls -lh "$LIB_OUTPUT_DIR/libnfd.a" | awk '{print $5}')
    if was_built "NFD"; then
        printf "${GREEN}%-20s${NC} %-12s %-10s ${GREEN}%-15s${NC}\n" "NFD-Extended" "${NFD_VERSION}" "$SIZE" "✓ Built"
    else
        printf "${GREEN}%-20s${NC} %-12s %-10s ${YELLOW}%-15s${NC}\n" "NFD-Extended" "${NFD_VERSION}" "$SIZE" "↻ Already present"
    fi
fi

if [[ "$BUILD_PDCURSES" == true ]] && [ -f "$LIB_OUTPUT_DIR/libpdcurses.a" ]; then
    SIZE=$(ls -lh "$LIB_OUTPUT_DIR/libpdcurses.a" | awk '{print $5}')
    if was_built "PDCurses"; then
        printf "${GREEN}%-20s${NC} %-12s %-10s ${GREEN}%-15s${NC}\n" "PDCurses" "v${PDCURSES_VERSION}" "$SIZE" "✓ Built"
    else
        printf "${GREEN}%-20s${NC} %-12s %-10s ${YELLOW}%-15s${NC}\n" "PDCurses" "v${PDCURSES_VERSION}" "$SIZE" "↻ Already present"
    fi
fi

echo ""
echo -e "${YELLOW}Path:${NC} $LIB_OUTPUT_DIR"

echo ""
TOTAL_SIZE=$(du -sh "$LIB_OUTPUT_DIR/" 2>/dev/null | awk '{print $1}')
echo -e "${YELLOW}Total Size:${NC} $TOTAL_SIZE"
echo ""

# Show header-only libraries and disabled features
echo -e "${BLUE}================================================================================${NC}"
echo -e "${BLUE}                       HEADER-ONLY LIBRARIES & FEATURES                        ${NC}"
echo -e "${BLUE}================================================================================${NC}"
echo ""
printf "${YELLOW}%-20s %-12s %-15s${NC}\n" "LIBRARY" "VERSION" "STATUS"
echo "--------------------------------------------------------------------------------"

if [[ "$BUILD_JSON" == true ]]; then
    if [ -f "$HEADER_OUTPUT_DIR/json/json.hpp" ]; then
        # Check if it was copied or already present
        if [[ " ${HEADERS_COPIED[@]} " =~ " nlohmann-json " ]]; then
            printf "${GREEN}%-20s${NC} %-12s ${GREEN}%-15s${NC}\n" "nlohmann-json" "v${JSON_VERSION}" "✓ Copied"
        else
            printf "${GREEN}%-20s${NC} %-12s ${YELLOW}%-15s${NC}\n" "nlohmann-json" "v${JSON_VERSION}" "↻ Already present"
        fi
    fi
fi

if [[ "$COPY_STB_IMAGE" == true ]]; then
    if [ -f "$HEADER_OUTPUT_DIR/stb/stb_image.h" ]; then
        # Check if it was copied or already present
        if [[ " ${HEADERS_COPIED[@]} " =~ " stb_image " ]]; then
            printf "${GREEN}%-20s${NC} %-12s ${GREEN}%-15s${NC}\n" "stb_image" "latest" "✓ Copied"
        else
            printf "${GREEN}%-20s${NC} %-12s ${YELLOW}%-15s${NC}\n" "stb_image" "latest" "↻ Already present"
        fi
    fi
else
    printf "${YELLOW}%-20s${NC} %-12s ${YELLOW}%-15s${NC}\n" "stb_image" "DISABLED" "(skip)"
fi

if [[ "$COPY_FONTAWESOME" == true ]]; then
    if [ -f "$HEADER_OUTPUT_DIR/fontawesome/IconsFontAwesome6.h" ]; then
        # Check if it was copied or already present
        if [[ " ${HEADERS_COPIED[@]} " =~ " IconFontCppHeaders " ]]; then
            printf "${GREEN}%-20s${NC} %-12s ${GREEN}%-15s${NC}\n" "FontAwesome" "latest" "✓ Copied"
        else
            printf "${GREEN}%-20s${NC} %-12s ${YELLOW}%-15s${NC}\n" "FontAwesome" "latest" "↻ Already present"
        fi
    fi
else
    printf "${YELLOW}%-20s${NC} %-12s ${YELLOW}%-15s${NC}\n" "FontAwesome" "DISABLED" "(skip)"
fi

# Show disabled features
if [[ "$BUILD_IMPLOT" != true ]]; then
    printf "${YELLOW}%-20s${NC} %-12s ${YELLOW}%-15s${NC}\n" "ImPlot" "DISABLED" "(skip)"
fi

if [[ "$BUILD_PLUTOVG" != true ]] || [[ "$BUILD_LUNASVG" != true ]]; then
    printf "${YELLOW}%-20s${NC} %-12s ${YELLOW}%-15s${NC}\n" "SVG Support" "DISABLED" "(skip)"
fi

echo ""
echo -e "${YELLOW}Path:${NC} $HEADER_OUTPUT_DIR"

echo ""

# Show all headers copied
echo -e "${BLUE}================================================================================${NC}"
echo -e "${BLUE}                         HEADERS IN src/libs/inc/                              ${NC}"
echo -e "${BLUE}================================================================================${NC}"
echo ""

# Helper function to check if headers were copied for a library
headers_were_copied() {
    local lib_name="$1"
    for copied in "${HEADERS_COPIED[@]}"; do
        if [[ "$copied" == *"$lib_name"* ]]; then
            return 0
        fi
    done
    return 1
}

printf "${YELLOW}%-20s %10s  %-15s${NC}\n" "LIBRARY" "FILES" "STATUS"
echo "--------------------------------------------------------------------------------"

if [ -d "$HEADER_OUTPUT_DIR" ]; then
    for dir in "$HEADER_OUTPUT_DIR"/*/ ; do
        if [ -d "$dir" ]; then
            dirname=$(basename "$dir")
            filecount=$(find "$dir" -type f \( -name "*.h" -o -name "*.hpp" \) 2>/dev/null | wc -l)
            if [ "$filecount" -gt 0 ]; then
                # Check if headers were copied or already present
                # Map directory names to library names used in tracking
                case "$dirname" in
                    plutovg)    lib_check="PlutoVG" ;;
                    lunasvg)    lib_check="LunaSVG" ;;
                    SDL3)       lib_check="SDL3" ;;
                    GLFW)       lib_check="GLFW" ;;
                    freetype2)  lib_check="FreeType" ;;
                    blend2d)    lib_check="Blend2D" ;;
                    nfd-ext)    lib_check="NFD" ;;
                    pdcurses)   lib_check="PDCurses" ;;
                    nlohmann|json) lib_check="nlohmann-json" ;;
                    stb)        lib_check="stb_image" ;;
                    fontawesome) lib_check="IconFontCppHeaders" ;;
                    imgui|implot|yoga) lib_check="bundled" ;;
                    *)          lib_check="$dirname" ;;
                esac
                
                if headers_were_copied "$lib_check"; then
                    printf "${GREEN}%-20s${NC} %10s  ${GREEN}%-15s${NC}\n" "$dirname" "$filecount files" "✓ Copied"
                else
                    printf "${GREEN}%-20s${NC} %10s  ${YELLOW}%-15s${NC}\n" "$dirname" "$filecount files" "↻ Already present"
                fi
            fi
        fi
    done
else
    echo "(No headers directory yet)"
fi

echo ""
echo -e "${YELLOW}Path:${NC} $HEADER_OUTPUT_DIR"

echo ""

# Show version tracking file
echo -e "${BLUE}================================================================================${NC}"
echo -e "${BLUE}                         INSTALLED DEPENDENCY VERSIONS                         ${NC}"
echo -e "${BLUE}================================================================================${NC}"
echo ""

if [ -f "$VERSION_FILE" ]; then
    cat "$VERSION_FILE" | sed 's/^/  /'
else
    echo "  (No version file yet)"
fi

echo ""
echo -e "${BLUE}================================================================================${NC}"
echo -e "${GREEN}✓ Dependencies Build Complete!${NC}"
echo ""
echo -e "${YELLOW}Next Step:${NC} Build the main Glimmer library"
if [ "$BUILD_TYPE" = "Debug" ]; then
    echo -e "${YELLOW}Command:${NC}   cd $PROJECT_ROOT && ./build.sh --platform=$PLATFORM --debug"
else
    echo -e "${YELLOW}Command:${NC}   cd $PROJECT_ROOT && ./build.sh --platform=$PLATFORM"
fi
echo ""
echo -e "${YELLOW}To view versions later:${NC} cat $VERSION_FILE"
echo -e "${BLUE}================================================================================${NC}"
echo ""
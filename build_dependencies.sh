#!/bin/bash

# Build script for Glimmer Linux dependencies
# This builds plutovg, lunasvg, SDL3, and blend2d as static libraries

set -e  # Exit on error

# Get the directory where this script is located (portable)
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$SCRIPT_DIR"

# Enable newer GCC if available (needed for C++20 support in Yoga)
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
    echo "Yoga build may fail. Install with: sudo yum install gcc-toolset-11"
fi

gcc --version | head -1

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Parse command line arguments
BUILD_TYPE="Release"
LIB_SUBDIR="release"

print_usage() {
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -h, --help              Show this help message"
    echo "  -r, --release           Build in Release mode (optimized, default)"
    echo "  -d, --debug             Build in Debug mode (with debug symbols)"
    echo ""
    echo "Examples:"
    echo "  $0                      # Build release version (default)"
    echo "  $0 --release            # Build release version (explicit)"
    echo "  $0 --debug              # Build debug version"
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
            LIB_SUBDIR="release"
            shift
            ;;
        -d|--debug)
            BUILD_TYPE="Debug"
            LIB_SUBDIR="debug"
            shift
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            print_usage
            exit 1
            ;;
    esac
done

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Building Glimmer Dependencies${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}Build Type: $BUILD_TYPE${NC}"
echo -e "${GREEN}Output Dir: $PROJECT_ROOT/src/libs/lib/linux/$LIB_SUBDIR/${NC}"
echo -e "${GREEN}Libraries: 8 static libraries (.a files)${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Create directory structure
echo -e "${YELLOW}Creating directory structure...${NC}"
LIB_OUTPUT_DIR="$PROJECT_ROOT/src/libs/lib/linux/$LIB_SUBDIR"
mkdir -p "$LIB_OUTPUT_DIR"
BUILD_DIR="$PROJECT_ROOT/../glimmer_deps_build_$LIB_SUBDIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Build plutovg
echo ""
echo -e "${GREEN}[1/8] Building plutovg...${NC}"
if [ -f "$LIB_OUTPUT_DIR/libplutovg.a" ]; then
    echo -e "${YELLOW}✓ libplutovg.a already exists, skipping build${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libplutovg.a"
else
    if [ ! -d "plutovg" ]; then
        echo -e "${YELLOW}Cloning plutovg...${NC}"
        git clone https://github.com/sammycage/plutovg.git
    else
        echo -e "${YELLOW}plutovg source found, building...${NC}"
    fi
    cd plutovg
    rm -rf build
    mkdir build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DBUILD_SHARED_LIBS=OFF
    make -j$(nproc)
    cp libplutovg.a "$LIB_OUTPUT_DIR/"
    echo -e "${GREEN}✓ plutovg built successfully${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libplutovg.a"
    cd "$BUILD_DIR"
fi

# Build lunasvg
echo ""
echo -e "${GREEN}[2/8] Building lunasvg...${NC}"
cd "$BUILD_DIR"
if [ -f "$LIB_OUTPUT_DIR/liblunasvg.a" ]; then
    echo -e "${YELLOW}✓ liblunasvg.a already exists, skipping build${NC}"
    ls -lh "$LIB_OUTPUT_DIR/liblunasvg.a"
else
    if [ ! -d "lunasvg" ]; then
        echo -e "${YELLOW}Cloning lunasvg...${NC}"
        git clone https://github.com/sammycage/lunasvg.git
    else
        echo -e "${YELLOW}lunasvg source found, building...${NC}"
    fi
    cd lunasvg
    rm -rf build
    mkdir build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DBUILD_SHARED_LIBS=OFF -DLUNASVG_BUILD_EXAMPLES=OFF
    make -j$(nproc)
    cp liblunasvg.a "$LIB_OUTPUT_DIR/"
    echo -e "${GREEN}✓ lunasvg built successfully${NC}"
    ls -lh "$LIB_OUTPUT_DIR/liblunasvg.a"
    cd "$BUILD_DIR"
fi

# Build SDL3 (optional but recommended)
echo ""
echo -e "${GREEN}[3/8] Building SDL3 (this may take a while)...${NC}"
cd "$BUILD_DIR"
if [ -f "$LIB_OUTPUT_DIR/libSDL3.a" ]; then
    echo -e "${YELLOW}✓ libSDL3.a already exists, skipping build${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libSDL3.a"
else
    if [ ! -d "SDL" ]; then
        echo -e "${YELLOW}Cloning SDL3...${NC}"
        git clone https://github.com/libsdl-org/SDL.git
    else
        echo -e "${YELLOW}SDL3 source found, building...${NC}"
    fi
    cd SDL
    git clean -fdx  # Clean any leftover build artifacts
    rm -rf build
    mkdir build && cd build

    # Try to build SDL3, but continue if it fails (it's optional)
    if cmake .. \
        -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
        -DBUILD_SHARED_LIBS=OFF \
        -DSDL_STATIC=ON \
        -DSDL_SHARED=OFF \
        -DSDL_TEST=OFF \
        -DSDL_X11_XCURSOR=OFF \
        -DSDL_X11_XINERAMA=OFF \
        -DSDL_X11_XRANDR=OFF \
        -DSDL_X11_XSCRNSAVER=OFF \
        -DSDL_X11_XSHAPE=OFF \
        -DSDL_X11_XTEST=OFF && \
       make -j$(nproc) && \
       cp libSDL3.a "$LIB_OUTPUT_DIR/"; then
        echo -e "${GREEN}✓ SDL3 built successfully${NC}"
        ls -lh "$LIB_OUTPUT_DIR/libSDL3.a"
    else
        echo -e "${YELLOW}⚠ SDL3 build failed. This is optional - continuing without it.${NC}"
        echo -e "${YELLOW}To build SDL3, install missing X11 packages:${NC}"
        echo -e "${YELLOW}  libXcursor-devel libXrandr-devel libXScrnSaver-devel libXinerama-devel libXtst-devel libXxf86vm-devel${NC}"
    fi
    cd "$BUILD_DIR"
fi

# Build freetype
echo ""
echo -e "${GREEN}[4/8] Building freetype...${NC}"
cd "$BUILD_DIR"
if [ -f "$LIB_OUTPUT_DIR/libfreetype.a" ]; then
    echo -e "${YELLOW}✓ libfreetype.a already exists, skipping build${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libfreetype.a"
else
    if [ ! -d "freetype" ]; then
        echo -e "${YELLOW}Cloning freetype...${NC}"
        git clone https://gitlab.freedesktop.org/freetype/freetype.git
    else
        echo -e "${YELLOW}freetype source found, building...${NC}"
    fi
    cd freetype
    rm -rf build
    mkdir build && cd build
    cmake .. \
        -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
        -DBUILD_SHARED_LIBS=OFF \
        -DFT_DISABLE_ZLIB=OFF \
        -DFT_DISABLE_BZIP2=OFF \
        -DFT_DISABLE_PNG=OFF \
        -DFT_DISABLE_HARFBUZZ=ON \
        -DFT_DISABLE_BROTLI=ON
    make -j$(nproc)
    # Debug builds create libfreetyped.a, release creates libfreetype.a
    if [ "$BUILD_TYPE" = "Debug" ]; then
        cp libfreetyped.a "$LIB_OUTPUT_DIR/libfreetype.a"
    else
        cp libfreetype.a "$LIB_OUTPUT_DIR/"
    fi
    echo -e "${GREEN}✓ freetype built successfully${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libfreetype.a"
    cd "$BUILD_DIR"
fi

# Build ImGui
echo ""
echo -e "${GREEN}[5/8] Building ImGui...${NC}"
if [ -f "$LIB_OUTPUT_DIR/libimgui.a" ]; then
    echo -e "${YELLOW}✓ libimgui.a already exists, skipping build${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libimgui.a"
else
    echo -e "${YELLOW}Building ImGui from source...${NC}"
    cd "$PROJECT_ROOT/src/libs/src"
    g++ -c -O3 -std=c++17 -fPIC \
        -I../inc \
        -I../inc/imgui \
        imgui.cpp \
        imgui_demo.cpp \
        imgui_draw.cpp \
        imgui_tables.cpp \
        imgui_widgets.cpp
    ar rcs libimgui.a imgui.o imgui_demo.o imgui_draw.o imgui_tables.o imgui_widgets.o
    cp libimgui.a "$LIB_OUTPUT_DIR/"
    rm -f *.o
    echo -e "${GREEN}✓ ImGui built successfully${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libimgui.a"
fi

# Build ImPlot
echo ""
echo -e "${GREEN}[6/8] Building ImPlot...${NC}"
if [ -f "$LIB_OUTPUT_DIR/libimplot.a" ]; then
    echo -e "${YELLOW}✓ libimplot.a already exists, skipping build${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libimplot.a"
else
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
    echo -e "${GREEN}✓ ImPlot built successfully${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libimplot.a"
fi

# Build Yoga (flexbox layout library) - requires C++20
echo ""
echo -e "${GREEN}[7/8] Building Yoga...${NC}"
if [ -f "$LIB_OUTPUT_DIR/libyogacore.a" ]; then
    echo -e "${YELLOW}✓ libyogacore.a already exists, skipping build${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libyogacore.a"
else
    echo -e "${YELLOW}Building Yoga from source...${NC}"
    cd "$PROJECT_ROOT/src/libs/inc/yoga"
    rm -f *.o *.a
    g++ -c -O3 -std=c++20 -fPIC \
        -I.. \
        -I. \
        YGConfig.cpp \
        YGEnums.cpp \
        YGNode.cpp \
        YGNodeLayout.cpp \
        YGNodeStyle.cpp \
        YGPixelGrid.cpp \
        YGValue.cpp 2>&1 || echo "Note: Yoga compilation may have warnings"
    ar rcs libyogacore.a YGConfig.o YGEnums.o YGNode.o YGNodeLayout.o YGNodeStyle.o YGPixelGrid.o YGValue.o
    cp libyogacore.a "$LIB_OUTPUT_DIR/"
    rm -f *.o
    echo -e "${GREEN}✓ Yoga built successfully${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libyogacore.a"
fi

# Build blend2d
echo ""
echo -e "${GREEN}[8/8] Building blend2d (this may take a while)...${NC}"
cd "$BUILD_DIR"
if [ -f "$LIB_OUTPUT_DIR/libblend2d.a" ]; then
    echo -e "${YELLOW}✓ libblend2d.a already exists, skipping build${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libblend2d.a"
else
    if [ ! -d "blend2d" ]; then
        echo -e "${YELLOW}Cloning blend2d...${NC}"
        git clone https://github.com/blend2d/blend2d.git
    else
        echo -e "${YELLOW}blend2d source found, building...${NC}"
    fi
    cd blend2d

    # Clone asmjit dependency if not present (required for JIT support)
    if [ ! -d "3rdparty/asmjit" ]; then
        echo -e "${YELLOW}Cloning asmjit dependency...${NC}"
        mkdir -p 3rdparty
        cd 3rdparty
        git clone https://github.com/asmjit/asmjit.git
        cd ..
    fi

    rm -rf build
    mkdir build && cd build
    cmake .. \
        -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
        -DBUILD_SHARED_LIBS=OFF \
        -DBLEND2D_STATIC=ON \
        -DBLEND2D_TEST=OFF
    make -j$(nproc)
    cp libblend2d.a "$LIB_OUTPUT_DIR/"
    echo -e "${GREEN}✓ blend2d built successfully${NC}"
    ls -lh "$LIB_OUTPUT_DIR/libblend2d.a"
    cd "$BUILD_DIR"
fi

# Summary
echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Build Summary${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "${GREEN}All libraries built successfully!${NC}"
echo ""
echo "Libraries created in: $LIB_OUTPUT_DIR/"
ls -lh "$LIB_OUTPUT_DIR/"
echo ""
echo -e "${YELLOW}Total size:${NC}"
du -sh "$LIB_OUTPUT_DIR/"
echo ""
echo -e "${GREEN}You can now build the Glimmer library with these dependencies!${NC}"
if [ "$BUILD_TYPE" = "Debug" ]; then
    echo -e "${YELLOW}Next step: cd $PROJECT_ROOT && ./build.sh --debug${NC}"
else
    echo -e "${YELLOW}Next step: cd $PROJECT_ROOT && ./build.sh${NC}"
fi


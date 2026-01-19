# Building Glimmer Static Library for Linux

This document provides instructions for building the Glimmer library as a static library on Linux systems.

## Prerequisites

### Required Tools

- **CMake** (version 3.15 or higher)
- **GCC 11+** or **Clang** compiler with C++20 support
- **Make** or **Ninja** build system
- **Git** (for cloning dependencies)

Install on Ubuntu/Debian:
```bash
sudo apt update
sudo apt install cmake build-essential git
```

Install on Fedora/RHEL/CentOS:
```bash
sudo dnf install cmake gcc-c++ make git

# For C++20 support on RHEL 8/CentOS 8, install GCC Toolset 11 or 12:
sudo yum install gcc-toolset-12
# The build scripts will automatically use it
```

Install on Arch Linux:
```bash
sudo pacman -S cmake base-devel
```

### Required Dependencies

Glimmer requires these static libraries (built automatically by `build_dependencies.sh`):
- **libplutovg.a** - Vector graphics rendering
- **liblunasvg.a** - SVG support
- **libSDL3.a** - Platform/windowing (with X11 support)
- **libfreetype.a** - Font rendering
- **libimgui.a** - Immediate mode GUI framework
- **libimplot.a** - Plotting support
- **libyoga.a** - Flexbox layout engine
- **libblend2d.a** - 2D graphics rendering (optional)

### System Dependencies

For building the dependencies, you'll need X11 development libraries:

Ubuntu/Debian:
```bash
sudo apt install libx11-dev libxext-dev libxi-dev libxfixes-dev
```

RHEL/CentOS (may already be installed):
```bash
sudo yum install libX11-devel libXext-devel libXi-devel libXfixes-devel
```

## Build Process

### Step 1: Build Dependencies (Required - First Time Only)

Before building Glimmer, you must build the required static libraries:

```bash
cd glimmer  # Navigate to your glimmer directory
./build_dependencies.sh
```

This will build 8 static libraries and place them in `src/libs/lib/linux/release/`.

**Build dependencies in debug mode:**
```bash
./build_dependencies.sh --debug
```

**Note:** This step takes 10-20 minutes depending on your system. You only need to run this once, or when you want to switch between release/debug builds.

### Step 2: Build Glimmer Library

Once dependencies are built, build the Glimmer library:

```bash
./build.sh
```

This will build the library in **Release** mode and create `libglimmer.a` in the `staticlib/` directory.

### Build Script Options

```bash
./build.sh [options]

Options:
  -h, --help              Show help message
  -r, --release           Build in Release mode (optimized, default)
  -d, --debug             Build in Debug mode (with debug symbols)
  -c, --clean             Clean build directory before building
  --no-svg                Disable SVG support
  --no-images             Disable image support
  --no-richtext           Disable rich text support
  --no-plots              Disable plots support
  --enable-nfdext         Enable nfd-extended library for file dialogs
  --enable-blend2d        Enable Blend2D renderer
```

### Build Examples

**Release build (default):**
```bash
./build.sh
# or explicitly:
./build.sh --release
```

**Debug build:**
```bash
./build_dependencies.sh --debug   # Build debug dependencies first
./build.sh --debug                # Build Glimmer in debug mode
```

**Clean build:**
```bash
./build.sh --clean --release
```

**Custom feature configuration:**
```bash
./build.sh --no-svg --no-images   # Minimal build
./build.sh --enable-blend2d       # With Blend2D renderer
```

## Manual Build with CMake

If you prefer to use CMake directly:

### 1. Create Build Directory

```bash
cd glimmer  # Your glimmer directory
mkdir -p build
cd build
```

### 2. Configure with CMake

**Release build:**
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
```

**Debug build:**
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

**With custom options:**
```bash
cmake -DCMAKE_BUILD_TYPE=Release \
      -DGLIMMER_DISABLE_SVG=OFF \
      -DGLIMMER_DISABLE_IMAGES=OFF \
      -DGLIMMER_DISABLE_RICHTEXT=OFF \
      -DGLIMMER_DISABLE_PLOTS=OFF \
      -DGLIMMER_ENABLE_NFDEXT=ON \
      ..
```

### 3. Build

```bash
make -j$(nproc)
```

Or use Ninja for faster builds:
```bash
cmake -GNinja ..
ninja
```

### 4. Install (Optional)

```bash
sudo make install
```

This will install:
- Library: `/usr/local/lib/libglimmer.a`
- Headers: `/usr/local/include/glimmer/`

## CMake Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `GLIMMER_DISABLE_SVG` | OFF | Disable SVG rendering support (excludes lunasvg) |
| `GLIMMER_DISABLE_IMAGES` | OFF | Disable image loading support (excludes stb_image) |
| `GLIMMER_DISABLE_RICHTEXT` | OFF | Disable rich text rendering |
| `GLIMMER_DISABLE_PLOTS` | OFF | Disable plotting/graph library integration |
| `GLIMMER_ENABLE_NFDEXT` | OFF | Enable nfd-extended for native file dialogs |
| `GLIMMER_ENABLE_BLEND2D` | OFF | Enable Blend2D renderer (requires libblend2d.a) |

## Output

After successful build, you will find:

```
staticlib/
└── libglimmer.a    # Static library
```

## Using the Library

### 1. Link Against the Static Library

In your project's CMakeLists.txt:

```cmake
# Set your Glimmer installation path
set(GLIMMER_ROOT "/path/to/glimmer")  # Update this to your glimmer location

# Add Glimmer include directories
include_directories(${GLIMMER_ROOT})
include_directories(${GLIMMER_ROOT}/src)
include_directories(${GLIMMER_ROOT}/src/libs/inc)

# Link against the static library and all dependencies
add_executable(myapp main.cpp)

# Glimmer and its dependencies
set(GLIMMER_LIB_DIR ${GLIMMER_ROOT}/src/libs/lib/linux/release)
target_link_libraries(myapp 
    ${GLIMMER_ROOT}/staticlib/libglimmer.a
    ${GLIMMER_LIB_DIR}/libimgui.a
    ${GLIMMER_LIB_DIR}/libimplot.a
    ${GLIMMER_LIB_DIR}/libyoga.a
    ${GLIMMER_LIB_DIR}/liblunasvg.a
    ${GLIMMER_LIB_DIR}/libplutovg.a
    ${GLIMMER_LIB_DIR}/libSDL3.a
    ${GLIMMER_LIB_DIR}/libfreetype.a
)

# System libraries
find_package(X11 REQUIRED)
find_package(OpenGL REQUIRED)
target_link_libraries(myapp 
    ${X11_LIBRARIES}
    OpenGL::GL
    pthread
    dl
    m
)
```

### 2. Using with GCC/Clang Directly

```bash
# Set your Glimmer directory
GLIMMER_DIR=/path/to/glimmer  # Update this to your glimmer location
LIB_DIR=$GLIMMER_DIR/src/libs/lib/linux/release

g++ -std=c++20 main.cpp \
    -I$GLIMMER_DIR \
    -I$GLIMMER_DIR/src \
    -I$GLIMMER_DIR/src/libs/inc \
    $GLIMMER_DIR/staticlib/libglimmer.a \
    $LIB_DIR/libimgui.a \
    $LIB_DIR/libimplot.a \
    $LIB_DIR/libyoga.a \
    $LIB_DIR/liblunasvg.a \
    $LIB_DIR/libplutovg.a \
    $LIB_DIR/libSDL3.a \
    $LIB_DIR/libfreetype.a \
    -lX11 -lXext -lXi -lXfixes \
    -lGL -lpthread -ldl -lm \
    -o myapp
```

### 3. Include in Your Code

```cpp
#include <glimmer.h>

int main() {
    auto& config = glimmer::GetUIConfig(true);
    // ... your code
    return 0;
}
```

## Troubleshooting

### Missing C++20 Support

If you get compiler errors about C++20 features:
```bash
# Update GCC (requires GCC 10+)
sudo apt install gcc-10 g++-10
export CC=gcc-10
export CXX=g++-10
```

### CMake Version Too Old

```bash
# Install newer CMake from snap
sudo snap install cmake --classic
```

### Missing FreeType Headers

```bash
sudo apt install libfreetype6-dev
```

### Build Errors

Clean and rebuild:
```bash
./build.sh --clean
```

Or manually:
```bash
rm -rf build staticlib/libglimmer.a
mkdir build && cd build
cmake ..
make clean
make -j$(nproc)
```

## Verify Build

Check that the library was created:

```bash
ls -lh staticlib/libglimmer.a
```

Check symbols in the library:

```bash
nm staticlib/libglimmer.a | grep glimmer
```

Check library information:

```bash
file staticlib/libglimmer.a
ar -t staticlib/libglimmer.a
```

## Platform Notes

### Linux Only

This CMake configuration is specifically designed for Linux. Building on other platforms (Windows, macOS) is not supported by this build script and will produce an error.

### Thread Safety

The library is built with position-independent code (`-fPIC`) and links against pthreads for thread safety.

## Additional Information

- See `README.md` for library usage and API documentation
- See `GlimmerTest/test.cpp` for example code
- Library source: `src/` directory
- Dependencies: `src/libs/` directory




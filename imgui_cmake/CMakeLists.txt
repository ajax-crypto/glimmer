cmake_minimum_required(VERSION 3.16)

# Project Configuration
project(ImGuiStaticLib VERSION 1.0.0 LANGUAGES CXX)

# ==============================================================================
# Paths & Dependencies
# ==============================================================================
# Allow user to override FreeType include path via command line:
# cmake .. -DFREETYPE_INCLUDE_DIR="C:/MyLibs/freetype/include"
option(FREETYPE_INCLUDE_DIR "" "${CMAKE_CURRENT_SOURCE_DIR}/../src/libs/inc/freetype2")

# ==============================================================================
# Options
# ==============================================================================
option(BUILD_WITH_SDL3 "Build ImGui with SDL3 backend" ON)
option(BUILD_WITH_GLFW "Build ImGui with GLFW backend" OFF)
option(BUILD_IMPLOT    "Include ImPlot in the library" ON)

# Default Build Type (Release) if not specified (Mostly for Linux/Ninja/Makefiles)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (Release, Debug)" FORCE)
endif()

# ==============================================================================
# Paths (Customize these relative to your CMakeLists.txt)
# ==============================================================================
# Assumes dependencies are in subfolders relative to this script
option(IMGUI_DIR "" "${CMAKE_CURRENT_SOURCE_DIR}/../dependency/imgui")
option(IMPLOT_DIR "" "${CMAKE_CURRENT_SOURCE_DIR}/../dependency/implot")
option(INCLUDE_DIR "" "${CMAKE_CURRENT_SOURCE_DIR}/../src/libs/inc")

#==============================================================================
# Compiler Flags
#==============================================================================
if(MSVC)
    add_definitions(-DUNICODE -D_UNICODE) # Ensure Unicode character set
    
    # MSVC specific flags
    # /W4: High warning level, /MP: Multi-processor build, /utf-8: Source encoding
    add_compile_options(/MP /utf-8)
    
    set(CMAKE_CXX_FLAGS_DEBUG "/MDd /Zi /Ob0 /Od /RTC1")
    set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /Ob2 /DNDEBUG")
else()
    # GCC/Clang specific flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

    # Allow implicit fallthrough
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-implicit-fallthrough")
    endif()
endif()

# ==============================================================================
# Main Library Target
# ==============================================================================
add_library(imgui_static STATIC)

# 1. Core ImGui Sources
target_sources(imgui_static PRIVATE
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/misc/freetype/imgui_freetype.cpp
)

# 2. Core Includes (FreeType assumed present in include/freetype2)
target_include_directories(imgui_static PUBLIC
    ${IMGUI_DIR}
    ${INCLUDE_DIR}
    ${FREETYPE_INCLUDE_DIR}
)

# 3. Core Definitions
target_compile_definitions(imgui_static PUBLIC
    IMGUI_ENABLE_FREETYPE
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    IMGUI_ENABLE_OSX_DEFAULT_CLIPBOARD_FUNCTIONS
    IMGUI_DISABLE_DEMO_WINDOWS
    IMGUI_USE_WCHAR32
)

# ==============================================================================
# Platform Backend Selection
# ==============================================================================
if(BUILD_WITH_SDL3)
    message(STATUS "Configuring ImGui with SDL3 Backend...")
    
    # Sources
    target_sources(imgui_static PRIVATE
        ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdlgpu3.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp
    )
    
    # Definitions
    target_compile_definitions(imgui_static PUBLIC IMGUI_IMPL_SDL3)

elseif(BUILD_WITH_GLFW)
    message(STATUS "Configuring ImGui with GLFW + OpenGL3 Backend...")
    
    # Sources
    target_sources(imgui_static PRIVATE
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    )
    
    # Definitions (Adjust loader if using glad/glew, default to internal if needed)
    target_compile_definitions(imgui_static PUBLIC IMGUI_IMPL_OPENGL_USE_LOADER) 

else()
    message(WARNING "No backend selected. Building ImGui Core only.")
endif()

# ==============================================================================
# Optional: ImPlot
# ==============================================================================
if(BUILD_IMPLOT)
    message(STATUS "Including ImPlot...")
    
    target_sources(imgui_static PRIVATE
        ${IMPLOT_DIR}/implot.cpp
        ${IMPLOT_DIR}/implot_items.cpp
        ${IMPLOT_DIR}/implot_demo.cpp
    )
    
    target_include_directories(imgui_static PUBLIC ${IMPLOT_DIR})
endif()

# ==============================================================================
# Compiler Flags
# ==============================================================================
if(MSVC)
    target_compile_options(imgui_static PRIVATE
        /std:c++17
        /EHsc
        /MP  # Multi-processor compilation
    )
    # Debug/Release flags are handled automatically by CMake's CMAKE_CXX_FLAGS configuration
else()
    target_compile_options(imgui_static PRIVATE
        -std=c++17
        -fPIC # Important for linking static lib into shared libs later
    )
endif()

# ==============================================================================
# Output Directory
# ==============================================================================
set_target_properties(imgui_static PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

message(STATUS "Build configured for: ${CMAKE_BUILD_TYPE}")

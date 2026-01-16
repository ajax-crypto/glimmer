cmake_minimum_required(VERSION 3.15)

# Only allow building on Linux
if(NOT UNIX OR APPLE)
    message(FATAL_ERROR "This CMake script is configured for Linux only")
endif()

project(Glimmer VERSION 1.0.0 LANGUAGES CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

#==============================================================================
# Platform Configuration
#==============================================================================
set(GLIMMER_PLATFORM "sdl3" CACHE STRING "Target platform: test, tui, sdl3, glfw")

# Validate platform
if(NOT GLIMMER_PLATFORM MATCHES "^(test|tui|sdl3|glfw)$")
    message(FATAL_ERROR "Invalid GLIMMER_PLATFORM: ${GLIMMER_PLATFORM}. Must be: test, tui, sdl3, or glfw")
endif()

# Map platform to library name and define
if(GLIMMER_PLATFORM STREQUAL "test")
    set(LIBRARY_NAME "glimmer_test")
    set(PLATFORM_DEFINE "GLIMMER_PLATFORM_TEST=-1")
elseif(GLIMMER_PLATFORM STREQUAL "tui")
    set(LIBRARY_NAME "glimmer_tui")
    set(PLATFORM_DEFINE "GLIMMER_PLATFORM_PDCURSES=0")
elseif(GLIMMER_PLATFORM STREQUAL "sdl3")
    set(LIBRARY_NAME "glimmer_sdl3")
    set(PLATFORM_DEFINE "GLIMMER_PLATFORM_SDL3=1")
elseif(GLIMMER_PLATFORM STREQUAL "glfw")
    set(LIBRARY_NAME "glimmer_glfw")
    set(PLATFORM_DEFINE "GLIMMER_PLATFORM_GLFW=2")
endif()

message(STATUS "Platform: ${GLIMMER_PLATFORM}")
message(STATUS "Library name: lib${LIBRARY_NAME}.a")

#==============================================================================
# Compiler Flags
#==============================================================================
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Allow implicit fallthrough
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-implicit-fallthrough")
endif()

#==============================================================================
# Feature Options (can override platform defaults)
#==============================================================================
option(GLIMMER_DISABLE_SVG "Disable SVG rendering support" OFF)
option(GLIMMER_DISABLE_IMAGES "Disable image loading support" OFF)
option(GLIMMER_DISABLE_RICHTEXT "Disable rich text rendering" OFF)
option(GLIMMER_DISABLE_PLOTS "Disable plotting/graph library integration" OFF)
option(GLIMMER_ENABLE_NFDEXT "Enable nfd-extended library for file pickers" OFF)
option(GLIMMER_ENABLE_BLEND2D "Enable Blend2D renderer (requires libblend2d.a)" OFF)
option(GLIMMER_FORCE_UPDATE "Force dependency refresh (from build.sh)" OFF)

# Configure compile definitions
add_compile_definitions(${PLATFORM_DEFINE})
add_compile_definitions(GLIMMER_PLATFORM=${GLIMMER_PLATFORM})

if(GLIMMER_DISABLE_SVG)
    add_compile_definitions(GLIMMER_DISABLE_SVG)
endif()

if(GLIMMER_DISABLE_IMAGES)
    add_compile_definitions(GLIMMER_DISABLE_IMAGES)
endif()

if(GLIMMER_DISABLE_RICHTEXT)
    add_compile_definitions(GLIMMER_DISABLE_RICHTEXT)
endif()

if(GLIMMER_DISABLE_PLOTS)
    add_compile_definitions(GLIMMER_DISABLE_PLOTS)
endif()

if(GLIMMER_ENABLE_NFDEXT)
    add_compile_definitions(GLIMMER_ENABLE_NFDEXT)
endif()

if(NOT GLIMMER_ENABLE_BLEND2D)
    add_compile_definitions(GLIMMER_DISABLE_BLEND2D_RENDERER)
endif()

if(GLIMMER_FORCE_UPDATE)
    add_compile_definitions(GLIMMER_FORCE_UPDATE)
endif()

# ImRichText target is always ImGui in Glimmer
add_compile_definitions(IM_RICHTEXT_TARGET_IMGUI)

#==============================================================================
# Source Files
#==============================================================================
set(GLIMMER_SOURCES
    src/context.cpp
    src/draw.cpp
    src/im_font_manager.cpp
    src/layout.cpp
    src/platform.cpp
    src/renderer.cpp
    src/style.cpp
    src/widgets.cpp
)

# ImRichText sources (conditional)
set(EXTRA_SOURCES)
if(NOT GLIMMER_DISABLE_RICHTEXT)
    list(APPEND EXTRA_SOURCES
        src/libs/src/imrichtext.cpp
        src/libs/src/imrichtextimpl.cpp
        src/libs/src/imrichtextutils.cpp
    )
endif()

# Platform-specific implementation sources (from downloaded ImGui backends)
if(GLIMMER_PLATFORM STREQUAL "sdl3")
    list(APPEND EXTRA_SOURCES
        src/libs/inc/imgui/backends/imgui_impl_sdl3.cpp
        src/libs/inc/imgui/backends/imgui_impl_opengl3.cpp
    )
elseif(GLIMMER_PLATFORM STREQUAL "glfw")
    list(APPEND EXTRA_SOURCES
        src/libs/inc/imgui/backends/imgui_impl_glfw.cpp
        src/libs/inc/imgui/backends/imgui_impl_opengl3.cpp
    )
endif()

# Combine all sources
set(ALL_SOURCES ${GLIMMER_SOURCES} ${EXTRA_SOURCES})

#==============================================================================
# Create Static Library
#==============================================================================
add_library(${LIBRARY_NAME} STATIC ${ALL_SOURCES})

# Define ImGui with FreeType and 32-bit Unicode support (for emoji)
target_compile_definitions(${LIBRARY_NAME} PRIVATE 
    IMGUI_ENABLE_FREETYPE
    IMGUI_USE_WCHAR32
)

#==============================================================================
# Include Directories
#==============================================================================
target_include_directories(${LIBRARY_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/imrichtext
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/freetype2
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/yoga
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/lunasvg
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/stb_image
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/implot
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/iconfonts
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/nfd-ext
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/SDL3
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/GLFW
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/imguisdl3
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/randrew.layout
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/blend2d
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/nlohmann
)

#==============================================================================
# System Libraries
#==============================================================================
find_package(Threads REQUIRED)

# OpenGL (required for graphical platforms)
if(GLIMMER_PLATFORM MATCHES "sdl3|glfw")
    find_package(OpenGL REQUIRED)
    target_link_libraries(${LIBRARY_NAME} PUBLIC OpenGL::GL)
endif()

# X11 (required for Linux GUI platforms)
if(GLIMMER_PLATFORM MATCHES "sdl3|glfw|tui")
    find_package(X11 REQUIRED)
    target_link_libraries(${LIBRARY_NAME} PUBLIC ${X11_LIBRARIES})
endif()

# Standard libraries
target_link_libraries(${LIBRARY_NAME} PUBLIC
    Threads::Threads
    ${CMAKE_DL_LIBS}
    m  # Math library
)

#==============================================================================
# Platform-Specific Dependency Libraries
#==============================================================================
# Determine library subdirectory path
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(LIB_SUBDIR "debug")
else()
    set(LIB_SUBDIR "release")
endif()

set(PLATFORM_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/libs/lib/linux/${LIB_SUBDIR}/${GLIMMER_PLATFORM}")

message(STATUS "Looking for libraries in: ${PLATFORM_LIB_DIR}")

# Core dependencies (required for all platforms)
set(CORE_LIBS
    libimgui.a
    libfreetype.a
    libyoga.a
)

foreach(lib ${CORE_LIBS})
    set(lib_path "${PLATFORM_LIB_DIR}/${lib}")
    if(EXISTS "${lib_path}")
        target_link_libraries(${LIBRARY_NAME} PUBLIC "${lib_path}")
        message(STATUS "✓ Linking ${lib}")
    else()
        message(FATAL_ERROR "Required library not found: ${lib_path}")
    endif()
endforeach()

# Platform-specific dependencies
if(GLIMMER_PLATFORM STREQUAL "test")
    # Minimal: no additional libraries
    
elseif(GLIMMER_PLATFORM STREQUAL "tui")
    # Terminal UI: pdcurses
    if(EXISTS "${PLATFORM_LIB_DIR}/libpdcurses.a")
        target_link_libraries(${LIBRARY_NAME} PUBLIC "${PLATFORM_LIB_DIR}/libpdcurses.a")
        message(STATUS "✓ Linking libpdcurses.a")
    else()
        message(FATAL_ERROR "libpdcurses.a not found for TUI platform")
    endif()
    
elseif(GLIMMER_PLATFORM STREQUAL "sdl3")
    # SDL3 platform: SDL3 + SVG + plots + blend2d
    set(SDL3_LIBS
        libSDL3.a
        libplutovg.a
        liblunasvg.a
        libimplot.a
    )
    
    foreach(lib ${SDL3_LIBS})
        set(lib_path "${PLATFORM_LIB_DIR}/${lib}")
        if(EXISTS "${lib_path}")
            target_link_libraries(${LIBRARY_NAME} PUBLIC "${lib_path}")
            message(STATUS "✓ Linking ${lib}")
        else()
            message(WARNING "${lib} not found (may be optional)")
        endif()
    endforeach()
    
    # Blend2D (if enabled)
    if(GLIMMER_ENABLE_BLEND2D)
        if(EXISTS "${PLATFORM_LIB_DIR}/libblend2d.a")
            target_link_libraries(${LIBRARY_NAME} PUBLIC "${PLATFORM_LIB_DIR}/libblend2d.a")
            message(STATUS "✓ Linking libblend2d.a")
        else()
            message(WARNING "libblend2d.a requested but not found")
        endif()
    endif()
    
elseif(GLIMMER_PLATFORM STREQUAL "glfw")
    # GLFW platform: GLFW + SVG + plots + nfd-ext
    set(GLFW_LIBS
        libglfw3.a
        libplutovg.a
        liblunasvg.a
        libimplot.a
    )
    
    foreach(lib ${GLFW_LIBS})
        set(lib_path "${PLATFORM_LIB_DIR}/${lib}")
        if(EXISTS "${lib_path}")
            target_link_libraries(${LIBRARY_NAME} PUBLIC "${lib_path}")
            message(STATUS "✓ Linking ${lib}")
        else()
            message(WARNING "${lib} not found (may be optional)")
        endif()
    endforeach()
    
    # NFD-Extended (if enabled)
    if(GLIMMER_ENABLE_NFDEXT)
        if(EXISTS "${PLATFORM_LIB_DIR}/libnfd.a")
            target_link_libraries(${LIBRARY_NAME} PUBLIC "${PLATFORM_LIB_DIR}/libnfd.a")
            message(STATUS "✓ Linking libnfd.a")
        else()
            message(WARNING "libnfd.a requested but not found")
        endif()
    endif()
endif()

#==============================================================================
# Output Configuration
#==============================================================================
set_target_properties(${LIBRARY_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/staticlib"
    OUTPUT_NAME "${LIBRARY_NAME}"
    POSITION_INDEPENDENT_CODE ON
)

#==============================================================================
# Installation Rules
#==============================================================================
install(TARGETS ${LIBRARY_NAME}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES glimmer.h DESTINATION include)
install(FILES src/platform_config.h DESTINATION include/glimmer)
install(DIRECTORY src/
    DESTINATION include/glimmer
    FILES_MATCHING PATTERN "*.h"
)

#==============================================================================
# Configuration Summary
#==============================================================================
message(STATUS "")
message(STATUS "========================================")
message(STATUS "Glimmer Static Library Configuration")
message(STATUS "========================================")
message(STATUS "  Platform: ${GLIMMER_PLATFORM}")
message(STATUS "  Library: lib${LIBRARY_NAME}.a")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Dependencies: ${PLATFORM_LIB_DIR}")
message(STATUS "")
message(STATUS "Feature Flags:")
message(STATUS "  Disable SVG: ${GLIMMER_DISABLE_SVG}")
message(STATUS "  Disable Images: ${GLIMMER_DISABLE_IMAGES}")
message(STATUS "  Disable RichText: ${GLIMMER_DISABLE_RICHTEXT}")
message(STATUS "  Disable Plots: ${GLIMMER_DISABLE_PLOTS}")
message(STATUS "  Enable NFD-Ext: ${GLIMMER_ENABLE_NFDEXT}")
message(STATUS "  Enable Blend2D: ${GLIMMER_ENABLE_BLEND2D}")
message(STATUS "  Force Update: ${GLIMMER_FORCE_UPDATE}")
message(STATUS "")
message(STATUS "Output: ${CMAKE_CURRENT_SOURCE_DIR}/staticlib/lib${LIBRARY_NAME}.a")
message(STATUS "========================================")
message(STATUS "")

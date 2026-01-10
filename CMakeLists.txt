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

# Compiler flags for Linux
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Allow implicit fallthrough
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-implicit-fallthrough")
endif()

# Optional features (can be disabled by setting these to ON)
option(GLIMMER_DISABLE_SVG "Disable SVG rendering support" OFF)
option(GLIMMER_DISABLE_IMAGES "Disable image loading support" OFF)
option(GLIMMER_DISABLE_RICHTEXT "Disable rich text rendering" OFF)
option(GLIMMER_DISABLE_PLOTS "Disable plotting/graph library integration" OFF)
option(GLIMMER_ENABLE_NFDEXT "Enable nfd-extended library for file pickers" OFF)
option(GLIMMER_ENABLE_BLEND2D "Enable Blend2D renderer (requires libblend2d.a)" OFF)

# Configure compile definitions based on options
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

# Configure blend2d renderer based on option
if(NOT GLIMMER_ENABLE_BLEND2D)
    add_compile_definitions(GLIMMER_DISABLE_BLEND2D_RENDERER)
endif()

# ImRichText target is always ImGui in Glimmer (blend2d is used separately)
add_compile_definitions(IM_RICHTEXT_TARGET_IMGUI)

# Source files for the main glimmer library
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

# Header files
set(GLIMMER_HEADERS
    src/context.h
    src/draw.h
    src/im_font_manager.h
    src/layout.h
    src/platform.h
    src/renderer.h
    src/style.h
    src/types.h
    src/utils.h
    src/widgets.h
    src/glimmer.h
    src/custom/PathInput.h
    glimmer.h
)

# ImRichText sources (only these, ImGui is prebuilt)
set(EXTRA_SOURCES)
if(NOT GLIMMER_DISABLE_RICHTEXT)
    list(APPEND EXTRA_SOURCES
        src/libs/src/imrichtext.cpp
        src/libs/src/imrichtextimpl.cpp
        src/libs/src/imrichtextutils.cpp
    )
endif()

# Platform implementation sources
list(APPEND EXTRA_SOURCES
    src/libs/src/imgui_impl_glfw.cpp
    src/libs/src/imgui_impl_opengl3.cpp
)

# Combine all sources
set(ALL_SOURCES ${GLIMMER_SOURCES} ${EXTRA_SOURCES})

# Create static library
add_library(glimmer STATIC ${ALL_SOURCES})

# Include directories
target_include_directories(glimmer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/imrichtext
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/freetype2
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/GLFW
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/yoga
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/lunasvg
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/stb_image
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/implot
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/iconfonts
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/nfd-ext
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/SDL3
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/imguisdl3
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/randrew.layout
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/blend2d
)

# Find required system libraries for Linux
find_package(Threads REQUIRED)

# Find FreeType (if not using bundled version)
find_package(Freetype QUIET)
if(Freetype_FOUND)
    target_link_libraries(glimmer PUBLIC Freetype::Freetype)
    message(STATUS "Using system FreeType library")
else()
    message(STATUS "System FreeType not found, using bundled headers")
endif()

# Find OpenGL (required for rendering)
find_package(OpenGL QUIET)
if(OpenGL_FOUND)
    target_link_libraries(glimmer PUBLIC OpenGL::GL)
    message(STATUS "OpenGL found")
endif()

# Find GLFW (if needed)
find_package(glfw3 QUIET)
if(glfw3_FOUND)
    target_link_libraries(glimmer PUBLIC glfw)
    message(STATUS "GLFW3 found")
endif()

# Link with standard libraries
target_link_libraries(glimmer PUBLIC
    Threads::Threads
    ${CMAKE_DL_LIBS}
)

# Linux-specific libraries
if(UNIX AND NOT APPLE)
    target_link_libraries(glimmer PUBLIC
        m  # Math library
    )
    
    # X11 libraries (required for SDL3 with X11 support)
    find_package(X11)
    if(X11_FOUND)
        target_link_libraries(glimmer PUBLIC ${X11_LIBRARIES})
        message(STATUS "Linking X11 libraries for SDL3")
    endif()
    
    # Link against bundled static libraries for Linux
    set(LINUX_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/libs/lib/linux")
    
    # Determine debug or release
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(LIB_SUBDIR "debug")
    else()
        set(LIB_SUBDIR "release")
    endif()
    
    # Link plutovg (required by lunasvg)
    if(EXISTS "${LINUX_LIB_DIR}/${LIB_SUBDIR}/libplutovg.a")
        target_link_libraries(glimmer PUBLIC "${LINUX_LIB_DIR}/${LIB_SUBDIR}/libplutovg.a")
        message(STATUS "Linking bundled plutovg (${LIB_SUBDIR})")
    else()
        message(WARNING "plutovg not found in ${LINUX_LIB_DIR}/${LIB_SUBDIR}/")
    endif()
    
    # Link lunasvg (for SVG support)
    if(EXISTS "${LINUX_LIB_DIR}/${LIB_SUBDIR}/liblunasvg.a")
        target_link_libraries(glimmer PUBLIC "${LINUX_LIB_DIR}/${LIB_SUBDIR}/liblunasvg.a")
        message(STATUS "Linking bundled lunasvg (${LIB_SUBDIR})")
    else()
        message(WARNING "lunasvg not found in ${LINUX_LIB_DIR}/${LIB_SUBDIR}/")
    endif()
    
    # Link SDL3 (optional - for platform/windowing)
    if(EXISTS "${LINUX_LIB_DIR}/${LIB_SUBDIR}/libSDL3.a")
        target_link_libraries(glimmer PUBLIC "${LINUX_LIB_DIR}/${LIB_SUBDIR}/libSDL3.a")
        message(STATUS "Linking bundled SDL3 (${LIB_SUBDIR})")
    else()
        message(STATUS "SDL3 not found (optional)")
    endif()
    
    # Link FreeType (for font rendering)
    if(EXISTS "${LINUX_LIB_DIR}/${LIB_SUBDIR}/libfreetype.a")
        target_link_libraries(glimmer PUBLIC "${LINUX_LIB_DIR}/${LIB_SUBDIR}/libfreetype.a")
        message(STATUS "Linking bundled FreeType (${LIB_SUBDIR})")
    else()
        message(WARNING "FreeType not found in ${LINUX_LIB_DIR}/${LIB_SUBDIR}/")
    endif()
    
    # Link ImGui (for immediate mode GUI)
    if(EXISTS "${LINUX_LIB_DIR}/${LIB_SUBDIR}/libimgui.a")
        target_link_libraries(glimmer PUBLIC "${LINUX_LIB_DIR}/${LIB_SUBDIR}/libimgui.a")
        message(STATUS "Linking bundled ImGui (${LIB_SUBDIR})")
    else()
        message(WARNING "ImGui not found in ${LINUX_LIB_DIR}/${LIB_SUBDIR}/")
    endif()
    
    # Link ImPlot (for plotting support)
    if(EXISTS "${LINUX_LIB_DIR}/${LIB_SUBDIR}/libimplot.a")
        target_link_libraries(glimmer PUBLIC "${LINUX_LIB_DIR}/${LIB_SUBDIR}/libimplot.a")
        message(STATUS "Linking bundled ImPlot (${LIB_SUBDIR})")
    else()
        message(WARNING "ImPlot not found in ${LINUX_LIB_DIR}/${LIB_SUBDIR}/")
    endif()
    
    # Link Yoga (for flexbox layout)
    if(EXISTS "${LINUX_LIB_DIR}/${LIB_SUBDIR}/libyogacore.a")
        target_link_libraries(glimmer PUBLIC "${LINUX_LIB_DIR}/${LIB_SUBDIR}/libyogacore.a")
        message(STATUS "Linking bundled Yoga (${LIB_SUBDIR})")
    else()
        message(WARNING "Yoga not found in ${LINUX_LIB_DIR}/${LIB_SUBDIR}/")
    endif()
    
    # Link blend2d (if enabled)
    if(GLIMMER_ENABLE_BLEND2D)
        if(EXISTS "${LINUX_LIB_DIR}/${LIB_SUBDIR}/libblend2d.a")
            target_link_libraries(glimmer PUBLIC "${LINUX_LIB_DIR}/${LIB_SUBDIR}/libblend2d.a")
            message(STATUS "Linking bundled blend2d (${LIB_SUBDIR})")
        else()
            message(WARNING "blend2d requested but not found in ${LINUX_LIB_DIR}/${LIB_SUBDIR}/")
        endif()
    endif()
endif()

# Set output directory for the static library
set_target_properties(glimmer PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/staticlib"
    OUTPUT_NAME "glimmer"
    POSITION_INDEPENDENT_CODE ON
)

# Installation rules (optional)
install(TARGETS glimmer
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES glimmer.h DESTINATION include)
install(DIRECTORY src/
    DESTINATION include/glimmer
    FILES_MATCHING PATTERN "*.h"
)

# Print configuration summary
message(STATUS "")
message(STATUS "Glimmer Static Library Configuration:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Disable SVG: ${GLIMMER_DISABLE_SVG}")
message(STATUS "  Disable Images: ${GLIMMER_DISABLE_IMAGES}")
message(STATUS "  Disable RichText: ${GLIMMER_DISABLE_RICHTEXT}")
message(STATUS "  Disable Plots: ${GLIMMER_DISABLE_PLOTS}")
message(STATUS "  Enable NFD-Ext: ${GLIMMER_ENABLE_NFDEXT}")
message(STATUS "  Enable Blend2D: ${GLIMMER_ENABLE_BLEND2D}")
message(STATUS "  Output: ${CMAKE_CURRENT_SOURCE_DIR}/staticlib/libglimmer.a")
message(STATUS "")


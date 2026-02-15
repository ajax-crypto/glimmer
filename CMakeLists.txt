cmake_minimum_required(VERSION 3.24)

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
# OS & Path Configuration
#==============================================================================
if(EMSCRIPTEN)
    # ===== NEW: Emscripten Configuration =====
    set(GLIMMER_OS_DIR "emscripten")
    set(LIB_EXT ".a")
    set(LIB_PREFIX "lib")
    message(STATUS "Building for Emscripten (WebAssembly)")
    
elseif(WIN32)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    set(GLIMMER_OS_DIR "win32")
    set(LIB_EXT ".lib")
    set(LIB_PREFIX "")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DUNICODE -D_UNICODE)
    
elseif(UNIX)
    set(GLIMMER_OS_DIR "linux")
    set(LIB_EXT ".a")
    set(LIB_PREFIX "lib")
    
else()
    message(FATAL_ERROR "Unsupported platform. This script supports Linux, Windows, and Emscripten only.")
endif()

#==============================================================================
# Platform Configuration
#==============================================================================
set(GLIMMER_PLATFORM "sdl3" CACHE STRING "Target platform: test, tui, sdl3, glfw")

# Validate platform
if(NOT GLIMMER_PLATFORM MATCHES "^(test|tui|sdl3|glfw)$")
    message(FATAL_ERROR "Invalid GLIMMER_PLATFORM: ${GLIMMER_PLATFORM}. Must be: test, tui, sdl3, or glfw")
endif()

# ===== NEW: Emscripten can only use SDL3 platform =====
if(EMSCRIPTEN AND NOT GLIMMER_PLATFORM STREQUAL "sdl3")
    message(WARNING "Emscripten only supports sdl3 platform. Forcing GLIMMER_PLATFORM=sdl3")
    set(GLIMMER_PLATFORM "sdl3")
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

message(STATUS "Platform: ${GLIMMER_PLATFORM} (${GLIMMER_OS_DIR})")

#==============================================================================
# Compiler Flags
#==============================================================================
if(EMSCRIPTEN)
    # ===== NEW: Emscripten-specific flags =====
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter")
    
    # Emscripten linker flags
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} \
        -sUSE_SDL=3 \
        -sUSE_WEBGPU=1 \
        -sALLOW_MEMORY_GROWTH=1 \
        -sMAXIMUM_MEMORY=4GB \
        -sFORCE_FILESYSTEM=1 \
        -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap'] \
        --preload-file assets@/assets \
    ")
    
    # Debug vs Release
    if(CMAKE_BUILD_TYPE MATCHES "Debug")
        set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -sASSERTIONS=1")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -sASSERTIONS=1 -sSAFE_HEAP=1")
    else()
        set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O3")
    endif()
    
elseif(MSVC)
    # MSVC specific flags
    add_compile_options(/MP /utf-8)
    set(CMAKE_CXX_FLAGS_DEBUG "/MDd /Zi /Ob0 /Od /RTC1")
    set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /Ob2 /DNDEBUG")
    
else()
    # GCC/Clang specific flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -Og")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -fno-math-errno -fdevirtualize-speculatively")

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-implicit-fallthrough")
    endif()
endif()

#==============================================================================
# Feature Options
#==============================================================================
option(GLIMMER_DISABLE_SVG "Disable SVG rendering support" OFF)
option(GLIMMER_DISABLE_IMAGES "Disable image loading support" OFF)
option(GLIMMER_DISABLE_RICHTEXT "Disable rich text rendering" OFF)
option(GLIMMER_DISABLE_PLOTS "Disable plotting/graph library integration" OFF)
option(GLIMMER_ENABLE_NFDEXT "Enable nfd-extended library for file pickers" OFF)
option(GLIMMER_ENABLE_BLEND2D "Enable Blend2D renderer" OFF)
option(GLIMMER_FORCE_UPDATE "Force dependency refresh" OFF)

# ===== NEW: Disable features not supported in Emscripten =====
if(EMSCRIPTEN)
    set(GLIMMER_ENABLE_NFDEXT OFF CACHE BOOL "NFD not supported in Emscripten" FORCE)
    set(GLIMMER_ENABLE_BLEND2D OFF CACHE BOOL "Blend2D not needed in Emscripten" FORCE)
endif()

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
    src/custom/PathInput.cpp
)

set(EXTRA_SOURCES)
if(NOT GLIMMER_DISABLE_RICHTEXT)
    list(APPEND EXTRA_SOURCES
        src/imrichtext/imrichtext.cpp
        src/imrichtext/imrichtextimpl.cpp
        src/imrichtext/imrichtextutils.cpp
    )
endif()

set(ALL_SOURCES ${GLIMMER_SOURCES} ${EXTRA_SOURCES})

#==============================================================================
# Visual Studio Code Organization
#==============================================================================
if (WIN32)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    
    set(GLIMMER_HEADERS
        src/context.h
        src/draw.h
        src/im_font_manager.h
        src/layout.h
        src/platform.h
        src/renderer.h
        src/style.h
        src/widgets.h
        src/custom/PathInput.h
    )

    set(EXTRA_HEADERS)
    if(NOT GLIMMER_DISABLE_RICHTEXT)
        list(APPEND EXTRA_HEADERS
            src/imrichtext/imrichtext.h
            src/imrichtext/imrichtextimpl.h
            src/imrichtext/imrichtextutils.h
        )
    endif()

    set(ALL_HEADERS ${GLIMMER_HEADERS} ${EXTRA_HEADERS})

    source_group("Header Files" FILES ${ALL_HEADERS})
    source_group("Source Files" FILES ${ALL_SOURCES})
endif()

#==============================================================================
# Create Static Library
#==============================================================================
add_library(${LIBRARY_NAME} STATIC ${ALL_HEADERS} ${ALL_SOURCES})

target_compile_definitions(${LIBRARY_NAME} PRIVATE 
    IMGUI_ENABLE_FREETYPE
    IMGUI_USE_WCHAR32
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    IMGUI_DISABLE_DEMO_WINDOWS
    GLIMMER_DISABLE_BLEND2D_RENDERER
)

#==============================================================================
# Include Directories
#==============================================================================
target_include_directories(${LIBRARY_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/imrichtext
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/blend2d
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/nlohmann
)

# ===== NEW: Add WebGPU headers for Emscripten =====
if(EMSCRIPTEN)
    target_include_directories(${LIBRARY_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/inc/webgpu
    )
endif()

#==============================================================================
# System Libraries
#==============================================================================
if(NOT EMSCRIPTEN)
    find_package(Threads REQUIRED)
    target_link_libraries(${LIBRARY_NAME} PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
endif()

# OpenGL (required for GLFW platform on native)
if(GLIMMER_PLATFORM MATCHES "glfw" AND NOT EMSCRIPTEN)
    find_package(OpenGL REQUIRED)
    target_link_libraries(${LIBRARY_NAME} PUBLIC OpenGL::GL)
endif()

# OS Specific System Libs
if(WIN32)
    # Windows System Libraries required by SDL3/GLFW
    target_link_libraries(${LIBRARY_NAME} PUBLIC
        user32 gdi32 shell32 advapi32 ole32 oleaut32 imm32 version winmm setupapi shlwapi
    )
elseif(UNIX AND NOT EMSCRIPTEN AND GLIMMER_PLATFORM MATCHES "sdl3|glfw")
    # Linux X11
    find_package(X11 REQUIRED)
    target_link_libraries(${LIBRARY_NAME} PUBLIC ${X11_LIBRARIES})
    target_link_libraries(${LIBRARY_NAME} PUBLIC m)
endif()

#==============================================================================
# Platform-Specific Dependency Libraries
#==============================================================================
if(CMAKE_BUILD_TYPE MATCHES "(Debug|DEBUG|debug)")
    set(LIB_SUBDIR "debug")
else()
    set(LIB_SUBDIR "release")
endif()

# Path now uses GLIMMER_OS_DIR (win32, linux, or emscripten)
set(PLATFORM_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/libs/lib/${GLIMMER_OS_DIR}/${LIB_SUBDIR}")

message(STATUS "Looking for libraries in: ${PLATFORM_LIB_DIR}")

# ===== MODIFIED: Skip library linking for Emscripten (uses USE_SDL=3, USE_WEBGPU=1) =====
if(NOT EMSCRIPTEN)
    # Define Core Libs based on Platform naming convention
    if(WIN32)
        if(EXISTS "${PLATFORM_LIB_DIR}/imgui_static.lib")
            set(IMGUI_LIB_NAME "imgui_static")
        else()
            set(IMGUI_LIB_NAME "libimgui_static")
        endif()
        set(CORE_LIBS ${IMGUI_LIB_NAME} freetype yoga)
    else()
        if(EXISTS "${PLATFORM_LIB_DIR}/libimgui_static.a")
            set(IMGUI_LIB_NAME "libimgui_static")
        else()
            set(IMGUI_LIB_NAME "libimgui")
        endif()
        set(CORE_LIBS ${IMGUI_LIB_NAME} libfreetype libyoga)
    endif()

    # Link Core Libs
    foreach(lib_base ${CORE_LIBS})
        if(WIN32)
            set(full_name "${lib_base}${LIB_EXT}")
        else()
            set(full_name "${lib_base}${LIB_EXT}")
        endif()

        set(lib_path "${PLATFORM_LIB_DIR}/${full_name}")
        if(EXISTS "${lib_path}")
            target_link_libraries(${LIBRARY_NAME} PUBLIC "${lib_path}")
            message(STATUS "✓ Linking ${full_name}")
        else()
            message(FATAL_ERROR "Required library not found: ${lib_path}")
        endif()
    endforeach()

    # Platform-specific dependencies
    if(GLIMMER_PLATFORM STREQUAL "test")
        # No extra libs

    elseif(GLIMMER_PLATFORM STREQUAL "tui")
        # PDCurses
        set(tui_lib_name "${LIB_PREFIX}pdcurses${LIB_EXT}")
        if(EXISTS "${PLATFORM_LIB_DIR}/${tui_lib_name}")
            target_link_libraries(${LIBRARY_NAME} PUBLIC "${PLATFORM_LIB_DIR}/${tui_lib_name}")
            message(STATUS "✓ Linking ${tui_lib_name}")
        else()
            message(FATAL_ERROR "${tui_lib_name} not found")
        endif()

    elseif(GLIMMER_PLATFORM STREQUAL "sdl3")
        # SDL3 + SVG + Plots
        if(WIN32)
            set(SDL3_LIBS SDL3 plutovg lunasvg imgui_static)
        else()
            set(SDL3_LIBS libSDL3 liblunasvg libimgui_static)
        endif()

        foreach(lib_base ${SDL3_LIBS})
            set(full_name "${lib_base}${LIB_EXT}")
            set(lib_path "${PLATFORM_LIB_DIR}/${full_name}")
            if(EXISTS "${lib_path}")
                target_link_libraries(${LIBRARY_NAME} PUBLIC "${lib_path}")
                message(STATUS "✓ Linking ${full_name}")
            else()
                message(WARNING "${full_name} not found (may be optional)")
            endif()
        endforeach()

        # Blend2D
        if(UNIX)
            if(GLIMMER_ENABLE_BLEND2D)
                set(b2d_name "${LIB_PREFIX}blend2d${LIB_EXT}")
                if(EXISTS "${PLATFORM_LIB_DIR}/${b2d_name}")
                    target_link_libraries(${LIBRARY_NAME} PUBLIC "${PLATFORM_LIB_DIR}/${b2d_name}")
                    message(STATUS "✓ Linking ${b2d_name}")
                else()
                    message(WARNING "Blend2D requested but ${b2d_name} not found")
                endif()
            endif()
        endif()

    elseif(GLIMMER_PLATFORM STREQUAL "glfw")
        # GLFW + SVG + Plots + NFD
        if(WIN32)
            set(GLFW_LIBS glfw3 plutovg lunasvg imgui_static)
        else()
            set(GLFW_LIBS libglfw3 libplutovg liblunasvg libimgui)
        endif()

        foreach(lib_base ${GLFW_LIBS})
            set(full_name "${lib_base}${LIB_EXT}")
            set(lib_path "${PLATFORM_LIB_DIR}/${full_name}")
            if(EXISTS "${lib_path}")
                target_link_libraries(${LIBRARY_NAME} PUBLIC "${lib_path}")
                message(STATUS "✓ Linking ${full_name}")
            else()
                message(WARNING "${full_name} not found (may be optional)")
            endif()
        endforeach()

        # NFD
        if(GLIMMER_ENABLE_NFDEXT)
            set(nfd_name "${LIB_PREFIX}nfd${LIB_EXT}")
            if(EXISTS "${PLATFORM_LIB_DIR}/${nfd_name}")
                target_link_libraries(${LIBRARY_NAME} PUBLIC "${PLATFORM_LIB_DIR}/${nfd_name}")
                message(STATUS "✓ Linking ${nfd_name}")
            else()
                message(WARNING "NFD requested but ${nfd_name} not found")
            endif()
        endif()
    endif()
else()
    # ===== NEW: Emscripten uses port system, no static libs needed =====
    message(STATUS "Emscripten build: Using -sUSE_SDL=3 and -sUSE_WEBGPU=1 (no static libs needed)")
endif()

#==============================================================================
# Output & Installation
#==============================================================================
set_target_properties(${LIBRARY_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/staticlib"
    OUTPUT_NAME "${LIBRARY_NAME}"
    POSITION_INDEPENDENT_CODE ON
)

install(TARGETS ${LIBRARY_NAME}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES glimmer.h DESTINATION include)
install(DIRECTORY src/ DESTINATION include/glimmer FILES_MATCHING PATTERN "*.h")

message(STATUS "")
message(STATUS "========================================")
message(STATUS "Glimmer Static Library Configuration")
message(STATUS "========================================")
message(STATUS "  OS: ${GLIMMER_OS_DIR}")
message(STATUS "  Platform: ${GLIMMER_PLATFORM}")

# ===== MODIFIED: Show WebGPU info for Emscripten =====
if(EMSCRIPTEN)
    message(STATUS "  Renderer: WebGPU (via Emscripten)")
    message(STATUS "  IO: SDL3 (via Emscripten)")
else()
    message(STATUS "  Output: lib${LIBRARY_NAME}.a / ${LIBRARY_NAME}.lib")
endif()

message(STATUS "========================================")

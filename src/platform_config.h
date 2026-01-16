#pragma once

/**
 * Glimmer Platform Configuration
 * 
 * This header defines platform targets and feature availability.
 * The GLIMMER_PLATFORM macro is set by the build system (CMake).
 */

// Platform type definitions
#define GLIMMER_PLATFORM_TEST       -1  // Minimal testing (no backends)
#define GLIMMER_PLATFORM_PDCURSES    0  // Terminal UI (pdcurses + json)
#define GLIMMER_PLATFORM_SDL3        1  // SDL3 + Blend2D (default)
#define GLIMMER_PLATFORM_GLFW        2  // GLFW + NFD-ext

// Default platform if not specified
#ifndef GLIMMER_PLATFORM
    #define GLIMMER_PLATFORM GLIMMER_PLATFORM_SDL3
#endif

// Feature detection based on platform
#if GLIMMER_PLATFORM == GLIMMER_PLATFORM_TEST
    // Minimal test platform: imgui + freetype + yoga only
    #ifndef GLIMMER_HAS_SVG
        #define GLIMMER_HAS_SVG 0
    #endif
    #ifndef GLIMMER_HAS_PLOTS
        #define GLIMMER_HAS_PLOTS 0
    #endif
    #ifndef GLIMMER_HAS_IMAGES
        #define GLIMMER_HAS_IMAGES 0
    #endif
    #ifndef GLIMMER_HAS_ICONS
        #define GLIMMER_HAS_ICONS 0
    #endif
    #define GLIMMER_HAS_BACKENDS 0
    #define GLIMMER_PLATFORM_NAME "test"

#elif GLIMMER_PLATFORM == GLIMMER_PLATFORM_PDCURSES
    // Terminal UI platform: pdcurses + json
    #ifndef GLIMMER_HAS_SVG
        #define GLIMMER_HAS_SVG 0
    #endif
    #ifndef GLIMMER_HAS_PLOTS
        #define GLIMMER_HAS_PLOTS 0
    #endif
    #ifndef GLIMMER_HAS_IMAGES
        #define GLIMMER_HAS_IMAGES 0
    #endif
    #ifndef GLIMMER_HAS_ICONS
        #define GLIMMER_HAS_ICONS 0
    #endif
    #define GLIMMER_HAS_TERMINAL_UI 1
    #define GLIMMER_HAS_JSON 1
    #define GLIMMER_PLATFORM_NAME "tui"

#elif GLIMMER_PLATFORM == GLIMMER_PLATFORM_SDL3
    // Full SDL3 platform: all features + Blend2D
    #ifndef GLIMMER_HAS_SVG
        #define GLIMMER_HAS_SVG 1
    #endif
    #ifndef GLIMMER_HAS_PLOTS
        #define GLIMMER_HAS_PLOTS 1
    #endif
    #ifndef GLIMMER_HAS_IMAGES
        #define GLIMMER_HAS_IMAGES 1
    #endif
    #ifndef GLIMMER_HAS_ICONS
        #define GLIMMER_HAS_ICONS 1
    #endif
    #define GLIMMER_HAS_BLEND2D 1
    #define GLIMMER_HAS_BACKENDS 1
    #define GLIMMER_PLATFORM_NAME "sdl3"

#elif GLIMMER_PLATFORM == GLIMMER_PLATFORM_GLFW
    // GLFW platform: all features + native file dialogs
    #ifndef GLIMMER_HAS_SVG
        #define GLIMMER_HAS_SVG 1
    #endif
    #ifndef GLIMMER_HAS_PLOTS
        #define GLIMMER_HAS_PLOTS 1
    #endif
    #ifndef GLIMMER_HAS_IMAGES
        #define GLIMMER_HAS_IMAGES 1
    #endif
    #ifndef GLIMMER_HAS_ICONS
        #define GLIMMER_HAS_ICONS 1
    #endif
    #define GLIMMER_HAS_FILE_DIALOGS 1
    #define GLIMMER_HAS_BACKENDS 1
    #define GLIMMER_PLATFORM_NAME "glfw"

#else
    #error "Unknown GLIMMER_PLATFORM value"
#endif

// Feature defaults (can be overridden by build flags)
#ifndef GLIMMER_HAS_SVG
    #define GLIMMER_HAS_SVG 0
#endif

#ifndef GLIMMER_HAS_PLOTS
    #define GLIMMER_HAS_PLOTS 0
#endif

#ifndef GLIMMER_HAS_IMAGES
    #define GLIMMER_HAS_IMAGES 0
#endif

#ifndef GLIMMER_HAS_ICONS
    #define GLIMMER_HAS_ICONS 0
#endif

#ifndef GLIMMER_HAS_BACKENDS
    #define GLIMMER_HAS_BACKENDS 0
#endif

#ifndef GLIMMER_HAS_TERMINAL_UI
    #define GLIMMER_HAS_TERMINAL_UI 0
#endif

#ifndef GLIMMER_HAS_JSON
    #define GLIMMER_HAS_JSON 0
#endif

#ifndef GLIMMER_HAS_BLEND2D
    #define GLIMMER_HAS_BLEND2D 0
#endif

#ifndef GLIMMER_HAS_FILE_DIALOGS
    #define GLIMMER_HAS_FILE_DIALOGS 0
#endif

// Platform name macro
#ifndef GLIMMER_PLATFORM_NAME
    #define GLIMMER_PLATFORM_NAME "unknown"
#endif

// Helper macros for conditional compilation
#if GLIMMER_HAS_SVG
    #define GLIMMER_IF_SVG(code) code
#else
    #define GLIMMER_IF_SVG(code)
#endif

#if GLIMMER_HAS_PLOTS
    #define GLIMMER_IF_PLOTS(code) code
#else
    #define GLIMMER_IF_PLOTS(code)
#endif

#if GLIMMER_HAS_IMAGES
    #define GLIMMER_IF_IMAGES(code) code
#else
    #define GLIMMER_IF_IMAGES(code)
#endif







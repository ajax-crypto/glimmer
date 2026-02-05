#include <stdint.h>

#define GLIMMER_PLATFORM_TEST -1
#define GLIMMER_PLATFORM_PDCURSES 0
#define GLIMMER_PLATFORM_SDL3 1
#define GLIMMER_PLATFORM_GLFW 2
#define GLIMMER_PLATFORM_WASM 3 // Not implemented yet
// Keep adding new platforms here...

// ImWchar is now defined by ImGui (removed conflicting typedef)

#ifndef GLIMMER_MAX_GLFW_CLIPBOARD_TEXTSZ
#define GLIMMER_MAX_GLFW_CLIPBOARD_TEXTSZ 4096
#endif

// Default platform is SDL3, set it to any of the above platform defined
#ifndef GLIMMER_TARGET_PLATFORM
#define GLIMMER_TARGET_PLATFORM GLIMMER_PLATFORM_SDL3
#endif

// Determines how many color stops are possible in a gradient
#ifndef GLIMMER_MAX_COLORSTOPS
#define GLIMMER_MAX_COLORSTOPS 4
#endif

// Name of the default font family, redefine if compiling only for specific platforms
#ifndef GLIMMER_DEFAULT_FONTFAMILY
#define GLIMMER_DEFAULT_FONTFAMILY "default-font-family"
#endif

// Name of the default monospace font family, redefine if compiling only for specific platforms
#ifndef GLIMMER_MONOSPACE_FONTFAMILY
#define GLIMMER_MONOSPACE_FONTFAMILY "monospace-family"
#endif

// Maximum instances of each type of widgets that can be created in a frame
#ifndef GLIMMER_TOTAL_ID_SIZE
#define GLIMMER_TOTAL_ID_SIZE (1 << 16)
#endif

// Maximum number of regions in a splitter
#ifndef GLIMMER_MAX_SPLITTER_REGIONS
#define GLIMMER_MAX_SPLITTER_REGIONS 4
#endif

#ifndef GLIMMER_MAX_STYLE_STACKSZ
#define GLIMMER_MAX_STYLE_STACKSZ 16
#endif

#ifndef GLIMMER_STYLE_BUFSZ 
#define GLIMMER_STYLE_BUFSZ 4096
#endif

// Maximum stacking depth for widget specific styles
#ifndef GLIMMER_MAX_WIDGET_SPECIFIC_STYLES
#define GLIMMER_MAX_WIDGET_SPECIFIC_STYLES 4
#endif

#ifndef GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL
#define GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL 4
#endif

#ifndef GLIMMER_MAX_LAYOUT_NESTING 
#define GLIMMER_MAX_LAYOUT_NESTING 8
#endif

#ifndef GLIMMER_MAX_REGION_NESTING 
#define GLIMMER_MAX_REGION_NESTING 8
#endif

#ifndef GLIMMER_MAX_OVERLAYS
#define GLIMMER_MAX_OVERLAYS 32
#endif

#ifndef GLIMMER_GLOBAL_ANIMATION_FRAMETIME
#define GLIMMER_GLOBAL_ANIMATION_FRAMETIME 18
#endif

#ifndef GLIMMER_NKEY_ROLLOVER_MAX
#define GLIMMER_NKEY_ROLLOVER_MAX 8
#endif

#ifndef GLIMMER_MAX_STATIC_MEDIA_SZ
#define GLIMMER_MAX_STATIC_MEDIA_SZ 4096
#endif

#ifndef GLIMMER_IMGUI_MAINWINDOW_NAME
#define GLIMMER_IMGUI_MAINWINDOW_NAME "main-window"
#endif

#define GLIMMER_FLAT_ENGINE 0
#define GLIMMER_CLAY_ENGINE 1
#define GLIMMER_YOGA_ENGINE 2
#define GLIMMER_SIMPLE_FLEX_ENGINE 3

#ifndef GLIMMER_FLEXBOX_ENGINE
#define GLIMMER_FLEXBOX_ENGINE GLIMMER_YOGA_ENGINE
#elif GLIMMER_FLEXBOX_ENGINE != GLIMMER_YOGA_ENGINE
#error "Other layout enfines haven'tbeen tested throroughly..."
#endif

#ifdef _WIN32
#define WINDOWS_DEFAULT_FONT \
    "c:\\Windows\\Fonts\\segoeui.ttf", \
    "c:\\Windows\\Fonts\\segoeuil.ttf",\
    "c:\\Windows\\Fonts\\segoeuib.ttf",\
    "c:\\Windows\\Fonts\\segoeuii.ttf",\
    "c:\\Windows\\Fonts\\segoeuiz.ttf"

#define WINDOWS_DEFAULT_MONOFONT \
    "c:\\Windows\\Fonts\\consola.ttf",\
    "",\
    "c:\\Windows\\Fonts\\consolab.ttf",\
    "c:\\Windows\\Fonts\\consolai.ttf",\
    "c:\\Windows\\Fonts\\consolaz.ttf"

#elif __linux__
#define FEDORA_DEFAULT_FONT \
    "/usr/share/fonts/open-sans/OpenSans-Regular.ttf",\
    "/usr/share/fonts/open-sans/OpenSans-Light.ttf",\
    "/usr/share/fonts/open-sans/OpenSans-Bold.ttf",\
    "/usr/share/fonts/open-sans/OpenSans-Italic.ttf",\
    "/usr/share/fonts/open-sans/OpenSans-BoldItalic.ttf"

#define FEDORA_DEFAULT_MONOFONT \
    "/usr/share/fonts/liberation-mono/LiberationMono-Regular.ttf",\
    "",\
    "/usr/share/fonts/liberation-mono/LiberationMono-Bold.ttf",\
    "/usr/share/fonts/liberation-mono/LiberationMono-Italic.ttf",\
    "/usr/share/fonts/liberation-mono/LiberationMono-BoldItalic.ttf"

#define POPOS_DEFAULT_FONT \
    "/usr/share/fonts/truetype/freefont/FreeSans.ttf",\
    "",\
    "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf",\
    "/usr/share/fonts/truetype/freefont/FreeSansOblique.ttf",\
    "/usr/share/fonts/truetype/freefont/FreeSansBoldOblique.ttf"

#define POPOS_DEFAULT_MONOFONT \
    "/usr/share/fonts/truetype/freefont/FreeMono.ttf",\
    "",\
    "/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf",\
    "/usr/share/fonts/truetype/freefont/FreeMonoOblique.ttf",\
    "/usr/share/fonts/truetype/freefont/FreeMonoBoldOblique.ttf"

#define MANJARO_DEFAULT_FONT \
    "/usr/share/fonts/noto/NotoSans-Regular.ttf",\
    "/usr/share/fonts/noto/NotoSans-Light.ttf",\
    "/usr/share/fonts/noto/NotoSans-Bold.ttf",\
    "/usr/share/fonts/noto/NotoSans-Italic.ttf",\
    "/usr/share/fonts/noto/NotoSans-BoldItalic.ttf"

#define MANJARO_DEFAULT_MONOFONT \
    "/usr/share/fonts/TTF/Hack-Regular.ttf",\
    "",\
    "/usr/share/fonts/TTF/Hack-Bold.ttf",\
    "/usr/share/fonts/TTF/Hack-Italic.ttf",\
    "/usr/share/fonts/TTF/Hack-BoldItalic.ttf",

#endif

#ifndef IM_RICHTEXT_MAXDEPTH
#define IM_RICHTEXT_MAXDEPTH 32
#endif

#ifndef IM_RICHTEXT_MAX_LISTDEPTH
#define IM_RICHTEXT_MAX_LISTDEPTH 16
#endif

#ifndef IM_RICHTEXT_MAX_LISTITEM
#define IM_RICHTEXT_MAX_LISTITEM 128
#endif

#ifndef IM_RICHTEXT_MAXTABSTOP
#define IM_RICHTEXT_MAXTABSTOP 32
#endif

#ifndef IM_RICHTEXT_BLINK_ANIMATION_INTERVAL
#define IM_RICHTEXT_BLINK_ANIMATION_INTERVAL 500
#endif

#ifndef IM_RICHTEXT_MARQUEE_ANIMATION_INTERVAL
#define IM_RICHTEXT_MARQUEE_ANIMATION_INTERVAL 18
#endif

#ifndef IM_RICHTEXT_NESTED_ITEMCOUNT_STRSZ
#define IM_RICHTEXT_NESTED_ITEMCOUNT_STRSZ 64
#endif

#ifndef IM_RICHTEXT_MAX_COLORSTOPS
#define IM_RICHTEXT_MAX_COLORSTOPS 4
#endif

#ifdef GLIMMER_HIDE_IMGUI_DEPENDENCY
struct ImVec2 
{ 
    float x = 0.f, y = 0.f; 
    ImVec2& operator+=(const ImVec2& pos) { x += pos.x; y += pos.y; return *this; }
    ImVec2& operator-=(const ImVec2& pos) { x += pos.x; y += pos.y; return *this; }
};
// ImVec2 arithmetic operators (ImGui 1.92+ doesn't define these)
#ifndef GLIMMER_IMVEC2_OPERATORS_DEFINED
#define GLIMMER_IMVEC2_OPERATORS_DEFINED
inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs)
{
    return ImVec2{ lhs.x + rhs.x, lhs.y + rhs.y };
}

inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs)
{
    return ImVec2{ lhs.x - rhs.x, lhs.y - rhs.y };
}

// Unary negation operator
inline ImVec2 operator-(const ImVec2& v)
{
    return ImVec2{ -v.x, -v.y };
}

inline ImVec2 operator*(const ImVec2& lhs, float rhs)
{
    return ImVec2{ lhs.x * rhs, lhs.y * rhs };
}

inline ImVec2 operator/(const ImVec2& lhs, float rhs)
{
    return ImVec2{ lhs.x / rhs, lhs.y / rhs };
}

inline ImVec2& operator+=(ImVec2& lhs, const ImVec2& rhs)
{
    lhs.x += rhs.x;
    lhs.y += rhs.y;
    return lhs;
}

inline ImVec2& operator-=(ImVec2& lhs, const ImVec2& rhs)
{
    lhs.x -= rhs.x;
    lhs.y -= rhs.y;
    return lhs;
}

inline ImVec2& operator*=(ImVec2& lhs, float rhs)
{
    lhs.x *= rhs;
    lhs.y *= rhs;
    return lhs;
}

inline ImVec2& operator/=(ImVec2& lhs, float rhs)
{
    lhs.x /= rhs;
    lhs.y /= rhs;
    return lhs;
}

// ImVec2 comparison operators
inline bool operator==(const ImVec2& lhs, const ImVec2& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

inline bool operator!=(const ImVec2& lhs, const ImVec2& rhs)
{
    return lhs.x != rhs.x || lhs.y != rhs.y;
}

inline bool operator>(const ImVec2& lhs, const ImVec2& rhs)
{
    return lhs.x > rhs.x || lhs.y > rhs.y;
}
#endif
struct ImRect
{
    ImVec2 Min{}, Max{};
    float GetWidth() const { return Max.x - Min.x; }
    float GetHeight() const { return Max.y - Min.y; }
    float GetArea() const { return GetWidth() * GetHeight(); }
    bool Contains(const ImVec2& pos) const 
    { return pos.x >= Min.x && pos.x <= Max.x && pos.y >= Min.y && pos.y <= Max.y; }
    ImRect& Translate(const ImVec2& pos) { Min.x += pos.x; Max.x += pos.x; Min.y += pos.y; Max.y += pos.y; return *this; }
    ImRect& TranslateX(float x) { Min.x += x; Max.x += x; return *this; }
    ImRect& TranslateY(float y) { Min.y += y; Max.y += y; return *this; }
};
#else
#include "libs/inc/imgui/imgui.h"
#include "libs/inc/imgui/imgui_internal.h"

// ImVec2 arithmetic operators (ImGui 1.92+ doesn't define these)
#ifndef GLIMMER_IMVEC2_OPERATORS_DEFINED
#define GLIMMER_IMVEC2_OPERATORS_DEFINED
inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs)
{
    return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
}

inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs)
{
    return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y);
}

// Unary negation operator
inline ImVec2 operator-(const ImVec2& v)
{
    return ImVec2(-v.x, -v.y);
}

inline ImVec2 operator*(const ImVec2& lhs, float rhs)
{
    return ImVec2(lhs.x * rhs, lhs.y * rhs);
}

inline ImVec2 operator/(const ImVec2& lhs, float rhs)
{
    return ImVec2(lhs.x / rhs, lhs.y / rhs);
}

inline ImVec2& operator+=(ImVec2& lhs, const ImVec2& rhs)
{
    lhs.x += rhs.x;
    lhs.y += rhs.y;
    return lhs;
}

inline ImVec2& operator-=(ImVec2& lhs, const ImVec2& rhs)
{
    lhs.x -= rhs.x;
    lhs.y -= rhs.y;
    return lhs;
}

inline ImVec2& operator*=(ImVec2& lhs, float rhs)
{
    lhs.x *= rhs;
    lhs.y *= rhs;
    return lhs;
}

inline ImVec2& operator/=(ImVec2& lhs, float rhs)
{
    lhs.x /= rhs;
    lhs.y /= rhs;
    return lhs;
}

// ImVec2 comparison operators
inline bool operator==(const ImVec2& lhs, const ImVec2& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

inline bool operator!=(const ImVec2& lhs, const ImVec2& rhs)
{
    return lhs.x != rhs.x || lhs.y != rhs.y;
}

inline bool operator>(const ImVec2& lhs, const ImVec2& rhs)
{
    return lhs.x > rhs.x || lhs.y > rhs.y;
}
#endif
#endif

#ifdef _DEBUG
#include <cstdio>
#include <unordered_map>
#define LOG(FMT, ...) std::fprintf(stderr, FMT, __VA_ARGS__)
#define HIGHLIGHT(FMT, ...) std::fprintf(stderr, "\x1B[93m" FMT "\x1B[0m", __VA_ARGS__)
#define LOGERROR(FMT, ...) std::fprintf(stderr, "\x1B[31m" FMT "\x1B[0m", __VA_ARGS__)
#ifdef _WIN32
#define BREAK_IF(...) if (__VA_ARGS__) __debugbreak()
#endif
#else
#define LOG(FMT, ...)
#define HIGHLIGHT(FMT, ...)
#define LOGERROR(FMT, ...)
#endif

#define DEBUG_RECT(start, end) Config.renderer->DrawDebugRect(start, end, IM_COL32(255,0,0,255), 1.f)
#define DEBUG_RECT2(start, end) Config.renderer->DrawDebugRect(start, end, IM_COL32(255,0,0,255), 2.f)
#define RECT_OUT(X) X.Min.x, X.Min.y, X.Max.x, X.Max.y
#define RECT_FMT "(%f, %f) x (%f, %f)"

#define ONCE(FMT, ...) if (Config.platform->frameCount == 0) std::fprintf(stdout, FMT, __VA_ARGS__)
#define EVERY_NTHFRAME(N, FMT, ...) if (Config.platform->frameCount % N == 0) std::fprintf(stdout, FMT, __VA_ARGS__)

#define COLOR_FMT "rgba(%d, %d, %d, %d)"
#define COLOR_OUT(X) X & 0xff, (X & 0xff00) >> 8, (X & 0xff0000) >> 16, (X & 0xff000000) >> 24

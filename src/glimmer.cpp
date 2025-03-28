#include "glimmer.h"

#define _USE_MATH_DEFINES
#include <math.h>

#ifdef IM_RICHTEXT_TARGET_IMGUI
#include "imgui.h"
#ifdef IMGUI_ENABLE_FREETYPE
#include "misc/freetype/imgui_freetype.h"
#endif
#endif
#ifdef IM_RICHTEXT_TARGET_BLEND2D
#include "blend2d.h"
#endif

#include <string>
#include <cstring>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <filesystem>
#include <fstream>
#include <deque>
#include <chrono>
#include <cassert>
#include <bit>

#ifdef _DEBUG
#include <iostream>
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#undef ERROR
#define ERROR(FMT, ...) { \
    CONSOLE_SCREEN_BUFFER_INFO cbinfo; \
    auto h = GetStdHandle(STD_ERROR_HANDLE); \
    GetConsoleScreenBufferInfo(h, &cbinfo); \
    SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_INTENSITY); \
    std::fprintf(stderr, FMT, __VA_ARGS__); \
    SetConsoleTextAttribute(h, cbinfo.wAttributes); }
#undef DrawText
#else
#define ERROR(FMT, ...) std::fprintf(stderr, "\x1B[31m" FMT "\x1B[0m", __VA_ARGS__)
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

#include <sstream>
#include <cstdio>
#include <memory>
#include <array>

#endif

namespace glimmer
{
    // =============================================================================================
    // STATIC DATA
    // =============================================================================================

    static WindowConfig GlobalWindowConfig{};

    struct AnimationData
    {
        int32_t elements = 0;
        int32_t types = 0;
        ImGuiDir direction = ImGuiDir::ImGuiDir_Right;
        float offset = 0.f;
        long long timestamp = 0;

        void moveByPixel(float amount, float max, float reset)
        {
            auto currts = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock().now().time_since_epoch()).count();
            if (timestamp == 0) timestamp = currts;

            if (currts - timestamp >= 33)
            {
                offset += amount;
                if (offset >= max) offset = reset;
            }
        }
    };

    constexpr int AnimationsPreallocSz = 32;
    constexpr int ShadowsPreallocSz = 64;
    constexpr int FontStylePreallocSz = 64;
    constexpr int BorderPreallocSz = 64;
    constexpr int GradientPreallocSz = 64;

    inline int log2(auto i) { return i <= 0 ? 0 : 8 * sizeof(i) - std::countl_zero(i) - 1; }

    struct ToggleButtonStyleDescriptor
    {
        uint32_t trackColor;
        uint32_t trackBorderColor;
        uint32_t thumbColor;
        int32_t trackGradient = -1;
        int32_t thumGradient = -1;
        float trackBorderThickness = 2.f;
        float thumbOffset = 1.f;
        float thumbExpand = 0.f;
    };

    union CommonWidgetStyleDescriptor
    {
        ToggleButtonStyleDescriptor toggle;

        CommonWidgetStyleDescriptor() {}
    };

    //// Solve the problem: only leaf can be a widget
    //struct WidgetDescriptor
    //{
    //    WidgetType wtype;
    //    int32_t id;
    //    int32_t sibling;
    //    ImVec2 constraint;
    //    ImRect border, margin, padding, content;
    //    float flex = 1.f;
    //    bool isComputed = false;
    //    // keep adding members
    //};

    //struct LayoutDescriptor
    //{
    //    std::vector<WidgetDescriptor> widgets;
    //    LayoutType type;
    //    ImVec2 currpos;
    //    ImVec2 maxdim;
    //    ImVec2 computed;
    //    ImVec2 constraint{ -1.f, -1.f };
    //    ImRect geometry;
    //    int32_t parent = -1;
    //    bool wrap = true;
    //    bool isRowWise = true;
    //    bool overflowed = false;
    //};

    /*template <typename T, int16_t SegmentSz>
    class GVector
    {
        T  initial[SegmentSz];
        T* segments[31];
        int size = 0;

    public:

        GVector() {}

        template <typename... ArgsT>
        T& emplace_back(ArgsT&&... args)
        {
            if (size < SegmentSz)
            {
                initial[size] = T{ std::forward<ArgsT>(args)... };
                return initial[size];
            }
            else
            {
                auto segidx = (size - SegmentSz) / SegmentSz;
                auto elidx = (size - SegmentSz) % SegmentSz;
                assert(segidx < 31);

                if (segments[segidx] == nullptr)
                    segments[segidx] = new T[SegmentSz]{};

                segments[segidx][elidx] = T{ std::forward<ArgsT>(args)... };
                return segments[segidx][elidx];
            }
        }

        T& operator[](int index)
        {
            assert(index < size);
            if (size < SegmentSz) return initial[size];
            else
            {
                auto segidx = (size - SegmentSz) / SegmentSz;
                auto elidx = (size - SegmentSz) % SegmentSz;
                std::div(1, 23);
                return segments[segidx][elidx];
            }
        }
    };*/

    struct WidgetStates
    {
        // This is quasi-persistent
        std::vector<WidgetStateData> states[WT_TotalTypes];
        std::vector<StyleDescriptor> styles[WT_TotalTypes];

        // This has to persistent
        std::vector<AnimationData> animations{ AnimationsPreallocSz, AnimationData{} };

        // Per widget specific style objects
        std::vector<ToggleButtonStyleDescriptor> toggleButtonStyles;

        // These are used for push/pop style API
        StyleDescriptor currstyle[32 * 6];
        int currstyleDepth = 0;

        // Keep track of widget IDs
        int maxids[WT_TotalTypes];
        int tempids[WT_TotalTypes];

        // Whether we are in a frame being rendered
        bool InsideFrame = false;

        //std::vector<LayoutDescriptor> layouts[16]; // stack of layout (upto 16 deep)
        //int32_t currLayoutDepth = -1;

        WidgetStateData& GetState(int32_t id)
        {
            auto index = id & 0xffff;
            auto wtype = (WidgetType)(id >> 16);
            return states[wtype][index];
        }

        StyleDescriptor& GetStyle(int32_t id, int32_t state)
        {
            auto index = id & 0xffff;
            auto wtype = (WidgetType)(id >> 16);
            auto style = log2((unsigned)state);
            auto& res = styles[wtype][index + style];
            if (res.font.font == nullptr) res.font.font = GetFont(res.font.family, res.font.size, FT_Normal);
            return res;
        }

        WidgetStates()
        {
            std::memset(maxids, 0, WT_TotalTypes);
            std::memset(tempids, INT_MAX, WT_TotalTypes);

            for (auto idx = 0; idx < WT_TotalTypes; ++idx)
            {
                maxids[idx] = 0;
                states[idx].resize(32, WidgetStateData{ (WidgetType)idx });
                styles[idx].resize(32 * 6, StyleDescriptor{});

                for (auto sidx = 0; sidx < 32 * 6; sidx++)
                {
                    //styles[idx][sidx].wtype = (WidgetType)idx;

                    switch (idx)
                    {
                    case WT_ToggleButton:
                        styles[idx][sidx].mindim = GlobalWindowConfig.toggleButtonSz;
                        toggleButtonStyles.resize(32 * 6, ToggleButtonStyleDescriptor{});
                        break;
                    default: break;
                    }
                }
            }
        }
    };

    static WidgetStates GlobalStates{};

    // =============================================================================================
    // FONT FUNCTIONS
    // =============================================================================================

#pragma region Font Management
    struct FontFamily
    {
#ifdef IM_RICHTEXT_TARGET_IMGUI
        std::map<float, ImFont*> FontPtrs[FT_Total];
#endif
#ifdef IM_RICHTEXT_TARGET_BLEND2D
        std::map<float, BLFont> Fonts[FT_Total];
        BLFontFace FontFace[FT_Total];
#endif
        FontCollectionFile Files;
        bool AutoScale = false;
    };

    struct FontMatchInfo
    {
        std::string files[FT_Total];
        std::string family;
        bool serif = false;
    };

    struct FontLookupInfo
    {
        std::deque<FontMatchInfo> info;
        std::unordered_map<std::string_view, int> ProportionalFontFamilies;
        std::unordered_map<std::string_view, int> MonospaceFontFamilies;
        std::unordered_set<std::string_view> LookupPaths;

        void Register(const std::string& family, const std::string& filepath, FontType ft, bool isMono, bool serif)
        {
            auto& lookup = info.emplace_back();
            lookup.files[ft] = filepath;
            lookup.serif = serif;
            lookup.family = family;
            auto& index = !isMono ? ProportionalFontFamilies[lookup.family] :
                MonospaceFontFamilies[lookup.family];
            index = (int)info.size() - 1;
        }
    };

    static std::unordered_map<std::string_view, FontFamily> FontStore;
    static FontLookupInfo FontLookup;

#ifdef IM_RICHTEXT_TARGET_IMGUI
    static void LoadFont(ImGuiIO& io, FontFamily& family, FontType ft, float size, ImFontConfig config, int flag)
    {
        if (ft == FT_Normal)
        {
            auto font = family.Files.Files[FT_Normal].empty() ? nullptr :
                io.Fonts->AddFontFromFileTTF(family.Files.Files[FT_Normal].data(), size, &config);
            assert(font != nullptr);
            family.FontPtrs[FT_Normal][size] = font;
        }
        else
        {
            auto fallback = family.FontPtrs[FT_Normal][size];

#ifdef IMGUI_ENABLE_FREETYPE
            auto configFlags = config.FontBuilderFlags;

            if (family.Files.Files[ft].empty()) {
                config.FontBuilderFlags = configFlags | flag;
                family.FontPtrs[ft][size] = io.Fonts->AddFontFromFileTTF(
                    family.Files.Files[FT_Normal].data(), size, &config);
            }
            else family.FontPtrs[ft][size] = io.Fonts->AddFontFromFileTTF(
                family.Files.Files[ft].data(), size, &config);
#else
            fonts[ft][size] = files.Files[ft].empty() ? fallback :
                io.Fonts->AddFontFromFileTTF(files.Files[ft].data(), size, &config);
#endif
        }
    }

    bool LoadFonts(std::string_view family, const FontCollectionFile& files, float size, ImFontConfig config, bool autoScale)
    {
        ImGuiIO& io = ImGui::GetIO();
        FontStore[family].Files = files;

        auto& ffamily = FontStore[family];
        ffamily.AutoScale = autoScale;
        LoadFont(io, ffamily, FT_Normal, size, config, 0);

#ifdef IMGUI_ENABLE_FREETYPE
        LoadFont(io, ffamily, FT_Bold, size, config, ImGuiFreeTypeBuilderFlags_Bold);
        LoadFont(io, ffamily, FT_Italics, size, config, ImGuiFreeTypeBuilderFlags_Oblique);
        LoadFont(io, ffamily, FT_BoldItalics, size, config, ImGuiFreeTypeBuilderFlags_Bold | ImGuiFreeTypeBuilderFlags_Oblique);
#else
        LoadFont(io, ffamily, FT_Bold, size, config, 0);
        LoadFont(io, ffamily, FT_Italics, size, config, 0);
        LoadFont(io, ffamily, FT_BoldItalics, size, config, 0);
#endif
        LoadFont(io, ffamily, FT_Light, size, config, 0);
        return true;
    }

#endif
#ifdef IM_RICHTEXT_TARGET_BLEND2D
    static void CreateFont(FontFamily& family, FontType ft, float size)
    {
        auto& face = family.FontFace[ft];

        if (ft == FT_Normal)
        {
            auto& font = family.Fonts[FT_Normal][size];
            auto res = face.createFromFile(family.Files.Files[FT_Normal].c_str());
            res = res == BL_SUCCESS ? font.createFromFace(face, size) : res;
            assert(res == BL_SUCCESS);
        }
        else
        {
            const auto& fallback = family.Fonts[FT_Normal][size];

            if (!files.Files[ft].empty())
            {
                auto res = face.createFromFile(family.Files.Files[ft].c_str());
                if (res == BL_SUCCESS) res = family.Fonts[ft][size].createFromFace(face, size);
                else family.Fonts[ft][size] = fallback;

                if (res != BL_SUCCESS) family.Fonts[ft][size] = fallback;
            }
            else
                family.Fonts[ft][size] = fallback;
        }
    }

    bool LoadFonts(std::string_view family, const FontCollectionFile& files, float size)
    {
        auto& ffamily = FontStore[family];
        ffamily.Files = files;
        CreateFont(ffamily, FT_Normal, size);
        CreateFont(ffamily, FT_Light, size);
        CreateFont(ffamily, FT_Bold, size);
        CreateFont(ffamily, FT_Italics, size);
        CreateFont(ffamily, FT_BoldItalics, size);
    }
#endif

#ifdef IM_RICHTEXT_TARGET_IMGUI
    static void LoadDefaultProportionalFont(float sz, const ImFontConfig& fconfig, bool autoScale)
    {
#ifdef _WIN32
        LoadFonts(IM_RICHTEXT_DEFAULT_FONTFAMILY, { WINDOWS_DEFAULT_FONT }, sz, fconfig, autoScale);
#elif __linux__
        std::filesystem::path fedoradir = "/usr/share/fonts/open-sans";
        std::filesystem::path ubuntudir = "/usr/share/fonts/truetype/freefont";
        std::filesystem::exists(fedoradir) ?
            LoadFonts(IM_RICHTEXT_DEFAULT_FONTFAMILY, { FEDORA_DEFAULT_FONT }, sz, fconfig) :
            std::filesystem::exists(ubuntudir) ?
            LoadFonts(IM_RICHTEXT_DEFAULT_FONTFAMILY, { POPOS_DEFAULT_FONT }, sz, fconfig) :
            LoadFonts(IM_RICHTEXT_DEFAULT_FONTFAMILY, { MANJARO_DEFAULT_FONT }, sz, fconfig);
#endif
        // TODO: Add default fonts for other platforms
    }

    static void LoadDefaultMonospaceFont(float sz, const ImFontConfig& fconfig, bool autoScale)
    {
#ifdef _WIN32
        LoadFonts(IM_RICHTEXT_MONOSPACE_FONTFAMILY, { WINDOWS_DEFAULT_MONOFONT }, sz, fconfig, autoScale);
#elif __linux__
        std::filesystem::path fedoradir = "/usr/share/fonts/liberation-mono";
        std::filesystem::path ubuntudir = "/usr/share/fonts/truetype/freefont";
        std::filesystem::exists(fedoradir) ?
            LoadFonts(IM_RICHTEXT_DEFAULT_FONTFAMILY, { FEDORA_DEFAULT_MONOFONT }, sz, fconfig) :
            std::filesystem::exists(ubuntudir) ?
            LoadFonts(IM_RICHTEXT_DEFAULT_FONTFAMILY, { POPOS_DEFAULT_MONOFONT }, sz, fconfig) :
            LoadFonts(IM_RICHTEXT_DEFAULT_FONTFAMILY, { MANJARO_DEFAULT_MONOFONT }, sz, fconfig);
#endif
        // TODO: Add default fonts for other platforms
    }
#endif

#ifdef IM_RICHTEXT_TARGET_BLEND2D
    static void LoadDefaultProportionalFont(float sz)
    {
#ifdef _WIN32
        LoadFonts(IM_RICHTEXT_DEFAULT_FONTFAMILY, { WINDOWS_DEFAULT_FONT }, sz);
#elif __linux__
        std::filesystem::path fedoradir = "/usr/share/fonts/open-sans";
        std::filesystem::path ubuntudir = "/usr/share/fonts/truetype/freefont";
        std::filesystem::exists(fedoradir) ?
            LoadFonts(IM_RICHTEXT_DEFAULT_FONTFAMILY, { FEDORA_DEFAULT_FONT }, sz) :
            std::filesystem::exists(ubuntudir) ?
            LoadFonts(IM_RICHTEXT_DEFAULT_FONTFAMILY, { POPOS_DEFAULT_FONT }, sz) :
            LoadFonts(IM_RICHTEXT_DEFAULT_FONTFAMILY, { MANJARO_DEFAULT_FONT }, sz);
#endif
        // TODO: Add default fonts for other platforms
    }

    static void LoadDefaultMonospaceFont(float sz)
    {
#ifdef _WIN32
        LoadFonts(IM_RICHTEXT_MONOSPACE_FONTFAMILY, { WINDOWS_DEFAULT_MONOFONT }, sz);
#elif __linux__
        std::filesystem::path fedoradir = "/usr/share/fonts/liberation-mono";
        std::filesystem::path ubuntudir = "/usr/share/fonts/truetype/freefont";
        std::filesystem::exists(fedoradir) ?
            LoadFonts(IM_RICHTEXT_DEFAULT_FONTFAMILY, { FEDORA_DEFAULT_MONOFONT }, sz) :
            std::filesystem::exists(ubuntudir) ?
            LoadFonts(IM_RICHTEXT_DEFAULT_FONTFAMILY, { POPOS_DEFAULT_MONOFONT }, sz) :
            LoadFonts(IM_RICHTEXT_DEFAULT_FONTFAMILY, { MANJARO_DEFAULT_MONOFONT }, sz);
#endif
        // TODO: Add default fonts for other platforms
    }
#endif

#ifndef IM_RICHTEXT_TARGET_IMGUI
    using ImWchar = uint32_t;
#endif

    static bool LoadDefaultFonts(float sz, const FontFileNames* names, bool skipProportional, bool skipMonospace,
        bool autoScale, const ImWchar* glyphs)
    {
#ifdef IM_RICHTEXT_TARGET_IMGUI
        ImFontConfig fconfig;
        fconfig.OversampleH = 2;
        fconfig.OversampleV = 1;
        fconfig.RasterizerMultiply = sz <= 16.f ? 2.f : 1.f;
        fconfig.GlyphRanges = glyphs;
#ifdef IMGUI_ENABLE_FREETYPE
        fconfig.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_LightHinting;
#endif
#endif

        auto copyFileName = [](const std::string_view fontname, char* fontpath, int startidx) {
            auto sz = std::min((int)fontname.size(), _MAX_PATH - startidx);

            if (sz == 0) std::memset(fontpath, 0, _MAX_PATH);
            else
            {
#ifdef _WIN32
                if (fontpath[startidx - 1] != '\\') { fontpath[startidx] = '\\'; startidx++; }
#else
                if (fontpath[startidx - 1] != '/') { fontpath[startidx] = '/'; startidx++; }
#endif
                std::memcpy(fontpath + startidx, fontname.data(), sz);
                fontpath[startidx + sz] = 0;
            }

            return fontpath;
        };

        if (names == nullptr)
        {
#ifdef IM_RICHTEXT_TARGET_IMGUI
            if (!skipProportional) LoadDefaultProportionalFont(sz, fconfig, autoScale);
            if (!skipMonospace) LoadDefaultMonospaceFont(sz, fconfig, autoScale);
#endif
#ifdef IM_RICHTEXT_TARGET_BLEND2D
            if (!skipProportional) LoadDefaultProportionalFont(sz);
            if (!skipMonospace) LoadDefaultMonospaceFont(sz);
#endif
        }
        else
        {
#if defined(_WIN32)
            static char BaseFontPaths[FT_Total][_MAX_PATH] = { "c:\\Windows\\Fonts\\", "c:\\Windows\\Fonts\\",
                "c:\\Windows\\Fonts\\","c:\\Windows\\Fonts\\", "c:\\Windows\\Fonts\\" };
#elif __APPLE__
            static char BaseFontPaths[FT_Total][_MAX_PATH] = { "/Library/Fonts/", "/Library/Fonts/",
                "/Library/Fonts/", "/Library/Fonts/", "/Library/Fonts/" };
#elif __linux__
            static char BaseFontPaths[FT_Total][_MAX_PATH] = { "/usr/share/fonts/", "/usr/share/fonts/",
                "/usr/share/fonts/", "/usr/share/fonts/", "/usr/share/fonts/" };
#else
#error "Platform unspported..."
#endif

            if (!names->BasePath.empty())
            {
                for (auto idx = 0; idx < FT_Total; ++idx)
                {
                    auto baseFontPath = BaseFontPaths[idx];
                    std::memset(baseFontPath, 0, _MAX_PATH);
                    auto sz = std::min((int)names->BasePath.size(), _MAX_PATH);
                    strncpy_s(baseFontPath, _MAX_PATH - 1, names->BasePath.data(), sz);
                    baseFontPath[sz] = '\0';
                }
            }

            const int startidx = (int)std::strlen(BaseFontPaths[0]);
            FontCollectionFile files;

            if (!skipProportional && !names->Proportional.Files[FT_Normal].empty())
            {
                files.Files[FT_Normal] = copyFileName(names->Proportional.Files[FT_Normal], BaseFontPaths[FT_Normal], startidx);
                files.Files[FT_Light] = copyFileName(names->Proportional.Files[FT_Light], BaseFontPaths[FT_Light], startidx);
                files.Files[FT_Bold] = copyFileName(names->Proportional.Files[FT_Bold], BaseFontPaths[FT_Bold], startidx);
                files.Files[FT_Italics] = copyFileName(names->Proportional.Files[FT_Italics], BaseFontPaths[FT_Italics], startidx);
                files.Files[FT_BoldItalics] = copyFileName(names->Proportional.Files[FT_BoldItalics], BaseFontPaths[FT_BoldItalics], startidx);
#ifdef IM_RICHTEXT_TARGET_IMGUI
                LoadFonts(IM_RICHTEXT_DEFAULT_FONTFAMILY, files, sz, fconfig, autoScale);
#endif
#ifdef IM_RICHTEXT_TARGET_BLEND2D
                LoadFonts(IM_RICHTEXT_DEFAULT_FONTFAMILY, files, sz);
#endif
            }
            else
            {
#ifdef IM_RICHTEXT_TARGET_IMGUI
                if (!skipProportional) LoadDefaultProportionalFont(sz, fconfig, autoScale);
#endif
#ifdef IM_RICHTEXT_TARGET_BLEND2D
                if (!skipProportional) LoadDefaultProportionalFont(sz);
#endif
            }

            if (!skipMonospace && !names->Monospace.Files[FT_Normal].empty())
            {
                files.Files[FT_Normal] = copyFileName(names->Monospace.Files[FT_Normal], BaseFontPaths[FT_Normal], startidx);
                files.Files[FT_Bold] = copyFileName(names->Monospace.Files[FT_Bold], BaseFontPaths[FT_Bold], startidx);
                files.Files[FT_Italics] = copyFileName(names->Monospace.Files[FT_Italics], BaseFontPaths[FT_Italics], startidx);
                files.Files[FT_BoldItalics] = copyFileName(names->Monospace.Files[FT_BoldItalics], BaseFontPaths[FT_BoldItalics], startidx);
#ifdef IM_RICHTEXT_TARGET_IMGUI
                LoadFonts(IM_RICHTEXT_MONOSPACE_FONTFAMILY, files, sz, fconfig, autoScale);
#endif
#ifdef IM_RICHTEXT_TARGET_BLEND2D
                LoadFonts(IM_RICHTEXT_MONOSPACE_FONTFAMILY, files, sz);
#endif
            }
            else
            {
#ifdef IM_RICHTEXT_TARGET_IMGUI
                if (!skipMonospace) LoadDefaultMonospaceFont(sz, fconfig, autoScale);
#endif
#ifdef IM_RICHTEXT_TARGET_BLEND2D
                if (!skipMonospace) LoadDefaultMonospaceFont(sz);
#endif
            }
        }

        return true;
    }

    const static std::unordered_map<TextContentCharset, std::vector<ImWchar>> GlyphRanges
    {
        { TextContentCharset::ASCII, { 1, 127, 0 } },
        { TextContentCharset::ASCIISymbols, { 1, 127, 0x20A0, 0x20CF, 0x2122, 0x2122,
            0x2190, 0x21FF, 0x2200, 0x22FF, 0x2573, 0x2573, 0x25A0, 0x25FF, 0x2705, 0x2705,
            0x2713, 0x2716, 0x274E, 0x274E, 0x2794, 0x2794, 0x27A4, 0x27A4, 0x27F2, 0x27F3,
            0x2921, 0x2922, 0X2A7D, 0X2A7E, 0x2AF6, 0x2AF6, 0x2B04, 0x2B0D, 0x2B60, 0x2BD1,
            0 } },
        { TextContentCharset::UTF8Simple, { 1, 256, 0x100, 0x17F, 0x180, 0x24F,
            0x370, 0x3FF, 0x400, 0x4FF, 0x500, 0x52F, 0x1E00, 0x1EFF, 0x1F00, 0x1FFF,
            0x20A0, 0x20CF, 0x2122, 0x2122,
            0x2190, 0x21FF, 0x2200, 0x22FF, 0x2573, 0x2573, 0x25A0, 0x25FF, 0x2705, 0x2705,
            0x2713, 0x2716, 0x274E, 0x274E, 0x2794, 0x2794, 0x27A4, 0x27A4, 0x27F2, 0x27F3,
            0x2921, 0x2922, 0x2980, 0x29FF, 0x2A00, 0x2AFF, 0X2A7D, 0X2A7E, 0x2AF6, 0x2AF6,
            0x2B04, 0x2B0D, 0x2B60, 0x2BD1, 0x1F600, 0x1F64F, 0x1F800, 0x1F8FF,
            0 } },
        { TextContentCharset::UnicodeBidir, {} }, // All glyphs supported by font
    };

    static bool LoadDefaultFonts(const std::vector<float>& sizes, uint64_t flt, TextContentCharset charset,
        const FontFileNames* names)
    {
        assert((names != nullptr) || (flt & FLT_Proportional) || (flt & FLT_Monospace));

        auto it = GlyphRanges.find(charset);
        auto glyphrange = it->second.empty() ? nullptr : it->second.data();

        for (auto sz : sizes)
        {
            LoadDefaultFonts(sz, names, !(flt & FLT_Proportional), !(flt & FLT_Monospace),
                flt & FLT_AutoScale, glyphrange);
        }

#ifdef IM_RICHTEXT_TARGET_IMGUI
        ImGui::GetIO().Fonts->Build();
#endif
        return true;
    }

    bool LoadDefaultFonts(const FontDescriptor* descriptors, int total)
    {
        assert(descriptors != nullptr);

        for (auto idx = 0; idx < total; ++idx)
        {
            auto names = descriptors[idx].names.has_value() ? &(descriptors[idx].names.value()) : nullptr;
            LoadDefaultFonts(descriptors[idx].sizes, descriptors[idx].flags, descriptors[idx].charset, names);
        }
        
        return true;
    }

    // Structure to hold font data
    struct FontInfo
    {
        std::string fontFamily;
        int weight = 400;          // Default to normal (400)
        bool isItalic = false;
        bool isBold = false;
        bool isMono = false;
        bool isLight = false;
        bool isSerif = true;
    };

    // Function to read a big-endian 16-bit unsigned integer
    uint16_t ReadUInt16(const unsigned char* data, size_t offset)
    {
        return (data[offset] << 8) | data[offset + 1];
    }

    // Function to read a big-endian 32-bit unsigned integer
    uint32_t ReadUInt32(const unsigned char* data, size_t offset)
    {
        return (static_cast<uint32_t>(data[offset]) << 24) |
            (static_cast<uint32_t>(data[offset + 1]) << 16) |
            (static_cast<uint32_t>(data[offset + 2]) << 8) |
            static_cast<uint32_t>(data[offset + 3]);
    }

    // Read a Pascal string (length byte followed by string data)
    std::string ReadPascalString(const unsigned char* data, size_t offset, size_t length)
    {
        std::string result;
        for (size_t i = 0; i < length; i++)
            if (data[offset + i] != 0)
                result.push_back(static_cast<char>(data[offset + i]));
        return result;
    }

    // Extract font information from a TTF file
    FontInfo ExtractFontInfo(const std::string& filename)
    {
        FontInfo info;
        std::ifstream file(filename, std::ios::binary);

        if (!file.is_open())
        {
#ifdef _DEBUG
            std::cerr << "Error: Could not open file " << filename << std::endl;
#endif
            return info;
        }

        // Get file size
        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        // Read the entire file into memory
        std::vector<unsigned char> buffer(fileSize);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
        file.close();

        // Check if this is a valid TTF file (signature should be 0x00010000 for TTF)
        uint32_t sfntVersion = ReadUInt32(buffer.data(), 0);
        if (sfntVersion != 0x00010000 && sfntVersion != 0x4F54544F)
        { // TTF or OTF
#ifdef _DEBUG
            std::cerr << "Error: Not a valid TTF/OTF file" << std::endl;
#endif
            return info;
        }

        // Parse the table directory
        uint16_t numTables = ReadUInt16(buffer.data(), 4);
        bool foundName = false;
        bool foundOS2 = false;
        uint32_t nameTableOffset = 0;
        uint32_t os2TableOffset = 0;

        // Table directory starts at offset 12
        for (int i = 0; i < numTables; i++)
        {
            size_t entryOffset = 12 + i * 16;
            char tag[5] = { 0 };
            memcpy(tag, buffer.data() + entryOffset, 4);

            if (strcmp(tag, "name") == 0)
            {
                nameTableOffset = ReadUInt32(buffer.data(), entryOffset + 8);
                foundName = true;
            }
            else if (strcmp(tag, "OS/2") == 0)
            {
                os2TableOffset = ReadUInt32(buffer.data(), entryOffset + 8);
                foundOS2 = true;
            }

            if (foundName && foundOS2) break;
        }

        // Process the 'name' table if found
        // Docs: https://learn.microsoft.com/en-us/typography/opentype/spec/name
        if (foundName)
        {
            uint16_t nameCount = ReadUInt16(buffer.data(), nameTableOffset + 2);
            uint16_t storageOffset = ReadUInt16(buffer.data(), nameTableOffset + 4);
            uint16_t familyNameID = 1;  // Font Family name
            uint16_t subfamilyNameID = 2;  // Font Subfamily name

            for (int i = 0; i < nameCount; i++)
            {
                size_t recordOffset = nameTableOffset + 6 + i * 12;
                uint16_t platformID = ReadUInt16(buffer.data(), recordOffset);
                uint16_t encodingID = ReadUInt16(buffer.data(), recordOffset + 2);
                uint16_t languageID = ReadUInt16(buffer.data(), recordOffset + 4);
                uint16_t nameID = ReadUInt16(buffer.data(), recordOffset + 6);
                uint16_t length = ReadUInt16(buffer.data(), recordOffset + 8);
                uint16_t stringOffset = ReadUInt16(buffer.data(), recordOffset + 10);

                // We prefer English Windows (platformID=3, encodingID=1, languageID=0x0409)
                bool isEnglish = (platformID == 3 && encodingID == 1 && (languageID == 0x0409 || languageID == 0));

                // If not English Windows, try platform-independent entries as fallback
                if (!isEnglish && platformID == 0) isEnglish = true;

                if (isEnglish)
                {
                    if (nameID == familyNameID && info.fontFamily.empty())
                    {
                        // Convert UTF-16BE to ASCII for simplicity
                        std::string name;
                        for (int j = 0; j < length; j += 2)
                        {
                            char c = buffer[nameTableOffset + storageOffset + stringOffset + j + 1];
                            if (c) name.push_back(c);
                        }
                        info.fontFamily = name;
                    }
                    else if (nameID == subfamilyNameID)
                    {
                        // Convert UTF-16BE to ASCII for simplicity
                        std::string name;
                        for (int j = 0; j < length; j += 2)
                        {
                            char c = buffer[nameTableOffset + storageOffset + stringOffset + j + 1];
                            if (c) name.push_back(c);
                        }

                        // Check if it contains "Italic" or "Oblique"
                        if (name.find("Italic") != std::string::npos ||
                            name.find("italic") != std::string::npos ||
                            name.find("Oblique") != std::string::npos ||
                            name.find("oblique") != std::string::npos)
                            info.isItalic = true;
                    }
                }
            }
        }

        // Process the 'OS/2' table if found
        // Docs: https://learn.microsoft.com/en-us/typography/opentype/spec/os2
        if (foundOS2)
        {
            // Weight is at offset 4 in the OS/2 table
            info.weight = ReadUInt16(buffer.data(), os2TableOffset + 4);

            // Check fsSelection bit field for italic flag (bit 0)
            uint16_t fsSelection = ReadUInt16(buffer.data(), os2TableOffset + 62);
            if ((fsSelection & 0x01) || (fsSelection & 0x100)) info.isItalic = true;
            if (fsSelection & 0x10) info.isBold = true;

            uint8_t panose[10];
            std::memcpy(panose, buffer.data() + os2TableOffset + 32, 10);

            // Refer to this: https://monotype.github.io/panose/pan2.htm for PANOSE docs
            if (panose[0] == 2 && panose[3] == 9) info.isMono = true;
            if (panose[0] == 2 && (panose[2] == 2 || panose[2] == 3 || panose[2] == 4)) info.isLight = true;
            if (panose[0] == 2 && (panose[1] == 11 || panose[1] == 12 || panose[1] == 13)) info.isSerif = false;
        }

        return info;
    }

#if __linux__
    struct FontFamilyInfo
    {
        std::string filename;
        std::string fontName;
        std::string style;
    };

    // Function to execute a command and return the output
    static std::string ExecCommand(const char* cmd)
    {
        std::array<char, 8192> buffer;
        std::string result;
        FILE* pipe = popen(cmd, "r");

        if (!pipe) return std::string{};

        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
            result += buffer.data();

        pclose(pipe);
        return result;
    }

    // Trim whitespace from a string_view and return as string
    static std::string_view Trim(std::string_view sv)
    {
        // Find first non-whitespace
        size_t start = 0;
        while (start < sv.size() && std::isspace(sv[start]))
            ++start;

        // Find last non-whitespace
        size_t end = sv.size();
        while (end > start && std::isspace(sv[end - 1]))
            --end;

        return sv.substr(start, end - start);
    }

    // Parse a single line from fc-list output
    static FontFamilyInfo ParseFcListLine(const std::string& line)
    {
        FontFamilyInfo info;
        std::string_view lineView(line);

        // fc-list typically outputs: filename: font name:style=style1,style2,...

        // Find the first colon for filename
        size_t firstColon = lineView.find(':');
        if (firstColon != std::string_view::npos)
        {
            info.filename = Trim(lineView.substr(0, firstColon));

            // Find the second colon for font name
            size_t secondColon = lineView.find(':', firstColon + 1);
            if (secondColon != std::string_view::npos)
            {
                info.fontName = Trim(lineView.substr(firstColon + 1, secondColon - firstColon - 1));

                // Find "style=" after the second colon
                std::string_view styleView = lineView.substr(secondColon + 1);
                size_t stylePos = styleView.find("style=");

                if (stylePos != std::string_view::npos)
                {
                    styleView = styleView.substr(stylePos + 6); // 6 is length of "style="

                    // Find the first comma in the style list
                    size_t commaPos = styleView.find(',');
                    if (commaPos != std::string_view::npos)
                        info.style = Trim(styleView.substr(0, commaPos));
                    else
                        info.style = Trim(styleView);
                }
                else
                    info.style = "Regular"; // Default style if not specified
            }
            else
            {
                // If there's no second colon, use everything after first colon as font name
                info.fontName = Trim(lineView.substr(firstColon + 1));
                info.style = "Regular";
            }
        }

        return info;
    }

    static bool PopulateFromFcList()
    {
        std::string output = ExecCommand("fc-list");

        if (!output.empty())
        {
            std::istringstream iss(output);
            std::string line;

            while (std::getline(iss, line))
            {
                if (!line.empty())
                {
                    auto info = ParseFcListLine(line);
                    auto isBold = info.style.find("Bold") != std::string::npos;
                    auto isItalics = (info.style.find("Oblique") != std::string::npos) ||
                        (info.style.find("Italic") != std::string::npos);
                    auto isMonospaced = info.fontName.find("Mono") != std::string::npos;
                    auto ft = isBold && isItalics ? FT_BoldItalics : isBold ? FT_Bold :
                        isItalics ? FT_Italics : FT_Normal;
                    auto isSerif = info.fontName.find("Serif") != std::string::npos;
                    FontLookup.Register(info.fontName, info.filename, ft, isMonospaced, isSerif);
                }
            }

            return true;
        }

        return false;
    }
#endif

#ifdef _WIN32
    static const std::string_view CommonFontNames[11]{
        "Arial", "Bookman Old Style", "Comic Sans MS", "Consolas", "Courier",
        "Georgia", "Lucida", "Segoe UI", "Tahoma", "Times New Roman", "Verdana"
    };
#elif __linux__
    static const std::string_view CommonFontNames[8]{
        "OpenSans", "FreeSans", "NotoSans", "Hack",
        "Bitstream Vera", "DejaVu", "Liberation", "Nimbus"
        // Add other common fonts expected
    };
#endif

    static void ProcessFileEntry(const std::filesystem::directory_entry& entry, bool cacheOnlyCommon)
    {
        auto fpath = entry.path().string();
        auto info = ExtractFontInfo(fpath);
#ifdef _DEBUG
        std::cout << "Checking font file: " << fpath << std::endl;
#endif

        if (cacheOnlyCommon)
        {
            for (const auto& fname : CommonFontNames)
            {
                if (info.fontFamily.find(fname) != std::string::npos)
                {
                    auto isBold = info.isBold || (info.weight >= 600);
                    auto ftype = isBold && info.isItalic ? FT_BoldItalics :
                        isBold ? FT_Bold : info.isItalic ? FT_Italics :
                        (info.weight < 400) || info.isLight ? FT_Light : FT_Normal;
                    FontLookup.Register(info.fontFamily, fpath, ftype, info.isMono, info.isSerif);
                    break;
                }
            }
        }
        else
        {
            auto isBold = info.isBold || (info.weight >= 600);
            auto ftype = isBold && info.isItalic ? FT_BoldItalics :
                isBold ? FT_Bold : info.isItalic ? FT_Italics :
                (info.weight < 400) || info.isLight ? FT_Light : FT_Normal;
            FontLookup.Register(info.fontFamily, fpath, ftype, info.isMono, info.isSerif);
        }
    }

    static void PreloadFontLookupInfoImpl(int timeoutMs, std::string_view* lookupPaths, int lookupSz)
    {
        std::unordered_set<std::string_view> notLookedUp;
        auto isDefaultPath = lookupSz == 0 || lookupPaths == nullptr;
        assert((lookupPaths == nullptr && lookupSz == 0) || (lookupPaths != nullptr && lookupSz > 0));

        for (auto idx = 0; idx < lookupSz; ++idx)
        {
            if (FontLookup.LookupPaths.count(lookupPaths[idx]) == 0)
                notLookedUp.insert(lookupPaths[idx]);
        }

        if (isDefaultPath)
#ifdef _WIN32
            notLookedUp.insert("C:\\Windows\\Fonts");
#elif __linux__
            notLookedUp.insert("/usr/share/fonts/");
#endif

        if (!notLookedUp.empty())
        {
#ifdef _WIN32
            auto start = std::chrono::system_clock().now().time_since_epoch().count();

            for (auto path : notLookedUp)
            {
                for (const auto& entry : std::filesystem::directory_iterator{ path })
                {
                    if (entry.is_regular_file() && ((entry.path().extension() == ".TTF") ||
                        (entry.path().extension() == ".ttf")))
                    {
                        ProcessFileEntry(entry, isDefaultPath);

                        auto current = std::chrono::system_clock().now().time_since_epoch().count();
                        if (timeoutMs != -1 && (int)(current - start) > timeoutMs) break;
                    }
                }
            }
#elif __linux__
            if (isDefaultPath)
            {
                if (!PopulateFromFcList())
                {
                    auto start = std::chrono::system_clock().now().time_since_epoch().count();

                    for (const auto& entry : std::filesystem::recursive_directory_iterator{ "/usr/share/fonts/" })
                    {
                        if (entry.is_regular_file() && entry.path().extension() == ".ttf")
                        {
                            ProcessFileEntry(entry, true);

                            auto current = std::chrono::system_clock().now().time_since_epoch().count();
                            if (timeoutMs != -1 && (int)(current - start) > timeoutMs) break;
                        }
                    }
                }
            }
            else
            {
                auto start = std::chrono::system_clock().now().time_since_epoch().count();

                for (auto path : notLookedUp)
                {
                    for (const auto& entry : std::filesystem::directory_iterator{ path })
                    {
                        if (entry.is_regular_file() && entry.path().extension() == ".ttf")
                        {
                            ProcessFileEntry(entry, false);

                            auto current = std::chrono::system_clock().now().time_since_epoch().count();
                            if (timeoutMs != -1 && (int)(current - start) > timeoutMs) break;
                        }
                    }
                }
            }
#endif      
        }
    }

    std::string_view FindFontFile(std::string_view family, FontType ft, std::string_view* lookupPaths, int lookupSz)
    {
        PreloadFontLookupInfoImpl(-1, lookupPaths, lookupSz);
        auto it = FontLookup.ProportionalFontFamilies.find(family);

        if (it == FontLookup.ProportionalFontFamilies.end())
        {
            it = FontLookup.MonospaceFontFamilies.find(family);

            if (it == FontLookup.MonospaceFontFamilies.end())
            {
                auto isDefaultMonospace = family.find("monospace") != std::string_view::npos;
                auto isDefaultSerif = family.find("serif") != std::string_view::npos &&
                    family.find("sans") == std::string_view::npos;

#ifdef _WIN32
                it = isDefaultMonospace ? FontLookup.MonospaceFontFamilies.find("Consolas") :
                    isDefaultSerif ? FontLookup.ProportionalFontFamilies.find("Times New Roman") :
                    FontLookup.ProportionalFontFamilies.find("Segoe UI");
#endif
                // TODO: Implement for Linux
            }
        }

        return FontLookup.info[it->second].files[ft];
    }

#ifdef IM_RICHTEXT_TARGET_IMGUI
    static auto LookupFontFamily(std::string_view family)
    {
        auto famit = FontStore.find(family);

        if (famit == FontStore.end())
        {
            for (auto it = FontStore.begin(); it != FontStore.end(); ++it)
            {
                if (it->first.find(family) == 0u ||
                    family.find(it->first) == 0u)
                {
                    return it;
                }
            }
        }

        if (famit == FontStore.end())
            famit = FontStore.find(IM_RICHTEXT_DEFAULT_FONTFAMILY);

        return famit;
    }

    void* GetFont(std::string_view family, float size, FontType ft)
    {
        auto famit = LookupFontFamily(family);
        const auto& fonts = famit->second.FontPtrs[ft];
        auto szit = fonts.find(size);

        if (szit == fonts.end() && !fonts.empty())
        {
            if (famit->second.AutoScale)
            {
                return fonts.begin()->second;
            }
            else
            {
                szit = fonts.lower_bound(size);
                szit = szit == fonts.begin() ? szit : std::prev(szit);
            }
        }

        return szit->second;
    }

//#ifndef IM_FONTMANAGER_STANDALONE
//    void* GetOverlayFont(const RenderConfig& config)
//    {
//        auto it = LookupFontFamily(IM_RICHTEXT_DEFAULT_FONTFAMILY);
//        auto fontsz = GlobalWindowConfig.defaultFontSz * 0.8f * GlobalWindowConfig.fontScaling;
//        return it->second.FontPtrs->lower_bound(fontsz)->second;
//    }
//#endif

    bool IsFontLoaded()
    {
        return ImGui::GetIO().Fonts->IsBuilt();
    }

#endif
#ifdef IM_RICHTEXT_TARGET_BLEND2D
    void PreloadFontLookupInfo(int timeoutMs)
    {
        PreloadFontLookupInfoImpl(timeoutMs, nullptr, 0);
    }

    void* GetFont(std::string_view family, float size, FontType type, FontExtraInfo extra)
    {
        auto famit = FontStore.find(family);

        if (famit != FontStore.end())
        {
            if (famit->second.Fonts[ft])
            {
                auto szit = famit->second.Fonts[ft].find(size);
                if (szit == famit->second.Fonts[ft].end())
                    CreateFont(family, ft, sz);
            }
            else CreateFont(family, ft, sz);
        }
        else
        {
            auto& ffamily = FontStore[family];
            ffamily.Files[ft] = extra.mapper != nullptr ? extra.mapper(family) :
                extra.filepath.empty() ? FindFontFile(family, type) : extra.filepath;
            assert(!ffamily.Files[ft].empty());
            CreateFont(ffamily, ft, sz);
        }

        return &(FontStore.at(family).Fonts[ft].at(size));
    }
#endif
#pragma endregion

    // =============================================================================================
    // INTERFACE IMPLEMENTATIONS
    // =============================================================================================

#pragma region Interface Impls

    ImGuiRenderer::ImGuiRenderer()
    {
        //UserData = ImGui::GetWindowDrawList();
    }

    void ImGuiRenderer::SetClipRect(ImVec2 startpos, ImVec2 endpos)
    {
        ImGui::PushClipRect(startpos, endpos, true);
    }

    void ImGuiRenderer::ResetClipRect()
    {
        ImGui::PopClipRect();
    }

    void ImGuiRenderer::DrawLine(ImVec2 startpos, ImVec2 endpos, uint32_t color, float thickness)
    {
        ((ImDrawList*)UserData)->AddLine(startpos, endpos, color, thickness);
    }

    void ImGuiRenderer::DrawPolyline(ImVec2* points, int sz, uint32_t color, float thickness)
    {
        ((ImDrawList*)UserData)->AddPolyline(points, sz, color, 0, thickness);
    }

    void ImGuiRenderer::DrawTriangle(ImVec2 pos1, ImVec2 pos2, ImVec2 pos3, uint32_t color, bool filled, bool thickness)
    {
        filled ? ((ImDrawList*)UserData)->AddTriangleFilled(pos1, pos2, pos3, color) :
            ((ImDrawList*)UserData)->AddTriangle(pos1, pos2, pos3, color, thickness);
    }

    void ImGuiRenderer::DrawRect(ImVec2 startpos, ImVec2 endpos, uint32_t color, bool filled, float thickness)
    {
        if (thickness > 0.f || filled)
        {
            filled ? ((ImDrawList*)UserData)->AddRectFilled(startpos, endpos, color) :
                ((ImDrawList*)UserData)->AddRect(startpos, endpos, color, 0.f, 0, thickness);
        }
    }

    void ImGuiRenderer::DrawRoundedRect(ImVec2 startpos, ImVec2 endpos, uint32_t color, bool filled,
        float topleftr, float toprightr, float bottomrightr, float bottomleftr, float thickness)
    {
        auto isUniformRadius = (topleftr == toprightr && toprightr == bottomrightr && bottomrightr == bottomleftr) ||
            ((topleftr + toprightr + bottomrightr + bottomleftr) == 0.f);

        if (isUniformRadius)
        {
            auto drawflags = 0;

            if (topleftr > 0.f) drawflags |= ImDrawFlags_RoundCornersTopLeft;
            if (toprightr > 0.f) drawflags |= ImDrawFlags_RoundCornersTopRight;
            if (bottomrightr > 0.f) drawflags |= ImDrawFlags_RoundCornersBottomRight;
            if (bottomleftr > 0.f) drawflags |= ImDrawFlags_RoundCornersBottomLeft;

            filled ? ((ImDrawList*)UserData)->AddRectFilled(startpos, endpos, color, toprightr, drawflags) :
                ((ImDrawList*)UserData)->AddRect(startpos, endpos, color, toprightr, drawflags, thickness);
        }
        else
        {
            auto& dl = *((ImDrawList*)UserData);
            ConstructRoundedRect(startpos, endpos, topleftr, toprightr, bottomrightr, bottomleftr);
            filled ? dl.PathFillConvex(color) : dl.PathStroke(color, ImDrawFlags_Closed, thickness);
        }
    }

    void ImGuiRenderer::DrawRectGradient(ImVec2 startpos, ImVec2 endpos, uint32_t colorfrom, uint32_t colorto, Direction dir)
    {
        dir == DIR_Horizontal ? ((ImDrawList*)UserData)->AddRectFilledMultiColor(startpos, endpos, colorfrom, colorto, colorto, colorfrom) :
            ((ImDrawList*)UserData)->AddRectFilledMultiColor(startpos, endpos, colorfrom, colorfrom, colorto, colorto);
    }

    void ImGuiRenderer::DrawRoundedRectGradient(ImVec2 startpos, ImVec2 endpos, float topleftr, float toprightr, float bottomrightr, float bottomleftr, uint32_t colorfrom, uint32_t colorto, Direction dir)
    {
        auto& dl = *((ImDrawList*)UserData);
        ConstructRoundedRect(startpos, endpos, topleftr, toprightr, bottomrightr, bottomleftr);

        DrawPolyGradient(dl._Path.Data, NULL, dl._Path.Size);
    }

    void ImGuiRenderer::DrawCircle(ImVec2 center, float radius, uint32_t color, bool filled, bool thickness)
    {
        filled ? ((ImDrawList*)UserData)->AddCircleFilled(center, radius, color) :
            ((ImDrawList*)UserData)->AddCircle(center, radius, color, 0, thickness);
    }

    void ImGuiRenderer::DrawSector(ImVec2 center, float radius, int start, int end, uint32_t color, bool filled, bool inverted, bool thickness)
    {
        constexpr float DegToRad = M_PI / 180.f;

        if (inverted)
        {
            auto& dl = *((ImDrawList*)UserData);
            dl.PathClear();
            dl.PathArcTo(center, radius, DegToRad * (float)start, DegToRad * (float)end, 32);
            auto start = dl._Path.front(), end = dl._Path.back();

            ImVec2 exterior[4] = { { std::min(start.x, end.x), std::min(start.y, end.y) },
                { std::max(start.x, end.x), std::min(start.y, end.y) },
                { std::min(start.x, end.x), std::max(start.y, end.y) },
                { std::max(start.x, end.x), std::max(start.y, end.y) }
            };

            auto maxDistIdx = 0;
            float dist = 0.f;
            for (auto idx = 0; idx < 4; ++idx)
            {
                auto curr = std::sqrt((end.x - start.x) * (end.x - start.x) +
                    (end.y - start.y) * (end.y - start.y));
                if (curr > dist)
                {
                    dist = curr;
                    maxDistIdx = idx;
                }
            }

            dl.PathLineTo(exterior[maxDistIdx]);
            filled ? dl.PathFillConcave(color) : dl.PathStroke(color, ImDrawFlags_Closed, thickness);
        }
        else
        {
            auto& dl = *((ImDrawList*)UserData);
            dl.PathClear();
            dl.PathArcTo(center, radius, DegToRad * (float)start, DegToRad * (float)end, 32);
            dl.PathLineTo(center);
            filled ? dl.PathFillConcave(color) : dl.PathStroke(color, ImDrawFlags_Closed, thickness);
        }
    }

    void ImGuiRenderer::DrawPolygon(ImVec2* points, int sz, uint32_t color, bool filled, float thickness)
    {
        filled ? ((ImDrawList*)UserData)->AddConvexPolyFilled(points, sz, color) :
            ((ImDrawList*)UserData)->AddPolyline(points, sz, color, ImDrawFlags_Closed, thickness);
    }

    void ImGuiRenderer::DrawPolyGradient(ImVec2* points, uint32_t* colors, int sz)
    {
        auto drawList = ((ImDrawList*)UserData);
        const ImVec2 uv = drawList->_Data->TexUvWhitePixel;

        if (drawList->Flags & ImDrawListFlags_AntiAliasedFill)
        {
            // Anti-aliased Fill
            const float AA_SIZE = 1.0f;
            const int idx_count = (sz - 2) * 3 + sz * 6;
            const int vtx_count = (sz * 2);
            drawList->PrimReserve(idx_count, vtx_count);

            // Add indexes for fill
            unsigned int vtx_inner_idx = drawList->_VtxCurrentIdx;
            unsigned int vtx_outer_idx = drawList->_VtxCurrentIdx + 1;
            for (int i = 2; i < sz; i++)
            {
                drawList->_IdxWritePtr[0] = (ImDrawIdx)(vtx_inner_idx);
                drawList->_IdxWritePtr[1] = (ImDrawIdx)(vtx_inner_idx + ((i - 1) << 1));
                drawList->_IdxWritePtr[2] = (ImDrawIdx)(vtx_inner_idx + (i << 1));
                drawList->_IdxWritePtr += 3;
            }

            // Compute normals
            ImVec2* temp_normals = (ImVec2*)alloca(sz * sizeof(ImVec2));
            for (int i0 = sz - 1, i1 = 0; i1 < sz; i0 = i1++)
            {
                const ImVec2& p0 = points[i0];
                const ImVec2& p1 = points[i1];
                ImVec2 diff = p1 - p0;
                diff *= ImInvLength(diff, 1.0f);
                temp_normals[i0].x = diff.y;
                temp_normals[i0].y = -diff.x;
            }

            for (int i0 = sz - 1, i1 = 0; i1 < sz; i0 = i1++)
            {
                // Average normals
                const ImVec2& n0 = temp_normals[i0];
                const ImVec2& n1 = temp_normals[i1];
                ImVec2 dm = (n0 + n1) * 0.5f;
                float dmr2 = dm.x * dm.x + dm.y * dm.y;
                if (dmr2 > 0.000001f)
                {
                    float scale = 1.0f / dmr2;
                    if (scale > 100.0f) scale = 100.0f;
                    dm *= scale;
                }
                dm *= AA_SIZE * 0.5f;

                // Add vertices
                drawList->_VtxWritePtr[0].pos = (points[i1] - dm);
                drawList->_VtxWritePtr[0].uv = uv; drawList->_VtxWritePtr[0].col = colors[i1];        // Inner
                drawList->_VtxWritePtr[1].pos = (points[i1] + dm);
                drawList->_VtxWritePtr[1].uv = uv; drawList->_VtxWritePtr[1].col = colors[i1] & ~IM_COL32_A_MASK;  // Outer
                drawList->_VtxWritePtr += 2;

                // Add indexes for fringes
                drawList->_IdxWritePtr[0] = (ImDrawIdx)(vtx_inner_idx + (i1 << 1));
                drawList->_IdxWritePtr[1] = (ImDrawIdx)(vtx_inner_idx + (i0 << 1));
                drawList->_IdxWritePtr[2] = (ImDrawIdx)(vtx_outer_idx + (i0 << 1));
                drawList->_IdxWritePtr[3] = (ImDrawIdx)(vtx_outer_idx + (i0 << 1));
                drawList->_IdxWritePtr[4] = (ImDrawIdx)(vtx_outer_idx + (i1 << 1));
                drawList->_IdxWritePtr[5] = (ImDrawIdx)(vtx_inner_idx + (i1 << 1));
                drawList->_IdxWritePtr += 6;
            }

            drawList->_VtxCurrentIdx += (ImDrawIdx)vtx_count;
        }
        else
        {
            // Non Anti-aliased Fill
            const int idx_count = (sz - 2) * 3;
            const int vtx_count = sz;
            drawList->PrimReserve(idx_count, vtx_count);
            for (int i = 0; i < vtx_count; i++)
            {
                drawList->_VtxWritePtr[0].pos = points[i];
                drawList->_VtxWritePtr[0].uv = uv;
                drawList->_VtxWritePtr[0].col = colors[i];
                drawList->_VtxWritePtr++;
            }
            for (int i = 2; i < sz; i++)
            {
                drawList->_IdxWritePtr[0] = (ImDrawIdx)(drawList->_VtxCurrentIdx);
                drawList->_IdxWritePtr[1] = (ImDrawIdx)(drawList->_VtxCurrentIdx + i - 1);
                drawList->_IdxWritePtr[2] = (ImDrawIdx)(drawList->_VtxCurrentIdx + i);
                drawList->_IdxWritePtr += 3;
            }

            drawList->_VtxCurrentIdx += (ImDrawIdx)vtx_count;
        }

        drawList->_Path.Size = 0;
    }

    void ImGuiRenderer::DrawRadialGradient(ImVec2 center, float radius, uint32_t in, uint32_t out, int start, int end)
    {
        auto drawList = ((ImDrawList*)UserData);
        if (((in | out) & IM_COL32_A_MASK) == 0 || radius < 0.5f)
            return;
        auto startrad = ((float)M_PI / 180.f) * (float)start;
        auto endrad = ((float)M_PI / 180.f) * (float)end;

        // Use arc with 32 segment count
        drawList->PathArcTo(center, radius, startrad, endrad, 32);
        const int count = drawList->_Path.Size - 1;

        unsigned int vtx_base = drawList->_VtxCurrentIdx;
        drawList->PrimReserve(count * 3, count + 1);

        // Submit vertices
        const ImVec2 uv = drawList->_Data->TexUvWhitePixel;
        drawList->PrimWriteVtx(center, uv, in);
        for (int n = 0; n < count; n++)
            drawList->PrimWriteVtx(drawList->_Path[n], uv, out);

        // Submit a fan of triangles
        for (int n = 0; n < count; n++)
        {
            drawList->PrimWriteIdx((ImDrawIdx)(vtx_base));
            drawList->PrimWriteIdx((ImDrawIdx)(vtx_base + 1 + n));
            drawList->PrimWriteIdx((ImDrawIdx)(vtx_base + 1 + ((n + 1) % count)));
        }

        drawList->_Path.Size = 0;
    }

    bool ImGuiRenderer::SetCurrentFont(std::string_view family, float sz, FontType type)
    {
        auto font = GetFont(family, sz, type);

        if (font != nullptr)
        {
            _currentFontSz = sz;
            ImGui::PushFont((ImFont*)font);
            return true;
        }

        return false;
    }

    bool ImGuiRenderer::SetCurrentFont(void* fontptr, float sz)
    {
        if (fontptr != nullptr)
        {
            _currentFontSz = sz;
            ImGui::PushFont((ImFont*)fontptr);
            return true;
        }

        return false;
    }

    void ImGuiRenderer::ResetFont()
    {
        ImGui::PopFont();
    }

    ImVec2 ImGuiRenderer::GetTextSize(std::string_view text, void* fontptr, float sz, float wrapWidth)
    {
        auto imfont = (ImFont*)fontptr;
        ImGui::PushFont(imfont);
        auto txtsz = ImGui::CalcTextSize(text.data(), text.data() + text.size(), false, wrapWidth);
        ImGui::PopFont();

        auto ratio = (sz / imfont->FontSize);
        txtsz.x *= ratio;
        txtsz.y *= ratio;
        return txtsz;
    }

    void ImGuiRenderer::DrawText(std::string_view text, ImVec2 pos, uint32_t color, float wrapWidth)
    {
        auto font = ImGui::GetFont();
        ((ImDrawList*)UserData)->AddText(font, _currentFontSz, pos, color, text.data(), text.data() + text.size(), 
            wrapWidth);
    }

    void ImGuiRenderer::DrawTooltip(ImVec2 pos, std::string_view text)
    {
        if (!text.empty())
        {
            //SetCurrentFont(config.DefaultFontFamily, GlobalWindowConfig.defaultFontSz, FT_Normal);
            ImGui::SetTooltip("%.*s", (int)text.size(), text.data());
            ResetFont();
        }
    }

    float ImGuiRenderer::EllipsisWidth(void* fontptr, float sz)
    {
        return ((ImFont*)fontptr)->EllipsisWidth;
    }

    void ImGuiRenderer::ConstructRoundedRect(ImVec2 startpos, ImVec2 endpos, float topleftr, float toprightr, float bottomrightr, float bottomleftr)
    {
        auto& dl = *((ImDrawList*)UserData);
        auto minlength = std::min(endpos.x - startpos.x, endpos.y - startpos.y);
        topleftr = std::min(topleftr, minlength);
        toprightr = std::min(toprightr, minlength);
        bottomrightr = std::min(bottomrightr, minlength);
        bottomleftr = std::min(bottomleftr, minlength);

        dl.PathClear();
        dl.PathLineTo(ImVec2{ startpos.x, endpos.y - bottomleftr });
        dl.PathLineTo(ImVec2{ startpos.x, startpos.y + topleftr });
        if (topleftr > 0.f) dl.PathArcToFast(ImVec2{ startpos.x + topleftr, startpos.y + topleftr }, topleftr, 6, 9);
        dl.PathLineTo(ImVec2{ endpos.x - toprightr, startpos.y });
        if (toprightr > 0.f) dl.PathArcToFast(ImVec2{ endpos.x - toprightr, startpos.y + toprightr }, toprightr, 9, 12);
        dl.PathLineTo(ImVec2{ endpos.x, endpos.y - bottomrightr });
        if (bottomrightr > 0.f) dl.PathArcToFast(ImVec2{ endpos.x - bottomrightr, endpos.y - bottomrightr }, bottomrightr, 0, 3);
        dl.PathLineTo(ImVec2{ startpos.x - bottomleftr, endpos.y });
        if (bottomleftr > 0.f) dl.PathArcToFast(ImVec2{ startpos.x + bottomleftr, endpos.y - bottomleftr }, bottomleftr, 3, 6);
    }

    float IRenderer::EllipsisWidth(void* fontptr, float sz)
    {
        return GetTextSize("...", fontptr, sz).x;
    }

#pragma endregion

    // =============================================================================================
    // STYLING FUNCTIONS
    // =============================================================================================

#pragma region Style parsing

#pragma optimize( "", on )
    [[nodiscard]] int SkipSpace(const char* text, int idx, int end)
    {
        while ((idx < end) && std::isspace(text[idx])) idx++;
        return idx;
    }

    [[nodiscard]] int SkipSpace(const std::string_view text, int from = 0)
    {
        auto end = (int)text.size();
        while ((from < end) && (std::isspace(text[from]))) from++;
        return from;
    }

    [[nodiscard]] int WholeWord(const std::string_view text, int from = 0)
    {
        auto end = (int)text.size();
        while ((from < end) && (!std::isspace(text[from]))) from++;
        return from;
    }

    [[nodiscard]] int SkipDigits(const std::string_view text, int from = 0)
    {
        auto end = (int)text.size();
        while ((from < end) && (std::isdigit(text[from]))) from++;
        return from;
    }

    [[nodiscard]] int SkipFDigits(const std::string_view text, int from = 0)
    {
        auto end = (int)text.size();
        while ((from < end) && ((std::isdigit(text[from])) || (text[from] == '.'))) from++;
        return from;
    }
#pragma optimize( "", off )

    [[nodiscard]] bool AreSame(const std::string_view lhs, const char* rhs)
    {
        auto rlimit = (int)std::strlen(rhs);
        auto llimit = (int)lhs.size();
        if (rlimit != llimit)
            return false;

        for (auto idx = 0; idx < rlimit; ++idx)
            if (std::tolower(lhs[idx]) != std::tolower(rhs[idx]))
                return false;

        return true;
    }

    [[nodiscard]] bool StartsWith(const std::string_view lhs, const char* rhs)
    {
        auto rlimit = (int)std::strlen(rhs);
        auto llimit = (int)lhs.size();
        if (rlimit > llimit)
            return false;

        for (auto idx = 0; idx < rlimit; ++idx)
            if (std::tolower(lhs[idx]) != std::tolower(rhs[idx]))
                return false;

        return true;
    }

    [[nodiscard]] bool AreSame(const std::string_view lhs, const std::string_view rhs)
    {
        auto rlimit = (int)rhs.size();
        auto llimit = (int)lhs.size();
        if (rlimit != llimit)
            return false;

        for (auto idx = 0; idx < rlimit; ++idx)
            if (std::tolower(lhs[idx]) != std::tolower(rhs[idx]))
                return false;

        return true;
    }

    [[nodiscard]] int ExtractInt(std::string_view input, int defaultVal)
    {
        int result = defaultVal;
        int base = 1;
        auto idx = (int)input.size() - 1;
        while (!std::isdigit(input[idx]) && idx >= 0) idx--;

        for (; idx >= 0; --idx)
        {
            result += (input[idx] - '0') * base;
            base *= 10;
        }

        return result;
    }

    [[nodiscard]] int ExtractIntFromHex(std::string_view input, int defaultVal)
    {
        int result = defaultVal;
        int base = 1;
        auto idx = (int)input.size() - 1;
        while (!std::isdigit(input[idx]) && !std::isalpha(input[idx]) && idx >= 0) idx--;

        for (; idx >= 0; --idx)
        {
            result += std::isdigit(input[idx]) ? (input[idx] - '0') * base :
                std::islower(input[idx]) ? ((input[idx] - 'a') + 10) * base :
                ((input[idx] - 'A') + 10) * base;
            base *= 16;
        }

        return result;
    }

    [[nodiscard]] IntOrFloat ExtractNumber(std::string_view input, float defaultVal)
    {
        float result = 0.f, base = 1.f;
        bool isInt = false;
        auto idx = (int)input.size() - 1;

        while (idx >= 0 && input[idx] != '.') idx--;
        auto decimal = idx;

        if (decimal != -1)
        {
            for (auto midx = decimal; midx >= 0; --midx)
            {
                result += (input[midx] - '0') * base;
                base *= 10.f;
            }

            base = 0.1f;
            for (auto midx = decimal + 1; midx < (int)input.size(); ++midx)
            {
                result += (input[midx] - '0') * base;
                base *= 0.1f;
            }
        }
        else
        {
            for (auto midx = (int)input.size() - 1; midx >= 0; --midx)
            {
                result += (input[midx] - '0') * base;
                base *= 10.f;
            }

            isInt = true;
        }

        return { result, !isInt };
    }

    [[nodiscard]] float ExtractFloatWithUnit(std::string_view input, float defaultVal, float ems, float parent, float scale)
    {
        float result = defaultVal, base = 1.f;
        auto idx = (int)input.size() - 1;
        while (!std::isdigit(input[idx]) && idx >= 0) idx--;

        if (AreSame(input.substr(idx + 1), "pt")) scale = 1.3333f;
        else if (AreSame(input.substr(idx + 1), "em")) scale = ems;
        else if (input[idx + 1] == '%') scale = parent * 0.01f;

        auto num = ExtractNumber(input.substr(0, idx + 1), defaultVal);
        result = num.value;

        return result * scale;
    }

    uint32_t ToRGBA(int r, int g, int b, int a)
    {
        return (((uint32_t)(a) << 24) |
            ((uint32_t)(b) << 16) |
            ((uint32_t)(g) << 8) |
            ((uint32_t)(r) << 0));
    }

    uint32_t ToRGBA(const std::tuple<int, int, int>& rgb)
    {
        return ToRGBA(std::get<0>(rgb), std::get<1>(rgb), std::get<2>(rgb), 255);
    }

    uint32_t ToRGBA(const std::tuple<int, int, int, int>& rgba)
    {
        return ToRGBA(std::get<0>(rgba), std::get<1>(rgba), std::get<2>(rgba), std::get<3>(rgba));
    }

    uint32_t ToRGBA(float r, float g, float b, float a)
    {
        return ToRGBA((int)(r * 255.f), (int)(g * 255.f), (int)(b * 255.f), (int)(a * 255.f));
    }

#ifndef IM_RICHTEXT_TARGET_IMGUI
    static void ColorConvertHSVtoRGB(float h, float s, float v, float& out_r, float& out_g, float& out_b)
    {
        if (s == 0.0f)
        {
            // gray
            out_r = out_g = out_b = v;
            return;
        }

        h = std::fmodf(h, 1.0f) / (60.0f / 360.0f);
        int   i = (int)h;
        float f = h - (float)i;
        float p = v * (1.0f - s);
        float q = v * (1.0f - s * f);
        float t = v * (1.0f - s * (1.0f - f));

        switch (i)
        {
        case 0: out_r = v; out_g = t; out_b = p; break;
        case 1: out_r = q; out_g = v; out_b = p; break;
        case 2: out_r = p; out_g = v; out_b = t; break;
        case 3: out_r = p; out_g = q; out_b = v; break;
        case 4: out_r = t; out_g = p; out_b = v; break;
        case 5: default: out_r = v; out_g = p; out_b = q; break;
        }
    }
#endif

    [[nodiscard]] std::tuple<IntOrFloat, IntOrFloat, IntOrFloat, IntOrFloat> GetCommaSeparatedNumbers(std::string_view stylePropVal, int& curr)
    {
        std::tuple<IntOrFloat, IntOrFloat, IntOrFloat, IntOrFloat> res;
        auto hasFourth = curr == 4;
        curr = SkipSpace(stylePropVal, curr);
        assert(stylePropVal[curr] == '(');
        curr++;

        auto valstart = curr;
        curr = SkipFDigits(stylePropVal, curr);
        std::get<0>(res) = ExtractNumber(stylePropVal.substr(valstart, curr - valstart), 0);
        curr = SkipSpace(stylePropVal, curr);
        assert(stylePropVal[curr] == ',');
        curr++;
        curr = SkipSpace(stylePropVal, curr);

        valstart = curr;
        curr = SkipFDigits(stylePropVal, curr);
        std::get<1>(res) = ExtractNumber(stylePropVal.substr(valstart, curr - valstart), 0);
        curr = SkipSpace(stylePropVal, curr);
        assert(stylePropVal[curr] == ',');
        curr++;
        curr = SkipSpace(stylePropVal, curr);

        valstart = curr;
        curr = SkipFDigits(stylePropVal, curr);
        std::get<2>(res) = ExtractNumber(stylePropVal.substr(valstart, curr - valstart), 0);
        curr = SkipSpace(stylePropVal, curr);

        if (hasFourth)
        {
            assert(stylePropVal[curr] == ',');
            curr++;
            curr = SkipSpace(stylePropVal, curr);

            valstart = curr;
            curr = SkipFDigits(stylePropVal, curr);
            std::get<3>(res) = ExtractNumber(stylePropVal.substr(valstart, curr - valstart), 0);
        }

        return res;
    }

    [[nodiscard]] uint32_t ExtractColor(std::string_view stylePropVal, uint32_t(*NamedColor)(const char*, void*), void* userData)
    {
        if (stylePropVal.size() >= 3u && AreSame(stylePropVal.substr(0, 3), "rgb"))
        {
            IntOrFloat r, g, b, a;
            auto hasAlpha = stylePropVal[3] == 'a' || stylePropVal[3] == 'A';
            int curr = hasAlpha ? 4 : 3;
            std::tie(r, g, b, a) = GetCommaSeparatedNumbers(stylePropVal, curr);
            auto isRelative = r.isFloat && g.isFloat && b.isFloat;
            a.value = isRelative ? hasAlpha ? a.value : 1.f :
                hasAlpha ? a.value : 255;

            assert(stylePropVal[curr] == ')');
            return isRelative ? ToRGBA(r.value, g.value, b.value, a.value) :
                ToRGBA((int)r.value, (int)g.value, (int)b.value, (int)a.value);
        }
        else if (stylePropVal.size() >= 3u && AreSame(stylePropVal.substr(0, 3), "hsv"))
        {
            IntOrFloat h, s, v;
            auto curr = 3;
            std::tie(h, s, v, std::ignore) = GetCommaSeparatedNumbers(stylePropVal, curr);

            assert(stylePropVal[curr] == ')');
            return ImColor::HSV(h.value, s.value, v.value);
        }
        else if (stylePropVal.size() >= 3u && AreSame(stylePropVal.substr(0, 3), "hsl"))
        {
            IntOrFloat h, s, l;
            auto curr = 3;
            std::tie(h, s, l, std::ignore) = GetCommaSeparatedNumbers(stylePropVal, curr);
            auto v = l.value + s.value * std::min(l.value, 1.f - l.value);
            s.value = v == 0.f ? 0.f : 2.f * (1.f - (l.value / v));

            assert(stylePropVal[curr] == ')');
            return ImColor::HSV(h.value, s.value, v);
        }
        else if (stylePropVal.size() >= 1u && stylePropVal[0] == '#')
        {
            return (uint32_t)ExtractIntFromHex(stylePropVal.substr(1), 0);
        }
        else if (NamedColor != nullptr)
        {
            static char buffer[32] = { 0 };
            std::memset(buffer, 0, 32);
            std::memcpy(buffer, stylePropVal.data(), std::min((int)stylePropVal.size(), 31));
            return NamedColor(buffer, userData);
        }
        else
        {
            return IM_COL32_BLACK;
        }
    }

    std::pair<uint32_t, float> ExtractColorStop(std::string_view input, uint32_t(*NamedColor)(const char*, void*), void* userData)
    {
        auto idx = 0;
        std::pair<uint32_t, float> colorstop;

        idx = WholeWord(input, idx);
        colorstop.first = ExtractColor(input.substr(0, idx), NamedColor, userData);
        idx = SkipSpace(input, idx);

        if ((idx < (int)input.size()) && std::isdigit(input[idx]))
        {
            auto start = idx;
            idx = SkipDigits(input, start);
            colorstop.second = ExtractNumber(input.substr(start, idx - start), -1.f).value;
        }
        else colorstop.second = -1.f;

        return colorstop;
    }

    ColorGradient ExtractLinearGradient(std::string_view input, uint32_t(*NamedColor)(const char*, void*), void* userData)
    {
        ColorGradient gradient;
        auto idx = 15; // size of "linear-gradient" string

        if (idx < (int)input.size())
        {
            idx = SkipSpace(input, idx);
            assert(input[idx] == '(');
            idx++;
            idx = SkipSpace(input, idx);

            std::optional<std::pair<uint32_t, float>> lastStop = std::nullopt;
            auto firstPart = true;
            auto start = idx;
            auto total = 0.f, unspecified = 0.f;

            do
            {
                idx = SkipSpace(input, idx);

                auto start = idx;
                while ((idx < (int)input.size()) && (input[idx] != ',') && (input[idx] != ')')
                    && !std::isspace(input[idx])) idx++;
                auto part = input.substr(start, idx - start);
                idx = SkipSpace(input, idx);
                auto isEnd = input[idx] == ')';

                if ((idx < (int)input.size()) && (input[idx] == ',' || isEnd)) {
                    if (firstPart)
                    {
                        if (AreSame(input, "to right")) {
                            gradient.dir = ImGuiDir::ImGuiDir_Right;
                        }
                        else if (AreSame(input, "to left")) {
                            gradient.dir = ImGuiDir::ImGuiDir_Left;
                        }
                        else {
                            auto colorstop = ExtractColorStop(part, NamedColor, userData);
                            if (colorstop.second != -1.f) total += colorstop.second;
                            else unspecified += 1.f;
                            lastStop = colorstop;
                        }
                        firstPart = false;
                    }
                    else {
                        auto colorstop = ExtractColorStop(part, NamedColor, userData);
                        if (colorstop.second != -1.f) total += colorstop.second;
                        else unspecified += 1.f;

                        if (lastStop.has_value())
                        {
                            gradient.colorStops[gradient.totalStops] =
                                ColorStop{ lastStop.value().first, colorstop.first, colorstop.second };
                            gradient.totalStops = std::min(gradient.totalStops + 1, IM_RICHTEXT_MAX_COLORSTOPS - 1);
                        }

                        lastStop = colorstop;
                    }
                }
                else break;

                if (isEnd) break;
                else if (input[idx] == ',') idx++;
            } while ((idx < (int)input.size()) && (input[idx] != ')'));

            unspecified -= 1.f;
            for (auto& colorstop : gradient.colorStops)
                if (colorstop.pos == -1.f) colorstop.pos = (100.f - total) / (100.f * unspecified);
                else colorstop.pos /= 100.f;
        }

        return gradient;
    }

    template <int maxsz>
    struct CaseInsensitiveHasher
    {
        std::size_t operator()(std::string_view key) const
        {
            thread_local static char buffer[maxsz] = { 0 };
            std::memset(buffer, 0, maxsz);
            auto limit = std::min((int)key.size(), maxsz - 1);

            for (auto idx = 0; idx < limit; ++idx)
                buffer[idx] = std::tolower(key[idx]);

            return std::hash<std::string_view>()(buffer);
        }
    };

    uint32_t GetColor(const char* name, void*)
    {
        const static std::unordered_map<std::string_view, uint32_t, CaseInsensitiveHasher<32>> Colors{
            { "black", ToRGBA(0, 0, 0) },
            { "silver", ToRGBA(192, 192, 192) },
            { "gray", ToRGBA(128, 128, 128) },
            { "white", ToRGBA(255, 255, 255) },
            { "maroon", ToRGBA(128, 0, 0) },
            { "red", ToRGBA(255, 0, 0) },
            { "purple", ToRGBA(128, 0, 128) },
            { "fuchsia", ToRGBA(255, 0, 255) },
            { "green", ToRGBA(0, 128, 0) },
            { "lime", ToRGBA(0, 255, 0) },
            { "olive", ToRGBA(128, 128, 0) },
            { "yellow", ToRGBA(255, 255, 0) },
            { "navy", ToRGBA(0, 0, 128) },
            { "blue", ToRGBA(0, 0, 255) },
            { "teal", ToRGBA(0, 128, 128) },
            { "aqua", ToRGBA(0, 255, 255) },
            { "aliceblue", ToRGBA(240, 248, 255) },
            { "antiquewhite", ToRGBA(250, 235, 215) },
            { "aquamarine", ToRGBA(127, 255, 212) },
            { "azure", ToRGBA(240, 255, 255) },
            { "beige", ToRGBA(245, 245, 220) },
            { "bisque", ToRGBA(255, 228, 196) },
            { "blanchedalmond", ToRGBA(255, 235, 205) },
            { "blueviolet", ToRGBA(138, 43, 226) },
            { "brown", ToRGBA(165, 42, 42) },
            { "burlywood", ToRGBA(222, 184, 135) },
            { "cadetblue", ToRGBA(95, 158, 160) },
            { "chartreuse", ToRGBA(127, 255, 0) },
            { "chocolate", ToRGBA(210, 105, 30) },
            { "coral", ToRGBA(255, 127, 80) },
            { "cornflowerblue", ToRGBA(100, 149, 237) },
            { "cornsilk", ToRGBA(255, 248, 220) },
            { "crimson", ToRGBA(220, 20, 60) },
            { "darkblue", ToRGBA(0, 0, 139) },
            { "darkcyan", ToRGBA(0, 139, 139) },
            { "darkgoldenrod", ToRGBA(184, 134, 11) },
            { "darkgray", ToRGBA(169, 169, 169) },
            { "darkgreen", ToRGBA(0, 100, 0) },
            { "darkgrey", ToRGBA(169, 169, 169) },
            { "darkkhaki", ToRGBA(189, 183, 107) },
            { "darkmagenta", ToRGBA(139, 0, 139) },
            { "darkolivegreen", ToRGBA(85, 107, 47) },
            { "darkorange", ToRGBA(255, 140, 0) },
            { "darkorchid", ToRGBA(153, 50, 204) },
            { "darkred", ToRGBA(139, 0, 0) },
            { "darksalmon", ToRGBA(233, 150, 122) },
            { "darkseagreen", ToRGBA(143, 188, 143) },
            { "darkslateblue", ToRGBA(72, 61, 139) },
            { "darkslategray", ToRGBA(47, 79, 79) },
            { "darkslategray", ToRGBA(47, 79, 79) },
            { "darkturquoise", ToRGBA(0, 206, 209) },
            { "darkviolet", ToRGBA(148, 0, 211) },
            { "deeppink", ToRGBA(255, 20, 147) },
            { "deepskyblue", ToRGBA(0, 191, 255) },
            { "dimgray", ToRGBA(105, 105, 105) },
            { "dimgrey", ToRGBA(105, 105, 105) },
            { "dodgerblue", ToRGBA(30, 144, 255) },
            { "firebrick", ToRGBA(178, 34, 34) },
            { "floralwhite", ToRGBA(255, 250, 240) },
            { "forestgreen", ToRGBA(34, 139, 34) },
            { "gainsboro", ToRGBA(220, 220, 220) },
            { "ghoshtwhite", ToRGBA(248, 248, 255) },
            { "gold", ToRGBA(255, 215, 0) },
            { "goldenrod", ToRGBA(218, 165, 32) },
            { "greenyellow", ToRGBA(173, 255, 47) },
            { "honeydew", ToRGBA(240, 255, 240) },
            { "hotpink", ToRGBA(255, 105, 180) },
            { "indianred", ToRGBA(205, 92, 92) },
            { "indigo", ToRGBA(75, 0, 130) },
            { "ivory", ToRGBA(255, 255, 240) },
            { "khaki", ToRGBA(240, 230, 140) },
            { "lavender", ToRGBA(230, 230, 250) },
            { "lavenderblush", ToRGBA(255, 240, 245) },
            { "lawngreen", ToRGBA(124, 252, 0) },
            { "lemonchiffon", ToRGBA(255, 250, 205) },
            { "lightblue", ToRGBA(173, 216, 230) },
            { "lightcoral", ToRGBA(240, 128, 128) },
            { "lightcyan", ToRGBA(224, 255, 255) },
            { "lightgoldenrodyellow", ToRGBA(250, 250, 210) },
            { "lightgray", ToRGBA(211, 211, 211) },
            { "lightgreen", ToRGBA(144, 238, 144) },
            { "lightgrey", ToRGBA(211, 211, 211) },
            { "lightpink", ToRGBA(255, 182, 193) },
            { "lightsalmon", ToRGBA(255, 160, 122) },
            { "lightseagreen", ToRGBA(32, 178, 170) },
            { "lightskyblue", ToRGBA(135, 206, 250) },
            { "lightslategray", ToRGBA(119, 136, 153) },
            { "lightslategrey", ToRGBA(119, 136, 153) },
            { "lightsteelblue", ToRGBA(176, 196, 222) },
            { "lightyellow", ToRGBA(255, 255, 224) },
            { "lilac", ToRGBA(200, 162, 200) },
            { "limegreen", ToRGBA(50, 255, 50) },
            { "linen", ToRGBA(250, 240, 230) },
            { "mediumaquamarine", ToRGBA(102, 205, 170) },
            { "mediumblue", ToRGBA(0, 0, 205) },
            { "mediumorchid", ToRGBA(186, 85, 211) },
            { "mediumpurple", ToRGBA(147, 112, 219) },
            { "mediumseagreen", ToRGBA(60, 179, 113) },
            { "mediumslateblue", ToRGBA(123, 104, 238) },
            { "mediumspringgreen", ToRGBA(0, 250, 154) },
            { "mediumturquoise", ToRGBA(72, 209, 204) },
            { "mediumvioletred", ToRGBA(199, 21, 133) },
            { "midnightblue", ToRGBA(25, 25, 112) },
            { "mintcream", ToRGBA(245, 255, 250) },
            { "mistyrose", ToRGBA(255, 228, 225) },
            { "moccasin", ToRGBA(255, 228, 181) },
            { "navajowhite", ToRGBA(255, 222, 173) },
            { "oldlace", ToRGBA(253, 245, 230) },
            { "olivedrab", ToRGBA(107, 142, 35) },
            { "orange", ToRGBA(255, 165, 0) },
            { "orangered", ToRGBA(255, 69, 0) },
            { "orchid", ToRGBA(218, 112, 214) },
            { "palegoldenrod", ToRGBA(238, 232, 170) },
            { "palegreen", ToRGBA(152, 251, 152) },
            { "paleturquoise", ToRGBA(175, 238, 238) },
            { "palevioletred", ToRGBA(219, 112, 147) },
            { "papayawhip", ToRGBA(255, 239, 213) },
            { "peachpuff", ToRGBA(255, 218, 185) },
            { "peru", ToRGBA(205, 133, 63) },
            { "pink", ToRGBA(255, 192, 203) },
            { "plum", ToRGBA(221, 160, 221) },
            { "powderblue", ToRGBA(176, 224, 230) },
            { "rosybrown", ToRGBA(188, 143, 143) },
            { "royalblue", ToRGBA(65, 105, 225) },
            { "saddlebrown", ToRGBA(139, 69, 19) },
            { "salmon", ToRGBA(250, 128, 114) },
            { "sandybrown", ToRGBA(244, 164, 96) },
            { "seagreen", ToRGBA(46, 139, 87) },
            { "seashell", ToRGBA(255, 245, 238) },
            { "sienna", ToRGBA(160, 82, 45) },
            { "skyblue", ToRGBA(135, 206, 235) },
            { "slateblue", ToRGBA(106, 90, 205) },
            { "slategray", ToRGBA(112, 128, 144) },
            { "slategrey", ToRGBA(112, 128, 144) },
            { "snow", ToRGBA(255, 250, 250) },
            { "springgreen", ToRGBA(0, 255, 127) },
            { "steelblue", ToRGBA(70, 130, 180) },
            { "tan", ToRGBA(210, 180, 140) },
            { "thistle", ToRGBA(216, 191, 216) },
            { "tomato", ToRGBA(255, 99, 71) },
            { "violet", ToRGBA(238, 130, 238) },
            { "wheat", ToRGBA(245, 222, 179) },
            { "whitesmoke", ToRGBA(245, 245, 245) },
            { "yellowgreen", ToRGBA(154, 205, 50) }
        };

        auto it = Colors.find(name);
        return it != Colors.end() ? it->second : uint32_t{ IM_COL32_BLACK };
    }

    bool IsColorVisible(uint32_t color)
    {
        return (color & 0xFF000000) != 0;
    }

    Border ExtractBorder(std::string_view input, float ems, float percent,
        uint32_t(*NamedColor)(const char*, void*), void* userData)
    {
        Border result;
        auto idx = WholeWord(input);

        if (AreSame(input.substr(0, idx), "none")) return result;
        result.thickness = ExtractFloatWithUnit(input.substr(0, idx), 1.f, ems, percent, 1.f);
        idx = SkipSpace(input, idx);

        auto idx2 = WholeWord(input, idx + 1);
        auto type = input.substr(idx + 1, idx2);
        if (AreSame(type, "solid")) result.lineType = LineType::Solid;
        else if (AreSame(type, "dashed")) result.lineType = LineType::Dashed;
        else if (AreSame(type, "dotted")) result.lineType = LineType::Dotted;

        idx2 = SkipSpace(input, idx2);
        auto idx3 = WholeWord(input, idx2 + 1);
        auto color = input.substr(idx2 + 1, idx3);
        result.color = ExtractColor(color, NamedColor, userData);

        return result;
    }

    static bool IsColor(std::string_view input, int from)
    {
        return input[from] != '-' && !std::isdigit(input[from]);
    }

    BoxShadow ExtractBoxShadow(std::string_view input, float ems, float percent,
        uint32_t(*NamedColor)(const char*, void*), void* userData)
    {
        BoxShadow result;
        auto idx = WholeWord(input);

        if (AreSame(input.substr(0, idx), "none")) return result;
        result.offset.x = ExtractFloatWithUnit(input.substr(0, idx), 0.f, ems, percent, 1.f);
        idx = SkipSpace(input, idx);

        auto prev = idx;
        idx = WholeWord(input, idx);

        if (!IsColor(input, prev))
        {
            result.offset.y = ExtractFloatWithUnit(input.substr(prev, (idx - prev)), 0.f, ems, percent, 1.f);
            idx = SkipSpace(input, idx);

            prev = idx;
            idx = WholeWord(input, idx);

            if (!IsColor(input, prev))
            {
                result.blur = ExtractFloatWithUnit(input.substr(prev, (idx - prev)), 0.f, ems, percent, 1.f);
                idx = SkipSpace(input, idx);

                prev = idx;
                idx = WholeWord(input, idx);

                if (!IsColor(input, prev))
                {
                    result.spread = ExtractFloatWithUnit(input.substr(prev, (idx - prev)), 0.f, ems, percent, 1.f);
                    idx = SkipSpace(input, idx);

                    prev = idx;
                    idx = WholeWord(input, idx);
                    result.color = ExtractColor(input.substr(prev, (idx - prev)), NamedColor, userData);
                }
                else
                    result.color = ExtractColor(input.substr(prev, (idx - prev)), NamedColor, userData);
            }
            else
                result.color = ExtractColor(input.substr(prev, (idx - prev)), NamedColor, userData);
        }
        else
            result.color = ExtractColor(input.substr(prev, (idx - prev)), NamedColor, userData);
        return result;
    }

    std::pair<std::string_view, bool> ExtractTag(const char* text, int end, char TagEnd,
        int& idx, bool& tagStart)
    {
        std::pair<std::string_view, bool> result;
        result.second = true;

        if (text[idx] == '/')
        {
            tagStart = false;
            idx++;
        }
        else if (!std::isalnum(text[idx]))
        {
            result.second = false;
            return result;
        }

        auto begin = idx;
        while ((idx < end) && !std::isspace(text[idx]) && (text[idx] != TagEnd)) idx++;

        if (idx - begin == 0)
        {
            result.second = false;
            return result;
        }

        result.first = std::string_view{ text + begin, (std::size_t)(idx - begin) };
        if (result.first.back() == '/') result.first = result.first.substr(0, result.first.size() - 1u);

        if (!tagStart)
        {
            if (text[idx] == TagEnd) idx++;

            if (result.first.empty())
            {
                result.second = false;
                return result;
            }
        }

        idx = SkipSpace(text, idx, end);
        return result;
    }

    [[nodiscard]] std::optional<std::string_view> GetQuotedString(const char* text, int& idx, int end)
    {
        auto insideQuotedString = false;
        auto begin = idx;

        if ((idx < end) && text[idx] == '\'')
        {
            idx++;

            while (idx < end)
            {
                if (text[idx] == '\\' && ((idx + 1 < end) && text[idx + 1] == '\''))
                {
                    insideQuotedString = !insideQuotedString;
                    idx++;
                }
                else if (!insideQuotedString && text[idx] == '\'') break;
                idx++;
            }
        }
        else if ((idx < end) && text[idx] == '"')
        {
            idx++;

            while (idx < end)
            {
                if (text[idx] == '\\' && ((idx + 1 < end) && text[idx + 1] == '"'))
                {
                    insideQuotedString = !insideQuotedString;
                    idx++;
                }
                else if (!insideQuotedString && text[idx] == '"') break;
                idx++;
            }
        }

        if ((idx < end) && (text[idx] == '"' || text[idx] == '\''))
        {
            std::string_view res{ text + begin + 1, (std::size_t)(idx - begin - 1) };
            idx++;
            return res;
        }

        return std::nullopt;
    }

    bool FourSidedBorder::isRounded() const
    {
        return cornerRadius[TopLeftCorner] > 0.f ||
            cornerRadius[TopRightCorner] > 0.f ||
            cornerRadius[BottomRightCorner] > 0.f ||
            cornerRadius[BottomLeftCorner] > 0.f;
    }

    bool FourSidedBorder::exists() const
    {
        return top.thickness > 0.f || bottom.thickness > 0.f ||
            left.thickness > 0.f || right.thickness > 0.f;
    }

    FourSidedBorder& FourSidedBorder::setColor(uint32_t color)
    {
        left.color = right.color = top.color = bottom.color = color;
        return *this;
    }

    FourSidedBorder& FourSidedBorder::setThickness(float thickness)
    {
        left.thickness = right.thickness = top.thickness = bottom.thickness = thickness;
        return *this;
    }

    FourSidedBorder& FourSidedBorder::setRadius(float radius)
    {
        cornerRadius[0] = cornerRadius[1] = cornerRadius[2] = cornerRadius[3] = radius;
        return *this;
    }

    static int PopulateSegmentStyle(StyleDescriptor& style, CommonWidgetStyleDescriptor& specific, 
        std::string_view stylePropName, std::string_view stylePropVal)
    {
        int prop = NoStyleChange;

        if (AreSame(stylePropName, "font-size"))
        {
            if (AreSame(stylePropVal, "xx-small")) style.font.size = GlobalWindowConfig.defaultFontSz * 0.6f * GlobalWindowConfig.fontScaling;
            else if (AreSame(stylePropVal, "x-small")) style.font.size = GlobalWindowConfig.defaultFontSz * 0.75f * GlobalWindowConfig.fontScaling;
            else if (AreSame(stylePropVal, "small")) style.font.size = GlobalWindowConfig.defaultFontSz * 0.89f * GlobalWindowConfig.fontScaling;
            else if (AreSame(stylePropVal, "medium")) style.font.size = GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling;
            else if (AreSame(stylePropVal, "large")) style.font.size = GlobalWindowConfig.defaultFontSz * 1.2f * GlobalWindowConfig.fontScaling;
            else if (AreSame(stylePropVal, "x-large")) style.font.size = GlobalWindowConfig.defaultFontSz * 1.5f * GlobalWindowConfig.fontScaling;
            else if (AreSame(stylePropVal, "xx-large")) style.font.size = GlobalWindowConfig.defaultFontSz * 2.f * GlobalWindowConfig.fontScaling;
            else if (AreSame(stylePropVal, "xxx-large")) style.font.size = GlobalWindowConfig.defaultFontSz * 3.f * GlobalWindowConfig.fontScaling;
            else
                style.font.size = ExtractFloatWithUnit(stylePropVal, GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling,
                    GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling, 1.f, GlobalWindowConfig.fontScaling);
            prop = StyleFontSize;
        }
        else if (AreSame(stylePropName, "font-weight"))
        {
            auto idx = SkipDigits(stylePropVal);

            if (idx == 0)
            {
                if (AreSame(stylePropVal, "bold")) style.font.flags |= FontStyleBold;
                else if (AreSame(stylePropVal, "light")) style.font.flags |= FontStyleLight;
                else ERROR("Invalid font-weight property value... [%.*s]\n",
                    (int)stylePropVal.size(), stylePropVal.data());
            }
            else
            {
                int weight = ExtractInt(stylePropVal.substr(0u, idx), 400);
                if (weight >= 600) style.font.flags |= FontStyleBold;
                if (weight < 400) style.font.flags |= FontStyleLight;
            }

            prop = StyleFontWeight;
        }
        else if (AreSame(stylePropName, "text-wrap"))
        {
            if (AreSame(stylePropVal, "nowrap")) style.font.flags |= FontStyleNoWrap;
            prop = StyleTextWrap;
        }
        else if (AreSame(stylePropName, "background-color") || AreSame(stylePropName, "background"))
        {
            if (StartsWith(stylePropVal, "linear-gradient"))
                style.gradient = ExtractLinearGradient(stylePropVal, GetColor, GlobalWindowConfig.userData);
            else style.bgcolor = ExtractColor(stylePropVal, GetColor, GlobalWindowConfig.userData);
            prop = StyleBackground;
        }
        else if (AreSame(stylePropName, "color"))
        {
            style.fgcolor = ExtractColor(stylePropVal, GetColor, GlobalWindowConfig.userData);
            prop = StyleFgColor;
        }
        else if (AreSame(stylePropName, "width"))
        {
            style.dimension.x = ExtractFloatWithUnit(stylePropVal, 0, GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling, 1.f, GlobalWindowConfig.scaling);
            prop = StyleWidth;
        }
        else if (AreSame(stylePropName, "height"))
        {
            style.dimension.y = ExtractFloatWithUnit(stylePropVal, 0, GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling, 1.f, GlobalWindowConfig.scaling);
            prop = StyleHeight;
        }
        else if (AreSame(stylePropName, "alignment") || AreSame(stylePropName, "text-align"))
        {
            style.alignment |= AreSame(stylePropVal, "justify") ? TextAlignJustify :
                AreSame(stylePropVal, "right") ? TextAlignRight :
                AreSame(stylePropVal, "center") ? TextAlignHCenter :
                TextAlignLeft;
            prop = StyleHAlignment;
        }
        else if (AreSame(stylePropName, "vertical-align"))
        {
            style.alignment |= AreSame(stylePropVal, "top") ? TextAlignTop :
                AreSame(stylePropVal, "bottom") ? TextAlignBottom :
                TextAlignVCenter;
            prop = StyleVAlignment;
        }
        else if (AreSame(stylePropName, "font-family"))
        {
            style.font.family = stylePropVal;
            prop = StyleFontFamily;
        }
        else if (AreSame(stylePropName, "padding"))
        {
            auto val = ExtractFloatWithUnit(stylePropVal, 0.f, GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling, 1.f, GlobalWindowConfig.scaling);
            style.padding.top = style.padding.right = style.padding.left = style.padding.bottom = val;
            prop = StylePadding;
        }
        else if (AreSame(stylePropName, "padding-top"))
        {
            auto val = ExtractFloatWithUnit(stylePropVal, 0.f, GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling, 1.f, GlobalWindowConfig.scaling);
            style.padding.top = val;
            prop = StylePadding;
        }
        else if (AreSame(stylePropName, "padding-bottom"))
        {
            auto val = ExtractFloatWithUnit(stylePropVal, 0.f, GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling, 1.f, GlobalWindowConfig.scaling);
            style.padding.bottom = val;
            prop = StylePadding;
        }
        else if (AreSame(stylePropName, "padding-left"))
        {
            auto val = ExtractFloatWithUnit(stylePropVal, 0.f, GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling, 1.f, GlobalWindowConfig.scaling);
            style.padding.left = val;
            prop = StylePadding;
        }
        else if (AreSame(stylePropName, "padding-right"))
        {
            auto val = ExtractFloatWithUnit(stylePropVal, 0.f, GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling, 1.f, GlobalWindowConfig.scaling);
            style.padding.right = val;
            prop = StylePadding;
        }
        else if (AreSame(stylePropName, "text-overflow"))
        {
            if (AreSame(stylePropVal, "ellipsis"))
            {
                style.font.flags |= FontStyleOverflowEllipsis;
                prop = StyleTextOverflow;
            }
        }
        else if (AreSame(stylePropName, "border"))
        {
            style.border.top = style.border.bottom = style.border.left = style.border.right = ExtractBorder(stylePropVal,
                GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling, 1.f, GetColor, GlobalWindowConfig.userData);
            style.border.isUniform = true;
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-top"))
        {
            style.border.top = ExtractBorder(stylePropVal, GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling,
                1.f, GetColor, GlobalWindowConfig.userData);
            style.border.isUniform = false;
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-left"))
        {
            style.border.left = ExtractBorder(stylePropVal, GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling,
                1.f, GetColor, GlobalWindowConfig.userData);
            style.border.isUniform = false;
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-right"))
        {
            style.border.right = ExtractBorder(stylePropVal, GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling,
                1.f, GetColor, GlobalWindowConfig.userData);
            style.border.isUniform = false;
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-bottom"))
        {
            style.border.bottom = ExtractBorder(stylePropVal, GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling,
                1.f, GetColor, GlobalWindowConfig.userData);
            prop = StyleBorder;
            style.border.isUniform = false;
        }
        else if (AreSame(stylePropName, "border-radius"))
        {
            auto radius = ExtractFloatWithUnit(stylePropVal, 0.f, GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling,
                1.f, 1.f);
            if (stylePropVal.back() == '%') style.relativeProps |= (RSP_BorderTopLeftRadius | RSP_BorderTopRightRadius | RSP_BorderBottomLeftRadius | 
                RSP_BorderBottomRightRadius);
            style.border.setRadius(radius);
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-width"))
        {
            auto width = ExtractFloatWithUnit(stylePropVal, 0.f, GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling,
                1.f, 1.f);
            style.border.setThickness(width);
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-color"))
        {
            auto color = ExtractColor(stylePropVal, GetColor, GlobalWindowConfig.userData);
            style.border.setColor(color);
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-top-left-radius"))
        {
            style.border.cornerRadius[TopLeftCorner] = ExtractFloatWithUnit(stylePropVal, 0.f, GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling,
                1.f, 1.f);
            if (stylePropVal.back() == '%') style.relativeProps |= RSP_BorderTopLeftRadius;
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-top-right-radius"))
        {
            style.border.cornerRadius[TopRightCorner] = ExtractFloatWithUnit(stylePropVal, 0.f, GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling,
                1.f, 1.f);
            if (stylePropVal.back() == '%') style.relativeProps |= RSP_BorderTopRightRadius;
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-bottom-right-radius"))
        {
            style.border.cornerRadius[BottomRightCorner] = ExtractFloatWithUnit(stylePropVal, 0.f, GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling,
                1.f, 1.f);
            if (stylePropVal.back() == '%') style.relativeProps |= RSP_BorderBottomRightRadius;
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-bottom-left-radius"))
        {
            style.border.cornerRadius[BottomLeftCorner] = ExtractFloatWithUnit(stylePropVal, 0.f, GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling,
                1.f, 1.f);
            if (stylePropVal.back() == '%') style.relativeProps |= RSP_BorderBottomLeftRadius;
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "margin"))
        {
            style.margin.left = style.margin.right = style.margin.top = style.margin.bottom =
                ExtractFloatWithUnit(stylePropVal, 0.f, GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling, 1.f, 1.f);
            prop = StyleMargin;
        }
        else if (AreSame(stylePropName, "margin-top"))
        {
            style.margin.top = ExtractFloatWithUnit(stylePropVal, 0.f, GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling, 1.f, 1.f);
            prop = StyleMargin;
        }
        else if (AreSame(stylePropName, "margin-left"))
        {
            style.margin.left = ExtractFloatWithUnit(stylePropVal, 0.f, GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling, 1.f, 1.f);
            prop = StyleMargin;
        }
        else if (AreSame(stylePropName, "margin-right"))
        {
            style.margin.right = ExtractFloatWithUnit(stylePropVal, 0.f, GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling, 1.f, 1.f);
            prop = StyleMargin;
        }
        else if (AreSame(stylePropName, "margin-bottom"))
        {
            style.margin.bottom = ExtractFloatWithUnit(stylePropVal, 0.f, GlobalWindowConfig.defaultFontSz * GlobalWindowConfig.fontScaling, 1.f, 1.f);
            prop = StyleMargin;
        }
        else if (AreSame(stylePropName, "font-style"))
        {
            if (AreSame(stylePropVal, "normal")) style.font.flags |= FontStyleNormal;
            else if (AreSame(stylePropVal, "italic") || AreSame(stylePropVal, "oblique"))
                style.font.flags |= FontStyleItalics;
            else ERROR("Invalid font-style property value [%.*s]\n",
                (int)stylePropVal.size(), stylePropVal.data());
            prop = StyleFontStyle;
        }
        else if (AreSame(stylePropName, "box-shadow"))
        {
            style.shadow = ExtractBoxShadow(stylePropVal, GlobalWindowConfig.defaultFontSz, 1.f, GetColor, GlobalWindowConfig.userData);
            prop = StyleBoxShadow;
        }
        else if (AreSame(stylePropName, "thumb-color"))
        {
            if (StartsWith(stylePropVal, "linear-gradient"))
            {
                style.gradient = ExtractLinearGradient(stylePropVal, GetColor, GlobalWindowConfig.userData);
            }
            else specific.toggle.thumbColor = ExtractColor(stylePropVal, GetColor, GlobalWindowConfig.userData);
            prop = StyleThumbColor;
        }
        else if (AreSame(stylePropName, "track-color"))
        {
            if (StartsWith(stylePropVal, "linear-gradient"))
            {
                style.gradient = ExtractLinearGradient(stylePropVal, GetColor, GlobalWindowConfig.userData);
            }
            else specific.toggle.trackColor = ExtractColor(stylePropVal, GetColor, GlobalWindowConfig.userData);
            prop = StyleTrackColor;
        }
        else if (AreSame(stylePropName, "track-outline"))
        {
            auto brd = ExtractBorder(stylePropVal, GlobalWindowConfig.defaultFontSz, 1.f, GetColor, GlobalWindowConfig.userData);
            specific.toggle.trackBorderColor = brd.color;
            specific.toggle.trackBorderThickness = brd.thickness;
            prop = StyleTrackOutlineColor;
        }
        else if (AreSame(stylePropName, "thumb-offset"))
        {
            specific.toggle.thumbOffset = ExtractFloatWithUnit(stylePropVal, 0.f, GlobalWindowConfig.defaultFontSz* GlobalWindowConfig.fontScaling, 1.f, 1.f);
            prop = StyleThumbOffset;
        }
        else
        {
            ERROR("Invalid style property... [%.*s]\n", (int)stylePropName.size(), stylePropName.data());
        }

        return prop;
    }

#pragma endregion

    // =============================================================================================
    // DRAWING FUNCTIONS
    // =============================================================================================

#pragma region Drawing functions

    IntersectRects ComputeIntersectRects(ImRect rect, ImVec2 startpos, ImVec2 endpos)
    {
        IntersectRects res;

        res.intersects[0].Max.x = startpos.x;
        res.intersects[1].Max.y = startpos.y;
        res.intersects[2].Min.x = endpos.x;
        res.intersects[3].Min.y = endpos.y;

        if (rect.Min.x < startpos.x)
        {
            res.intersects[0].Min.x = res.intersects[1].Min.x = res.intersects[3].Min.x = rect.Min.x;
        }
        else
        {
            res.intersects[1].Min.x = res.intersects[3].Min.x = rect.Min.x;
            res.visibleRect[0] = false;
        }

        if (rect.Min.y < startpos.y)
        {
            res.intersects[0].Min.y = res.intersects[1].Min.y = res.intersects[2].Min.y = rect.Min.y;
        }
        else
        {
            res.intersects[0].Min.y = res.intersects[2].Min.y = rect.Min.y;
            res.visibleRect[1] = false;
        }

        if (rect.Max.x > endpos.x)
        {
            res.intersects[1].Max.x = res.intersects[2].Max.x = res.intersects[3].Max.x = rect.Max.x;
        }
        else
        {
            res.intersects[1].Max.x = res.intersects[3].Max.x = rect.Max.x;
            res.visibleRect[2] = false;
        }

        if (rect.Max.y > endpos.y)
        {
            res.intersects[0].Max.y = res.intersects[2].Max.y = res.intersects[3].Max.y = rect.Max.y;
        }
        else
        {
            res.intersects[0].Max.y = res.intersects[2].Max.y = rect.Max.y;
            res.visibleRect[3] = false;
        }

        return res;
    }

    RectBreakup ComputeRectBreakups(ImRect rect, float amount)
    {
        RectBreakup res;

        res.rects[0].Min = ImVec2{ rect.Min.x - amount, rect.Min.y };
        res.rects[0].Max = ImVec2{ rect.Min.x, rect.Max.y };

        res.rects[1].Min = ImVec2{ rect.Min.x, rect.Min.y - amount };
        res.rects[1].Max = ImVec2{ rect.Max.x, rect.Min.y };

        res.rects[2].Min = ImVec2{ rect.Max.x, rect.Min.y };
        res.rects[2].Max = ImVec2{ rect.Max.x + amount, rect.Max.y };

        res.rects[3].Min = ImVec2{ rect.Min.x, rect.Max.y };
        res.rects[3].Max = ImVec2{ rect.Max.x, rect.Max.y + amount };

        ImVec2 delta{ amount, amount };
        res.corners[0].Min = rect.Min - delta;
        res.corners[0].Max = rect.Min;
        res.corners[1].Min = ImVec2{ rect.Max.x, rect.Min.y - amount };
        res.corners[1].Max = res.corners[1].Min + delta;
        res.corners[2].Min = rect.Max;
        res.corners[2].Max = rect.Max + delta;
        res.corners[3].Min = ImVec2{ rect.Min.x - amount, rect.Max.y };
        res.corners[3].Max = res.corners[3].Min + delta;

        return res;
    }

    void DrawBorderRect(ImVec2 startpos, ImVec2 endpos, const StyleDescriptor& style, uint32_t bgcolor, IRenderer& renderer)
    {
        if (style.border.isUniform && style.border.top.thickness > 0.f && IsColorVisible(style.border.top.color) &&
            style.border.top.color != bgcolor)
        {
            if (!style.border.isRounded())
                renderer.DrawRect(startpos, endpos, style.border.top.color, false, style.border.top.thickness);
            else
                renderer.DrawRoundedRect(startpos, endpos, style.border.top.color, false,
                    style.border.cornerRadius[TopLeftCorner], style.border.cornerRadius[TopRightCorner],
                    style.border.cornerRadius[BottomRightCorner], style.border.cornerRadius[BottomLeftCorner],
                    style.border.top.thickness);
        }
        else
        {
            auto width = endpos.x - startpos.x, height = endpos.y - startpos.y;

            if (style.border.top.thickness > 0.f && style.border.top.color != bgcolor && IsColorVisible(style.border.top.color))
                renderer.DrawLine(startpos, startpos + ImVec2{ width, 0.f }, style.border.top.color, style.border.top.thickness);
            if (style.border.right.thickness > 0.f && style.border.right.color != bgcolor && IsColorVisible(style.border.right.color))
                renderer.DrawLine(startpos + ImVec2{ width - style.border.right.thickness, 0.f }, endpos -
                    ImVec2{ style.border.right.thickness, 0.f }, style.border.right.color, style.border.right.thickness);
            if (style.border.left.thickness > 0.f && style.border.left.color != bgcolor && IsColorVisible(style.border.left.color))
                renderer.DrawLine(startpos, startpos + ImVec2{ 0.f, height }, style.border.left.color, style.border.left.thickness);
            if (style.border.bottom.thickness > 0.f && style.border.bottom.color != bgcolor && IsColorVisible(style.border.bottom.color))
                renderer.DrawLine(startpos + ImVec2{ 0.f, height - style.border.bottom.thickness }, endpos -
                    ImVec2{ 0.f, style.border.bottom.thickness }, style.border.bottom.color, style.border.bottom.thickness);
        }
    }

    void DrawBoxShadow(ImVec2 startpos, ImVec2 endpos, const StyleDescriptor& style, IRenderer& renderer)
    {
        // In order to draw box-shadow, the following steps are used:
        // 1. Draw the underlying rectangle with shadow color, spread and offset.
        // 2. Decompose the blur region into 8 rects i.e. 4 for corners, 4 for the sides
        // 3. For each rect, determine the vertex color i.e. shadow color or transparent,
        //    and draw a rect gradient accordingly.
        if ((style.shadow.blur > 0.f || style.shadow.spread > 0.f || style.shadow.offset.x != 0.f ||
            style.shadow.offset.y != 0) && IsColorVisible(style.shadow.color))
        {
            ImRect rect{ startpos, endpos };
            rect.Expand(style.shadow.spread);
            rect.Translate(style.shadow.offset);

            if (style.shadow.blur > 0.f)
            {
                auto outercol = style.shadow.color & ~IM_COL32_A_MASK;
                auto brk = ComputeRectBreakups(rect, style.shadow.blur);

                // Sides: Left -> Top -> Right -> Bottom
                renderer.DrawRectGradient(brk.rects[0].Min, brk.rects[0].Max, outercol, style.shadow.color, DIR_Horizontal);
                renderer.DrawRectGradient(brk.rects[1].Min, brk.rects[1].Max, outercol, style.shadow.color, DIR_Vertical);
                renderer.DrawRectGradient(brk.rects[2].Min, brk.rects[2].Max, style.shadow.color, outercol, DIR_Horizontal);
                renderer.DrawRectGradient(brk.rects[3].Min, brk.rects[3].Max, style.shadow.color, outercol, DIR_Vertical);

                // Corners: Top-left -> Top-right -> Bottom-right -> Bottom-left
                switch (GlobalWindowConfig.shadowQuality)
                {
                case BoxShadowQuality::Balanced:
                {
                    ImVec2 center = brk.corners[0].Max;
                    auto radius = brk.corners[0].GetHeight() - 0.5f; // all corners of same size
                    ImVec2 offset{ radius / 2.f, radius / 2.f };
                    radius *= 1.75f;

                    renderer.SetClipRect(brk.corners[0].Min, brk.corners[0].Max);
                    renderer.DrawRadialGradient(center + offset, radius, style.shadow.color, outercol, 180, 270);
                    renderer.ResetClipRect();

                    center = ImVec2{ brk.corners[1].Min.x, brk.corners[1].Max.y };
                    renderer.SetClipRect(brk.corners[1].Min, brk.corners[1].Max);
                    renderer.DrawRadialGradient(center + ImVec2{ -offset.x, offset.y }, radius, style.shadow.color, outercol, 270, 360);
                    renderer.ResetClipRect();

                    center = brk.corners[2].Min;
                    renderer.SetClipRect(brk.corners[2].Min, brk.corners[2].Max);
                    renderer.DrawRadialGradient(center - offset, radius, style.shadow.color, outercol, 0, 90);
                    renderer.ResetClipRect();

                    center = ImVec2{ brk.corners[3].Max.x, brk.corners[3].Min.y };
                    renderer.SetClipRect(brk.corners[3].Min, brk.corners[3].Max);
                    renderer.DrawRadialGradient(center + ImVec2{ offset.x, -offset.y }, radius, style.shadow.color, outercol, 90, 180);
                    renderer.ResetClipRect();
                    break;
                }
                default:
                    break;
                }
            }

            rect.Expand(style.shadow.blur > 0.f ? 1.f : 0.f);
            if (!style.border.isRounded())
                renderer.DrawRect(rect.Min, rect.Max, style.shadow.color, true);
            else
                renderer.DrawRoundedRect(rect.Min, rect.Max, style.shadow.color, true,
                    style.border.cornerRadius[TopLeftCorner], style.border.cornerRadius[TopRightCorner],
                    style.border.cornerRadius[BottomRightCorner], style.border.cornerRadius[BottomLeftCorner]);

            auto diffcolor = GlobalWindowConfig.bgcolor - 1;
            auto border = style.border;
            border.setColor(GlobalWindowConfig.bgcolor);
            DrawBorderRect(startpos, endpos, style, diffcolor, renderer);
            if (!border.isRounded())
                renderer.DrawRect(startpos, endpos, GlobalWindowConfig.bgcolor, true);
            else
                renderer.DrawRoundedRect(startpos, endpos, GlobalWindowConfig.bgcolor, true,
                    style.border.cornerRadius[TopLeftCorner], style.border.cornerRadius[TopRightCorner],
                    style.border.cornerRadius[BottomRightCorner], style.border.cornerRadius[BottomLeftCorner]);
        }
    }

    template <typename ItrT>
    void DrawLinearGradient(ImVec2 initpos, ImVec2 endpos, float angle, ImGuiDir dir,
        ItrT start, ItrT end, const FourSidedBorder& border, IRenderer& renderer)
    {
        if (!border.isRounded())
        {
            auto width = endpos.x - initpos.x;
            auto height = endpos.y - initpos.y;

            if (dir == ImGuiDir::ImGuiDir_Left)
            {
                for (auto it = start; it != end; ++it)
                {
                    auto extent = width * it->pos;
                    renderer.DrawRectGradient(initpos, initpos + ImVec2{ extent, height },
                        it->from, it->to, DIR_Horizontal);
                    initpos.x += extent;
                }
            }
            else if (dir == ImGuiDir::ImGuiDir_Down)
            {
                for (auto it = start; it != end; ++it)
                {
                    auto extent = height * it->pos;
                    renderer.DrawRectGradient(initpos, initpos + ImVec2{ width, extent },
                        it->from, it->to, DIR_Vertical);
                    initpos.y += extent;
                }
            }
        }
        else
        {

        }
    }

    void DrawBackground(ImVec2 startpos, ImVec2 endpos, const StyleDescriptor& style, IRenderer& renderer)
    {
        if (style.gradient.totalStops != 0)
            (style.gradient.dir == ImGuiDir_Down || style.gradient.dir == ImGuiDir_Left) ?
            DrawLinearGradient(startpos, endpos, style.gradient.angleDegrees, style.gradient.dir,
                std::begin(style.gradient.colorStops), std::begin(style.gradient.colorStops) + style.gradient.totalStops, style.border, renderer) :
            DrawLinearGradient(startpos, endpos, style.gradient.angleDegrees, style.gradient.dir,
                std::rbegin(style.gradient.colorStops), std::rbegin(style.gradient.colorStops) + style.gradient.totalStops, style.border, renderer);
        else if (IsColorVisible(style.bgcolor))
            if (!style.border.isRounded())
                renderer.DrawRect(startpos, endpos, style.bgcolor, true);
            else
                renderer.DrawRoundedRect(startpos, endpos, style.bgcolor, true,
                    style.border.cornerRadius[TopLeftCorner], style.border.cornerRadius[TopRightCorner],
                    style.border.cornerRadius[BottomRightCorner], style.border.cornerRadius[BottomLeftCorner]);
    }

    void DrawText(ImVec2 startpos, ImVec2 endpos, std::string_view text, bool disabled, const StyleDescriptor& style, IRenderer& renderer)
    {
        ImRect content{ startpos, endpos };
        
        renderer.SetClipRect(content.Min, content.Max);
        renderer.SetCurrentFont(style.font.font, style.font.size);

        if ((style.font.flags & FontStyleOverflowMarquee) != 0 &&
            !disabled && style.index.animation != InvalidIdx)
        {
            auto& animation = GlobalStates.animations[style.index.animation];
            ImVec2 textsz = renderer.GetTextSize(text, style.font.font, style.font.size);

            if (textsz.x > content.GetWidth())
            {
                animation.moveByPixel(1.f, content.GetWidth(), -textsz.x);
                content.Min.x += animation.offset;
                renderer.DrawText(text, content.Min, style.fgcolor, style.dimension.x > 0 ? style.dimension.x : -1.f);
            }
            else
                renderer.DrawText(text, content.Min, style.fgcolor, style.dimension.x > 0 ? style.dimension.x : -1.f);
        }
        else if (style.font.flags & FontStyleOverflowEllipsis)
        {
            float width = 0.f, available = content.GetWidth() - renderer.EllipsisWidth(style.font.font, style.font.size);
            auto renderedText = false;

            for (auto chidx = 0; chidx < (int)text.size(); ++chidx)
            {
                // TODO: This is only valid for ASCII, this should be fixed for UTF-8
                auto w = renderer.GetTextSize(text.substr(chidx, 1), style.font.font, style.font.size).x;
                if (width + w > available)
                {
                    renderer.DrawText(text.substr(0, chidx), content.Min, style.fgcolor, -1.f);
                    renderer.DrawText("...", content.Min + ImVec2{ width, 0.f }, style.fgcolor, -1.f);
                    renderedText = true;
                    break;
                }
                width += w;
            }

            if (!renderedText)
                renderer.DrawText(text, content.Min, style.fgcolor, style.dimension.x > 0 ? style.dimension.x : -1.f);
        }
        else
            renderer.DrawText(text, content.Min, style.fgcolor, style.dimension.x > 0 ? style.dimension.x : -1.f);

        renderer.ResetFont();
        renderer.ResetClipRect();
    }

#pragma endregion

    // =============================================================================================
    // WIDGET FUNCTIONS
    // =============================================================================================

#pragma region Widgets

    /* Here is the box model that is followed here:

            +--------------------------------+
            |            margin              |
            |   +------------------------+   |
            |   |       border           |   |
            |   |   +--------------+     |   |
            |   |   |   padding    |     |   |
            |   |   |  +--------+  |     |   |
            |   |   |  |        |  |     |   |
            |   |   |  |content |  |     |   |
            |   |   |  |        |  |     |   |
            |   |   |  +--------+  |     |   |
            |   |   |              |     |   |
            |   |   +--------------+     |   |
            |   |                        |   |
            |   +------------------------+   |
            |                                |
            +--------------------------------+

    */
    static std::tuple<ImRect, ImRect, ImRect, ImRect> GetBoxModelBounds(ImVec2 pos, const StyleDescriptor& style, 
        std::string_view text, IRenderer& renderer, float width, float height)
    {
        ImRect content, padding, border, margin;
        const auto& borderstyle = style.border;
        const auto& font = style.font;

        margin.Min = pos;
        border.Min = pos + ImVec2{ style.margin.left, style.margin.top };
        padding.Min = border.Min + ImVec2{ borderstyle.left.thickness, borderstyle.top.thickness };
        content.Min = padding.Min + ImVec2{ style.padding.left, style.padding.top };
        auto textsz = renderer.GetTextSize(text, font.font, font.size,
            margin.Max.x == 0.f ? -1.f : margin.Max.x);

        if (width == 0.f)
        {
            content.Max.x = style.dimension.x != 0.f ? style.dimension.x : 
                std::clamp(content.Min.x + textsz.x, style.mindim.x, style.maxdim.x);
            padding.Max.x = content.Max.x + style.padding.right;
            border.Max.x = padding.Max.x + borderstyle.right.thickness;
            margin.Max.x = border.Max.x + style.margin.right;
        }
        else
        {
            margin.Max.x = width == FLT_MAX ? std::min(ImGui::GetCurrentWindow()->Size.x, margin.Min.x + style.maxdim.x) : 
                margin.Min.x + width;
            border.Max.x = margin.Max.x - style.margin.right;
            padding.Max.x = border.Max.x - borderstyle.right.thickness;
            content.Max.x = padding.Max.x - style.padding.right;
        }

        if (height == 0.f)
        {
            content.Max.y = style.dimension.y != 0.f ? style.dimension.y :
                std::clamp(content.Min.y + textsz.y, style.mindim.y, style.maxdim.y);
            padding.Max.y = content.Max.y + style.padding.bottom;
            border.Max.y = padding.Max.y + borderstyle.bottom.thickness;
            margin.Max.y = border.Max.y + style.margin.bottom;
        }
        else
        {
            margin.Max.y = height == FLT_MAX ? std::min(ImGui::GetCurrentWindow()->Size.y, margin.Min.y + style.maxdim.y) : 
                margin.Min.y + height;
            border.Max.y = margin.Max.y - style.margin.bottom;
            padding.Max.y = border.Max.y - borderstyle.left.thickness;
            content.Max.y = padding.Max.y - style.padding.bottom;
        }

        return std::make_tuple(content, padding, border, margin);
    }

    void ShowTooltip(long long& hoverDuration, const ImRect& margin, ImVec2 pos, std::string_view tooltip, IRenderer& renderer)
    {
        auto currts = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock().now().time_since_epoch()).count();
        if (hoverDuration == 0) hoverDuration = currts;
        else if ((int32_t)(currts - hoverDuration) >= GlobalWindowConfig.tooltipDelay)
        {
            auto font = GetFont(GlobalWindowConfig.tooltipFontFamily, GlobalWindowConfig.tooltipFontSz, FT_Normal);
            auto textsz = renderer.GetTextSize(tooltip, font, GlobalWindowConfig.tooltipFontSz);
            auto offsetx = std::max(0.f, margin.GetWidth() - textsz.x) * 0.5f;
            auto tooltippos = pos + ImVec2{ offsetx, -(textsz.y + 2.f) };
            renderer.DrawTooltip(tooltippos, tooltip);
        }
    }
    
    static void CopyStyle(const StyleDescriptor& src, StyleDescriptor& dest)
    {
        if (dest.specified & StyleUpdatedFromBase) return;

        for (int64_t idx = 0; idx <= StyleTotal; ++idx)
        {
            auto prop = (StyleProperty)((1ll << idx));
            if ((dest.specified & prop) == 0)
            {
                switch (prop)
                {
                case glimmer::StyleBackground:
                    dest.bgcolor = src.bgcolor;
                    dest.gradient = src.gradient;
                    break;
                case glimmer::StyleFgColor:
                    dest.fgcolor = src.fgcolor;
                    break;
                case glimmer::StyleFontSize:
                case glimmer::StyleFontFamily:
                case glimmer::StyleFontWeight:
                case glimmer::StyleFontStyle:
                    dest.font = src.font;
                    break;
                case glimmer::StyleHeight:
                    dest.dimension.y = src.dimension.y;
                    break;
                case glimmer::StyleWidth:
                    dest.dimension.x = src.dimension.x;
                    break;
                case glimmer::StyleHAlignment:
                    break;
                case glimmer::StyleVAlignment:
                    break;
                case glimmer::StylePadding:
                    dest.padding = src.padding;
                    break;
                case glimmer::StyleMargin:
                    dest.margin = src.margin;
                    break;
                case glimmer::StyleBorder:
                    dest.border = src.border;
                    break;
                case glimmer::StyleOverflow:
                    break;
                case glimmer::StyleBorderRadius:
                {
                    const auto& srcborder = src.border;
                    auto& dstborder = dest.border;
                    dstborder.cornerRadius[0] = srcborder.cornerRadius[0];
                    dstborder.cornerRadius[1] = srcborder.cornerRadius[1];
                    dstborder.cornerRadius[2] = srcborder.cornerRadius[2];
                    dstborder.cornerRadius[3] = srcborder.cornerRadius[3];
                    break;
                }
                case glimmer::StyleCellSpacing:
                    break;
                case glimmer::StyleTextWrap:
                    break;
                case glimmer::StyleBoxShadow:
                    dest.shadow = src.shadow;
                    break;
                case glimmer::StyleTextOverflow:
                    break;
                case glimmer::StyleMinWidth:
                    dest.mindim.x = src.mindim.x;
                    break;
                case glimmer::StyleMaxWidth:
                    dest.maxdim.x = src.maxdim.x;
                    break;
                case glimmer::StyleMinHeight:
                    dest.mindim.y = src.mindim.y;
                    break;
                case glimmer::StyleMaxHeight:
                    dest.maxdim.y = src.maxdim.y;
                    break;
                default:
                    break;
                }
            }
        }

        dest.specified |= StyleUpdatedFromBase;
    }

    /*WidgetDrawResult ButtonImpl(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding, const ImRect& content, IRenderer& renderer)
    {
        WidgetDrawResult result;
        auto& state = GlobalStates.GetState(id).button;
        auto& style = GlobalStates.GetStyle(id, state.state);
        auto ismouseover = padding.Contains(ImGui::GetIO().MousePos);
        state.state = !ismouseover ? WS_Default :
            ImGui::IsMouseDown(ImGuiMouseButton_Left) ? WS_Pressed | WS_Hovered : WS_Hovered;

        DrawBoxShadow(border.Min, border.Max, style, renderer);
        DrawBackground(border.Min, border.Max, style, renderer);
        DrawBorderRect(border.Min, border.Max, style, style.bgcolor, renderer);
        DrawText(content.Min, content.Max, state.text, state.state & WS_Disabled, style, renderer);

        if (ismouseover && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            result.event = WidgetEvent::Clicked;
        else if (ismouseover && !state.tooltip.empty() && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
            ShowTooltip(state._hoverDuration, margin, margin.Min, state.tooltip, renderer);
        else state._hoverDuration == 0;

        result.geometry = margin;
        return result;
    }

    void UpdateLayout(LayoutDescriptor& layout, ImVec2 sz)
    {
        switch (layout.type)
        {
        case glimmer::LayoutType::HBox:
            layout.currpos.x += sz.x;
            break;
        case glimmer::LayoutType::VBox:
            break;
        case glimmer::LayoutType::Grid:
            break;
        case glimmer::LayoutType::Flex:
            if (layout.isRowWise)
            {
                auto layoutsz = layout.geometry.GetSize();
                if (layout.wrap && (sz.x + layout.currpos.x > layoutsz.x))
                {
                    layout.currpos.y += layout.maxdim.y;
                    layout.currpos.x = 0.f;
                }
                else
                {
                    layout.maxdim.y = std::max(layout.maxdim.y, sz.y);
                    layout.currpos.x += sz.x;
                }
            }
            break;
        default:
            break;
        }
    }

    WidgetDrawResult Button(int32_t id, IRenderer& renderer, std::optional<ImVec2> geometry)
    {
        assert(id <= (int)GlobalStates.states[WT_Button].size());

        if (GlobalWindowConfig.layoutPolicy == LayoutPolicy::ImmediateMode)
        {
            auto& layout = GlobalStates.layouts[GlobalStates.currLayoutDepth].back();
            auto& state = GlobalStates.GetState(id).button;
            CopyStyle(GlobalStates.GetStyle(id, WS_Default), GlobalStates.GetStyle(id, state.state));

            // TODO: Adjust everything...
            auto& style = GlobalStates.GetStyle(id, state.state);
            auto [content, padding, border, margin] = GetBoxModelBounds(layout.currpos, style, state.text, renderer, 
                geometry ? geometry.value() : ImVec2{ -1.f, -1.f });
            auto res = ButtonImpl(id, margin, border, padding, content, renderer);
            UpdateLayout(layout, res.geometry.GetSize());
            return res;
        }
        else
        {
            WidgetDescriptor wdesc;
            wdesc.wtype = WT_Button;
            wdesc.id = id;
            wdesc.constraint = geometry ? geometry.value() : ImVec2{ -1.f, -1.f };
            SubmitWidget(std::move(wdesc));

            WidgetEvent result = WidgetEvent::None;
            ImRect margin;

            return { result, margin };
        }
    }

    void SubmitWidget(WidgetDescriptor&& desc)
    {
        auto& layout = GlobalStates.layouts[GlobalStates.currLayoutDepth];
        layout.back().widgets.emplace_back(desc);
    }*/

    WindowConfig& GetWindowConfig()
    {
        return GlobalWindowConfig;
    }

    void BeginFrame()
    {
        GlobalStates.InsideFrame = true;
    }

    void EndFrame()
    {
        GlobalStates.InsideFrame = false;
    }

    int32_t GetNextId(WidgetType type)
    {
        return GlobalStates.maxids[type];
    }

    static void UsePushedStyle(int32_t wid)
    {
        // If a set of styles is already pushed, add it to widget being created
        if (GlobalStates.currstyleDepth >= 6)
        {
            GlobalStates.GetStyle(wid, WS_Default) = GlobalStates.currstyle[GlobalStates.currstyleDepth - 6];
            GlobalStates.GetStyle(wid, WS_Disabled) = GlobalStates.currstyle[GlobalStates.currstyleDepth - 5];
            GlobalStates.GetStyle(wid, WS_Hovered) = GlobalStates.currstyle[GlobalStates.currstyleDepth - 4];
            GlobalStates.GetStyle(wid, WS_Pressed) = GlobalStates.currstyle[GlobalStates.currstyleDepth - 3];
            GlobalStates.GetStyle(wid, WS_Focused) = GlobalStates.currstyle[GlobalStates.currstyleDepth - 2];
            GlobalStates.GetStyle(wid, WS_Checked) = GlobalStates.currstyle[GlobalStates.currstyleDepth - 1];
        }
    }

    WidgetDrawResult Label(int32_t id, ImVec2 pos, float width, float height)
    {
        assert((id & 0xffff) <= (int)GlobalStates.states[WT_Label].size());

        UsePushedStyle(id);
        auto& state = GlobalStates.GetState(id).label;
        auto& renderer = *GlobalWindowConfig.renderer;

        // TODO: Adjust everything...
        auto& style = GlobalStates.GetStyle(id, state.state);
        auto [content, padding, border, margin] = GetBoxModelBounds(pos, style, state.text, renderer, width, height);
        WidgetDrawResult result;
        auto ismouseover = padding.Contains(ImGui::GetIO().MousePos);

        DrawBoxShadow(border.Min, border.Max, style, renderer);
        DrawBackground(border.Min, border.Max, style, renderer);
        DrawBorderRect(border.Min, border.Max, style, style.bgcolor, renderer);
        DrawText(content.Min, content.Max, state.text, state.state & WS_Disabled, style, renderer);

        if (ismouseover && !state.tooltip.empty() && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
            ShowTooltip(state._hoverDuration, margin, margin.Min, state.tooltip, renderer);
        else state._hoverDuration == 0;

        result.geometry = margin;
        return result;
    }

    WidgetDrawResult Button(int32_t id, ImVec2 pos, float width, float height)
    {
        assert((id & 0xffff) <= (int)GlobalStates.states[WT_Button].size());

        UsePushedStyle(id);
        auto& state = GlobalStates.GetState(id).button;
        auto& renderer = *GlobalWindowConfig.renderer;
        CopyStyle(GlobalStates.GetStyle(id, WS_Default), GlobalStates.GetStyle(id, state.state));

        // TODO: Adjust everything...
        auto& style = GlobalStates.GetStyle(id, state.state);
        auto [content, padding, border, margin] = GetBoxModelBounds(pos, style, state.text, renderer, width, height);
        WidgetDrawResult result;
        auto ismouseover = padding.Contains(ImGui::GetIO().MousePos);
        state.state = !ismouseover ? WS_Default :
            ImGui::IsMouseDown(ImGuiMouseButton_Left) ? WS_Pressed | WS_Hovered : WS_Hovered;

        DrawBoxShadow(border.Min, border.Max, style, renderer);
        DrawBackground(border.Min, border.Max, style, renderer);
        DrawBorderRect(border.Min, border.Max, style, style.bgcolor, renderer);
        DrawText(content.Min, content.Max, state.text, state.state & WS_Disabled, style, renderer);

        if (ismouseover && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            result.event = WidgetEvent::Clicked;
        else if (ismouseover && !state.tooltip.empty() && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
            ShowTooltip(state._hoverDuration, margin, margin.Min, state.tooltip, renderer);
        else state._hoverDuration == 0;

        result.geometry = margin;
        return result;
    }

    WidgetDrawResult ToggleButton(int32_t id, ImVec2 pos, IRenderer& renderer, std::optional<ImVec2> geometry)
    {
        assert(id <= (int)GlobalStates.states[WT_ToggleButton].size());

        auto& state = GlobalStates.GetState(id).toggle;
        CopyStyle(GlobalStates.GetStyle(id, WS_Default), GlobalStates.GetStyle(id, state.state));

        WidgetEvent result = WidgetEvent::None;
        auto& style = GlobalStates.GetStyle(id, state.state);

        ImRect extent;
        if (geometry.has_value()) { extent.Min = pos; extent.Max = extent.Min + geometry.value(); }
        else
        {
            extent.Min = pos + ImVec2{ style.margin.top + style.padding.top, style.margin.left + style.padding.right };
            extent.Max = extent.Min + style.dimension;
        }
        
        auto radius = ((extent.GetHeight()) * 0.5f) + GlobalStates.toggleButtonStyles[style.index.custom].thumbOffset;
        auto center = extent.Min + ImVec2{ radius + 1.f, radius };
        auto mousepos = ImGui::GetIO().MousePos;
        auto dist = std::sqrtf((mousepos.x - center.x) * (mousepos.x - center.x) + (mousepos.y - center.y) * (mousepos.y - center.y));
        auto mouseover = extent.Contains(mousepos);
        state.state = mouseover && ImGui::IsMouseDown(ImGuiMouseButton_Left) ? WS_Hovered | WS_Pressed :
            mouseover ? WS_Hovered : WS_Default;

        auto& specificStyle = GlobalStates.toggleButtonStyles[style.index.custom + log2((unsigned)state.state)];
        renderer.DrawRoundedRect(extent.Min, extent.Max, specificStyle.trackColor, true, radius, radius, radius, radius);
        renderer.DrawRoundedRect(extent.Min, extent.Max, specificStyle.trackBorderColor, false, radius, radius, radius, radius, 
            specificStyle.trackBorderThickness);

        if (mouseover && specificStyle.thumbExpand > 0.f)
            renderer.DrawCircle(center, radius + specificStyle.thumbExpand, style.fgcolor, true);

        renderer.DrawCircle(center, radius, style.fgcolor, true);

        if (mouseover && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            result = WidgetEvent::Clicked;
            state.checked = !state.checked;
        }

        if (!geometry.has_value()) extent.Max += ImVec2{ style.margin.right + style.padding.right, style.margin.bottom + style.padding.right };
        return { result, extent };
    }

    WidgetDrawResult RadioButton(int32_t id, ImVec2 pos, IRenderer& renderer, std::optional<ImVec2> geometry)
    {
        assert(id <= (int)GlobalStates.states[WT_RadioButton].size());

        auto& state = GlobalStates.GetState(id).radio;
        CopyStyle(GlobalStates.GetStyle(id, WS_Default), GlobalStates.GetStyle(id, state.state));

        WidgetEvent result = WidgetEvent::None;
        auto& style = GlobalStates.GetStyle(id, state.state);

        ImRect extent;
        if (geometry.has_value()) { extent.Min = pos; extent.Max = extent.Min + geometry.value(); }
        else
        {
            extent.Min = pos + ImVec2{ style.margin.top + style.padding.top, style.margin.left + style.padding.right };
            extent.Max = extent.Min + style.dimension;
        }

        auto mousepos = ImGui::GetIO().MousePos;
        auto mouseover = extent.Contains(mousepos);
        state.state = mouseover && ImGui::IsMouseDown(ImGuiMouseButton_Left) ? WS_Hovered | WS_Pressed :
            mouseover ? WS_Hovered : WS_Default;

        auto radius = extent.GetHeight() * 0.5f;
        auto center = ImVec2{ radius * 0.5f, radius * 0.5f };
        auto color = GlobalStates.GetStyle(id, state.state).fgcolor;
        renderer.DrawCircle(center, radius, color, false, 1.f);
        radius -= 1.f;
        renderer.DrawCircle(center, radius, color, true);

        if (mouseover && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            result = WidgetEvent::Clicked;
            state.checked = !state.checked;
        }

        if (!geometry.has_value()) extent.Max += ImVec2{ style.margin.right + style.padding.right, style.margin.bottom + style.padding.right };
        return { result, extent };
    }

    void PushStyle(std::string_view defcss, std::string_view hovercss, std::string_view pressedcss, std::string_view focusedcss, 
        std::string_view checkedcss, std::string_view disblcss)
    {
        GlobalStates.currstyle[GlobalStates.currstyleDepth].From(defcss);
        GlobalStates.currstyle[GlobalStates.currstyleDepth + 1].From(disblcss);
        GlobalStates.currstyle[GlobalStates.currstyleDepth + 2].From(hovercss);
        GlobalStates.currstyle[GlobalStates.currstyleDepth + 3].From(pressedcss);
        GlobalStates.currstyle[GlobalStates.currstyleDepth + 4].From(focusedcss);
        GlobalStates.currstyle[GlobalStates.currstyleDepth + 5].From(checkedcss);
        CopyStyle(GlobalStates.currstyle[GlobalStates.currstyleDepth], GlobalStates.currstyle[GlobalStates.currstyleDepth + 1]);
        CopyStyle(GlobalStates.currstyle[GlobalStates.currstyleDepth], GlobalStates.currstyle[GlobalStates.currstyleDepth + 2]);
        CopyStyle(GlobalStates.currstyle[GlobalStates.currstyleDepth], GlobalStates.currstyle[GlobalStates.currstyleDepth + 3]);
        CopyStyle(GlobalStates.currstyle[GlobalStates.currstyleDepth], GlobalStates.currstyle[GlobalStates.currstyleDepth + 4]);
        CopyStyle(GlobalStates.currstyle[GlobalStates.currstyleDepth], GlobalStates.currstyle[GlobalStates.currstyleDepth + 5]);
        GlobalStates.currstyleDepth += 6;
    }

    StyleDescriptor& GetStyle(int32_t id, int32_t state)
    {
        auto styleidx = log2((unsigned)state);
        auto& src = GlobalStates.GetStyle(id, state);

        for (auto stidx = styleidx + 1; stidx < 5; ++stidx) 
            CopyStyle(src, GlobalStates.GetStyle(id, 1 << stidx));

        return src;
    }

    void PopStyle(int depth)
    {
        while (GlobalStates.currstyleDepth >= 6 && depth > 0)
        {
            GlobalStates.currstyle[GlobalStates.currstyleDepth - 1] = StyleDescriptor{};
            GlobalStates.currstyle[GlobalStates.currstyleDepth - 2] = StyleDescriptor{};
            GlobalStates.currstyle[GlobalStates.currstyleDepth - 3] = StyleDescriptor{};
            GlobalStates.currstyle[GlobalStates.currstyleDepth - 4] = StyleDescriptor{};
            GlobalStates.currstyle[GlobalStates.currstyleDepth - 5] = StyleDescriptor{};
            GlobalStates.currstyle[GlobalStates.currstyleDepth - 6] = StyleDescriptor{};
            GlobalStates.currstyleDepth -= 6;
            depth--;
        }
    }

    WidgetStateData& CreateWidget(WidgetType type, int16_t id)
    {
        // Create widgets before beginning a frame
        assert(!GlobalStates.InsideFrame);

        int32_t wid = id;
        wid = wid | (type << 16);
        GlobalStates.maxids[type]++;
        if (GlobalStates.InsideFrame) GlobalStates.tempids[type] = std::min(GlobalStates.tempids[type], GlobalStates.maxids[type]);
        
        // If a set of styles is already pushed, add it to widget being created
        if (GlobalStates.currstyleDepth >= 6)
        {
            GlobalStates.GetStyle(wid, WS_Default) = GlobalStates.currstyle[GlobalStates.currstyleDepth - 6];
            GlobalStates.GetStyle(wid, WS_Disabled) = GlobalStates.currstyle[GlobalStates.currstyleDepth - 5];
            GlobalStates.GetStyle(wid, WS_Hovered) = GlobalStates.currstyle[GlobalStates.currstyleDepth - 4];
            GlobalStates.GetStyle(wid, WS_Pressed) = GlobalStates.currstyle[GlobalStates.currstyleDepth - 3];
            GlobalStates.GetStyle(wid, WS_Focused) = GlobalStates.currstyle[GlobalStates.currstyleDepth - 2];
            GlobalStates.GetStyle(wid, WS_Checked) = GlobalStates.currstyle[GlobalStates.currstyleDepth - 1];
        }

        return GlobalStates.GetState(wid);
    }

    WidgetStateData& CreateWidget(int32_t id)
    {
        auto wtype = (WidgetType)(id >> 16);
        GlobalStates.maxids[wtype]++;
        return CreateWidget(wtype, (int16_t)(id & 0xffff));
    }

    /*void StartLayout(LayoutType type, ImVec2 size)
    {
        assert((GlobalWindowConfig.layoutPolicy == LayoutPolicy::DeferredMode ||
            size != ImVec2{ -1.f, -1.f }));

        GlobalStates.currLayoutDepth++;
        auto& layout = GlobalStates.layouts[GlobalStates.currLayoutDepth].emplace_back();
        layout.type = type;
        layout.currpos = GlobalStates.currLayoutDepth > 0 ? 
            GlobalStates.layouts[GlobalStates.currLayoutDepth - 1].back().currpos : ImVec2{0.f, 0.f};

        if (size != ImVec2{ -1.f, -1.f })
        {
            layout.geometry.Min = layout.currpos;
            layout.geometry.Max = layout.geometry.Min + size;
        }
        
        // If all leaves are at same level, this is not required
        if (GlobalStates.currLayoutDepth > 0)
        {
            layout.parent = (int32_t)GlobalStates.layouts[GlobalStates.currLayoutDepth - 1].size() - 1;

            WidgetDescriptor desc;
            desc.wtype = WT_SubLayout;
            desc.id = GlobalStates.currLayoutDepth;
            desc.sibling = (int32_t)GlobalStates.layouts[GlobalStates.currLayoutDepth].size() - 1;
            GlobalStates.layouts[GlobalStates.currLayoutDepth - 1].back().widgets.push_back(desc);
        }
    }

    void ComputeMinSizes(LayoutDescriptor& layout)
    {
        for (auto& widget : layout.widgets)
        {
            if (widget.wtype != WT_SubLayout)
            {
                if (!widget.isComputed)
                {
                    switch (widget.wtype)
                    {
                    case WT_Label:
                    case WT_Button:
                    {
                        auto& state = GlobalStates.GetState(widget.id).button;
                        CopyStyle(GlobalStates.GetStyle(widget.id, WS_Default), GlobalStates.GetStyle(widget.id, state.state));

                        auto& style = GlobalStates.GetStyle(widget.id, state.state);
                        std::tie(widget.content, widget.padding, widget.border, widget.margin) = GetBoxModelBounds(ImVec2{ 0.f, 0.f },
                            style, state.text, * GlobalWindowConfig.renderer, layout.constraint);
                        widget.isComputed = true;
                        break;
                    }
                    default:
                        break;
                    }
                }

                if (layout.isRowWise) { layout.computed.x += widget.margin.GetSize().x; layout.computed.y = std::max(layout.computed.y, widget.margin.GetSize().y); }
                else { layout.computed.y += widget.margin.GetSize().y; layout.computed.x = std::max(layout.computed.x, widget.margin.GetSize().x); }
            }
            else
            {
                auto& sublayout = GlobalStates.layouts[widget.id][widget.sibling];
                ComputeMinSizes(sublayout); // Can be avoided by storing parent layout ids in widgets
                if (layout.isRowWise) { layout.computed.x += sublayout.computed.x; layout.computed.y = std::max(layout.computed.y, sublayout.computed.y); }
                else { layout.computed.y += sublayout.computed.y; layout.computed.x = std::max(layout.computed.x, sublayout.computed.x); }
            }
        }
    }

    void SetElementPosition(const LayoutDescriptor& layout, ImVec2 available, float unitwidth, float unitex,
        WidgetDescriptor& widget, float& currposx, float& height, float& maxheight)
    {
        auto sz = widget.wtype == WT_SubLayout ? GlobalStates.layouts[widget.id][widget.sibling].computed :
            widget.margin.GetSize();
        auto& state = GlobalStates.GetState(widget.id).button;
        auto& style = GlobalStates.GetStyle(widget.id, state.state);

        auto w = sz.x;
        if (widget.flex != FLT_MAX && widget.flex > 0.f) w = std::min(unitwidth * widget.flex, style.maxdim.x);
        else if (widget.flex == FLT_MAX) w = std::min(w + unitex, style.maxdim.x);

        if ((currposx + w - layout.geometry.Min.x) > available.x)
        {
            height += maxheight;
            currposx = layout.geometry.Min.x;
            maxheight = 0.f;
        }
        else
            maxheight = std::max(maxheight, sz.y);

        if (widget.wtype == WT_SubLayout)
        {
            GlobalStates.layouts[widget.id][widget.sibling].geometry = { ImVec2{ currposx, height }, ImVec2{ currposx + w, height + sz.y } };
        }
        else
        {
            widget.margin.Min += ImVec2{ currposx, height };
            widget.border.Min += ImVec2{ currposx, height };
            widget.padding.Min += ImVec2{ currposx, height };
            widget.content.Min += ImVec2{ currposx, height };
            currposx += w;
            widget.margin.Max += ImVec2{ currposx, height };
            widget.border.Max += ImVec2{ currposx, height };
            widget.padding.Max += ImVec2{ currposx, height };
            widget.content.Max += ImVec2{ currposx, height };
        }  
    }

    void DoTopDownLayout(LayoutDescriptor& layout, int sibling, int depth)
    {
        auto available = layout.geometry.GetSize();

        if (layout.isRowWise)
        {
            auto sum = 0.f, expanding = 0.f, occupied = 0.f, height = 0.f, fxwidth = 0.f;
            auto widx = 0;

            for (const auto& widget : layout.widgets)
            {
                sum += widget.flex > 0.f ? widget.flex : 0.f;
                if (widget.flex == FLT_MAX) expanding += 1.f;
                auto minw = widget.wtype == WT_SubLayout ? GlobalStates.layouts[widget.id][widget.sibling].computed.x :
                    widget.margin.GetSize().x;
                if (widget.flex < 0.f) fxwidth += minw;
                occupied += minw;
            }

            // Available space through which we can expand
            auto expandable = std::max(available.x - occupied, 0.f);
            auto unitwidth = (available.x - fxwidth) / sum;
            auto currposx = layout.geometry.Min.x;

            if (occupied > available.x && !layout.wrap)
            {
                //layout.geometry.Max += ImVec2{ occupied - available.x, 0.f };
                layout.overflowed = true;

                for (auto& widget : layout.widgets)
                {
                    auto minw = widget.wtype == WT_SubLayout ? GlobalStates.layouts[widget.id][widget.sibling].computed.x :
                        widget.margin.GetSize().x;
                    widget.margin.Min.x += currposx;
                    widget.border.Min.x += currposx;
                    widget.padding.Min.x += currposx;
                    widget.content.Min.x += currposx;
                    currposx += minw;
                    widget.margin.Max.x += currposx;
                    widget.border.Max.x += currposx;
                    widget.padding.Max.x += currposx;
                    widget.content.Max.x += currposx;
                }
            }
            else
            {
                auto height = layout.geometry.Min.y, maxheight = 0.f;
                auto unitex = expandable / expanding;

                for (auto& widget : layout.widgets)
                {
                    if (widget.flex == 0.f) continue;
                    else SetElementPosition(layout, available, unitwidth, unitex, widget, currposx, height, maxheight);
                }
            }
        }
    }

    void EndLayout()
    {
        // process the geometry of tthe layottu hierarchy?
        // there are no widgets here...
        GlobalStates.currLayoutDepth--;

        // Top-level
        if (GlobalStates.currLayoutDepth == -1)
        {
            assert(GlobalStates.layouts[0].size() == 1u);

            ImRect maxInLine[64][4];

            auto available = ImGui::GetCurrentWindow()->Size;
            GlobalStates.layouts[0][0].geometry.Max = available;
            GlobalStates.layouts[0][0].geometry.Min = ImVec2{ 0.f, 0.f };

            // Precompute min constraints of widgets and layouts
            for (auto& layout : GlobalStates.layouts[1])
                ComputeMinSizes(layout);

            // From top-level layout which spans entire window, start re-positioning
            for (auto depth = 1; depth < 16; ++depth)
            {
                auto lidx = 0;
                for (auto& layout : GlobalStates.layouts[depth])
                {
                    DoTopDownLayout(layout, lidx, depth);

                    for (const auto& widget : layout.widgets)
                    {
                        switch (widget.wtype)
                        {
                        case WT_Button:
                        {
                            ButtonImpl(widget.id, widget.margin, widget.border, widget.padding, widget.content, *GlobalWindowConfig.renderer);
                            break;
                        }
                        default:
                            break;
                        }
                    }

                    ++lidx;
                }
            }
        }
    }*/

    StyleDescriptor::StyleDescriptor()
    {
        font.size = GlobalWindowConfig.defaultFontSz;
        index.animation = index.custom = 0;
    }

    StyleDescriptor& StyleDescriptor::Border(float thick, std::tuple<int, int, int, int> color)
    {
        border.setThickness(thick); border.setColor(ToRGBA(color));
        return *this;
    }

    StyleDescriptor& StyleDescriptor::Raised(float amount)
    {
        return *this;
    }

    StyleDescriptor& StyleDescriptor::From(std::string_view css, bool checkForDuplicate)
    {
        if (css.empty()) return *this;

        auto sidx = 0;
        int prop = 0;
        CommonWidgetStyleDescriptor desc{};

        while (sidx < (int)css.size())
        {
            sidx = SkipSpace(css, sidx);
            auto stbegin = sidx;
            while ((sidx < (int)css.size()) && (css[sidx] != ':') &&
                !std::isspace(css[sidx])) sidx++;
            auto stylePropName = css.substr(stbegin, sidx - stbegin);

            sidx = SkipSpace(css, sidx);
            if (css[sidx] == ':') sidx++;
            sidx = SkipSpace(css, sidx);

            auto stylePropVal = GetQuotedString(css.data(), sidx, (int)css.size());
            if (!stylePropVal.has_value() || stylePropVal.value().empty())
            {
                stbegin = sidx;
                while ((sidx < (int)css.size()) && css[sidx] != ';') sidx++;
                stylePropVal = css.substr(stbegin, sidx - stbegin);

                if ((sidx < (int)css.size()) && css[sidx] == ';') sidx++;
            }

            if (stylePropVal.has_value())
                prop |= PopulateSegmentStyle(*this, desc, stylePropName, stylePropVal.value());
        }

        if (prop != 0)
        {
            if (prop & StyleThumbColor || prop & StyleTrackColor || prop & StyleTrackOutlineColor || prop & StyleThumbOffset)
            {
                auto found = false;
                
                if (checkForDuplicate)
                {
                    for (auto idx = 0; idx < GlobalStates.toggleButtonStyles.size(); ++idx)
                        if (std::memcmp(&(GlobalStates.toggleButtonStyles[idx]), &(desc.toggle), sizeof(ToggleButtonStyleDescriptor)) == 0)
                        {
                            found = true;
                            index.custom = idx;
                        }
                }

                if (!found)
                {
                    index.custom = (uint16_t)GlobalStates.toggleButtonStyles.size();
                    GlobalStates.toggleButtonStyles.push_back(desc.toggle);
                }
            }
        }

        specified = prop;
        return *this;
    }

    WidgetStateData::WidgetStateData(WidgetType type)
    {
        switch (type)
        {
        case glimmer::WT_Label: label = LabelState{}; break;
        case glimmer::WT_Button: button = ButtonState{}; break;
        case glimmer::WT_RadioButton:
            break;
        case glimmer::WT_ToggleButton:
            break;
        default:
            break;
        }
    }

#pragma endregion
    
}

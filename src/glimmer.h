#pragma once

#include <string_view>
#include <optional>
#include <vector>

#define IM_RICHTEXT_DEFAULT_FONTFAMILY "default-font-family"
#define IM_RICHTEXT_MONOSPACE_FONTFAMILY "monospace-family"
#define IM_RICHTEXT_MAX_COLORSTOPS 4

#include "imgui.h"
#include "imgui_internal.h"

namespace glimmer
{
    // =============================================================================================
    // FONT MANAGEMENT
    // =============================================================================================
    enum FontType
    {
        FT_Normal, FT_Light, FT_Bold, FT_Italics, FT_BoldItalics, FT_Total
    };

    // Determines which UTF-8 characters are present in provided rich text
    // NOTE: Irrespective of the enum value, text is expected to be UTF-8 encoded
    enum class TextContentCharset
    {
        ASCII,        // Standard ASCII characters (0-127)
        ASCIISymbols, // Extended ASCII + certain common characters i.e. math symbols, arrows, ™, etc.
        UTF8Simple,   // Simple UTF8 encoded text without support for GPOS/kerning/ligatures (libgrapheme)
        UnicodeBidir  // Standard compliant Unicode BiDir algorithm implementation (Harfbuzz)
    };

    struct FontCollectionFile
    {
        std::string_view Files[FT_Total];
    };

    struct FontFileNames
    {
        FontCollectionFile Proportional;
        FontCollectionFile Monospace;
        std::string_view BasePath;
    };

    struct RenderConfig;

    enum FontLoadType : uint64_t
    {
        FLT_Proportional = 1,
        FLT_Monospace = 2,

        FLT_HasSmall = 4,
        FLT_HasSuperscript = 8,
        FLT_HasSubscript = 16,
        FLT_HasH1 = 32,
        FLT_HasH2 = 64,
        FLT_HasH3 = 128,
        FLT_HasH4 = 256,
        FLT_HasH5 = 512,
        FLT_HasH6 = 1024,

        // Use this to auto-scale fonts, loading the largest size for a family
        // NOTE: For ImGui backend, this will save on memory from texture
        FLT_AutoScale = 2048,

        // Include all <h*> tags
        FLT_HasHeaders = FLT_HasH1 | FLT_HasH2 | FLT_HasH3 | FLT_HasH4 | FLT_HasH5 | FLT_HasH6,

        // TODO: Handle absolute size font-size fonts (Look at imrichtext.cpp: PopulateSegmentStyle function)
    };

    struct FontDescriptor
    {
        std::optional<FontFileNames> names = std::nullopt;
        std::vector<float> sizes;
        TextContentCharset charset = TextContentCharset::ASCII;
        uint64_t flags = FLT_Proportional;
    };

//#ifndef IM_FONTMANAGER_STANDALONE
//    // Get font sizes required from the specified config and flt
//    // flt is a bitwise OR of FontLoadType flags
//    std::vector<float> GetFontSizes(const RenderConfig& config, uint64_t flt);
//#endif

    // Load default fonts based on provided descriptor. Custom paths can also be 
    // specified through FontDescriptor::names member. If not specified, a OS specific
    // default path is selected i.e. C:\Windows\Fonts for Windows and 
    // /usr/share/fonts/ for Linux.
    bool LoadDefaultFonts(const FontDescriptor* descriptors, int totalNames = 1);

//#ifndef IM_FONTMANAGER_STANDALONE
//    // Load default fonts from specified config, bitwise OR of FontLoadType flags, 
//    // and provided charset which determines which glyph ranges to load
//    bool LoadDefaultFonts(const RenderConfig& config, uint64_t flt, TextContentCharset charset);
//#endif

    // Find out path to .ttf file for specified font family and font type
    // The exact filepath returned is based on reading TTF OS/2 and name tables
    // The matching is done on best effort basis.
    // NOTE: This is not a replacement of fontconfig library and can only perform
    //       rudimentary font fallback
    [[nodiscard]] std::string_view FindFontFile(std::string_view family, FontType ft,
        std::string_view* lookupPaths = nullptr, int lookupSz = 0);

    //struct GlyphRangeMappedFont
    //{
    //    std::pair<int32_t, int32_t> Range;
    //    std::string_view FontPath;
    //};

    //// Find a set of fonts which match the glyph ranges provided through the charset
    //[[nodiscard]] std::vector<GlyphRangeMappedFont> FindFontFileEx(std::string_view family, 
    //    FontType ft, TextContentCharset charset, std::string_view* lookupPaths = nullptr, 
    //    int lookupSz = 0);

#ifdef IM_RICHTEXT_TARGET_IMGUI
    // Get the closest matching font based on provided parameters. The return type is
    // ImFont* cast to void* to better fit overall library.
    // NOTE: size matching happens with lower_bound calls, this is done because all fonts
    //       should be preloaded for ImGui, dynamic font atlas updates are not supported.
    [[nodiscard]] void* GetFont(std::string_view family, float size, FontType type);

//#ifndef IM_FONTMANAGER_STANDALONE
//    // Get font to display overlay i.e. style info in side panel
//    [[nodiscard]] void* GetOverlayFont(const RenderConfig& config);
//#endif

    // Return status of font atlas construction
    [[nodiscard]] bool IsFontLoaded();
#endif
#ifdef IM_RICHTEXT_TARGET_BLEND2D
    using FontFamilyToFileMapper = std::string_view(*)(std::string_view);
    struct FontExtraInfo { FontFamilyToFileMapper mapper = nullptr; std::string_view filepath; };

    // Preload font lookup info which is used in FindFontFile and GetFont function to perform
    // fast lookup + rudimentary fallback.
    void PreloadFontLookupInfo(int timeoutMs);

    // Get the closest matching font based on provided parameters. The return type is
    // BLFont* cast to void* to better fit overall library.
    // NOTE: The FontExtraInfo::mapper can be assigned to a function which loads fonts based
    //       con content codepoints and can perform better fallback.
    [[nodiscard]] void* GetFont(std::string_view family, float size, FontType type, FontExtraInfo extra);
#endif

    // =============================================================================================
    // INTERFACES
    // =============================================================================================

    template <typename T>
    struct Span
    {
        T const* source = nullptr;
        int sz = 0;

        template <typename ItrT>
        Span(ItrT start, ItrT end)
            : source{ &(*start) }, sz{ (int)(end - start) }
        {
        }

        Span(T const* from, int size) : source{ from }, sz{ size } {}

        explicit Span(T const& el) : source{ &el }, sz{ 1 } {}

        T const* begin() const { return source; }
        T const* end() const { return source + sz; }

        const T& front() const { return *source; }
        const T& back() const { return *(source + sz - 1); }

        const T& operator[](int idx) const { return source[idx]; }
    };

    template <typename T> Span(T* from, int size) -> Span<T>;
    template <typename T> Span(T&) -> Span<T>;

    struct BoundedBox
    {
        float top = 0.f, left = 0.f;
        float width = 0.f, height = 0.f;

        ImVec2 start(ImVec2 origin) const { return ImVec2{ left, top } + origin; }
        ImVec2 end(ImVec2 origin) const { return ImVec2{ left + width, top + height } + origin; }
        ImVec2 center(ImVec2 origin) const { return ImVec2{ left + (0.5f * width), top + (0.5f * height) } + origin; }
    };

    enum Direction
    {
        DIR_Horizontal = 1,
        DIR_Vertical
    };

    // Implement this to draw primitives in your favorite graphics API
    // TODO: Separate gradient creation vs. drawing
    struct IRenderer
    {
        void* UserData = nullptr;

        virtual void SetClipRect(ImVec2 startpos, ImVec2 endpos) = 0;
        virtual void ResetClipRect() = 0;

        virtual void DrawLine(ImVec2 startpos, ImVec2 endpos, uint32_t color, float thickness = 1.f) = 0;
        virtual void DrawPolyline(ImVec2* points, int sz, uint32_t color, float thickness) = 0;
        virtual void DrawTriangle(ImVec2 pos1, ImVec2 pos2, ImVec2 pos3, uint32_t color, bool filled, bool thickness = 1.f) = 0;
        virtual void DrawRect(ImVec2 startpos, ImVec2 endpos, uint32_t color, bool filled, float thickness = 1.f) = 0;
        virtual void DrawRoundedRect(ImVec2 startpos, ImVec2 endpos, uint32_t color, bool filled, float topleftr, float toprightr, float bottomrightr, float bottomleftr, float thickness = 1.f) = 0;
        virtual void DrawRectGradient(ImVec2 startpos, ImVec2 endpos, uint32_t colorfrom, uint32_t colorto, Direction dir) = 0;
        virtual void DrawRoundedRectGradient(ImVec2 startpos, ImVec2 endpos, float topleftr, float toprightr, float bottomrightr, float bottomleftr,
            uint32_t colorfrom, uint32_t colorto, Direction dir) = 0;
        virtual void DrawPolygon(ImVec2* points, int sz, uint32_t color, bool filled, float thickness = 1.f) = 0;
        virtual void DrawPolyGradient(ImVec2* points, uint32_t* colors, int sz) = 0;
        virtual void DrawCircle(ImVec2 center, float radius, uint32_t color, bool filled, bool thickness = 1.f) = 0;
        virtual void DrawSector(ImVec2 center, float radius, int start, int end, uint32_t color, bool filled, bool inverted, bool thickness = 1.f) = 0;
        virtual void DrawRadialGradient(ImVec2 center, float radius, uint32_t in, uint32_t out, int start, int end) = 0;

        virtual bool SetCurrentFont(std::string_view family, float sz, FontType type) { return false; };
        virtual bool SetCurrentFont(void* fontptr, float sz) { return false; };
        virtual void ResetFont() {};

        virtual ImVec2 GetTextSize(std::string_view text, void* fontptr, float sz, float wrapWidth = -1.f) = 0;
        virtual void DrawText(std::string_view text, ImVec2 pos, uint32_t color, float wrapWidth = -1.f) = 0;
        virtual void DrawTooltip(ImVec2 pos, std::string_view text) = 0;
        virtual float EllipsisWidth(void* fontptr, float sz);
    };

    // =============================================================================================
    // IMPLEMENTATIONS
    // =============================================================================================
    struct ImGuiRenderer final : public IRenderer
    {
        ImGuiRenderer();

        void SetClipRect(ImVec2 startpos, ImVec2 endpos);
        void ResetClipRect();

        void DrawLine(ImVec2 startpos, ImVec2 endpos, uint32_t color, float thickness = 1.f);
        void DrawPolyline(ImVec2* points, int sz, uint32_t color, float thickness);
        void DrawTriangle(ImVec2 pos1, ImVec2 pos2, ImVec2 pos3, uint32_t color, bool filled, bool thickness = 1.f);
        void DrawRect(ImVec2 startpos, ImVec2 endpos, uint32_t color, bool filled, float thickness = 1.f);
        void DrawRoundedRect(ImVec2 startpos, ImVec2 endpos, uint32_t color, bool filled, float topleftr, float toprightr, float bottomrightr, float bottomleftr, float thickness = 1.f);
        void DrawRectGradient(ImVec2 startpos, ImVec2 endpos, uint32_t colorfrom, uint32_t colorto, Direction dir);
        void DrawRoundedRectGradient(ImVec2 startpos, ImVec2 endpos, float topleftr, float toprightr, float bottomrightr, float bottomleftr,
            uint32_t colorfrom, uint32_t colorto, Direction dir);
        void DrawPolygon(ImVec2* points, int sz, uint32_t color, bool filled, float thickness = 1.f);
        void DrawPolyGradient(ImVec2* points, uint32_t* colors, int sz);
        void DrawCircle(ImVec2 center, float radius, uint32_t color, bool filled, bool thickness = 1.f);
        void DrawSector(ImVec2 center, float radius, int start, int end, uint32_t color, bool filled, bool inverted, bool thickness = 1.f);
        void DrawRadialGradient(ImVec2 center, float radius, uint32_t in, uint32_t out, int start, int end);

        bool SetCurrentFont(std::string_view family, float sz, FontType type) override;
        bool SetCurrentFont(void* fontptr, float sz) override;
        void ResetFont() override;
        [[nodiscard]] ImVec2 GetTextSize(std::string_view text, void* fontptr, float sz, float wrapWidth);
        void DrawText(std::string_view text, ImVec2 pos, uint32_t color, float wrapWidth = -1.f);
        void DrawTooltip(ImVec2 pos, std::string_view text);
        [[nodiscard]] float EllipsisWidth(void* fontptr, float sz) override;

    private:

        void ConstructRoundedRect(ImVec2 startpos, ImVec2 endpos, float topleftr, float toprightr, float bottomrightr, float bottomleftr);

        float _currentFontSz = 0.f;
    };

    // =============================================================================================
    // STYLING
    // =============================================================================================
    struct IntOrFloat
    {
        float value = 0.f;
        bool isFloat = false;
    };

    struct ColorStop
    {
        uint32_t from, to;
        float pos = -1.f;
    };
    
    struct ColorGradient
    {
        ColorStop colorStops[IM_RICHTEXT_MAX_COLORSTOPS];
        int totalStops = 0;
        float angleDegrees = 0.f;
        ImGuiDir dir = ImGuiDir::ImGuiDir_Down;
    };

    struct FourSidedMeasure
    {
        float top = 0.f, left = 0.f, right = 0.f, bottom = 0.f;

        float h() const { return left + right; }
        float v() const { return top + bottom; }
    };

    enum class LineType
    {
        Solid, Dashed, Dotted, DashDot
    };

    struct Border
    {
        uint32_t color = IM_COL32_BLACK_TRANS;
        float thickness = 0.f;
        LineType lineType = LineType::Solid; // Unused for rendering
    };

    enum BoxCorner
    {
        TopLeftCorner,
        TopRightCorner,
        BottomRightCorner,
        BottomLeftCorner
    };

    struct FourSidedBorder
    {
        Border top, left, bottom, right;
        float cornerRadius[4];
        bool isUniform = false;

        float h() const { return left.thickness + right.thickness; }
        float v() const { return top.thickness + bottom.thickness; }
        bool isRounded() const;
        bool exists() const;

        FourSidedBorder& setColor(uint32_t color);
        FourSidedBorder& setThickness(float thickness);
        FourSidedBorder& setRadius(float radius);
        //FourSidedBorder& setLineType(uint32_t color);
    };

    struct BoxShadow
    {
        ImVec2 offset{ 0.f, 0.f };
        float spread = 0.f;
        float blur = 0.f;
        uint32_t color = IM_COL32_BLACK_TRANS;
    };

    struct IntersectRects
    {
        ImRect intersects[4];
        bool visibleRect[4] = { true, true, true, true };
    };

    struct RectBreakup
    {
        ImRect rects[4];
        ImRect corners[4];
    };

    enum FontStyleFlag : int32_t
    {
        FontStyleNone = 0,
        FontStyleNormal = 1,
        FontStyleBold = 1 << 1,
        FontStyleItalics = 1 << 2,
        FontStyleLight = 1 << 3,
        FontStyleStrikethrough = 1 << 4,
        FontStyleUnderline = 1 << 5,
        FontStyleOverflowEllipsis = 1 << 6,
        FontStyleNoWrap = 1 << 7,
        FontStyleOverflowMarquee = 1 << 8
    };

    struct FontStyle
    {
        void* font = nullptr; // Pointer to font object
        std::string_view family = IM_RICHTEXT_DEFAULT_FONTFAMILY;
        float size = 24.f;
        int32_t flags = FontStyleNone;
    };

    enum StyleProperty : int64_t
    {
        StyleError = -1,
        NoStyleChange = 0,
        StyleBackground = 1,
        StyleFgColor = 1 << 1,
        StyleFontSize = 1 << 2,
        StyleFontFamily = 1 << 3,
        StyleFontWeight = 1 << 4,
        StyleFontStyle = 1 << 5,
        StyleHeight = 1 << 6,
        StyleWidth = 1 << 7,
        StyleListBulletType = 1 << 8,
        StyleHAlignment = 1 << 9,
        StyleVAlignment = 1 << 10,
        StylePadding = 1 << 11,
        StyleMargin = 1 << 12,
        StyleBorder = 1 << 13,
        StyleOverflow = 1 << 14,
        StyleBorderRadius = 1 << 16,
        StyleCellSpacing = 1 << 17,
        StyleBlink = 1 << 18,
        StyleTextWrap = 1 << 19,
        StyleBoxShadow = 1 << 20,
        StyleWordBreak = 1 << 21,
        StyleWhitespaceCollapse = 1 << 22,
        StyleWhitespace = 1 << 23,
        StyleTextOverflow = 1 << 24,
    };

    enum TextAlignment
    {
        TextAlignLeft = 1,
        TextAlignRight = 1 << 1,
        TextAlignHCenter = 1 << 2,
        TextAlignTop = 1 << 3,
        TextAlignBottom = 1 << 4,
        TextAlignVCenter = 1 << 5,
        TextAlignJustify = 1 << 6,
        TextAlignCenter = TextAlignHCenter | TextAlignVCenter,
        TextAlignLeading = TextAlignLeft | TextAlignVCenter
    };

    // Geometry functions
    [[nodiscard]] IntersectRects ComputeIntersectRects(ImRect rect, ImVec2 startpos, ImVec2 endpos);
    [[nodiscard]] RectBreakup ComputeRectBreakups(ImRect rect, float amount);

    // Generic string helpers, case-insensitive matches
    [[nodiscard]] bool AreSame(const std::string_view lhs, const char* rhs);
    [[nodiscard]] bool AreSame(const std::string_view lhs, const std::string_view rhs);
    [[nodiscard]] bool StartsWith(const std::string_view lhs, const char* rhs);
    [[nodiscard]] int SkipDigits(const std::string_view text, int from = 0);
    [[nodiscard]] int SkipFDigits(const std::string_view text, int from = 0);
    [[nodiscard]] int SkipSpace(const std::string_view text, int from = 0);
    [[nodiscard]] int SkipSpace(const char* text, int idx, int end);
    [[nodiscard]] std::optional<std::string_view> GetQuotedString(const char* text, int& idx, int end);

    // String to number conversion functions
    [[nodiscard]] int ExtractInt(std::string_view input, int defaultVal);
    [[nodiscard]] int ExtractIntFromHex(std::string_view input, int defaultVal);
    [[nodiscard]] IntOrFloat ExtractNumber(std::string_view input, float defaultVal);
    [[nodiscard]] float ExtractFloatWithUnit(std::string_view input, float defaultVal, float ems, float parent, float scale);

    // Color related functions
    [[nodiscard]] uint32_t ToRGBA(int r, int g, int b, int a = 255);
    [[nodiscard]] uint32_t ToRGBA(float r, float g, float b, float a = 1.f);
    [[nodiscard]] uint32_t GetColor(const char* name, void*);
    [[nodiscard]] bool IsColorVisible(uint32_t color);

    // Parsing functions
    [[nodiscard]] uint32_t ExtractColor(std::string_view stylePropVal, uint32_t(*NamedColor)(const char*, void*), void* userData);
    [[nodiscard]] ColorGradient ExtractLinearGradient(std::string_view input, uint32_t(*NamedColor)(const char*, void*), void* userData);
    [[nodiscard]] Border ExtractBorder(std::string_view input, float ems, float percent,
        uint32_t(*NamedColor)(const char*, void*), void* userData);
    [[nodiscard]] BoxShadow ExtractBoxShadow(std::string_view input, float ems, float percent,
        uint32_t(*NamedColor)(const char*, void*), void* userData);

    enum AnimationType
    {
        AT_None = 0,
        AT_Marquee = 1,
        AT_Blink = 1 << 1,
        AT_Shimmer = 1 << 2,
        AT_Cycle = 1 << 3,
        AT_Pulsate = 1 << 4,
        AT_Linear = 1 << 5,
        AT_Bounce = 1 << 6,
        AT_ExpDecay = 1 << 7,
        AT_Switch = 1 << 8
    };

    enum AnimationElement
    {
        AE_None = 0,
        AE_BgColor = 1,
        AE_FgColor = 1 << 1,
        AE_Padding = 1 << 2,
        AE_BorderColor = 1 << 3,
        AE_BorderWidth = 1 << 4,
        AE_FontSize = 1 << 5,
        AE_FontWeight = 1 << 6
    };

    struct StyleDescriptor
    {
        uint32_t bgcolor = IM_COL32_BLACK_TRANS;
        uint32_t fgcolor = IM_COL32_BLACK;
        ImVec2 dimension;
        //FontStyle font;
        int32_t alignment = TextAlignLeading;
        FourSidedMeasure padding;
        FourSidedMeasure margin;
        //FourSidedBorder border;
        //BoxShadow shadow;
        //ColorGradient gradient;
        int32_t animation = -1;

        //int32_t _borderCornerRel = 0;

        StyleDescriptor& BgColor(int r, int g, int b, int a) { bgcolor = ToRGBA(r, g, b, a); return *this; }
        StyleDescriptor& FgColor(int r, int g, int b, int a) { fgcolor = ToRGBA(r, g, b, a); return *this; }
        StyleDescriptor& Size(float w, float h) { dimension = ImVec2{ w, h }; return *this; }
        StyleDescriptor& Align(int32_t align) { alignment = align; return *this; }
        StyleDescriptor& Padding(float p) { padding.left = padding.top = padding.bottom = padding.right = p; return *this; }
        StyleDescriptor& Margin(float p) { margin.left = margin.top = margin.bottom = margin.right = p; return *this; }
        StyleDescriptor& Border(float thick, std::tuple<int, int, int, int> color) 
        { border.setThickness(thick); border.setColor(ToRGBA(std::get<0>(color), std::get<1>(color), std::get<2>(color), std::get<3>(color))); return *this; }
        StyleDescriptor& Raised(float amount) { shadow.blur = amount; shadow.color = ToRGBA(100, 100, 100); return *this; }

        /*template <typename T>
        StyleDescriptor& Animate(AnimationElement el, AnimationType type, )*/
    };

    enum class BoxShadowQuality
    {
        Fast,     // Shadow corners are hard triangles
        Balanced, // Shadow corners are rounded (with coarse roundedness)
        High      // Unimplemented
    };

    struct WindowConfig
    {
        uint32_t bgcolor;
        int32_t tooltipDelay = 500;
        float tooltipFontSz = 16.f;
        float defaultFontSz = 16.f;
        std::string_view tooltipFontFamily = IM_RICHTEXT_DEFAULT_FONTFAMILY;
        BoxShadowQuality shadowQuality = BoxShadowQuality::Balanced;
    };

    // =============================================================================================
    // DRAWING FUNCTIONS
    // =============================================================================================

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
                // TODO: Add support for non-axis aligned gradients
                /*ImVec2 points[4];
                ImU32 colors[4];

                for (auto it = start; it != end; ++it)
                {
                    auto extent = width * it->pos;
                    auto m = std::tanf(angle);

                    points[0] = initpos;
                    points[1] = points[0] + ImVec2{ extent, 0.f };
                    points[2] = initpos - ImVec2{ m * initpos.y, height };
                    points[3] = points[2] + ImVec2{ extent, 0.f };

                    colors[0] = colors[3] = it->from;
                    colors[1] = colors[2] = it->to;

                    DrawPolyFilledMultiColor(drawList, points, colors, 4);
                    initpos.x += extent;
                }*/

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

    void DrawBorderRect(const FourSidedBorder& border, ImVec2 startpos, ImVec2 endpos, uint32_t bgcolor, IRenderer& renderer);
    void DrawBoxShadow(ImVec2 startpos, ImVec2 endpos, const BoxShadow& shadow, FourSidedBorder border, IRenderer& renderer);
    void DrawBackground(ImVec2 startpos, ImVec2 endpos, const ColorGradient& gradient, uint32_t color, const FourSidedBorder& border, IRenderer& renderer);
    void DrawText(ImVec2 startpos, ImVec2 endpos, std::string_view text, bool disabled, const StyleDescriptor& style, IRenderer& renderer);

    // =============================================================================================
    // WIDGETS
    // =============================================================================================

    enum WidgetState : int32_t
    {
        WS_Default = 0, 
        WS_Disabled = 1,
        WS_Focused = 1 << 1,
        WS_Hovered = 1 << 2,
        WS_Pressed = 1 << 3,
        WS_Dragged = 1 << 4,
    };

    inline void ValidateState(int state)
    {
        assert((state % 2 == 0) || (state == 1));
    }

    /*template <typename K, typename V, int max, int mod = max> struct PerfectHashMap
    {
        static_assert((sizeof(V) * max) <= (1 << 12));
        static_assert(std::is_integral_v<K>);

        PerfectHashMap() { std::memset(mapped, 0, sizeof(mapped)); }

        static int hash(K key) const { auto h = key % mod; while (occupancy & (1 << h)) { h++; h %= mod; } return h; };

        template <typename... Vt>
        void emplace(K key, Vt&&... value) { 
            auto h = hash(key); 
            if constexpr (!std::is_fundamental_v<V>)
                ::new (void*)(mapped + (h * sizeof(V))) V{ std::forward<Vt>(value)... }; 
            else if constexpr (sizeof...(Vt) == 1)
                std::memset(mapped[h * sizeof(V)],  
            occupancy |= (1 << h); 
        }
        V& operator[](K key) { auto h = hash(key); occupancy |= (1 << h); return mapped[h]; }
        V const& operator[](K key) const { return mapped[hash(key)]; }
        bool exists(K key) const { return occupancy & (1 << hash(key)); }
        bool empty() const { return occupancy == 0; }

        void clear() { 
            if constexpr (!std::is_fundamental_v<V>)
                for (auto h = 0; h < mod; ++h) if (occupancy & (1 << h)) getvptr(h)->~V();
            std::memset(mapped, 0, sizeof(mapped)); occupancy = 0; 
        }

    private:

        V* getvptr(int h) { return reinterpret_cast<V*>((void*)(mapped + h * sizeof(V))); }

        unsigned char mapped[max * sizeof(V)];
        uint64_t occupancy = 0;
    };
*/

    struct CommonWidgetData
    {
        int32_t state = WS_Default;
        std::string_view tooltip = "";
        long long _hoverDuration = 0; // for tooltip
    };
    
    struct ButtonState : public CommonWidgetData
    {
        std::string_view text;
    };

    enum class WidgetEvent
    {
        None, Clicked, Hovered, Pressed, DoubleClicked, Dragged
    };

    struct WidgetDrawResult
    {
        WidgetEvent event = WidgetEvent::None;
        ImRect geometry;
    };

    ButtonState& RegisterButton(int id);
    WidgetDrawResult Button(int id, bool disabled, std::string_view text, ImVec2 pos, IRenderer& renderer, 
        std::optional<ImVec2> geometry = std::nullopt);

    StyleDescriptor& GetStyle(int id, int state);

    struct GridLayout
    {
        int32_t rows = 0, cols = 0;
        int32_t currrow = -1, currcol = -1;
        ImVec2 dimension;
    };
}


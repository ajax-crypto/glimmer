#pragma once

#include "types.h"

namespace glimmer
{
    struct LayoutDescriptor;

    [[nodiscard]] uint32_t ToRGBA(int r, int g, int b, int a = 255);
    [[nodiscard]] uint32_t ToRGBA(float r, float g, float b, float a = 1.f);
    [[nodiscard]] uint32_t ToRGBA(const std::tuple<int, int, int, int>& color);
    [[nodiscard]] uint32_t ToRGBA(const std::tuple<int, int, int>& color);
    [[nodiscard]] std::tuple<int, int, int, int> DecomposeColor(uint32_t color);
    [[nodiscard]] uint32_t GetColor(const char* name, void*);

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

    struct FontStyle
    {
        void* font = nullptr; // Pointer to font object
        std::string_view family = IM_RICHTEXT_DEFAULT_FONTFAMILY;
        float size = 16.f;
        int32_t flags = TextIsPlainText;
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
        StyleMinWidth = 1 << 25,
        StyleMaxWidth = 1 << 26,
        StyleMinHeight = 1 << 27,
        StyleMaxHeight = 1 << 28,
        StyleThumbColor = 1 << 29,
        StyleTrackColor = 1 << 30,
        StyleTrackOutlineColor = 1ll << 31,
        StyleThumbOffset = 1ll << 32,

        StyleUpdatedFromBase = 1ll << 62,
        StyleTotal = 28
    };

    enum RelativeStyleProperty : uint32_t
    {
        None = 0,
        RSP_Width = 1,
        RSP_Height = 1 << 1,
        RSP_MinHeight = 1 << 2,
        RSP_MaxHeight = 1 << 3,
        RSP_MinWidth = 1 << 4,
        RSP_MaxWidth = 1 << 5,
        RSP_BorderTopRightRadius = 1 << 6,
        RSP_BorderTopLeftRadius = 1 << 7,
        RSP_BorderBottomLeftRadius = 1 << 8,
        RSP_BorderBottomRightRadius = 1 << 9,
    };

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

    struct CustomStyleDataIndices
    {
        unsigned animation : 8;
        unsigned custom : 16;
    };

    struct StyleDescriptor
    {
        uint64_t specified = 0;
        uint32_t bgcolor = IM_COL32_BLACK_TRANS;
        uint32_t fgcolor = IM_COL32_BLACK;
        ImVec2 dimension{ -1.f, -1.f };
        ImVec2 mindim{ 0.f, 0.f };
        ImVec2 maxdim{ FLT_MAX, FLT_MAX };
        int32_t alignment = TextAlignLeading;
        uint32_t relativeProps = 0;
        CustomStyleDataIndices index;
        FourSidedMeasure padding;
        FourSidedMeasure margin;
        FourSidedBorder border;
        FontStyle font;
        BoxShadow shadow;
        ColorGradient gradient;

        StyleDescriptor();
        StyleDescriptor(std::string_view css);

        StyleDescriptor& BgColor(int r, int g, int b, int a = 255);
        StyleDescriptor& FgColor(int r, int g, int b, int a = 255);
        StyleDescriptor& Size(float w, float h);
        StyleDescriptor& Align(int32_t align);
        StyleDescriptor& Padding(float p);
        StyleDescriptor& Margin(float p);
        StyleDescriptor& Border(float thick, std::tuple<int, int, int, int> color);
        StyleDescriptor& Raised(float amount);

        StyleDescriptor& From(std::string_view css, bool checkForDuplicate = true);
    };

    struct ToggleButtonStyleDescriptor
    {
        uint32_t trackColor;
        uint32_t trackBorderColor;
        uint32_t thumbColor;
        int32_t trackGradient = -1;
        int32_t indicatorTextColor; // Text color of ON/OFF text
        float trackBorderThickness = 0.f;
        float thumbOffset = -2.f;
        float thumbExpand = 0.f;
        float animate = 0.3f;
        bool showText = true; // Show ON/OFF text inside button
        void* fontptr = nullptr;
        float fontsz = 12.f;

        static ToggleButtonStyleDescriptor ParseFrom(std::string_view css);
    };

    struct SliderStyleDescriptor
    {
        uint32_t trackColor;
        uint32_t thumbColor;
        int32_t trackGradient = -1;
        float trackBorderThickness = 1.f;
        float thumbOffset = 2.f;
        float thumbExpand = 0.f;

        static SliderStyleDescriptor ParseFrom(std::string_view css);
    };

    struct SpinnerStyleDescriptor
    {
        uint32_t upColor, downColor;
        uint32_t upbtnColor, downbtnColor;
        float btnBorderThickness = 2.f;
        bool upDownArrows = true;

        static SpinnerStyleDescriptor ParseFrom(std::string_view css);
    };

    struct RadioButtonStyleDescriptor
    {
        uint32_t checkedColor = ToRGBA(0, 0, 0);
        uint32_t outlineColor = ToRGBA(0, 0, 0);
        float outlineThickness = 2.f;
        float checkedRadius = 0.6f; // relative to total
        float animate = 0.3f;

        static RadioButtonStyleDescriptor ParseFrom(std::string_view css);
    };

    struct TabBarStyleDescriptor
    {
        uint32_t pincolor = 0;
        uint32_t pinbgcolor = 0;
        uint32_t closecolor = 0;
        uint32_t closebgcolor = 0;
        float pinPadding = 2.f;
        float closePadding = 2.f;

        static TabBarStyleDescriptor ParseFrom(std::string_view css);
    };

    union CommonWidgetStyleDescriptor
    {
        ToggleButtonStyleDescriptor toggle;

        CommonWidgetStyleDescriptor() {}
    };

    void PushStyle(std::string_view defcss, std::string_view hovercss = "", std::string_view pressedcss = "",
        std::string_view focusedcss = "", std::string_view checkedcss = "", std::string_view disblcss = "");
    void PushStyle(WidgetState state, std::string_view css);
    void SetNextStyle(std::string_view defcss, std::string_view hovercss = "", std::string_view pressedcss = "",
        std::string_view focusedcss = "", std::string_view checkedcss = "", std::string_view disblcss = "");
    StyleDescriptor& GetCurrentStyle(int32_t state = WS_Default);
    void PopStyle(int depth = 1, int32_t state = WS_Default);
    void CopyStyle(const StyleDescriptor& src, StyleDescriptor& dest);

    std::pair<Sizing, bool> ParseLayoutStyle(LayoutDescriptor& layout, std::string_view css, float pwidth, float pheight);

#define RECT_OUT(X) X.Min.x, X.Min.y, X.Max.x, X.Max.y
}

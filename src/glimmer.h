#pragma once

#define IM_RICHTEXT_MAX_COLORSTOPS 4

#include "imgui.h"
#include "imgui_internal.h"
#include "im_font_manager.h"

namespace glimmer
{
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

    [[nodiscard]] uint32_t ToRGBA(int r, int g, int b, int a = 255);
    [[nodiscard]] uint32_t ToRGBA(float r, float g, float b, float a = 1.f);
    [[nodiscard]] uint32_t ToRGBA(const std::tuple<int, int, int, int>& color);
    [[nodiscard]] uint32_t ToRGBA(const std::tuple<int, int, int>& color);
    [[nodiscard]] uint32_t GetColor(const char* name, void*);

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
        std::string_view family = IM_RICHTEXT_MONOSPACE_FONTFAMILY;
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

    constexpr unsigned int InvalidIdx = (unsigned)-1;

    enum WidgetType
    {
        WT_Invalid = -1,
        WT_Sublayout = -2,
        WT_Label = 0, WT_Button, WT_RadioButton, WT_ToggleButton,
        WT_ItemGrid,
        WT_Custom,
        WT_TotalTypes
    };

    struct CustomStyleDataIndices
    {
        unsigned animation : 8;
        unsigned custom : 16;
    };

    enum class OverflowMode { Clip, Wrap, Scroll };

    struct StyleDescriptor
    {
        uint64_t specified = 0;
        uint32_t bgcolor = IM_COL32_BLACK_TRANS;
        uint32_t fgcolor = IM_COL32_BLACK;
        CustomStyleDataIndices index;
        ImVec2 dimension{ -1.f, -1.f };
        ImVec2 mindim{ 0.f, 0.f };
        ImVec2 maxdim{ FLT_MAX, FLT_MAX };
        int32_t alignment = TextAlignLeading;
        uint32_t relativeProps = 0;
        FourSidedMeasure padding;
        FourSidedMeasure margin;
        FourSidedBorder border;
        FontStyle font;
        BoxShadow shadow;
        ColorGradient gradient;

        StyleDescriptor();
        StyleDescriptor(std::string_view css);

        StyleDescriptor& BgColor(int r, int g, int b, int a = 255) { bgcolor = ToRGBA(r, g, b, a); return *this; }
        StyleDescriptor& FgColor(int r, int g, int b, int a = 255) { fgcolor = ToRGBA(r, g, b, a); return *this; }
        StyleDescriptor& Size(float w, float h) { dimension = ImVec2{ w, h }; return *this; }
        StyleDescriptor& Align(int32_t align) { alignment = align; return *this; }
        StyleDescriptor& Padding(float p) { padding.left = padding.top = padding.bottom = padding.right = p; return *this; }
        StyleDescriptor& Margin(float p) { margin.left = margin.top = margin.bottom = margin.right = p; return *this; }
        StyleDescriptor& Border(float thick, std::tuple<int, int, int, int> color);
        StyleDescriptor& Raised(float amount);

        StyleDescriptor& From(std::string_view css, bool checkForDuplicate = true);
    };

    enum class BoxShadowQuality
    {
        Fast,     // Shadow corners are hard triangles
        Balanced, // Shadow corners are rounded (with coarse roundedness)
        High      // Unimplemented
    };

    enum class LayoutPolicy
    {
        ImmediateMode, DeferredMode
    };

    struct WindowConfig
    {
        uint32_t bgcolor;
        int32_t tooltipDelay = 500;
        float tooltipFontSz = 16.f;
        float defaultFontSz = 16.f;
        float fontScaling = 2.f;
        float scaling = 1.f;
        float scrollbarSz = 15.f;
        ImVec2 toggleButtonSz{ 100.f, 40.f };
        std::string_view tooltipFontFamily = IM_RICHTEXT_DEFAULT_FONTFAMILY;
        BoxShadowQuality shadowQuality = BoxShadowQuality::Balanced;
        LayoutPolicy layoutPolicy = LayoutPolicy::ImmediateMode;
        IRenderer* renderer = nullptr;
        void* userData = nullptr;
    };

    // =============================================================================================
    // WIDGETS
    // =============================================================================================

    enum WidgetState : int32_t
    {
        WS_Default = 0,
        WS_Focused = 1 << 1,
        WS_Hovered = 1 << 2,
        WS_Pressed = 1 << 3,
        WS_Checked = 1 << 4,
        WS_Disabled = 1 << 5,
        WS_Dragged = 1 << 6,
    };

    inline void ValidateState(int state)
    {
        assert((state % 2 == 0) || (state == 1));
    }

    struct CommonWidgetData
    {
        int32_t state = WS_Default;
        std::string_view tooltip = "";
        long long _hoverDuration = 0; // for tooltip
        bool floating = false;
    };

    struct ButtonState : public CommonWidgetData
    {
        std::string_view text;
    };

    using LabelState = ButtonState;

    struct ToggleButtonState : public CommonWidgetData
    {
        bool checked = false;
    };

    using RadioButtonState = ToggleButtonState;

    enum ColumnProperty : int32_t
    {
        COL_Resizable = 1,
        COL_Pinned = 2,
        COL_Sortable = 1 << 2,
        COL_Filterable = 1 << 3,
        COL_Expandable = 1 << 4,
        COL_WidthAbsolute = 1 << 5,
        COL_WrapHeader = 1 << 6,
    };

    struct ItemGridState : public CommonWidgetData
    {
        struct CellData 
        { 
            WidgetType wtype = WT_Label; 
            union CellState
            {
                LabelState label;
                ButtonState button;
                ToggleButtonState toggle;
                RadioButtonState radio;
                ImVec2 (*CustomWidget)(ImVec2, ImVec2);
                // add support for custom combinations of widgets

                CellState() {}
                ~CellState() {}
            } state;
            int16_t colspan = 1;
            int16_t children = 0;

            CellData() { state.label = LabelState{}; }
            ~CellData() {}
        };

        struct ColumnConfig
        {
            std::string_view name;
            ImRect content;
            ImRect textrect;
            int32_t props = COL_Resizable;
            int16_t width = 0;
            int16_t parent = -1;
            StyleDescriptor style;
        };

        struct Configuration
        {
            std::vector<std::vector<ColumnConfig>> headers;
            int32_t rows = 0;
        } config;

        CellData& (*cell)(int32_t, int16_t, int16_t) = nullptr;
        int32_t currow = -1;
        int16_t currcol = -1;
        int16_t sortedcol = -1;
        int16_t coldrag = -1;
        bool uniformRowHeights = false;

        void setCellPadding(float padding);
        void setColumnResizable(int16_t col, bool resizable);
    };

    enum class WidgetEvent
    {
        None, Clicked, Hovered, Pressed, DoubleClicked, RightClicked, Dragged, Edited
    };

    struct WidgetDrawResult
    {
        int32_t id = -1;
        WidgetEvent event = WidgetEvent::None;
        int32_t row = -1;
        int16_t col = -1;
        int16_t depth = -1;
        ImRect geometry, content;
    };

    struct WidgetStateData
    {
        WidgetType type;

        union SharedWidgetState {
            LabelState label;
            ButtonState button;
            ToggleButtonState toggle;
            RadioButtonState radio;
            ItemGridState grid;
            // Add others...

            SharedWidgetState() {}
            ~SharedWidgetState() {}
        } state;

        WidgetStateData(WidgetType type);
        WidgetStateData(const WidgetStateData& src);
        ~WidgetStateData();
    };

    WindowConfig& GetWindowConfig();

    void BeginFrame();
    void EndFrame();
    int32_t GetNextId(WidgetType type);
    int16_t GetNextCount(WidgetType type);

    enum WidgetGeometry : int32_t
    {
        ExpandH = 2, ExpandV = 4, ExpandAll = ExpandH | ExpandV,
        ToLeft = 8, ToRight = 16, ToBottom = 32, ToTop = 64,
        FromRight = ToLeft, FromLeft = ToRight, FromTop = ToBottom, FromBottom = ToTop,
        ToBottomLeft = ToLeft | ToBottom, ToBottomRight = ToBottom | ToRight,
        ToTopLeft = ToTop | ToLeft, ToTopRight = ToTop | ToRight
    };

    struct NeighborWidgets
    {
        int32_t top = -1, left = -1, right = -1, bottom = -1;
    };

    WidgetStateData& CreateWidget(WidgetType type, int16_t id);
    WidgetStateData& CreateWidget(int32_t id);

    WidgetDrawResult Label(int32_t id, ImVec2 pos, int32_t geometry = 0);
    WidgetDrawResult Button(int32_t id, ImVec2 pos, int32_t geometry = 0);
    WidgetDrawResult ToggleButton(int32_t id, ImVec2 pos, int32_t geometry = 0);
    WidgetDrawResult CheckBox(int32_t id, ImVec2 pos, int32_t geometry = 0);

    void Label(int32_t id, int32_t geometry = 0, const NeighborWidgets& neighbors = NeighborWidgets{});
    void Button(int32_t id, int32_t geometry = 0, const NeighborWidgets& neighbors = NeighborWidgets{});
    void ToggleButton(int32_t id, int32_t geometry = 0, const NeighborWidgets& neighbors = NeighborWidgets{});
    void CheckBox(int32_t id, int32_t geometry = 0, const NeighborWidgets& neighbors = NeighborWidgets{});
    void ItemGrid(int32_t id, int32_t geometry = 0, const NeighborWidgets& neighbors = NeighborWidgets{});

    void PushStyle(std::string_view defcss, std::string_view hovercss = "", std::string_view pressedcss = "",
        std::string_view focusedcss = "", std::string_view checkedcss = "", std::string_view disblcss = "");
    void SetNextStyle(std::string_view defcss, std::string_view hovercss = "", std::string_view pressedcss = "",
        std::string_view focusedcss = "", std::string_view checkedcss = "", std::string_view disblcss = "");
    StyleDescriptor& GetCurrentStyle(int32_t state = WS_Default);
    void PopStyle(int depth = 1, int32_t state = WS_Default);

    enum class Layout { Invalid, Horizontal, Vertical, Grid };
    enum FillDirection : int32_t { FD_None = 0, FD_Horizontal = 1, FD_Vertical = 2 };

    constexpr float EXPAND_SZ = FLT_MAX;
    constexpr float FIT_SZ = -1.f;
    constexpr float SHRINK_SZ = -2.f;

    // Ad-hoc Layout
    void PushSpan(int32_t direction); // Combination of FillDirection
    void SetSpan(int32_t direction); // Combination of FillDirection
    void PushDir(int32_t direction); // Combination of WidgetGeometry
    void SetDir(int32_t direction); // Combination of WidgetGeometry
    void PopSpan(int depth = 1);

    void Move(int32_t direction); // Combination of FillDirection
    void Move(int32_t id, int32_t direction); // Combination of FillDirection
    void Move(int32_t hid, int32_t vid, bool toRight, bool toBottom); // Combination of WidgetGeometry flags
    void Move(ImVec2 amount, int32_t direction); // Combination of WidgetGeometry
    void Move(ImVec2 pos);

    // Structured Layout inside container
    ImRect BeginLayout(Layout layout, FillDirection fill, int32_t alignment = TextAlignLeading, 
        ImVec2 spacing = { 0.f, 0.f }, const NeighborWidgets& neighbors = NeighborWidgets{});
    ImRect BeginLayout(Layout layout, std::string_view desc, const NeighborWidgets& neighbors = NeighborWidgets{});
    void PushSizing(float width, float height, bool relativew = false, bool relativeh = false);
    void PopSizing(int depth = -1);
    ImRect EndLayout(int depth = 1);
}


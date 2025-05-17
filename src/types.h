#pragma once

#define IM_RICHTEXT_MAX_COLORSTOPS 4

#include "platform.h"
#include "utils.h"

#include <string_view>
#include <optional>
#include <vector>

#ifndef IM_RICHTEXT_DEFAULT_FONTFAMILY
#define IM_RICHTEXT_DEFAULT_FONTFAMILY "default-font-family"
#endif
#ifndef IM_RICHTEXT_MONOSPACE_FONTFAMILY
#define IM_RICHTEXT_MONOSPACE_FONTFAMILY "monospace-family"
#endif

namespace glimmer
{
    // =============================================================================================
    // STYLING
    // =============================================================================================
    enum Direction
    {
        DIR_Horizontal = 1,
        DIR_Vertical
    };
    
    struct IntOrFloat
    {
        float value = 0.f;
        bool isFloat = false;
    };

    constexpr unsigned int InvalidIdx = (unsigned)-1;

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

    struct IRenderer;

    struct WindowConfig
    {
        uint32_t bgcolor;
        uint32_t focuscolor;
        int32_t tooltipDelay = 500;
        float tooltipFontSz = 16.f;
        float defaultFontSz = 16.f;
        float fontScaling = 2.f;
        float scaling = 1.f;
        float scrollbarSz = 15.f;
        float scrollAppearAnimationDuration = 0.3f;
        float splitterSize = 5.f;
        ImVec2 toggleButtonSz{ 100.f, 40.f };
        std::string_view tooltipFontFamily = IM_RICHTEXT_DEFAULT_FONTFAMILY;
        BoxShadowQuality shadowQuality = BoxShadowQuality::Balanced;
        LayoutPolicy layoutPolicy = LayoutPolicy::ImmediateMode;
        IRenderer* renderer = nullptr;
        IPlatform* platform = nullptr;
        void* userData = nullptr;
    };

    inline bool IsColorVisible(uint32_t color)
    {
        return (color & 0xFF000000) != 0;
    }

    // =============================================================================================
    // WIDGETS
    // =============================================================================================

    enum WidgetType
    {
        WT_Invalid = -1,
        WT_Sublayout = -2,
        WT_Label = 0, WT_Button, WT_RadioButton, WT_ToggleButton, WT_Checkbox,
        WT_Layout, WT_Scrollable, WT_Splitter, WT_SplitterScrollRegion,
        WT_Slider,
        WT_TextInput,
        WT_DropDown,
        WT_TabBar,
        WT_ItemGrid,
        WT_Custom,
        WT_TotalTypes
    };

    enum WidgetState : int32_t
    {
        WS_Default = 0,
        WS_Focused = 1 << 1,
        WS_Hovered = 1 << 2,
        WS_Pressed = 1 << 3,
        WS_Checked = 1 << 4,
        WS_Disabled = 1 << 5,
        WS_PartialCheck = 1 << 6,
        WS_Selected = 1 << 7,
        WS_Dragged = 1 << 8,
    };

    inline void ValidateState(int state)
    {
        assert((state % 2 == 0) || (state == 1));
    }

    enum class TextType { PlainText, RichText, SVG };

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
        TextType type = TextType::PlainText;
    };

    using LabelState = ButtonState;

    struct ToggleButtonState : public CommonWidgetData
    {
        bool checked = false;
    };

    using RadioButtonState = ToggleButtonState;

    enum class CheckState 
    {
        Checked, Unchecked, Partial
    };

    struct CheckboxState : public CommonWidgetData
    {
        CheckState check = CheckState::Unchecked;
    };

    struct ScrollBarState
    {
        ImVec2 pos;
        ImVec2 lastMousePos;
        float opacity = 0.f;
        bool mouseDownOnVGrip = false;
        bool mouseDownOnHGrip = false;
    };

    struct LayoutRegion
    {
        ImRect viewport{ { -1.f, -1.f }, {} }; // visible region of content
        ImVec2 max; // maximum coordinates of the widgets inside region
        //Vector<int32_t, int16_t> widgets; // widget ids for defered rendering
    };

    struct ScrollableRegion : public LayoutRegion
    {
        std::pair<bool, bool> enabled; // enable scroll in horizontal and vertical direction respectively
        ScrollBarState state;
    };

    struct TextInputState : public CommonWidgetData
    {
        std::vector<char> text;
        std::string_view placeholder;
        std::pair<int, int> selection{ -1, -1 };
        void (*ShowList)(const TextInputState&, ImVec2) = nullptr;
    };

    struct DropDownState : public CommonWidgetData
    {
        std::string_view text;
        Direction dir;
        TextInputState input;
        int32_t inputId = -1;
        int32_t selected = -1;
        std::span<std::pair<WidgetType, std::string_view>> options;
        bool isComboBox = false;
        bool opened = false;
        void (*ShowList)(ImVec2, ImVec2, DropDownState&) = nullptr;
    };

    enum ColumnProperty : int32_t
    {
        COL_Resizable = 1,
        COL_Pinned = 2,
        COL_Sortable = 1 << 2,
        COL_Filterable = 1 << 3,
        COL_Expandable = 1 << 4,
        COL_WidthAbsolute = 1 << 5,
        COL_WrapHeader = 1 << 6,
        COL_Moveable = 1 << 7
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
                ImVec2(*CustomWidget)(ImVec2, ImVec2);
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

        void setColumnResizable(int16_t col, bool resizable);
        void setColumnProps(int16_t col, ColumnProperty prop, bool set = true);
    };

    enum TabButtonFlags : int32_t
    {
        TAB_Closeable = 1,
        TAB_Pinned = 2,
        TAB_Add = 4
    };

    struct TabBarState
    {
        struct TabButtonDesc
        {
            ButtonState state;
            int32_t flags = 0;
        };

        int selected = 0;
        std::vector<TabButtonDesc> tabs;
        bool scrollable = false;
        bool horizontal = true;
    };

    enum class WidgetEvent
    {
        None, Focused, Clicked, Hovered, Pressed, DoubleClicked, RightClicked, Dragged, Edited, Selected
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
            CheckboxState checkbox;
            TextInputState input;
            DropDownState dropdown;
            TabBarState tab;
            ItemGridState grid;
            ScrollableRegion scroll;
            // Add others...

            SharedWidgetState() {}
            ~SharedWidgetState() {}
        } state;

        WidgetStateData(WidgetType type);
        WidgetStateData(const WidgetStateData& src);
        WidgetStateData& operator=(const WidgetStateData& src);
        ~WidgetStateData();
    };

    enum WidgetGeometry : int32_t
    {
        ExpandH = 2, ExpandV = 4, ExpandAll = ExpandH | ExpandV,
        ToLeft = 8, ToRight = 16, ToBottom = 32, ToTop = 64,
        OnlyOnce = 1 << 16,
        FromRight = ToLeft, FromLeft = ToRight, FromTop = ToBottom, FromBottom = ToTop,
        ToBottomLeft = ToLeft | ToBottom, ToBottomRight = ToBottom | ToRight,
        ToTopLeft = ToTop | ToLeft, ToTopRight = ToTop | ToRight
    };

    struct NeighborWidgets
    {
        int32_t top = -1, left = -1, right = -1, bottom = -1;
    };

    enum class Layout { Invalid, Horizontal, Vertical, Grid };
    enum FillDirection : int32_t { FD_None = 0, FD_Horizontal = 1, FD_Vertical = 2 };

    constexpr float EXPAND_SZ = FLT_MAX;
    constexpr float FIT_SZ = -1.f;
    constexpr float SHRINK_SZ = -2.f;

    enum class OverflowMode { Clip, Wrap, Scroll };

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
        FontStyleOverflowMarquee = 1 << 8,
        TextIsPlainText = 1 << 9,
        TextIsRichText = 1 << 10,
        TextIsSVG = 1 << 11
    };

    struct LayoutItemDescriptor
    {
        WidgetType wtype = WidgetType::WT_Invalid;
        int32_t id = -1;
        ImRect margin, border, padding, content, text;
        ImVec2 relative;
        int32_t sizing = 0;
        int16_t row = 0, col = 0;
        int16_t from = -1, to = -1;
        bool isComputed = false;
    };

    struct LayoutDescriptor
    {
        Layout type = Layout::Invalid;
        int32_t fill = FD_None;
        int32_t alignment = TextAlignLeading;
        int16_t from = -1, to = -1, itemidx = -1;
        int16_t currow = -1, currcol = -1;
        ImRect geometry{ { FIT_SZ, FIT_SZ }, { FIT_SZ, FIT_SZ } };
        ImVec2 nextpos{ 0.f, 0.f }, prevpos{ 0.f, 0.f };
        ImVec2 spacing{ 0.f, 0.f };
        ImVec2 maxdim{ 0.f, 0.f };
        ImVec2 cumulative{ 0.f, 0.f };
        ImVec2 rows[32];
        ImVec2 cols[32];
        OverflowMode hofmode = OverflowMode::Scroll;
        OverflowMode vofmode = OverflowMode::Scroll;
        ScrollableRegion scroll;
        bool popSizingOnEnd = false;

        Vector<LayoutDescriptor, int16_t, 16> children{ false };
    };

    struct Sizing
    {
        float horizontal = FIT_SZ;
        float vertical = FIT_SZ;
        bool relativeh = false;
        bool relativev = false;
    };

    struct SplitRegion
    {
        float min = 0.f;
        float max = 1.f;
        float initial = 0.5f;
        bool scrollable = true;
    };

    inline int32_t ToTextFlags(TextType type)
    {
        int32_t result = 0;
        switch (type)
        {
        case glimmer::TextType::PlainText: result |= TextIsPlainText; break;
        case glimmer::TextType::RichText: result |= TextIsRichText; break;
        case glimmer::TextType::SVG: result |= TextIsSVG; break;
        default: break;
        }
        return result;
    }
}

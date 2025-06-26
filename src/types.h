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

    [[nodiscard]] inline constexpr uint32_t ToRGBA(int r, int g, int b, int a = 255)
    {
        return (((uint32_t)(a) << 24) |
            ((uint32_t)(b) << 16) |
            ((uint32_t)(g) << 8) |
            ((uint32_t)(r) << 0));
    }

    [[nodiscard]] inline constexpr uint32_t ToRGBA(const std::tuple<int, int, int>& rgb)
    {
        return ToRGBA(std::get<0>(rgb), std::get<1>(rgb), std::get<2>(rgb), 255);
    }

    [[nodiscard]] inline constexpr std::tuple<int, int, int, int> DecomposeColor(uint32_t color)
    {
        return { color & 0xff, (color & 0xff00) >> 8, (color & 0xff0000) >> 16, (color & 0xff000000) >> 24 };
    }

    [[nodiscard]] inline constexpr uint32_t ToRGBA(const std::tuple<int, int, int, int>& rgba)
    {
        return ToRGBA(std::get<0>(rgba), std::get<1>(rgba), std::get<2>(rgba), std::get<3>(rgba));
    }

    [[nodiscard]] inline constexpr uint32_t ToRGBA(float r, float g, float b, float a)
    {
        return ToRGBA((int)(r * 255.f), (int)(g * 255.f), (int)(b * 255.f), (int)(a * 255.f));
    }

    [[nodiscard]] inline constexpr uint32_t DarkenColor(uint32_t rgba, float amount = 2)
    {
        auto [r, g, b, a] = DecomposeColor(rgba);
        r = std::min((int)((float)r / amount), 255); 
        g = std::min((int)((float)g / amount), 255);
        b = std::min((int)((float)b / amount), 255);
        return ToRGBA(r, g, b, a);
    }

    [[nodiscard]] inline constexpr uint32_t LightenColor(uint32_t rgba, float amount = 2)
    {
        auto [r, g, b, a] = DecomposeColor(rgba);
        r = std::min((int)((float)r * amount), 255); 
        g = std::min((int)((float)g * amount), 255);
        b = std::min((int)((float)b * amount), 255);
        return ToRGBA(r, g, b, a);
    }

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

    enum WidgetType
    {
        WT_Invalid = -1,
        WT_Sublayout = -2,
        WT_Label = 0, WT_Button, WT_RadioButton, WT_ToggleButton, WT_Checkbox,
        WT_Layout, WT_Scrollable, WT_Splitter, WT_SplitterRegion, WT_Accordion,
        WT_Slider, WT_Spinner,
        WT_TextInput,
        WT_DropDown,
        WT_TabBar,
        WT_ItemGrid,
        WT_Charts,
        WT_TotalTypes
    };

    struct UIConfig
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
        float sliderSize = 20.f;
        float minScrollGripSz = 20.f;
        ImVec2 toggleButtonSz{ 100.f, 40.f };
        std::string_view tooltipFontFamily = IM_RICHTEXT_DEFAULT_FONTFAMILY;
        BoxShadowQuality shadowQuality = BoxShadowQuality::Balanced;
        LayoutPolicy layoutPolicy = LayoutPolicy::ImmediateMode;
        IRenderer* renderer = nullptr;
        IPlatform* platform = nullptr;
        int32_t(*GetTotalWidgetCount)(WidgetType) = nullptr;
        void* userData = nullptr;
    };

    inline bool IsColorVisible(uint32_t color)
    {
        return (color & 0xFF000000) != 0;
    }

    enum WidgetState : int32_t
    {
        WS_Default = 1,
        WS_Focused = 1 << 1,
        WS_Hovered = 1 << 2,
        WS_Pressed = 1 << 3,
        WS_Checked = 1 << 4,
        WS_PartialCheck = 1 << 5,
        WS_Selected = 1 << 6,
        WS_Dragged = 1 << 7,
        WS_Disabled = 1 << 8,
        WS_AllStates = WS_Default | WS_Focused | WS_Hovered | WS_Pressed | WS_Checked | 
            WS_PartialCheck | WS_Selected | WS_Dragged | WS_Disabled
    };

    enum class TextType { PlainText, RichText, SVG, ImagePath, SVGPath };

    struct CommonWidgetData
    {
        int32_t state = WS_Default;
        int32_t id = -1;
        std::string_view tooltip = "";
        float _hoverDuration = 0; // for tooltip, in seconds
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
        bool* out = nullptr;
    };

    using RadioButtonState = ToggleButtonState;

    enum class CheckState 
    {
        Checked, Unchecked, Partial
    };

    struct CheckboxState : public CommonWidgetData
    {
        CheckState check = CheckState::Unchecked;
        CheckState* out = nullptr;
    };

    enum class SpinnerButtonPlacement { VerticalLeft, VerticalRight, EitherSide };

    enum class OutPtrType
    {
        Invalid, i32, f32, f64
    };

    struct SpinnerState : public CommonWidgetData
    {
        float data = 0.f;
        float min = 0.f, max = (float)INT_MAX, delta = 1.f;
        SpinnerButtonPlacement placement = SpinnerButtonPlacement::VerticalRight;
        int precision = 3;
        float repeatRate = 0.5; // in seconds
        float repeatTrigger = 1.f; // in seconds
        bool isInteger = true;
        void* out = nullptr;
        OutPtrType outType = OutPtrType::Invalid;
    };

    struct SliderState : public CommonWidgetData
    {
        float data = 0.f;
        float min = 0.f, max = FLT_MAX, delta = 1.f;
        uint32_t(*TrackColor)(float) = nullptr; // Use this to color the track based on value
        Direction dir = DIR_Horizontal;
        void* out = nullptr;
        OutPtrType outType = OutPtrType::Invalid;
    };

    enum ScrollType : int32_t
    {
        ST_Horizontal = 1,
        ST_Vertical = 2,
        ST_Always_H = 4,
        ST_Always_V = 8,
        ST_NoMouseWheel_V = 16,
        ST_ShowScrollBarInsideViewport = 32
    };

    struct ScrollBarState
    {
        ImVec2 pos;
        ImVec2 lastMousePos;
        ImVec2 opacity;
        bool mouseDownOnVGrip = false;
        bool mouseDownOnHGrip = false;
    };

    struct ScrollableRegion
    {
        //std::pair<bool, bool> enabled; // enable scroll in horizontal and vertical direction respectively
        int32_t type = ST_ShowScrollBarInsideViewport; // scroll bar properties
        ImRect viewport{ { -1.f, -1.f }, {} }; // visible region of content
        ImVec2 content; // total occupied size of the widgets inside region
        ImVec2 extent{ FLT_MAX, FLT_MAX }; // total available space inside the scroll region, default is infinite if scroll enabled
        ScrollBarState state;
    };

    struct TextInputState : public CommonWidgetData
    {
        std::vector<char> text;
        Span<char> out;
        std::string_view placeholder;
        std::string_view prefix, suffix;
        std::pair<int, int> selection{ -1, -1 };
        void (*ShowList)(const TextInputState&, ImVec2, ImVec2) = nullptr;
        float overlayHeight = FLT_MAX;
    };

    struct DropDownState : public CommonWidgetData
    {
        std::string_view text;
        Direction dir;
        TextInputState input;
        int32_t inputId = -1;
        int32_t selected = -1;
        int32_t* out = nullptr;
        std::span<std::pair<WidgetType, std::string_view>> options;
        bool isComboBox = false;
        bool opened = false;
        bool (*ShowList)(int32_t, ImVec2, ImVec2, DropDownState&) = nullptr;
    };

    enum TabItemProperty
    {
        TI_Closeable = 1, 
        TI_Pinnable = 2,
        TI_Active = 4,
        TI_AddNewTab = 8,
        TI_AnchoredToEnd = 16
    };

    enum TabItemState
    {
        TI_Pinned = 1, TI_Disabled = 2
    };

    enum class TabBarItemSizing
    {
        Scrollable, ResizeToFit, MultiRow, DropDown
    };

    struct TabBarState
    {
        TabBarItemSizing sizing;
        ImVec2 spacing;
        Direction direction = DIR_Horizontal;
        float btnspacing = 5.f;
        float btnsize = 0.75f; // 75% of tab text height
        int selected = -1;
        bool expandTabs = false;
        bool circularButtons = true;
        bool createNewTabs = false;
        bool showExpanded = false; // pop out drawer for vertical tabs
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
        COL_Moveable = 1 << 7,
        COL_SortOnlyAscending = 1 << 8,
        COL_SortOnlyDescending = 1 << 9,
        COL_InitialSortedAscending = 1 << 10,
        COL_InitialSortedDescending = 1 << 11
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

    enum class ItemDescendentVisualState
    {
        NoDescendent, Collapsed, Expanded
    };

    struct ItemGridItemProps
    {
        int16_t rowsdpan = 1, colspan = 1;
        int16_t children = 0;
        ItemDescendentVisualState vstate = ItemDescendentVisualState::NoDescendent;
        int32_t alignment = TextAlignCenter;
        uint32_t highlightBgColor = ToRGBA(186, 244, 250);
        uint32_t highlightFgColor = ToRGBA(0, 0, 0);
        uint32_t selectionBgColor = ToRGBA(0, 0, 120);
        uint32_t selectionFgColor = ToRGBA(255, 255, 255);
        bool highlightCell = false;
        bool selectCell = false;
        bool wrapText = false;
        bool isContentWidget = false;
        bool disabled = false;
    };

    enum class WidgetEvent
    {
        None, Focused, Clicked, Hovered, Pressed, DoubleClicked, RightClicked, 
        Dragged, Edited, Selected, Scrolled
    };

    struct WidgetDrawResult
    {
        int32_t id = -1;
        WidgetEvent event = WidgetEvent::None;
        int32_t row = -1;
        int16_t col = -1;
        int16_t depth = -1;
        int16_t tabclosed = -1;
        int16_t tabidx = -1;
        ImRect geometry, content;
        float wheel = 0.f;
        bool newTab = false;
        bool order = false;
    };

    enum class ItemGridPopulateMethod
    {
        ByRows, ByColumns
    };

    enum ItemGridHighlightType
    {
        IG_HighlightRows = 1, 
        IG_HighlightColumns = 2,
        IG_HighlightCell = 4
    };

    enum ItemGridSelectionType
    {
        IG_SelectCell = 1,
        IG_SelectRow = 2,
        IG_SelectColumn = 4,
        IG_SelectSingleItem = 8,
        IG_SelectContiguosItem = 16,
        IG_SelectMultiItem = 32
    };

    enum ItemGridNodeProperty
    {
        IG_Selected = 1, IG_Highlighted = 2
    };

    struct ItemGridConfig : public CommonWidgetData
    {
        struct ColumnConfig
        {
            ImRect extent;
            ImRect content;
            std::string_view name;
            int32_t props = COL_Resizable;
            int16_t width = 0;
            int16_t parent = -1;
        };

        struct Configuration
        {
            std::vector<std::vector<ColumnConfig>> headers;
            int32_t rows = 0;
            float indent = 10.f;
        } config;

        ImVec2 cellpadding{ 2.f, 2.f };
        float gridwidth = 1.f;
        uint32_t gridcolor = ToRGBA(100, 100, 100);
        uint32_t highlightBgColor = ToRGBA(186, 244, 250);
        uint32_t highlightFgColor = ToRGBA(0, 0, 0);
        uint32_t selectionBgColor = ToRGBA(0, 0, 120);
        uint32_t selectionFgColor = ToRGBA(255, 255, 255);
        
        int16_t sortedcol = -1;
        int16_t coldrag = -1;
        int16_t frozencols = 0;
        int32_t highlights = 0;
        int32_t selection = 0;
        int32_t scrollprops = ST_Always_H | ST_Always_V;
        ItemGridPopulateMethod populateMethod = ItemGridPopulateMethod::ByRows;
        bool uniformRowHeights = false;
        bool isTree = false;

        ItemGridItemProps (*cellprops)(int32_t, int16_t, int16_t, int32_t, int32_t) = nullptr;
        void (*cellwidget)(std::pair<float, float>, int32_t, int16_t, int16_t) = nullptr;
        std::string_view (*cellcontent)(std::pair<float, float>, int32_t, int16_t, int16_t) = nullptr;
        void (*header)(ImVec2, float, int16_t, int16_t, int16_t) = nullptr;

        void setColumnResizable(int16_t col, bool resizable);
        void setColumnProps(int16_t col, ColumnProperty prop, bool set = true);
    };

    struct WidgetConfigData
    {
        WidgetType type;

        union SharedWidgetState {
            LabelState label;
            ButtonState button;
            ToggleButtonState toggle;
            RadioButtonState radio;
            CheckboxState checkbox;
            SpinnerState spinner;
            SliderState slider;
            TextInputState input;
            DropDownState dropdown;
            TabBarState tab;
            ItemGridConfig grid;
            ScrollableRegion scroll;
            // Add others...

            SharedWidgetState() {}
            ~SharedWidgetState() {}
        } state;

        CommonWidgetData data;

        WidgetConfigData(WidgetType type);
        WidgetConfigData(const WidgetConfigData& src);
        WidgetConfigData& operator=(const WidgetConfigData& src);
        ~WidgetConfigData();
    };

    enum WidgetGeometry : int32_t
    {
        ExpandH = 2, ExpandV = 4, ExpandAll = ExpandH | ExpandV,
        ToLeft = 8, ToRight = 16, ToBottom = 32, ToTop = 64,
        ShrinkH = 128, ShrinkV = 256, ShrinkAll = ShrinkH | ShrinkV,
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

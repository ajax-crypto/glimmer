#pragma once

#include "types.h"
#include "style.h"

#include <bit>

#ifndef GLIMMER_MAX_SPLITTER_REGIONS
#define GLIMMER_MAX_SPLITTER_REGIONS 8
#endif

#ifndef GLLIMMER_MAX_STYLE_STACKSZ
#define GLLIMMER_MAX_STYLE_STACKSZ 16
#endif

#ifndef GLIMMER_MAX_WIDGET_SPECIFIC_STYLES
#define GLIMMER_MAX_WIDGET_SPECIFIC_STYLES 4
#endif

namespace glimmer
{
    // =============================================================================================
    // STATIC DATA
    // =============================================================================================

    inline WindowConfig Config{};

    struct AnimationData
    {
        int32_t elements = 0;
        int32_t types = 0;
        ImGuiDir direction = ImGuiDir::ImGuiDir_Right;
        float offset = 0.f;
        float timestamp = 0;

        void moveByPixel(float amount, float max, float reset);
    };

    constexpr int AnimationsPreallocSz = 32;
    constexpr int ShadowsPreallocSz = 64;
    constexpr int FontStylePreallocSz = 64;
    constexpr int BorderPreallocSz = 64;
    constexpr int GradientPreallocSz = 64;

    inline int log2(auto i) { return i <= 0 ? 0 : 8 * sizeof(i) - std::countl_zero(i) - 1; }

    struct ItemGridStyleDescriptor
    {
        uint32_t gridcolor = IM_COL32(100, 100, 100, 255);
    };

#ifndef GLIMMER_MAX_LAYOUT_NESTING 
#define GLIMMER_MAX_LAYOUT_NESTING 8
#endif

    enum WidgetStateIndex
    {
        WSI_Default, WSI_Focused, WSI_Hovered, WSI_Pressed, WSI_Checked,
        WSI_PartiallyChecked, WSI_Selected, WSI_Dragged, WSI_Disabled,
        WSI_Total
    };

    enum class ItemGridCurrentState
    {
        Default, ResizingColumns, ReorderingColumns
    };

    struct ItemGridInternalState
    {
        struct HeaderCellResizeState
        {
            ImVec2 lastPos; // Last mouse position while dragging
            float modified = 0.f; // Records already modified column width
            bool mouseDown = false; // If mouse is down on column boundary
        };

        struct HeaderCellDragState
        {
            ItemGridState::ColumnConfig config;
            ImVec2 lastPos;
            ImVec2 startPos;
            int16_t column = -1;
            int16_t level = -1;
            bool mouseDown = false;
        };

        struct BiDirMap
        {
            Vector<int16_t, int16_t> ltov{ 128, -1 };
            Vector<int16_t, int16_t> vtol{ 128, -1 };
        };

        Vector<HeaderCellResizeState, int16_t> cols[8];
        BiDirMap colmap[8];
        HeaderCellDragState drag;
        ScrollBarState scroll;
        ImVec2 totalsz;
        ItemGridCurrentState state = ItemGridCurrentState::Default;

        void swapColumns(int16_t from, int16_t to, std::vector<std::vector<ItemGridState::ColumnConfig>>& headers, int level);
    };

    struct ToggleButtonInternalState
    {
        float btnpos = -1.f;
        float progress = 0.f;
        bool animate = false;
    };

    struct RadioButtonInternalState
    {
        float radius = -1.f;
        float progress = 0.f;
        bool animate = false;
    };

    struct CheckboxInternalState
    {
        float progress = -1.f;
        bool animate = false;
    };

    enum class TextOpType { Addition, Deletion, Replacement };

    struct TextInputOperation
    {
        std::pair<int32_t, int32_t> range{ -1, -1 };
        int32_t caretpos = 0;
        char opmem[128] = { 0 };
        TextOpType type;
    };

    struct InputTextInternalState
    {
        int caretpos = 0;
        bool caretVisible = true;
        bool isSelecting = false;
        float lastCaretShowTime = 0.f;
        float selectionStart = -1.f;
        float lastClickTime = -1.f;
        ScrollBarState scroll;
        Vector<float, int16_t> pixelpos; // Cumulative pixel position of characters
        UndoRedoStack<TextInputOperation> ops; // Text operations for redo/undo stack
        TextInputOperation currops;

        void moveLeft(float amount)
        {
            scroll.pos.x = std::max(0.f, scroll.pos.x - amount);
        }

        void moveRight(float amount)
        {
            scroll.pos.x = std::min(scroll.pos.x + amount, pixelpos.back());
        }
    };

    struct AdHocLayoutState
    {
        ImVec2 nextpos{ 0.f, 0.f }; // position of next widget
        int32_t lastItemId = -1; // last inserted item's ID
    };

    struct SplitterContainerState
    {
        ImRect extent;
        int32_t id;
        Direction dir = DIR_Vertical;
        bool isScrollable = false;
    };

    struct SplitterInternalState
    {
        struct SplitRange
        {
            float min, max;
            float curr = -1.f;
        };

        int current = 0;
        SplitRange spacing[GLIMMER_MAX_SPLITTER_REGIONS]; // spacing from (i-1)th to ith splitter
        int32_t states[GLIMMER_MAX_SPLITTER_REGIONS]; // ith splitter's state
        int32_t scrollids[GLIMMER_MAX_SPLITTER_REGIONS]; // id of ith scroll data
        ScrollableRegion scrolldata[GLIMMER_MAX_SPLITTER_REGIONS]; // ith scroll region data
        ImRect viewport[GLIMMER_MAX_SPLITTER_REGIONS]; // ith non-scroll region geometry
        bool isdragged[GLIMMER_MAX_SPLITTER_REGIONS]; // ith drag state

        SplitterInternalState();
    };

    struct SpinnerInternalState
    {
        float lastChangeTime = 0.f;
        float repeatRate = 0.f;
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
        bool closing = false;
        bool hscroll = false, vscroll = false;
    };

    enum class LayoutOps { PushStyle, PopStyle, SetStyle, AddWidget };

    using StyleStackT = DynamicStack<StyleDescriptor, int16_t, GLLIMMER_MAX_STYLE_STACKSZ>;

    struct TabItemDescriptor
    {
        std::string_view name, tooltip;
        int32_t itemflags = 0;
    };

    struct TabBarDescriptor
    {
        int32_t id;
        int32_t geometry;
        TabBarItemSizing sizing = TabBarItemSizing::ShrinkToFit;
        NeighborWidgets neighbors;
        Vector<TabItemDescriptor, int16_t, 8> items;
        bool newTabButton = false;

        TabBarDescriptor() {}
    };

    struct LayoutDescriptor
    {
        Layout type = Layout::Invalid;
        int32_t id = 0;
        int32_t fill = FD_None;
        int32_t alignment = TextAlignLeading;
        int16_t from = -1, to = -1, itemidx = -1;
        int16_t styleStartIdx[WSI_Total];
        int16_t currow = -1, currcol = -1;
        int32_t currStyleStates = 0;
        ImRect geometry{ { FIT_SZ, FIT_SZ }, { FIT_SZ, FIT_SZ } };
        ImVec2 nextpos{ 0.f, 0.f }, prevpos{ 0.f, 0.f };
        ImVec2 spacing{ 0.f, 0.f };
        ImVec2 maxdim{ 0.f, 0.f };
        ImVec2 cumulative{ 0.f, 0.f };
        Vector<ImVec2, int16_t> rows;
        Vector<ImVec2, int16_t> cols;
        OverflowMode hofmode = OverflowMode::Scroll;
        OverflowMode vofmode = OverflowMode::Scroll;
        ScrollableRegion scroll;
        bool popSizingOnEnd = false;

        Vector<LayoutDescriptor, int16_t, 16> children{ false };
        Vector<std::pair<int64_t, LayoutOps>, int16_t> itemIndexes;
        StyleStackT styles[WSI_Total]{ false, false, false, false,
            false, false, false, false, false };
        FixedSizeStack<int32_t, 16> containerStack;
        Vector<TabBarDescriptor, int16_t, 8> tabbars{ false };
        StyleDescriptor currstyle[WSI_Total];

        LayoutDescriptor() 
        {
            for (auto idx = 0; idx < WSI_Total; ++idx) styleStartIdx[idx] = -1;
        }

        void reset();
    };

    struct TabBarInternalState
    {
        struct ItemDescriptor
        {
            int16_t state = 0;
            ImRect extent, close, pin, text;
            bool pinned = false;
        };

        int16_t current = -1;
        int16_t hovered = -1;
        Vector<ItemDescriptor, int16_t, 16> tabs;
        ImRect create;
        ScrollableRegion scroll;
    };

    ImVec2 AddItemToLayout(LayoutDescriptor& layout, LayoutItemDescriptor& item);

    // Determine extent of layout/splitter/other containers
    void AddExtent(LayoutItemDescriptor& wdesc, const StyleDescriptor& style, const NeighborWidgets& neighbors,
        float width = 0.f, float height = 0.f);

    void AddFontPtr(FontStyle& font);
    void ResetActivePopUps(ImVec2 mousepos, bool escape);
    void InitFrameData();
    void ResetFrameData();

    struct EventDeferInfo
    {
        WidgetType type; 
        int32_t id;

        ImRect margin;
        ImRect border;
        ImRect padding;
        ImRect content;
        ImRect text;

        ImRect extent;
        ImRect decbtn, incbtn;
        ImVec2 center;
        float maxrad;

        EventDeferInfo(WidgetType type_, int32_t id_, const ImRect& m, const ImRect& b, const ImRect& p, 
            const ImRect& c, const ImRect& t)
            : type{type_}, id{id_}, margin{m}, border{b}, padding{p}, content{c}, text{t} {}

        EventDeferInfo(WidgetType type_, int32_t id_, const ImRect& m, const ImRect& b, const ImRect& p,
            const ImRect& c)
            : type{ type_ }, id{ id_ }, margin{ m }, border{ b }, padding{ p }, content{ c } {}

        EventDeferInfo(WidgetType type_, int32_t id_, const ImRect& e, float m)
            : type{ type_ }, id{ id_ }, extent{ e }, maxrad{ m } {}

        EventDeferInfo(WidgetType type_, int32_t id_, ImVec2 c, const ImRect& e)
            : type{ type_ }, id{ id_ }, extent{ e }, center{ c } {}

        EventDeferInfo(WidgetType type_, int32_t id_, const ImRect& e)
            : type{ type_ }, id{ id_ }, extent{ e } {}

        EventDeferInfo(WidgetType type_, int32_t id_, const ImRect& p, const ImRect& c)
            : type{ type_ }, id{ id_ }, padding{ p }, content{ c } {}

        EventDeferInfo(WidgetType type_, int32_t id_, const ImRect& e, const ImRect& inc, const ImRect& dec)
            : type{ type_ }, id{ id_ }, extent{ e }, incbtn{ inc }, decbtn{ dec } {}
    };

    // Captures widget states, is stored as a linked-list, each context representing
    // a window or overlay, this enables serialized Id's for nested overlays as well
    struct WidgetContextData
    {
        // This is quasi-persistent
        std::vector<WidgetStateData> states[WT_TotalTypes];
        std::vector<ItemGridInternalState> gridStates;
        std::vector<TabBarInternalState> tabStates;
        std::vector<ToggleButtonInternalState> toggleStates;
        std::vector<RadioButtonInternalState> radioStates;
        std::vector<CheckboxInternalState> checkboxStates;
        std::vector<InputTextInternalState> inputTextStates;
        std::vector<SplitterInternalState> splitterStates;
        std::vector<SpinnerInternalState> spinnerStates;
        std::vector<TabBarInternalState> tabBarStates;
        std::vector<int32_t> splitterScrollPaneParentIds;
        std::vector<std::vector<std::pair<int32_t, int32_t>>> dropDownOptions;
        
        // Tab bars are not nested
        TabBarDescriptor currentTab;

        std::vector<WidgetContextData*> nestedContexts[WT_TotalTypes];
        WidgetContextData* parentContext = nullptr;

        // Styling data is static as it is persisted across contexts
        static StyleStackT StyleStack[WSI_Total];
        static StyleDescriptor currStyle[WSI_Total];
        static int32_t currStyleStates;

        // Per widget specific style objects
        static DynamicStack<ToggleButtonStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES> toggleButtonStyles[WSI_Total];
        static DynamicStack<RadioButtonStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES> radioButtonStyles[WSI_Total];
        static DynamicStack<SliderStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES> sliderStyles[WSI_Total];
        static DynamicStack<SpinnerStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES> spinnerStyles[WSI_Total];
        static DynamicStack<TabBarStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES> tabBarStyles[WSI_Total];

        // This has to persistent
        std::vector<AnimationData> animations{ AnimationsPreallocSz, AnimationData{} };

        // Layout related members
        Vector<LayoutItemDescriptor, int16_t> layoutItems{ 128 };
        Vector<ImRect, int16_t> itemGeometries[WT_TotalTypes]{
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ false },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ false },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ true } };
        DynamicStack<int32_t, int16_t> containerStack{ 16 };
        FixedSizeStack<SplitterContainerState, 16> splitterStack;
        FixedSizeStack<LayoutDescriptor, GLIMMER_MAX_LAYOUT_NESTING> layouts;
        FixedSizeStack<Sizing, GLIMMER_MAX_LAYOUT_NESTING> sizing;
        FixedSizeStack<int32_t, GLIMMER_MAX_LAYOUT_NESTING> spans;
        DynamicStack<AdHocLayoutState, int16_t, 4> adhocLayout;

        // Keep track of widget IDs
        int maxids[WT_TotalTypes];
        int tempids[WT_TotalTypes];

        // Whether we are in a frame being rendered + current renderer
        bool InsideFrame = false;
        bool usingDeferred = false;
        bool deferEvents = false;
        Vector<EventDeferInfo, int16_t> deferedEvents;
        IRenderer* deferedRenderer = nullptr;

        ImVec2 popupOrigin;
        int32_t popupTarget = -1;
        ImRect activePopUpRegion;

        WidgetStateData& GetState(int32_t id)
        {
            auto index = id & 0xffff;
            auto wtype = (WidgetType)(id >> 16);
            return states[wtype][index];
        }

        ItemGridInternalState& GridState(int32_t id)
        {
            auto index = id & 0xffff;
            return gridStates[index];
        }

        TabBarInternalState& TabStates(int32_t id)
        {
            auto index = id & 0xffff;
            return tabStates[index];
        }

        ToggleButtonInternalState& ToggleState(int32_t id)
        {
            auto index = id & 0xffff;
            return toggleStates[index];
        }

        RadioButtonInternalState& RadioState(int32_t id)
        {
            auto index = id & 0xffff;
            return radioStates[index];
        }

        CheckboxInternalState& CheckboxState(int32_t id)
        {
            auto index = id & 0xffff;
            return checkboxStates[index];
        }

        InputTextInternalState& InputTextState(int32_t id)
        {
            auto index = id & 0xffff;
            return inputTextStates[index];
        }

        SplitterInternalState& SplitterState(int32_t id)
        {
            auto index = id & 0xffff;
            return splitterStates[index];
        }

        SpinnerInternalState& SpinnerState(int32_t id)
        {
            auto index = id & 0xffff;
            return spinnerStates[index];
        }

        TabBarInternalState& TabBarState(int32_t id)
        {
            auto index = id & 0xffff;
            return tabBarStates[index];
        }

        ScrollableRegion& ScrollRegion(int32_t id)
        {
            auto index = id & 0xffff;
            auto type = id >> 16;
            return states[type][index].state.scroll;
        }

        StyleDescriptor& GetStyle(int32_t state);

        void ToggleDeferedRendering(bool defer);
        void PushContainer(int32_t parentId, int32_t id);
        void PopContainer(int32_t id);
        void AddItemGeometry(int id, const ImRect& geometry, bool ignoreParent = false);
        WidgetDrawResult HandleEvents(ImVec2 origin);

        void ResetLayoutData();

        const ImRect& GetGeometry(int32_t id) const;
        float MaximumExtent(Direction dir) const;
        ImVec2 MaximumExtent() const;
        ImVec2 MaximumAbsExtent() const;
        ImVec2 WindowSize() const;
        ImVec2 NextAdHocPos() const;

        WidgetContextData();
    };

    WidgetContextData& GetContext();
    WidgetContextData& PushContext(int32_t id);
    void PopContext();
}

#pragma once

#include "types.h"
#include "style.h"

#include <bit>

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
        long long timestamp = 0;

        void moveByPixel(float amount, float max, float reset);
    };

    constexpr int AnimationsPreallocSz = 32;
    constexpr int ShadowsPreallocSz = 64;
    constexpr int FontStylePreallocSz = 64;
    constexpr int BorderPreallocSz = 64;
    constexpr int GradientPreallocSz = 64;

    inline int log2(auto i) { return i <= 0 ? 0 : 8 * sizeof(i) - std::countl_zero(i) - 1; }

    struct ElementSpan
    {
        int32_t direction = 0;
        bool popWhenUsed = false;
    };

    struct ItemGridStyleDescriptor
    {
        uint32_t gridcolor = IM_COL32(100, 100, 100, 255);
    };

    struct ScrollBarState
    {
        ImVec2 pos;
        ImVec2 lastMousePos;
        float opacity = 0.f;
        bool mouseDownOnVGrip = false;
        bool mouseDownOnHGrip = false;
    };

#ifndef GLIMMER_MAX_LAYOUT_NESTING 
#define GLIMMER_MAX_LAYOUT_NESTING 8
#endif

    enum WidgetStateIndex
    {
        WSI_Default, WSI_Focused, WSI_Hovered, WSI_Pressed, WSI_Checked, WSI_Disabled,
        WSI_PartiallyChecked, WSI_Selected,
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

    struct TabBarInternalState
    {
        struct BiDirMap
        {
            Vector<int16_t, int16_t> ltov{ 128, -1 };
            Vector<int16_t, int16_t> vtol{ 128, -1 };
        };

        BiDirMap map;
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
        bool capslock = false;
        bool replace = false;
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

    inline std::vector<std::pair<char, char>> KeyMappings;

    struct AdHocLayoutState
    {
        ImVec2 nextpos{ 0.f, 0.f };
        int32_t lastItemId = -1;
    };

    struct WidgetContextData
    {
        // This is quasi-persistent
        std::vector<WidgetStateData> states[WT_TotalTypes];
        std::vector<ItemGridInternalState> gridStates;
        std::vector<TabBarInternalState> tabStates;
        std::vector<ToggleButtonInternalState> toggleStates;
        std::vector<RadioButtonInternalState> radioStates;
        std::vector<InputTextInternalState> inputTextStates;

        DynamicStack<StyleDescriptor, int16_t> pushedStyles[WSI_Total];
        StyleDescriptor currStyle[WSI_Total];
        int32_t currStyleStates = 0;

        Vector<LayoutItemDescriptor, int16_t> layoutItems{ 128 };
        Vector<ImRect, int16_t> itemGeometries[WT_TotalTypes];

        // This has to persistent
        std::vector<AnimationData> animations{ AnimationsPreallocSz, AnimationData{} };

        // Per widget specific style objects
        DynamicStack<ToggleButtonStyleDescriptor, int16_t> toggleButtonStyles[WSI_Total];
        DynamicStack<RadioButtonStyleDescriptor, int16_t>  radioButtonStyles[WSI_Total];

        // Keep track of widget IDs
        int maxids[WT_TotalTypes];
        int tempids[WT_TotalTypes];

        // Whether we are in a frame being rendered
        bool InsideFrame = false;

        int32_t currLayoutDepth = -1;
        LayoutDescriptor layouts[GLIMMER_MAX_LAYOUT_NESTING];

        int32_t currSizingDepth = 0;
        Sizing sizing[GLIMMER_MAX_LAYOUT_NESTING];

        int32_t currSpanDepth = -1;
        ElementSpan spans[GLIMMER_MAX_LAYOUT_NESTING];
        
        DynamicStack<AdHocLayoutState, int16_t, 4> adhocLayout;

        bool usingDeferred = false;
        IRenderer* deferedRenderer = nullptr;

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

        InputTextInternalState& InputTextState(int32_t id)
        {
            auto index = id & 0xffff;
            return inputTextStates[index];
        }

        StyleDescriptor& GetStyle(int32_t state);

        void ToggleDeferedRendering(bool defer);

        void AddItemGeometry(int id, const ImRect& geometry);
        const ImRect& GetGeometry(int32_t id) const;

        WidgetContextData();
    };

    inline WidgetContextData Context{};
}

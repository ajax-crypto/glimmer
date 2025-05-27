#pragma once
#include "types.h"

namespace glimmer
{
    UIConfig& GetUIConfig();

    int32_t GetNextId(WidgetType type);
    int16_t GetNextCount(WidgetType type);

    WidgetStateData& GetWidgetState(WidgetType type, int16_t id);
    WidgetStateData& GetWidgetState(int32_t id);

    WidgetDrawResult Label(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult Button(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult ToggleButton(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult RadioButton(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult Checkbox(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult Spinner(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult Slider(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult TextInput(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult DropDown(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult ItemGrid(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});

    void StartSplitRegion(int32_t id, Direction dir, const std::initializer_list<SplitRegion>& splits,
        int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    void NextSplitRegion();
    void EndSplitRegion();

    bool StartPopUp(int32_t id, ImVec2 origin);
    WidgetDrawResult EndPopUp();

    void StartScrollableRegion(int32_t id, bool hscroll, bool vscroll, int32_t geometry = ToBottomRight, 
        const NeighborWidgets& neighbors = NeighborWidgets{});
    void EndScrollableRegion();

    bool StartTabBar(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    void AddTab(std::string_view name, std::string_view tooltip = "", int32_t flags = 0);
    WidgetDrawResult EndTabBar(bool canAddTab);

    bool StartItemGrid();
    bool StartItemGridHeader();
    void AddHeader();
    WidgetDrawResult EndItemGridHeader();
    void AddColumn();
    void AddRow();
    WidgetDrawResult EndItemGrid();
}

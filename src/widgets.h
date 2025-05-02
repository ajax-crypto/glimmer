#pragma once
#include "types.h"

namespace glimmer
{
    WindowConfig& GetWindowConfig();

    void BeginFrame();
    void EndFrame();
    int32_t GetNextId(WidgetType type);
    int16_t GetNextCount(WidgetType type);

    WidgetStateData& GetWidgetState(WidgetType type, int16_t id);
    WidgetStateData& GetWidgetState(int32_t id);

    WidgetDrawResult Label(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult Button(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult ToggleButton(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult RadioButton(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult CheckBox(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult TextInput(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult DropDown(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult TabBar(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult ItemGrid(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
}

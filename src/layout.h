#pragma once

#include "types.h"

namespace glimmer
{
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

    ImVec2 AddItemToLayout(LayoutDescriptor& layout, LayoutItemDescriptor& item);
}

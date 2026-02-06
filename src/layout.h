#pragma once

#include "types.h"

namespace glimmer
{
    void Move(int32_t direction); // Combination of Direction enum values
    void Move(int32_t id, int32_t direction); // Combination of Direction enum values
    void Move(std::string_view id, int32_t direction); // Combination of Direction enum values
    void Move(int32_t hid, int32_t vid, bool toRight, bool toBottom); // Move w.r.t different widgets in vertical/horizontal direction
    void Move(std::string_view hid, std::string_view vid, bool toRight, bool toBottom); // Move w.r.t different widgets in vertical/horizontal direction
    void Move(ImVec2 amount, int32_t direction); // Combination of WidgetGeometry
    void Move(ImVec2 pos);
    void AddSpacing(ImVec2 spacing);

    void MoveDown();
    void MoveUp();
    void MoveLeft();
    void MoveRight();

    void MoveBelow(int32_t id);
    void MoveAbove(int32_t id);
    void MoveRight(int32_t id);
    void MoveLeft(int32_t id);

    void MoveBelow(std::string_view id);
    void MoveAbove(std::string_view id);
    void MoveRight(std::string_view id);
    void MoveLeft(std::string_view id);

    // Structured Layout inside container
    ImRect BeginFlexLayout(Direction dir, int32_t geometry, bool wrap = false,
        ImVec2 spacing = { 0.f, 0.f }, ImVec2 size = { 0.f, 0.f }, const NeighborWidgets& neighbors = NeighborWidgets{});
    ImRect BeginGridLayout(int rows, int cols, GridLayoutDirection dir, int32_t geometry, const std::initializer_list<float>& rowExtents = {},
        const std::initializer_list<float>& colExtents = {}, ImVec2 spacing = { 0.f, 0.f }, ImVec2 size = { 0.f, 0.f },
        const NeighborWidgets& neighbors = NeighborWidgets{});
    ImRect BeginLayout(std::string_view desc, const NeighborWidgets& neighbors = NeighborWidgets{});
    void NextRow();
    void NextColumn();
    WidgetDrawResult EndLayout(int depth = 1);

    void CacheLayout();
    void InvalidateLayout();
    void ContextPushed(void* data);
    void ContextPopped();
}

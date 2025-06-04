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
    WidgetDrawResult StaticItemGrid(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});

    void StartSplitRegion(int32_t id, Direction dir, const std::initializer_list<SplitRegion>& splits,
        int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    void NextSplitRegion();
    void EndSplitRegion();

    bool StartPopUp(int32_t id, ImVec2 origin);
    WidgetDrawResult EndPopUp();

    void StartScrollableRegion(int32_t id, bool hscroll, bool vscroll, int32_t geometry = ToBottomRight, 
        const NeighborWidgets& neighbors = NeighborWidgets{}, ImVec2 maxsz = { -1.f, -1.f });
    void EndScrollableRegion();

    bool StartTabBar(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    void AddTab(std::string_view name, std::string_view tooltip = "", int32_t flags = 0);
    WidgetDrawResult EndTabBar(bool canAddTab);

    bool StartAccordion(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    bool StartAccordionHeader();
    void AddAccordionHeaderExpandedIcon(std::string_view res, bool svgOrImage, bool isPath);
    void AddAccordionHeaderCollapsedIcon(std::string_view res, bool svgOrImage, bool isPath);
    void AddAccordionHeaderText(std::string_view content, bool isRichText);
    void EndAccordionHeader(std::optional<bool> expanded);
    bool StartAccordionContent(bool hscroll, bool vscroll, ImVec2 maxsz = {});
    void EndAccordionContent();
    WidgetDrawResult EndAccordion();

    bool StartItemGrid(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    bool StartItemGridHeader(int levels = 1);
    void AddHeaderColumn(const ItemGridState::ColumnConfig& config);
    void CategorizeColumns();
    void AddColumnCategory(const ItemGridState::ColumnConfig& config, int16_t from, int16_t to);
    WidgetDrawResult EndItemGridHeader();
    void PopulateItemGrid(bool byRows);
    WidgetDrawResult EndItemGrid(int totalRows);
}

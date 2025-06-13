#pragma once
#include "types.h"

namespace glimmer
{
    UIConfig& GetUIConfig();

    int32_t GetNextId(WidgetType type);
    int16_t GetNextCount(WidgetType type);

    WidgetConfigData& GetWidgetConfig(WidgetType type, int16_t id);
    WidgetConfigData& GetWidgetConfig(int32_t id);

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

    bool StartPopUp(int32_t id, ImVec2 origin, ImVec2 size = { FLT_MAX, FLT_MAX });
    WidgetDrawResult EndPopUp();

    void StartScrollableRegion(int32_t id, int32_t flags, int32_t geometry = ToBottomRight, 
        const NeighborWidgets& neighbors = NeighborWidgets{}, ImVec2 maxsz = { FLT_MAX, FLT_MAX });
    ImRect EndScrollableRegion();

    bool StartTabBar(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    void AddTab(std::string_view name, std::string_view tooltip = "", int32_t flags = 0);
    void AddTab(TextType type, std::string_view name, TextType extype, std::string_view expanded, int32_t flags = 0);
    WidgetDrawResult EndTabBar(std::optional<bool> canAddTab = std::nullopt);

    bool StartAccordion(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    bool StartAccordionHeader();
    void AddAccordionHeaderExpandedIcon(std::string_view res, bool svgOrImage, bool isPath);
    void AddAccordionHeaderCollapsedIcon(std::string_view res, bool svgOrImage, bool isPath);
    void AddAccordionHeaderText(std::string_view content, bool isRichText = false);
    void EndAccordionHeader(std::optional<bool> expanded = std::nullopt);
    bool StartAccordionContent(float height = FLT_MAX, int32_t scrollflags = 0, ImVec2 maxsz = { FLT_MAX, FLT_MAX });
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

    bool StartPlot(std::string_view id, ImVec2 size = { FLT_MAX, FLT_MAX }, int32_t flags = 0);
    WidgetDrawResult EndPlot();
}

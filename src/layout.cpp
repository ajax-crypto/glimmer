#include "layout.h"

#include "context.h"
#include "style.h"
#include "draw.h"

#define GLIMMER_FLAT_LAYOUT_ENGINE 0
#define GLIMMER_CLAY_LAYOUT_ENGINE 1
#define GLIMMER_YOGA_LAYOUT_ENGINE 2

#ifndef GLIMMER_LAYOUT_ENGINE
#define GLIMMER_LAYOUT_ENGINE GLIMMER_YOGA_LAYOUT_ENGINE
#endif

#if GLIMMER_LAYOUT_ENGINE == GLIMMER_CLAY_LAYOUT_ENGINE
#define CLAY_IMPLEMENTATION
#include "libs/inc/clay/clay.h"

Clay_Arena LayoutArena;
void* LayoutMemory = nullptr;
#endif

#if GLIMMER_LAYOUT_ENGINE == GLIMMER_YOGA_LAYOUT_ENGINE
#include "libs/inc/yoga/Yoga.h"
YGNodeRef root = nullptr;
std::vector<YGNodeRef> children;

static ImRect GetBoundingBox(YGNodeConstRef node)
{
    ImRect box;
    box.Min = { YGNodeLayoutGetLeft(node), YGNodeLayoutGetTop(node) };
    box.Max = box.Min + ImVec2{ YGNodeLayoutGetWidth(node), YGNodeLayoutGetHeight(node) };
    return box;
}
#endif

namespace glimmer
{
    WidgetDrawResult LabelImpl(int32_t id, const StyleDescriptor& style, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io, int32_t textflags);
    WidgetDrawResult ButtonImpl(int32_t id, const StyleDescriptor& style, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io);
    WidgetDrawResult ToggleButtonImpl(int32_t id, ToggleButtonState& state, const StyleDescriptor& style, const ImRect& extent, ImVec2 textsz, IRenderer& renderer, const IODescriptor& io);
    WidgetDrawResult RadioButtonImpl(int32_t id, RadioButtonState& state, const StyleDescriptor& style, const ImRect& extent, IRenderer& renderer, const IODescriptor& io);
    WidgetDrawResult CheckboxImpl(int32_t id, CheckboxState& state, const StyleDescriptor& style, const ImRect& extent, const ImRect& padding, IRenderer& renderer, const IODescriptor& io);
    WidgetDrawResult SliderImpl(int32_t id, SliderState& state, const StyleDescriptor& style, const ImRect& extent, IRenderer& renderer, const IODescriptor& io);
    WidgetDrawResult SpinnerImpl(int32_t id, const SpinnerState& state, const StyleDescriptor& style, const ImRect& extent, const IODescriptor& io, IRenderer& renderer);
    WidgetDrawResult TextInputImpl(int32_t id, TextInputState& state, const StyleDescriptor& style, const ImRect& extent, const ImRect& content, IRenderer& renderer, const IODescriptor& io);
    WidgetDrawResult DropDownImpl(int32_t id, DropDownState& state, const StyleDescriptor& style, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io);
    WidgetDrawResult ItemGridImpl(int32_t id, const StyleDescriptor& style, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io);
    void StartScrollableImpl(int32_t id, bool hscroll, bool vscroll, ImVec2 maxsz, const StyleDescriptor& style,
        const ImRect& border, const ImRect& content, IRenderer& renderer);
    void EndScrollableImpl(int32_t id, IRenderer& renderer);
    WidgetDrawResult TabBarImpl(int32_t id, const ImRect& content, const StyleDescriptor& style, const IODescriptor& io,
        IRenderer& renderer);
    void RecordItemGeometry(const LayoutItemDescriptor& layoutItem);

#pragma region Layout functions

    void PushSpan(int32_t direction)
    {
        GetContext().spans.push() = direction;
    }

    void SetSpan(int32_t direction)
    {
        GetContext().spans.push() = direction | OnlyOnce;
    }

    void Move(int32_t direction)
    {
        auto& context = GetContext();
        if (!context.layouts.empty()) return;
        auto& layout = context.adhocLayout.top();
        assert(layout.lastItemId != -1);
        Move(layout.lastItemId, direction);
    }

    void Move(int32_t id, int32_t direction)
    {
        auto& context = GetContext();
        if (!context.layouts.empty()) return;
        const auto& geometry = context.GetGeometry(id);
        auto& layout = context.adhocLayout.top();
        layout.nextpos = geometry.Min;
        if (direction & FD_Horizontal) layout.nextpos.x = geometry.Max.x;
        if (direction & FD_Vertical) layout.nextpos.y = geometry.Max.y;
    }

    void Move(int32_t hid, int32_t vid, bool toRight, bool toBottom)
    {
        auto& context = GetContext();
        if (!context.layouts.empty()) return;
        const auto& hgeometry = context.GetGeometry(hid);
        const auto& vgeometry = context.GetGeometry(vid);
        auto& layout = context.adhocLayout.top();
        layout.nextpos.x = toRight ? hgeometry.Max.x : hgeometry.Min.x;
        layout.nextpos.y = toBottom ? vgeometry.Max.y : vgeometry.Min.y;
    }

    void Move(ImVec2 amount, int32_t direction)
    {
        auto& context = GetContext();
        if (!context.layouts.empty()) return;
        if (direction & ToLeft) amount.x = -amount.x;
        if (direction & ToTop) amount.y = -amount.y;
        auto& layout = context.adhocLayout.top();
        layout.nextpos += amount;
    }

    void Move(ImVec2 pos)
    {
        auto& context = GetContext();
        if (!context.layouts.empty()) return;
        context.adhocLayout.top().nextpos = pos;
    }

    void PopSpan(int depth)
    {
        auto& context = GetContext();
        context.spans.pop(depth, true);
    }

    static void ComputeExtent(LayoutItemDescriptor& layoutItem, ImVec2 nextpos, const StyleDescriptor& style, 
        const NeighborWidgets& neighbors, float width, float height)
    {
        auto& context = GetContext();

        if (layoutItem.sizing & FromLeft)
        {
            layoutItem.margin.Min.x = nextpos.x;
            layoutItem.border.Min.x = layoutItem.margin.Min.x + style.margin.left;
            layoutItem.padding.Min.x = layoutItem.border.Min.x + style.border.left.thickness;
            layoutItem.content.Min.x = layoutItem.padding.Min.x + style.padding.left;
        }
        else
        {
            layoutItem.margin.Max.x = nextpos.x + width;
            layoutItem.border.Max.x = layoutItem.margin.Max.x - style.margin.left;
            layoutItem.padding.Max.x = layoutItem.border.Max.x - style.border.left.thickness;
            layoutItem.content.Max.x = layoutItem.padding.Max.x - style.padding.left;
        }

        if (layoutItem.sizing & FromTop)
        {
            layoutItem.margin.Min.y = nextpos.y;
            layoutItem.border.Min.y = layoutItem.margin.Min.y + style.margin.top;
            layoutItem.padding.Min.y = layoutItem.border.Min.y + style.border.top.thickness;
            layoutItem.content.Min.y = layoutItem.padding.Min.y + style.padding.top;
        }
        else
        {
            layoutItem.margin.Max.y = nextpos.y + height;
            layoutItem.border.Max.y = layoutItem.margin.Max.y - style.margin.top;
            layoutItem.padding.Max.y = layoutItem.border.Max.y - style.border.top.thickness;
            layoutItem.content.Max.y = layoutItem.padding.Max.y - style.padding.top;
        }

        if (style.specified & StyleWidth)
        {
            auto w = clamp(style.dimension.x, style.mindim.x, style.maxdim.x);

            if (layoutItem.sizing & FromRight) [[unlikely]]
            {
                layoutItem.content.Min.x = layoutItem.content.Max.x - w;
                layoutItem.padding.Min.x = layoutItem.content.Min.x - style.padding.right;
                layoutItem.border.Min.x = layoutItem.padding.Min.x - style.border.right.thickness;
                layoutItem.margin.Min.x = layoutItem.border.Min.x - style.margin.right;
            }
            else
            {
                layoutItem.content.Max.x = layoutItem.content.Min.x + w;
                layoutItem.padding.Max.x = layoutItem.content.Max.x + style.padding.right;
                layoutItem.border.Max.x = layoutItem.padding.Max.x + style.border.right.thickness;
                layoutItem.margin.Max.x = layoutItem.border.Max.x + style.margin.right;
            }
        }
        else
        {
            if (layoutItem.sizing & FromRight) [[unlikely]]
            {
                if (neighbors.left != -1) layoutItem.margin.Min.x = context.GetGeometry(neighbors.left).Max.x;
                else layoutItem.margin.Min.x = layoutItem.margin.Max.x - width;

                layoutItem.border.Min.x = layoutItem.margin.Min.x + style.margin.right;
                layoutItem.padding.Min.x = layoutItem.border.Min.x + style.border.right.thickness;
                layoutItem.content.Min.x = layoutItem.padding.Min.x + style.padding.right;
            }
            else
            {
                if (neighbors.right != -1) layoutItem.margin.Max.x = context.GetGeometry(neighbors.right).Min.x;
                else layoutItem.margin.Max.x = layoutItem.margin.Min.x + width;

                layoutItem.border.Max.x = layoutItem.margin.Max.x - style.margin.right;
                layoutItem.padding.Max.x = layoutItem.border.Max.x - style.border.right.thickness;
                layoutItem.content.Max.x = layoutItem.padding.Max.x - style.padding.right;
            }
        }

        if (style.specified & StyleHeight)
        {
            auto h = clamp(style.dimension.y, style.mindim.x, style.maxdim.x);

            if (layoutItem.sizing & FromBottom)
            {
                layoutItem.content.Min.y = layoutItem.content.Max.y - h;
                layoutItem.padding.Min.y = layoutItem.content.Min.y - style.padding.bottom;
                layoutItem.border.Min.y = layoutItem.padding.Min.y - style.border.bottom.thickness;
                layoutItem.margin.Min.y = layoutItem.border.Min.y - style.margin.bottom;
            }
            else
            {
                layoutItem.content.Max.y = layoutItem.content.Min.y + h;
                layoutItem.padding.Max.y = layoutItem.content.Max.y + style.padding.bottom;
                layoutItem.border.Max.y = layoutItem.padding.Max.y + style.border.bottom.thickness;
                layoutItem.margin.Max.y = layoutItem.border.Max.y + style.margin.bottom;
            }
        }
        else
        {
            if (layoutItem.sizing & FromBottom)
            {
                if (neighbors.top != -1) layoutItem.margin.Min.y = context.GetGeometry(neighbors.top).Max.y;
                else layoutItem.margin.Min.y = layoutItem.margin.Max.y - height;

                layoutItem.border.Min.y = layoutItem.margin.Min.y + style.margin.bottom;
                layoutItem.padding.Min.y = layoutItem.border.Min.y + style.border.bottom.thickness;
                layoutItem.content.Min.y = layoutItem.padding.Min.y + style.padding.bottom;
            }
            else
            {
                if (neighbors.bottom != -1) layoutItem.margin.Max.y = context.GetGeometry(neighbors.bottom).Min.y;
                else layoutItem.margin.Max.y = layoutItem.margin.Min.y + height;

                layoutItem.border.Max.y = layoutItem.margin.Max.y - style.margin.bottom;
                layoutItem.padding.Max.y = layoutItem.border.Max.y - style.border.bottom.thickness;
                layoutItem.content.Max.y = layoutItem.padding.Max.y - style.padding.bottom;
            }
        }
    }

    static void AddDefaultDirection(LayoutItemDescriptor& layoutItem)
    {
        if (!(layoutItem.sizing & FromLeft) && !(layoutItem.sizing & FromRight))
            layoutItem.sizing |= FromLeft;

        if (!(layoutItem.sizing & FromTop) && !(layoutItem.sizing & FromBottom))
            layoutItem.sizing |= FromTop;
    }

    void AddExtent(LayoutItemDescriptor& layoutItem, const StyleDescriptor& style, const NeighborWidgets& neighbors,
        float width, float height)
    {
        auto& context = GetContext();
        auto totalsz = context.MaximumAbsExtent();
        auto nextpos = !context.layouts.empty() ? context.layouts.top().nextpos : context.NextAdHocPos();
        AddDefaultDirection(layoutItem);

        if (width <= 0.f) width = clamp(totalsz.x - nextpos.x, style.mindim.x, style.maxdim.x);
        if (height <= 0.f) height = clamp(totalsz.y - nextpos.y, style.mindim.y, style.maxdim.y);

        ComputeExtent(layoutItem, nextpos, style, neighbors, width, height);
    }

    void AddExtent(LayoutItemDescriptor& layoutItem, const StyleDescriptor& style, const NeighborWidgets& neighbors,
        ImVec2 size, ImVec2 totalsz)
    {
        auto& context = GetContext();
        auto nextpos = !context.layouts.empty() ? context.layouts.top().nextpos : context.NextAdHocPos();
        auto [width, height] = size;
        AddDefaultDirection(layoutItem);

        if (width <= 0.f) width = clamp(totalsz.x - nextpos.x, style.mindim.x, style.maxdim.x);
        if (height <= 0.f) height = clamp(totalsz.y - nextpos.y, style.mindim.y, style.maxdim.y);

        ComputeExtent(layoutItem, nextpos, style, neighbors, width, height);
    }

#if GLIMMER_LAYOUT_ENGINE == GLIMMER_FLAT_LAYOUT_ENGINE
    static float GetTotalSpacing(LayoutDescriptor& layout, Direction dir)
    {
        return dir == DIR_Horizontal ? (((float)layout.currcol + 2.f) * layout.spacing.x) :
            (((float)layout.currow + 2.f) * layout.spacing.y);
    }

    static void TranslateX(LayoutItemDescriptor& item, float amount)
    {
        item.margin.TranslateX(amount);
        item.border.TranslateX(amount);
        item.padding.TranslateX(amount);
        item.content.TranslateX(amount);
        item.text.TranslateX(amount);
    }

    static void TranslateY(LayoutItemDescriptor& item, float amount)
    {
        item.margin.TranslateY(amount);
        item.border.TranslateY(amount);
        item.padding.TranslateY(amount);
        item.content.TranslateY(amount);
        item.text.TranslateY(amount);
    }

    static void AlignLayoutAxisItems(LayoutDescriptor& layout)
    {
        auto& context = GetContext();

        switch (layout.type)
        {
        case Layout::Horizontal:
        {
            // If wrapping is enabled and the alignment is horizontally centered,
            // perform h-centering of the current row of widgets and move to next row
            // Otherwise, if output should be justified, move all widgets to specific
            // location after distributing the diff equally...
            auto width = layout.geometry.Max.x == FLT_MAX ? layout.extent.GetWidth() : layout.geometry.GetWidth();

            if ((layout.alignment & TextAlignHCenter) && (layout.fill & FD_Horizontal))
            {
                auto totalspacing = GetTotalSpacing(layout, DIR_Horizontal);
                auto hdiff = (width - layout.rows[layout.currow].x - totalspacing) * 0.5f;

                if (hdiff > 0.f)
                {
                    for (auto idx = layout.from; idx <= layout.to; ++idx)
                    {
                        auto& widget = context.layoutItems[idx];
                        if (widget.row == layout.currow) TranslateX(widget, hdiff);
                    }
                }
            }
            else if ((layout.alignment & TextAlignJustify) && (layout.fill & FD_Horizontal))
            {
                auto hdiff = (width - layout.rows[layout.currow].x) /
                    ((float)layout.currcol + 1.f);

                if (hdiff > 0.f)
                {
                    auto currposx = hdiff;
                    for (auto idx = layout.from; idx <= layout.to; ++idx)
                    {
                        auto& widget = context.layoutItems[idx];

                        if (widget.row == layout.currow)
                        {
                            auto translatex = currposx - widget.margin.Min.x;
                            TranslateX(widget, translatex);
                            currposx += widget.margin.GetWidth() + hdiff;
                        }
                    }
                }
            }
            break;
        }

        case Layout::Vertical:
        {
            // Similar logic as for horizontal alignment, implemented for vertical here
            auto height = layout.geometry.Max.y == FLT_MAX ? layout.extent.GetHeight() : layout.geometry.GetHeight();

            if ((layout.alignment & TextAlignVCenter) && (layout.fill & FD_Vertical))
            {
                auto totalspacing = GetTotalSpacing(layout, DIR_Vertical);
                auto hdiff = (height - layout.cols[layout.currcol].y - totalspacing) * 0.5f;

                if (hdiff > 0.f)
                {
                    for (auto idx = layout.from; idx <= layout.to; ++idx)
                    {
                        auto& widget = context.layoutItems[idx];
                        if (widget.col == layout.currcol) TranslateY(widget, hdiff);
                    }
                }
            }
            else if ((layout.alignment & TextAlignJustify) && (layout.fill & FD_Vertical))
            {
                auto hdiff = (height - layout.cols[layout.currcol].x) /
                    ((float)layout.currow + 1.f);

                if (hdiff > 0.f)
                {
                    auto currposx = hdiff;
                    for (auto idx = layout.from; idx <= layout.to; ++idx)
                    {
                        auto& widget = context.layoutItems[idx];
                        if (widget.col == layout.currcol)
                        {
                            auto translatex = currposx - widget.margin.Min.y;
                            TranslateY(widget, hdiff);
                            currposx += widget.margin.GetHeight() + hdiff;
                        }
                    }
                }
            }
            break;
        }
        }
    }

    static void AlignCrossAxisItems(LayoutDescriptor& layout, int depth)
    {
        auto& context = GetContext();

        if ((layout.fill & FD_Horizontal) == 0)
            layout.geometry.Max.x = context.layoutItems[layout.itemidx].margin.Max.x = layout.maxdim.x;
        if ((layout.fill & FD_Vertical) == 0)
            layout.geometry.Max.y = context.layoutItems[layout.itemidx].margin.Max.y = layout.maxdim.y;

        switch (layout.type)
        {
        case Layout::Horizontal:
        {
            auto height = layout.geometry.Max.y == FLT_MAX ? layout.extent.GetHeight() : layout.geometry.GetHeight();

            if ((layout.alignment & TextAlignVCenter) && (layout.fill & FD_Vertical))
            {
                auto totalspacing = GetTotalSpacing(layout, DIR_Vertical);
                auto cumulativey = layout.cumulative.y + layout.maxdim.y;
                auto vdiff = (height - totalspacing - cumulativey) * 0.5f;

                if (vdiff > 0.f)
                {
                    for (auto idx = layout.from; idx <= layout.to; ++idx)
                    {
                        auto& widget = context.layoutItems[idx];
                        TranslateY(widget, vdiff);
                    }
                }
            }
            else if ((layout.alignment & TextAlignJustify) && (layout.fill & FD_Vertical))
            {
                auto cumulativey = layout.cumulative.y + layout.maxdim.y;
                auto vdiff = (height - cumulativey) / ((float)layout.currow + 1.f);

                if (vdiff > 0.f)
                {
                    auto currposy = vdiff;
                    for (auto idx = layout.from; idx <= layout.to; ++idx)
                    {
                        auto& widget = context.layoutItems[idx];
                        auto translatey = currposy - widget.margin.Min.y;
                        TranslateY(widget, vdiff);
                        currposy += vdiff + widget.margin.GetHeight();
                    }
                }
            }
            else if ((layout.alignment & TextAlignBottom) && (layout.fill & FD_Vertical))
            {
                auto totalspacing = GetTotalSpacing(layout, DIR_Vertical);
                auto vdiff = height - layout.maxdim.y - totalspacing;

                if (vdiff > 0.f)
                {
                    for (auto idx = layout.from; idx <= layout.to; ++idx)
                    {
                        auto& widget = context.layoutItems[idx];
                        TranslateY(widget, vdiff);
                    }
                }
            }
            break;
        }

        case Layout::Vertical:
        {
            auto width = layout.geometry.Max.x == FLT_MAX ? layout.extent.GetWidth() : layout.geometry.GetWidth();

            if ((layout.alignment & TextAlignHCenter) && (layout.fill & FD_Horizontal))
            {
                auto totalspacing = GetTotalSpacing(layout, DIR_Horizontal);
                auto cumulativex = layout.cumulative.x + layout.maxdim.x;
                auto hdiff = (width - totalspacing - cumulativex) * 0.5f;

                if (hdiff > 0.f)
                {
                    for (auto idx = layout.from; idx <= layout.to; ++idx)
                    {
                        auto& widget = context.layoutItems[idx];
                        TranslateX(widget, hdiff);
                    }
                }
            }
            else if ((layout.alignment & TextAlignJustify) && (layout.fill & FD_Horizontal))
            {
                auto cumulativex = layout.cumulative.x + layout.maxdim.x;
                auto hdiff = (width - cumulativex) / ((float)layout.currcol + 1.f);

                if (hdiff > 0.f)
                {
                    auto currposy = hdiff;
                    for (auto idx = layout.from; idx <= layout.to; ++idx)
                    {
                        auto& widget = context.layoutItems[idx];
                        auto translatex = currposy - widget.margin.Min.x;
                        TranslateX(widget, hdiff);
                        currposy += hdiff + widget.margin.GetWidth();
                    }
                }
            }
            else if ((layout.alignment & TextAlignRight) && (layout.fill & FD_Vertical))
            {
                auto totalspacing = GetTotalSpacing(layout, DIR_Horizontal);
                auto hdiff = width - layout.maxdim.x - totalspacing;

                if (hdiff > 0.f)
                {
                    for (auto idx = layout.from; idx <= layout.to; ++idx)
                    {
                        auto& widget = context.layoutItems[idx];
                        TranslateX(widget, hdiff);
                    }
                }
            }
            break;
        }
        }
    }
#endif

    void AddItemToLayout(LayoutDescriptor& layout, LayoutItemDescriptor& item, const StyleDescriptor& style)
    {
        layout.itemIndexes.emplace_back(GetContext().layoutItems.size(), LayoutOps::AddWidget);

#if GLIMMER_LAYOUT_ENGINE == GLIMMER_FLAT_LAYOUT_ENGINE
        ImVec2 offset = layout.nextpos - layout.geometry.Min;
        layout.currow = std::max(layout.currow, (int16_t)0);
        layout.currcol = std::max(layout.currcol, (int16_t)0);

        if (layout.rows.empty()) layout.rows.resize(32);
        if (layout.cols.empty()) layout.cols.resize(16);

        switch (layout.type)
        {
        case Layout::Horizontal:
        {
            auto width = item.margin.GetWidth();

            if ((layout.fill & FD_Horizontal) && (layout.hofmode == OverflowMode::Wrap))
            {
                if (layout.nextpos.x + width > layout.geometry.Max.x)
                {
                    if ((layout.fill & FD_Vertical) == 0) layout.geometry.Max.y += layout.maxdim.y;

                    offset = ImVec2{ layout.spacing.x, layout.cumulative.y + layout.maxdim.y + 
                        GetTotalSpacing(layout, DIR_Vertical) };
                    layout.nextpos.x = width + layout.spacing.x;
                    layout.nextpos.y = offset.y + layout.spacing.y;
                    AlignLayoutAxisItems(layout);

                    layout.cumulative.y += layout.maxdim.y;
                    layout.maxdim.y = 0.f;
                    layout.currcol = 0;
                    layout.currow++;
                    item.row = layout.currow;
                    item.col = 0;
                    layout.rows[layout.currow].x = 0.f;
                }
                else
                {
                    layout.maxdim.y = std::max(layout.maxdim.y, item.margin.GetHeight());
                    layout.nextpos.x += width + layout.spacing.x;
                    layout.rows[layout.currow].x += width;
                    item.col = layout.currcol;
                    item.row = layout.currow;
                    layout.currcol++;
                }
            }
            else
            {
                if ((layout.fill & FD_Horizontal) == 0) layout.geometry.Max.x += width;
                layout.nextpos.x += (width + layout.spacing.x);
                layout.maxdim.y = std::max(layout.maxdim.y, item.margin.GetHeight());
                if ((layout.fill & FD_Vertical) == 0) layout.geometry.Max.y = layout.geometry.Min.y + layout.maxdim.y;
                else layout.cumulative.y = layout.maxdim.y;
                layout.currcol++;
                layout.rows[layout.currow].x += width;
            }

            break;
        }

        case Layout::Vertical:
        {
            auto height = item.margin.GetHeight();

            if ((layout.fill & FD_Vertical) && (layout.vofmode == OverflowMode::Wrap))
            {
                if (layout.nextpos.y + height > layout.geometry.Max.y)
                {
                    if ((layout.fill & FD_Vertical) == 0) layout.geometry.Max.x += layout.maxdim.x;

                    offset = ImVec2{ layout.cumulative.x + layout.maxdim.x + GetTotalSpacing(layout, DIR_Horizontal), 
                        layout.spacing.y};
                    layout.nextpos.x = offset.x + layout.spacing.x;
                    layout.nextpos.y = height + layout.spacing.y;
                    AlignLayoutAxisItems(layout);

                    layout.cumulative.x += layout.maxdim.x;
                    layout.maxdim.x = 0.f;
                    layout.currow = 0;
                    layout.currcol++;
                    item.col = layout.currcol;
                    layout.cols[layout.currcol].y = 0.f;
                }
                else
                {
                    layout.maxdim.x = std::max(layout.maxdim.x, item.margin.GetWidth());
                    layout.nextpos.y += height + layout.spacing.y;
                    layout.cols[layout.currcol].y += height;
                    item.col = layout.currcol;
                    item.row = layout.currow;
                    layout.currow++;
                }
            }
            else
            {
                if ((layout.fill & FD_Vertical) == 0) layout.geometry.Max.y += height;
                layout.nextpos.y += (height + layout.spacing.y);
                layout.maxdim.x = std::max(layout.maxdim.x, item.margin.GetWidth());
                if ((layout.fill & FD_Horizontal) == 0) layout.geometry.Max.x = layout.geometry.Min.x + layout.maxdim.x;
                layout.currow++;
                layout.cols[layout.currcol].y += height;
            }
            break;
        }

        case Layout::Grid:
        {
            break;
        }
        default:
            break;
        }

        // This offsetting is not required, TODO: Remove it?
        /*item.margin.Translate(offset);
        item.border.Translate(offset);
        item.padding.Translate(offset);
        item.content.Translate(offset);
        item.text.Translate(offset);*/
        layout.extent.Min = ImMin(layout.extent.Min, item.margin.Min);
        layout.extent.Max = ImMax(layout.extent.Max, item.margin.Max);

#elif GLIMMER_LAYOUT_ENGINE == GLIMMER_CLAY_LAYOUT_ENGINE

        Clay__OpenElement();
        Clay_ElementDeclaration decl;
        decl.custom.customData = reinterpret_cast<void*>((intptr_t)layout.to);
        decl.layout.layoutDirection = layout.fill & FD_Horizontal ? Clay_LayoutDirection::CLAY_LEFT_TO_RIGHT : Clay_LayoutDirection::CLAY_TOP_TO_BOTTOM;
        decl.clip.vertical = decl.clip.horizontal = false;
        decl.userData = nullptr;
        decl.id.id = 0;
        decl.backgroundColor.a = 0;
        decl.cornerRadius.bottomLeft = decl.cornerRadius.bottomRight = decl.cornerRadius.topLeft = decl.cornerRadius.topRight = decl.border.width.betweenChildren = 0;
        decl.image.imageData = nullptr;
        decl.border.width.top = decl.border.width.bottom = decl.border.width.left = decl.border.width.right = 0;

        decl.layout.sizing.width.size.minMax.min = decl.layout.sizing.width.size.minMax.max = item.margin.GetWidth();
        decl.layout.sizing.width.type = item.sizing & ExpandH ? Clay__SizingType::CLAY__SIZING_TYPE_GROW : 
            item.relative.x > 0.f ? Clay__SizingType::CLAY__SIZING_TYPE_PERCENT : Clay__SizingType::CLAY__SIZING_TYPE_FIXED;

        decl.layout.sizing.height.size.minMax.min = decl.layout.sizing.height.size.minMax.max = item.margin.GetHeight();
        decl.layout.sizing.height.type = item.sizing & ExpandV ? Clay__SizingType::CLAY__SIZING_TYPE_GROW :
            item.relative.y > 0.f ? Clay__SizingType::CLAY__SIZING_TYPE_PERCENT : Clay__SizingType::CLAY__SIZING_TYPE_FIXED;

        Clay__ConfigureOpenElement(decl);
        Clay__CloseElement();

#elif GLIMMER_LAYOUT_ENGINE == GLIMMER_YOGA_LAYOUT_ENGINE
        
        YGNodeRef child = YGNodeNew();
        YGNodeStyleSetWidth(child, item.margin.GetWidth());
        YGNodeStyleSetHeight(child, item.margin.GetHeight());
        if (style.maxdim.x != FLT_MAX) YGNodeStyleSetMaxWidth(child, style.maxdim.x);
        if (style.maxdim.y != FLT_MAX) YGNodeStyleSetMaxHeight(child, style.maxdim.y);
        if (style.mindim.x != 0) YGNodeStyleSetMinWidth(child, style.mindim.x);
        if (style.mindim.y != 0) YGNodeStyleSetMinHeight(child, style.mindim.y);
        
        // Main-axis flex growth/shrink
        if ((layout.type == Layout::Horizontal) && (item.sizing & ExpandH)) YGNodeStyleSetFlexGrow(child, 1);
        else if ((layout.type == Layout::Vertical) && (item.sizing & ExpandV)) YGNodeStyleSetFlexGrow(child, 1);
        else YGNodeStyleSetFlexGrow(child, 0);

        if ((layout.type == Layout::Horizontal) && (item.sizing & ShrinkH)) YGNodeStyleSetFlexShrink(child, 1);
        else if ((layout.type == Layout::Vertical) && (item.sizing & ShrinkV)) YGNodeStyleSetFlexShrink(child, 1);
        else YGNodeStyleSetFlexShrink(child, 0);

        // Cross-axis override aligment
        if ((layout.type == Layout::Vertical) && (item.sizing & ExpandH)) YGNodeStyleSetAlignSelf(child, YGAlignStretch);
        else if ((layout.type == Layout::Horizontal) && (item.sizing & ExpandV)) YGNodeStyleSetAlignSelf(child, YGAlignStretch);

        YGNodeInsertChild(root, child, YGNodeGetChildCount(root));
        children.emplace_back(child);

#endif

        auto& context = GetContext();
        context.layoutItems.push_back(item);

        if (!context.spans.empty() && (context.spans.top() & OnlyOnce) != 0) context.spans.pop(1, true);

        if (layout.from == -1) layout.from = layout.to = (int16_t)(context.layoutItems.size() - 1);
        else layout.to = (int16_t)(context.layoutItems.size() - 1);
        layout.itemidx = layout.to;
    }

    static ImRect GetAvailableSpace(int32_t direction, ImVec2 nextpos, const NeighborWidgets& neighbors)
    {
        ImRect available;
        auto& context = GetContext();
        auto maxabs = context.MaximumAbsExtent();

        available.Min.y = nextpos.y;
        available.Max.y = neighbors.bottom == -1 ? maxabs.y : context.GetGeometry(neighbors.bottom).Min.y;
        available.Min.x = nextpos.x;
        available.Max.x = neighbors.right == -1 ? maxabs.x : context.GetGeometry(neighbors.right).Min.x;

        return available;
    }

    static bool IsLayoutDependentOnContent(const LayoutDescriptor& layout)
    {
        return layout.fill != 0;
    }

    ImRect BeginLayout(Layout type, int32_t fill, int32_t alignment, bool wrap, ImVec2 spacing, const NeighborWidgets& neighbors)
    {
        auto& context = GetContext();

#if GLIMMER_LAYOUT_ENGINE == GLIMMER_FLAT_LAYOUT_ENGINE
        assert(context.layouts.empty()); // No nested layout is supported
#endif

        auto& layout = context.layouts.push();
        const auto& style = context.currStyleStates == 0 ? context.StyleStack[WSI_Default].top() : context.currStyle[WSI_Default];

        auto& el = context.nestedContextStack.push();
        el.source = NestedContextSourceType::Layout;
        layout.id = (WT_Layout << 16) | context.maxids[WT_Layout];
        context.maxids[WT_Layout]++;

        layout.type = type;
        layout.alignment = alignment;
        layout.fill = fill;
        layout.spacing = spacing;
        layout.type == Layout::Horizontal ? (layout.hofmode = wrap ? OverflowMode::Wrap : OverflowMode::Scroll) :
            (layout.vofmode = wrap ? OverflowMode::Wrap : OverflowMode::Scroll);
        
        // Record style stack states for context, will be restored in EndLayout()
        for (auto idx = 0; idx < WSI_Total; ++idx)
        {
            layout.styleStartIdx[idx] = context.StyleStack[idx].size();
            layout.currstyle[idx] = context.currStyle[idx];
        }
        layout.currStyleStates = context.currStyleStates;
        
        auto nextpos = context.NextAdHocPos();
        ImRect available = GetAvailableSpace(alignment, nextpos, neighbors);

#if GLIMMER_LAYOUT_ENGINE == GLIMMER_CLAY_LAYOUT_ENGINE

        if (context.layouts.size() == 1)
        {
            uint64_t totalMemorySize = Clay_MinMemorySize();
            if (LayoutMemory == nullptr) LayoutMemory = malloc(totalMemorySize);
            LayoutArena = Clay_CreateArenaWithCapacityAndMemory(totalMemorySize, LayoutMemory);
            Clay_Initialize(LayoutArena, { available.GetWidth(), available.GetHeight() }, {NULL});
            Clay_BeginLayout();
        }

        Clay__OpenElement();
        Clay_ElementDeclaration decl;
        decl.layout.layoutDirection = layout.type == Layout::Horizontal ? Clay_LayoutDirection::CLAY_LEFT_TO_RIGHT : Clay_LayoutDirection::CLAY_TOP_TO_BOTTOM;
        decl.layout.childGap = layout.spacing.x;
        decl.layout.childAlignment.x = alignment & TextAlignHCenter ? Clay_LayoutAlignmentX::CLAY_ALIGN_X_CENTER : 
            alignment & TextAlignRight ? Clay_LayoutAlignmentX::CLAY_ALIGN_X_RIGHT : Clay_LayoutAlignmentX::CLAY_ALIGN_X_LEFT;
        decl.layout.childAlignment.y = alignment & TextAlignVCenter ? Clay_LayoutAlignmentY::CLAY_ALIGN_Y_BOTTOM : 
            alignment & TextAlignBottom ? Clay_LayoutAlignmentY::CLAY_ALIGN_Y_BOTTOM : Clay_LayoutAlignmentY::CLAY_ALIGN_Y_TOP;

        decl.layout.sizing.width.size.minMax.min = decl.layout.sizing.width.size.minMax.max = available.GetWidth();
        decl.layout.sizing.width.type = Clay__SizingType::CLAY__SIZING_TYPE_FIXED;
        decl.layout.sizing.height.size.minMax.min = decl.layout.sizing.height.size.minMax.max = available.GetHeight();
        decl.layout.sizing.height.type = Clay__SizingType::CLAY__SIZING_TYPE_FIXED;
        decl.clip.vertical = decl.clip.horizontal = false;
        decl.userData = nullptr;
        decl.backgroundColor.a = 0;
        decl.cornerRadius.bottomLeft = decl.cornerRadius.bottomRight = decl.cornerRadius.topLeft = decl.cornerRadius.topRight = 0;
        decl.image.imageData = nullptr;
        decl.border.width.top = decl.border.width.bottom = decl.border.width.left = decl.border.width.right = decl.border.width.betweenChildren = 0;

        decl.image.imageData = decl.custom.customData = nullptr;
        decl.floating.attachTo = CLAY_ATTACH_TO_NONE;
        Clay__ConfigureOpenElement(decl);

        // Do we need to do this?
        /*if (!context.containerStack.empty())
        {
            auto id = context.containerStack.top();
            auto wtype = (WidgetType)(id >> 16);

            Clay_SetExternalScrollHandlingEnabled(true);
            Clay_SetQueryScrollOffsetFunction([](uint32_t id, void* data) {
                context.ScrollRegion(id);
            });
        }*/

#elif GLIMMER_LAYOUT_ENGINE == GLIMMER_YOGA_LAYOUT_ENGINE

        assert(context.layouts.size() == 1); // TODO: Implement nested layout support...

        root = YGNodeNew();
        if ((fill & FD_Horizontal) && (available.Max.x != FLT_MAX)) YGNodeStyleSetWidth(root, available.GetWidth());
        if ((fill & FD_Vertical) && (available.Max.y != FLT_MAX)) YGNodeStyleSetHeight(root, available.GetHeight());
        YGNodeStyleSetFlexDirection(root, type == Layout::Horizontal ? YGFlexDirectionRow : YGFlexDirectionColumn);
        YGNodeStyleSetFlexWrap(root, wrap ? YGWrapWrap : YGWrapNoWrap);
        YGNodeStyleSetPosition(root, YGEdgeLeft, 0.f);
        YGNodeStyleSetPosition(root, YGEdgeTop, 0.f);
        YGNodeStyleSetGap(root, YGGutterRow, spacing.x);
        YGNodeStyleSetGap(root, YGGutterColumn, spacing.y);

        if ((type == Layout::Horizontal))
        {
            // Main axis alignment
            if (alignment & TextAlignRight) YGNodeStyleSetJustifyContent(root, YGJustifyFlexEnd);
            else if (alignment & TextAlignHCenter) YGNodeStyleSetJustifyContent(root, YGJustifyCenter);
            else if (alignment & TextAlignJustify) YGNodeStyleSetJustifyContent(root, YGJustifySpaceAround);
            else YGNodeStyleSetJustifyContent(root, YGJustifyFlexStart);

            // Cross axis alignment
            if (alignment & TextAlignBottom) YGNodeStyleSetAlignItems(root, YGAlignFlexEnd);
            else if (alignment & TextAlignVCenter) YGNodeStyleSetAlignItems(root, YGAlignCenter);
            else YGNodeStyleSetAlignItems(root, YGAlignFlexStart);
        }
        else
        {
            // Main axis alignment
            if (alignment & TextAlignBottom) YGNodeStyleSetJustifyContent(root, YGJustifyFlexEnd);
            else if (alignment & TextAlignVCenter) YGNodeStyleSetJustifyContent(root, YGJustifyCenter);
            else YGNodeStyleSetJustifyContent(root, YGJustifyFlexStart);

            // Cross axis alignment
            if (alignment & TextAlignRight) YGNodeStyleSetAlignItems(root, YGAlignFlexEnd);
            else if (alignment & TextAlignHCenter) YGNodeStyleSetAlignItems(root, YGAlignCenter);
            else YGNodeStyleSetAlignItems(root, YGAlignFlexStart);
        }

#endif

        layout.geometry = available;
        layout.nextpos = nextpos + layout.spacing;
        layout.extent.Min = { FLT_MAX, FLT_MAX };
        return layout.geometry;
    }

    void NextRow()
    {
        auto& context = GetContext();

        if (!context.layouts.empty())
        {
            auto& layout = context.layouts.top();
            if (layout.type == Layout::Horizontal && layout.hofmode == OverflowMode::Wrap)
            {
                layout.cumulative.y += layout.maxdim.y;
                layout.maxdim.y = 0.f;
                layout.currcol = 0;
                layout.currow++;
                layout.rows[layout.currow].x = 0.f;
            }
        }
    }

    void NextColumn()
    {
        auto& context = GetContext();

        if (!context.layouts.empty())
        {
            auto& layout = context.layouts.top();
            if (layout.type == Layout::Horizontal && layout.hofmode == OverflowMode::Wrap)
            {
                layout.cumulative.x += layout.maxdim.x;
                layout.maxdim.x = 0.f;
                layout.currow = 0;
                layout.currcol++;
                layout.cols[layout.currcol].y = 0.f;
            }
        }
    }

    void PushSizing(float width, float height, bool relativew, bool relativeh)
    {
        auto& context = GetContext();
        auto& sizing = context.sizing.push();
        sizing.horizontal = width;
        sizing.vertical = height;
        sizing.relativeh = relativew;
        sizing.relativev = relativeh;
    }

    void PopSizing(int depth)
    {
        auto& context = GetContext();
        context.sizing.pop(depth, true);
    }

    static void UpdateGeometry(LayoutItemDescriptor& item, const ImRect& bbox, const StyleDescriptor& style)
    {
        item.margin.Min.x = bbox.Min.x;
        item.margin.Max.x = bbox.Max.x;

        item.border.Min.x = item.margin.Min.x + style.margin.left;
        item.border.Max.x = item.margin.Max.x - style.margin.right;

        item.padding.Min.x = item.border.Min.x + style.border.left.thickness;
        item.padding.Max.x = item.border.Max.x - style.border.right.thickness;

        item.content.Min.x = item.padding.Min.x + style.padding.left;
        item.content.Max.x = item.padding.Max.x - style.padding.right;

        auto textw = item.text.GetWidth();
        item.text.Min.x = item.content.Min.x;
        item.text.Max.x = item.text.Min.x + textw;

        item.margin.Min.y = bbox.Min.y;
        item.margin.Max.y = bbox.Max.y;

        item.border.Min.y = item.margin.Min.y + style.margin.top;
        item.border.Max.y = item.margin.Max.y - style.margin.bottom;

        item.padding.Min.y = item.border.Min.y + style.border.top.thickness;
        item.padding.Max.y = item.border.Max.y - style.border.bottom.thickness;

        item.content.Min.y = item.padding.Min.y + style.padding.top;
        item.content.Max.y = item.padding.Max.y - style.padding.bottom;

        auto texth = item.text.GetHeight();
        item.text.Min.y = item.content.Min.y;
        item.text.Max.y = item.text.Min.y + texth;
    }

    static StyleDescriptor const& GetStyle(StyleStackT* StyleStack, StyleDescriptor* currStyle,
        int32_t currStyleStates, int32_t state)
    {
        const auto& defstyle = (currStyleStates & WS_Default) ? currStyle[WSI_Default] :
            !StyleStack[WSI_Default].empty() ? StyleStack[WSI_Default].top() : GetContext().GetStyle(WS_Default);

        auto idx = log2((unsigned)state);
        auto& style = (currStyleStates & state) ? currStyle[idx] :
            !StyleStack[idx].empty() ? StyleStack[idx].top() : GetContext().GetStyle(state);
        CopyStyle(defstyle, style);
        return style;
    }

    static WidgetDrawResult RenderWidget(const LayoutDescriptor& layout, LayoutItemDescriptor& item, StyleStackT* StyleStack,
        StyleDescriptor* currStyle, int32_t currStyleStates, const IODescriptor& io)
    {
        WidgetDrawResult result;
        auto& context = GetContext();
        auto bbox = item.margin;
        auto wtype = (WidgetType)(item.id >> 16);
        auto& renderer = context.GetRenderer();
        renderer.SetClipRect(bbox.Min, bbox.Max);

        switch (wtype)
        {
        case glimmer::WT_Label: {
            auto& state = context.GetState(item.id).state.label;
            auto flags = ToTextFlags(state.type);
            const auto& style = GetStyle(StyleStack, currStyle, currStyleStates, state.state);
            UpdateGeometry(item, bbox, style);
            context.AddItemGeometry(item.id, bbox);
            result = LabelImpl(item.id, style, item.margin, item.border, item.padding, item.content, item.text, renderer, io, flags);
            if (!context.nestedContextStack.empty())
                RecordItemGeometry(item);
            break;
        }
        case glimmer::WT_Button: {
            auto& state = context.GetState(item.id).state.button;
            auto flags = ToTextFlags(state.type);
            const auto& style = GetStyle(StyleStack, currStyle, currStyleStates, state.state);
            UpdateGeometry(item, bbox, style);
            context.AddItemGeometry(item.id, bbox);
            result = ButtonImpl(item.id, style, item.margin, item.border, item.padding, item.content, item.text, renderer, io);
            if (!context.nestedContextStack.empty())
                RecordItemGeometry(item);
            break;
        }
        case glimmer::WT_RadioButton: {
            auto& state = context.GetState(item.id).state.radio;
            const auto& style = GetStyle(StyleStack, currStyle, currStyleStates, state.state);
            UpdateGeometry(item, bbox, style);
            context.AddItemGeometry(item.id, bbox);
            result = RadioButtonImpl(item.id, state, style, item.margin, renderer, io);
            if (!context.nestedContextStack.empty())
                RecordItemGeometry(item);
            break;
        }
        case glimmer::WT_ToggleButton: {
            auto& state = context.GetState(item.id).state.toggle;
            const auto& style = GetStyle(StyleStack, currStyle, currStyleStates, state.state);
            UpdateGeometry(item, bbox, style);
            context.AddItemGeometry(item.id, bbox);
            result = ToggleButtonImpl(item.id, state, style, item.margin, ImVec2{ item.text.GetWidth(), item.text.GetHeight() }, renderer, io);
            if (!context.nestedContextStack.empty())
                RecordItemGeometry(item);
            break;
        }
        case glimmer::WT_Checkbox: {
            auto& state = context.GetState(item.id).state.checkbox;
            const auto& style = GetStyle(StyleStack, currStyle, currStyleStates, state.state);
            UpdateGeometry(item, bbox, style);
            context.AddItemGeometry(item.id, bbox);
            result = CheckboxImpl(item.id, state, style, item.margin, item.padding, renderer, io);
            if (!context.nestedContextStack.empty())
                RecordItemGeometry(item);
            break;
        }
        case WT_Spinner: {
            auto& state = context.GetState(item.id).state.spinner;
            const auto& style = GetStyle(StyleStack, currStyle, currStyleStates, state.state);
            UpdateGeometry(item, bbox, style);
            context.AddItemGeometry(item.id, bbox);
            result = SpinnerImpl(item.id, state, style, item.padding, io, renderer);
            if (!context.nestedContextStack.empty())
                RecordItemGeometry(item);
            break;
        }
        case glimmer::WT_Slider: {
            auto& state = context.GetState(item.id).state.slider;
            const auto& style = GetStyle(StyleStack, currStyle, currStyleStates, state.state);
            UpdateGeometry(item, bbox, style);
            context.AddItemGeometry(item.id, bbox);
            result = SliderImpl(item.id, state, style, item.border, renderer, io);
            if (!context.nestedContextStack.empty())
                RecordItemGeometry(item);
            break;
        }
        case glimmer::WT_TextInput: {
            auto& state = context.GetState(item.id).state.input;
            const auto& style = GetStyle(StyleStack, currStyle, currStyleStates, state.state);
            UpdateGeometry(item, bbox, style);
            context.AddItemGeometry(item.id, bbox);
            result = TextInputImpl(item.id, state, style, item.margin, item.content, renderer, io);
            if (!context.nestedContextStack.empty())
                RecordItemGeometry(item);
            break;
        }
        case glimmer::WT_DropDown: {
            auto& state = context.GetState(item.id).state.dropdown;
            const auto& style = GetStyle(StyleStack, currStyle, currStyleStates, state.state);
            UpdateGeometry(item, bbox, style);
            context.AddItemGeometry(item.id, bbox);
            result = DropDownImpl(item.id, state, style, item.margin, item.border, item.padding, item.content, item.text, renderer, io);
            if (!context.nestedContextStack.empty())
                RecordItemGeometry(item);
            break;
        }
        case glimmer::WT_ItemGrid: {
            auto& state = context.GetState(item.id).state.grid;
            const auto& style = GetStyle(StyleStack, currStyle, currStyleStates, state.state);
            UpdateGeometry(item, bbox, style);
            context.AddItemGeometry(item.id, bbox);
            result = ItemGridImpl(item.id, style, item.margin, item.border, item.padding, item.content, item.text, renderer, io);
            break;
        }
        case WT_Scrollable: {
            if (item.closing) EndScrollableImpl(item.id, renderer);
            else 
            {
                const auto& style = GetStyle(StyleStack, currStyle, currStyleStates, WS_Default);
                StartScrollableImpl(item.id, item.hscroll, item.vscroll, item.extent, style, item.border, item.content, renderer);
            }
            if (!context.nestedContextStack.empty())
                RecordItemGeometry(item);
            break;
        }
        case WT_TabBar: {
            const auto& style = GetStyle(StyleStack, currStyle, currStyleStates, WS_Default);
            UpdateGeometry(item, bbox, style);
            context.AddItemGeometry(item.id, bbox);
            context.currentTab = layout.tabbars[item.id];
            result = TabBarImpl(item.id, item.content, style, io, renderer);
            if (!context.nestedContextStack.empty())
                RecordItemGeometry(item);
            break;
        }
        default:
            break;
        }

        renderer.ResetClipRect();
        return result;
    }

    WidgetDrawResult EndLayout(int depth)
    {
        WidgetDrawResult result;
        ImRect geometry;
        auto& context = GetContext();
        
        while (depth > 0 && !context.layouts.empty())
        {
            auto& layout = context.layouts.top();

            if (context.layouts.size() == 1)
            {
#if GLIMMER_LAYOUT_ENGINE == GLIMMER_FLAT_LAYOUT_ENGINE

                AlignLayoutAxisItems(layout);
                AlignCrossAxisItems(layout, depth);

#elif GLIMMER_LAYOUT_ENGINE == GLIMMER_CLAY_LAYOUT_ENGINE

                Clay__CloseElement();
                auto renderCommands = Clay_EndLayout();
                auto widgetidx = 0;

#elif GLIMMER_LAYOUT_ENGINE == GLIMMER_YOGA_LAYOUT_ENGINE

                auto widgetidx = 0;
                YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);

#endif

                static StyleStackT StyleStack[WSI_Total];

                auto io = Config.platform->CurrentIO();
                auto popStyle = false;
                StyleDescriptor currStyle[WSI_Total];
                int32_t currStyleStates = 0;
                ImVec2 min{ FLT_MAX, FLT_MAX }, max;

                // Restore style states for context
                context.currStyleStates = layout.currStyleStates;
                for (auto idx = 0; idx < WSI_Total; ++idx)
                {
                    auto diff = context.StyleStack[idx].size() - layout.styleStartIdx[idx];
                    if (diff > 0) context.StyleStack[idx].pop(diff, true);

                    context.currStyle[idx] = layout.currstyle[idx];
                }

                for (auto idx = 0; idx < WSI_Total; ++idx) StyleStack[idx].clear(true);

                for (const auto [data, op] : layout.itemIndexes)
                {
                    switch (op)
                    {
                    case LayoutOps::AddWidget:
                    {
                        auto& item = context.layoutItems[(int16_t)data];

#if GLIMMER_LAYOUT_ENGINE == GLIMMER_CLAY_LAYOUT_ENGINE

                        auto& command = renderCommands.internalArray[widgetidx];
                        if (command.commandType == CLAY_RENDER_COMMAND_TYPE_CUSTOM)
                        {
                            ImRect bbox = { { command.boundingBox.x, command.boundingBox.y }, { command.boundingBox.x + command.boundingBox.width,
                                command.boundingBox.y + command.boundingBox.height } };
                            bbox.Translate(layout.geometry.Min);
                            item.margin = bbox;
                        }

                        ++widgetidx;

#elif GLIMMER_LAYOUT_ENGINE == GLIMMER_YOGA_LAYOUT_ENGINE
                        
                        auto child = children[widgetidx];
                        auto bbox = GetBoundingBox(child);
                        bbox.Translate(layout.geometry.Min);
                        item.margin = bbox;
                        ++widgetidx;

#endif
                        if (auto res = RenderWidget(layout, item, StyleStack, currStyle,
                            currStyleStates, io); res.event != WidgetEvent::None)
                            result = res;

                        min = ImMin(min, item.margin.Min);
                        max = ImMax(min, item.margin.Max);

                        if (popStyle)
                        {
                            popStyle = false;
                            for (auto idx = 0; idx < WSI_Total; ++idx)
                                if ((1 << idx) & context.currStyleStates)
                                    StyleStack[idx].pop(1, true);
                        }
                        break;
                    }
                    case LayoutOps::PushStyle:
                    {
                        auto state = data & 0xffffffff;
                        auto index = data >> 32;
                        StyleStack[state].push() = layout.styles[state][index];
                        break;
                    }
                    case LayoutOps::PopStyle:
                    {
                        auto states = data & 0xffffffff;
                        auto amount = data >> 32;
                        for (auto idx = 0; idx < WSI_Total; ++idx)
                            if ((1 << idx) & states)
                                StyleStack[idx].pop(amount, true);
                        break;
                    }
                    case LayoutOps::SetStyle:
                    {
                        auto state = data & 0xffffffff;
                        auto index = data >> 32;
                        currStyleStates |= (1 << state);
                        currStyle[state] = layout.styles[state][index];
                        popStyle = true;
                        break;
                    }
                    default:
                        break;
                    }
                }
 
                if (!(layout.fill & ExpandH))
                {
                    layout.geometry.Min.x = min.x - layout.spacing.x;
                    layout.geometry.Max.x = max.x + layout.spacing.x;
                }

                if (!(layout.fill & ExpandV))
                {
                    layout.geometry.Min.y = min.y - layout.spacing.y;
                    layout.geometry.Max.y = max.y + layout.spacing.y;
                }
                
                geometry = layout.geometry;
                context.AddItemGeometry(layout.id, geometry);

#if GLIMMER_LAYOUT_ENGINE == GLIMMER_YOGA_LAYOUT_ENGINE
                YGNodeFreeRecursive(root);
                children.clear();
#endif
            }

            --depth;
            layout.reset();
            context.layouts.pop(1, false);
            context.nestedContextStack.pop(1, true);
        }

        if (context.layouts.empty())
        {
            context.ResetLayoutData();
        }

        return result;
    }

#pragma endregion
}
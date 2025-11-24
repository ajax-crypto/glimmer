#include "layout.h"

#include "context.h"
#include "libs/inc/imgui/imgui.h"
#include "style.h"
#include "draw.h"
#include "types.h"
#include "utils.h"

#define GLIMMER_FLAT_ENGINE 0
#define GLIMMER_CLAY_ENGINE 1
#define GLIMMER_YOGA_ENGINE 2

#ifndef GLIMMER_FLEXBOX_ENGINE
#define GLIMMER_FLEXBOX_ENGINE GLIMMER_YOGA_ENGINE
#elif GLIMMER_FLEXBOX_ENGINE == GLIMMER_CLAY_ENGINE
#error "Clay Layout Engine integration requires fixes to work correctly"
#endif

#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_CLAY_ENGINE
#define CLAY_IMPLEMENTATION
#include "libs/inc/clay/clay.h"

Clay_Arena LayoutArena;
void* LayoutMemory = nullptr;
#endif

#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_YOGA_ENGINE
#include "libs/inc/yoga/Yoga.h"

struct YogaNodeTree
{
    YGNodeRef root = nullptr;
    std::vector<YGNodeRef> children;
};

static glimmer::Vector<YogaNodeTree, int16_t, 32> FlexLayoutItems;
static glimmer::Vector<YGNodeRef, int16_t> AllFlexItems;

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
    constexpr int32_t GridLayoutMask = 1 << 16;
    static Vector<GridLayoutItem, int16_t, 64> GridLayoutItems;

    WidgetDrawResult LabelImpl(int32_t id, const StyleDescriptor& style, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io, int32_t textflags);
    WidgetDrawResult ButtonImpl(int32_t id, const StyleDescriptor& style, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, const ImRect& prefix, IRenderer& renderer, const IODescriptor& io);
    WidgetDrawResult ToggleButtonImpl(int32_t id, ToggleButtonState& state, const StyleDescriptor& style, const ImRect& extent, ImVec2 textsz, IRenderer& renderer, const IODescriptor& io);
    WidgetDrawResult RadioButtonImpl(int32_t id, RadioButtonState& state, const StyleDescriptor& style, const ImRect& extent, IRenderer& renderer, const IODescriptor& io);
    WidgetDrawResult CheckboxImpl(int32_t id, CheckboxState& state, const StyleDescriptor& style, const ImRect& extent, const ImRect& padding, IRenderer& renderer, const IODescriptor& io);
    WidgetDrawResult SliderImpl(int32_t id, SliderState& state, const StyleDescriptor& style, const ImRect& extent, IRenderer& renderer, const IODescriptor& io);
    WidgetDrawResult RangeSliderImpl(int32_t id, RangeSliderState& state, const StyleDescriptor& style, const ImRect& extent, IRenderer& renderer, const IODescriptor& io);
    WidgetDrawResult SpinnerImpl(int32_t id, const SpinnerState& state, const StyleDescriptor& style, const ImRect& extent, const IODescriptor& io, IRenderer& renderer);
    WidgetDrawResult TextInputImpl(int32_t id, TextInputState& state, const StyleDescriptor& style, const ImRect& extent, const ImRect& content, 
        const ImRect& prefix, const ImRect& suffix, IRenderer& renderer, const IODescriptor& io);
    WidgetDrawResult DropDownImpl(int32_t id, DropDownState& state, const StyleDescriptor& style, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, const ImRect& prefix, IRenderer& renderer, const IODescriptor& io);
    WidgetDrawResult ItemGridImpl(int32_t id, const StyleDescriptor& style, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io);
    void StartScrollableImpl(int32_t id, int32_t flags, ImVec2 maxsz, const StyleDescriptor& style,
        const ImRect& border, const ImRect& content, IRenderer& renderer);
    void EndScrollableImpl(int32_t id, IRenderer& renderer);
    WidgetDrawResult TabBarImpl(int32_t id, const ImRect& content, const StyleDescriptor& style, const IODescriptor& io,
        IRenderer& renderer);
    void RecordItemGeometry(const LayoutItemDescriptor& layoutItem);
    void CopyStyle(const StyleDescriptor& src, StyleDescriptor& dest);

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
        if (!layout.addedOffset && layout.insideContainer) layout.addedOffset = true;
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
        if (!layout.addedOffset && layout.insideContainer) layout.addedOffset = true;
    }

    void Move(ImVec2 amount, int32_t direction)
    {
        auto& context = GetContext();
        if (!context.layouts.empty()) return;
        if (direction & ToLeft) amount.x = -amount.x;
        if (direction & ToTop) amount.y = -amount.y;
        auto& layout = context.adhocLayout.top();
        layout.nextpos += amount;
        if (!layout.addedOffset && layout.insideContainer) layout.addedOffset = true;
    }

    void Move(ImVec2 pos)
    {
        auto& context = GetContext();
        if (!context.layouts.empty()) return;
        auto& layout = context.adhocLayout.top();
        layout.nextpos = pos;
        if (!layout.addedOffset && layout.insideContainer) layout.addedOffset = true;
    }

    void AddSpacing(ImVec2 spacing)
    {
        auto& context = GetContext();
        if (!context.layouts.empty()) return;
        auto& layout = context.adhocLayout.top();
        layout.nextpos += spacing;
        if (!layout.addedOffset && layout.insideContainer) layout.addedOffset = true;
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
            layoutItem.border.Max.x = layoutItem.margin.Max.x - style.margin.right;
            layoutItem.padding.Max.x = layoutItem.border.Max.x - style.border.right.thickness;
            layoutItem.content.Max.x = layoutItem.padding.Max.x - style.padding.right;
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
            layoutItem.border.Max.y = layoutItem.margin.Max.y - style.margin.bottom;
            layoutItem.padding.Max.y = layoutItem.border.Max.y - style.border.bottom.thickness;
            layoutItem.content.Max.y = layoutItem.padding.Max.y - style.padding.bottom;
        }

        if (style.dimension.x > 0.f)
        {
            // TODO: Implement multiple box sizing modes?
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

        if (style.dimension.y > 0.f)
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

    static void ReserveSpaceForScrollBars(WidgetContextData& context, LayoutItemDescriptor& layoutItem)
    {
        if (layoutItem.wtype == WT_Scrollable)
        {
            auto type = context.GetState(layoutItem.id).state.scroll.type;
            if (type & ST_Always_H)
            {
                layoutItem.border.Max.x -= Config.scrollbarSz;
                layoutItem.padding.Max.x -= Config.scrollbarSz;
                layoutItem.content.Max.x -= Config.scrollbarSz;
            }

            if (type & ST_Always_V)
            {
                layoutItem.border.Max.y -= Config.scrollbarSz;
                layoutItem.padding.Max.y -= Config.scrollbarSz;
                layoutItem.content.Max.y -= Config.scrollbarSz;
            }
        }
    }

    void AddExtent(LayoutItemDescriptor& layoutItem, const StyleDescriptor& style, const NeighborWidgets& neighbors,
        float width, float height)
    {
        auto& context = GetContext();
        auto totalsz = context.MaximumExtent();
        auto nextpos = !context.layouts.empty() ? context.layouts.top().nextpos : context.NextAdHocPos();
        AddDefaultDirection(layoutItem);

        if (width <= 0.f) width = clamp(totalsz.x - nextpos.x, style.mindim.x, style.maxdim.x);
        if (height <= 0.f) height = clamp(totalsz.y - nextpos.y, style.mindim.y, style.maxdim.y);

        ComputeExtent(layoutItem, nextpos, style, neighbors, width, height);
        ReserveSpaceForScrollBars(context, layoutItem);
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
        ReserveSpaceForScrollBars(context, layoutItem);
    }

#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_FLAT_ENGINE
    static float GetTotalSpacing(LayoutBuilder& layout, Direction dir)
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

    static void AlignLayoutAxisItems(LayoutBuilder& layout)
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

            if ((layout.alignment & AlignHCenter) && (layout.fill & FD_Horizontal))
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
            else if ((layout.alignment & AlignJustify) && (layout.fill & FD_Horizontal))
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

            if ((layout.alignment & AlignVCenter) && (layout.fill & FD_Vertical))
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
            else if ((layout.alignment & AlignJustify) && (layout.fill & FD_Vertical))
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

    static void AlignCrossAxisItems(LayoutBuilder& layout, int depth)
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

            if ((layout.alignment & AlignVCenter) && (layout.fill & FD_Vertical))
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
            else if ((layout.alignment & AlignJustify) && (layout.fill & FD_Vertical))
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
            else if ((layout.alignment & AlignBottom) && (layout.fill & FD_Vertical))
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

            if ((layout.alignment & AlignHCenter) && (layout.fill & FD_Horizontal))
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
            else if ((layout.alignment & AlignJustify) && (layout.fill & FD_Horizontal))
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
            else if ((layout.alignment & AlignRight) && (layout.fill & FD_Vertical))
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

    void AddItemToLayout(LayoutBuilder& layout, LayoutItemDescriptor& item, const StyleDescriptor& style)
    {
        layout.itemIndexes.emplace_back(GetContext().layoutItems.size(), LayoutOps::AddWidget);

#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_FLAT_ENGINE
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
        default:
            break;
        }

        layout.extent.Min = ImMin(layout.extent.Min, item.margin.Min);
        layout.extent.Max = ImMax(layout.extent.Max, item.margin.Max);

#elif GLIMMER_FLEXBOX_ENGINE == GLIMMER_CLAY_ENGINE

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

#elif GLIMMER_FLEXBOX_ENGINE == GLIMMER_YOGA_ENGINE
        
        if (layout.type != Layout::Grid)  
        {
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

            // Record child items for current root, and record all child items across all flex layouts
            auto& top = FlexLayoutItems.back();
            YGNodeInsertChild(top.root, child, YGNodeGetChildCount(top.root));
            top.children.emplace_back(child);
            item.implData = child;
            AllFlexItems.push_back(child);

            // Record this widget for rendering once geometry is determined
            auto wt = (WidgetType)(item.id >> WidgetTypeBits);
            if (wt != WT_Layout)
                GetContext().RecordForReplay((AllFlexItems.size() - 1), LayoutOps::AddWidget);
        }
#endif

        if (layout.type == Layout::Grid)
        {
            auto& griditem = GridLayoutItems.emplace_back();
            griditem.maxdim = item.margin.GetSize();
            griditem.row = layout.currow; griditem.col = layout.currcol;
            griditem.rowspan = layout.currspan.first;
            griditem.colspan = layout.currspan.second;
            griditem.index = GetContext().layoutItems.size();
            griditem.alignment = item.sizing;
            griditem.bbox.Min = ImVec2{};
            griditem.bbox.Max = item.margin.GetSize();
            layout.griditems.emplace_back((int16_t)(GridLayoutItems.size() - 1));
            
            if (layout.gpmethod == ItemGridPopulateMethod::ByRows)
            {
                layout.currcol += griditem.colspan;
                layout.maxdim.y = std::max(layout.maxdim.y, griditem.maxdim.y);
                if (layout.currcol >= layout.gridsz.second)
                {
                    layout.rows.emplace_back(layout.maxdim);
                    layout.maxdim = ImVec2{};
                    layout.currcol = 0;
                    layout.currow++;
                }
            }
            else
            {
                layout.currow += griditem.rowspan;
                layout.maxdim.x = std::max(layout.maxdim.x, griditem.maxdim.x);
                if (layout.currow >= layout.gridsz.first)
                {
                    layout.cols.emplace_back(layout.maxdim);
                    layout.maxdim = ImVec2{};
                    layout.currow = 0;
                    layout.currcol++;
                }
            }

            // Record this widget for rendering once geometry is determined
            item.implData = reinterpret_cast<void*>(GridLayoutItems.size() - 1);
            auto wt = (WidgetType)(item.id >> WidgetTypeBits);
            if (wt != WT_Layout)
                GetContext().RecordForReplay((GridLayoutItems.size() - 1) | GridLayoutMask, LayoutOps::AddWidget);
        }

        auto& context = GetContext();
        context.layoutItems.push_back(item);

        if (!context.spans.empty() && (context.spans.top() & OnlyOnce) != 0) context.spans.pop(1, true);

#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_FLAT_ENGINE
        if (layout.from == -1) layout.from = layout.to = (int16_t)(context.layoutItems.size() - 1);
        else layout.to = (int16_t)(context.layoutItems.size() - 1);
        layout.itemidx = layout.to;
#endif
    }

    static ImRect GetAvailableSpace(ImVec2 nextpos, const NeighborWidgets& neighbors)
    {
        ImRect available;
        auto& context = GetContext();
        auto maxabs = context.MaximumExtent();

        available.Min.y = nextpos.y;
        available.Max.y = neighbors.bottom == -1 ? maxabs.y : context.GetGeometry(neighbors.bottom).Min.y;
        available.Min.x = nextpos.x;
        available.Max.x = neighbors.right == -1 ? maxabs.x : context.GetGeometry(neighbors.right).Min.x;

        return available;
    }

    static bool IsLayoutDependentOnContent(const LayoutBuilder& layout)
    {
        return layout.fill != 0;
    }

    static void AddLayoutAsChildItem(WidgetContextData& context, LayoutBuilder& layout, const ImRect& available)
    {
        if (context.layouts.size() > 1)
        {
            auto& parent = context.layouts.top(1);
            LayoutItemDescriptor item;
            StyleDescriptor style;
            item.id = layout.id;
            item.margin = available;
            parent.itemIndexes.emplace_back(context.layoutItems.size(), LayoutOps::AddLayout);
            layout.itemidx = context.layoutItems.size();
            AddItemToLayout(parent, item, style);
        }
    }

    ImRect BeginFlexLayout(Direction dir, int32_t geometry, bool wrap, ImVec2 spacing, 
        ImVec2 size, const NeighborWidgets& neighbors)
    {
        auto& context = GetContext();

        // Only top-level layouts can have neighbors
        assert(context.layouts.size() == 0 || (neighbors.bottom == neighbors.top && neighbors.top == neighbors.left &&
            neighbors.left == neighbors.right && neighbors.right == -1));
        // No expansion if nested layout, nested layout's size is implicit, or explicit from CSS
        assert(context.layouts.size() == 0 || (!(geometry & ExpandH) && !(geometry & ExpandV)));

#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_FLAT_ENGINE
        assert(context.layouts.empty()); // No nested layout is supported
#endif

        auto& layout = context.layouts.push();
        auto& el = context.nestedContextStack.push();
        el.source = NestedContextSourceType::Layout;
        layout.id = (WT_Layout << 16) | context.maxids[WT_Layout];
        context.maxids[WT_Layout]++;

        layout.type = dir == DIR_Horizontal ? Layout::Horizontal : Layout::Vertical;
        layout.alignment = geometry & ~ExpandAll;
        layout.spacing = spacing;
        layout.size = size;
        layout.type == Layout::Horizontal ? (layout.hofmode = wrap ? OverflowMode::Wrap : OverflowMode::Scroll) :
            (layout.vofmode = wrap ? OverflowMode::Wrap : OverflowMode::Scroll);
        if (geometry & ExpandH) layout.fill = FD_Horizontal;
        if (geometry & ExpandV) layout.fill |= FD_Vertical;
        if (size.x > 0.f) layout.alignment |= ExplicitH;
        if (size.y > 0.f) layout.alignment |= ExplicitV;

        // Record style stack states for context, will be restored in EndLayout()
        for (auto idx = 0; idx < WSI_Total; ++idx)
            layout.styleStartIdx[idx] = context.StyleStack[idx].size() - 1;

        auto nextpos = context.layouts.size() == 1 ? context.NextAdHocPos() : ImVec2{};
        ImRect available = context.layouts.size() == 1 ? GetAvailableSpace(nextpos, neighbors) : ImRect{};
        if (size.x > 0.f) available.Max.x = available.Min.x + size.x;
        if (size.y > 0.f) available.Max.y = available.Min.x + size.y;

#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_CLAY_ENGINE

        if (context.layouts.size() == 1)
        {
            uint64_t totalMemorySize = Clay_MinMemorySize();
            if (LayoutMemory == nullptr) LayoutMemory = malloc(totalMemorySize);
            LayoutArena = Clay_CreateArenaWithCapacityAndMemory(totalMemorySize, LayoutMemory);
            Clay_Initialize(LayoutArena, { available.GetWidth(), available.GetHeight() }, { NULL });
            Clay_BeginLayout();
        }

        Clay__OpenElement();
        Clay_ElementDeclaration decl;
        decl.layout.layoutDirection = layout.type == Layout::Horizontal ? Clay_LayoutDirection::CLAY_LEFT_TO_RIGHT : Clay_LayoutDirection::CLAY_TOP_TO_BOTTOM;
        decl.layout.childGap = layout.spacing.x;
        decl.layout.childAlignment.x = alignment & AlignHCenter ? Clay_LayoutAlignmentX::CLAY_ALIGN_X_CENTER :
            alignment & AlignRight ? Clay_LayoutAlignmentX::CLAY_ALIGN_X_RIGHT : Clay_LayoutAlignmentX::CLAY_ALIGN_X_LEFT;
        decl.layout.childAlignment.y = alignment & AlignVCenter ? Clay_LayoutAlignmentY::CLAY_ALIGN_Y_BOTTOM :
            alignment & AlignBottom ? Clay_LayoutAlignmentY::CLAY_ALIGN_Y_BOTTOM : Clay_LayoutAlignmentY::CLAY_ALIGN_Y_TOP;

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
            auto wtype = (WidgetType)(id >> WidgetTypeBits);

            Clay_SetExternalScrollHandlingEnabled(true);
            Clay_SetQueryScrollOffsetFunction([](uint32_t id, void* data) {
                context.ScrollRegion(id);
            });
        }*/

#elif GLIMMER_FLEXBOX_ENGINE == GLIMMER_YOGA_ENGINE

        if (layout.type != Layout::Grid)
        {
            auto& top = FlexLayoutItems.emplace_back();
            top.root = YGNodeNew();
            if ((layout.fill & FD_Horizontal) && (available.Max.x != FLT_MAX) && (available.Max.x > 0.f)) 
                YGNodeStyleSetWidth(top.root, available.GetWidth() - (2.f * layout.spacing.x));
            if ((layout.fill & FD_Vertical) && (available.Max.y != FLT_MAX) && (available.Max.y > 0.f))
                YGNodeStyleSetHeight(top.root, available.GetHeight() - (2.f * layout.spacing.y));
            YGNodeStyleSetFlexDirection(top.root, layout.type == Layout::Horizontal ? YGFlexDirectionRow : YGFlexDirectionColumn);
            YGNodeStyleSetFlexWrap(top.root, wrap ? YGWrapWrap : YGWrapNoWrap);
            YGNodeStyleSetPosition(top.root, YGEdgeLeft, 0.f);
            YGNodeStyleSetPosition(top.root, YGEdgeTop, 0.f);
            YGNodeStyleSetGap(top.root, YGGutterRow, spacing.x);
            YGNodeStyleSetGap(top.root, YGGutterColumn, spacing.y);

            if (layout.type == Layout::Horizontal)
            {
                // Main axis alignment
                if (geometry & AlignRight) YGNodeStyleSetJustifyContent(top.root, YGJustifyFlexEnd);
                else if (geometry & AlignHCenter) YGNodeStyleSetJustifyContent(top.root, YGJustifyCenter);
                else if (geometry & AlignJustify) YGNodeStyleSetJustifyContent(top.root, YGJustifySpaceAround);
                else YGNodeStyleSetJustifyContent(top.root, YGJustifyFlexStart);

                // Cross axis alignment
                if (geometry & AlignBottom) YGNodeStyleSetAlignItems(top.root, YGAlignFlexEnd);
                else if (geometry & AlignVCenter) YGNodeStyleSetAlignItems(top.root, YGAlignCenter);
                else YGNodeStyleSetAlignItems(top.root, YGAlignFlexStart);
            }
            else
            {
                // Main axis alignment
                if (geometry & AlignBottom) YGNodeStyleSetJustifyContent(top.root, YGJustifyFlexEnd);
                else if (geometry & AlignVCenter) YGNodeStyleSetJustifyContent(top.root, YGJustifyCenter);
                else YGNodeStyleSetJustifyContent(top.root, YGJustifyFlexStart);

                // Cross axis alignment
                if (geometry & AlignRight) YGNodeStyleSetAlignItems(top.root, YGAlignFlexEnd);
                else if (geometry & AlignHCenter) YGNodeStyleSetAlignItems(top.root, YGAlignCenter);
                else YGNodeStyleSetAlignItems(top.root, YGAlignFlexStart);
            }
        }

#endif
        
        // If current layout is nested layout, create a layout item and add it
        // to parent layout's child items
        AddLayoutAsChildItem(context, layout, available);

        layout.available = available;
        layout.startpos = nextpos;
        layout.nextpos = ImVec2{};
        layout.extent.Min = { FLT_MAX, FLT_MAX };
        layout.geometry = ImRect{};
        return layout.geometry;
    }

    ImRect BeginGridLayout(int rows, int cols, GridLayoutDirection dir, int32_t geometry, const std::initializer_list<float>& rowExtents,
        const std::initializer_list<float>& colExtents, ImVec2 spacing, ImVec2 size, const NeighborWidgets& neighbors)
    {
        auto& context = GetContext();

        // Only top-level layouts can have neighbors
        assert(context.layouts.size() == 0 || (neighbors.bottom == neighbors.top && neighbors.top == neighbors.left && 
            neighbors.left == neighbors.right && neighbors.right == -1));
        // No expansion if nested layout, nested layout's size is implicit, or explicit from CSS
        assert(context.layouts.size() == 0 || (!(geometry & ExpandH) && !(geometry & ExpandV)));
        // Row/Column extents only apply for top-level layouts or nested layouts with non-zero explicit size
        assert(context.layouts.size() == 0 || ((size.x == 0.f && rowExtents.size() == 0) && 
            (size.y == 0.f && colExtents.size() == 0)));
        // For row-wise addition of widgets, columns must be specified to wrap (And vice-versa)
        assert((dir == GridLayoutDirection::ByRows && cols > 0) || (dir == GridLayoutDirection::ByColumns && rows > 0));

        auto& layout = context.layouts.push();
        auto& el = context.nestedContextStack.push();
        el.source = NestedContextSourceType::Layout;
        layout.id = (WT_Layout << 16) | context.maxids[WT_Layout];
        context.maxids[WT_Layout]++;

        layout.type = Layout::Grid;
        layout.gpmethod = dir;
        layout.alignment = geometry & ~ExpandAll;
        layout.spacing = spacing;
        layout.size = size;
        layout.gridsz = std::make_pair(rows, cols);
        if (geometry & ExpandH) layout.fill = FD_Horizontal;
        if (geometry & ExpandV) layout.fill |= FD_Vertical;
        if (size.x > 0.f) layout.alignment |= ExplicitH;
        if (size.y > 0.f) layout.alignment |= ExplicitV;

        // Record style stack states for context, will be restored in EndLayout()
        for (auto idx = 0; idx < WSI_Total; ++idx)
            layout.styleStartIdx[idx] = context.StyleStack[idx].size() - 1;

        auto nextpos = context.layouts.size() == 1 ? context.NextAdHocPos() : ImVec2{};
        ImRect available = context.layouts.size() == 1 || (geometry & ExpandAll) != 0 ? 
            GetAvailableSpace(nextpos, neighbors) : ImRect{};
        if (size.x > 0.f) available.Max.x = available.Min.x + size.x;
        if (size.y > 0.f) available.Max.y = available.Min.x + size.y;
        auto sz = available.GetSize();

#ifdef _DEBUG
        auto total = 0.f;

        if (rowExtents.size() > 0)
        {
            for (auto rowext : rowExtents)
            {
                layout.rows.emplace_back(0.f, sz.y * rowext);
                total += rowext;
            }

            assert(total == 1.f);
            assert(rows < 0 || rowExtents.size() == rows);
        }
        
        total = 0.f;

        if (colExtents.size() > 0)
        {
            for (auto colext : colExtents)
            {
                layout.cols.emplace_back(sz.x * colext, 0.f);
                total += colext;
            }

            assert(total == 1.f);
            assert(cols < 0 || colExtents.size() == cols);
        }
#else
        assert(rows < 0 || rowExtents.size() == rows);
        for (auto rowext : rowExtents)
            layout.rows.emplace_back(0.f, sz.y * rowext);

        assert(cols < 0 || colExtents.size() == cols);
        for (auto colext : colExtents)
            layout.cols.emplace_back(sz.x * colext, 0.f);
#endif

        // If current layout is nested layout, create a layout item and add it
        // to parent layout's child items
        AddLayoutAsChildItem(context, layout, available);

        layout.available = available;
        layout.startpos = nextpos;
        layout.nextpos = ImVec2{};
        layout.extent.Min = { FLT_MAX, FLT_MAX };
        layout.currow = layout.currcol = 0;
        layout.geometry = ImRect{};
        if (rows > 0 && cols > 0) GridLayoutItems.expand(rows * cols, true);
        return layout.geometry;
    }

    ImRect BeginLayout(std::string_view desc, const NeighborWidgets& neighbors)
    {
        // TODO: Implement layout CSS parsing
        return ImRect{};
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
            else if (layout.type == Layout::Grid)
            {
                layout.currow++;
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
            else if (layout.type == Layout::Grid)
            {
                layout.currcol++;
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

        auto pw = item.prefix.GetWidth();
        item.prefix.Min.x = item.padding.Min.x + style.padding.left;
        item.prefix.Max.x = item.prefix.Min.x + pw;

        auto sw = item.suffix.GetWidth();
        item.suffix.Max.x = item.padding.Max.x - style.padding.right;
        item.suffix.Min.x = item.suffix.Max.x - sw;

        item.content.Min.x = item.prefix.Max.x;
        item.content.Max.x = item.suffix.Min.x;

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

        auto ph = item.prefix.GetHeight();
        auto vdiff = std::max((item.content.GetHeight() - ph) * 0.5f, 0.f);
        item.prefix.Min.y = item.content.Min.y + vdiff;
        item.prefix.Max.y = item.prefix.Min.y + ph;

        auto sh = item.suffix.GetHeight();
        vdiff = std::max((item.content.GetHeight() - sh) * 0.5f, 0.f);
        item.suffix.Min.y = item.content.Min.y + vdiff;
        item.suffix.Max.y = item.suffix.Min.y + sh;

        auto texth = item.text.GetHeight();
        item.text.Min.y = item.content.Min.y;
        item.text.Max.y = item.text.Min.y + texth;
    }

    static WidgetDrawResult RenderWidget(const LayoutBuilder& layout, LayoutItemDescriptor& item, StyleStackT* StyleStack,
        const IODescriptor& io)
    {
        WidgetDrawResult result;
        auto& context = GetContext();
        auto bbox = item.margin;
        auto wtype = (WidgetType)(item.id >> WidgetTypeBits);
        auto& renderer = context.GetRenderer();
        renderer.SetClipRect(bbox.Min, bbox.Max);

        switch (wtype)
        {
        case glimmer::WT_Label: {
            auto& state = context.GetState(item.id).state.label;
            auto flags = ToTextFlags(state.type);
            const auto& style = GetStyle(context, item.id, StyleStack, state.state);
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
            const auto& style = GetStyle(context, item.id, StyleStack, state.state);
            UpdateGeometry(item, bbox, style);
            context.AddItemGeometry(item.id, bbox);
            result = ButtonImpl(item.id, style, item.margin, item.border, item.padding, item.content, item.text, item.prefix, renderer, io);
            if (!context.nestedContextStack.empty())
                RecordItemGeometry(item);
            break;
        }
        case glimmer::WT_RadioButton: {
            auto& state = context.GetState(item.id).state.radio;
            const auto& style = GetStyle(context, item.id, StyleStack, state.state);
            UpdateGeometry(item, bbox, style);
            context.AddItemGeometry(item.id, bbox);
            result = RadioButtonImpl(item.id, state, style, item.margin, renderer, io);
            if (!context.nestedContextStack.empty())
                RecordItemGeometry(item);
            break;
        }
        case glimmer::WT_ToggleButton: {
            auto& state = context.GetState(item.id).state.toggle;
            const auto& style = GetStyle(context, item.id, StyleStack, state.state);
            UpdateGeometry(item, bbox, style);
            context.AddItemGeometry(item.id, bbox);
            result = ToggleButtonImpl(item.id, state, style, item.margin, ImVec2{ item.text.GetWidth(), item.text.GetHeight() }, renderer, io);
            if (!context.nestedContextStack.empty())
                RecordItemGeometry(item);
            break;
        }
        case glimmer::WT_Checkbox: {
            auto& state = context.GetState(item.id).state.checkbox;
            const auto& style = GetStyle(context, item.id, StyleStack, state.state);
            UpdateGeometry(item, bbox, style);
            context.AddItemGeometry(item.id, bbox);
            result = CheckboxImpl(item.id, state, style, item.margin, item.padding, renderer, io);
            if (!context.nestedContextStack.empty())
                RecordItemGeometry(item);
            break;
        }
        case WT_Spinner: {
            auto& state = context.GetState(item.id).state.spinner;
            const auto& style = GetStyle(context, item.id, StyleStack, state.state);
            UpdateGeometry(item, bbox, style);
            context.AddItemGeometry(item.id, bbox);
            result = SpinnerImpl(item.id, state, style, item.padding, io, renderer);
            if (!context.nestedContextStack.empty())
                RecordItemGeometry(item);
            break;
        }
        case glimmer::WT_Slider: {
            auto& state = context.GetState(item.id).state.slider;
            const auto& style = GetStyle(context, item.id, StyleStack, state.state);
            UpdateGeometry(item, bbox, style);
            context.AddItemGeometry(item.id, bbox);
            result = SliderImpl(item.id, state, style, item.border, renderer, io);
            if (!context.nestedContextStack.empty())
                RecordItemGeometry(item);
            break;
        }
        case glimmer::WT_RangeSlider: {
            auto& state = context.GetState(item.id).state.rangeSlider;
            const auto& style = GetStyle(context, item.id, StyleStack, state.state);
            UpdateGeometry(item, bbox, style);
            context.AddItemGeometry(item.id, bbox);
            result = RangeSliderImpl(item.id, state, style, item.border, renderer, io);
            if (!context.nestedContextStack.empty())
                RecordItemGeometry(item);
            break;
        }
        case glimmer::WT_TextInput: {
            auto& state = context.GetState(item.id).state.input;
            const auto& style = GetStyle(context, item.id, StyleStack, state.state);
            UpdateGeometry(item, bbox, style);
            context.AddItemGeometry(item.id, bbox);
            result = TextInputImpl(item.id, state, style, item.margin, item.content, item.prefix, item.suffix, renderer, io);
            if (!context.nestedContextStack.empty())
                RecordItemGeometry(item);
            break;
        }
        case glimmer::WT_DropDown: {
            auto& state = context.GetState(item.id).state.dropdown;
            const auto& style = GetStyle(context, item.id, StyleStack, state.state);
            UpdateGeometry(item, bbox, style);
            context.AddItemGeometry(item.id, bbox);
            result = DropDownImpl(item.id, state, style, item.margin, item.border, item.padding, item.content, item.text, item.prefix, renderer, io);
            if (!context.nestedContextStack.empty())
                RecordItemGeometry(item);
            break;
        }
        case glimmer::WT_ItemGrid: {
            auto& state = context.GetState(item.id).state.grid;
            const auto& style = GetStyle(context, item.id, StyleStack, state.state);
            UpdateGeometry(item, bbox, style);
            context.AddItemGeometry(item.id, bbox);
            result = ItemGridImpl(item.id, style, item.margin, item.border, item.padding, item.content, item.text, renderer, io);
            break;
        }
        case WT_Scrollable: {
            /*if (item.closing) EndScrollableImpl(item.id, renderer);
            else 
            {
                const auto& style = GetStyle(StyleStack, WS_Default);
                StartScrollableImpl(item.id, item.hscroll, item.vscroll, item.extent, style, item.border, item.content, renderer);
            }
            if (!context.nestedContextStack.empty())
                RecordItemGeometry(item);*/
            // TODO: Fix this
            break;
        }
        case WT_TabBar: {
            auto& state = context.TabBarState(item.id);
            const auto style = context.GetStyle(WS_Default, item.id);
            UpdateGeometry(item, bbox, style);
            result = TabBarImpl(item.id, item.margin, style, io, renderer);
            if (result.event != WidgetEvent::Clicked) result.tabidx = state.current;
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

    static ImVec2 GetTotalSize(GridLayoutItem& item, const LayoutBuilder& layout, int currow, int currcol)
    {
        auto totalw = 0.f;
        for (auto col = 0; col < item.colspan; ++col)
            totalw += layout.cols[currcol + col].x;
        totalw += (float)(item.colspan - 1) * layout.spacing.x;

        auto totalh = 0.f;
        for (auto row = 0; row < item.rowspan; ++row)
            totalh += layout.rows[currow + row].y;
        totalh += (float)(item.rowspan - 1) * layout.spacing.y;

        return { totalw, totalh };
    }

    static void HAlignItemInGridCell(GridLayoutItem& item, const LayoutBuilder& layout, ImVec2 currpos, 
        float totalw)
    {
        if (item.alignment & AlignRight)
            item.bbox.Min.x = currpos.x + (totalw - item.maxdim.x);
        else if (item.alignment & AlignHCenter)
            item.bbox.Min.x = currpos.x + ((totalw - item.maxdim.x) * 0.5f);
        else
            item.bbox.Min.x = currpos.x;
    }

    static void VAlignItemInGridCell(GridLayoutItem& item, const LayoutBuilder& layout, ImVec2 currpos,
        float totalh)
    {
        if (item.alignment & AlignBottom)
            item.bbox.Min.y = currpos.y + (totalh - item.maxdim.y);
        else if (item.alignment & AlignVCenter)
            item.bbox.Min.y = currpos.y + ((totalh - item.maxdim.y) * 0.5f);
        else
            item.bbox.Min.y = currpos.y;
    }

    static void AlignItemInGridCell(GridLayoutItem& item, const LayoutBuilder& layout, ImVec2 currpos, 
        float totalw, float totalh)
    {
        if (item.alignment & ExpandH)
        {
            item.bbox.Min.x = currpos.x;
            item.bbox.Max.x = totalw + item.bbox.Min.x;
        }
        else
        {
            auto width = (item.alignment & ShrinkH) ? std::min(item.bbox.GetWidth(), totalw) : item.bbox.GetWidth();
            HAlignItemInGridCell(item, layout, currpos, totalw);
            item.bbox.Max.x = item.bbox.Min.x + width;
        }

        if (item.alignment & ExpandV)
        {
            item.bbox.Min.y = currpos.y;
            item.bbox.Max.y = item.bbox.Min.y + totalh;
        }
        else
        {
            auto height = (item.alignment & ShrinkV) ? std::min(item.bbox.GetHeight(), totalh) : item.bbox.GetHeight();
            VAlignItemInGridCell(item, layout, currpos, totalh);
            item.bbox.Max.y = item.bbox.Min.y + height;
        }
    }

    static void PerformGridLayout(LayoutBuilder& layout)
    {
        auto currow = 0, currcol = 0;
        ImVec2 currpos = layout.geometry.Min + layout.spacing;

        if (layout.gpmethod == ItemGridPopulateMethod::ByRows)
        {
            if (layout.cols.empty())
            {
                Vector<float, int16_t> colmaxs{ (int16_t)layout.gridsz.second, 0.f };

                if (layout.fill & FD_Horizontal)
                {
                    auto cellw = layout.available.GetWidth() / (float)layout.gridsz.second;

                    for (auto idx : layout.griditems)
                    {
                        auto& item = GridLayoutItems[idx];
                        if (item.colspan == 1)
                            colmaxs[item.col] = cellw;
                    }
                }
                else
                {
                    for (auto idx : layout.griditems)
                    {
                        auto& item = GridLayoutItems[idx];
                        if (item.colspan == 1)
                            colmaxs[item.col] = std::max(colmaxs[item.col], item.maxdim.x);
                    }
                }

                for (auto cidx = 0; cidx < colmaxs.size(); ++cidx)
                    layout.cols.emplace_back(colmaxs[cidx], 0.f);
            }

            for (auto idx : layout.griditems)
            {
                auto& item = GridLayoutItems[idx];
                if (item.row > currow)
                {
                    currpos.y += layout.rows[currow].y + layout.spacing.y;
                    currpos.x = layout.geometry.Min.x + layout.spacing.x;

                    for (auto col = 0; col < item.col; ++col)
                        currpos.x += layout.cols[col].x + layout.spacing.x;

                    currow = item.row;
                    currcol = 0;
                }
                
                if (item.row == currow)
                {
                    auto [totalw, totalh] = GetTotalSize(item, layout, currow, currcol);
                    AlignItemInGridCell(item, layout, currpos, totalw, totalh);
                    currpos.x += totalw + layout.spacing.x;
                    currcol += item.colspan;

                }
                else assert(false);
            }
        }
        else
        {
            if (layout.rows.empty())
            {
                Vector<float, int16_t> colmaxs{ (int16_t)layout.gridsz.second, 0.f };

                if (layout.fill & FD_Vertical)
                {
                    auto cellh = layout.available.GetHeight() / (float)layout.gridsz.first;

                    for (auto idx : layout.griditems)
                    {
                        auto& item = GridLayoutItems[idx];
                        if (item.rowspan == 1)
                            colmaxs[item.row] = cellh;
                    }
                }
                else
                {
                    for (auto idx : layout.griditems)
                    {
                        auto& item = GridLayoutItems[idx];
                        if (item.rowspan == 1)
                            colmaxs[item.row] = std::max(colmaxs[item.row], item.maxdim.y);
                    }
                }

                for (auto cidx = 0; cidx < colmaxs.size(); ++cidx)
                    layout.rows.emplace_back(0.f, colmaxs[cidx]);
            }

            for (auto idx : layout.griditems)
            {
                auto& item = GridLayoutItems[idx];
                if (item.col > currcol)
                {
                    currpos.x += layout.cols[currcol].x + layout.spacing.x;
                    currpos.y = layout.geometry.Min.y + layout.spacing.y;

                    for (auto row = 0; row < item.row; ++row)
                        currpos.y += layout.cols[row].y + layout.spacing.y;

                    currcol = item.col;
                    currow = 0;
                }
                
                if (item.col == currcol)
                {
                    auto [totalw, totalh] = GetTotalSize(item, layout, currow, currcol);
                    AlignItemInGridCell(item, layout, currpos, totalw, totalh);
                    currpos.y += totalh + layout.spacing.y;
                    currow += item.rowspan;
                }
                else assert(false);
            }
        }
    }

    static void ComputeLayoutGeometry(WidgetContextData& context, LayoutBuilder& layout)
    {
        // Execute layout algorithm and compute layout item geometry in layout local
        // coordinate starting at layout.spacing
        if (layout.type != Layout::Grid)
        {
#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_FLAT_ENGINE

            AlignLayoutAxisItems(layout);
            AlignCrossAxisItems(layout, depth);

#elif GLIMMER_FLEXBOX_ENGINE == GLIMMER_CLAY_ENGINE

            Clay__CloseElement();
            auto renderCommands = Clay_EndLayout();
            auto widgetidx = 0;

#elif GLIMMER_FLEXBOX_ENGINE == GLIMMER_YOGA_ENGINE

            YGNodeCalculateLayout(FlexLayoutItems.back().root, YGUndefined, YGUndefined, YGDirectionLTR);

#endif
        }
        else PerformGridLayout(layout);

        ImVec2 min{ FLT_MAX, FLT_MAX }, max;
        auto widgetidx = 0;

        // Loop through all layout items, and capture min/max coordinates
        // to determine the occupied dimension i.e. implicit dimension
        // NOTE: This needs to be done even if explicit dimension is specified
        //       to correctly align layout items w.r.t specified dimension i.e.
        //       x- or y-axis centering.
        for (const auto [data, op] : layout.itemIndexes)
        {
            switch (op)
            {
            case LayoutOps::AddWidget:
            {
                ImRect bbox;

                if (layout.type != Layout::Grid)
                {
#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_CLAY_ENGINE

                    auto& command = renderCommands.internalArray[widgetidx];
                    if (command.commandType == CLAY_RENDER_COMMAND_TYPE_CUSTOM)
                    {
                        ImRect bbox = { { command.boundingBox.x, command.boundingBox.y }, { command.boundingBox.x + command.boundingBox.width,
                            command.boundingBox.y + command.boundingBox.height } };
                        item.margin = bbox;
                    }

                    ++widgetidx;

#elif GLIMMER_FLEXBOX_ENGINE == GLIMMER_YOGA_ENGINE

                    auto child = FlexLayoutItems.back().children[widgetidx];
                    bbox = GetBoundingBox(child);
                    ++widgetidx;

#endif
                }
                else
                {
                    bbox = GridLayoutItems[layout.griditems[widgetidx]].bbox;
                    ++widgetidx;
                }

                min = ImMin(min, bbox.Min);
                max = ImMax(max, bbox.Max);
                break;
            }
            default:
                break;
            }
        }

        if (layout.type == Layout::Grid)
        {
            auto implicitW = max.x - min.x + (2.f * layout.spacing.x);
            auto implicitH = max.y - min.y + (2.f * layout.spacing.y);

            // Based on the layout's geometry, align layout items. If layout can expand 
            // in either x- or y-axis, center items if alignment is set accordingly.
            // NOTE: If layout is inside a scroll region, and scroll content dimensions are not
            //       set, it is assumed to be FLT_MAX, int which case, no centering of items 
            //       will happen even if alignment is set for it.
            if (!(layout.fill & FD_Horizontal))
            {
                if ((layout.alignment & AlignRight) && (layout.available.Max.x != FLT_MAX))
                {
                    layout.geometry.Max.x = layout.available.Max.x;
                    layout.geometry.Min.x = layout.geometry.Max.x - implicitW;
                }
                else [[likely]]
                {
                    layout.geometry.Min.x = layout.available.Min.x;
                    layout.geometry.Max.x = layout.geometry.Min.x + implicitW;
                }

                layout.available.Min.x = layout.geometry.Min.x;
                layout.available.Max.x = layout.geometry.Max.x;
            }
            else
            {

                auto hdiff = 0.f, width = layout.available.GetWidth();
                if ((layout.available.Max.x != FLT_MAX) && (width > 0.f))
                    hdiff = std::max((width - implicitW) * 0.5f, 0.f);

                layout.geometry.Min.x = layout.available.Min.x + hdiff;
                layout.geometry.Max.x = layout.geometry.Min.x + implicitW;
            }

            if (!(layout.fill & FD_Vertical))
            {
                if ((layout.alignment & AlignBottom) && (layout.available.Max.y != FLT_MAX))
                {
                    layout.geometry.Max.y = layout.available.Max.y;
                    layout.geometry.Min.y = layout.geometry.Max.y - implicitH ;
                }
                else [[likely]]
                {
                    layout.geometry.Min.y = layout.available.Min.y;
                    layout.geometry.Max.y = layout.geometry.Min.y + implicitH;
                }

                layout.available.Min.y = layout.geometry.Min.y;
                layout.available.Max.y = layout.geometry.Max.y;
            }
            else if (layout.alignment & AlignVCenter)
            {
                auto vdiff = 0.f, height = layout.available.GetHeight();
                if ((layout.available.Max.y != FLT_MAX) && (height > 0.f))
                    vdiff = std::max((height - implicitH) * 0.5f, 0.f);

                layout.geometry.Min.y = layout.available.Min.y + vdiff;
                layout.geometry.Max.y = layout.geometry.Min.y + implicitH;
            }
        }
        else
        {
            layout.geometry.Min = min + layout.startpos;
            layout.geometry.Max = max + layout.startpos + ImVec2{ layout.spacing.x * 2.f, layout.spacing.y * 2.f };

            if (layout.available.Max.x == FLT_MAX || layout.available.GetWidth() <= 0.f ||
                !(layout.fill & FD_Horizontal))
            {
                layout.available.Min.x = layout.geometry.Min.x;
                layout.available.Max.x = layout.geometry.Max.x;
            }

            if (layout.available.Max.y == FLT_MAX || layout.available.GetHeight() <= 0.f ||
                !(layout.fill & FD_Vertical))
            {
                layout.available.Min.y = layout.geometry.Min.y;
                layout.available.Max.y = layout.geometry.Max.y;
            }
        }

        context.AddItemGeometry(layout.id, layout.available);

        // If the layout being processed is a nested layout, it either has a user-specified size or
        // implicit size based on child content. Hence, update the layout implementation nodes
        // of parent layout that the child layout has a determined relative geometry.
        if (layout.itemidx != -1 && context.layoutItems[layout.itemidx].implData != nullptr)
        {
            auto sz = layout.geometry.GetSize();
            auto itemidx = layout.itemidx;

            if (layout.type != Layout::Grid)
            {
#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_YOGA_ENGINE
                auto child = static_cast<YGNodeRef>(context.layoutItems[itemidx].implData);
                YGNodeStyleSetWidth(child, sz.x);
                YGNodeStyleSetHeight(child, sz.y);
#endif
            }
            else
            {
                auto gridItemIdx = reinterpret_cast<int>(context.layoutItems[itemidx].implData);
                auto& item = GridLayoutItems[gridItemIdx];
                item.maxdim = sz;
            }
        }
    }

    static void RenderWidgets(WidgetContextData& context, LayoutBuilder& layout, WidgetDrawResult& result)
    {
        // This stores the data for replay of style push/pop operations within a layout block
        static StyleStackT StyleStack[WSI_Total];

        for (auto idx = 0; idx < WSI_Total; ++idx)
        {
            StyleStack[idx].clear(true);
            StyleStack[idx].push() = context.StyleStack[idx][layout.styleStartIdx[idx]];
        }

        auto io = Config.platform->CurrentIO();

        for (const auto [data, op] : context.layoutReplayContent)
        {
            switch (op)
            {
            case LayoutOps::AddWidget:
            {
                auto& item = context.layoutItems[(int16_t)data];

                if (!(data & GridLayoutMask))
                {
#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_CLAY_ENGINE

                    auto& command = renderCommands.internalArray[widgetidx];
                    if (command.commandType == CLAY_RENDER_COMMAND_TYPE_CUSTOM)
                    {
                        ImRect bbox = { { command.boundingBox.x, command.boundingBox.y }, { command.boundingBox.x + command.boundingBox.width,
                            command.boundingBox.y + command.boundingBox.height } };
                        bbox.Translate(layout.startpos + layout.spacing);
                        item.margin = bbox;
                    }

#elif GLIMMER_FLEXBOX_ENGINE == GLIMMER_YOGA_ENGINE

                    auto child = AllFlexItems[data];
                    auto bbox = GetBoundingBox(child);
                    bbox.Translate(layout.startpos + layout.spacing);
                    item.margin = bbox;

#endif
                }
                else
                {
                    auto idx = data & ~GridLayoutMask;
                    auto bbox = GridLayoutItems[idx].bbox;
                    bbox.Translate(layout.startpos + layout.spacing);
                    item.margin = bbox;
                }

                if (auto res = RenderWidget(layout, item, StyleStack, io); res.event != WidgetEvent::None)
                    result = res;
                break;
            }
            case LayoutOps::PushStyle:
            {
                auto state = data & 0xffffffff;
                auto index = data >> 32;
                StyleStack[state].push() = context.layoutStyles[state][index];
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
            case LayoutOps::IgnoreStyleStack:
            {
                WidgetContextData::IgnoreStyleStack(data);
                break;
            }
            case LayoutOps::RestoreStyleStack:
            {
                WidgetContextData::RestoreStyleStack();
                break;
            }
            case LayoutOps::PushTextType:
				PushTextType((TextType)(data));
                break;
            case LayoutOps::PopTextType:
                PopTextType();
                break;
            default:
                break;
            }
        }
    }

    WidgetDrawResult EndLayout(int depth)
    {
        WidgetDrawResult result;
        auto& context = GetContext();

        // Keep popping layouts as per specified depth 
        // Once a layout is popped, run layout algorithm on it
        // and compute layout item geometry
        // However, only render items once the top-mosy layout
        // is done computing the item geometries.
        while (depth > 0 && !context.layouts.empty())
        {
            auto& layout = context.layouts.top();
            ComputeLayoutGeometry(context, layout);
            
            if (context.layouts.size() == 1)
            {
                RenderWidgets(context, layout, result);
                context.adhocLayout.top().lastItemId = layout.id;
            }

            --depth;
            layout.reset();
            context.layouts.pop(1, false);
            context.nestedContextStack.pop(1, true);
        }

#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_YOGA_ENGINE

        // TODO: Maybe reuse these items?
        for (auto& item : FlexLayoutItems)
        {
            YGNodeFreeRecursive(item.root);
            item.children.clear();
        }

#endif

        if (context.layouts.empty()) context.ResetLayoutData();
        AllFlexItems.clear(true);
        GridLayoutItems.clear(true);
        FlexLayoutItems.clear(false);
        return result;
    }

#pragma endregion
}
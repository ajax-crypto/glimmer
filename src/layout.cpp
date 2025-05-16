#include "layout.h"

#include "context.h"
#include "style.h"
#include "draw.h"

#define GLIMMER_FAST_LAYOUT_ENGINE 0
#define GLIMMER_CLAY_LAYOUT_ENGINE 1
#define GLIMMER_YOGA_LAYOUT_ENGINE 2

#ifndef GLIMMER_LAYOUT_ENGINE
#define GLIMMER_LAYOUT_ENGINE GLIMMER_FAST_LAYOUT_ENGINE
#endif

#if GLIMMER_LAYOUT_ENGINE == GLIMMER_CLAY_LAYOUT_ENGINE
#define CLAY_IMPLEMENTATION
#include "clay/clay.h"

Clay_Arena LayoutArena;
void* LayoutMemory = nullptr;
#endif

namespace glimmer
{
    WidgetDrawResult LabelImpl(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io, int32_t textflags);
    WidgetDrawResult ButtonImpl(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io);
    WidgetDrawResult ToggleButtonImpl(int32_t id, ToggleButtonState& state, const ImRect& extent, ImVec2 textsz, IRenderer& renderer, const IODescriptor& io);
    WidgetDrawResult RadioButtonImpl(int32_t id, RadioButtonState& state, const ImRect& extent, IRenderer& renderer, const IODescriptor& io);
    WidgetDrawResult CheckboxImpl(int32_t id, CheckboxState& state, const ImRect& extent, const ImRect& padding, IRenderer& renderer, const IODescriptor& io);
    WidgetDrawResult TextInputImpl(int32_t id, TextInputState& state, const ImRect& extent, const ImRect& content, IRenderer& renderer, const IODescriptor& io);
    WidgetDrawResult DropDownImpl(int32_t id, DropDownState& state, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io);
    WidgetDrawResult TabBarImpl(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io);
    WidgetDrawResult ItemGridImpl(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io);

#pragma region Layout functions

    void PushSpan(int32_t direction)
    {
        Context.spans.push(direction);
    }

    void SetSpan(int32_t direction)
    {
        Context.spans.push(direction | OnlyOnce);
    }

    void Move(int32_t direction)
    {
        if (!Context.layouts.empty()) return;
        auto& layout = Context.adhocLayout.top();
        assert(layout.lastItemId != -1);
        Move(layout.lastItemId, direction);
    }

    void Move(int32_t id, int32_t direction)
    {
        if (!Context.layouts.empty()) return;
        const auto& geometry = Context.GetGeometry(id);
        auto& layout = Context.adhocLayout.top();
        layout.nextpos = geometry.Min;
        if (direction & FD_Horizontal) layout.nextpos.x = geometry.Max.x;
        if (direction & FD_Vertical) layout.nextpos.y = geometry.Max.y;
    }

    void Move(int32_t hid, int32_t vid, bool toRight, bool toBottom)
    {
        if (!Context.layouts.empty()) return;
        const auto& hgeometry = Context.GetGeometry(hid);
        const auto& vgeometry = Context.GetGeometry(vid);
        auto& layout = Context.adhocLayout.top();
        layout.nextpos.x = toRight ? hgeometry.Max.x : hgeometry.Min.x;
        layout.nextpos.y = toBottom ? vgeometry.Max.y : vgeometry.Min.y;
    }

    void Move(ImVec2 amount, int32_t direction)
    {
        if (!Context.layouts.empty()) return;
        if (direction & ToLeft) amount.x = -amount.x;
        if (direction & ToTop) amount.y = -amount.y;
        auto& layout = Context.adhocLayout.top();
        layout.nextpos += amount;
    }

    void Move(ImVec2 pos)
    {
        if (!Context.layouts.empty()) return;
        Context.adhocLayout.top().nextpos = pos;
    }

    void PopSpan(int depth)
    {
        Context.spans.pop(depth);
    }

    void AddExtent(LayoutItemDescriptor& wdesc, const StyleDescriptor& style, const NeighborWidgets& neighbors,
        float width, float height)
    {
        auto totalsz = Context.MaximumAbsExtent();
        auto nextpos = Context.NextAdHocPos();
        if (width <= 0.f) width = totalsz.x - nextpos.x;
        if (height <= 0.f) height = totalsz.y - nextpos.y;

        wdesc.margin.Min = nextpos;
        wdesc.border.Min = wdesc.margin.Min + ImVec2{ style.margin.left, style.margin.top };
        wdesc.padding.Min = wdesc.border.Min + ImVec2{ style.border.left.thickness, style.border.top.thickness };
        wdesc.content.Min = wdesc.padding.Min + ImVec2{ style.padding.left, style.padding.top };

        if (style.specified & StyleWidth)
        {
            auto w = ImClamp(style.dimension.x, style.mindim.x, style.maxdim.x);
            wdesc.content.Max.x = wdesc.content.Min.x + w;
            wdesc.padding.Max.x = wdesc.content.Max.x + style.padding.right;
            wdesc.border.Max.x = wdesc.padding.Max.x + style.border.right.thickness;
            wdesc.margin.Max.x = wdesc.border.Max.x + style.margin.right;
        }
        else
        {
            if (neighbors.right != -1) wdesc.margin.Max.x = Context.GetGeometry(neighbors.right).Min.x;
            else wdesc.margin.Max.x = wdesc.margin.Min.x + width;

            wdesc.border.Max.x = wdesc.margin.Max.x - style.margin.right;
            wdesc.padding.Max.x = wdesc.border.Max.x - style.border.right.thickness;
            wdesc.content.Max.x = wdesc.padding.Max.x - style.padding.right;
        }

        if (style.specified & StyleHeight)
        {
            auto h = ImClamp(style.dimension.y, style.mindim.x, style.maxdim.x);
            wdesc.content.Max.y = wdesc.content.Min.y + h;
            wdesc.padding.Max.y = wdesc.content.Max.y + style.padding.bottom;
            wdesc.border.Max.y = wdesc.padding.Max.y + style.border.bottom.thickness;
            wdesc.margin.Max.y = wdesc.border.Max.y + style.margin.bottom;
        }
        else
        {
            if (neighbors.bottom != -1) wdesc.margin.Max.y = Context.GetGeometry(neighbors.bottom).Min.y;
            else wdesc.margin.Max.y = wdesc.margin.Min.y + height;

            wdesc.border.Max.y = wdesc.margin.Max.y - style.margin.bottom;
            wdesc.padding.Max.y = wdesc.border.Max.y - style.border.bottom.thickness;
            wdesc.content.Max.y = wdesc.padding.Max.y - style.padding.bottom;
        }
    }

#if GLIMMER_LAYOUT_ENGINE == GLIMMER_FAST_LAYOUT_ENGINE
    static float GetTotalSpacing(LayoutDescriptor& layout, Direction dir)
    {
        return dir == DIR_Horizontal ? (((float)layout.currcol + 1.f) * layout.spacing.x) :
            (((float)layout.currow + 1.f) * layout.spacing.y);
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
        switch (layout.type)
        {
        case Layout::Horizontal:
        {
            // If wrapping is enabled and the alignment is horizontally centered,
            // perform h-centering of the current row of widgets and move to next row
            // Otherwise, if output should be justified, move all widgets to specific
            // location after distributing the diff equally...
            if ((layout.alignment & TextAlignHCenter) && (layout.fill & FD_Horizontal))
            {
                auto totalspacing = GetTotalSpacing(layout, DIR_Horizontal);
                auto hdiff = (layout.geometry.GetWidth() - layout.rows[layout.currow].x - totalspacing) * 0.5f;

                if (hdiff > 0.f)
                {
                    //for (auto row = 0; row <= layout.currow; ++row)
                    {
                        for (auto idx = layout.from; idx <= layout.to; ++idx)
                        {
                            auto& widget = Context.layoutItems[idx];
                            if (widget.row == layout.currow) TranslateX(widget, hdiff);
                        }
                    }
                }
            }
            else if ((layout.alignment & TextAlignJustify) && (layout.fill & FD_Horizontal))
            {
                auto hdiff = (layout.geometry.GetWidth() - layout.rows[layout.currow].x) /
                    ((float)layout.currcol + 1.f);

                if (hdiff > 0.f)
                {
                    //for (auto row = 0; row <= layout.currow; ++row)
                    {
                        auto currposx = hdiff;
                        for (auto idx = layout.from; idx <= layout.to; ++idx)
                        {
                            auto& widget = Context.layoutItems[idx];

                            if (widget.row == layout.currow)
                            {
                                auto translatex = currposx - widget.margin.Min.x;
                                TranslateX(widget, translatex);
                                currposx += widget.margin.GetWidth() + hdiff;
                            }
                        }
                    }
                }
            }
            break;
        }

        case Layout::Vertical:
        {
            // Similar logic as for horizontal alignment, implemented for vertical here
            if ((layout.alignment & TextAlignVCenter) && (layout.fill & FD_Vertical))
            {
                auto totalspacing = GetTotalSpacing(layout, DIR_Vertical);
                auto hdiff = (layout.geometry.GetHeight() - layout.cols[layout.currcol].y - totalspacing) * 0.5f;

                if (hdiff > 0.f)
                {
                    for (auto idx = layout.from; idx <= layout.to; ++idx)
                    {
                        auto& widget = Context.layoutItems[idx];
                        if (widget.col == layout.currcol) TranslateY(widget, hdiff);
                    }
                }
            }
            else if ((layout.alignment & TextAlignJustify) && (layout.fill & FD_Vertical))
            {
                auto hdiff = (layout.geometry.GetHeight() - layout.cols[layout.currcol].x) /
                    ((float)layout.currow + 1.f);

                if (hdiff > 0.f)
                {
                    auto currposx = hdiff;
                    for (auto idx = layout.from; idx <= layout.to; ++idx)
                    {
                        auto& widget = Context.layoutItems[idx];
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
        if ((layout.fill & FD_Horizontal) == 0)
            layout.geometry.Max.x = Context.layoutItems[layout.itemidx].margin.Max.x = layout.maxdim.x;
        if ((layout.fill & FD_Vertical) == 0)
            layout.geometry.Max.y = Context.layoutItems[layout.itemidx].margin.Max.y = layout.maxdim.y;

        switch (layout.type)
        {
        case Layout::Horizontal:
        {
            if ((layout.alignment & TextAlignVCenter) && (layout.fill & FD_Vertical))
            {
                auto totalspacing = GetTotalSpacing(layout, DIR_Vertical);
                auto cumulativey = layout.cumulative.y + layout.maxdim.y;
                auto vdiff = (layout.geometry.GetHeight() - totalspacing - cumulativey) * 0.5f;

                if (vdiff > 0.f)
                {
                    for (auto idx = layout.from; idx <= layout.to; ++idx)
                    {
                        auto& widget = Context.layoutItems[idx];
                        TranslateY(widget, vdiff);
                    }
                }
            }
            else if ((layout.alignment & TextAlignJustify) && (layout.fill & FD_Vertical))
            {
                auto cumulativey = layout.cumulative.y + layout.maxdim.y;
                auto vdiff = (layout.geometry.GetHeight() - cumulativey) / ((float)layout.currow + 1.f);

                if (vdiff > 0.f)
                {
                    auto currposy = vdiff;
                    for (auto idx = layout.from; idx <= layout.to; ++idx)
                    {
                        auto& widget = Context.layoutItems[idx];
                        auto translatey = currposy - widget.margin.Min.y;
                        TranslateY(widget, vdiff);
                        currposy += vdiff + widget.margin.GetHeight();
                    }
                }
            }
            else if ((layout.alignment & TextAlignBottom) && (layout.fill & FD_Vertical))
            {
                auto totalspacing = GetTotalSpacing(layout, DIR_Vertical);
                auto vdiff = layout.geometry.GetHeight() - layout.maxdim.y - totalspacing;

                if (vdiff > 0.f)
                {
                    for (auto idx = layout.from; idx <= layout.to; ++idx)
                    {
                        auto& widget = Context.layoutItems[idx];
                        TranslateY(widget, vdiff);
                    }
                }
            }
            break;
        }

        case Layout::Vertical:
        {
            if ((layout.alignment & TextAlignHCenter) && (layout.fill & FD_Horizontal))
            {
                auto totalspacing = GetTotalSpacing(layout, DIR_Horizontal);
                auto cumulativex = layout.cumulative.x + layout.maxdim.x;
                auto hdiff = (layout.geometry.GetWidth() - totalspacing - cumulativex) * 0.5f;

                if (hdiff > 0.f)
                {
                    for (auto idx = layout.from; idx <= layout.to; ++idx)
                    {
                        auto& widget = Context.layoutItems[idx];
                        TranslateX(widget, hdiff);
                    }
                }
            }
            else if ((layout.alignment & TextAlignJustify) && (layout.fill & FD_Horizontal))
            {
                auto cumulativex = layout.cumulative.x + layout.maxdim.x;
                auto hdiff = (layout.geometry.GetWidth() - cumulativex) / ((float)layout.currcol + 1.f);

                if (hdiff > 0.f)
                {
                    auto currposy = hdiff;
                    for (auto idx = layout.from; idx <= layout.to; ++idx)
                    {
                        auto& widget = Context.layoutItems[idx];
                        auto translatex = currposy - widget.margin.Min.x;
                        TranslateX(widget, hdiff);
                        currposy += hdiff + widget.margin.GetWidth();
                    }
                }
            }
            else if ((layout.alignment & TextAlignRight) && (layout.fill & FD_Vertical))
            {
                auto totalspacing = GetTotalSpacing(layout, DIR_Horizontal);
                auto vdiff = layout.geometry.GetWidth() - layout.maxdim.x - totalspacing;

                if (vdiff > 0.f)
                {
                    for (auto idx = layout.from; idx <= layout.to; ++idx)
                    {
                        auto& widget = Context.layoutItems[idx];
                        TranslateX(widget, vdiff);
                    }
                }
            }
            break;
        }
        }
    }
#endif

    ImVec2 AddItemToLayout(LayoutDescriptor& layout, LayoutItemDescriptor& item)
    {
#if GLIMMER_LAYOUT_ENGINE == GLIMMER_FAST_LAYOUT_ENGINE
        ImVec2 offset = layout.nextpos - layout.geometry.Min;
        layout.currow = std::max(layout.currow, (int16_t)0);
        layout.currcol = std::max(layout.currcol, (int16_t)0);

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

                    offset = ImVec2{ 0.f, layout.cumulative.y + layout.maxdim.y };
                    layout.nextpos.x = width;
                    layout.nextpos.y = offset.y;
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
                    layout.nextpos.x += width;
                    layout.rows[layout.currow].x += width;
                    item.col = layout.currcol;
                    item.row = layout.currow;
                    layout.currcol++;
                }
            }
            else
            {
                if ((layout.fill & FD_Horizontal) == 0) layout.geometry.Max.x += width;
                layout.nextpos.x += width;
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

                    offset = ImVec2{ layout.cumulative.x + layout.maxdim.x, 0.f };
                    layout.nextpos.x = offset.x;
                    layout.nextpos.y = height;
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
                    layout.nextpos.y += height;
                    layout.cols[layout.currcol].y += height;
                    item.col = layout.currcol;
                    item.row = layout.currow;
                    layout.currow++;
                }
            }
            else
            {
                if ((layout.fill & FD_Vertical) == 0) layout.geometry.Max.y += height;
                layout.nextpos.y += height;
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

        item.margin.Translate(offset);
        item.border.Translate(offset);
        item.padding.Translate(offset);
        item.content.Translate(offset);
        item.text.Translate(offset);
        Context.layoutItems.push_back(item);
        
        if (!Context.spans.empty() && (Context.spans.top() & OnlyOnce) != 0) Context.spans.pop();

        if (layout.from == -1) layout.from = layout.to = (int16_t)(Context.layoutItems.size() - 1);
        else layout.to = (int16_t)(Context.layoutItems.size() - 1);
        layout.itemidx = layout.to;
        return offset;

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
        return ImVec2{ 0, 0 };
#endif
        
    }

    static ImRect GetAvailableSpace(int32_t direction, ImVec2 nextpos, const NeighborWidgets& neighbors)
    {
        ImRect available;
        auto maxabs = Context.MaximumAbsExtent();

        available.Min.y = nextpos.y;
        available.Max.y = neighbors.bottom == -1 ? maxabs.y : Context.GetGeometry(neighbors.bottom).Min.y;
        available.Min.x = nextpos.x;
        available.Max.x = neighbors.right == -1 ? maxabs.x : Context.GetGeometry(neighbors.right).Min.x;

        return available;
    }

    static bool IsLayoutDependentOnContent(const LayoutDescriptor& layout)
    {
        return layout.fill != 0;
    }

    ImRect BeginLayout(Layout type, int32_t fill, int32_t alignment, bool wrap, ImVec2 spacing, const NeighborWidgets& neighbors)
    {
        auto& layout = Context.layouts.push();
        const auto& style = Context.currStyleStates == 0 ? Context.pushedStyles[WSI_Default].top() : Context.currStyle[WSI_Default];

        layout.type = type;
        layout.alignment = alignment;
        layout.fill = fill;
        layout.spacing = spacing;
        layout.type == Layout::Horizontal ? (layout.hofmode = wrap ? OverflowMode::Wrap : OverflowMode::Scroll) :
            (layout.vofmode = wrap ? OverflowMode::Wrap : OverflowMode::Scroll);
        
        auto nextpos = Context.NextAdHocPos();
        ImRect available = GetAvailableSpace(alignment, nextpos, neighbors);

#if GLIMMER_LAYOUT_ENGINE == GLIMMER_CLAY_LAYOUT_ENGINE
        if (Context.layouts.size() == 1)
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
        /*if (!Context.containerStack.empty())
        {
            auto id = Context.containerStack.top();
            auto wtype = (WidgetType)(id >> 16);

            Clay_SetExternalScrollHandlingEnabled(true);
            Clay_SetQueryScrollOffsetFunction([](uint32_t id, void* data) {
                Context.ScrollRegion(id);
            });
        }*/
#endif

        layout.geometry = available;
        layout.nextpos = nextpos;
        return layout.geometry;
    }

    void NextRow()
    {
        if (!Context.layouts.empty())
        {
            auto& layout = Context.layouts.top();
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
        if (!Context.layouts.empty())
        {
            auto& layout = Context.layouts.top();
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
        auto& sizing = Context.sizing.push();
        sizing.horizontal = width;
        sizing.vertical = height;
        sizing.relativeh = relativew;
        sizing.relativev = relativeh;
    }

    void PopSizing(int depth)
    {
        Context.sizing.pop(depth);
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

    static WidgetDrawResult RenderWidget(LayoutItemDescriptor& item, const IODescriptor& io)
    {
        WidgetDrawResult result;
        auto bbox = item.margin;
        auto wtype = (WidgetType)(item.id >> 16);
        auto& renderer = Context.usingDeferred ? *Context.deferedRenderer : *Config.renderer;
        //LOG("Rendering widget at (%f, %f) to (%f, %f)\n", bbox.Min.x, bbox.Min.y, bbox.Max.x, bbox.Max.y);

        switch (wtype)
        {
        case glimmer::WT_Label: {
            auto& state = Context.GetState(item.id).state.label;
            auto flags = ToTextFlags(state.type);
            const auto& style = Context.GetStyle(state.state);
            UpdateGeometry(item, bbox, style);
            Context.AddItemGeometry(item.id, bbox);
            result = LabelImpl(item.id, item.margin, item.border, item.padding, item.content, item.text, renderer, io, flags);
            break;
        }
        case glimmer::WT_Button: {
            auto& state = Context.GetState(item.id).state.button;
            auto flags = ToTextFlags(state.type);
            const auto& style = Context.GetStyle(state.state);
            UpdateGeometry(item, bbox, style);
            Context.AddItemGeometry(item.id, bbox);
            result = ButtonImpl(item.id, item.margin, item.border, item.padding, item.content, item.text, renderer, io);
            break;
        }
        case glimmer::WT_RadioButton: {
            auto& state = Context.GetState(item.id).state.radio;
            const auto& style = Context.GetStyle(state.state);
            UpdateGeometry(item, bbox, style);
            Context.AddItemGeometry(item.id, bbox);
            result = RadioButtonImpl(item.id, state, item.margin, renderer, io);
            break;
        }
        case glimmer::WT_ToggleButton: {
            auto& state = Context.GetState(item.id).state.toggle;
            const auto& style = Context.GetStyle(state.state);
            UpdateGeometry(item, bbox, style);
            Context.AddItemGeometry(item.id, bbox);
            result = ToggleButtonImpl(item.id, state, item.margin, ImVec2{ item.text.GetWidth(), item.text.GetHeight() }, renderer, io);
            break;
        }
        case glimmer::WT_Checkbox: {
            auto& state = Context.GetState(item.id).state.checkbox;
            const auto& style = Context.GetStyle(state.state);
            UpdateGeometry(item, bbox, style);
            Context.AddItemGeometry(item.id, bbox);
            result = CheckboxImpl(item.id, state, item.margin, item.padding, renderer, io);
            break;
        }
        case glimmer::WT_Slider:
            break;
        case glimmer::WT_TextInput: {
            auto& state = Context.GetState(item.id).state.input;
            const auto& style = Context.GetStyle(state.state);
            UpdateGeometry(item, bbox, style);
            Context.AddItemGeometry(item.id, bbox);
            result = TextInputImpl(item.id, state, item.margin, item.content, renderer, io);
            break;
        }
        case glimmer::WT_DropDown: {
            auto& state = Context.GetState(item.id).state.dropdown;
            const auto& style = Context.GetStyle(state.state);
            UpdateGeometry(item, bbox, style);
            Context.AddItemGeometry(item.id, bbox);
            result = DropDownImpl(item.id, state, item.margin, item.border, item.padding, item.content, item.text, renderer, io);
            break;
        }
        case glimmer::WT_TabBar:
            break;
        case glimmer::WT_ItemGrid: {
            auto& state = Context.GetState(item.id).state.grid;
            const auto& style = Context.GetStyle(state.state);
            UpdateGeometry(item, bbox, style);
            Context.AddItemGeometry(item.id, bbox);
            result = ItemGridImpl(item.id, item.margin, item.border, item.padding, item.content, item.text, renderer, io);
            break;
        }
        default:
            break;
        }

        return result;
    }

    WidgetDrawResult EndLayout(int depth)
    {
        WidgetDrawResult result;
        ImRect geometry;
        
        while (depth > 0 && !Context.layouts.empty())
        {
#if GLIMMER_LAYOUT_ENGINE == GLIMMER_CLAY_LAYOUT_ENGINE
            Clay__CloseElement();

            if (Context.layouts.size() == 1)
            {
                const auto& layout = Context.layouts.top();
                auto renderCommands = Clay_EndLayout();

                for (auto idx = 0; idx < renderCommands.length; ++idx)
                {
                    auto& command = renderCommands.internalArray[idx];
                    if (command.commandType == CLAY_RENDER_COMMAND_TYPE_CUSTOM)
                    {
                        ImRect bbox = { { command.boundingBox.x, command.boundingBox.y }, { command.boundingBox.x + command.boundingBox.width,
                            command.boundingBox.y + command.boundingBox.height } };
                        bbox.Translate(layout.geometry.Min);
                        auto data = command.renderData.custom;
                        auto id = (int16_t)((intptr_t)(data.customData));
                        RenderWidget(id, bbox);
                    }
                }
            }
#elif GLIMMER_LAYOUT_ENGINE == GLIMMER_FAST_LAYOUT_ENGINE
            auto& layout = Context.layouts.top();

            if (!IsLayoutDependentOnContent(layout) || Context.layouts.size() == 1)
            {
                AlignLayoutAxisItems(layout);
                AlignCrossAxisItems(layout, depth);
            }
            else
            {
                auto& parent = Context.layouts.top(1);
                auto& item = Context.layoutItems.emplace_back();

                item.id = (WT_Layout << 16) | (parent.children.size() - 1);
                item.to = (int16_t)(Context.layoutItems.size() - 1);
                if (item.from == -1) item.from = item.to;
                parent.children.emplace_back(layout);
            }

            if (Context.layouts.size() == 1)
            {
                auto io = Config.platform->CurrentIO();

                /*if (layout.vofmode == OverflowMode::Scroll || layout.hofmode == OverflowMode::Scroll)
                {
                    auto& region = layout.scroll;
                    const auto& style = Context.GetStyle(WS_Default);

                    auto& renderer = Context.usingDeferred ? *Context.deferedRenderer : *Config.renderer;
                    LayoutItemDescriptor wdesc;
                    wdesc.margin = layout.geometry;
                    wdesc.border.Min = wdesc.margin.Min + ImVec2{ style.margin.left, style.margin.top };
                    wdesc.border.Max = wdesc.margin.Max - ImVec2{ style.margin.right, style.margin.bottom };
                    DrawBorderRect(wdesc.border.Min, wdesc.border.Max, style.border, style.bgcolor, renderer);

                    region.viewport = wdesc.content;
                    region.enabled = { layout.vofmode == OverflowMode::Scroll, layout.hofmode == OverflowMode::Scroll };
                    renderer.SetClipRect(wdesc.content.Min, wdesc.content.Max);
                    Context.containerStack.push(id);
                }*/

                for (auto& item : Context.layoutItems)
                    if (auto res = RenderWidget(item, io); res.event != WidgetEvent::None)
                        result = res;
                geometry = layout.geometry;


            }
#endif

            --depth;
            Context.layouts.pop();
        }

        if (Context.layouts.empty())
        {
            Context.layoutItems.clear();
            Context.adhocLayout.top().nextpos = geometry.Max;
        }

        return result;
    }

#pragma endregion
}
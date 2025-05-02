#include "layout.h"

#include "context.h"
#include "style.h"
#include "draw.h"

namespace glimmer
{
#pragma region Layout functions

    void PushSpan(int32_t direction)
    {
        Context.currSpanDepth++;
        Context.spans[Context.currSpanDepth].direction = direction;
    }

    void SetSpan(int32_t direction)
    {
        PushSpan(direction);
        Context.spans[Context.currSpanDepth].popWhenUsed = true;
    }

    void Move(int32_t direction)
    {
        auto& layout = Context.adhocLayout.top();
        assert(layout.lastItemId != -1);
        Move(layout.lastItemId, direction);
    }

    void Move(int32_t id, int32_t direction)
    {
        const auto& geometry = Context.GetGeometry(id);
        auto& layout = Context.adhocLayout.top();
        layout.nextpos = geometry.Min;
        if (direction & FD_Horizontal) layout.nextpos.x = geometry.Max.x;
        if (direction & FD_Vertical) layout.nextpos.y = geometry.Max.y;
    }

    void Move(int32_t hid, int32_t vid, bool toRight, bool toBottom)
    {
        const auto& hgeometry = Context.GetGeometry(hid);
        const auto& vgeometry = Context.GetGeometry(vid);
        auto& layout = Context.adhocLayout.top();
        layout.nextpos.x = toRight ? hgeometry.Max.x : hgeometry.Min.x;
        layout.nextpos.y = toBottom ? vgeometry.Max.y : vgeometry.Min.y;
    }

    void Move(ImVec2 amount, int32_t direction)
    {
        if (direction & ToLeft) amount.x = -amount.x;
        if (direction & ToTop) amount.y = -amount.y;
        auto& layout = Context.adhocLayout.top();
        layout.nextpos += amount;
    }

    void Move(ImVec2 pos)
    {
        Context.adhocLayout.top().nextpos = pos;
    }

    void PopSpan(int depth)
    {
        while (depth > 0 && Context.currSpanDepth > -1)
        {
            Context.spans[Context.currSpanDepth] = ElementSpan{};
            --Context.currSpanDepth;
            --depth;
        }
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
            if ((layout.alignment & TextAlignHCenter) && (layout.sizing & ExpandH))
            {
                auto totalspacing = 2.f * layout.spacing.x * (float)layout.currcol;
                auto hdiff = (layout.geometry.GetWidth() - layout.rows[layout.currow].x - totalspacing) * 0.5f;

                if (hdiff > 0.f)
                {
                    for (auto idx = layout.from; idx <= layout.to; ++idx)
                    {
                        auto& widget = Context.layoutItems[idx];

                        if (widget.row == layout.currow)
                        {
                            widget.content.TranslateX(hdiff);
                            widget.padding.TranslateX(hdiff);
                            widget.border.TranslateX(hdiff);
                            widget.margin.TranslateX(hdiff);
                        }
                    }
                }
            }
            else if ((layout.alignment & TextAlignJustify) && (layout.sizing & ExpandH))
            {
                auto hdiff = (layout.geometry.GetWidth() - layout.rows[layout.currow].x) /
                    ((float)layout.currcol + 1.f);

                if (hdiff > 0.f)
                {
                    auto currposx = hdiff;
                    for (auto idx = layout.from; idx <= layout.to; ++idx)
                    {
                        auto& widget = Context.layoutItems[idx];

                        if (widget.row == layout.currow)
                        {
                            auto translatex = currposx - widget.margin.Min.x;
                            widget.content.TranslateX(translatex);
                            widget.padding.TranslateX(translatex);
                            widget.border.TranslateX(translatex);
                            widget.margin.TranslateX(translatex);
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
            if ((layout.alignment & TextAlignVCenter) && (layout.sizing & ExpandV))
            {
                auto totalspacing = 2.f * layout.spacing.x * (float)layout.currow;
                auto hdiff = (layout.geometry.GetHeight() - layout.cols[layout.currcol].x - totalspacing) * 0.5f;

                if (hdiff > 0.f)
                {
                    for (auto idx = layout.from; idx <= layout.to; ++idx)
                    {
                        auto& widget = Context.layoutItems[idx];
                        if (widget.col == layout.currcol)
                        {
                            widget.content.TranslateY(hdiff);
                            widget.padding.TranslateY(hdiff);
                            widget.border.TranslateY(hdiff);
                            widget.margin.TranslateY(hdiff);
                        }
                    }
                }
            }
            else if ((layout.alignment & TextAlignJustify) && (layout.sizing & ExpandV))
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
                            widget.content.TranslateY(translatex);
                            widget.padding.TranslateY(translatex);
                            widget.border.TranslateY(translatex);
                            widget.margin.TranslateY(translatex);
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
        if ((layout.sizing & ExpandH) == 0)
            layout.geometry.Max.x = Context.layoutItems[layout.itemidx].margin.Max.x = layout.maxdim.x;
        if ((layout.sizing & ExpandV) == 0)
            layout.geometry.Max.y = Context.layoutItems[layout.itemidx].margin.Max.y = layout.maxdim.y;

        switch (layout.type)
        {
        case Layout::Horizontal:
        {
            if ((layout.alignment & TextAlignVCenter) && (layout.sizing & ExpandV))
            {
                auto totalspacing = (float)layout.currow * layout.spacing.y;
                auto cumulativey = layout.cumulative.y + layout.maxdim.y;
                auto vdiff = (layout.geometry.GetHeight() - totalspacing - cumulativey) * 0.5f;

                if (vdiff > 0.f)
                {
                    for (auto idx = layout.from; idx <= layout.to; ++idx)
                    {
                        auto& widget = Context.layoutItems[idx];
                        widget.margin.TranslateY(vdiff);
                        widget.border.TranslateY(vdiff);
                        widget.padding.TranslateY(vdiff);
                        widget.content.TranslateY(vdiff);
                    }
                }
            }
            else if ((layout.alignment & TextAlignJustify) && (layout.sizing & ExpandV))
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
                        widget.margin.TranslateY(translatey);
                        widget.border.TranslateY(translatey);
                        widget.padding.TranslateY(translatey);
                        widget.content.TranslateY(translatey);
                        currposy += vdiff + widget.margin.GetHeight();
                    }
                }
            }
            break;
        }

        case Layout::Vertical:
        {
            if ((layout.alignment & TextAlignHCenter) && (layout.sizing & ExpandH))
            {
                auto totalspacing = (float)layout.currcol * layout.spacing.x;
                auto cumulativex = layout.cumulative.x + layout.maxdim.x;
                auto hdiff = (layout.geometry.GetWidth() - totalspacing - cumulativex) * 0.5f;

                if (hdiff > 0.f)
                {
                    for (auto idx = layout.from; idx <= layout.to; ++idx)
                    {
                        auto& widget = Context.layoutItems[idx];
                        widget.margin.TranslateX(hdiff);
                        widget.border.TranslateX(hdiff);
                        widget.padding.TranslateX(hdiff);
                        widget.content.TranslateX(hdiff);
                    }
                }
            }
            else if ((layout.alignment & TextAlignJustify) && (layout.sizing & ExpandH))
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
                        widget.margin.TranslateX(translatex);
                        widget.border.TranslateX(translatex);
                        widget.padding.TranslateX(translatex);
                        widget.content.TranslateX(translatex);
                        currposy += hdiff + widget.margin.GetWidth();
                    }
                }
            }
            break;
        }
        }
    }

    ImVec2 AddItemToLayout(LayoutDescriptor& layout, LayoutItemDescriptor& item)
    {
        ImVec2 offset = layout.nextpos;

        switch (layout.type)
        {
        case Layout::Horizontal:
        {
            auto width = item.margin.GetWidth();

            if ((layout.sizing & ExpandH) && (layout.hofmode == OverflowMode::Wrap))
            {
                if (layout.nextpos.x + width > layout.geometry.Max.x)
                {
                    layout.geometry.Max.y += layout.maxdim.y;
                    offset = ImVec2{ layout.geometry.Min.x, layout.geometry.Max.y - layout.maxdim.y };
                    layout.nextpos.x = offset.x + width;
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
                if ((layout.sizing & ExpandH) == 0) layout.geometry.Max.x += width;
                layout.nextpos.x += width;
                layout.maxdim.y = std::max(layout.maxdim.y, item.margin.GetHeight());
                if ((layout.sizing & ExpandV) == 0) layout.geometry.Max.y = layout.geometry.Min.y + layout.maxdim.y;
                else layout.cumulative.y = layout.maxdim.y;
            }
            break;
        }

        case Layout::Vertical:
        {
            auto height = item.margin.GetHeight();

            if ((layout.sizing & ExpandV) && (layout.vofmode == OverflowMode::Wrap))
            {
                if (layout.nextpos.y + height > layout.geometry.Max.y)
                {
                    layout.geometry.Max.x += layout.maxdim.x;
                    offset = ImVec2{ layout.geometry.Max.x - layout.maxdim.x, layout.geometry.Min.y };
                    layout.nextpos.x = offset.x;
                    layout.nextpos.y = offset.y + height;
                    AlignLayoutAxisItems(layout);

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
                if ((layout.sizing & ExpandV) == 0) layout.geometry.Max.y += height;
                layout.nextpos.y += height;
                layout.maxdim.x = std::max(layout.maxdim.x, item.margin.GetWidth());
                if ((layout.sizing & ExpandH) == 0) layout.geometry.Max.x = layout.geometry.Min.x + layout.maxdim.x;
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

        Context.layoutItems.push_back(item);
        if (layout.from == -1) layout.from = layout.to = (int16_t)(Context.layoutItems.size() - 1);
        else layout.to = (int16_t)(Context.layoutItems.size() - 1);
        layout.itemidx = layout.to;
        return offset;
    }

    static void ComputeInitialGeometry(LayoutDescriptor& layout, const StyleDescriptor& style)
    {
        if (Context.currLayoutDepth > 0)
        {
            auto& parent = Context.layouts[Context.currLayoutDepth - 1];
            const auto& sizing = Context.sizing[Context.currSizingDepth];

            // Ideally, for left or right alignment for parent, and expand for child
            // The child should be flexible and shrink to accommodate siblings?
            // TODO: Add a shrink flag to layouts?
            if ((layout.fill & FD_Horizontal) && (parent.sizing & ExpandH))
            {
                layout.geometry.Min.x = parent.nextpos.x + layout.spacing.x;
                layout.geometry.Max.x = parent.prevpos.x - parent.spacing.x;
            }
            else if (parent.alignment & TextAlignLeft)
            {
                layout.geometry.Min.x = parent.nextpos.x + layout.spacing.x;

                if (sizing.horizontal != FIT_SZ)
                {
                    auto hmeasure = sizing.relativeh ?
                        (parent.geometry.GetWidth() * sizing.horizontal) :
                        sizing.horizontal;
                    layout.geometry.Max.x = layout.geometry.Min.x + hmeasure;
                }
                else
                {
                    layout.sizing &= ~ExpandH;
                    layout.geometry.Max.x = layout.geometry.Min.x + layout.spacing.x;
                }

                parent.nextpos.x = layout.geometry.Max.x + parent.spacing.x;
            }
            else if (parent.alignment & TextAlignRight)
            {
                layout.geometry.Max.x = parent.prevpos.x - layout.spacing.x;

                if (layout.fill & FD_Horizontal) layout.geometry.Min.x = parent.nextpos.x + layout.spacing.x;
                else if (sizing.horizontal != FIT_SZ)
                {
                    auto hmeasure = sizing.relativeh ?
                        (parent.geometry.GetWidth() * sizing.horizontal) :
                        sizing.horizontal;
                    layout.geometry.Min.x = layout.geometry.Max.x - hmeasure;
                }

                parent.prevpos.x = layout.geometry.Min.x - parent.spacing.x;
            }

            if ((layout.fill & FD_Vertical) && (parent.sizing & ExpandV))
            {
                layout.geometry.Max.y = parent.prevpos.y - parent.spacing.y;
                layout.geometry.Min.y = parent.nextpos.y + layout.spacing.y;
            }
            else if (parent.alignment & TextAlignTop)
            {
                layout.geometry.Min.y = parent.nextpos.y + layout.spacing.y;

                if (sizing.vertical != FIT_SZ)
                {
                    auto vmeasure = sizing.relativev ?
                        (parent.geometry.GetHeight() * sizing.vertical) :
                        sizing.vertical;
                    layout.geometry.Max.y = layout.geometry.Min.y + vmeasure;
                }
                else
                {
                    layout.sizing &= ~ExpandV;
                    layout.geometry.Max.y = layout.geometry.Min.y + layout.spacing.y;
                }

                parent.nextpos.y = layout.geometry.Max.y + parent.spacing.y;
            }
            else if (parent.alignment & TextAlignBottom)
            {
                layout.geometry.Max.y = parent.prevpos.y - layout.spacing.y;

                if (layout.fill & FD_Vertical) layout.geometry.Min.y = parent.nextpos.y + layout.spacing.y;
                else if (sizing.vertical != FIT_SZ)
                {
                    auto vmeasure = sizing.relativev ?
                        (parent.geometry.GetHeight() * sizing.vertical) :
                        sizing.vertical;
                    layout.geometry.Min.y = layout.geometry.Max.y - vmeasure;
                }

                parent.prevpos.y = layout.geometry.Max.y - parent.spacing.y;
            }
        }
        else
        {
            layout.geometry.Max = ImGui::GetCurrentWindow()->Size;
        }

        layout.geometry.Min += ImVec2{ style.border.left.thickness, style.border.top.thickness };
        layout.geometry.Max -= ImVec2{ style.border.right.thickness, style.border.bottom.thickness };
        layout.nextpos = layout.geometry.Min + layout.spacing;

        if (layout.sizing & ExpandH) layout.prevpos.x = layout.geometry.Max.x - layout.spacing.x;
        if (layout.sizing & ExpandV) layout.prevpos.y = layout.geometry.Max.y - layout.spacing.y;
    }

    ImRect BeginLayout(Layout type, FillDirection fill, int32_t alignment, ImVec2 spacing, const NeighborWidgets& neighbors)
    {
        assert(Context.currSizingDepth > -1);
        Context.currLayoutDepth++;

        const auto& style = Context.currStyleStates == 0 ? Context.pushedStyles[WSI_Default].top() : Context.currStyle[WSI_Default];
        auto& layout = Context.layouts[Context.currLayoutDepth];
        layout.type = type;
        layout.alignment = alignment;
        layout.fill = fill;
        layout.spacing = spacing;

        ComputeInitialGeometry(layout, style);
        return layout.geometry;
    }

    ImRect BeginLayout(Layout type, std::string_view css, const NeighborWidgets& neighbors)
    {
        Context.currLayoutDepth++;

        auto& layout = Context.layouts[Context.currLayoutDepth];
        const auto& style = Context.currStyleStates == 0 ? Context.pushedStyles[WSI_Default].top() : Context.currStyle[WSI_Default];
        layout.type = type;
        auto pwidth = Context.currLayoutDepth > 0 ? Context.layouts[Context.currLayoutDepth - 1].geometry.GetWidth() : 1.f;
        auto pheight = Context.currLayoutDepth > 0 ? Context.layouts[Context.currLayoutDepth - 1].geometry.GetHeight() : 1.f;
        auto [sizing, hasSizing ] = ParseLayoutStyle(layout, css, pwidth, pheight);

        if (hasSizing)
        {
            PushSizing(sizing.horizontal, sizing.vertical, sizing.relativeh, sizing.relativev);
            layout.popSizingOnEnd = true;
        }

        ComputeInitialGeometry(layout, style);
        return layout.geometry;
    }

    void PushSizing(float width, float height, bool relativew, bool relativeh)
    {
        Context.currSizingDepth++;
        auto& sizing = Context.sizing[Context.currSizingDepth];
        sizing.horizontal = width;
        sizing.vertical = height;
        sizing.relativeh = relativew;
        sizing.relativev = relativeh;
    }

    void PopSizing(int depth)
    {
        while (depth > 0 && Context.currSizingDepth > 0)
        {
            Context.sizing[Context.currSizingDepth] = Sizing{};
            Context.currSizingDepth--;
            depth--;
        }
    }

    // Declarations of widget drawing impls:
    WidgetDrawResult ButtonImpl(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding, const ImRect& content, const ImRect& textpos, IRenderer& renderer);
    WidgetDrawResult LabelImpl(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding, const ImRect& content, const ImRect& textpos, IRenderer& renderer);

    ImRect EndLayout(int depth)
    {
        ImRect res;
        auto& style = Context.currStyleStates == 0 ? Context.pushedStyles[WSI_Default].top() : Context.currStyle[WSI_Default];

        while (depth > 0 && Context.currLayoutDepth > -1)
        {
            // Align items in last row/column and align overall items
            // in cross-axis for flex-row/column layouts
            auto& layout = Context.layouts[Context.currLayoutDepth];
            AlignLayoutAxisItems(layout);
            AlignCrossAxisItems(layout, depth);
            //Context.layoutItems[layout.itemidx].viewport = layout.nextpos;

            // Update parent's nextpos as this layout is complete. This step is
            // necessary as sublayout may need to perform actual layout before
            // knowing its own dimension
            if (Context.currLayoutDepth > 0)
                Context.layouts[Context.currLayoutDepth - 1].nextpos +=
                ImVec2{ layout.geometry.GetWidth(), layout.geometry.GetHeight() };

            DrawBorderRect(layout.geometry.Min, layout.geometry.Max, style.border, Config.bgcolor,
                *Config.renderer);

            if (layout.popSizingOnEnd) PopSizing();

            if (Context.currLayoutDepth > 0)
            {
                auto& parent = Context.layouts[Context.currLayoutDepth];
                LayoutItemDescriptor desc;
                desc.wtype = WT_Sublayout;
                desc.margin = layout.geometry;
                desc.id = Context.currLayoutDepth;
                AddItemToLayout(parent, desc);
            }

            if (Context.currLayoutDepth == 0)
            {
                /*ImVec2 initpos{0.f, 0.f};
                //int layoutidxs[128];
                //auto totalLayouts = 0;
                //std::fill(std::begin(layoutidxs), std::end(layoutidxs), -1);

                //// Extract all sublayouts
                //for (auto idx = 0; idx < (int)Context.layoutItems.size(); ++idx)
                //{
                //    if (Context.layoutItems[idx].wtype == WT_Sublayout)
                //    {
                //        layoutidxs[totalLayouts++] = idx;
                //        totalLayouts++;
                //    }
                //}

                //// Sort by depth
                //std::sort(std::begin(layoutidxs), std::begin(layoutidxs) + totalLayouts, [](int lhs, int rhs) {
                //        auto lhsdepth = Context.layoutItems[lhs].id;
                //        auto rhsdepth = Context.layoutItems[rhs].id;
                //        return lhsdepth == rhsdepth ? lhs < rhs : lhsdepth < rhsdepth;
                //    });

                /*auto currdepth = 0;
                for (auto layoutidx = 0; layoutidx < totalLayouts; layoutidx++)
                {
                    auto& layout = Context.layoutItems[layoutidx];

                    if (layout.id > 0)
                    {
                        auto width = layout.margin.GetWidth();
                        layout.margin.Min.x = initpos.x;
                        layout.margin.Max.x = layout.margin.Min.x + width;

                    }

                    currdepth = layout.id;
                }*/

                // Handle scroll panes...
                for (auto& widget : Context.layoutItems)
                {
                    switch (widget.wtype)
                    {
                    case WT_Label:
                    {
                        LabelImpl(widget.id, widget.margin, widget.border, widget.padding, widget.content, widget.text,
                            *Config.renderer);
                        break;
                    }
                    case WT_Button:
                    {
                        ButtonImpl(widget.id, widget.margin, widget.border, widget.padding, widget.content, widget.text,
                            *Config.renderer);
                        break;
                    }
                    default:
                        break;
                    }
                }
            }

            res = Context.layouts[Context.currLayoutDepth].geometry;
            Context.layouts[Context.currLayoutDepth] = LayoutDescriptor{};
            --Context.currLayoutDepth;
            --depth;
        }

        return res;
    }

#pragma endregion
}
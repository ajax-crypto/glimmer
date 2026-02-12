#include "layout.h"

#include "context.h"
#include "libs/inc/imgui/imgui.h"
#include "style.h"
#include "draw.h"
#include "types.h"
#include "utils.h"
#include "widgets.h"

#include <limits>
#include <cstdint>

#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_CLAY_ENGINE
#define CLAY_IMPLEMENTATION
#include "libs/inc/clay/clay.h"

Clay_Arena LayoutArena;
void* LayoutMemory = nullptr;
#endif

#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_SIMPLE_FLEX_ENGINE
#define LAY_IMPLEMENTATION
#define LAY_FLOAT 1
#include "libs/inc/randrew.layout/layout.h"

struct SimpleFlexTreeRoot
{
    lay_context ctx;
    lay_id root = LAY_INVALID_ID;
    int32_t rootIdx = -1;
    int32_t depth = 0;
    std::vector<std::pair<int32_t, lay_id>> widgets;
    std::vector<std::pair<int32_t, lay_id>> layouts;
    std::vector<lay_id> levelOrderNodes[GLIMMER_MAX_LAYOUT_NESTING];
};

static glimmer::Vector<SimpleFlexTreeRoot, int16_t, 8> LayContexts;
static glimmer::FixedSizeStack<int16_t, GLIMMER_MAX_LAYOUT_NESTING> LayoutRootStack{ false };
static int16_t NextFreeContextIdx = 0;

static ImRect GetBoundingBox(lay_id item, int index)
{
    auto vec4 = lay_get_rect(&(LayContexts[index].ctx), item);
    return ImRect{ vec4[0], vec4[1], vec4[0] + vec4[2], vec4[1] + vec4[3] };
}

void PopFlexLayoutNode()
{
    if (!LayoutRootStack.empty())
    {
        auto top = LayoutRootStack.top();
        auto& root = LayContexts[top];

        if (root.depth > 0) root.depth--;
        else LayoutRootStack.pop(1, false);
    }

    // If root stack is empty => we do not have any parents
    // which are flexbox layout, which implies nodes can be reused
    //if (LayoutRootStack.empty()) NextFreeContextIdx = 0;
}

#endif

#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_YOGA_ENGINE
#include "libs/inc/yoga/Yoga.h"

struct YogaTreeRoot
{
    YGNodeRef root = nullptr;
    int32_t rootIdx = -1; // context.layouts index
    int32_t depth = 0; // depth of current nested yoga nodes from root
    std::vector<std::pair<int32_t, YGNodeRef>> widgets; // pair of context.layoutItems index & yoga node
    std::vector<std::pair<int32_t, YGNodeRef>> layouts; // pair of context.layouts index & yoga node
    std::vector<YGNodeRef> levelOrderNodes[GLIMMER_MAX_LAYOUT_NESTING];
};

static glimmer::Vector<YogaTreeRoot, int16_t, 8> FlexLayoutRoots;
static glimmer::Vector<YGNodeRef, int16_t, 32> AllFlexItems;
static glimmer::FixedSizeStack<int16_t, GLIMMER_MAX_LAYOUT_NESTING> FlexLayoutRootStack{ false };
static std::unordered_map<glimmer::WidgetContextData*, int> YogaFreeNodeStartIndexes;
static int16_t NextFreeNodeIdx = 0;

YGNodeRef GetNewYogaNode(const glimmer::LayoutBuilder& layout, int32_t layoutIdx, bool isWidget, bool isParentFlexLayout)
{
    auto node = (AllFlexItems.size() <= NextFreeNodeIdx) ? YGNodeNew() : AllFlexItems[NextFreeNodeIdx];
    auto rootIdx = FlexLayoutRootStack.empty() || !isParentFlexLayout ? -1 : FlexLayoutRootStack.top();
    if (AllFlexItems.size() <= NextFreeNodeIdx) AllFlexItems.push_back(node);

    if (rootIdx == -1)
    {
        auto& root = FlexLayoutRoots.emplace_back();
        root.root = node;
        root.rootIdx = layoutIdx;
        FlexLayoutRootStack.push() = FlexLayoutRoots.size() - 1;
    }
    else
    {
        auto& root = FlexLayoutRoots[rootIdx];

        if (isWidget)
        {
            auto index = layout.itemIndexes.back().first;
            root.widgets.emplace_back(index, node);
        }
        else root.layouts.emplace_back(layoutIdx, node);

        root.levelOrderNodes[root.depth].push_back(node);
        if (!isWidget) root.depth++;
    }

    NextFreeNodeIdx++;
    return node;
}

void ResetYogaRoot(YogaTreeRoot& root)
{
    if (root.root == nullptr) return;

    YGNodeRemoveAllChildren(root.root);

    for (auto depth = GLIMMER_MAX_LAYOUT_NESTING - 1; depth >= 0; --depth)
    {
        if (depth > 0)
        {
            for (auto node : root.levelOrderNodes[depth - 1])
                YGNodeRemoveAllChildren(node);
        }

        for (auto node : root.levelOrderNodes[depth])
            YGNodeReset(node);

        root.levelOrderNodes[depth].clear();
    }

    YGNodeReset(root.root);
    root.root = nullptr;
    root.widgets.clear();
    root.layouts.clear();
    root.depth = 0;
}

void PopYogaLayoutNode()
{
    if (!FlexLayoutRootStack.empty())
    {
        auto top = FlexLayoutRootStack.top();
        auto& root = FlexLayoutRoots[top];

        if (root.depth > 0) root.depth--;
        else
        {
            FlexLayoutRootStack.pop(1, false);
            ResetYogaRoot(root);
        }
    }
}

void ResetYogaLayoutSystem(int from)
{
    for (auto idx = from; idx < FlexLayoutRoots.size(); ++idx)
        ResetYogaRoot(FlexLayoutRoots[idx]);

    NextFreeNodeIdx = std::max(0, from);

    if (NextFreeNodeIdx == 0)
    {
        FlexLayoutRootStack.clear(false);
        FlexLayoutRoots.clear(false);
    }
}

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
    static Vector<GridLayoutItem, int16_t, 64> GridLayoutItems;

    std::tuple<ImRect, ImRect, ImRect, ImRect> GetBoxModelBounds(ImRect content, const StyleDescriptor& style);
    WidgetDrawResult RenderWidgetPostLayout(LayoutItemDescriptor& item, const IODescriptor& io, bool render);
    WidgetDrawResult EndPopUp(WidgetContextData& overlayctx, std::optional<uint32_t> bgcoloropt, int32_t flags);
    std::pair<int32_t, bool> GetIdFromString(std::string_view id, WidgetType type);
    WidgetDrawResult RegionImpl(int32_t id, const StyleDescriptor& style, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, IRenderer& renderer, const IODescriptor& io, int depth);

#pragma region Layout functions

    void Move(int32_t direction)
    {
        auto& context = GetContext();
        if (!context.layoutStack.empty()) return;
        auto& layout = context.adhocLayout.top();
        assert(layout.lastItemId != -1);
        Move(layout.lastItemId, direction);
    }

    void Move(int32_t id, int32_t direction)
    {
        auto& context = GetContext();
        if (!context.layoutStack.empty()) return;
        const auto geometry = context.GetGeometry(id);
        auto& layout = context.adhocLayout.top();
        layout.nextpos = geometry.Min;
        if (direction & FD_Horizontal) layout.nextpos.x = geometry.Max.x;
        if (direction & FD_Vertical) layout.nextpos.y = geometry.Max.y;
        if (!layout.addedOffset && layout.insideContainer) layout.addedOffset = true;
    }

    void Move(std::string_view id, int32_t direction)
    {
        Move(GetIdFromString(id, WT_Layout).first, direction);
    }

    void Move(int32_t hid, int32_t vid, bool toRight, bool toBottom)
    {
        auto& context = GetContext();
        if (!context.layoutStack.empty()) return;
        const auto hgeometry = context.GetGeometry(hid);
        const auto vgeometry = context.GetGeometry(vid);
        auto& layout = context.adhocLayout.top();
        layout.nextpos.x = toRight ? hgeometry.Max.x : hgeometry.Min.x;
        layout.nextpos.y = toBottom ? vgeometry.Max.y : vgeometry.Min.y;
        if (!layout.addedOffset && layout.insideContainer) layout.addedOffset = true;
    }

    void Move(std::string_view hid, std::string_view vid, bool toRight, bool toBottom)
    {
        Move(GetIdFromString(hid, WT_Layout).first, GetIdFromString(vid, WT_Layout).first,
            toRight, toBottom);
    }

    void Move(ImVec2 amount, int32_t direction)
    {
        auto& context = GetContext();
        if (!context.layoutStack.empty()) return;
        if (direction & ToLeft) amount.x = -amount.x;
        if (direction & ToTop) amount.y = -amount.y;
        auto& layout = context.adhocLayout.top();
        layout.nextpos += amount;
        if (!layout.addedOffset && layout.insideContainer) layout.addedOffset = true;
    }

    void Move(ImVec2 pos)
    {
        auto& context = GetContext();
        if (!context.layoutStack.empty()) return;
        auto& layout = context.adhocLayout.top();
        layout.nextpos = pos;
        if (!layout.addedOffset && layout.insideContainer) layout.addedOffset = true;
    }

    void AddSpacing(ImVec2 spacing)
    {
        auto& context = GetContext();
        if (!context.layoutStack.empty()) return;
        auto& layout = context.adhocLayout.top();
        layout.nextpos += spacing;
        if (!layout.addedOffset && layout.insideContainer) layout.addedOffset = true;
    }

    void MoveDown()
    {
        Move(DIR_Vertical);
    }

    void MoveUp()
    {
        auto& context = GetContext();
        auto& layout = context.adhocLayout.top();
        Move(layout.lastItemId, layout.lastItemId, true, false);
    }

    void MoveLeft()
    {
        auto& context = GetContext();
        auto& layout = context.adhocLayout.top();
        Move(layout.lastItemId, layout.lastItemId, false, true);
    }

    void MoveRight()
    {
        Move(DIR_Horizontal);
    }

    void MoveBelow(int32_t id)
    {
        Move(id, FD_Vertical);
    }

    void MoveAbove(int32_t id)
    {
        auto& context = GetContext();
        if (!context.layoutStack.empty()) return;
        const auto geometry = context.GetGeometry(id);
        auto& layout = context.adhocLayout.top();
        layout.nextpos.y = geometry.Min.y;
        if (!layout.addedOffset && layout.insideContainer) layout.addedOffset = true;
    }

    void MoveRight(int32_t id)
    {
        Move(id, FD_Horizontal);
    }

    void MoveLeft(int32_t id)
    {
        auto& context = GetContext();
        if (!context.layoutStack.empty()) return;
        const auto geometry = context.GetGeometry(id);
        auto& layout = context.adhocLayout.top();
        layout.nextpos.x = geometry.Min.x;
        if (!layout.addedOffset && layout.insideContainer) layout.addedOffset = true;
    }

    void MoveBelow(std::string_view id)
    {
        MoveBelow(GetIdFromString(id, WT_Layout).first);
    }

    void MoveAbove(std::string_view id)
    {
        MoveAbove(GetIdFromString(id, WT_Layout).first);
    }

    void MoveRight(std::string_view id)
    {
        MoveRight(GetIdFromString(id, WT_Layout).first);
    }

    void MoveLeft(std::string_view id)
    {
        MoveLeft(GetIdFromString(id, WT_Layout).first);
    }

    static void ComputeExtent(LayoutItemDescriptor& layoutItem, ImVec2 nextpos, const StyleDescriptor& style, 
        const NeighborWidgets& neighbors, float width, float height, ImVec2 totalsz)
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
            auto w = style.relative & StyleWidth ?
                clamp(style.dimension.x, style.mindim.x, style.maxdim.x) :
                clamp(style.dimension.x, style.mindim.x, style.maxdim.x);

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
            auto type = context.GetConfig(layoutItem.id).config.scroll.type;
            if (type & ST_Always_H)
            {
                layoutItem.border.Max.x -= Config.scrollbar.width;
                layoutItem.padding.Max.x -= Config.scrollbar.width;
                layoutItem.content.Max.x -= Config.scrollbar.width;
            }

            if (type & ST_Always_V)
            {
                layoutItem.border.Max.y -= Config.scrollbar.width;
                layoutItem.padding.Max.y -= Config.scrollbar.width;
                layoutItem.content.Max.y -= Config.scrollbar.width;
            }
        }
    }

    void AddExtent(LayoutItemDescriptor& layoutItem, const StyleDescriptor& style, const NeighborWidgets& neighbors,
        float width, float height)
    {
        auto& context = GetContext();
        auto totalsz = context.MaximumExtent();
        auto nextpos = !context.layoutStack.empty() ? context.layouts[context.layoutStack.top()].nextpos : context.NextAdHocPos();
        AddDefaultDirection(layoutItem);

        if (width <= 0.f) width = clamp(totalsz.x - nextpos.x, style.mindim.x, style.maxdim.x);
        if (height <= 0.f) height = clamp(totalsz.y - nextpos.y, style.mindim.y, style.maxdim.y);

        ComputeExtent(layoutItem, nextpos, style, neighbors, width, height, totalsz);
        ReserveSpaceForScrollBars(context, layoutItem);
    }

    void AddExtent(LayoutItemDescriptor& layoutItem, const StyleDescriptor& style, const NeighborWidgets& neighbors,
        ImVec2 size, ImVec2 totalsz)
    {
        auto& context = GetContext();
        auto nextpos = !context.layoutStack.empty() ? context.layouts[context.layoutStack.top()].nextpos : context.NextAdHocPos();
        auto [width, height] = size;
        AddDefaultDirection(layoutItem);

        if (width <= 0.f) width = clamp(totalsz.x - nextpos.x, style.mindim.x, style.maxdim.x);
        if (height <= 0.f) height = clamp(totalsz.y - nextpos.y, style.mindim.y, style.maxdim.y);

        ComputeExtent(layoutItem, nextpos, style, neighbors, width, height, totalsz);
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

#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_YOGA_ENGINE

#ifdef _DEBUG
    static void ValidateYogaNodes(YGNodeRef parent, YGNodeRef child)
    {
        auto pnode = YGNodeGetParent(child);
        assert(pnode == parent);
        std::unordered_map<YGNodeRef, bool> visited;
        visited[child] = true;
        while (pnode != nullptr)
        {
            assert(visited.count(pnode) == 0);
            visited[pnode] = true;
            pnode = YGNodeGetParent(pnode);
        }
    }
#endif

    static void SetYogaNodeSizingProperties(YGNodeRef root, const StyleDescriptor& style, ImVec2 size, int32_t fill)
    {
        if (!(fill & FD_Horizontal))
            if (size.x > 0.f)
                YGNodeStyleSetWidth(root, size.x);
            else if (style.relative & StyleWidth)
                YGNodeStyleSetWidthPercent(root, style.dimension.x * 100.f);
            else if (style.dimension.x > 0.f)
                YGNodeStyleSetWidth(root, style.dimension.x);

        if (fill & FD_Horizontal)
            YGNodeStyleSetWidthPercent(root, 100);
        else if (style.maxdim.x != FLT_MAX)
            if (style.relative & StyleMaxWidth)
                YGNodeStyleSetMaxWidthPercent(root, style.maxdim.x * 100.f);
            else
                YGNodeStyleSetMaxWidth(root, style.maxdim.x);

        if (style.mindim.x > 0.f)
            if (style.relative & StyleMaxWidth)
                YGNodeStyleSetMinWidthPercent(root, style.mindim.x * 100.f);
            else
                YGNodeStyleSetMinWidth(root, style.mindim.x);

        if (!(fill & FD_Vertical))
            if (size.y > 0.f)
                YGNodeStyleSetHeight(root, size.y);
            else if (style.relative & StyleHeight)
                YGNodeStyleSetHeightPercent(root, style.dimension.y * 100.f);
            else if (style.dimension.y > 0.f)
                YGNodeStyleSetHeight(root, style.dimension.y);

        if (fill & FD_Vertical)
            YGNodeStyleSetHeightPercent(root, 100);
        else if (style.maxdim.y != FLT_MAX)
            if (style.relative & StyleMaxWidth)
                YGNodeStyleSetMaxHeightPercent(root, style.maxdim.y * 100.f);
            else
                YGNodeStyleSetHeight(root, style.maxdim.y);

        if (style.mindim.y > 0.f)
            if (style.relative & StyleMaxWidth)
                YGNodeStyleSetMinHeightPercent(root, style.mindim.y * 100.f);
            else
                YGNodeStyleSetMinHeight(root, style.mindim.y);
    }

#endif

    void AddItemToLayout(LayoutBuilder& layout, LayoutItemDescriptor& item, const StyleDescriptor& style)
    {
        auto& context = GetContext();
        auto isItemLayout = item.wtype == WT_Layout;

        if (!isItemLayout)
            layout.itemIndexes.emplace_back(context.layoutItems.size(), 
                item.wtype == WT_Scrollable ? LayoutOps::PushScrollRegion : LayoutOps::AddWidget);

        if (layout.type == Layout::Horizontal || layout.type == Layout::Vertical)
        {
            if (!WidgetContextData::CacheItemGeometry)
            {
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
                                layout.spacing.y };
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

                YGNodeRef child = GetNewYogaNode(layout, context.layoutStack.top(), !isItemLayout, true);

                auto fill = 0;
                if (item.sizing & ExpandH) fill = FD_Horizontal;
                if (item.sizing & ExpandV) fill |= FD_Vertical;
                SetYogaNodeSizingProperties(child, style, item.margin.GetSize(), fill);

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

                // Associate child with corresponding parent node
                auto parent = static_cast<YGNodeRef>(layout.implData);
                assert(parent != child);
                YGNodeInsertChild(parent, child, YGNodeGetChildCount(parent));
                item.implData = child;

#ifdef _DEBUG
                ValidateYogaNodes(parent, child);
#endif
                

#elif GLIMMER_FLEXBOX_ENGINE == GLIMMER_SIMPLE_FLEX_ENGINE

                auto& root = LayContexts[LayoutRootStack.top()];
                auto parent = reinterpret_cast<lay_id>(layout.implData);

                auto litem = isItemLayout ? reinterpret_cast<lay_id>(item.implData) : lay_item(&root.ctx);
                lay_set_size_xy(&root.ctx, litem, item.margin.GetWidth(), item.margin.GetHeight());

                if (item.sizing & ExpandH) lay_set_behave(&root.ctx, litem, LAY_HFILL);
                else if (item.sizing & AlignHCenter) lay_set_behave(&root.ctx, litem, LAY_HCENTER);
                else if (item.sizing & AlignLeft) lay_set_behave(&root.ctx, litem, LAY_LEFT);
                else if (item.sizing & AlignRight) lay_set_behave(&root.ctx, litem, LAY_RIGHT);

                if (item.sizing & ExpandV) lay_set_behave(&root.ctx, litem, LAY_VFILL);
                else if (item.sizing & AlignVCenter) lay_set_behave(&root.ctx, litem, LAY_VCENTER);
                else if (item.sizing & AlignTop) lay_set_behave(&root.ctx, litem, LAY_TOP);
                else if (item.sizing & AlignBottom) lay_set_behave(&root.ctx, litem, LAY_BOTTOM);

                lay_insert(&root.ctx, parent, litem);

                if (!isItemLayout)
                {
                    item.implData = reinterpret_cast<void*>(litem);
                    root.widgets.emplace_back((int32_t)context.layoutItems.size(), litem);
                }

#endif
            }

            // Record this widget for rendering once geometry is determined
            if (!isItemLayout)
                context.RecordForReplay(context.layoutItems.size(), LayoutOps::AddWidget);
        }
        else if (layout.type == Layout::Grid)
        {
            if (!WidgetContextData::CacheItemGeometry)
            {
                auto& griditem = GridLayoutItems.emplace_back();
                griditem.maxdim = item.margin.GetSize();
                griditem.row = layout.currow; griditem.col = layout.currcol;
                griditem.rowspan = layout.currspan.first;
                griditem.colspan = layout.currspan.second;
                griditem.index = GetContext().layoutItems.size();
                griditem.alignment = item.sizing;
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
                item.implData = reinterpret_cast<void*>((intptr_t)(GridLayoutItems.size() - 1));
            }

            if (!isItemLayout)
                context.RecordForReplay(context.layoutItems.size(), LayoutOps::AddWidget);
        }
        else
            context.RecordForReplay(item.id, LayoutOps::PushScrollRegion);

        item.layoutIdx = context.layoutStack.top();
        context.layoutItems.push_back(item);

#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_FLAT_ENGINE
        if (layout.from == -1) layout.from = layout.to = (int16_t)(context.layoutItems.size() - 1);
        else layout.to = (int16_t)(context.layoutItems.size() - 1);
        layout.itemidx = layout.to;
#endif

        if (item.wtype == WT_Scrollable)
        {
            context.layoutStack.push() = context.layouts.size();
            auto& scroll = context.layouts.emplace_back();
            scroll.id = item.id;
            scroll.type = Layout::ScrollRegion;
            scroll.itemidx = context.layoutItems.size() - 1;
            if (context.layoutStack.size() > 1) scroll.parentIdx = context.layoutStack.top(1);
            context.layoutItems.back().layoutIdx = context.layoutStack.push();
        }
        else if (!isItemLayout && context.layoutStack.size() > 1)
        {
            auto& layout = context.layouts[context.layoutStack.top(1)];
            if (layout.type == Layout::ScrollRegion)
                context.layoutItems.back().scrollid = layout.id;
        }
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
        if (context.layoutStack.size() > 1)
        {
            auto idx = context.layoutStack.top(1);
            auto& parent = context.layouts[idx];
            LayoutItemDescriptor item;
            StyleDescriptor style;
            item.id = layout.id;
            item.margin = available;
            item.wtype = WT_Layout;
            item.implData = layout.implData;
            parent.itemIndexes.emplace_back(context.layoutItems.size(), LayoutOps::AddLayout);
            layout.itemidx = context.layoutItems.size();
            AddItemToLayout(parent, item, style);
        }
    }

    static bool IsParentFlexLayout(const WidgetContextData& context)
    {
        if (context.layoutStack.size() == 1) return false;
        auto parentLayoutType = context.layouts[context.layoutStack.top(1)].type;
        return parentLayoutType == Layout::Horizontal ||
            parentLayoutType == Layout::Vertical;
    }

    static void UpdateLayoutStartPos(WidgetContextData& context, LayoutBuilder& layout, ImVec2 nextpos, int regionIdx, const NeighborWidgets& neighbors)
    {
        if (context.layoutStack.size() == 1)
        {
            layout.startpos = nextpos;

            if (regionIdx != -1)
            {
                auto regionId = context.regions[regionIdx].id;
                auto& state = context.GetStateData(regionId);
                auto style = context.GetStyle(regionId);
                layout.startpos += ImVec2{ style.margin.left, style.margin.top };
                //state.neighbors = neighbors;
            }
        }
    }

    // Region size is determined in both axes in similar fashion
    // For any given axis :
    // 1. If `size` parameter is specified, prefer that
    // 2. If region has `style` information, prefer that
    // 3. If region can be filled or wrapping is enabled, prefer available space
    static ImVec2 GetRegionSize(WidgetContextData& context, int32_t regionIdx, const ImRect& available, 
        ImVec2 size, int32_t fill, bool wrap, Layout type, int32_t& specified)
    {
        ImVec2 res{ -1.f, -1.f };
        specified = specified | (StyleWidth | StyleHeight);

        if (regionIdx != -1)
        {
            auto rid = context.regions[regionIdx].id;
            auto style = context.GetStyle(rid);

            if (size.x > 0.f)
                res.x = clamp(size.x, style.mindim.x, style.maxdim.x);
            else if (style.relative & StyleWidth)
                res.x = clamp(available.GetWidth() * style.dimension.x, style.mindim.x, style.maxdim.x);
            else if (style.dimension.x > 0.f)
                res.x = clamp(style.dimension.x, style.mindim.x, style.maxdim.x);
            else if ((wrap && (type == Layout::Horizontal)) || (fill & FD_Horizontal))
            {
                specified = specified & ~StyleWidth;
                res.x = available.GetWidth();
            }

            if (size.y > 0.f)
                res.y = clamp(size.y, style.mindim.y, style.maxdim.y);
            else if (style.relative & StyleHeight)
                res.y = clamp(available.GetHeight() * style.dimension.y, style.mindim.y, style.maxdim.y);
            else if (style.dimension.y > 0.f)
                res.y = clamp(style.dimension.y, style.mindim.y, style.maxdim.y);
            else if ((wrap && (type == Layout::Vertical)) || (fill & FD_Vertical))
            {
                specified = specified & ~StyleHeight;
                res.y = available.GetHeight();
            }
        }
        else
        {
            if (size.x > 0.f) res.x = size.x;
            else if ((wrap && (type == Layout::Horizontal)) || (fill & FD_Horizontal))
            {
                specified = specified & ~StyleWidth;
                res.x = available.GetWidth();
            }

            if (size.y > 0.f) res.y = size.y;
            else if ((wrap && (type == Layout::Vertical)) || (fill & FD_Vertical))
            {
                specified = specified & ~StyleHeight;
                res.y = available.GetHeight();
            }
        }

        return res;
    }

    static std::pair<ImRect, ImVec2> InitLayoutNode(WidgetContextData& context, LayoutBuilder& layout, int32_t id, int32_t geometry,
        ImVec2 spacing, ImVec2 size, const NeighborWidgets& neighbors)
    {
        auto& el = context.nestedContextStack.push();
        el.source = NestedContextSourceType::Layout;
        layout.id = id;
        context.maxids[WT_Layout]++;

        layout.alignment = geometry & ~ExpandAll;
        layout.spacing = spacing;
        layout.size = size;
        if (geometry & ExpandH) layout.fill = FD_Horizontal;
        if (geometry & ExpandV) layout.fill |= FD_Vertical;
        if (size.x > 0.f) layout.alignment |= ExplicitH;
        if (size.y > 0.f) layout.alignment |= ExplicitV;

        // Record style stack states for context, will be restored in EndLayout()
        /*for (auto idx = 0; idx < WSI_Total; ++idx)
            layout.styleStartIdx[idx] = context.StyleStack[idx].size() - 1;*/

        auto nextpos = context.layoutStack.size() == 1 ? context.NextAdHocPos() :
            context.layouts[context.layoutStack.top(1)].nextpos;
        ImRect available = context.layoutStack.size() == 1 ? GetAvailableSpace(nextpos, neighbors) : ImRect{};
        if (size.x > 0.f) available.Max.x = available.Min.x + size.x;
        if (size.y > 0.f) available.Max.y = available.Min.x + size.y;

        return { available, nextpos };
    }

    ImRect BeginFlexLayoutRegion(Direction dir, int32_t geometry, bool wrap,
        ImVec2 spacing, ImVec2 size, const NeighborWidgets& neighbors, int regionIdx)
    {
        auto& context = GetContext();
        auto id = (WT_Layout << WidgetTypeBits) | context.maxids[WT_Layout];

        // Only top-level layouts can have neighbors
        assert(context.layoutStack.size() == 0 || (neighbors.bottom == neighbors.top && neighbors.top == neighbors.left &&
            neighbors.left == neighbors.right && neighbors.right == -1));

#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_FLAT_ENGINE
        assert(context.layoutStack.empty()); // No nested layout is supported
#endif

        auto& layout = context.layouts.next(true);
        context.layoutStack.push() = context.layouts.size() - 1;
        if (context.layoutStack.size() > 1) layout.parentIdx = context.layoutStack.top(1);
        
        auto isParentFlexLayout = IsParentFlexLayout(context);
        auto [available, nextpos] = InitLayoutNode(context, layout, id, geometry, spacing, size, neighbors);

        layout.type = dir == DIR_Horizontal ? Layout::Horizontal : Layout::Vertical;
        layout.type == Layout::Horizontal ? (layout.hofmode = wrap ? OverflowMode::Wrap : OverflowMode::Scroll) :
            (layout.vofmode = wrap ? OverflowMode::Wrap : OverflowMode::Scroll);

        if (!WidgetContextData::CacheItemGeometry)
        {
#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_CLAY_ENGINE

            if (context.layoutStack.size() == 1)
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

            auto root = GetNewYogaNode(layout, context.layoutStack.top(), false, isParentFlexLayout);
            layout.implData = root;

            if (context.layoutStack.size() == 1)
            {
                auto [width, height] = GetRegionSize(context, regionIdx, available, size, layout.fill, wrap, layout.type, layout.specified);
                if (width >= 0.f)
                    YGNodeStyleSetWidth(root, width);
                if (height >= 0.f)
                    YGNodeStyleSetHeight(root, height);
            }
            else
            {
                if (regionIdx != -1)
                {
                    auto rid = context.regions[regionIdx].id;
                    auto style = context.GetStyle(rid);
                    SetYogaNodeSizingProperties(root, style, size, layout.fill);
                }
                else
                {
                    if (size.x > 0.f)
                        YGNodeStyleSetWidth(root, size.x);
                    else if (layout.fill & FD_Horizontal)
                        YGNodeStyleSetWidthPercent(root, 100);

                    if (size.y > 0.f)
                        YGNodeStyleSetHeight(root, size.y);
                    else if (layout.fill & FD_Vertical)
                        YGNodeStyleSetHeightPercent(root, 100);
                }
            }

            YGNodeStyleSetFlexDirection(root, layout.type == Layout::Horizontal ? YGFlexDirectionRow : YGFlexDirectionColumn);
            YGNodeStyleSetFlexWrap(root, wrap ? YGWrapWrap : YGWrapNoWrap);
            YGNodeStyleSetPosition(root, YGEdgeLeft, 0.f);
            YGNodeStyleSetPosition(root, YGEdgeTop, 0.f);
            YGNodeStyleSetGap(root, YGGutterRow, spacing.x);
            YGNodeStyleSetGap(root, YGGutterColumn, spacing.y);

            if (layout.type == Layout::Horizontal)
            {
                // Main axis alignment
                if (geometry & AlignRight) YGNodeStyleSetJustifyContent(root, YGJustifyFlexEnd);
                else if (geometry & AlignHCenter) YGNodeStyleSetJustifyContent(root, YGJustifyCenter);
                else if (geometry & AlignJustify) YGNodeStyleSetJustifyContent(root, YGJustifySpaceAround);
                else YGNodeStyleSetJustifyContent(root, YGJustifyFlexStart);

                // Cross axis alignment
                if (geometry & AlignBottom) YGNodeStyleSetAlignItems(root, YGAlignFlexEnd);
                else if (geometry & AlignVCenter) YGNodeStyleSetAlignItems(root, YGAlignCenter);
                else YGNodeStyleSetAlignItems(root, YGAlignFlexStart);
            }
            else
            {
                // Main axis alignment
                if (geometry & AlignBottom) YGNodeStyleSetJustifyContent(root, YGJustifyFlexEnd);
                else if (geometry & AlignVCenter) YGNodeStyleSetJustifyContent(root, YGJustifyCenter);
                else YGNodeStyleSetJustifyContent(root, YGJustifyFlexStart);

                // Cross axis alignment
                if (geometry & AlignRight) YGNodeStyleSetAlignItems(root, YGAlignFlexEnd);
                else if (geometry & AlignHCenter) YGNodeStyleSetAlignItems(root, YGAlignCenter);
                else YGNodeStyleSetAlignItems(root, YGAlignFlexStart);
            }

            // If layout is a region, add spacing for margin/border/padding
            if (regionIdx != -1)
            {
                auto rid = context.regions[regionIdx].id;
                auto style = context.GetStyle(rid);

                YGNodeStyleSetMargin(root, YGEdgeTop, style.margin.top);
                YGNodeStyleSetMargin(root, YGEdgeBottom, style.margin.bottom);
                YGNodeStyleSetMargin(root, YGEdgeLeft, style.margin.left);
                YGNodeStyleSetMargin(root, YGEdgeRight, style.margin.right);

                YGNodeStyleSetPadding(root, YGEdgeTop, style.padding.top);
                YGNodeStyleSetPadding(root, YGEdgeBottom, style.padding.bottom);
                YGNodeStyleSetPadding(root, YGEdgeLeft, style.padding.left);
                YGNodeStyleSetPadding(root, YGEdgeRight, style.padding.right);

                YGNodeStyleSetBorder(root, YGEdgeTop, style.border.top.thickness);
                YGNodeStyleSetBorder(root, YGEdgeBottom, style.border.bottom.thickness);
                YGNodeStyleSetBorder(root, YGEdgeLeft, style.border.left.thickness);
                YGNodeStyleSetBorder(root, YGEdgeRight, style.border.right.thickness);
            }

            if (!isParentFlexLayout)
            {
                AddLayoutAsChildItem(context, layout, available);
            }
            else if (context.layoutStack.size() > 1)
            {
                auto idx = context.layoutStack.top(1);
                auto parent = static_cast<YGNodeRef>(context.layouts[idx].implData);
                assert(parent != root);
                YGNodeInsertChild(parent, root, YGNodeGetChildCount(parent));
#ifdef _DEBUG
                ValidateYogaNodes(parent, root);
#endif
            }

#elif GLIMMER_FLEXBOX_ENGINE == GLIMMER_SIMPLE_FLEX_ENGINE

            auto& root = [&layout, &context]() -> SimpleFlexTreeRoot& {
                if (NextFreeContextIdx >= LayContexts.size())
                {
                    auto& root = LayContexts.emplace_back();
                    lay_init_context(&root.ctx);
                    lay_reserve_items_capacity(&root.ctx, 16);
                }

                LayoutRootStack.push() = NextFreeContextIdx;
                NextFreeContextIdx++;
                return LayContexts[LayoutRootStack.top()];
                }();
            auto litem = lay_item(&root.ctx);
            layout.implData = reinterpret_cast<void*>(litem);
            if (isParentFlexLayout) root.layouts.emplace_back(context.layoutStack.top(), litem);
            else { root.root = litem; root.rootIdx = context.layoutStack.top(); }

            auto width = available.GetWidth(), height = available.GetHeight();
            if ((layout.fill & FD_Horizontal) && (available.Max.x != FLT_MAX) && (available.Max.x > 0.f))
                width = available.GetWidth();
            if ((layout.fill & FD_Vertical) && (available.Max.y != FLT_MAX) && (available.Max.y > 0.f))
                height = available.GetHeight();

            lay_set_size_xy(&root.ctx, litem, width, height);

            auto flags = (uint32_t)LAY_FLEX;
            flags |= layout.type == Layout::Horizontal ? LAY_ROW : LAY_COLUMN;
            if (wrap) flags |= LAY_WRAP;
            if (layout.alignment & AlignJustify) flags |= LAY_JUSTIFY;
            lay_set_contain(&root.ctx, litem, flags);

            // If layout is a region, add spacing for margin/border/padding
            if (regionIdx != -1)
            {
                auto rid = context.regions[regionIdx].id;
                auto style = context.GetStyle(rid);

                lay_set_margins_ltrb(&root.ctx, litem,
                    style.margin.left + style.padding.left + style.border.left.thickness,
                    style.margin.top + style.padding.top + style.border.top.thickness,
                    style.margin.right + style.padding.right + style.border.right.thickness,
                    style.margin.bottom + style.padding.bottom + style.border.bottom.thickness);
            }

            if (!isParentFlexLayout)
            {
                AddLayoutAsChildItem(context, layout, available);
            }
            else if (context.layoutStack.size() > 1)
            {
                auto idx = context.layoutStack.top(1);
                auto parent = reinterpret_cast<lay_id>(context.layouts[idx].implData);
                lay_insert(&root.ctx, parent, litem);
                LayContexts.back().depth++;
            }

#endif
        }
        else
        {
            if (!isParentFlexLayout)
            {
                AddLayoutAsChildItem(context, layout, available);
            }
        }

        layout.available = available;
        layout.extent.Min = { FLT_MAX, FLT_MAX };
        layout.geometry = ImRect{};
        layout.regionIdx = regionIdx;

        UpdateLayoutStartPos(context, layout, nextpos, regionIdx, neighbors);
        layout.nextpos = layout.startpos;
        return layout.geometry;
    }

    ImRect BeginGridLayoutRegion(int rows, int cols, GridLayoutDirection dir, int32_t geometry, const std::initializer_list<float>& rowExtents,
        const std::initializer_list<float>& colExtents, ImVec2 spacing, ImVec2 size, const NeighborWidgets& neighbors, int regionIdx)
    {
        auto& context = GetContext();
        auto id = (WT_Layout << WidgetTypeBits) | context.maxids[WT_Layout];

        // Only top-level layouts can have neighbors
        assert(context.layoutStack.size() == 0 || (neighbors.bottom == neighbors.top && neighbors.top == neighbors.left &&
            neighbors.left == neighbors.right && neighbors.right == -1));
        // No expansion if nested layout, nested layout's size is implicit, or explicit from CSS
        assert(context.layoutStack.size() == 0 || (!(geometry & ExpandH) && !(geometry & ExpandV)));
        // Row/Column extents only apply for top-level layouts or nested layouts with non-zero explicit size
        assert(context.layoutStack.size() == 0 || ((size.x == 0.f && rowExtents.size() == 0) &&
            (size.y == 0.f && colExtents.size() == 0)));
        // For row-wise addition of widgets, columns must be specified to wrap (And vice-versa)
        assert((dir == GridLayoutDirection::ByRows && cols > 0) || (dir == GridLayoutDirection::ByColumns && rows > 0));

        auto& layout = context.layouts.next(true);
        context.layoutStack.push() = context.layouts.size() - 1;
        if (context.layoutStack.size() > 1) layout.parentIdx = context.layoutStack.top(1);

        auto [available, nextpos] = InitLayoutNode(context, layout, id, geometry, spacing, size, neighbors);
        layout.type = Layout::Grid;
        layout.gpmethod = dir;
        layout.gridsz = std::make_pair(rows, cols);

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
        layout.extent.Min = { FLT_MAX, FLT_MAX };
        layout.currow = layout.currcol = 0;
        layout.geometry = ImRect{};
        layout.regionIdx = regionIdx;
        if (rows > 0 && cols > 0) GridLayoutItems.expand(rows * cols, true);

        UpdateLayoutStartPos(context, layout, nextpos, regionIdx, neighbors);
        layout.nextpos = layout.startpos;
        return layout.geometry;
    }

    ImRect BeginFlexLayout(Direction dir, int32_t geometry, bool wrap, ImVec2 spacing, 
        ImVec2 size, const NeighborWidgets& neighbors)
    {
        return BeginFlexLayoutRegion(dir, geometry, wrap, spacing, size, neighbors, -1);
    }

    ImRect BeginGridLayout(int rows, int cols, GridLayoutDirection dir, int32_t geometry, const std::initializer_list<float>& rowExtents,
        const std::initializer_list<float>& colExtents, ImVec2 spacing, ImVec2 size, const NeighborWidgets& neighbors)
    {
        return BeginGridLayoutRegion(rows, cols, dir, geometry, rowExtents, colExtents, spacing, size, neighbors, -1);
    }

    ImRect BeginLayout(std::string_view desc, const NeighborWidgets& neighbors)
    {
        // TODO: Implement layout CSS parsing
        return ImRect{};
    }

    void NextRow()
    {
        if (WidgetContextData::CacheItemGeometry) return;
        auto& context = GetContext();

        if (!context.layoutStack.empty())
        {
            auto idx = context.layoutStack.top();
            auto& layout = context.layouts[idx];

            if (layout.type == Layout::Horizontal && layout.hofmode == OverflowMode::Wrap)
            {
#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_FLAT_ENGINE
                layout.cumulative.y += layout.maxdim.y;
                layout.maxdim.y = 0.f;
                layout.currcol = 0;
                layout.currow++;
                layout.rows[layout.currow].x = 0.f;
#elif GLIMMER_FLEXBOX_ENGINE == GLIMMER_YOGA_ENGINE
                YGNodeRef child = YGNodeNew();
                auto parent = static_cast<YGNodeRef>(layout.implData);
                YGNodeStyleSetWidthPercent(child, 100);
                YGNodeStyleSetHeight(child, 0);
                YGNodeInsertChild(parent, child, YGNodeGetChildCount(parent));
#endif
            }
            else if (layout.type == Layout::Grid)
            {
                layout.currow++;
            }
        }
    }

    void NextColumn()
    {
        if (WidgetContextData::CacheItemGeometry) return;
        auto& context = GetContext();

        if (!context.layoutStack.empty())
        {
            auto idx = context.layoutStack.top();
            auto& layout = context.layouts[idx];

            if (layout.type == Layout::Horizontal && layout.hofmode == OverflowMode::Wrap)
            {
#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_FLAT_ENGINE
                layout.cumulative.x += layout.maxdim.x;
                layout.maxdim.x = 0.f;
                layout.currow = 0;
                layout.currcol++;
                layout.cols[layout.currcol].y = 0.f;
#elif GLIMMER_FLEXBOX_ENGINE == GLIMMER_YOGA_ENGINE
                YGNodeRef child = YGNodeNew();
                auto parent = static_cast<YGNodeRef>(layout.implData);
                YGNodeStyleSetHeightPercent(child, 100);
                YGNodeStyleSetWidth(child, 0);
                YGNodeInsertChild(parent, child, YGNodeGetChildCount(parent));
#endif
            }
            else if (layout.type == Layout::Grid)
            {
                layout.currcol++;
            }
        }
    }

    void ComputeBoxModelGeometry(LayoutItemDescriptor& item, const ImRect& bbox, const StyleDescriptor& style)
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

    //static 

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

    static void ComputeGridBoundingBox(GridLayoutItem& item, const LayoutBuilder& layout, ImVec2 currpos, 
        float totalw, float totalh)
    {
        if (item.alignment & ExpandH)
        {
            item.bbox.Min.x = currpos.x;
            item.bbox.Max.x = totalw + item.bbox.Min.x;
        }
        else
        {
            auto width = (item.alignment & ShrinkH) ? std::min(item.maxdim.x, totalw) : item.maxdim.x;
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
            auto height = (item.alignment & ShrinkV) ? std::min(item.maxdim.y, totalh) : item.maxdim.y;
            VAlignItemInGridCell(item, layout, currpos, totalh);
            item.bbox.Max.y = item.bbox.Min.y + height;
        }
    }

    // If the layout being processed is a nested layout, it either has a user-specified size or
    // implicit size based on child content. Hence, update the layout implementation nodes
    // of parent layout that the child layout has it's relative geometry computed.
    static void UpdateParentNode(WidgetContextData& context, LayoutBuilder& layout)
    {
        if (context.layoutStack.size() > 1)
        {
            auto& parent = context.layouts[context.layoutStack.top(1)];
            auto& item = context.layoutItems[layout.itemidx];
            item.content = item.padding = item.border = item.margin = layout.geometry;
            //LOG("Updating parent layout for child layout: " RECT_FMT "\n", RECT_OUT(item.margin));

            if (parent.type == Layout::Grid)
            {
                auto& griditem = GridLayoutItems[(int16_t)((intptr_t)item.implData)];
                griditem.maxdim = item.margin.GetSize();
            }
            else if (parent.type == Layout::ScrollRegion)
            {
                auto& region = context.ScrollRegion(parent.id);
                region.content = layout.geometry.GetSize();
                parent.geometry = layout.geometry;
            }
        }
    }

    static void PerformGridLayout(LayoutBuilder& layout)
    {
        auto currow = 0, currcol = 0;
        ImVec2 currpos{};
        ImVec2 min{ FLT_MAX, FLT_MAX }, max;

        if (layout.gpmethod == ItemGridPopulateMethod::ByRows)
        {
            // Pre-fill cell geometries of grid layout
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
                    currpos.y += layout.rows[currow].y + (currow == layout.gridsz.first ? 0.f : layout.spacing.y);
                    currpos.x = layout.geometry.Min.x;

                    for (auto col = 0; col < item.col; ++col)
                        currpos.x += layout.cols[col].x + layout.spacing.x;

                    currow = item.row;
                    currcol = 0;
                }
                
                if (item.row == currow)
                {
                    auto [totalw, totalh] = GetTotalSize(item, layout, currow, currcol);
                    ComputeGridBoundingBox(item, layout, currpos, totalw, totalh);
                    currpos.x += totalw + (currcol == layout.gridsz.second ? 0.f : layout.spacing.x);
                    currcol += item.colspan;
                    min = ImMin(min, item.bbox.Min);
                    max = ImMax(max, item.bbox.Max);
                }
                else assert(false);
            }
        }
        else
        {
            // Pre-fill cell geometries of grid layout
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
                    currpos.x += layout.cols[currcol].x + (currcol == layout.gridsz.second ? 0.f : layout.spacing.x);
                    currpos.y = layout.geometry.Min.y;

                    for (auto row = 0; row < item.row; ++row)
                        currpos.y += layout.cols[row].y + layout.spacing.y;

                    currcol = item.col;
                    currow = 0;
                }
                
                if (item.col == currcol)
                {
                    auto [totalw, totalh] = GetTotalSize(item, layout, currow, currcol);
                    ComputeGridBoundingBox(item, layout, currpos, totalw, totalh);
                    currpos.y += totalh + (currow == layout.gridsz.first ? 0.f : layout.spacing.y);
                    currow += item.rowspan;
                    min = ImMin(min, item.bbox.Min);
                    max = ImMax(max, item.bbox.Max);
                }
                else assert(false);
            }
        }

        auto& context = GetContext();
        auto implicitW = max.x - min.x;
        auto implicitH = max.y - min.y;

        if (context.layoutStack.size() == 1)
        {
            layout.geometry.Min = ImVec2{};
            layout.geometry.Max = ImVec2{ 
                (layout.fill & FD_Horizontal) ? std::max(implicitW, layout.available.GetWidth()) : implicitW,
                (layout.fill & FD_Vertical) ? std::max(implicitH, layout.available.GetHeight()) : implicitH };
        }
        else
        {
            layout.geometry.Min = ImVec2{};
            layout.geometry.Max = ImVec2{ implicitW, implicitH };
        }
        
        layout.contentsz = { implicitW, implicitH };
        auto isParentFlex = IsParentFlexLayout(context);
        if (isParentFlex)
        {
            auto& item = context.layoutItems[layout.itemidx];
            item.margin = layout.geometry;

#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_YOGA_ENGINE

            auto node = static_cast<YGNodeRef>(item.implData);
            YGNodeStyleSetWidth(node, layout.geometry.GetWidth());
            YGNodeStyleSetHeight(node, layout.geometry.GetHeight());

#elif GLIMMER_FLEXBOX_ENGINE == GLIMMER_SIMPLE_FLEX_ENGINE

            auto node = reinterpret_cast<lay_id>(item.implData);
            auto& root = LayContexts[LayoutRootStack.top()];
            lay_set_size_xy(&root.ctx, node, layout.geometry.GetWidth(), layout.geometry.GetHeight());

#endif
        }
        else UpdateParentNode(context, layout);

        // Loop through all layout items and update their geometry
        // If an item itself is another layout (implies it was nested)
        // update the corresponding layout's geometry
        for (auto [idx, op] : layout.itemIndexes)
        {
            auto& item = context.layoutItems[(int16_t)idx];
            item.margin = GridLayoutItems[(int16_t)((intptr_t)item.implData)].bbox;
            
            if (item.wtype == WT_Layout && item.layoutIdx != -1)
            {
                auto& layout = context.layouts[item.layoutIdx];
                layout.geometry = item.margin;
                //LOG("Grid Layout sublayout Margin: " RECT_FMT "\n", RECT_OUT(item.margin));
            }
            //else LOG("Grid Layout widget Margin: " RECT_FMT "\n", RECT_OUT(item.margin));
        }
    }

#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_YOGA_ENGINE || GLIMMER_FLEXBOX_ENGINE == GLIMMER_SIMPLE_FLEX_ENGINE
#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_YOGA_ENGINE

    static ImRect UpdateLayoutGeometry(YGNodeRef node, WidgetContextData& context, int lidx)
    {
        auto bbox = GetBoundingBox(node);

#elif GLIMMER_FLEXBOX_ENGINE == GLIMMER_SIMPLE_FLEX_ENGINE

    static void UpdateLayoutGeometry(lay_id node, int ctxidx, WidgetContextData & context, int lidx)
    {
        auto bbox = GetBoundingBox(node, ctxidx);

#endif

        ImVec2 shift{};
        auto& layout = context.layouts[lidx];

		if (layout.regionIdx != -1)
        {
			auto& region = context.regions[layout.regionIdx];
            const auto style = context.GetStyle(region.id);
            shift += { style.padding.right + style.border.right.thickness + style.margin.right,
				style.padding.bottom + style.border.bottom.thickness + style.margin.bottom };
        }

        ImVec2 layoutsz{
            layout.specified & StyleWidth ? bbox.GetWidth() : layout.contentsz.x + shift.x,
            layout.specified & StyleHeight ? bbox.GetHeight() : layout.contentsz.y + shift.y
        };
        layout.geometry.Min = bbox.Min;
        layout.geometry.Max = ImMin(bbox.Max, layout.geometry.Min + layoutsz);

        //LOG("Flexbox Layout sublayout Margin: " RECT_FMT "\n", RECT_OUT(layout.geometry));
        return layout.geometry;
    }

#endif

    void PerformFlexboxLayout(WidgetContextData& context, LayoutBuilder& layout)
    {
#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_FLAT_ENGINE

        AlignLayoutAxisItems(layout);
        AlignCrossAxisItems(layout, depth);

#elif GLIMMER_FLEXBOX_ENGINE == GLIMMER_CLAY_ENGINE

        Clay__CloseElement();
        auto renderCommands = Clay_EndLayout();
        auto widgetidx = 0;

#elif GLIMMER_FLEXBOX_ENGINE == GLIMMER_YOGA_ENGINE

        auto isParentFlex = IsParentFlexLayout(context);

        if (!isParentFlex)
        {
            auto rootNode = static_cast<YGNodeRef>(layout.implData);
            auto& root = FlexLayoutRoots[FlexLayoutRootStack.top()];
            YGNodeCalculateLayout(rootNode, YGUndefined, YGUndefined, YGDirectionLTR);
            ImRect extent{ { FLT_MAX, FLT_MAX }, {} };

            // Extract layout local coordinates for flex-subtree layouts
            for (auto [lidx, node] : root.layouts)
            {
                auto rect = UpdateLayoutGeometry(node, context, lidx);
                extent.Min = ImMin(rect.Min, extent.Min);
                extent.Max = ImMax(rect.Max, extent.Max);
            }

            for (auto [lindex, node] : root.widgets)
            {
                auto& item = context.layoutItems[lindex];
                item.margin = GetBoundingBox(node);
                extent.Min = ImMin(item.margin.Min, extent.Min);
                extent.Max = ImMax(item.margin.Max, extent.Max);
                //LOG("Flexbox Layout widget Margin: " RECT_FMT "\n", RECT_OUT(item.margin));
            }

            layout.contentsz = extent.GetSize();
            UpdateLayoutGeometry(rootNode, context, root.rootIdx);
            UpdateParentNode(context, layout);
        }

#elif GLIMMER_FLEXBOX_ENGINE == GLIMMER_SIMPLE_FLEX_ENGINE

        auto isParentFlex = IsParentFlexLayout(context);

        if (!isParentFlex)
        {
            auto rootNode = reinterpret_cast<lay_id>(layout.implData);
            auto& root = LayContexts[LayoutRootStack.top()];
            lay_run_context(&(root.ctx));

            for (auto [lidx, node] : root.layouts)
                UpdateLayoutGeometry(node, LayoutRootStack.top(), context, lidx);

            UpdateLayoutGeometry(rootNode, LayoutRootStack.top(), context, root.rootIdx);
            UpdateParentNode(context, layout);
        }

#endif
    }

    // Execute layout algorithm and compute layout item geometry in layout local
    static void ComputeLayoutGeometry(WidgetContextData& context, LayoutBuilder& layout)
    {
        if (WidgetContextData::CacheItemGeometry)
        {
			if (layout.type == Layout::ScrollRegion)
                context.RecordForReplay(layout.id, LayoutOps::PopScrollRegion);
            return;
        }

        if (layout.type == Layout::Horizontal || layout.type == Layout::Vertical)
            PerformFlexboxLayout(context, layout);
        else if (layout.type == Layout::Grid) 
            PerformGridLayout(layout);
        else if (layout.type == Layout::ScrollRegion)
        {
            // This is a scroll region inside a layout hierarchy
            // Hence it's content also have to be in a layout hierarchy
            assert(context.layoutStack.size() > 1);
            context.RecordForReplay(layout.id, LayoutOps::PopScrollRegion);
        }
    }

    // Translate layout items by layout.geometry.Min
    static void UpdateItemGeometry(WidgetContextData& context, LayoutItemDescriptor& item, const LayoutBuilder& layout)
    {
        item.margin.Translate(layout.geometry.Min);
        //LOG("Moving Item to: " RECT_FMT "\n", RECT_OUT(item.margin));

        if (item.scrollid != -1)
        {
            const auto& region = context.ScrollRegion(item.scrollid);
            item.margin.Translate(region.barstate.pos);
        }
        else if (item.wtype == WT_Scrollable)
        {
            auto& region = context.ScrollRegion(item.id);
            region.viewport.Translate(item.margin.Min);
            region.content = item.margin.GetSize();
        }
    }

    // Convert layout-local coordinates of widgets to global coordinates
    static void UpdateWidgetGeometryPass(WidgetContextData& context, LayoutBuilder& layout, 
        const IODescriptor& io, RegionStackT& regionStack)
    {
        // An inverted stack at per-depth level
        static Vector<int32_t, int16_t> RegionDrawStack[GLIMMER_MAX_REGION_NESTING];
        int32_t depth = -1;

        for (auto dd = 0; dd < GLIMMER_MAX_REGION_NESTING; ++dd)
            RegionDrawStack[dd].clear(true);

        for (const auto [data, op] : context.replayContent)
        {
            switch (op)
            {
            case LayoutOps::AddWidget:
            {
                if (WidgetContextData::CacheItemGeometry) break;

                auto& item = context.layoutItems[(int16_t)data];
                const auto& sublayout = context.layouts[item.layoutIdx];
                
                if (layout.type == Layout::ScrollRegion)
                    assert(false); // Top level layout is never a scroll-region
                else 
                    UpdateItemGeometry(context, item, sublayout);

                // This does not generate any draw commands but only computes widget geometry
                RenderWidgetPostLayout(item, io, false);
                break;
            }
            case LayoutOps::PushRegion:
            {
                regionStack.push() = (int32_t)data;
                depth++;
                break;
            }
            case LayoutOps::PopRegion:
            {
                auto ridx = regionStack.top();
                RegionDrawStack[depth].push_back(ridx);
                regionStack.pop(1, true);
                depth--;
                break;
            }
            default:
                break;
            }
        }

        for (auto dd = 0; dd < GLIMMER_MAX_REGION_NESTING; ++dd)
        {
            for (auto ridx : RegionDrawStack[dd])
            {
                auto& region = context.regions[ridx];
                auto [content, padding, border, margin] = GetBoxModelBounds(
                    { region.origin, region.origin + region.size }, region.style);
                RegionImpl(region.id, region.style, margin, border, padding, content, 
                    context.GetRenderer(), io, region.depth);
            }
        }
    }

    static void RenderWidgetPass(WidgetContextData& context, LayoutBuilder& layout,
        WidgetDrawResult& result, const IODescriptor& io)
    {
        for (const auto [data, op] : context.replayContent)
        {
            switch (op)
            {
            case LayoutOps::AddWidget:
            {
                auto& item = context.layoutItems[(int16_t)data];
                if (WidgetContextData::CacheItemGeometry) 
                    item.margin = context.GetGeometry(item.id);
                if (auto res = RenderWidgetPostLayout(item, io, true); res.event != WidgetEvent::None)
                    result = res;
                break;
            }
            case LayoutOps::PushRegion:
            {
                auto& region = context.regions[(int32_t)data];
                context.GetRenderer().SetClipRect(region.origin, region.origin + region.size);
                break;
            }
            case LayoutOps::PopRegion:
            {
                context.GetRenderer().ResetClipRect();
                break;
            }
            default:
                break;
            }
        }
    }

    static void InitLocalRegionStack(WidgetContextData& context, LayoutBuilder& layout, RegionStackT& stack)
    {
        stack.clear(true);
    }

    static void RenderWidgets(WidgetContextData& context, LayoutBuilder& layout, WidgetDrawResult& result)
    {
        static RegionStackT RegionStack;
        InitLocalRegionStack(context, layout, RegionStack);
        auto io = Config.platform->CurrentIO();
        UpdateWidgetGeometryPass(context, layout, io, RegionStack);
        RenderWidgetPass(context, layout, result, io);
    }

    static ImVec2 UpdateRegionGeometry(WidgetContextData& context, LayoutBuilder& layout, bool isTopLevel)
    {
        ImVec2 shift{};

        if (isTopLevel)
        {
            if (layout.alignment & ToLeft)
            {
                auto width = layout.geometry.GetWidth();
                layout.geometry.Max.x = layout.available.Max.x;
                layout.geometry.Min.x = layout.geometry.Max.x - width;

                auto hdiff = layout.startpos.x - layout.geometry.Min.x;
                if (hdiff > 0.f)
                    layout.geometry.TranslateX(hdiff);
            }
            else
            {
                layout.geometry.Min.x += layout.startpos.x;
                layout.geometry.Max.x += layout.startpos.x;
            }

            if ((layout.fill & FD_Horizontal) && (layout.alignment & AlignHCenter))
            {
                auto hdiff = (layout.geometry.GetWidth() - layout.contentsz.x) * 0.5f;
                shift.x = hdiff;
            }

            if (layout.alignment & ToTop)
            {
                auto height = layout.geometry.GetHeight();
                layout.geometry.Max.y = layout.available.Max.y;
                layout.geometry.Min.y = layout.geometry.Max.y - height;

                auto vdiff = layout.startpos.y - layout.geometry.Min.y;
                if (vdiff > 0.f)
                    layout.geometry.TranslateY(vdiff);
            }
            else
            {
                layout.geometry.Min.y += layout.startpos.y;
                layout.geometry.Max.y += layout.startpos.y;
            }

            if ((layout.fill & FD_Vertical) && (layout.alignment & AlignVCenter))
            {
                auto vdiff = (layout.geometry.GetHeight() - layout.contentsz.y) * 0.5f;
                shift.y = vdiff;
            }
        }

        if (layout.regionIdx != -1)
        {
            auto& region = context.regions[layout.regionIdx];
            region.origin = layout.geometry.Min;
            region.size = layout.geometry.GetSize();
            context.AddItemGeometry(region.id, layout.geometry);
        }

        layout.startpos = layout.geometry.Min;
        return shift;
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
        while (depth > 0 && !context.layoutStack.empty())
        {
            auto& layout = context.layouts[context.layoutStack.top()];
            ComputeLayoutGeometry(context, layout);
            
            if (context.layoutStack.size() == 1)
            {
                // Propagate layout shifts to entire tree
                static Vector<std::pair<int32_t, ImVec2>, int16_t, 16> parents, currps;
                parents.clear(true); currps.clear(true);

                auto lidx = context.layoutStack.top();
                auto shift = UpdateRegionGeometry(context, layout, true);
                parents.emplace_back(lidx, shift);

                while (!parents.empty())
                {
                    currps.clear(true);

                    for (auto pidx = 0; pidx < parents.size(); ++pidx)
                    {
                        lidx = 0;
                        for (auto& sublayout : context.layouts)
                        {
                            if (sublayout.parentIdx == parents[pidx].first)
                            {
                                assert(sublayout.parentIdx != lidx);

                                const auto& parent = context.layouts[parents[pidx].first];
                                auto hasParentAligned = parent.parentIdx == -1 || parent.type != Layout::Grid;
                                auto offset = parent.geometry.Min + (hasParentAligned ? ImVec2{} : parents[pidx].second);
                                sublayout.geometry.Translate(offset);

                                auto chshift = UpdateRegionGeometry(context, sublayout, false);
                                currps.emplace_back(lidx, chshift);
                            }
                            ++lidx;
                        }
                    }

                    std::swap(parents, currps);
                }

                context.AddItemGeometry(layout.id, layout.geometry);
                RenderWidgets(context, layout, result);
                context.adhocLayout.top().lastItemId = layout.id;

                // If pending popup rendering exists, render it now as
                // layout resolution of widgets are done
                if (context.popupContext != nullptr &&
                    context.popupContext->popupTargetId != -1)
                {
                    EndPopUp(*context.popupContext, context.popupBgColor, context.popupFlags);
                    context.popupContext->popupTargetId = -1;
                    context.popupContext = nullptr;
                }
            }

            --depth;
            context.lastLayoutIdx = context.layoutStack.top();
            context.layoutStack.pop(1, false);
            context.nestedContextStack.pop(1, true);

            if (layout.type == Layout::Horizontal || layout.type == Layout::Vertical)
            {
#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_YOGA_ENGINE
                PopYogaLayoutNode();
#elif GLIMMER_FLEXBOX_ENGINE == GLIMMER_SIMPLE_FLEX_ENGINE
                PopFlexLayoutNode();
#endif
            }
        }

        if (context.layoutStack.empty())
        {
            context.ResetLayoutData();

#if GLIMMER_FLEXBOX_ENGINE == GLIMMER_YOGA_ENGINE
            ResetYogaLayoutSystem(YogaFreeNodeStartIndexes.at(&context));
#elif GLIMMER_FLEXBOX_ENGINE == GLIMMER_SIMPLE_FLEX_ENGINE
            
            for (auto& root : LayContexts)
                lay_reset_context(&root.ctx);

            NextFreeContextIdx = 0;
#endif

            GridLayoutItems.clear(true);
        }

        return result;
    }

    void CacheLayout()
    {
		WidgetContextData::CacheItemGeometry = true;
    }

    void InvalidateLayout()
    {
        WidgetContextData::CacheItemGeometry = false;
    }

    void ContextPushed(void* data)
    {
        YogaFreeNodeStartIndexes[(WidgetContextData*)data] = NextFreeNodeIdx;
    }

    void ContextPopped()
    {
        // Nothing required...
    }

#pragma endregion
}

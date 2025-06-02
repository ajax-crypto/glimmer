#include "context.h"
#include "im_font_manager.h"
#include "renderer.h"
#include <list>

#ifndef GLIMMER_MAX_OVERLAYS
#define GLIMMER_MAX_OVERLAYS 32
#endif

#ifndef GLIMMER_GLOBAL_ANIMATION_FRAMETIME
#define GLIMMER_GLOBAL_ANIMATION_FRAMETIME 18
#endif

namespace glimmer
{
    static std::list<WidgetContextData> WidgetContexts;
    static WidgetContextData* CurrentContext = nullptr;
    static bool StartedRendering = false;

    void AnimationData::moveByPixel(float amount, float max, float reset)
    {
        timestamp += Config.platform->desc.deltaTime;

        if (timestamp >= GLIMMER_GLOBAL_ANIMATION_FRAMETIME)
        {
            offset += amount;
            if (offset >= max) offset = reset;
            timestamp = 0.f;
        }
    }

    void LayoutDescriptor::reset()
    {
        type = Layout::Invalid;
        id = 0;
        fill = FD_None;
        alignment = TextAlignLeading;
        from = -1, to = -1, itemidx = -1;
        currow = -1, currcol = -1;
        currStyleStates = 0;
        geometry = ImRect{ { FIT_SZ, FIT_SZ },{ FIT_SZ, FIT_SZ } };
        nextpos = ImVec2{ 0.f, 0.f };
        prevpos = ImVec2{ 0.f, 0.f };
        spacing = ImVec2{ 0.f, 0.f };
        maxdim = ImVec2{ 0.f, 0.f };
        cumulative = ImVec2{ 0.f, 0.f };
        hofmode = OverflowMode::Scroll;
        vofmode = OverflowMode::Scroll;
        scroll = ScrollableRegion{};
        popSizingOnEnd = false;

        for (auto idx = 0; idx < WSI_Total; ++idx)
        {
            styleStartIdx[idx] = -1;
            styles[idx].clear(false);
        }

        itemIndexes.clear(true);
        tabbars.clear(false);
        containerStack.clear(true);
        rows.clear(true);
        cols.clear(true);
    }

    void TabBarDescriptor::reset()
    {
        id = -1;
        geometry = 0;
        sizing = TabBarItemSizing::ShrinkToFit;
        neighbors = NeighborWidgets{};
        items.clear(true);
        newTabButton = false;
    }

    void CurrentItemGridState::reset()
    {
        id = -1;
        origin = size = nextpos = maxHeaderExtent = maxCellExtent = totalsz = ImVec2{};
        geometry = 0;
        levels = currlevel = depth = 0;
        selectedCol = -1;
        movingCols = std::make_pair<int16_t, int16_t>(-1, -1);
        phase = ItemGridConstructPhase::None;
        headers[0].clear(true); headers[1].clear(true); headers[2].clear(true); headers[3].clear(true);
        headers[4].clear(true);
        headerHeights[0] = headerHeights[1] = headerHeights[2] = headerHeights[3] = headerHeight = 0.f;
        currRow = currCol = 0;
        event = WidgetDrawResult{};
        inRow = contentInteracted = addedBounds = false;
    }

    void RecordWidget(ItemGridUIOperation& el, int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        el.type = UIOperationType::Widget;
        el.data.widget.id = id; el.data.widget.geometry = geometry; el.data.widget.neighbors = neighbors;
    }

    void RecordWidget(ItemGridUIOperation& el, const LayoutItemDescriptor& item, int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        el.type = UIOperationType::Widget;
        el.data.widget.id = id; el.data.widget.geometry = geometry; el.data.widget.neighbors = neighbors;
        el.data.widget.bounds = item.margin; el.data.widget.text = item.text;
    }

    void RecordAdhocMovement(ItemGridUIOperation& el, int32_t direction)
    {
        el.type = UIOperationType::Movement;
        el.data.movement.direction = direction;
    }

    void RecordAdhocMovement(ItemGridUIOperation& el, int32_t id, int32_t direction)
    {
        el.type = UIOperationType::Movement;
        el.data.movement.hid = id;
        el.data.movement.direction = direction;
    }

    void RecordAdhocMovement(ItemGridUIOperation& el, int32_t hid, int32_t vid, bool toRight, bool toBottom)
    {
        el.type = UIOperationType::Movement;
        el.data.movement.direction = toRight ? ToRight : ToLeft;
        el.data.movement.direction |= toBottom ? ToBottom : ToTop;
        el.data.movement.hid = hid; el.data.movement.vid = vid;
    }

    void RecordAdhocMovement(ItemGridUIOperation& el, ImVec2 amount, int32_t direction)
    {
        el.type = UIOperationType::Movement;
        el.data.movement.direction = direction;
        el.data.movement.amount = amount;
    }

    void RecordBeginLayout(ItemGridUIOperation& el, Layout layout, int32_t fill, int32_t alignment, bool wrap, ImVec2 spacing, const NeighborWidgets& neighbors)
    {
        el.type = UIOperationType::LayoutBegin;
        el.data.layoutBegin.layout = layout;
        el.data.layoutBegin.fill = fill;
        el.data.layoutBegin.alignment = alignment;
        el.data.layoutBegin.wrap = wrap;
        el.data.layoutBegin.spacing = spacing;
        el.data.layoutBegin.neighbors = neighbors;
    }

    void RecordEndLayout(ItemGridUIOperation& el, int depth)
    {
        el.type = UIOperationType::LayoutEnd;
        el.data.layoutEnd.depth = depth;
    }

    void RecordPopupBegin(ItemGridUIOperation& el, int32_t id, ImVec2 origin)
    {
        el.type = UIOperationType::PopupBegin;
        el.data.popupBegin.id = id;
        el.data.popupBegin.origin = origin;
    }

    void RecordPopupEnd(ItemGridUIOperation& el)
    {
        el.type = UIOperationType::PopupEnd;
    }

    ImVec2 WidgetContextData::NextAdHocPos() const
    {
        const auto& layout = adhocLayout.top();
        auto offset = ImVec2{};

        if (!containerStack.empty())
        {
            auto id = containerStack.top();
            auto index = id & 0xffff;
            auto type = id >> 16;
            offset = states[type][index].state.scroll.state.pos;
        }

        return layout.nextpos + offset;
    }

    void AddFontPtr(FontStyle& font)
    {
        if (font.font == nullptr && StartedRendering)
        {
            auto isbold = font.flags & FontStyleBold;
            auto isitalics = font.flags & FontStyleItalics;
            auto islight = font.flags & FontStyleLight;
            auto ft = isbold && isitalics ? FT_BoldItalics : isbold ? FT_Bold : isitalics ? FT_Italics : islight ? FT_Light : FT_Normal;
            font.font = GetFont(font.family, font.size, ft);
        }
    }

    void ResetActivePopUps(ImVec2 mousepos, bool escape)
    {
        for (auto it = WidgetContexts.rbegin(); it != WidgetContexts.rend(); ++it)
        {
            if (it->parentContext && it->parentContext->activePopUpRegion != ImRect{} &&
                (!it->parentContext->activePopUpRegion.Contains(mousepos) || escape))
            {
                it->parentContext->activePopUpRegion = ImRect{};
            }
        }
    }

    void InitFrameData()
    {
        StartedRendering = true;

        for (auto it = WidgetContexts.rbegin(); it != WidgetContexts.rend(); ++it)
        {
            auto& context = *it;
            context.InsideFrame = true;
            context.adhocLayout.push();
        }

        for (auto idx = 0; idx < WSI_Total; ++idx)
            AddFontPtr(WidgetContextData::StyleStack[idx].top().font);
    }

    void ResetFrameData()
    {
        for (auto& context : WidgetContexts)
        {
            context.InsideFrame = false;
            context.adhocLayout.clear(true);

            for (auto idx = 0; idx < WT_TotalTypes; ++idx)
            {
                context.itemGeometries[idx].reset(ImRect{ {0.f, 0.f}, {0.f, 0.f} });
            }
            
            context.ResetLayoutData();
            context.maxids[WT_SplitterScrollRegion] = 0;
            context.maxids[WT_Layout] = 0;
            context.activePopUpRegion = ImRect{};

            assert(context.layouts.empty());
        }

        CurrentContext = &(*(WidgetContexts.begin()));

        for (auto idx = 0; idx < WSI_Total; ++idx)
        {
            auto popsz = WidgetContextData::StyleStack[idx].size() - 1;
            if (popsz > 0) WidgetContextData::StyleStack[idx].pop(popsz, true);
            WidgetContextData::currStyle[idx] = StyleDescriptor{};
        }

        WidgetContextData::currStyleStates = 0;
    }

    StyleDescriptor& WidgetContextData::GetStyle(int32_t state)
    {
        auto style = log2((unsigned)state);
        auto& res = (state & currStyleStates) ? currStyle[style] : StyleStack[style].top();
        AddFontPtr(res.font);
        return res;
    }

    void WidgetContextData::ToggleDeferedRendering(bool defer, bool reset)
    {
        usingDeferred = defer;

        if (defer)
        {
            auto renderer = CreateDeferredRenderer(&ImGuiMeasureText);
            if (reset) { renderer->Reset(); adhocLayout.push(); }
            deferedRenderer = renderer;
        }
        else if (reset)
        {
            if (adhocLayout.size() > 1) adhocLayout.pop(1, true);
            deferedRenderer->Reset();
        }
    }

    void WidgetContextData::PushContainer(int32_t parentId, int32_t id)
    {
        auto index = id & 0xffff;
        auto wtype = (WidgetType)(id >> 16);

        if (wtype == WT_SplitterScrollRegion)
            splitterScrollPaneParentIds[index] = parentId;

        containerStack.push() = id;
    }

    void WidgetContextData::PopContainer(int32_t id)
    {
        containerStack.pop(1, true);

        auto index = id & 0xffff;
        auto wtype = (WidgetType)(id >> 16);

        if (wtype == WT_SplitterScrollRegion)
        {
            splitterScrollPaneParentIds[index] = -1;
        }
    }
    
    void WidgetContextData::AddItemGeometry(int id, const ImRect& geometry, bool ignoreParent)
    {
        auto index = id & 0xffff;
        auto wtype = (WidgetType)(id >> 16);

        if (index >= (int)itemGeometries[wtype].size())
            itemGeometries[wtype].resize(index + 128);

        itemGeometries[wtype][index] = geometry;
        adhocLayout.top().lastItemId = id;

        if (!containerStack.empty() && !ignoreParent)
        {
            id = containerStack.top();
            wtype = (WidgetType)(id >> 16);

            if (wtype == WT_SplitterScrollRegion)
            {
                const auto& splitter = splitterStack.top();
                auto& state = SplitterState(splitter.id);
                state.scrolldata[state.current].max = ImMax(state.scrolldata[state.current].max, geometry.Max);
            }
        }

        /*if (currSpanDepth > 0 && spans[currSpanDepth].popWhenUsed)
        {
            spans[currSpanDepth] = ElementSpan{};
            --currSpanDepth;
        }*/
    }

    void HandleLabelEvent(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io, WidgetDrawResult& result);
    void HandleButtonEvent(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io, WidgetDrawResult& result);
    void HandleRadioButtonEvent(int32_t id, const ImRect& extent, float maxrad, IRenderer& renderer, const IODescriptor& io,
        WidgetDrawResult& result);
    void HandleToggleButtonEvent(int32_t id, const ImRect& extent, ImVec2 center, IRenderer& renderer, const IODescriptor& io,
        WidgetDrawResult& result);
    void HandleCheckboxEvent(int32_t id, const ImRect& extent, const IODescriptor& io,
        WidgetDrawResult& result);
    void HandleSliderEvent(int32_t id, const ImRect& extent, const ImRect& thumb, const IODescriptor& io, 
        WidgetDrawResult& result);
    void HandleTextInputEvent(int32_t id, const ImRect& content, const IODescriptor& io,
        IRenderer& renderer, WidgetDrawResult& result);
    void HandleDropDownEvent(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const IODescriptor& io, IRenderer& renderer, WidgetDrawResult& result);
    void HandleItemGridEvent(int32_t id, const ImRect& padding, const ImRect& content,
        const IODescriptor& io, IRenderer& renderer, WidgetDrawResult& result);
    void HandleSpinnerEvent(int32_t id, const ImRect& extent, const ImRect& incbtn, const ImRect& decbtn, const IODescriptor& io,
        WidgetDrawResult& result);
    void HandleTabBarEvent(int32_t id, const ImRect& content, const IODescriptor& io, IRenderer& renderer, WidgetDrawResult& result);

    WidgetDrawResult WidgetContextData::HandleEvents(ImVec2 origin, int from, int to)
    {
        auto io = Config.platform->CurrentIO();
        auto& renderer = usingDeferred ? *deferedRenderer : *Config.renderer;
        WidgetDrawResult result;
        to = to == -1 ? deferedEvents.size() : to;

        for (auto idx = from; idx < to; ++idx)
        {
            auto ev = deferedEvents[idx];

            switch (ev.type)
            {
            case WT_Label: 
                ev.margin.Translate(origin);
                ev.border.Translate(origin); 
                ev.padding.Translate(origin); 
                ev.content.Translate(origin); 
                ev.text.Translate(origin);
                HandleLabelEvent(ev.id, ev.margin, ev.border, ev.padding, ev.content, ev.text, renderer, io, result);
                break;
            case WT_Button:
                ev.margin.Translate(origin);
                ev.border.Translate(origin);
                ev.padding.Translate(origin);
                ev.content.Translate(origin);
                ev.text.Translate(origin);
                HandleButtonEvent(ev.id, ev.margin, ev.border, ev.padding, ev.content, ev.text, renderer, io, result);
                break;
            case WT_Checkbox:
                ev.extent.Translate(origin);
                HandleCheckboxEvent(ev.id, ev.extent, io, result);
                break;
            case WT_RadioButton:
                ev.extent.Translate(origin);
                HandleRadioButtonEvent(ev.id, ev.extent, ev.maxrad, renderer, io, result);
                break;
            case WT_ToggleButton:
                ev.extent.Translate(origin);
                ev.center += origin;
                HandleToggleButtonEvent(ev.id, ev.extent, ev.center, renderer, io, result);
                break;
            case WT_Spinner:
                ev.extent.Translate(origin);
                ev.center += origin;
                HandleSpinnerEvent(ev.id, ev.extent, ev.incbtn, ev.decbtn, io, result);
                break;
            case WT_Slider:
                ev.content.Translate(origin);
                ev.padding.Translate(origin);
                HandleSliderEvent(ev.id, ev.padding, ev.content, io, result);
                break;
            case WT_TextInput:
                ev.content.Translate(origin);
                HandleTextInputEvent(ev.id, ev.content, io, renderer, result);
                break;
            case WT_DropDown:
                ev.margin.Translate(origin);
                ev.border.Translate(origin);
                ev.padding.Translate(origin);
                ev.content.Translate(origin);
                HandleDropDownEvent(ev.id, ev.margin, ev.border, ev.padding, ev.content, io, renderer, result);
                break;
            case WT_ItemGrid:
                ev.padding.Translate(origin);
                ev.content.Translate(origin);
                HandleItemGridEvent(ev.id, ev.padding, ev.content, io, renderer, result);
                break;
            case WT_TabBar:
                ev.extent.Translate(origin);
                HandleTabBarEvent(ev.id, ev.extent, io, renderer, result);
                break;
            default:
                break;
            }
        }

        if (to == -1) deferedEvents.clear(true);
        return result;
    }

    void WidgetContextData::ResetLayoutData()
    {
        for (auto lidx = 0; lidx < layouts.size(); ++lidx)
            layouts[lidx].reset();

        layouts.clear(false);
        layoutItems.clear(true);
    }

    void WidgetContextData::ResetCurrentStyle()
    {
        currStyleStates = 0;
        for (auto idx = 0; idx < WSI_Total; ++idx) currStyle[idx] = StyleDescriptor{};
    }

    const ImRect& WidgetContextData::GetGeometry(int32_t id) const
    {
        auto index = id & 0xffff;
        auto wtype = (WidgetType)(id >> 16);
        return itemGeometries[wtype][index];
    }

    float WidgetContextData::MaximumExtent(Direction dir) const
    {
        if (!containerStack.empty())
        {
            auto id = containerStack.top();
            auto index = id & 0xffff;
            auto wtype = (WidgetType)(id >> 16);

            if (wtype != WT_ItemGrid)
                return dir == DIR_Horizontal ? itemGeometries[wtype][index].GetWidth() :
                    itemGeometries[wtype][index].GetHeight();
            else
            {
                auto& grid = CurrentItemGridContext->itemGrids.top();
                auto extent = grid.headers[grid.currlevel][grid.currCol].extent;
                return dir == DIR_Horizontal ? extent.GetWidth() : extent.GetHeight();
            }
        }
        else
        {
            auto rect = ImGui::GetCurrentWindow()->InnerClipRect;
            return dir == DIR_Horizontal ? rect.GetWidth() : rect.GetHeight();
        }
    }

    ImVec2 WidgetContextData::MaximumExtent() const
    {
        if (!containerStack.empty())
        {
            auto id = containerStack.top();
            auto index = id & 0xffff;
            auto wtype = (WidgetType)(id >> 16);

            if (wtype != WT_ItemGrid)
                return itemGeometries[wtype][index].GetSize();
            else
            {
                auto& grid = CurrentItemGridContext->itemGrids.top();
                auto extent = grid.headers[grid.currlevel][grid.currCol].extent;
                return extent.GetSize();
            }
        }
        else
        {
            auto rect = ImGui::GetCurrentWindow()->InnerClipRect;
            return ImVec2{ rect.GetWidth(), rect.GetHeight() };
        }
    }

    ImVec2 WidgetContextData::MaximumAbsExtent() const
    {
        if (!containerStack.empty())
        {
            auto id = containerStack.top();
            auto index = id & 0xffff;
            auto wtype = (WidgetType)(id >> 16);

            if (wtype != WT_ItemGrid)
                return itemGeometries[wtype][index].Max;
            else
            {
                auto& grid = CurrentItemGridContext->itemGrids.top();
                const auto& config = CurrentItemGridContext->GetState(id).state.grid;
                auto max = grid.headers[grid.currlevel][grid.currCol].extent.Max - config.cellpadding;
                return max;
            }
        }
        else
        {
            auto rect = ImGui::GetCurrentWindow()->InnerClipRect;
            return rect.Max;
        }
    }

    ImVec2 WidgetContextData::WindowSize() const
    {
        auto rect = ImGui::GetCurrentWindow()->InnerClipRect;
        return ImVec2{ rect.GetWidth(), rect.GetHeight() };
    }
    
    WidgetContextData::WidgetContextData()
    {
        memset(maxids, 0, WT_TotalTypes);
        memset(tempids, INT_MAX, WT_TotalTypes);

        gridStates.resize(Config.GetTotalWidgetCount ? Config.GetTotalWidgetCount(WT_ItemGrid) : 4);
        tabStates.resize(Config.GetTotalWidgetCount ? Config.GetTotalWidgetCount(WT_TabBar) : 8);
        toggleStates.resize(Config.GetTotalWidgetCount ? Config.GetTotalWidgetCount(WT_ToggleButton) : 32);
        radioStates.resize(Config.GetTotalWidgetCount ? Config.GetTotalWidgetCount(WT_RadioButton) : 32);
        checkboxStates.resize(Config.GetTotalWidgetCount ? Config.GetTotalWidgetCount(WT_Checkbox) : 32);
        inputTextStates.resize(Config.GetTotalWidgetCount ? Config.GetTotalWidgetCount(WT_TextInput) : 32);
        splitterStates.resize(Config.GetTotalWidgetCount ? Config.GetTotalWidgetCount(WT_Splitter) : 4);
        spinnerStates.resize(Config.GetTotalWidgetCount ? Config.GetTotalWidgetCount(WT_Spinner) : 8);
        tabBarStates.resize(Config.GetTotalWidgetCount ? Config.GetTotalWidgetCount(WT_TabBar) : 4);
        splitterScrollPaneParentIds.resize((Config.GetTotalWidgetCount ? Config.GetTotalWidgetCount(WT_SplitterScrollRegion) : 32) 
            * GLIMMER_MAX_SPLITTER_REGIONS, -1);
        dropDownOptions.resize(Config.GetTotalWidgetCount ? Config.GetTotalWidgetCount(WT_DropDown) : 16);

        for (auto idx = 0; idx < WT_TotalTypes; ++idx)
        {
            maxids[idx] = 0;
            states[idx].resize(Config.GetTotalWidgetCount ? Config.GetTotalWidgetCount((WidgetType)idx) : 32, 
                WidgetStateData{ (WidgetType)idx });
        }
    }
    
    SplitterInternalState::SplitterInternalState()
    {
        for (auto idx = 0; idx < GLIMMER_MAX_SPLITTER_REGIONS; ++idx)
        {
            states[idx] = WS_Default;
            scrollids[idx] = -1;
            isdragged[idx] = false;
            dragstart[idx] = 0.f;
        }
    }

    WidgetContextData& GetContext()
    {
        return *CurrentContext;
    }

    static void InitContextStyles()
    {
        for (auto idx = 0; idx < WSI_Total; ++idx)
        {
            auto& style = WidgetContextData::StyleStack[idx].push();
            style.font.size *= Config.fontScaling;

            WidgetContextData::radioButtonStyles[idx].push();
            auto& slider = WidgetContextData::sliderStyles[idx].push();
            auto& spinner = WidgetContextData::spinnerStyles[idx].push();
            auto& toggle = WidgetContextData::toggleButtonStyles[idx].push();
            auto& tab = WidgetContextData::tabBarStyles[idx].push();
            toggle.fontsz *= Config.fontScaling;

            switch (idx)
            {
            case WSI_Hovered:
                slider.thumbColor = ToRGBA(255, 255, 255);
                spinner.downbtnColor = spinner.upbtnColor = ToRGBA(240, 240, 240);
                tab.closebgcolor  = tab.pinbgcolor = ToRGBA(150, 150, 150);
                tab.pincolor = ToRGBA(0, 0, 0);
                tab.closecolor = ToRGBA(255, 0, 0);
                [[fallthrough]];
            case WSI_Checked:
                toggle.trackColor = ToRGBA(200, 200, 200);
                toggle.indicatorTextColor = ToRGBA(100, 100, 100);
                break;
            default:
                toggle.trackColor = ToRGBA(152, 251, 152);
                toggle.indicatorTextColor = ToRGBA(0, 100, 0);
                slider.thumbColor = ToRGBA(240, 240, 240);
                spinner.downbtnColor = spinner.upbtnColor = ToRGBA(200, 200, 200);
                tab.closebgcolor = tab.pinbgcolor = ToRGBA(0, 0, 0, 0);
                tab.pincolor = ToRGBA(0, 0, 0);
                tab.closecolor = ToRGBA(255, 0, 0);
                break;
            }
        }
    }

    WidgetContextData& PushContext(int32_t id)
    {
        if (id < 0)
        {
            if (CurrentContext == nullptr)
            {
                constexpr auto totalStyles = 16 * WSI_Total;
                CurrentContext = &(WidgetContexts.emplace_back());
                InitContextStyles();
            }
        }
        else
        {
            auto wtype = (id >> 16);
            auto index = id & 0xffff;

            if ((int)CurrentContext->nestedContexts[wtype].size() <= index)
            {
                auto& ctx = WidgetContexts.emplace_back();
                ctx.parentContext = CurrentContext;
                ctx.InsideFrame = CurrentContext->InsideFrame;

                auto& children = CurrentContext->nestedContexts[wtype];
                if (children.empty()) children.resize(32, nullptr);
                children[index] = &ctx;

                auto& layout = ctx.adhocLayout.push();
                layout.nextpos = CurrentContext->adhocLayout.top().nextpos;
            }

            CurrentContext = CurrentContext->nestedContexts[wtype][index];
            
            for (auto idx = 0; idx < WT_TotalTypes; ++idx) CurrentContext->maxids[idx] = 0;
        }

        return *CurrentContext;
    }

    void PopContext()
    {
        CurrentContext = GetContext().parentContext;
        assert(CurrentContext != nullptr);
    }

    StyleStackT WidgetContextData::StyleStack[WSI_Total];
    StyleDescriptor WidgetContextData::currStyle[WSI_Total];
    int32_t WidgetContextData::currStyleStates = 0;
    WidgetContextData* WidgetContextData::CurrentItemGridContext = nullptr;
    DynamicStack<ToggleButtonStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES> WidgetContextData::toggleButtonStyles[WSI_Total];
    DynamicStack<RadioButtonStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES>  WidgetContextData::radioButtonStyles[WSI_Total];
    DynamicStack<SliderStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES> WidgetContextData::sliderStyles[WSI_Total];
    DynamicStack<SpinnerStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES> WidgetContextData::spinnerStyles[WSI_Total];
    DynamicStack<TabBarStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES> WidgetContextData::tabBarStyles[WSI_Total];
}
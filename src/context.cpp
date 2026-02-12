#include "context.h"
#include "im_font_manager.h"
#include "renderer.h"
#include "style.h"
#include "widgets.h"
#include "libs/inc/implot/implot.h"
#include <list>

#include "draw.h"
#include "layout.h"

namespace glimmer
{
    static std::list<WidgetContextData> WidgetContexts;
    static WidgetContextData* CurrentContext = nullptr;
    static ImPlotContext* ChartsContext = nullptr;
    static bool StartedRendering = false;
    static bool HasImPlotContext = false;
    static bool RemovePopupAtFrameExit = false;

    WidgetDrawResult HandleDeferredEvent(WidgetContextData& context, EventDeferInfo& ev, ImVec2 origin, IRenderer& renderer);
    void InitContextStyles();

    LayoutBuilder::LayoutBuilder()
    {}

    void LayoutBuilder::reset()
    {
        type = Layout::Invalid;
        id = specified = 0;
        fill = FD_None;
        alignment = TextAlignLeading;
        from = -1, to = -1, itemidx = -1;
        currow = -1, currcol = -1;
        geometry = ImRect{ { FIT_SZ, FIT_SZ },{ FIT_SZ, FIT_SZ } };
        available = ImRect{ };
        nextpos = ImVec2{ 0.f, 0.f };
        prevpos = ImVec2{ 0.f, 0.f };
        spacing = ImVec2{ 0.f, 0.f };
        maxdim = ImVec2{ 0.f, 0.f };
        startpos = ImVec2{ 0.f, 0.f };
        cumulative = ImVec2{ 0.f, 0.f };
        size = ImVec2{ 0.f, 0.f };
        hofmode = OverflowMode::Scroll;
        vofmode = OverflowMode::Scroll;
        scroll = ScrollableRegion{};
        tabbar.reset();
        popSizingOnEnd = false;
        currspan.first = currspan.second = 1;
        itemIndexes.clear(true);
        griditems.clear(true);
        containerStack.clear(true);
        rows.clear(true);
        cols.clear(true);
    }

    void TabBarBuilder::reset()
    {
        id = -1;
        geometry = 0;
        sizing = TabBarItemSizing::ResizeToFit;
        neighbors = NeighborWidgets{};
        items.clear(true); endItems.clear(true);
        newTabButton = false;
    }

    void NavDrawerBuilder::reset()
    {
        geometry = 0;
        neighbors = NeighborWidgets{};
        items.clear(true);
    }

    void ItemGridBuilder::reset()
    {
        id = -1;
        origin = size = nextpos = maxHeaderExtent = maxCellExtent = totalsz = ImVec2{};
        geometry = 0;
        levels = currlevel = depth = 0;
        selectedCol = -1;
        movingCols = std::make_pair<int16_t, int16_t>(-1, -1);
        phase = ItemGridConstructPhase::None;
        perDepthRowCount.clear(true);
        cellvals.clear(true);
        rowYs.clear(true);
        clickedItem.row = clickedItem.col = clickedItem.depth = -1;
        resizecol = parentId = -1;
		rowcount = 0;

        for (auto idx = 0; idx < 5; ++idx)
        {
            for (auto hidx = 0; hidx < headers[idx].size(); ++hidx)
            {
                headers[idx][hidx].content = headers[idx][hidx].extent = ImRect{};
                headers[idx][hidx].range = RendererEventIndexRange{};
                headers[idx][hidx].alignment = TextAlignCenter;
                headers[idx][hidx].offset = ImVec2{};
                headers[idx][hidx].parent = -1;
                headers[idx][hidx].props = 0;
            }

            headers[idx].clear(false);
        }

        headerHeights[0] = headerHeights[1] = headerHeights[2] = headerHeights[3] = headerHeight = 0.f;
        currRow = currCol = 0;
        event = WidgetDrawResult{};
        contentInteracted = addedBounds = false;
    }

    void AccordionBuilder::reset()
    {
        id = geometry = 0;
        origin = size = totalsz = textsz = extent = ImVec2{};
        content = ImRect{};
        headerHeight = 0.f;
        totalRegions = 0;
        text = icon[0] = icon[1] = "";
        textType = TextType::PlainText;
        event = WidgetDrawResult{};
        regions.clear(true);
        border = FourSidedBorder{};
        bgcolor = 0;
        resflags[0] = resflags[1] = RT_INVALID;
        hscroll = vscroll = false;
    }

    ImVec2 WidgetContextData::NextAdHocPos() const
    {
        const auto& layout = adhocLayout.top();
        auto offset = ImVec2{};

        if (!containerStack.empty())
        {
            auto id = containerStack.top();
            auto index = id & WidgetIndexMask;
            auto type = (WidgetType)(id >> WidgetTypeBits);

            if (type == WT_Accordion)
            {
                auto& accordion = accordions.top();
                const auto& state = AccordionState(accordion.id);
                offset = state.scrolls[accordion.totalRegions].barstate.pos;
                return state.scrolls[accordion.totalRegions].viewport.Min + offset;
            }
            else if (type == WT_Scrollable && !layout.addedOffset)
            {
                offset = configs[type][index].config.scroll.barstate.pos;
            }
        }

        return layout.nextpos - offset;
    }

    std::optional<NestedContextSource> WidgetContextData::IsInside(NestedContextSourceType source) const
    {
		for (auto idx = nestedContextStack.size() - 1; idx >= 0; --idx)
        {
            const auto& context = nestedContextStack[idx];
            if (context.source == source)
                return context;
        }

        return std::nullopt;
    }

    std::optional<NestedContextSource> WidgetContextData::GetParentWidget() const
    {
        for (auto idx = nestedContextStack.size() - 1; idx >= 0; --idx)
        {
            const auto& context = nestedContextStack[idx];
            if (context.source == NestedContextSourceType::DropDownPopup ||
                context.source == NestedContextSourceType::ItemGrid ||
                context.source == NestedContextSourceType::Region)
                return context;
        }

        return std::nullopt;
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

    void InitFrameData()
    {
        StartedRendering = true;
        RemovePopupAtFrameExit = false;

        if (!HasImPlotContext)
        {
            HasImPlotContext = true;
            ChartsContext = ImPlot::CreateContext();
            auto& style = ImPlot::GetStyle();
            style.PlotPadding = { 0.f, 0.f };
        }

        for (auto it = WidgetContexts.rbegin(); it != WidgetContexts.rend(); ++it)
        {
            auto& context = *it;
            context.InsideFrame = true;
            context.adhocLayout.push();
        }

        for (auto idx = 0; idx < WSI_Total; ++idx)
            AddFontPtr(StyleEngine.stack.styles[idx].top().font);

        if (Config.platform->desc.isRightClicked())
            WidgetContextData::RightClickContext.pos = Config.platform->desc.mousepos;

        if (Config.logger) Config.logger->EnterFrame({});
    }

    void ResetFrameData()
    {
        for (auto& context : WidgetContexts)
        {
            context.InsideFrame = false;
            context.adhocLayout.clear(true);

            if (!WidgetContextData::CacheItemGeometry)
            {
                for (auto idx = 0; idx < WT_TotalTypes; ++idx)
                {
                    context.itemGeometries[idx].reset(ImRect{ {0.f, 0.f}, {0.f, 0.f} });
                }

                context.regions.clear(true);
            }
            
            context.ResetLayoutData();
            context.maxids[WT_SplitterRegion] = 0;
            context.maxids[WT_Layout] = 0;
            context.maxids[WT_Charts] = 0;
            context.lastLayoutIdx = -1;

            for (auto& layout : context.layouts)
                layout.reset();
            context.layouts.clear(false);

            assert(context.layoutStack.empty());
        }

        CurrentContext = &(*(WidgetContexts.begin()));
        auto rtpos = WidgetContextData::RightClickContext.pos;
        WidgetContextData::RightClickContext = UIElementDescriptor{};
        WidgetContextData::RightClickContext.pos = rtpos;
        WidgetContextData::ContextMenuOptionParams.clear(true);

        for (auto idx = 0; idx < WSI_Total; ++idx)
        {
            auto popsz = StyleEngine.stack.styles[idx].size() - 1;
            if (popsz > 0) StyleEngine.stack.styles[idx].pop(popsz, true);
        }

        if (RemovePopupAtFrameExit)
        {
            WidgetContextData::ActivePopUpRegion = ImRect{};
            WidgetContextData::RightClickContext.pos = ImVec2{};
            WidgetContextData::PopupContext = nullptr;
        }

        if (Config.logger) Config.logger->ExitFrame();
    }

    void WidgetContextData::RecordForReplay(int64_t data, LayoutOps ops)
    {
        replayContent.emplace_back(data, ops);
    }

    int32_t WidgetContextData::GetNextCount(WidgetType type)
    {
        auto count = maxids[type];

        if (count == states[type].size())
        {
            auto sz = Config.GetTotalWidgetCount ? Config.GetTotalWidgetCount(type) :
                WidgetContextData::GetExpectedWidgetCount(type);
            states[type].reserve(states[type].size() + sz);
            configs[type].reserve(states[type].size() + sz);

            //auto oldsz = WidgetStyles[type].size();
            //WidgetStyles[type].expand_and_create(sz, false);

            for (auto idx = 0; idx < sz; ++idx)
            {
                configs[type].emplace_back(WidgetConfigData{ type });
                states[type].emplace_back();
                /*auto& styles = WidgetStyles[type][idx + oldsz];

                for (auto ws = 0; ws < WSI_Total; ++ws)
                    styles[ws] = StyleDescriptor{};*/
            }

            switch (type)
            {
            case WT_Checkbox: {
                checkboxStates.reserve(checkboxStates.size() + sz);
                for (auto idx = 0; idx < sz; ++idx)
                    checkboxStates.emplace_back();
                break;
            }
            case WT_RadioButton: {
                radioStates.reserve(radioStates.size() + sz);
                for (auto idx = 0; idx < sz; ++idx)
                    radioStates.emplace_back();
                break;
            }
            case WT_TextInput: {
                inputTextStates.reserve(inputTextStates.size() + sz);
                for (auto idx = 0; idx < sz; ++idx)
                    inputTextStates.emplace_back();
                break;
            }
            case WT_ToggleButton: {
                toggleStates.reserve(toggleStates.size() + sz);
                for (auto idx = 0; idx < sz; ++idx)
                    toggleStates.emplace_back();
                break;
            }
            case WT_Spinner: {
                spinnerStates.reserve(spinnerStates.size() + sz);
                for (auto idx = 0; idx < sz; ++idx)
                    spinnerStates.emplace_back();
                break;
            }
            case WT_Accordion: {
                accordionStates.reserve(accordionStates.size() + sz);
                for (auto idx = 0; idx < sz; ++idx)
                    accordionStates.emplace_back();
                break;
            }
            case WT_NavDrawer: {
                navDrawerStates.reserve(navDrawerStates.size() + sz);
                for (auto idx = 0; idx < sz; ++idx)
                    navDrawerStates.emplace_back();
                break;
            }
            case WT_TabBar: {
                tabBarStates.reserve(tabBarStates.size() + sz);
                for (auto idx = 0; idx < sz; ++idx)
                    tabBarStates.emplace_back();
                break;
            }
            case WT_ItemGrid: {
                gridStates.reserve(gridStates.size() + sz);
                for (auto idx = 0; idx < sz; ++idx)
                    gridStates.emplace_back();
                break;
            }
            case WT_Splitter: {
                splitterStates.reserve(splitterStates.size() + sz);
                for (auto idx = 0; idx < sz; ++idx)
                    splitterStates.emplace_back();
                break;
            }
            default: break;
            }
        }

        maxids[type]++;
        return count;
    }

    int32_t WidgetContextData::GetState(int32_t id) const
    {
        auto index = id & WidgetIndexMask;
        auto wtype = (WidgetType)(id >> WidgetTypeBits);
        return states[wtype][index].state;
    }

    WidgetStateData& WidgetContextData::GetStateData(int32_t id)
    {
        auto index = id & WidgetIndexMask;
        auto wtype = (WidgetType)(id >> WidgetTypeBits);
        return states[wtype][index];
    }

    void WidgetContextData::SetTooltip(int32_t id, std::string_view tooltip, TextType type)
    {
        auto index = id & WidgetIndexMask;
        auto wtype = (WidgetType)(id >> WidgetTypeBits);
        
        switch (wtype)
        {
        case WT_Region: {
            auto& config = configs[wtype][index].config.region;
            config.tooltip = tooltip; config.tooltipType = type;
            break;
        }
        case WT_Label: {
            auto& config = configs[wtype][index].config.label;
            config.tooltip = tooltip; config.tooltipType = type;
            break;
        }
        case WT_Button: {
            auto& config = configs[wtype][index].config.button;
            config.tooltip = tooltip; config.tooltipType = type;
            break;
        }
        case WT_ToggleButton: {
            auto& config = configs[wtype][index].config.toggle;
            config.tooltip = tooltip; config.tooltipType = type;
            break;
        }
        case WT_RadioButton: {
            auto& config = configs[wtype][index].config.radio;
            config.tooltip = tooltip; config.tooltipType = type;
            break;
        }
        case WT_Checkbox: {
            auto& config = configs[wtype][index].config.checkbox;
            config.tooltip = tooltip; config.tooltipType = type;
            break;
        }
        case WT_Slider: {
            auto& config = configs[wtype][index].config.slider;
            config.tooltip = tooltip; config.tooltipType = type;
            break;
        }
        case WT_RangeSlider: {
            auto& config = configs[wtype][index].config.rangeSlider;
            config.tooltip = tooltip; config.tooltipType = type;
            break;
        }
        case WT_Spinner: {
            auto& config = configs[wtype][index].config.spinner;
            config.tooltip = tooltip; config.tooltipType = type;
            break;
        }
        case WT_TextInput: {
            auto& config = configs[wtype][index].config.input;
            config.tooltip = tooltip; config.tooltipType = type;
            break;
        }
        case WT_DropDown: {
            auto& config = configs[wtype][index].config.dropdown;
            config.tooltip = tooltip; config.tooltipType = type;
            break;
        }
        case WT_TabBar: {
            auto& config = configs[wtype][index].config.tab;
            config.tooltip = tooltip; config.tooltipType = type;
            break;
        }
        case WT_ItemGrid: {
            auto& config = configs[wtype][index].config.grid;
            config.tooltip = tooltip; config.tooltipType = type;
            break;
        }
        case WT_MediaResource: {
            auto& config = configs[wtype][index].config.media;
            config.tooltip = tooltip; config.tooltipType = type;
            break;
        }
        default:
            break;
        }
    }

    StyleDescriptor WidgetContextData::GetStateStyle(int32_t state)
    {
        auto res = StyleEngine.GetStyle(state);
        AddFontPtr(res.font);
        return res;
    }

    StyleDescriptor WidgetContextData::GetStyle(int32_t state, int32_t id)
    {
        auto index = id & WidgetIndexMask;
        auto wtype = (WidgetType)(id >> WidgetTypeBits);
        if (state == -1)
            state = states[wtype][index].state;
        auto& style = states[wtype][index].style;
        AddFontPtr(style.font);
        return style;
    }

    StyleDescriptor WidgetContextData::GetStyle(int32_t id)
    {
        auto index = id & WidgetIndexMask;
        auto wtype = (WidgetType)(id >> WidgetTypeBits);
        auto state = states[wtype][index].state;
        auto& style = states[wtype][index].style;
        AddFontPtr(style.font);
        return style;
    }

    void WidgetContextData::RegisterWidgetIdClass(WidgetType wt, int32_t index, const WidgetIdClasses& idClasses)
    {
        /*if (!idClasses.classes.empty())
        {
            for (int i = 0; i < idClasses.classes.size(); ++i)
            {
                for (auto idx = 0; idx < WSI_Total; ++idx)
                {
                    auto& style = glimmer::GetStyle(idClasses.classes[i], (WidgetStateIndex)idx);
                    WidgetStyles[wt][index][idx].InheritFrom(style);
                }
            }
        }

        for (auto idx = 0; idx < WSI_Total; ++idx)
        {
            auto& style = glimmer::GetStyle(idClasses.id, (WidgetStateIndex)idx);
            WidgetStyles[wt][index][idx].InheritFrom(style);
        }*/
    }

    void WidgetContextData::RemovePopup()
    {
        RemovePopupAtFrameExit = true;
    }

    IRenderer& WidgetContextData::ToggleDeferedRendering(bool defer, bool reset)
    {
        usingDeferred = defer;

        if (defer)
        {
            auto renderer = CreateDeferredRenderer();
            if (reset) { renderer->Reset(); adhocLayout.push(); }
            deferedRenderer = renderer;
            return *deferedRenderer;
        }
        else if (reset)
        {
            if (adhocLayout.size() > 1) adhocLayout.pop(1, true);
            deferedRenderer->Reset();
            return *Config.renderer;
        }

        return *Config.renderer;
    }

    IRenderer& WidgetContextData::GetRenderer()
    {
        if (!containerStack.empty())
        {
            auto id = containerStack.top();
            auto wtype = (WidgetType)(id >> WidgetTypeBits);

            if (wtype == WT_Accordion)
            {
                auto& accordion = accordions.top();
                return (accordion.geometry & FromRight) || (accordion.geometry & FromBottom) || usingDeferred ?
                    *deferedRenderer : *Config.renderer;
            }
        }

        return usingDeferred ? *deferedRenderer : *Config.renderer;
    }

    void WidgetContextData::PushContainer(int32_t parentId, int32_t id)
    {
        auto index = id & WidgetIndexMask;
        auto wtype = (WidgetType)(id >> WidgetTypeBits);

        if (wtype == WT_SplitterRegion)
            splitterScrollPaneParentIds[index] = parentId;

        containerStack.push() = id;
    }

    void WidgetContextData::PopContainer(int32_t id)
    {
        containerStack.pop(1, true);

        auto index = id & WidgetIndexMask;
        auto wtype = (WidgetType)(id >> WidgetTypeBits);

        if (wtype == WT_SplitterRegion)
        {
            splitterScrollPaneParentIds[index] = -1;
        }
    }

    void WidgetContextData::RecordDeferRange(RendererEventIndexRange& range, bool start) const
    {
        if (start)
        {
            range.events.first = deferedEvents.size();
            range.primitives.first = deferedRenderer->TotalEnqueued();
        }
        else
        {
            range.events.second = deferedEvents.size();
            range.primitives.second = deferedRenderer->TotalEnqueued();
        }
    }
    
    void WidgetContextData::AddItemGeometry(int id, const ImRect& geometry, bool ignoreParent)
    {
        auto index = id & WidgetIndexMask;
        auto wtype = (WidgetType)(id >> WidgetTypeBits);

        if (wtype < WT_TotalTypes)
        {
            if (index >= itemGeometries[wtype].size())
            {
                auto sz = std::max((int16_t)128, (int16_t)(index - itemGeometries[wtype].size() + 1));
                itemGeometries[wtype].expand_and_create(sz, true);
            }

            itemGeometries[wtype][index] = geometry;
        }

        adhocLayout.top().lastItemId = id;

        if (!containerStack.empty() && !ignoreParent)
        {
            id = containerStack.top();
            wtype = (WidgetType)(id >> WidgetTypeBits);

            if (wtype == WT_Scrollable)
            {
                index = id & WidgetIndexMask;
                auto& region = configs[wtype][index].config.scroll;
                region.content.x = std::max(region.content.x, geometry.Max.x);
                region.content.y = std::max(region.content.y, geometry.Max.y);
            }
            else if (wtype == WT_Accordion)
            {
                auto& accordion = accordions.top();
                accordion.extent.x = std::max(accordion.extent.x, geometry.Max.x);
                accordion.extent.y = std::max(accordion.extent.y, geometry.Max.y);
            }
        }
    }

    EventDeferInfo EventDeferInfo::ForRegion(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding, const ImRect& content)
    {
        EventDeferInfo info;
        info.type = WT_Region;
        info.id = id;
        info.params.region.margin = margin;
        info.params.region.border = border;
        info.params.region.padding = padding;
        info.params.region.content = content;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForLabel(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text)
    {
        EventDeferInfo info;
        info.type = WT_Label;
        info.id = id;
        info.params.label.margin = margin;
        info.params.label.border = border;
        info.params.label.padding = padding;
        info.params.label.content = content;
        info.params.label.text = text;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForButton(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding, const ImRect& content, const ImRect& text)
    {
        EventDeferInfo info;
        info.type = WT_Button;
        info.id = id;
        info.params.button.margin = margin;
        info.params.button.border = border;
        info.params.button.padding = padding;
        info.params.button.content = content;
        info.params.button.text = text;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForCheckbox(int32_t id, const ImRect& extent)
    {
        EventDeferInfo info;
        info.type = WT_Checkbox;
        info.id = id;
        info.params.checkbox.extent = extent;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForRadioButton(int32_t id, const ImRect& extent, float maxrad)
    {
        EventDeferInfo info;
        info.type = WT_RadioButton;
        info.id = id;
        info.params.radio.extent = extent;
        info.params.radio.maxrad = maxrad;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForToggleButton(int32_t id, const ImRect& extent, ImVec2 center)
    {
        EventDeferInfo info;
        info.type = WT_ToggleButton;
        info.id = id;
        info.params.toggle.extent = extent;
        info.params.toggle.center = center;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForSpinner(int32_t id, const ImRect& extent, const ImRect& incbtn, const ImRect& decbtn)
    {
        EventDeferInfo info;
        info.type = WT_Spinner;
        info.id = id;
        info.params.spinner.extent = extent;
        info.params.spinner.incbtn = incbtn;
        info.params.spinner.decbtn = decbtn;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForSlider(int32_t id, const ImRect& extent, const ImRect& thumb)
    {
        EventDeferInfo info;
        info.type = WT_Slider;
        info.id = id;
        info.params.slider.extent = extent;
        info.params.slider.thumb = thumb;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForRangeSlider(int32_t id, const ImRect& extent, const ImRect& minthumb, const ImRect& maxthumb)
    {
        EventDeferInfo info;
        info.type = WT_RangeSlider;
        info.id = id;
        info.params.rangeslider.extent = extent;
        info.params.rangeslider.minThumb = minthumb;
        info.params.rangeslider.maxThumb = maxthumb;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForTextInput(int32_t id, const ImRect& extent, const ImRect& clear)
    {
        EventDeferInfo info;
        info.type = WT_TextInput;
        info.id = id;
        info.params.input.content = extent;
        info.params.input.clear = clear;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForDropDown(int32_t id, const ImRect& margin, const ImRect& border,
        const ImRect& padding, const ImRect& content)
    {
        EventDeferInfo info;
        info.type = WT_DropDown;
        info.id = id;
        info.params.dropdown.margin = margin;
        info.params.dropdown.border = border;
        info.params.dropdown.padding = padding;
        info.params.dropdown.content = content;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForTabBar(int32_t id, const ImRect& content)
    {
        EventDeferInfo info;
        info.type = WT_TabBar;
        info.id = id;
        info.params.tabbar.content = content;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForNavDrawer(int32_t id)
    {
        EventDeferInfo info;
        info.type = WT_NavDrawer;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForAccordion(int32_t id, const ImRect& region, int32_t ridx)
    {
        EventDeferInfo info;
        info.type = WT_Accordion;
        info.id = id;
        info.params.accordion.region = region;
        info.params.accordion.ridx = ridx;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForScrollRegion(int32_t id)
    {
        EventDeferInfo info;
        info.type = WT_Scrollable;
        info.id = id;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForMediaResource(int32_t id, const ImRect& padding, const ImRect& content)
    {
        EventDeferInfo info;
        info.type = WT_MediaResource;
        info.id = id;
        info.params.media.content = content;
        info.params.media.padding = padding;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForCustom(int32_t id)
    {
        EventDeferInfo info;
        info.type = WT_Custom;
        info.id = id;
        return info;
    }

    WidgetDrawResult WidgetContextData::HandleEvents(ImVec2 origin, int from, int to)
    {
        WidgetDrawResult result;
        auto& renderer = usingDeferred ? *deferedRenderer : *Config.renderer;
        to = to == -1 ? deferedEvents.size() : to;

        for (auto idx = from; idx < to; ++idx)
        {
            auto ev = deferedEvents[idx];
            auto res = HandleDeferredEvent(*this, ev, origin, renderer);
            if (res.event != WidgetEvent::None)
                result = res;
            result.id = ev.id;
            result.geometry = res.geometry;
        }

        if (to == -1) deferedEvents.clear(true);
        return result;
    }

    void WidgetContextData::ResetLayoutData()
    {
        for (auto idx = 0; idx < WSI_Total; ++idx)
            layoutStyles[idx].clear(true);

        layoutStack.clear(false);
        layoutItems.clear(true);
        replayContent.clear(true);
    }

    void WidgetContextData::ClearDeferredData()
    {
        deferedEvents.clear(true);
        deferedRenderer->Reset();
    }

    ImRect WidgetContextData::GetGeometry(int32_t id) const
    {
        auto index = id & WidgetIndexMask;
        auto wtype = id >> WidgetTypeBits;

        if (wtype < WT_TotalTypes)
        {
            return itemGeometries[wtype].size() > index ?
                itemGeometries[wtype][index] : ImRect{};
        }
        else return Config.CustomWidgetProvider ?
            Config.CustomWidgetProvider(wtype)->GetGeometry(id) : ImRect{};
    }

    ImRect WidgetContextData::GetLayoutSize() const
    {
        return lastLayoutIdx == -1 ? ImRect{} : itemGeometries[WT_Layout][lastLayoutIdx];
    }

    ImVec2 WidgetContextData::MaximumSize() const
    {
        if (!containerStack.empty())
        {
            auto id = containerStack.top();
            auto index = id & WidgetIndexMask;
            auto wtype = (WidgetType)(id >> WidgetTypeBits);

            if (wtype == WT_ItemGrid)
            {
                auto& grid = CurrentItemGridContext->itemGrids.top();
                auto extent = grid.headers[grid.currlevel][grid.currCol].extent;
                return extent.GetSize();
            }
            else if (wtype == WT_Scrollable)
            {
                auto index = id & WidgetIndexMask;
                auto type = id >> WidgetTypeBits;
                auto& region = configs[type][index].config.scroll;
                auto sz = ImVec2{ (region.type & ST_Horizontal) ? region.extent.x : region.viewport.GetWidth(),
                    (region.type & ST_Vertical) ? region.extent.y : region.viewport.GetHeight() };
                if (region.type & ST_Always_H) sz.y -= Config.scrollbar.width;
                if (region.type & ST_Always_V) sz.x -= Config.scrollbar.width;
                return sz;
            }
            else if (wtype == WT_Accordion)
            {
                auto& accordion = accordions.top();
                return ImVec2{ accordion.hscroll ? accordion.extent.x - accordion.origin.x : accordion.size.x,
                    accordion.vscroll ? accordion.extent.y - accordion.origin.y : accordion.size.y };
            }
            else
            {
                return wtype < WT_TotalTypes ? itemGeometries[wtype][index].GetSize() :
                    Config.CustomWidgetProvider ? Config.CustomWidgetProvider(wtype)->GetGeometry(id).GetSize() : 
                        ImVec2{};
            }
        }
        else
        {
            auto rect = ImGui::GetCurrentWindow()->InnerClipRect;
            auto currpos = adhocLayout.top().nextpos;
            return ImVec2{ popupSize.x == -1.f ? rect.GetWidth() - currpos.x : popupSize.x,
                popupSize.y == -1.f ? rect.GetHeight() - currpos.y : popupSize.y };
        }
    }

    ImVec2 WidgetContextData::MaximumExtent() const
    {
        if (!containerStack.empty())
        {
            auto id = containerStack.top();
            auto index = id & WidgetIndexMask;
            auto wtype = (WidgetType)(id >> WidgetTypeBits);

            if (wtype == WT_ItemGrid)
            {
                auto& grid = CurrentItemGridContext->itemGrids.top();
                const auto& config = CurrentItemGridContext->GetConfig(id).config.grid;
                auto max = grid.headers[grid.currlevel][grid.currCol].extent.Max - config.cellpadding;
                return max;
            }
            else if (wtype == WT_Scrollable)
            {
                auto index = id & WidgetIndexMask;
                auto type = id >> WidgetTypeBits;
                auto& region = configs[type][index].config.scroll;
                auto sz = ImVec2{ (region.type & ST_Horizontal) ? region.extent.x + region.viewport.Min.x : region.viewport.Max.x,
                    (region.type & ST_Vertical) ? region.extent.y + region.viewport.Min.y : region.viewport.Max.y };
                if (region.type & ST_Always_H) sz.y -= Config.scrollbar.width;
                if (region.type & ST_Always_V) sz.x -= Config.scrollbar.width;
                return sz;
            }
            else if (wtype == WT_Accordion)
            {
                auto& accordion = accordions.top();
                const auto& state = AccordionState(accordion.id);
                const auto& region = state.scrolls[accordion.totalRegions];
                return ImVec2{ accordion.hscroll ? region.extent.x : region.viewport.Max.x,
                    accordion.vscroll ? region.extent.y : region.viewport.Max.y };
            }
            else
            {
                return wtype < WT_TotalTypes ? itemGeometries[wtype][index].Max :
                    Config.CustomWidgetProvider ? Config.CustomWidgetProvider(wtype)->GetGeometry(id).Max :
                    ImVec2{};
            }
        }
        else
        {
            auto rect = ImGui::GetCurrentWindow()->InnerClipRect;
            auto ispopup = popupOrigin.x != -1.f;
            return ispopup ? popupOrigin + ImMin(popupSize, rect.Max) : rect.Max;
        }
    }

    ImVec2 WidgetContextData::WindowSize() const
    {
        auto rect = ImGui::GetCurrentWindow()->InnerClipRect;
        auto ispopup = popupOrigin.x != -1.f;
        return ispopup ? popupSize : rect.GetSize();
    }

    int WidgetContextData::GetExpectedWidgetCount(WidgetType type)
    {
        switch (type)
        {
            case WT_ItemGrid: return 4;
            case WT_TabBar: return 8;
            case WT_Splitter: return 4;
            case WT_Accordion: return 4;
            case WT_DropDown: return 16;
            default: return 32; // Default for other widget types
        }
    }
    
    WidgetContextData::WidgetContextData()
    {
        for (auto idx = 0; idx < WT_TotalTypes; ++idx)
        {
            maxids[idx] = tempids[idx] = 0;

            if (idx != WT_Layout && idx != WT_Sublayout && idx != WT_Scrollable &&
                idx != WT_SplitterRegion)
            {
                auto count = Config.GetTotalWidgetCount ? Config.GetTotalWidgetCount((WidgetType)idx) : 
                    GetExpectedWidgetCount((WidgetType)idx);
                configs[idx].resize(count, WidgetConfigData{ (WidgetType)idx });
                states[idx].resize(count, WidgetStateData{});
                //WidgetStyles[idx].resize(count);

                switch (idx)
                {
                case WT_ItemGrid: gridStates.resize(count); break;
                case WT_TabBar: tabBarStates.resize(count); break;
                case WT_NavDrawer: navDrawerStates.resize(count); break;
                case WT_ToggleButton: toggleStates.resize(count); break;
                case WT_RadioButton: radioStates.resize(count); break;
                case WT_Checkbox: checkboxStates.resize(count); break;
                case WT_TextInput: inputTextStates.resize(count); break;
                case WT_Spinner: spinnerStates.resize(count); break;
                case WT_DropDown: dropdownStates.resize(count); break;
                case WT_Splitter: 
                    splitterStates.resize(count); 
                    splitterScrollPaneParentIds.resize(count * GLIMMER_MAX_SPLITTER_REGIONS, -1);
                    break;
                case WT_Accordion: accordionStates.resize(count); break;
                default:
                    break;
                }
            }
        }
    }
    
    SplitterPersistentState::SplitterPersistentState()
    {
        for (auto idx = 0; idx < GLIMMER_MAX_SPLITTER_REGIONS; ++idx)
        {
            states[idx] = WS_Default;
            containers[idx] = -1;
            isdragged[idx] = false;
            dragstart[idx] = 0.f;
        }
    }

    WidgetContextData& GetContext()
    {
        return *CurrentContext;
    }

    WidgetContextData& PushContext(int32_t id)
    {
        if (id < 0)
        {
            if (CurrentContext == nullptr)
            {
                constexpr auto totalStyles = 16 * WSI_Total;
                CurrentContext = &(WidgetContexts.emplace_back());
                ContextPushed(CurrentContext);
                InitContextStyles();
            }
        }
        else
        {
            auto wtype = (id >> WidgetTypeBits);
            auto index = id & WidgetIndexMask;
            auto& children = CurrentContext->nestedContexts[wtype];

            if ((int)children.size() <= index)
            {
                auto count = Config.GetTotalWidgetCount ?
                    Config.GetTotalWidgetCount((WidgetType)wtype) :
                    WidgetContextData::GetExpectedWidgetCount((WidgetType)wtype);
                auto required = children.size() + count;
                children.reserve(required);

				for (auto idx = 0; idx < required; ++idx)
                {
                    if (idx == index)
                    {
                        auto& ctx = WidgetContexts.emplace_back();
                        ctx.parentContext = CurrentContext;
                        ctx.InsideFrame = CurrentContext->InsideFrame;
                        auto& layout = ctx.adhocLayout.push();
                        layout.nextpos = CurrentContext->adhocLayout.top().nextpos;
                        children.emplace_back(&ctx);
                    }
                    else
						children.emplace_back(nullptr);
                }
            }
			else if (children[index] == nullptr)
            {
                auto& ctx = WidgetContexts.emplace_back();
                ctx.parentContext = CurrentContext;
                ctx.InsideFrame = CurrentContext->InsideFrame;
                auto& layout = ctx.adhocLayout.push();
                layout.nextpos = CurrentContext->adhocLayout.top().nextpos;
                children[index] = &ctx;
            }

            CurrentContext = CurrentContext->nestedContexts[wtype][index];
            ContextPushed(CurrentContext);
            
            for (auto idx = 0; idx < WT_TotalTypes; ++idx) CurrentContext->maxids[idx] = 0;
        }

        return *CurrentContext;
    }

    WidgetContextData& PushContext(int32_t id, NestedContextSourceType source)
    {
        auto curr = CurrentContext;
		auto& ctx = PushContext(id);
        auto& el = ctx.nestedContextStack.push();
        el.base = curr;
        el.source = source;
        el.id = id;
        return ctx;
    }

    void PopContext()
    {
        CurrentContext = GetContext().parentContext;
        ContextPopped();
        assert(CurrentContext != nullptr);
    }

    void PopNestedSource(WidgetContextData* context)
    {
        if (context == nullptr) context = &GetContext();
        context->nestedContextStack.pop(1, true);
    }

    void Cleanup()
    {
        ImPlot::DestroyContext(ChartsContext);
        if (Config.logger) Config.logger->Finish();
    }

    // This is a global config which can only be set during initialization
    // Read-only access is provided once set for subsequent frames
    UIConfig Config{};

    // Default global style engine
    GlimmerStyleEngine StyleEngine{};

    NestedContextSource InvalidSource{};

    bool WidgetContextData::CacheItemGeometry = false;
    ImRect WidgetContextData::ActivePopUpRegion;
    UIElementDescriptor WidgetContextData::RightClickContext;
    WidgetContextData* WidgetContextData::PopupContext = nullptr;
    Vector<ContextMenuItemDescriptor, int16_t, 16> WidgetContextData::ContextMenuOptions{ false };
    Vector<ContextMenuItemParams, int16_t, 16> WidgetContextData::ContextMenuOptionParams{ false };
    int32_t WidgetContextData::PopupTarget = -1;
    WidgetContextData* WidgetContextData::CurrentItemGridContext = nullptr;
    int32_t WidgetContextData::CurrentWidgetId = -1;
}
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
     
    void ItemGridInternalState::swapColumns(int16_t from, int16_t to, std::vector<std::vector<ItemGridState::ColumnConfig>>& headers, int level)
    {
        auto lfrom = colmap[level].vtol[from];
        auto lto = colmap[level].vtol[to];
        colmap[level].vtol[from] = lto; colmap[level].ltov[lfrom] = to;
        colmap[level].vtol[to] = lfrom; colmap[level].ltov[lto] = from;

        std::pair<int16_t, int16_t> movingColRangeFrom = { lfrom, lfrom }, nextMovingRangeFrom = { INT16_MAX, -1 };
        std::pair<int16_t, int16_t> movingColRangeTo = { lto, lto }, nextMovingRangeTo = { INT16_MAX, -1 };

        for (auto l = level + 1; l < (int)headers.size(); ++l)
        {
            for (int16_t col = 0; col < (int16_t)headers[l].size(); ++col)
            {
                auto& hdr = headers[l][col];
                if (hdr.parent >= movingColRangeFrom.first && hdr.parent <= movingColRangeFrom.second)
                {
                    nextMovingRangeFrom.first = std::min(nextMovingRangeFrom.first, col);
                    nextMovingRangeFrom.second = std::max(nextMovingRangeFrom.second, col);
                }
                else if (hdr.parent >= movingColRangeTo.first && hdr.parent <= movingColRangeTo.second)
                {
                    nextMovingRangeTo.first = std::min(nextMovingRangeTo.first, col);
                    nextMovingRangeTo.second = std::max(nextMovingRangeTo.second, col);
                }
            }

            auto startTo = colmap[l].ltov[nextMovingRangeFrom.first];
            auto startFrom = colmap[l].ltov[nextMovingRangeTo.first];

            for (auto col = nextMovingRangeTo.first, idx = startTo; col <= nextMovingRangeTo.second; ++col, ++idx)
            {
                colmap[l].ltov[col] = idx;
                colmap[l].vtol[idx] = col;
            }

            for (auto col = nextMovingRangeFrom.first, idx = startFrom; col <= nextMovingRangeFrom.second; ++col, ++idx)
            {
                colmap[l].ltov[col] = idx;
                colmap[l].vtol[idx] = col;
            }

            movingColRangeFrom = nextMovingRangeFrom;
            movingColRangeTo = nextMovingRangeTo;
            nextMovingRangeFrom = nextMovingRangeTo = { INT16_MAX, -1 };
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

    void WidgetContextData::ToggleDeferedRendering(bool defer)
    {
        usingDeferred = defer;

        if (defer)
        {
            auto renderer = CreateDeferredRenderer(&ImGuiMeasureText);
            renderer->Reset();
            deferedRenderer = renderer;
            adhocLayout.push();
        }
        else
        {
            adhocLayout.pop(1, true);
            deferedRenderer = nullptr;
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

    WidgetDrawResult WidgetContextData::HandleEvents(ImVec2 origin)
    {
        auto io = Config.platform->CurrentIO();
        auto& renderer = usingDeferred ? *deferedRenderer : *Config.renderer;
        WidgetDrawResult result;

        for (auto ev : deferedEvents)
        {
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

        deferedEvents.clear(true);
        return result;
    }

    void WidgetContextData::ResetLayoutData()
    {
        for (auto lidx = 0; lidx < layouts.size(); ++lidx)
            layouts[lidx].reset();

        layouts.clear(false);
        layoutItems.clear(true);
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
            return dir == DIR_Horizontal ? itemGeometries[wtype][index].GetWidth() : 
                itemGeometries[wtype][index].GetHeight();
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
            return ImVec2{ itemGeometries[wtype][index].GetWidth(),
                itemGeometries[wtype][index].GetHeight() };
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
            return itemGeometries[wtype][index].Max;
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

        gridStates.resize(4);
        tabStates.resize(8);
        toggleStates.resize(32);
        radioStates.resize(32);
        checkboxStates.resize(32);
        inputTextStates.resize(32);
        splitterStates.resize(4);
        spinnerStates.resize(8);
        tabBarStates.resize(4);
        splitterScrollPaneParentIds.resize(32 * GLIMMER_MAX_SPLITTER_REGIONS, -1);
        dropDownOptions.resize(32);

        for (auto idx = 0; idx < WT_TotalTypes; ++idx)
        {
            maxids[idx] = 0;
            states[idx].resize(32, WidgetStateData{ (WidgetType)idx });
        }
    }
    
    SplitterInternalState::SplitterInternalState()
    {
        for (auto idx = 0; idx < GLIMMER_MAX_SPLITTER_REGIONS; ++idx)
        {
            states[idx] = WS_Default;
            scrollids[idx] = -1;
            isdragged[idx] = false;
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

                auto& children = CurrentContext->nestedContexts[wtype];
                if (children.empty()) children.resize(32, nullptr);
                children[index] = &ctx;
            }

            CurrentContext = CurrentContext->nestedContexts[wtype][index];
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
    DynamicStack<ToggleButtonStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES> WidgetContextData::toggleButtonStyles[WSI_Total];
    DynamicStack<RadioButtonStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES>  WidgetContextData::radioButtonStyles[WSI_Total];
    DynamicStack<SliderStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES> WidgetContextData::sliderStyles[WSI_Total];
    DynamicStack<SpinnerStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES> WidgetContextData::spinnerStyles[WSI_Total];
    DynamicStack<TabBarStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES> WidgetContextData::tabBarStyles[WSI_Total];
}
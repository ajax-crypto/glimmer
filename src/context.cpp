#include "context.h"
#include "im_font_manager.h"
#include "renderer.h"
#include <chrono>
#include <list>

#ifndef GLIMMER_MAX_OVERLAYS
#define GLIMMER_MAX_OVERLAYS 32
#endif

namespace glimmer
{
    static std::list<WidgetContextData> WidgetContexts;
    static WidgetContextData* CurrentContext = nullptr;

    void AnimationData::moveByPixel(float amount, float max, float reset)
    {
        auto currts = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock().now().time_since_epoch()).count();
        if (timestamp == 0) timestamp = currts;

        if (currts - timestamp >= 33)
        {
            offset += amount;
            if (offset >= max) offset = reset;
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

    StyleDescriptor& WidgetContextData::GetStyle(int32_t state)
    {
        auto style = log2((unsigned)state);
        auto& res = (state & currStyleStates) ? currStyle[style] : pushedStyles[style].top();
        if (res.font.font == nullptr) res.font.font = GetFont(res.font.family, res.font.size, FT_Normal);
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
            adhocLayout.pop();
            deferedRenderer = nullptr;
        }
    }

    void WidgetContextData::PushContainer(int32_t parentId, int32_t id)
    {
        auto index = id & 0xffff;
        auto wtype = (WidgetType)(id >> 16);

        if (wtype == WT_SplitterScrollRegion)
            splitterScrollPaneParentIds[index] = parentId;

        containerStack.push(id);
    }

    void WidgetContextData::PopContainer(int32_t id)
    {
        containerStack.pop();

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
    void HandleTextInputEvent(int32_t id, const ImRect& content, const IODescriptor& io,
        IRenderer& renderer, WidgetDrawResult& result);
    void HandleDropDownEvent(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const IODescriptor& io, IRenderer& renderer, WidgetDrawResult& result);
    void HandleItemGridEvent(int32_t id, const ImRect& padding, const ImRect& content,
        const IODescriptor& io, IRenderer& renderer, WidgetDrawResult& result);

    WidgetDrawResult WidgetContextData::HandleEvents()
    {
        auto io = Config.platform->CurrentIO();
        auto& renderer = usingDeferred ? *deferedRenderer : *Config.renderer;
        WidgetDrawResult result;

        for (const auto& ev : deferedEvents)
        {
            switch (ev.type)
            {
            case WT_Label: 
                HandleLabelEvent(ev.id, ev.margin, ev.border, ev.padding, ev.content, ev.text, renderer, io, result);
                break;
            case WT_Button:
                HandleButtonEvent(ev.id, ev.margin, ev.border, ev.padding, ev.content, ev.text, renderer, io, result);
                break;
            case WT_Checkbox:
                HandleCheckboxEvent(ev.id, ev.extent, io, result);
                break;
            case WT_RadioButton:
                HandleRadioButtonEvent(ev.id, ev.extent, ev.maxrad, renderer, io, result);
                break;
            case WT_ToggleButton:
                HandleToggleButtonEvent(ev.id, ev.extent, ev.center, renderer, io, result);
                break;
            case WT_TextInput:
                HandleTextInputEvent(ev.id, ev.content, io, renderer, result);
                break;
            case WT_DropDown:
                HandleDropDownEvent(ev.id, ev.margin, ev.border, ev.padding, ev.content, io, renderer, result);
                break;
            case WT_ItemGrid:
                HandleItemGridEvent(ev.id, ev.padding, ev.content, io, renderer, result);
                break;
            default:
                break;
            }
        }

        deferedEvents.clear();
        return result;
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
        std::memset(maxids, 0, WT_TotalTypes);
        std::memset(tempids, INT_MAX, WT_TotalTypes);

        gridStates.resize(8);
        tabStates.resize(16);
        toggleStates.resize(32);
        radioStates.resize(32);
        checkboxStates.resize(32);
        inputTextStates.resize(32);
        splitterStates.resize(4);
        splitterScrollPaneParentIds.resize(32 * 8, -1);
        dropDownOptions.resize(32);

        for (auto idx = 0; idx < WT_TotalTypes; ++idx)
        {
            maxids[idx] = 0;
            states[idx].resize(32, WidgetStateData{ (WidgetType)idx });
        }
    }
    
    SplitterInternalState::SplitterInternalState()
    {
        for (auto idx = 0; idx < 8; ++idx)
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

    WidgetContextData& PushContext(int32_t id)
    {
        if (id < 0)
        {
            if (CurrentContext == nullptr)
            {
                constexpr auto totalStyles = 16 * WSI_Total;
                CurrentContext = &(WidgetContexts.emplace_back());

                for (auto idx = 0; idx < WSI_Total; ++idx)
                {
                    CurrentContext->pushedStyles[idx].push();
                    CurrentContext->radioButtonStyles[idx].push();
                    auto& toggle = CurrentContext->toggleButtonStyles[idx].push();
                    toggle.fontsz *= Config.fontScaling;

                    if (idx != WSI_Checked)
                    {
                        toggle.trackColor = ToRGBA(200, 200, 200);
                        toggle.indicatorTextColor = ToRGBA(100, 100, 100);
                    }
                    else
                    {
                        toggle.trackColor = ToRGBA(152, 251, 152);
                        toggle.indicatorTextColor = ToRGBA(0, 100, 0);
                    }
                }
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

    DynamicStack<StyleDescriptor, int16_t> WidgetContextData::pushedStyles[WSI_Total];
    StyleDescriptor WidgetContextData::currStyle[WSI_Total];
    int32_t WidgetContextData::currStyleStates;
    DynamicStack<ToggleButtonStyleDescriptor, int16_t> WidgetContextData::toggleButtonStyles[WSI_Total];
    DynamicStack<RadioButtonStyleDescriptor, int16_t>  WidgetContextData::radioButtonStyles[WSI_Total];
}
#include "widgets.h"

#include <cstring>
#include <cassert>
#include <cmath>
#include <cctype>
#include <charconv>
#include "style.h"
#include "draw.h"
#include "context.h"
#include "layout.h"
#include "im_font_manager.h"
#include "imrichtext.h"
#include "testing.h"

#ifndef GLIMMER_DISABLE_PLOTS
#include "libs/inc/implot/implot.h"
#endif

#include <unordered_map>
#include <string>
#include <stdint.h>

namespace glimmer
{
    // This is required, as for some widgets, double clicking leads to a editor, and hence a text input widget
    WidgetDrawResult TextInputImpl(int32_t id, TextInputState& state, const StyleDescriptor& style, const ImRect& extent, const ImRect& content,
        const ImRect& prefix, const ImRect& suffix, IRenderer& renderer, const IODescriptor& io);
    void AddExtent(LayoutItemDescriptor& layoutItem, const StyleDescriptor& style, const NeighborWidgets& neighbors,
        ImVec2 size, ImVec2 totalsz);
    WidgetDrawResult Widget(int32_t id, WidgetType type, int32_t geometry, const NeighborWidgets& neighbors, bool onlyComputeGeometry = false);
    ImRect BeginFlexLayoutRegion(Direction dir, int32_t geometry, bool wrap,
        ImVec2 spacing, ImVec2 size, const NeighborWidgets& neighbors, int regionIdx);
    ImRect BeginGridLayoutRegion(int rows, int cols, GridLayoutDirection dir, int32_t geometry, const std::initializer_list<float>& rowExtents,
        const std::initializer_list<float>& colExtents, ImVec2 spacing, ImVec2 size,
        const NeighborWidgets& neighbors, int regionIdx);
    bool BeginPopup(int32_t id, ImVec2 origin, ImVec2 size, NestedContextSourceType type);
    WidgetDrawResult EndPopUp(WidgetContextData& overlayctx, std::optional<uint32_t> bgcoloropt, int32_t flags);
    bool BeginPopup(int32_t id, ImVec2 origin, ImVec2 size);
    bool HandleContextMenu(int32_t id, const ImRect& region, const IODescriptor& io);

#pragma region Widget ID Handling

    static Vector<char, int32_t> IdStringBackingStore{ GLIMMER_TOTAL_ID_SIZE, 0 };
    static int32_t IdStringBackingStoreSize = 0;
    static std::unordered_map<std::string_view, int32_t> NamedIds[WT_TotalTypes];
    static std::unordered_map<void*, int32_t> OutPtrIds[WT_TotalTypes];

    static std::string_view CreatePermanentCopy(std::string_view input)
    {
        auto sz = input.size();
        memcpy(IdStringBackingStore.data() + IdStringBackingStoreSize, input.data(), sz);
        auto res = std::string_view{ IdStringBackingStore.data() + IdStringBackingStoreSize, sz };
        IdStringBackingStoreSize += sz;
        return res;
    }

    static WidgetIdClasses ExtractIdClasses(std::string_view input)
    {
        auto idx = 0;
        WidgetIdClasses result;

        while (idx < (int)input.size() && std::isspace(input[idx]))
            ++idx;

        while (idx < (int)input.size())
        {
            if (input[idx] != '.')
            {
                if (input[idx] == '#') idx++;
                auto start = idx;
                while (idx < (int)input.size() && !std::isspace(input[idx]))
                    ++idx;

                result.id = input.substr(input[start] == '#' ? start + 1 : start, idx - start);
            }
            else
            {
                idx++;
                auto start = idx;
                while (idx < (int)input.size() && !std::isspace(input[idx]))
                    ++idx;

                result.classes.emplace_back(input.substr(start, idx - start));
            }

            while (idx < (int)input.size() && std::isspace(input[idx]))
                ++idx;
        }

        return result;
    }

    std::pair<int32_t, bool> GetIdFromString(std::string_view id, WidgetType type)
    {
        auto initial = false;
        auto it = NamedIds[type].find(id);
        if (it == NamedIds[type].end())
        {
            auto key = CreatePermanentCopy(id);
            auto idClasses = ExtractIdClasses(key);
            it = NamedIds[type].emplace(idClasses.id, GetNextId(type)).first;
            GetContext().RegisterWidgetIdClass(type, it->second, idClasses);
            if (Config.RecordWidgetId) (*Config.RecordWidgetId)(key, it->second);
            if (Config.logger) Config.logger->RegisterId(it->second, id);
            initial = true;
        }
        return { it->second, initial };
    }

    static std::pair<int32_t, bool> GetIdFromOutPtr(void* ptr, WidgetType type)
    {
        assert(ptr != nullptr);
        auto it = OutPtrIds[type].find(ptr);
        if (it == OutPtrIds[type].end())
        {
            auto id = GetNextId(type);
            if (Config.logger) Config.logger->RegisterId(id, ptr);
            return { OutPtrIds[type].emplace(ptr, id).first->second, true };
        }
        return { it->second, false };
    }

    int32_t GetNextId(WidgetType type)
    {
        int32_t id = GetNextCount(type);
        id = id | (type << WidgetTypeBits);
        return id;
    }

    int16_t GetNextCount(WidgetType type)
    {
        auto& context = GetContext();
        return context.GetNextCount(type);
    }

#pragma endregion

#pragma region WidgetConfigData

    WidgetConfigData::WidgetConfigData(WidgetType ty)
        : type{ ty }
    {
        switch (type)
        {
        case WT_Region: ::new (&state.region) RegionState{}; break;
        case WT_Label: ::new (&state.label) LabelState{}; break;
        case WT_Button: ::new (&state.button) ButtonState{}; break;
        case WT_RadioButton: new (&state.radio) RadioButtonState{}; break;
        case WT_ToggleButton: new (&state.toggle) ToggleButtonState{}; break;
        case WT_Checkbox: new (&state.checkbox) CheckboxState{}; break;
        case WT_Spinner: new (&state.spinner) SpinnerState{}; break;
        case WT_Slider: new (&state.slider) SliderState{}; break;
        case WT_RangeSlider: new (&state.slider) RangeSliderState{}; break;
        case WT_TextInput: new (&state.input) TextInputState{}; break;
        case WT_DropDown: new (&state.dropdown) DropDownState{}; break;
        case WT_SplitterRegion: [[fallthrough]];
        case WT_Scrollable: new (&state.scroll) ScrollableRegion{}; break;
        case WT_TabBar: new (&state.tab) TabBarState{}; break;
        case WT_ItemGrid: ::new (&state.grid) ItemGridConfig{}; break;
        case WT_MediaResource: ::new (&state.media) MediaState{}; break;
        default: break;
        }
    }

    WidgetConfigData::WidgetConfigData(const WidgetConfigData& src)
        : WidgetConfigData{ src.type }
    {
        switch (type)
        {
        case WT_Region: state.region = src.state.region; break;
        case WT_Label: state.label = src.state.label; break;
        case WT_Button: state.button = src.state.button; break;
        case WT_RadioButton: state.radio = src.state.radio; break;
        case WT_ToggleButton: state.toggle = src.state.toggle; break;
        case WT_Checkbox: state.checkbox = src.state.checkbox; break;
        case WT_Spinner: state.spinner = src.state.spinner; break;
        case WT_Slider: state.slider = src.state.slider; break;
        case WT_RangeSlider: state.rangeSlider = src.state.rangeSlider; break;
        case WT_TextInput: state.input = src.state.input; break;
        case WT_DropDown: state.dropdown = src.state.dropdown; break;
        case WT_SplitterRegion: [[fallthrough]];
        case WT_Scrollable: state.scroll = src.state.scroll; break;
        case WT_TabBar: state.tab = src.state.tab; break;
        case WT_ItemGrid: state.grid = src.state.grid; break;
        case WT_MediaResource: state.media = src.state.media; break;
        default: break;
        }
    }

    WidgetConfigData& WidgetConfigData::operator=(const WidgetConfigData& src)
    {
        type = src.type;
        switch (src.type)
        {
        case WT_Label: state.label = src.state.label; break;
        case WT_Region: state.region = src.state.region; break;
        case WT_Button: state.button = src.state.button; break;
        case WT_RadioButton: state.radio = src.state.radio; break;
        case WT_ToggleButton: state.toggle = src.state.toggle; break;
        case WT_Checkbox: state.checkbox = src.state.checkbox; break;
        case WT_Spinner: state.spinner = src.state.spinner; break;
        case WT_Slider: state.slider = src.state.slider; break;
        case WT_RangeSlider: state.rangeSlider = src.state.rangeSlider; break;
        case WT_TextInput: state.input = src.state.input; break;
        case WT_DropDown: state.dropdown = src.state.dropdown; break;
        case WT_SplitterRegion: [[fallthrough]];
        case WT_Scrollable: state.scroll = src.state.scroll; break;
        case WT_TabBar: state.tab = src.state.tab; break;
        case WT_ItemGrid: state.grid = src.state.grid; break;
        case WT_MediaResource: state.media = src.state.media; break;
        default: break;
        }
        return *this;
    }

    WidgetConfigData::~WidgetConfigData()
    {
        switch (type)
        {
        case WT_Region: state.region.~RegionState(); break;
        case WT_Label: state.label.~LabelState(); break;
        case WT_Button: state.button.~ButtonState(); break;
        case WT_RadioButton: state.radio.~RadioButtonState(); break;
        case WT_ToggleButton: state.toggle.~ToggleButtonState(); break;
        case WT_Checkbox: state.checkbox.~CheckboxState(); break;
        case WT_Spinner: state.spinner.~SpinnerState(); break;
        case WT_Slider: state.slider.~SliderState(); break;
        case WT_RangeSlider: state.rangeSlider.~RangeSliderState(); break;
        case WT_TextInput: state.input.~TextInputState(); break;
        case WT_DropDown: state.dropdown.~DropDownState(); break;
        case WT_Scrollable: state.scroll.~ScrollableRegion(); break;
        case WT_TabBar: state.tab.~TabBarState(); break;
        case WT_ItemGrid: state.grid.~ItemGridConfig(); break;
        case WT_MediaResource: state.media.~MediaState(); break;
        default: break;
        }
    }

    WidgetConfigData& CreateWidgetConfig(WidgetType type, int16_t id)
    {
        auto& context = GetContext();
        int32_t wid = id;
        wid = wid | (type << WidgetTypeBits);

        if (context.InsideFrame)
            context.tempids[type] = std::min(context.tempids[type], context.maxids[type]);

        auto& state = context.GetState(wid);
        switch (type)
        {
        case WT_Region: state.state.region.id = wid; break;
        case WT_Label: state.state.label.id = wid; break;
        case WT_Button: state.state.button.id = wid; break;
        case WT_RadioButton: state.state.radio.id = wid; break;
        case WT_ToggleButton: state.state.toggle.id = wid; break;
        case WT_Checkbox: state.state.checkbox.id = wid; break;
        case WT_Spinner: state.state.spinner.id = wid; break;
        case WT_Slider: state.state.slider.id = wid; break;
        case WT_RangeSlider: state.state.rangeSlider.id = wid; break;
        case WT_TextInput: state.state.input.id = wid; break;
        case WT_DropDown: state.state.dropdown.id = wid; break;
        case WT_ItemGrid: state.state.grid.id = wid; break;
        default: break;
        }
        return state;
    }

    WidgetConfigData& CreateWidgetConfig(int32_t id)
    {
        auto wtype = (WidgetType)(id >> WidgetTypeBits);
        return CreateWidgetConfig(wtype, (int16_t)(id & WidgetIndexMask));
    }

#pragma endregion

#pragma region Utilities

    static TextType ToTextTypeEnum(int32_t flags)
    {
        return flags & TextIsRichText ? TextType::RichText :
            flags & TextIsSVG ? TextType::SVG : TextType::PlainText;
    }

    static bool IsBetween(float point, float min, float max, float tolerance = 0.f)
    {
        return (point < (max + tolerance)) && (point > (min - tolerance));
    }

    static ImVec2 GetTextSize(TextType type, std::string_view text, const FontStyle& font, float width, IRenderer& renderer)
    {
        switch (type)
        {
#ifndef GLIMMER_DISABLE_RICHTEXT
        case glimmer::TextType::RichText: 
        {
            auto id = ImRichText::CreateRichText(text.data(), text.data() + text.size());
            return ImRichText::GetBounds(id, ImVec2{ width == -1.f ? FLT_MAX : width, FLT_MAX });
        }
#else
        case glimmer::TextType::RichText: [[fallthrough]];
#endif
        case glimmer::TextType::PlainText: return renderer.GetTextSize(text, font.font, font.size, width);
        case glimmer::TextType::SVG: return ImVec2{ font.size, font.size };
        default: break;
        }

        return ImVec2{};
    }

    template <typename StringT>
    static void CopyToClipboard(StringT& string, int start, int end)
    {
        static char buffer[256] = { 0 };
        auto dest = 0;
        auto sz = end - start + 2;
        IPlatform& platform = *Config.platform;

        if (sz > 254)
        {
            char* hbuffer = (char*)std::malloc(sz);
            for (auto idx = start; idx <= end; ++idx, ++dest)
                hbuffer[dest] = string[idx];
            hbuffer[dest] = 0;
            platform.SetClipboardText(hbuffer);
            std::free(hbuffer);
        }
        else
        {
            memset(buffer, 0, 256);
            for (auto idx = start; idx <= end; ++idx, ++dest)
                buffer[dest] = string[idx];
            buffer[dest] = 0;
            platform.SetClipboardText(buffer);
        }
    }

    static void AddExtent(LayoutItemDescriptor& layoutItem, const NeighborWidgets& neighbors)
    {
        auto& context = GetContext();
        auto sz = context.MaximumExtent();
        auto nextpos = context.NextAdHocPos();
        layoutItem.margin.Min = nextpos;
        layoutItem.border.Min = layoutItem.margin.Min;
        layoutItem.padding.Min = layoutItem.border.Min;
        layoutItem.content.Min = layoutItem.padding.Min;

        if (neighbors.right != -1) layoutItem.margin.Max.x = context.GetGeometry(neighbors.right).Min.x;
        else layoutItem.margin.Max.x = sz.x;

        layoutItem.border.Max.x = layoutItem.margin.Max.x;
        layoutItem.padding.Max.x = layoutItem.border.Max.x;
        layoutItem.content.Max.x = layoutItem.padding.Max.x;

        if (neighbors.bottom != -1) layoutItem.margin.Max.y = context.GetGeometry(neighbors.bottom).Min.y;
        else layoutItem.margin.Max.y = sz.y;

        layoutItem.border.Max.y = layoutItem.margin.Max.y;
        layoutItem.padding.Max.y = layoutItem.border.Max.y;
        layoutItem.content.Max.y = layoutItem.padding.Max.y;
    }

    static int32_t ToTextFlags(int32_t resflags)
    {
        int32_t res = 0;
        if (resflags & RT_SVG)
            res = (resflags & RT_PATH) ? TextIsSVGFile : TextIsSVG;
        if ((resflags & RT_PNG) || (resflags & RT_JPG) || (resflags & RT_BMP) || (resflags & RT_PSD) ||
            (resflags & RT_GENERIC_IMG))
            res = TextIsImgPath;
        return res;
    }

    static void DrawFocusRect(int32_t state, ImVec2 start, ImVec2 end, IRenderer& renderer)
    {
        if (state & WS_Focused)
        {
            renderer.DrawRect(start, end, Config.focuscolor, false, 1.f);
        }
    }

    static void DrawStyledShape(IRenderer& renderer, const StyleDescriptor& style, uint32_t color, ImVec2 center, float radius)
    {
        if (style.border.isRounded())
            if ((style.border.cornerRadius[TopLeftCorner] >= radius) && (style.border.cornerRadius[TopRightCorner] >= radius) &&
                (style.border.cornerRadius[BottomRightCorner] >= radius) && (style.border.cornerRadius[BottomLeftCorner] >= radius))
                renderer.DrawCircle(center, radius, color, true);
            else
                renderer.DrawRoundedRect(center - ImVec2{ radius, radius }, center + ImVec2{ radius, radius }, color, true,
                    style.border.cornerRadius[TopLeftCorner], style.border.cornerRadius[TopRightCorner], style.border.cornerRadius[BottomRightCorner],
                    style.border.cornerRadius[BottomLeftCorner]);
        else
            renderer.DrawRect(center - ImVec2{ radius, radius }, center + ImVec2{ radius, radius }, color, true);
    }

    static void UpdateMaxAvailableExtent(WidgetContextData& context, ImVec2& maxxy, int32_t id)
    {
        if (!context.nestedContextStack.empty())
        {
            auto nestedSrc = context.GetParentWidget();

            if (nestedSrc.has_value() && nestedSrc.value().source == NestedContextSourceType::ItemGrid)
            {
                if (WidgetContextData::CurrentItemGridContext != nullptr)
                {
                    auto& grid = WidgetContextData::CurrentItemGridContext->itemGrids.top();
                    auto& extent = grid.headers[grid.currlevel][grid.currCol].content;
                    maxxy.x = extent.Max.x; // Bounded in x direction, y is from content
                }
            }
            else if (nestedSrc.has_value() && nestedSrc.value().source == NestedContextSourceType::DropDownPopup)
            {
                auto ddid = nestedSrc.value().id;
                auto& persistent = nestedSrc.value().base->DropDownState(ddid);
                auto index = (int16_t)(id >> WidgetTypeBits);
                persistent.widgets[index].emplace_back(
                    (int16_t)(id & WidgetIndexMask),
                    (int16_t)(persistent.items.size() - 1)
                );

                const auto& config = nestedSrc.value().base->GetState(ddid).state.dropdown;
                if (config.sizePolicy & (DD_SetPopupWidthToInitial | DD_FitToInitialContent))
                    maxxy.x = persistent.maxsz.x + persistent.extra.x + persistent.indicator.x;
            }
            else if (nestedSrc.has_value() && nestedSrc.value().source == NestedContextSourceType::Custom)
            {
				auto pid = nestedSrc.value().id;
                if (Config.CustomWidgetProvider)
                {
                    auto wtype = (int16_t)(pid >> WidgetTypeBits);
                    auto wfactory = Config.CustomWidgetProvider(wtype);
                    if (wfactory)
                        wfactory->UpdateAvailableChildExtent(maxxy, id);
                }
            }
        }
    }

    void UpdateParentWidgetGeometry(const LayoutItemDescriptor& layoutItem, const StyleDescriptor& style)
    {
        auto& context = GetContext();

        if (!context.nestedContextStack.empty())
        {
            auto nestedSrc = context.GetParentWidget();

            if (nestedSrc.has_value() && nestedSrc.value().source == NestedContextSourceType::ItemGrid)
            {
                auto& grid = nestedSrc.value().base->itemGrids.top(); // context.CurrentItemGridContext->itemGrids.top();
                if (grid.phase == ItemGridConstructPhase::HeaderCells)
                {
                    grid.maxHeaderExtent.x = std::max(grid.maxHeaderExtent.x, layoutItem.margin.Max.x);
                    grid.maxHeaderExtent.y = std::max(grid.maxHeaderExtent.y, layoutItem.margin.Max.y);
                }
                else
                {
                    if (grid.maxCellExtent != ImVec2{})
                    {
                        grid.maxCellExtent.x = std::max(grid.maxCellExtent.x, layoutItem.margin.Max.x);
                        grid.maxCellExtent.y = std::max(grid.maxCellExtent.y, layoutItem.margin.Max.y);
                    }
                    else
                        grid.maxCellExtent = layoutItem.margin.Max;
                }
            }
            else if (nestedSrc.has_value() && nestedSrc.value().source == NestedContextSourceType::DropDownPopup)
            {
                auto ddid = nestedSrc.value().id;
                auto& persistent = nestedSrc.value().base->DropDownState(ddid);
                auto index = (int16_t)(layoutItem.id >> WidgetTypeBits);

                for (auto id : persistent.widgets[index])
                    if (id.first == (int16_t)(layoutItem.id & WidgetIndexMask))
                    {
                        auto& item = persistent.items[id.second];
                        item.geometry.Min = ImMin(layoutItem.margin.Min, item.geometry.Min);
                        item.geometry.Max = ImMax(layoutItem.margin.Max, item.geometry.Max);
                        break;
                    }
            }
            else if (nestedSrc.has_value() && nestedSrc.value().source == NestedContextSourceType::Custom)
            {
                if (Config.CustomWidgetProvider)
                {
                    auto wtype = (int16_t)(nestedSrc.value().id >> WidgetTypeBits);
                    auto wfactory = Config.CustomWidgetProvider(wtype);

                    if (wfactory)
                        wfactory->RecordChildWidgetGeometry(layoutItem);
                }
            }
        }
    }

#pragma endregion

#pragma region Tooltip

    static int32_t PreviousWidget = -1;
    static std::string_view NextTootip = "";

    static void UpdateTooltip(std::string_view& tooltip)
    {
        if (!NextTootip.empty())
        {
            tooltip = NextTootip;
            NextTootip = "";
        }
    }

    void ShowTooltip(float& hoverDuration, const ImRect& area, std::string_view tooltip, const IODescriptor& io)
    {
        if (area.Contains(io.mousepos) && !tooltip.empty() && !io.isMouseDown())
        {
            hoverDuration += io.deltaTime;

            if (hoverDuration >= Config.tooltipDelay)
            {
                auto font = GetFont(Config.tooltipFontFamily, Config.tooltipFontSz, FT_Normal);
                auto textsz = Config.renderer->GetTextSize(tooltip, font, Config.tooltipFontSz);

                ImVec2 tooltippos;
                auto halfw = ((area.GetWidth() - textsz.x) * 0.5f);
                auto startx = io.mousepos.x - halfw, endx = io.mousepos.x + halfw;
                auto hdiff1 = std::min(startx, 0.f), hdiff2 = std::min(0.f, GetContext().WindowSize().x - endx);
                tooltippos.x = io.mousepos.x - halfw - hdiff1 + hdiff2;

                tooltippos.y = io.mousepos.y - (textsz.y + 2.f);
                if (tooltippos.y < 0.f) tooltippos.y = io.mousepos.y + 2.f;
                Config.renderer->DrawTooltip(tooltippos, tooltip);
            }
        }
        else hoverDuration = 0;
    }

    void SetTooltip(int32_t id, std::string_view tooltip)
    {
        auto wtype = (WidgetType)(id >> WidgetTypeBits);
        auto& state = CreateWidgetConfig(id).state;

        switch (wtype)
        {
        case glimmer::WT_Label:
            state.label.tooltip = tooltip;
            break;
        case glimmer::WT_Button:
            state.button.tooltip = tooltip;
            break;
        case glimmer::WT_RadioButton:
            state.radio.tooltip = tooltip;
            break;
        case glimmer::WT_ToggleButton:
            state.toggle.tooltip = tooltip;
            break;
        case glimmer::WT_Checkbox:
            state.checkbox.tooltip = tooltip;
            break;
        case glimmer::WT_Slider:
            state.slider.tooltip = tooltip;
            break;
        case glimmer::WT_RangeSlider:
            state.rangeSlider.tooltip = tooltip;
            break;
        case glimmer::WT_Spinner:
            state.spinner.tooltip = tooltip;
            break;
        case glimmer::WT_TextInput:
            state.input.tooltip = tooltip;
            break;
        case glimmer::WT_DropDown:
            state.dropdown.tooltip = tooltip;
            break;
        default:
            break;
        }
    }

    void SetPrevTooltip(std::string_view tooltip)
    {
        if (PreviousWidget != -1)
            SetTooltip(PreviousWidget, tooltip);
    }

    void SetNextTooltip(std::string_view tooltip)
    {
        NextTootip = tooltip;
    }

#pragma endregion

#pragma region Scrollbars

    static bool HandleHScroll(ScrollableRegion& region, IRenderer& renderer, const IODescriptor& io, float btnsz, bool showButtons = true, 
        std::optional<std::pair<float, float>> xbounds = std::nullopt)
    {
        auto hasHScroll = false;
        float gripExtent = 0.f;
        auto viewport = region.viewport;
        const auto mousepos = io.mousepos;
        const float vwidth = xbounds.has_value() ? xbounds.value().second - xbounds.value().first : viewport.GetWidth();
        const auto width = region.content.x;
        const float posrange = width - vwidth;
        auto& scroll = region.state;

        if (width > vwidth)
        {
            auto hasMouseInteraction = (region.type & ST_Always_H);
            auto hasOpacity = scroll.opacity.x > 0.f;
            scroll.progress.x = scroll.opacity.x >= 255.f ? 0.f : scroll.progress.x;

            if (!hasMouseInteraction)
            {
                auto checkForHover = (mousepos.y <= viewport.Max.y) && mousepos.y >= (viewport.Max.y - (1.5f * btnsz));
                auto isActive = xbounds.has_value() ? ImRect{ { xbounds.value().first, viewport.Min.y }, { xbounds.value().second, viewport.Max.y } }.Contains(mousepos) :
                    viewport.Contains(mousepos);
                hasMouseInteraction = (isActive && checkForHover) || scroll.mouseDownOnHGrip;
            }

            if (hasMouseInteraction || hasOpacity)
            {
                if (!(region.type & ST_Always_H))
                {
                    if (hasMouseInteraction && scroll.opacity.x < 255.f)
                        scroll.opacity.x = 255.f * anim::EaseOut(scroll.progress.x, Config.scrollbar.animationDuration);
                    else if (!hasMouseInteraction && scroll.opacity.x > 0.f)
                        scroll.opacity.x = 255.f * (1.f - anim::EaseOut(scroll.progress.x, Config.scrollbar.animationDuration));
                }
                else
                    scroll.opacity.x = 255.f;

                auto lrsz = showButtons ? btnsz : 0.f;
                auto startx = xbounds.has_value() ? xbounds.value().first : viewport.Min.x,
                    endx = xbounds.has_value() ? xbounds.value().second : viewport.Max.x;
                ImRect left{ { startx, viewport.Max.y - lrsz }, { startx + lrsz, viewport.Max.y } };
                ImRect right{ { endx - lrsz, viewport.Max.y - lrsz }, { endx, viewport.Max.y } };
                ImRect path{ { left.Max.x, left.Min.y }, { right.Min.x, right.Max.y } };

                auto pathsz = path.GetWidth();
                auto sizeOfGrip = (vwidth / width) * pathsz;
                auto spos = ((pathsz - sizeOfGrip) / posrange) * scroll.pos.x;
                ImRect grip{ { left.Max.x + spos, viewport.Max.y - btnsz },
                    { left.Max.x + spos + std::max(sizeOfGrip, Config.scrollbar.minGripSz), viewport.Max.y } };

                if (showButtons)
                {
                    auto leftst = left.Contains(io.mousepos) && io.isLeftMouseDown() ?
                        WSI_Pressed : left.Contains(io.mousepos) ? WSI_Hovered : WSI_Default;
                    renderer.DrawRect(left.Min, left.Max, SetAlpha(Config.scrollbar.colors[leftst].buttonbg, (int)scroll.opacity.x), true);
                    renderer.DrawTriangle({ left.Min.x + (btnsz * 0.25f), left.Min.y + (0.5f * btnsz) },
                        { left.Max.x - (0.125f * btnsz), left.Min.y + (0.125f * btnsz) },
                        { left.Max.x - (0.125f * btnsz), left.Max.y - (0.125f * btnsz) }, 
                        SetAlpha(Config.scrollbar.colors[leftst].buttonfg, (int)scroll.opacity.x), true);

                    auto rightst = right.Contains(io.mousepos) && io.isLeftMouseDown() ?
                        WSI_Pressed : right.Contains(io.mousepos) ? WSI_Hovered : WSI_Default;
                    renderer.DrawRect(right.Min, right.Max, SetAlpha(Config.scrollbar.colors[rightst].buttonbg, (int)scroll.opacity.x), true);
                    renderer.DrawTriangle({ right.Min.x + (btnsz * 0.25f), right.Min.y + (0.125f * btnsz) },
                        { right.Max.x - (0.125f * btnsz), right.Min.y + (0.5f * btnsz) },
                        { right.Min.x + (btnsz * 0.25f), right.Max.y - (0.125f * btnsz) }, 
                        SetAlpha(Config.scrollbar.colors[rightst].buttonfg, (int)scroll.opacity.x), true);
                }

                if (region.type & ST_Always_H)
                {
                    auto state = path.Contains(io.mousepos) && io.isLeftMouseDown() ?
                        WSI_Pressed : path.Contains(io.mousepos) ? WSI_Hovered : WSI_Default;
                    renderer.DrawRect(path.Min, path.Max, Config.scrollbar.colors[state].track, true);
                }

                if (grip.Contains(mousepos))
                {
                    Config.platform->SetMouseCursor(MouseCursor::Grab);

                    if (io.isLeftMouseDown())
                    {
                        if (!scroll.mouseDownOnHGrip)
                        {
                            scroll.mouseDownOnHGrip = true;
                            scroll.lastMousePos.x = mousepos.x;
                        }

                        auto step = mousepos.x - scroll.lastMousePos.x;
                        if (step != 0.f)
                        {
                            step = (posrange / (pathsz - sizeOfGrip)) * step;
                            scroll.pos.x = ImClamp(scroll.pos.x + step, 0.f, posrange);
                            scroll.lastMousePos.x = mousepos.x;
                        }

                        renderer.DrawRect(grip.Min, grip.Max, Config.scrollbar.colors[WSI_Pressed].grip, true);
                    }
                    else
                        renderer.DrawRect(grip.Min, grip.Max, SetAlpha(Config.scrollbar.colors[WSI_Hovered].grip, (int)scroll.opacity.x), true);
                }
                else
                {
                    if (scroll.mouseDownOnHGrip)
                    {
                        auto step = mousepos.x - scroll.lastMousePos.x;
                        if (step != 0.f)
                        {
                            step = (posrange / (pathsz - sizeOfGrip)) * step;
                            scroll.pos.x = ImClamp(scroll.pos.x + step, 0.f, posrange);
                            scroll.lastMousePos.x = mousepos.x;
                        }

                        renderer.DrawRect(grip.Min, grip.Max, Config.scrollbar.colors[WSI_Pressed].grip, true);
                    }
                    else
                        renderer.DrawRect(grip.Min, grip.Max, SetAlpha(Config.scrollbar.colors[WSI_Default].grip, (int)scroll.opacity.x), true);

                    if (left.Contains(mousepos))
                    {
                        Config.platform->SetMouseCursor(MouseCursor::Grab);

                        if (io.isLeftMouseDown())
                            scroll.pos.x = ImClamp(scroll.pos.x - 1.f, 0.f, posrange);
                    }
                    else if (right.Contains(mousepos))
                    {
                        Config.platform->SetMouseCursor(MouseCursor::Grab);

                        if (io.isLeftMouseDown())
                            scroll.pos.x = ImClamp(scroll.pos.x + 1.f, 0.f, posrange);
                    }
                }

                if (!io.isLeftMouseDown() && scroll.mouseDownOnHGrip)
                {
                    scroll.mouseDownOnHGrip = false;
                    renderer.DrawRect(grip.Min, grip.Max, Config.scrollbar.colors[WSI_Default].grip, true);
                }

                WITH_WIDGET_LOG(WT_Scrollable, -1, ImRect{ left.Min, right.Max });
                LOG_RECT(path); LOG_RECT(grip); LOG_RECT(viewport);
            }

            hasHScroll = true;
        }

        return hasHScroll;
    }

    // TODO: Add showbuttons params
    static bool HandleVScroll(ScrollableRegion& region, IRenderer& renderer, const IODescriptor& io, float btnsz, 
        bool hasHScroll = false, std::optional<std::pair<float, float>> ybounds = std::nullopt)
    {
        auto viewport = region.viewport;
        const auto mousepos = io.mousepos;
        const auto vheight = ybounds.has_value() ? ybounds.value().second - ybounds.value().first : viewport.GetHeight();
        const auto height = region.content.y;
        const float posrange = height - vheight;
        auto& scroll = region.state;

        if (height > vheight)
        {
            auto hasOpacity = scroll.opacity.y > 0.f;
            auto hasMouseInteraction = (region.type & ST_Always_V);
            scroll.progress.y = scroll.opacity.y >= 255.f ? 0.f : scroll.progress.y;

            if (!hasMouseInteraction)
            {
                auto checkForHover = (mousepos.x <= viewport.Max.x) && mousepos.x >= (viewport.Max.x - (1.5f * btnsz)) &&
                    (!hasHScroll || mousepos.y < (viewport.Max.y - btnsz));
                auto isActive = ybounds.has_value() ? ImRect{ { viewport.Min.x, ybounds.value().first }, { viewport.Max.x, ybounds.value().second } }.Contains(mousepos) :
                    viewport.Contains(mousepos);
                hasMouseInteraction = (isActive && checkForHover) || scroll.mouseDownOnVGrip;
            }

            if (hasMouseInteraction || hasOpacity)
            {
                if (!(region.type & ST_Always_V))
                {
                    if (hasMouseInteraction && scroll.opacity.y < 255.f)
                        scroll.opacity.y = 255.f * anim::EaseOut(scroll.progress.y, Config.scrollbar.animationDuration);
                    else if (!hasMouseInteraction && scroll.opacity.y > 0.f)
                        scroll.opacity.y = 255.f * (1.f - anim::EaseOut(scroll.progress.y, Config.scrollbar.animationDuration));
                }
                else
                    scroll.opacity.y = 255.f;
                
                // TODO: Is this required?
                auto extrah = hasHScroll && !(region.type & ST_Always_H) ? btnsz : 0.f;
                auto starty = ybounds.has_value() ? ybounds.value().first : viewport.Min.y,
                    endy = ybounds.has_value() ? ybounds.value().second : viewport.Max.y;
                ImRect top{ { viewport.Max.x - btnsz, starty }, { viewport.Max.x, starty + btnsz } };
                ImRect bottom{ { viewport.Max.x - btnsz, endy - btnsz - extrah }, { viewport.Max.x, endy - extrah } };
                ImRect path{ { top.Min.x, top.Max.y }, { bottom.Max.x, bottom.Min.y } };

                auto pathsz = path.GetHeight();
                auto sizeOfGrip = (vheight / height) * pathsz;
                auto spos = ((pathsz - sizeOfGrip) / posrange) * scroll.pos.y;
                ImRect grip{ { viewport.Max.x - btnsz, top.Max.y + spos },
                    { viewport.Max.x, std::max(sizeOfGrip, Config.scrollbar.minGripSz) + top.Max.y + spos } };

                auto topst = top.Contains(io.mousepos) && io.isLeftMouseDown() ?
                    WSI_Pressed : top.Contains(io.mousepos) ? WSI_Hovered : WSI_Default;
                renderer.DrawRect(top.Min, top.Max, SetAlpha(Config.scrollbar.colors[topst].buttonbg, (int)scroll.opacity.y), true);
                renderer.DrawTriangle({ top.Min.x + (btnsz * 0.5f), top.Min.y + (0.25f * btnsz) },
                    { top.Max.x - (0.125f * btnsz), top.Min.y + (0.75f * btnsz) },
                    { top.Min.x + (0.125f * btnsz), top.Min.y + (0.75f * btnsz) }, 
                    SetAlpha(Config.scrollbar.colors[topst].buttonfg, (int)scroll.opacity.y), true);

                auto bottomst = bottom.Contains(io.mousepos) && io.isLeftMouseDown() ?
                    WSI_Pressed : bottom.Contains(io.mousepos) ? WSI_Hovered : WSI_Default;
                renderer.DrawRect(bottom.Min, bottom.Max, SetAlpha(Config.scrollbar.colors[bottomst].buttonbg, (int)scroll.opacity.y), true);
                renderer.DrawTriangle({ bottom.Min.x + (btnsz * 0.125f), bottom.Min.y + (0.25f * btnsz) },
                    { bottom.Max.x - (0.125f * btnsz), bottom.Min.y + (0.25f * btnsz) },
                    { bottom.Max.x - (0.5f * btnsz), bottom.Max.y - (0.25f * btnsz) }, 
                    SetAlpha(Config.scrollbar.colors[bottomst].buttonfg, (int)scroll.opacity.y), true);

                if (region.type & ST_Always_V)
                {
                    auto state = path.Contains(io.mousepos) && io.isLeftMouseDown() ?
                        WSI_Pressed : path.Contains(io.mousepos) ? WSI_Hovered : WSI_Default;
                    renderer.DrawRect(path.Min, path.Max, Config.scrollbar.colors[state].track, true);
                }

                if (grip.Contains(mousepos))
                {
                    Config.platform->SetMouseCursor(MouseCursor::Grab);

                    if (io.isLeftMouseDown())
                    {
                        if (!scroll.mouseDownOnVGrip)
                        {
                            scroll.mouseDownOnVGrip = true;
                            scroll.lastMousePos.y = mousepos.y;
                        }

                        auto movey = mousepos.y - scroll.lastMousePos.y;
                        if (movey != 0.f)
                        {
                            auto step = (posrange / (pathsz - sizeOfGrip)) * movey;
                            scroll.pos.y = ImClamp(scroll.pos.y + step, 0.f, posrange);
                            scroll.lastMousePos.y = mousepos.y;
                            LOG("Moved by %.2f to %.2f | Scroll position: %.2f [delta: %2.2f] [Max: %.2f] [Movement Range: %.2f]\n", movey, scroll.lastMousePos.y,
                                scroll.pos.y, step, posrange, pathsz - sizeOfGrip);
                        }

                        renderer.DrawRect(grip.Min, grip.Max, Config.scrollbar.colors[WSI_Pressed].grip, true);
                    }
                    else
                        renderer.DrawRect(grip.Min, grip.Max, SetAlpha(Config.scrollbar.colors[WSI_Hovered].grip, (int)scroll.opacity.y), true);
                }
                else
                {
                    if (scroll.mouseDownOnVGrip)
                    {
                        auto movey = mousepos.y - scroll.lastMousePos.y;
                        if (movey != 0.f)
                        {
                            auto step = (posrange / (pathsz - sizeOfGrip)) * movey;
                            scroll.pos.y = ImClamp(scroll.pos.y + step, 0.f, posrange);
                            scroll.lastMousePos.y = mousepos.y;
                            LOG("Moved by %f to %f | Scroll position: %f [delta: %f] [Max: %f] [Movement Range: %f]\n", movey, scroll.lastMousePos.y, 
                                scroll.pos.y, step, posrange, pathsz - sizeOfGrip);
                        }

                        renderer.DrawRect(grip.Min, grip.Max, Config.scrollbar.colors[WSI_Pressed].grip, true);
                    }
                    else
                        renderer.DrawRect(grip.Min, grip.Max, SetAlpha(Config.scrollbar.colors[WSI_Default].grip, (int)scroll.opacity.y), true);

                    if (top.Contains(mousepos))
                    {
                        Config.platform->SetMouseCursor(MouseCursor::Grab);

                        if (io.isLeftMouseDown())
                            scroll.pos.y = ImClamp(scroll.pos.y - 1.f, 0.f, posrange);
                    }
                    else if (bottom.Contains(mousepos))
                    {
                        Config.platform->SetMouseCursor(MouseCursor::Grab);

                        if (io.isLeftMouseDown())
                            scroll.pos.y = ImClamp(scroll.pos.y + 1.f, 0.f, posrange);
                    }
                }

                if (!io.isLeftMouseDown() && scroll.mouseDownOnVGrip)
                {
                    scroll.mouseDownOnVGrip = false;
                    renderer.DrawRect(grip.Min, grip.Max, Config.scrollbar.colors[WSI_Default].grip, true);
                }

                WITH_WIDGET_LOG(WT_Scrollable, -1, ImRect{ top.Min, bottom.Max });
                LOG_RECT(path); LOG_RECT(grip); LOG_RECT(viewport);
            }

            if (viewport.Contains(mousepos) && !(region.type & ST_NoMouseWheel_V))
            {
                auto rotation = io.mouseWheel;
                scroll.pos.y = ImClamp(rotation + scroll.pos.y, 0.f, posrange);
            }

            return true;
        }

        return false;
    }

    void StartScrollableImpl(int32_t id, int32_t scrollType, ImVec2 maxsz, const StyleDescriptor& style,
        const ImRect& border, const ImRect& content, IRenderer& renderer)
    {
        DrawBorderRect(border.Min, border.Max, style.border, style.bgcolor, renderer);

        auto& context = GetContext();
        auto& region = context.ScrollRegion(id);
        region.viewport = content;
        region.type |= scrollType;
        region.extent = maxsz;
        region.content = region.viewport.Min;
        renderer.SetClipRect(content.Min, content.Max);
        context.AddItemGeometry(id, region.viewport);
        context.containerStack.push() = id;
        if (context.layoutStack.empty())
            context.adhocLayout.top().insideContainer = true;
    }

    void BeginScrollableRegion(int32_t id, int32_t scrollType, int32_t geometry,
        const NeighborWidgets& neighbors, ImVec2 maxsz)
    {
        auto& context = GetContext();

        if (!context.layoutStack.empty())
        {
            const auto style = context.GetStyle(WS_Default, id);
            auto& layout = context.layouts[context.layoutStack.top()];
            LayoutItemDescriptor layoutItem;
            layoutItem.wtype = WT_Scrollable;
            layoutItem.id = id;
            layoutItem.extent = maxsz;
            AddExtent(layoutItem, style, neighbors);
            AddItemToLayout(layout, layoutItem, style);
            layout.containerStack.push() = id;
        }
        else
        {
            auto& region = context.ScrollRegion(id);
            const auto style = context.GetStyle(WS_Default, id);

            auto& renderer = context.GetRenderer();
            LayoutItemDescriptor layoutItem;
            layoutItem.wtype = WT_Scrollable;
            layoutItem.id = id;
            AddExtent(layoutItem, style, neighbors);
            StartScrollableImpl(id, scrollType, maxsz, style, layoutItem.border, layoutItem.content, renderer);
        }
    }

    void BeginScrollableRegion(std::string_view id, int32_t flags, int32_t geometry, const NeighborWidgets& neighbors, ImVec2 maxsz)
    {
        auto [iid, __] = GetIdFromString(id, WT_Scrollable);
        BeginScrollableRegion(iid, flags, geometry, neighbors);
    }

    ImRect EndScrollableImpl(int32_t id, IRenderer& renderer)
    {
        auto& context = GetContext();
        auto& region = context.ScrollRegion(id);
        renderer.ResetClipRect();

        auto hasHScroll = false;
        auto io = Config.platform->CurrentIO();
        auto mousepos = io.mousepos;
        auto width = region.content.x + region.state.pos.x;
        auto height = region.content.y + region.state.pos.y;

        if (region.viewport.Max.x < width && (region.type & ST_Horizontal))
        {
            // Convert absolute max coordinates to size
            hasHScroll = true;
            region.content.x += -region.viewport.Min.x + region.state.pos.x;
            HandleHScroll(region, renderer, io, Config.scrollbar.width);
        }

        if (region.viewport.Max.y < height && (region.type & ST_Vertical))
        {
            // Convert absolute max coordinates to size
            region.content.y += -region.viewport.Min.y + region.state.pos.y;
            HandleVScroll(region, renderer, io, Config.scrollbar.width, hasHScroll);
        }

        context.containerStack.pop(1, true);
        context.AddItemGeometry(id, region.viewport);
        if (context.layoutStack.empty())
        {
            context.adhocLayout.top().insideContainer = false;
            context.adhocLayout.top().addedOffset = false;
        }

        auto res = region.viewport;
        if (region.type & ST_Always_H) res.Max.y -= Config.scrollbar.width;
        if (region.type & ST_Always_V) res.Max.x -= Config.scrollbar.width;
        return res;
    }

    ImRect EndScrollableRegion()
    {
        auto& context = GetContext();

        if (!context.layoutStack.empty())
        {
            auto& layout = context.layouts[context.layoutStack.top()];
            layout.containerStack.pop(1, true);
            EndLayout();
            return ImRect{};
        }
        else
        {
            auto id = context.containerStack.top();
            auto& renderer = context.GetRenderer();
            return EndScrollableImpl(id, renderer);
        }
    }

#pragma endregion

#pragma region Box Model Computation

    /* Here is the box model that is followed here:

           +--------------------------------+
           |            margin              |
           |   +------------------------+   |
           |   |       border           |   |
           |   |   +--------------+     |   |
           |   |   |   padding    |     |   |
           |   |   |  +--------+  |     |   |
           |   |   |  |        |  |     |   |
           |   |   |  |content |  |     |   |
           |   |   |  |        |  |     |   |
           |   |   |  +--------+  |     |   |
           |   |   |              |     |   |
           |   |   +--------------+     |   |
           |   |                        |   |
           |   +------------------------+   |
           |                                |
           +--------------------------------+

   */

    static std::tuple<ImRect, ImRect, ImRect, ImRect, ImRect> GetBoxModelBounds(ImVec2 pos, const StyleDescriptor& style,
        std::string_view text, IRenderer& renderer, int32_t geometry, TextType type,
        const NeighborWidgets& neighbors, float width, float height)
    {
        ImRect content, padding, border, margin;
        const auto& borderstyle = style.border;
        const auto& font = style.font;
        auto& context = GetContext();
        margin.Min = pos;

        if (geometry & ToLeft)
        {
            border.Min.x = pos.x - style.margin.right;
            padding.Min.x = border.Min.x - borderstyle.right.thickness;
            content.Min.x = padding.Min.x - style.padding.right;
        }
        else
        {
            border.Min.x = pos.x + style.margin.left;
            padding.Min.x = border.Min.x + borderstyle.left.thickness;
            content.Min.x = padding.Min.x + style.padding.left;
        }

        if (geometry & ToTop)
        {
            border.Min.y = pos.y - style.margin.bottom;
            padding.Min.y = border.Min.y - borderstyle.bottom.thickness;
            content.Min.y = padding.Min.y - style.padding.bottom;
        }
        else
        {
            border.Min.y = pos.y + style.margin.top;
            padding.Min.y = border.Min.y + borderstyle.top.thickness;
            content.Min.y = padding.Min.y + style.padding.top;
        }

        auto hastextw = (style.dimension.x > 0.f) && !((style.font.flags & FontStyleOverflowEllipsis) ||
            (style.font.flags & FontStyleOverflowMarquee));
        ImVec2 textsz{ 0.f, 0.f };
        auto textMetricsComputed = false, hexpanded = false, vexpanded = false;

        auto setHFromContent = [&] {
            if (geometry & ToLeft)
            {
                if (style.dimension.x > 0.f)
                {
                    margin.Max.x = margin.Min.x - clamp(style.dimension.x, style.mindim.x, style.maxdim.x);
                    border.Max.x = margin.Max.x + style.margin.left;
                    padding.Max.x = border.Max.x + borderstyle.left.thickness;
                    content.Max.x = padding.Max.x + style.padding.left;
                }
                else
                {
                    content.Max.x = content.Min.x - clamp(textsz.x, style.mindim.x, style.maxdim.x);
                    padding.Max.x = content.Max.x - style.padding.left;
                    border.Max.x = padding.Max.x - borderstyle.left.thickness;
                    margin.Max.x = border.Max.x - style.margin.left;
                }
            }
            else
            {
                if (style.dimension.x > 0.f)
                {
                    margin.Max.x = margin.Min.x + clamp(style.dimension.x, style.mindim.x, style.maxdim.x);
                    border.Max.x = margin.Max.x - style.margin.right;
                    padding.Max.x = border.Max.x - borderstyle.right.thickness;
                    content.Max.x = padding.Max.x - style.padding.right;
                }
                else
                {
                    content.Max.x = content.Min.x + clamp(textsz.x, style.mindim.x, style.maxdim.x);
                    padding.Max.x = content.Max.x + style.padding.right;
                    border.Max.x = padding.Max.x + borderstyle.right.thickness;
                    margin.Max.x = border.Max.x + style.margin.right;
                }
            }
            };

        auto setVFromContent = [&] {
            if (geometry & ToTop)
            {
                if (style.dimension.y > 0.f)
                {
                    margin.Max.y = margin.Min.y - clamp(style.dimension.y, style.mindim.y, style.maxdim.y);
                    border.Max.y = margin.Max.y + style.margin.top;
                    padding.Max.y = border.Max.y + borderstyle.top.thickness;
                    content.Max.y = padding.Max.y + style.padding.top;
                }
                else
                {
                    content.Max.y = content.Min.y - clamp(textsz.y, style.mindim.y, style.maxdim.y);
                    padding.Max.y = content.Max.y - style.padding.bottom;
                    border.Max.y = padding.Max.y - borderstyle.bottom.thickness;
                    margin.Max.y = border.Max.y - style.margin.bottom;
                }
            }
            else
            {
                if (style.dimension.y > 0.f)
                {
                    margin.Max.y = margin.Min.y + clamp(style.dimension.y, style.mindim.y, style.maxdim.y);
                    border.Max.y = margin.Max.y - style.margin.bottom;
                    padding.Max.y = border.Max.y - borderstyle.bottom.thickness;
                    content.Max.y = padding.Max.y - style.padding.bottom;
                }
                else
                {
                    content.Max.y = content.Min.y + clamp(textsz.y, style.mindim.y, style.maxdim.y);
                    padding.Max.y = content.Max.y + style.padding.bottom;
                    border.Max.y = padding.Max.y + borderstyle.bottom.thickness;
                    margin.Max.y = border.Max.y + style.margin.bottom;
                }
            }
            };

        auto setHFromExpansion = [&](float max) {
            if (geometry & ToLeft)
            {
                margin.Max.x = std::max(max, margin.Min.x - style.maxdim.x);
                border.Max.x = margin.Max.x + style.margin.left;
                padding.Max.x = border.Max.x + borderstyle.left.thickness;
                content.Max.x = padding.Max.x + style.padding.left;
            }
            else
            {
                margin.Max.x = std::min(max, margin.Min.x + style.maxdim.x);
                border.Max.x = margin.Max.x - style.margin.right;
                padding.Max.x = border.Max.x - borderstyle.right.thickness;
                content.Max.x = padding.Max.x - style.padding.right;
            }
            };

        auto setVFromExpansion = [&](float max) {
            if (geometry & ToTop)
            {
                margin.Max.y = std::max(max, margin.Min.y - style.maxdim.y);
                border.Max.y = margin.Max.y + style.margin.top;
                padding.Max.y = border.Max.y + borderstyle.top.thickness;
                content.Max.y = padding.Max.y + style.padding.top;
            }
            else
            {
                margin.Max.y = std::min(max, margin.Min.y + style.maxdim.y);
                border.Max.y = margin.Max.y - style.margin.bottom;
                padding.Max.y = border.Max.y - borderstyle.bottom.thickness;
                content.Max.y = padding.Max.y - style.padding.bottom;
            }
            };

        if ((geometry & ExpandH) == 0)
        {
            textMetricsComputed = true;
            textsz = GetTextSize(type, text, style.font, hastextw ? style.dimension.x : -1.f, renderer);
            setHFromContent();
        }
        else
        {
            // If we are inside a layout, and layout has expand policy, then it has a geometry
            // Use said geometry or set from content dimensions
            if (!context.layoutStack.empty())
            {
                const auto& layout = context.layouts[context.layoutStack.top()];
                auto isLayoutFit = (layout.fill & FD_Horizontal) == 0 && layout.type == Layout::Vertical && layout.vofmode != OverflowMode::Wrap;

                if (!isLayoutFit)
                {
                    setHFromExpansion((geometry & ToLeft) ? layout.prevpos.x : layout.nextpos.x);
                    hexpanded = true;
                }
                else
                {
                    textMetricsComputed = true;
                    textsz = GetTextSize(type, text, style.font, hastextw ? style.dimension.x : -1.f, renderer);
                    setHFromContent();
                }
            }
            else
            {
                // If widget has a expand size policy then expand upto neighbors or upto window size
                setHFromExpansion((geometry & ToLeft) ? neighbors.left == -1 ? 0.f : context.GetGeometry(neighbors.left).Max.x
                    : neighbors.right == -1 ? width : context.GetGeometry(neighbors.right).Min.x);
                hexpanded = true;
            }
        }

        if ((geometry & ExpandV) == 0)
        {
            // When settings height from content, if text metrics are not already computed,
            // computed them on the fixed width that was computed in previous step
            if (!textMetricsComputed)
            {
                textMetricsComputed = true;
                textsz = GetTextSize(type, text, style.font, hastextw ? style.dimension.x : -1.f, renderer);
            }

            setVFromContent();
        }
        else
        {
            // If we are inside a layout, and layout has expand policy, then it has a geometry
            // Use said geometry or set from content dimensions
            if (!context.layoutStack.empty())
            {
                const auto& layout = context.layouts[context.layoutStack.top()];
                auto isLayoutFit = (layout.fill & FD_Vertical) == 0 && layout.type == Layout::Horizontal && layout.hofmode != OverflowMode::Wrap;

                if (!isLayoutFit)
                {
                    setVFromExpansion((geometry & ToTop) ? layout.prevpos.y : layout.nextpos.y);
                    vexpanded = true;
                }
                else
                {
                    if (!textMetricsComputed)
                    {
                        textMetricsComputed = true;
                        textsz = GetTextSize(type, text, style.font, hastextw ? style.dimension.x : -1.f, renderer);
                    }

                    setVFromContent();
                }
            }
            else
            {
                // If widget has a expand size policy then expand upto neighbors or upto window size
                setVFromExpansion((geometry & ToTop) ? neighbors.top == -1 ? 0.f : context.GetGeometry(neighbors.top).Max.y
                    : neighbors.bottom == -1 ? height : context.GetGeometry(neighbors.bottom).Min.y);
                vexpanded = true;
            }
        }

        if (geometry & ToTop)
        {
            std::swap(margin.Min.y, margin.Max.y);
            std::swap(border.Min.y, border.Max.y);
            std::swap(padding.Min.y, padding.Max.y);
            std::swap(content.Min.y, content.Max.y);
        }

        if (geometry & ToLeft)
        {
            std::swap(margin.Min.x, margin.Max.x);
            std::swap(border.Min.x, border.Max.x);
            std::swap(padding.Min.x, padding.Max.x);
            std::swap(content.Min.x, content.Max.x);
        }

        ImVec2 textpos;

        if (hexpanded)
        {
            auto cw = content.GetWidth();

            if (style.alignment & TextAlignHCenter)
            {
                if (!textMetricsComputed)
                {
                    textMetricsComputed = true;
                    textsz = GetTextSize(type, text, style.font, cw, renderer);
                }

                if (textsz.x < cw)
                {
                    auto hdiff = (cw - textsz.x) * 0.5f;
                    textpos.x = content.Min.x + hdiff;
                }
            }
            else if (style.alignment & TextAlignRight)
            {
                if (!textMetricsComputed)
                {
                    textMetricsComputed = true;
                    textsz = GetTextSize(type, text, style.font, cw, renderer);
                }

                if (textsz.x < cw)
                {
                    auto hdiff = (cw - textsz.x);
                    textpos.x = content.Min.x + hdiff;
                }
            }
            else textpos.x = content.Min.x;
        }
        else textpos.x = content.Min.x;

        if (vexpanded)
        {
            auto cw = content.GetWidth();
            auto ch = content.GetHeight();

            if (style.alignment & TextAlignVCenter)
            {
                if (!textMetricsComputed)
                {
                    textMetricsComputed = true;
                    textsz = GetTextSize(type, text, style.font, cw, renderer);
                }

                if (textsz.y < ch)
                {
                    auto vdiff = (ch - textsz.y) * 0.5f;
                    textpos.y = content.Min.y + vdiff;
                }
            }
            else if (style.alignment & TextAlignBottom)
            {
                if (!textMetricsComputed)
                {
                    textMetricsComputed = true;
                    textsz = GetTextSize(type, text, style.font, cw, renderer);
                }

                if (textsz.y < ch)
                {
                    auto vdiff = (ch - textsz.y);
                    textpos.y = content.Min.y + vdiff;
                }
            }
            else textpos.y = content.Min.y;
        }
        else textpos.y = content.Min.y;

        content = { { std::min(content.Min.x, content.Max.x), std::min(content.Min.y, content.Max.y) },
            { std::max(content.Min.x, content.Max.x), std::max(content.Min.y, content.Max.y) } };
        padding = { { std::min(padding.Min.x, padding.Max.x), std::min(padding.Min.y, padding.Max.y) },
            { std::max(padding.Min.x, padding.Max.x), std::max(padding.Min.y, padding.Max.y) } };
        border = { { std::min(border.Min.x, border.Max.x), std::min(border.Min.y, border.Max.y) },
            { std::max(border.Min.x, border.Max.x), std::max(border.Min.y, border.Max.y) } };
        margin = { { std::min(margin.Min.x, margin.Max.x), std::min(margin.Min.y, margin.Max.y) },
            { std::max(margin.Min.x, margin.Max.x), std::max(margin.Min.y, margin.Max.y) } };

        return std::make_tuple(content, padding, border, margin, ImRect{ textpos, textpos + textsz });
    }

    std::tuple<ImRect, ImRect, ImRect, ImRect> GetBoxModelBounds(ImRect content, const StyleDescriptor& style)
    {
        ImRect margin, border, padding;

        if (style.dimension.x > 0.f)
        {
            margin = content;
            border.Min.x = margin.Min.x + style.border.left.thickness;
            border.Max.x = margin.Max.x - style.border.right.thickness;
            padding.Min.x = border.Min.x + style.padding.left;
            padding.Max.x = border.Max.x - style.padding.right;
            content.Min.x = padding.Min.x + style.padding.left;
            content.Max.x = padding.Max.x - style.padding.right;
        }
        else
        {
            padding.Min.x = content.Min.x - style.padding.left;
            padding.Max.x = content.Max.x + style.padding.right;
            border.Min.x = padding.Min.x - style.border.left.thickness;
            border.Max.x = padding.Max.x + style.border.right.thickness;
            margin.Min.x = border.Min.x - style.margin.left;
            margin.Max.x = border.Max.x + style.margin.right;
        }

        if (style.dimension.y > 0.f)
        {
            margin = content;
            border.Min.y = margin.Min.y + style.border.top.thickness;
            border.Max.y = margin.Max.y - style.border.bottom.thickness;
            padding.Min.y = border.Min.y + style.padding.top;
            padding.Max.y = border.Max.y - style.padding.bottom;
            content.Min.y = padding.Min.y + style.padding.top;
            content.Max.y = padding.Max.y - style.padding.bottom;
        }
        else
        {
            padding.Min.y = content.Min.y - style.padding.top;
            padding.Max.y = content.Max.y + style.padding.bottom;
            border.Min.y = padding.Min.y - style.border.top.thickness;
            border.Max.y = padding.Max.y + style.border.bottom.thickness;
            margin.Min.y = border.Min.y - style.margin.top;
            margin.Max.y = border.Max.y + style.margin.bottom;
        }

        return std::make_tuple(content, padding, border, margin);
    }

    static ImVec2 DetermineBounds(ImVec2 textsz, std::string_view prefix, std::string_view suffix,
        ImVec2 pos, LayoutItemDescriptor& item, const StyleDescriptor& style,
        IRenderer& renderer, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto cwidth = style.dimension.x > 0.f ? clamp(style.dimension.x, style.mindim.x, style.maxdim.x)
            : textsz.x + (!suffix.empty() ? style.font.size : 0.f) + (!prefix.empty() ? style.font.size : 0.f) +
            style.margin.h() + style.padding.h() + style.border.h();
        auto cheight = style.dimension.y > 0.f ? clamp(style.dimension.y, style.mindim.y, style.maxdim.y)
            : textsz.y + style.margin.v() + style.padding.v() + style.border.v();

        if (geometry & ToRight)
        {
            item.margin.Min.x = pos.x;
            item.border.Min.x = item.margin.Min.x + style.margin.left;
            item.padding.Min.x = item.border.Min.x + style.border.left.thickness;

            if (!prefix.empty())
            {
                item.prefix.Min.x = item.padding.Min.x + style.padding.left;
                item.prefix.Max.x = item.prefix.Min.x + style.font.size;
            }

            item.content.Min.x = item.padding.Min.x + item.prefix.GetWidth() + style.padding.left;
            item.margin.Max.x = item.margin.Min.x + cwidth;
            item.border.Max.x = item.margin.Max.x - style.margin.right;
            item.padding.Max.x = item.border.Max.x - style.border.right.thickness;

            if (!suffix.empty())
            {
                item.suffix.Max.x = item.padding.Max.x - style.padding.right;
                item.suffix.Min.x = item.suffix.Max.x - style.font.size;
            }

            item.content.Max.x = item.padding.Max.x - item.suffix.GetWidth() - style.padding.right;
        }
        else
        {
            item.margin.Max.x = pos.x;
            item.border.Max.x = item.margin.Max.x - style.margin.right;
            item.padding.Max.x = item.border.Max.x - style.border.right.thickness;

            if (!suffix.empty())
            {
                item.suffix.Max.x = item.padding.Max.x - style.padding.right;
                item.suffix.Min.x = item.prefix.Max.x - style.font.size;
            }

            item.content.Max.x = item.padding.Max.x - item.suffix.GetWidth() - style.padding.right;
            item.margin.Min.x = item.margin.Max.x - cwidth;
            item.border.Min.x = item.margin.Min.x + style.margin.left;
            item.padding.Min.x = item.border.Min.x + style.border.left.thickness;

            if (!prefix.empty())
            {
                item.prefix.Min.x = item.padding.Min.x + style.padding.left;
                item.prefix.Max.x = item.prefix.Min.x + style.font.size;
            }

            item.content.Min.x = item.padding.Min.x + item.prefix.GetWidth() + style.padding.left;
        }

        if (geometry & ToBottom)
        {
            item.margin.Min.y = pos.y;
            item.border.Min.y = item.margin.Min.y + style.margin.top;
            item.padding.Min.y = item.border.Min.y + style.border.top.thickness;

            if (!prefix.empty())
            {
                auto vdiff = std::max((cheight - style.font.size) * 0.5f, 0.f);
                item.prefix.Min.y = vdiff + pos.y;
                item.prefix.Max.y = item.prefix.Min.y + style.font.size;
            }

            item.content.Min.y = item.padding.Min.y + style.padding.top;
            item.margin.Max.y = item.margin.Min.y + cheight;
            item.border.Max.y = item.margin.Max.y - style.margin.bottom;
            item.padding.Max.y = item.border.Max.y - style.border.bottom.thickness;

            if (!suffix.empty())
            {
                auto vdiff = std::max((cheight - style.font.size) * 0.5f, 0.f);
                item.prefix.Min.y = vdiff + pos.y;
                item.prefix.Max.y = item.prefix.Min.y + style.font.size;
            }

            item.content.Max.y = item.padding.Max.y - style.padding.bottom;
        }
        else
        {
            item.margin.Max.y = pos.y;
            item.border.Max.y = item.margin.Max.y - style.margin.bottom;
            item.padding.Max.y = item.border.Max.y - style.border.bottom.thickness;

            if (!suffix.empty())
            {
                auto vdiff = (cheight - style.font.size) * 0.5f;
                item.prefix.Min.y = vdiff + pos.y;
                item.prefix.Max.y = item.prefix.Min.y + style.font.size;
            }

            item.content.Max.y = item.padding.Max.y - style.padding.bottom;
            item.margin.Min.y = item.margin.Max.y - cheight;
            item.border.Min.y = item.margin.Min.y + style.margin.top;
            item.padding.Min.y = item.border.Min.y + style.border.top.thickness;

            if (!prefix.empty())
            {
                auto vdiff = (cheight - style.font.size) * 0.5f;
                item.prefix.Min.y = vdiff + pos.y;
                item.prefix.Max.y = item.prefix.Min.y + style.font.size;
            }

            item.content.Min.y = item.padding.Min.y + style.padding.top;
        }

        if (item.text.GetArea() > 0.f)
        {
            auto sz = item.text.GetSize();
            item.text.Min = item.content.Min;
            item.text.Max = item.text.Min + sz;
        }
        else item.text = item.content;

        return item.margin.GetSize();
    }

#pragma endregion

#pragma region Region

    void HandleRegionEvent(WidgetContextData& context, int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, IRenderer& renderer, const IODescriptor& io, WidgetDrawResult& result)
    {
        if (!context.deferEvents)
        {
            auto& state = context.GetState(id).state.region;

            if (state.events != 0)
            {
                auto hasmouseover = state.events & ETP_Hovered;
                auto ismouseover = padding.Contains(io.mousepos);

                if (hasmouseover && ismouseover)
                    result.event = WidgetEvent::Hovered;

                if (ismouseover && (state.events & ETP_Clicked) && io.clicked())
                    result.event = WidgetEvent::Clicked;

                if (ismouseover && (state.events & ETP_DoubleClicked) && io.isLeftMouseDoubleClicked())
                    result.event = WidgetEvent::DoubleClicked;

                if (ismouseover && (state.events & ETP_RightClicked) && io.isRightClicked())
                    result.event = WidgetEvent::RightClicked;

                if (ismouseover)
                {
                    //Config.platform->SetMouseCursor(MouseCursor::Grab);
                    WidgetContextData::CurrentWidgetId = id;
                }
            }

            if (WidgetContextData::CurrentWidgetId == -1)
            {
                ShowTooltip(state._hoverDuration, padding, state.tooltip, io);
                HandleContextMenu(id, content, io);
            }

            WITH_WIDGET_LOG(id, margin);
            LOG_STATE(state.state);
            LOG_RECT(padding);
            LOG_STYLE2(state.state, id);
        }
        else context.deferedEvents.emplace_back(EventDeferInfo::ForRegion(id, margin, border, padding, content));
    }

    WidgetDrawResult RegionImpl(int32_t id, const StyleDescriptor& style, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, IRenderer& renderer, const IODescriptor& io, int depth)
    {
        auto& context = GetContext();
        assert((id & WidgetIndexMask) <= (int)context.states[WT_Region].size());

        WidgetDrawResult result;

        if (!context.regionBuilders.empty())
        {
            DrawBoxShadow(border.Min, border.Max, style, renderer);
            DrawBackground(border.Min, border.Max, style, renderer);
            DrawBorderRect(border.Min, border.Max, style.border, style.bgcolor, renderer);
        }

        HandleRegionEvent(context, id, margin, border, padding, content, renderer, io, result);
        result.geometry = margin;
        return result;
    }

    void BeginFlexRegion(int32_t id, Direction dir, ImVec2 spacing, bool wrap, int32_t events, ImVec2 size, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto& context = GetContext();
        auto& state = context.GetState(id).state.region;
        auto& region = context.regions.emplace_back();
        context.regionBuilders.push() = context.regions.size() - 1;

        auto style = context.GetStyle(state.state, id);
        state.events = events;

        region.id = id;
        region.depth = context.regionBuilders.size() - 1;
        region.layout = dir == DIR_Horizontal ? Layout::Horizontal : Layout::Vertical;
        size.x = size.x >= 0.f ? size.x : style.dimension.x;
        size.y = size.y >= 0.f ? size.y : style.dimension.y;

        context.RecordForReplay(context.regionBuilders.top(), LayoutOps::PushRegion);
        BeginFlexLayoutRegion(dir, geometry, wrap, spacing, size, neighbors, context.regionBuilders.top());
    }

    void BeginFlexRegion(std::string_view id, Direction dir, ImVec2 spacing, bool wrap, int32_t events, ImVec2 size, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto wid = GetIdFromString(id, WT_Region).first;
        BeginFlexRegion(wid, dir, spacing, wrap, events, size, geometry, neighbors);
    }

    void BeginGridRegion(int32_t id, int rows, int cols, ImVec2 spacing, int32_t events, ImVec2 size, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto& context = GetContext();
        auto& state = context.GetState(id).state.region;
        auto& region = context.regions.emplace_back();
        context.regionBuilders.push() = context.regions.size() - 1;

        auto style = context.GetStyle(state.state, id);
        state.events = events;

        region.id = id;
        region.depth = context.regionBuilders.size() - 1;
        region.layout = Layout::Grid;
        size.x = size.x >= 0.f ? size.x : style.dimension.x;
        size.y = size.y >= 0.f ? size.y : style.dimension.y;

        context.RecordForReplay(context.regionBuilders.top(), LayoutOps::PushRegion);
        BeginGridLayoutRegion(rows, cols, GridLayoutDirection::ByRows, geometry, {}, {}, spacing,
            size, neighbors, context.regionBuilders.top());
    }

    void BeginGridRegion(std::string_view id, int rows, int cols, ImVec2 spacing, int32_t events, ImVec2 size, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto wid = GetIdFromString(id, WT_Region).first;
        BeginGridRegion(wid, rows, cols, spacing, events, size, geometry, neighbors);
    }

    WidgetDrawResult EndRegion()
    {
        auto& context = GetContext();
        auto idx = context.regionBuilders.top();
        context.RecordForReplay(idx, LayoutOps::PopRegion);

        auto pos = EndLayout();
        context.regionBuilders.pop(1, true);
        return pos;
    }

#pragma endregion

#pragma region Button & Labels
    
    void HandleLabelEvent(WidgetContextData& context, int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io, WidgetDrawResult& result)
    {
        if (!context.deferEvents)
        {
            auto& state = context.GetState(id).state.label;
            auto ismouseover = padding.Contains(io.mousepos);
            state.state = !ismouseover ? WS_Default : WS_Hovered;
            if (ismouseover) WidgetContextData::CurrentWidgetId = id;
            ShowTooltip(state._hoverDuration, padding, state.tooltip, io);
            HandleContextMenu(id, content, io);

            WITH_WIDGET_LOG(id, margin);
            LOG_STATE(state.state);
            LOG_RECT(padding); if (state.type == TextType::PlainText) LOG_TEXT(state.text);
            LOG_STYLE2(state.state, id);
        }
        else context.deferedEvents.emplace_back(EventDeferInfo::ForLabel(id, margin, border, padding, content, text));
    }

    static ImVec2 ButtonBounds(int32_t id, ImVec2 pos, LayoutItemDescriptor& item, const StyleDescriptor& style,
        IRenderer& renderer, int32_t geometry, const NeighborWidgets& neighbors, float width)
    {
        const auto& config = CreateWidgetConfig(id).state.button;
        auto textsz = GetTextSize(config.type, config.text, style.font, width, renderer);
        return DetermineBounds(textsz, config.prefix, config.suffix, pos, item, style, renderer, geometry, neighbors);
    }

    void HandleButtonEvent(WidgetContextData& context, int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io, WidgetDrawResult& result)
    {
        if (!context.deferEvents)
        {
            auto& state = context.GetState(id).state.button;
            auto ismouseover = padding.Contains(io.mousepos);
            state.state = !ismouseover ? WS_Default :
                io.isLeftMouseDown() ? WS_Pressed | WS_Hovered : WS_Hovered;
            if (ismouseover && io.clicked())
                result.event = WidgetEvent::Clicked;
            if (ismouseover)
            {
                Config.platform->SetMouseCursor(MouseCursor::Grab);
                WidgetContextData::CurrentWidgetId = id;
            }
            ShowTooltip(state._hoverDuration, padding, state.tooltip, io);
            HandleContextMenu(id, content, io);

            WITH_WIDGET_LOG(id, margin);
            LOG_STATE(state.state);
            LOG_RECT(padding); if (state.type == TextType::PlainText) LOG_TEXT(state.text);
            LOG_STYLE2(state.state, id);
        }
        else context.deferedEvents.emplace_back(EventDeferInfo::ForButton(id, margin, border, padding, content, text));
    }

    WidgetDrawResult LabelImpl(int32_t id, const StyleDescriptor& style, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io, int32_t textflags)
    {
        auto& context = GetContext();
        assert((id & WidgetIndexMask) <= (int)context.states[WT_Label].size());

        WidgetDrawResult result;
        auto& state = context.GetState(id).state.label;

        DrawBoxShadow(border.Min, border.Max, style, renderer);
        DrawBackground(border.Min, border.Max, style, renderer);
        DrawBorderRect(border.Min, border.Max, style.border, style.bgcolor, renderer);
        DrawText(content.Min, content.Max, text, state.text, state.state & WS_Disabled, style, 
            renderer, textflags | style.font.flags);
        HandleLabelEvent(context, id, margin, border, padding, content, text, renderer, io, result);
        
        result.geometry = margin;
        return result;
    }

    WidgetDrawResult ButtonImpl(int32_t id, const StyleDescriptor& style, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, const ImRect& prefix, IRenderer& renderer, const IODescriptor& io)
    {
        WidgetDrawResult result;
        auto& context = GetContext();
        auto& state = context.GetState(id).state.button;

        DrawBoxShadow(border.Min, border.Max, style, renderer);
        DrawBackground(border.Min, border.Max, style, renderer);
        DrawBorderRect(border.Min, border.Max, style.border, style.bgcolor, renderer);
        DrawText(content.Min, content.Max, text, state.text, state.state & WS_Disabled, style, renderer);
        DrawText(prefix.Min, prefix.Max, prefix, state.prefix, state.state & WS_Disabled, style, 
            renderer, ToTextFlags(state.resTypes.first));
        HandleButtonEvent(context, id, margin, border, padding, content, text, renderer, io, result);
        DrawFocusRect(state.state, border.Min, border.Max, renderer);

        result.geometry = margin;
        return result;
    }

    WidgetDrawResult Label(int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        return Widget(id, WT_Label, geometry, neighbors);
    }

    WidgetDrawResult Label(std::string_view id, std::string_view content, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto wid = GetIdFromString(id, WT_Label).first;
        CreateWidgetConfig(wid).state.label.text = content;
        return Widget(wid, WT_Label, geometry, neighbors);
    }

    WidgetDrawResult Label(std::string_view id, std::string_view content, TextType type, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto wid = GetIdFromString(id, WT_Label).first;
        auto& label = CreateWidgetConfig(wid).state.label;
		label.text = content; label.type = type;
        return Widget(wid, WT_Label, geometry, neighbors);
    }

    WidgetDrawResult Label(std::string_view id, std::string_view content, std::string_view tooltip, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto wid = GetIdFromString(id, WT_Label).first;
        auto& config = CreateWidgetConfig(wid).state.label;
        config.text = content; config.tooltip = tooltip;
        return Widget(wid, WT_Label, geometry, neighbors);
    }

    WidgetDrawResult Button(int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        return Widget(id, WT_Button, geometry, neighbors);
    }

    WidgetDrawResult Button(std::string_view id, std::string_view content, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto wid = GetIdFromString(id, WT_Button).first;
        CreateWidgetConfig(wid).state.button.text = content;
        return Widget(wid, WT_Button, geometry, neighbors);
    }

    void BeginButton(std::string_view id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto wid = GetIdFromString(id, WT_Button).first;
        BeginFlexRegion(wid, DIR_Horizontal, { 0.f, 0.f }, true, ETP_Hovered | ETP_Clicked, { -1.f, -1.f }, geometry, neighbors);
    }

    WidgetDrawResult EndButton()
    {
        return EndRegion();
    }

#pragma endregion

#pragma region Context Menu

    bool HandleContextMenu(int32_t id, const ImRect& region, const IODescriptor& io)
    {
        if (WidgetContextData::RightClickContext.pos != ImVec2{} &&
            io.isRightClicked() && region.Contains(io.mousepos))
        {
            WidgetContextData::RightClickContext.id = id;
            WidgetContextData::RightClickContext.pos = io.mousepos;
            WidgetContextData::RightClickContext.type = (WidgetType)(id >> WidgetTypeBits);
            return true;
        }

        return false;
    }

    bool BeginContextMenu(ImVec2 fixedsz)
    {
        if (WidgetContextData::RightClickContext.pos != ImVec2{})
        {
            BeginPopup(WT_ContextMenu << WidgetTypeBits, WidgetContextData::RightClickContext.pos, fixedsz);
            SetPopupCallback(PopupCallback::PCB_AfterRender, [](void*, IRenderer&, ImVec2 offset, const ImRect& region) {
                auto io = Config.platform->desc;

                if (!region.Contains(io.mousepos))
                {
                    if (io.isMouseDown()) WidgetContextData::RemovePopup();
                }
                else
                {
                    for (auto idx = 0; idx < WidgetContextData::ContextMenuOptions.size(); ++idx)
                    {
                        auto& option = WidgetContextData::ContextMenuOptions[idx];
                        option.content.Translate(offset);
                        option.prefix.Translate(offset);

                        auto mouseover = (option.content.Contains(io.mousepos));
                        auto mouseopt = mouseover && (option.prefixId == -1 || !option.prefix.Contains(io.mousepos));
                        option.state = (mouseopt && io.isLeftMouseDown()) ? WS_Pressed | WS_Hovered :
                            mouseover ? WS_Hovered : WS_Default;
                        if (io.clicked() && mouseopt)
                            option.state |= WS_Selected;
                        else
                            option.state &= ~WS_Selected;
                    }
                }
            });
            return true;
        }

        return false;
    }

    UIElementDescriptor GetContextMenuContext()
    {
        return WidgetContextData::RightClickContext;
    }

    void AddContextMenuEntry(std::string_view text, TextType type, std::string_view prefix, ResourceType rt)
    {
        auto& context = GetContext();
        auto& option = WidgetContextData::ContextMenuOptionParams.emplace_back();
        option.text = text;
        option.prefix = prefix;
        option.type = type;
        option.rt = rt;
    }

    void AddContextMenuEntry(SymbolIcon icon, std::string_view text, TextType type)
    {
        auto& context = GetContext();
        auto& option = WidgetContextData::ContextMenuOptionParams.emplace_back();
        option.text = text;
        option.type = type;
        option.icon = icon;
    }

    void AddContextMenuEntry(CheckState* state, std::string_view text, TextType type)
    {
        auto& context = GetContext();
        auto& option = WidgetContextData::ContextMenuOptionParams.emplace_back();
        option.text = text;
        option.type = type;
        option.check = state;
    }

    void AddContextMenuSeparator(uint32_t color, float thickness)
    {
        auto& option = WidgetContextData::ContextMenuOptionParams.emplace_back();
        option.thickness = thickness;
        option.color = color;
    }

    WidgetDrawResult EndContextMenu()
    {
        enum PrefixType
        {
            NoPrefix, Checkable = 1, Icon = 2
        };

        int hasPrefix = PrefixType::NoPrefix;
        auto& context = GetContext();
        auto& renderer = *context.deferedRenderer;

        for (auto idx = 0; idx < WidgetContextData::ContextMenuOptionParams.size(); ++idx)
            if (WidgetContextData::ContextMenuOptionParams[idx].check != nullptr)
                hasPrefix |= PrefixType::Checkable;
            else if (!WidgetContextData::ContextMenuOptionParams[idx].prefix.empty() ||
                WidgetContextData::ContextMenuOptionParams[idx].icon != SymbolIcon::None)
                hasPrefix |= PrefixType::Icon;

        for (auto idx = 0; idx < WidgetContextData::ContextMenuOptionParams.size(); ++idx)
        {
            if (idx >= WidgetContextData::ContextMenuOptions.size())
                WidgetContextData::ContextMenuOptions.expand_and_create(1, true);

            auto& option = WidgetContextData::ContextMenuOptions[idx];
            const auto& params = WidgetContextData::ContextMenuOptionParams[idx];

            auto style = context.GetStyle(option.state);
            auto xprefix = (hasPrefix & PrefixType::Icon) ? style.font.size :
                (hasPrefix & PrefixType::Checkable) ? (style.font.size * 0.6f) : 0.f;

            if (params.thickness > 0.f)
            {
                auto pos = context.NextAdHocPos();
                renderer.DrawRect(pos, pos + ImVec2{ FLT_MAX, 0.f }, params.color, true, params.thickness);
                context.adhocLayout.top().nextpos.y += params.thickness;
            }
            else if (params.check == nullptr)
            {
                ImVec2 prefixsz{ style.font.size, style.font.size };
                auto pos = context.NextAdHocPos();
                context.adhocLayout.top().nextpos.x = pos.x = style.padding.left;

                if (!params.prefix.empty() && params.rt != RT_INVALID)
                {
                    renderer.DrawResource(params.rt, pos, prefixsz, style.fgcolor, params.prefix);
                    pos.x += prefixsz.x + 10.f;
                }
                else if (params.icon != SymbolIcon::None)
                {
                    DrawSymbol(pos, prefixsz, { 0.f, 0.f }, params.icon, style.fgcolor, style.fgcolor, 2.f, renderer);
                    pos.x += prefixsz.x + 10.f;
                }
                else if (hasPrefix) pos.x += prefixsz.x + 10.f;

                auto [content, padding, border, margin, textrect] = GetBoxModelBounds(pos, style, params.text, renderer, ToBottomRight,
                    params.type, {}, FLT_MAX, FLT_MAX);
                DrawBackground(margin.Min, margin.Max, style, renderer);
                DrawText(content.Min, content.Max, textrect, params.text, option.state & WS_Disabled, style, renderer, style.font.flags);

                option.content.Min = { 0.f, margin.Min.y };
                option.content.Max = margin.Max;
                option.textrect = margin;

                context.adhocLayout.top().nextpos.y += margin.GetHeight();
            }
            else
            {
                auto pos = context.NextAdHocPos();
                context.adhocLayout.top().nextpos.x = pos.x = style.padding.left;

                auto [content, padding, border, margin, textrect] = GetBoxModelBounds(pos, style, params.text, renderer, ToBottomRight,
                    params.type, {}, context.popupSize.x, context.popupSize.y);

                auto oldy = pos.y;
                pos.y += (margin.GetHeight() - (style.font.size * 0.6f)) * 0.5f;
                context.adhocLayout.top().nextpos.y = pos.y;

                auto checkstyle = style;
                checkstyle.padding = checkstyle.margin = FourSidedMeasure{};
                // TODO: Add default checkbox styling...
                for (auto idx = 0; idx < WSI_Total; ++idx)
                    WidgetContextData::StyleStack[idx].push() = checkstyle;
                auto res = Checkbox(params.check);
                for (auto idx = 0; idx < WSI_Total; ++idx)
                    WidgetContextData::StyleStack[idx].pop(1, true);

                auto shiftx = xprefix + 10.f;
                option.prefix = res.geometry;
                option.prefixId = res.id;

                context.adhocLayout.top().nextpos.y = pos.y = oldy;

                margin.TranslateX(shiftx); content.TranslateX(shiftx); textrect.TranslateX(shiftx);
                DrawBackground(margin.Min, margin.Max, style, renderer);
                DrawText(content.Min, content.Max, textrect, params.text, option.state & WS_Disabled, style, renderer, style.font.flags);

                option.content.Min = { 0.f, margin.Min.y };
                option.content.Max = margin.Max;
                option.textrect = margin;

                context.adhocLayout.top().nextpos.y += margin.GetHeight();
            }
        }

        auto res = EndPopUp();

        if (res.event == WidgetEvent::None)
        {
            for (auto idx = 0; idx < WidgetContextData::ContextMenuOptions.size(); ++idx)
                if (WidgetContextData::ContextMenuOptions[idx].state & WS_Selected)
                {
                    res.event = WidgetEvent::Clicked;
                    res.optidx = idx;
                    WidgetContextData::RemovePopup();
                    break;
                }

            if (res.event == WidgetEvent::None && Config.platform->desc.clicked())
            {
                if (res.geometry.Contains(Config.platform->desc.mousepos))
                    res.event = WidgetEvent::RightClicked;
                else
                    WidgetContextData::RemovePopup();
            }
        }

        return res;
    }

#pragma endregion

#pragma region Toggle Button

    static std::pair<ImRect, ImVec2> ToggleButtonBounds(ToggleButtonState& state, const ImRect& extent, IRenderer& renderer)
    {
        auto& context = GetContext();
        auto style = context.GetStyle(state.state, state.id);
        auto& specificStyle = context.toggleButtonStyles[log2((unsigned)state.state)].top();
        ImRect result;
        ImVec2 text;
        result.Min = result.Max = extent.Min;

        if (specificStyle.showText)
        {
            if (specificStyle.fontptr == nullptr) specificStyle.fontptr = GetFont(GLIMMER_DEFAULT_FONTFAMILY,
                specificStyle.fontsz, FT_Bold);

            renderer.SetCurrentFont(specificStyle.fontptr, specificStyle.fontsz);
            auto text1 = renderer.GetTextSize(Config.toggleButtonText[0], specificStyle.fontptr, specificStyle.fontsz);
            auto text2 = renderer.GetTextSize(Config.toggleButtonText[1], specificStyle.fontptr, specificStyle.fontsz);
            text = { text1.x + text2.x, std::max(text1.y, text2.y) };
            result.Max += text;
            renderer.ResetFont();

            auto extra = 2.f * (-specificStyle.thumbOffset + specificStyle.trackBorderThickness);
            result.Max.x += extra;
            result.Max.y += extra;
        }
        else
        {
            result.Max.x += ((extent.GetHeight()) * 2.f); 
            result.Max.y += extent.GetHeight();
        }

        return { result, text };
    }

    void HandleToggleButtonEvent(WidgetContextData& context, int32_t id, const ImRect& extent, ImVec2 center, IRenderer& renderer, const IODescriptor& io,
        WidgetDrawResult& result)
    {
        if (!context.deferEvents)
        {
            auto& toggle = context.ToggleState(id);
            auto& state = context.GetState(id).state.toggle;
            auto mousepos = io.mousepos;
            auto mouseover = extent.Contains(mousepos);

            if (mouseover && io.clicked())
            {
                result.event = WidgetEvent::Clicked;
                state.checked = !state.checked;
                toggle.animate = true;
                toggle.progress = 0.018f;
            }

            toggle.btnpos = toggle.animate ? center.x : -1.f;
            state.state = mouseover && io.isLeftMouseDown() ? WS_Hovered | WS_Pressed :
                mouseover ? WS_Hovered : WS_Default;
            state.state = state.checked ? state.state | WS_Checked : state.state & ~WS_Checked;
            if (mouseover) WidgetContextData::CurrentWidgetId = id;
            if (state.out) *state.out = state.checked;
            ShowTooltip(state._hoverDuration, extent, state.tooltip, io);
            HandleContextMenu(id, extent, io);

            WITH_WIDGET_LOG(id, extent);
            LOG_STATE(state.state);
            LOG_NUM(state.checked);
            LOG_STYLE2(state.state, id);
        }
        else context.deferedEvents.emplace_back(EventDeferInfo::ForToggleButton(id, extent, center));
    }

    WidgetDrawResult ToggleButtonImpl(int32_t id, ToggleButtonState& state, const StyleDescriptor& style, const ImRect& extent, ImVec2 textsz,
        IRenderer& renderer, const IODescriptor& io)
    {
        WidgetDrawResult result;
        auto& context = GetContext();
        auto& specificStyle = context.toggleButtonStyles[log2((unsigned)state.state)].top();
        auto& toggle = context.ToggleState(id);

        auto extra = (-specificStyle.thumbOffset + specificStyle.trackBorderThickness);
        auto radius = (extent.GetHeight() * 0.5f) - (2.f * extra);
        auto movement = extent.GetWidth() - (2.f * (radius + extra));
        auto ratio = anim::EaseOut(toggle.progress, specificStyle.animationDuration);
        auto moveAmount = toggle.animate ? ratio * movement * (state.checked ? 1.f : -1.f) : 0.f;

        auto center = toggle.btnpos == -1.f ? state.checked ? extent.Max - ImVec2{ extra + radius, extra + radius }
            : extent.Min + ImVec2{ radius + extra, extra + radius }
        : ImVec2{ toggle.btnpos, extra + radius };
        center.x = ImClamp(center.x + moveAmount, extent.Min.x + (extra * 0.5f), extent.Max.x - extra);
        center.y = extent.Min.y + (extent.GetHeight() * 0.5f);
        toggle.animate = (center.x < (extent.Max.x - extra - radius)) && (center.x > (extent.Min.x + extra + radius));

        auto rounded = extent.GetHeight() * 0.5f;
        auto tcol = toggle.animate ? anim::InterpolateColor(state.checked ? 
            context.toggleButtonStyles[WSI_Default].top().trackColor :
            context.toggleButtonStyles[WSI_Checked].top().trackColor,
            specificStyle.trackColor, toggle.progress) :
            specificStyle.trackColor;

        renderer.DrawRoundedRect(extent.Min, extent.Max, tcol, true, rounded, rounded, rounded, rounded);
        renderer.DrawRoundedRect(extent.Min, extent.Max, specificStyle.trackBorderColor, false, rounded, rounded, rounded, rounded,
            specificStyle.trackBorderThickness);

        if (specificStyle.showText && !toggle.animate)
        {
            renderer.SetCurrentFont(specificStyle.fontptr, specificStyle.fontsz);
            auto texth = ((extent.GetHeight() - textsz.y) * 0.5f) - 2.f;
            state.checked ? renderer.DrawText(Config.toggleButtonText[1],
                    extent.Min + ImVec2{extra, texth}, specificStyle.indicatorTextColor) :
                renderer.DrawText(Config.toggleButtonText[0],
                    extent.Min + ImVec2{ (extent.GetWidth() * 0.5f) - 5.f, texth }, specificStyle.indicatorTextColor);
            renderer.ResetFont();
        }

        renderer.DrawCircle(center, radius + specificStyle.thumbExpand, style.fgcolor, true);
        DrawFocusRect(state.state, extent.Min, extent.Max, renderer);
        HandleToggleButtonEvent(context, id, extent, center, renderer, io, result);

        result.geometry = extent;
        return result;
    }

    WidgetDrawResult ToggleButton(int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        return Widget(id, WT_ToggleButton, geometry, neighbors);
    }

    WidgetDrawResult ToggleButton(bool* state, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto id = GetIdFromOutPtr(state, WT_ToggleButton).first;
        auto& config = CreateWidgetConfig(id).state.toggle;
        config.checked = *state; config.out = state;
        return Widget(id, WT_ToggleButton, geometry, neighbors);
    }

    WidgetDrawResult ToggleButton(std::string_view id, bool* state, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto wid = GetIdFromString(id, WT_ToggleButton).first;
        auto& config = CreateWidgetConfig(wid).state.toggle;
        config.checked = *state; config.out = state;
        return Widget(wid, WT_ToggleButton, geometry, neighbors);
    }

#pragma endregion

#pragma region Radio Button

    static ImRect RadioButtonBounds(const RadioButtonState& state, const ImRect& extent)
    {
        auto& context = GetContext();
        const auto style = context.GetStyle(state.state, state.id);
        return ImRect{ extent.Min, extent.Min + ImVec2{ style.font.size, style.font.size } };
    }

    void HandleRadioButtonEvent(WidgetContextData& context, int32_t id, const ImRect& extent, float maxrad, IRenderer& renderer, const IODescriptor& io,
        WidgetDrawResult& result)
    {
        if (!context.deferEvents)
        {
            auto& radio = context.RadioState(id);
            auto& state = context.GetState(id).state.radio;
            auto mousepos = io.mousepos;
            auto mouseover = extent.Contains(mousepos);

            if (mouseover && io.clicked())
            {
                result.event = WidgetEvent::Clicked;
                state.checked = !state.checked;
                radio.animate = true;
                radio.progress = 0.018f;
            }

            state.state = mouseover && io.isLeftMouseDown() ? WS_Hovered | WS_Pressed :
                mouseover ? WS_Hovered : WS_Default;
            state.state = state.checked ? state.state | WS_Checked : state.state & ~WS_Checked;
            if (mouseover) WidgetContextData::CurrentWidgetId = id;
            if (state.out) *state.out = state.checked;
            ShowTooltip(state._hoverDuration, extent, state.tooltip, io);
            HandleContextMenu(id, extent, io);

            WITH_WIDGET_LOG(id, extent);
            LOG_STATE(state.state);
            LOG_NUM(state.checked);
            LOG_STYLE2(state.state, id);
        }
        else context.deferedEvents.emplace_back(EventDeferInfo::ForRadioButton(id, extent, maxrad));
    }

    WidgetDrawResult RadioButtonImpl(int32_t id, RadioButtonState& state, const StyleDescriptor& style, const ImRect& extent,
        IRenderer& renderer, const IODescriptor& io)
    {
        WidgetDrawResult result;
        auto& context = GetContext();
        auto& specificStyle = context.radioButtonStyles[log2((unsigned)state.state)].top();
        auto& radio = context.RadioState(id);

        auto radius = (extent.GetWidth() - 2.f) * 0.5f;
        auto center = extent.Min + ImVec2{ radius + 1.f, radius + 1.f };
        renderer.DrawCircle(center, radius, specificStyle.outlineColor, false, specificStyle.outlineThickness);
        auto maxrad = radius * specificStyle.checkedRadius;
        
        if (radio.animate)
        {
            auto ratio = anim::EaseOut(radio.progress, specificStyle.animationDuration);
            radius =  state.checked ? ratio * maxrad : (1.f - ratio) * maxrad;
        }
        else radius = state.checked ? maxrad : 0.f;

        if (radius > 0.f) renderer.DrawCircle(center, radius, specificStyle.checkedColor, true);
        DrawFocusRect(state.state, extent.Min, extent.Max, renderer);

        radio.animate = radius > 0.f && radius < maxrad;
        HandleRadioButtonEvent(context, id, extent, maxrad, renderer, io, result);
        result.geometry = extent;
        return result;
    }

    WidgetDrawResult RadioButton(int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        return Widget(id, WT_RadioButton, geometry, neighbors);
    }

    WidgetDrawResult RadioButton(bool* state, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto id = GetIdFromOutPtr(state, WT_RadioButton).first;
        auto& config = CreateWidgetConfig(id).state.radio;
        config.checked = *state; config.out = state;
        return Widget(id, WT_RadioButton, geometry, neighbors);
    }

    WidgetDrawResult RadioButton(std::string_view id, bool* state, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto wid = GetIdFromString(id, WT_RadioButton).first;
        auto& config = CreateWidgetConfig(wid).state.radio;
        config.checked = *state; config.out = state;
        return Widget(wid, WT_RadioButton, geometry, neighbors);
    }

#pragma endregion

#pragma region Checkbox

    static ImRect CheckboxBounds(const CheckboxState& state, const ImRect& extent)
    {
        auto& context = GetContext();
        const auto style = context.GetStyle(state.state, state.id);
        return ImRect{ extent.Min, extent.Min + ImVec2{ style.font.size * 0.6f, style.font.size * 0.6f } };
    }

    void HandleCheckboxEvent(WidgetContextData& context, int32_t id, const ImRect& extent, const IODescriptor& io,
        WidgetDrawResult& result)
    {
        if (!context.deferEvents)
        {
            auto& check = context.CheckboxState(id);
            auto& state = context.GetState(id).state.checkbox;

            auto mousepos = io.mousepos;
            auto mouseover = extent.Contains(mousepos);
            auto isclicked = mouseover && io.isLeftMouseDown();
            state.state = isclicked ? state.state | WS_Hovered | WS_Pressed :
                mouseover ? state.state & ~WS_Pressed : state.state & ~WS_Hovered;

            if (mouseover && io.clicked())
            {
                state.check = state.check == CheckState::Unchecked ? CheckState::Checked : CheckState::Unchecked;
                state.state = state.check == CheckState::Unchecked ? state.state & ~WS_Checked : state.state | WS_Checked;
                result.event = WidgetEvent::Clicked;
                check.animate = state.check != CheckState::Unchecked;
                check.progress = 0.f;
            }

            if (mouseover) WidgetContextData::CurrentWidgetId = id;
            if (state.out) *state.out = state.check;
            ShowTooltip(state._hoverDuration, extent, state.tooltip, io);
            HandleContextMenu(id, extent, io);

            WITH_WIDGET_LOG(id, extent);
            LOG_STATE(state.state);
            LOG_NUM(state.check);
            LOG_STYLE2(state.state, id);
        }
        else context.deferedEvents.emplace_back(EventDeferInfo::ForCheckbox(id, extent));
    }

    WidgetDrawResult CheckboxImpl(int32_t id, CheckboxState& state, const StyleDescriptor& style, const ImRect& extent, const ImRect& padding,
        IRenderer& renderer, const IODescriptor& io)
    {
        auto& context = GetContext();
        auto& check = context.CheckboxState(id);
        WidgetDrawResult result;

        DrawBorderRect(extent.Min, extent.Max, style.border, style.bgcolor, renderer);
        DrawBackground(padding.Min, padding.Max, style, renderer);
        auto height = padding.GetHeight(), width = padding.GetWidth();

        if (check.animate && check.progress < 1.f)
            check.progress += (io.deltaTime / 0.25f);

        switch (state.check)
        {
        case CheckState::Checked:
        {
            auto start = ImVec2{ padding.Min.x, padding.Min.y + (height * 0.5f) };
            auto end = ImVec2{ padding.Min.x + (width * 0.333f), padding.Max.y };
            auto tickw = padding.Max.x - end.x;
            renderer.DrawLine(start, end, style.fgcolor, 2.f);
            renderer.DrawLine(end, ImVec2{ padding.Max.x - ((1.f - check.progress) * tickw), padding.Min.y +
                ((1.f - check.progress) * height) }, style.fgcolor, 2.f);
            break;
        }
        case CheckState::Partial:
            renderer.DrawLine(padding.Min + ImVec2{ 0.f, height * 0.5f }, padding.Max - 
                ImVec2{ 0.f, height * 0.5f }, style.fgcolor, 2.f);
            break;
        default:
            break;
        }

        DrawFocusRect(state.state, extent.Min, extent.Max, renderer);
        HandleCheckboxEvent(context, id, extent, io, result);
        
        result.geometry = extent;
        return result;
    }

    WidgetDrawResult Checkbox(int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        return Widget(id, WT_Checkbox, geometry, neighbors);
    }

    WidgetDrawResult Checkbox(CheckState* state, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto id = GetIdFromOutPtr(state, WT_Checkbox).first;
        auto& config = CreateWidgetConfig(id).state.checkbox;
        config.check = *state; config.out = state;
        return Widget(id, WT_Checkbox, geometry, neighbors);
    }

    WidgetDrawResult Checkbox(std::string_view id, CheckState* state, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto wid = GetIdFromString(id, WT_Checkbox).first;
        auto& config = CreateWidgetConfig(wid).state.checkbox;
        config.check = *state; config.out = state;
        return Widget(wid, WT_Checkbox, geometry, neighbors);
    }

#pragma endregion

#pragma region Spinner

    static ImRect SpinnerBounds(int32_t id, const SpinnerState& state, IRenderer& renderer, const ImRect& extent)
    {
        auto digits = (int)(ceilf(log10f(state.max) + 1.f)) + (!state.isInteger ? state.precision + 1 : 0);
        auto& context = GetContext();
        const auto style = context.GetStyle(state.state, state.id);

        static char buffer[32] = { 0 };
        assert(digits < 31);
        memset(buffer, '0', digits);
        buffer[digits] = 0;

        std::string_view text{ buffer, (size_t)digits };
        auto txtsz = renderer.GetTextSize(text, style.font.font, style.font.size);
        ImRect result;
        result.Min = extent.Min;

        switch (state.placement)
        {
        case SpinnerButtonPlacement::EitherSide:
            result.Max.x = result.Min.x + txtsz.x + (2.f * style.font.size) + style.padding.v();
            result.Max.y = result.Min.y + txtsz.y;
            break;
        case SpinnerButtonPlacement::VerticalLeft:
        case SpinnerButtonPlacement::VerticalRight:
            result.Max.x = result.Min.x + txtsz.x + style.font.size + style.padding.v();
            result.Max.y = result.Min.y + txtsz.y;
            break;
        default:
            break;
        }

        result.Max.x += style.padding.h();
        result.Max.y += style.padding.v();
        if (style.dimension.x > 0.f) result.Max.x = result.Min.x + style.dimension.x;
        if (style.dimension.y > 0.f) result.Max.y = result.Min.y + style.dimension.y;
        return result;
    }

    void HandleSpinnerEvent(WidgetContextData& context, int32_t id, const ImRect& extent, const ImRect& incbtn, const ImRect& decbtn, const IODescriptor& io,
        WidgetDrawResult& result)
    {
        if (!context.deferEvents)
        {
            auto& state = context.GetState(id).state.spinner;
            auto& spinner = context.SpinnerState(id);

            if (incbtn.Contains(io.mousepos))
            {
                if (io.isLeftMouseDown())
                {
                    if (!(state.state & WS_Pressed))
                    {
                        spinner.lastChangeTime = 0.f;
                        spinner.repeatRate = state.repeatTrigger;
                    }

                    if (spinner.lastChangeTime >= state.repeatRate)
                    {
                        state.data = std::min(state.data + state.delta, state.max);
                        spinner.repeatRate = state.repeatRate;
                        result.event = WidgetEvent::Edited;
                    }
                    else spinner.lastChangeTime += io.deltaTime;

                    state.state |= WS_Pressed;
                }
                else 
                {
                    state.state &= ~WS_Pressed;

                    if (io.clicked())
                    {
                        state.data = std::min(state.data + state.delta, state.max);
                        result.event = WidgetEvent::Edited;
                    }
                }
            }
            else if (decbtn.Contains(io.mousepos))
            {
                if (io.isLeftMouseDown())
                {
                    if (!(state.state & WS_Pressed))
                    {
                        spinner.lastChangeTime = 0.f;
                        spinner.repeatRate = state.repeatTrigger;
                    }

                    if (spinner.lastChangeTime >= state.repeatRate)
                    {
                        state.data = std::max(state.data - state.delta, state.min);
                        spinner.repeatRate = state.repeatRate;
                        result.event = WidgetEvent::Edited;
                    }
                    else spinner.lastChangeTime += io.deltaTime;

                    state.state |= WS_Pressed;
                }
                else
                {
                    state.state &= ~WS_Pressed;

                    if (io.clicked())
                    {
                        state.data = std::max(state.data - state.delta, state.min);
                        result.event = WidgetEvent::Edited;
                    }
                }
            }
            else spinner.lastChangeTime = 0.f;

            if (state.out)
            {
                switch (state.outType)
                {
                case OutPtrType::i32: *(int32_t*)(state.out) = (int32_t)state.data; break;
                case OutPtrType::f32: *(float*)(state.out) = state.data; break;
                case OutPtrType::f64: *(double*)(state.out) = state.data; break;
                default: break;
                }
            }

            if (extent.Contains(io.mousepos)) WidgetContextData::CurrentWidgetId = id;
            ShowTooltip(state._hoverDuration, extent, state.tooltip, io);
            HandleContextMenu(id, extent, io);

            WITH_WIDGET_LOG(id, extent);
            LOG_STATE(state.state);
            LOG_NUM(state.data); LOG_NUM(state.min); LOG_NUM(state.max);
            LOG_STYLE2(state.state, id);
        }
        else
            context.deferedEvents.emplace_back(EventDeferInfo::ForSpinner(id, extent, incbtn, decbtn));
    }

    WidgetDrawResult SpinnerImpl(int32_t id, const SpinnerState& state, const StyleDescriptor& style, 
        const ImRect& extent, const IODescriptor& io, IRenderer& renderer)
    {
        WidgetDrawResult result;
        ImRect incbtn, decbtn;
        auto& context = GetContext();
        const auto& specificStyle = context.spinnerStyles[log2((unsigned)state.state)].top();
        static char buffer[32] = { 0 };

        ImRect border{ extent.Min - ImVec2{ style.border.left.thickness, style.border.top.thickness }, 
            extent.Max + ImVec2{ style.border.right.thickness, style.border.bottom.thickness } };
        DrawBackground(extent.Min, extent.Max, style, renderer);
        DrawBorderRect(border.Min, border.Max, style.border, style.bgcolor, renderer);
        renderer.SetCurrentFont(style.font.font, style.font.size);

        auto end = state.isInteger ? std::to_chars(buffer, buffer + 32, (int)state.data).ptr :
            std::to_chars(buffer, buffer + 32, state.data, std::chars_format::general, state.precision).ptr;
        std::string_view text{ buffer, end };
        auto txtsz = renderer.GetTextSize(text, style.font.font, style.font.size);

        auto drawbutton = [&style, &renderer, &specificStyle, &state](const ImRect& rect, bool inc) {
            auto color = inc ? specificStyle.upbtnColor : specificStyle.downbtnColor;
            auto darker = DarkenColor(color);

            switch (state.placement)
            {
            case SpinnerButtonPlacement::EitherSide:
                if (inc)
                    if (style.border.cornerRadius[TopRightCorner] != 0.f ||
                        style.border.cornerRadius[BottomRightCorner] != 0.f)
                        renderer.DrawRoundedRect(rect.Min, rect.Max,
                            color, true, 0.f, style.border.cornerRadius[TopRightCorner],
                            style.border.cornerRadius[BottomRightCorner], 0.f);
                    else renderer.DrawRect(rect.Min, rect.Max, color, true);
                else
                    if (style.border.cornerRadius[TopLeftCorner] != 0.f ||
                        style.border.cornerRadius[BottomLeftCorner] != 0.f)
                        renderer.DrawRoundedRect(rect.Min, rect.Max,
                            color, true, style.border.cornerRadius[TopLeftCorner], 0.f, 0.f,
                            style.border.cornerRadius[BottomLeftCorner]);
                    else renderer.DrawRect(rect.Min, rect.Max, color, true);
                renderer.DrawLine(inc ? rect.Min : ImVec2{ rect.Max.x, rect.Min.y }, inc ? ImVec2{ rect.Min.x, rect.Max.y } : rect.Max, 
                    darker, specificStyle.btnBorderThickness);
                break;
            case SpinnerButtonPlacement::VerticalRight:
                if (inc)
                    if (style.border.cornerRadius[TopRightCorner] != 0.f)
                        renderer.DrawRoundedRect(rect.Min, rect.Max,
                            color, true, 0.f, style.border.cornerRadius[TopRightCorner], 0.f, 0.f);
                    else renderer.DrawRect(rect.Min, rect.Max, color, true);
                else
                    if (style.border.cornerRadius[BottomRightCorner] != 0.f)
                        renderer.DrawRoundedRect(rect.Min, rect.Max, color, true, 0.f, 0.f,
                            style.border.cornerRadius[BottomRightCorner], 0.f);
                    else renderer.DrawRect(rect.Min, rect.Max, color, true);
                break;
            case SpinnerButtonPlacement::VerticalLeft: break; // TODO ...
            default: break;
            }

            if (!specificStyle.upDownArrows) DrawSymbol(rect.Min, rect.GetSize(), { 8.f, 5.f }, inc ? SymbolIcon::Plus : SymbolIcon::Minus, darker, 0, 2.f, renderer);
            else DrawSymbol(rect.Min, rect.GetSize(), { 8.f, 5.f }, inc ? SymbolIcon::UpArrow : SymbolIcon::DownArrow, darker, 0, 2.f, renderer);

            return darker;
        };

        switch (state.placement)
        {
        case SpinnerButtonPlacement::EitherSide:
        {
            ImVec2 btnsz{ style.font.size + style.padding.v(), style.font.size + style.padding.v() };
            decbtn = ImRect{ extent.Min, extent.Min + btnsz };
            drawbutton(decbtn, false);

            auto availw = extent.GetWidth() - (2.f * btnsz.x) - style.padding.h();
            auto txtstart = decbtn.Max + ImVec2{ availw * 0.5f, -decbtn.GetHeight() } + ImVec2{ style.padding.left, style.padding.top };
            renderer.DrawText(text, txtstart, style.fgcolor);

            incbtn = ImRect{ extent.Max, extent.Max - btnsz };
            drawbutton(incbtn, true);
            break;
        }
        case SpinnerButtonPlacement::VerticalLeft:
            // TODO: left aligned up/down button...
            break;
        case SpinnerButtonPlacement::VerticalRight:
        {
            ImVec2 btnsz{ style.font.size + style.padding.v(), (style.font.size + style.padding.v()) * 0.5f };

            auto availw = extent.GetWidth() - btnsz.x - style.padding.h() - txtsz.x;
            auto txtstart = ImVec2{ extent.Min.x + (availw * 0.5f), extent.Min.y } + ImVec2{ style.padding.left, style.padding.top };
            renderer.DrawText(text, txtstart, style.fgcolor);

            auto btnstart = ImVec2{ extent.Max.x - btnsz.x, extent.Min.y };
            incbtn = ImRect{ btnstart, btnstart + btnsz };
            drawbutton(incbtn, true);

            decbtn = ImRect{ ImVec2{ incbtn.Min.x, incbtn.Max.y }, extent.Max };
            auto darker = drawbutton(decbtn, false);
            renderer.DrawLine(incbtn.Min, ImVec2{ incbtn.Min.x, decbtn.Max.y }, darker, specificStyle.btnBorderThickness);
            renderer.DrawLine(ImVec2{ incbtn.Min.x, decbtn.Min.y }, ImVec2{ incbtn.Max.x, decbtn.Min.y }, darker, specificStyle.btnBorderThickness);
            break;
        }
        default:
            break;
        }

        renderer.ResetFont();
        DrawFocusRect(state.state, border.Min, border.Max, renderer);
        HandleSpinnerEvent(context, id, extent, incbtn, decbtn, io, result);
        result.geometry = extent;
        return result;
    }

    WidgetDrawResult Spinner(int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        return Widget(id, WT_Spinner, geometry, neighbors);
    }

    WidgetDrawResult Spinner(int32_t* value, int32_t step, std::pair<int32_t, int32_t> range, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto id = GetIdFromOutPtr(value, WT_Spinner).first;
        auto& config = CreateWidgetConfig(id).state.spinner;
        config.data = (float)*value; config.out = value; config.outType = OutPtrType::i32;
        config.isInteger = true; config.delta = (float)step; config.min = (float)range.first; config.max = (float)range.second;
        return Widget(id, WT_Spinner, geometry, neighbors);
    }

    WidgetDrawResult Spinner(std::string_view id, int32_t* value, int32_t step, std::pair<int32_t, int32_t> range, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto wid = GetIdFromString(id, WT_Spinner).first;
        auto& config = CreateWidgetConfig(wid).state.spinner;
        config.data = (float)*value; config.out = value; config.outType = OutPtrType::i32;
        config.isInteger = true; config.delta = (float)step; config.min = (float)range.first; config.max = (float)range.second;
        return Widget(wid, WT_Spinner, geometry, neighbors);
    }

    WidgetDrawResult Spinner(float* value, float step, std::pair<float, float> range, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto id = GetIdFromOutPtr(value, WT_Spinner).first;
        auto& config = CreateWidgetConfig(id).state.spinner;
        config.data = *value; config.out = value; config.outType = OutPtrType::f32;
        config.isInteger = false; config.delta = step; config.min = range.first; config.max = range.second;
        return Widget(id, WT_Spinner, geometry, neighbors);
    }

    WidgetDrawResult Spinner(std::string_view id, float* value, float step, std::pair<float, float> range, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto wid = GetIdFromString(id, WT_Spinner).first;
        auto& config = CreateWidgetConfig(wid).state.spinner;
        config.data = *value; config.out = value; config.outType = OutPtrType::f32;
        config.isInteger = false; config.delta = step; config.min = range.first; config.max = range.second;
        return Widget(wid, WT_Spinner, geometry, neighbors);
    }

    WidgetDrawResult Spinner(double* value, float step, std::pair<float, float> range, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto id = GetIdFromOutPtr(value, WT_Spinner).first;
        auto& config = CreateWidgetConfig(id).state.spinner;
        config.data = (float)*value; config.out = value; config.outType = OutPtrType::f64;
        config.isInteger = false; config.delta = step; config.min = range.first; config.max = range.second;
        return Widget(id, WT_Spinner, geometry, neighbors);
    }

    WidgetDrawResult Spinner(std::string_view id, double* value, float step, std::pair<float, float> range, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto wid = GetIdFromString(id, WT_Spinner).first;
        auto& config = CreateWidgetConfig(wid).state.spinner;
        config.data = (float)*value; config.out = value; config.outType = OutPtrType::f64;
        config.isInteger = false; config.delta = step; config.min = range.first; config.max = range.second;
        return Widget(wid, WT_Spinner, geometry, neighbors);
    }

#pragma endregion

#pragma region Slider

    ImRect SliderBounds(int32_t id, const ImRect& extent)
    {
        ImRect result;
        auto& context = GetContext();
        auto& state = context.GetState(id).state.slider;
        const auto style = context.GetStyle(state.state, state.id);
        auto slidersz = std::max(Config.sliderSize, style.font.size);
        auto width = (style.dimension.x > 0.f) ? style.dimension.x : 
            (state.dir == DIR_Horizontal ? extent.GetWidth() : slidersz);
        auto height = (style.dimension.y > 0.f) ? style.dimension.y :
            (state.dir == DIR_Horizontal ? slidersz : extent.GetHeight());

        result.Min = extent.Min;
        result.Max = result.Min + ImVec2{ width, height };

        return result;
    }

    void HandleSliderEvent(WidgetContextData& context, int32_t id, const ImRect& extent, const ImRect& thumb, const IODescriptor& io, WidgetDrawResult& result)
    {
        if (!context.deferEvents)
        {
            auto& state = context.GetState(id).state.slider;
            const auto style = context.GetStyle(state.state, state.id);
            const auto& specificStyle = context.sliderStyles[log2((unsigned)state.state)].top();
            auto center = thumb.Min + ImVec2{ thumb.GetWidth(), thumb.GetWidth() };
            auto width = extent.GetWidth(), height = extent.GetHeight();
            auto horizontal = width > height;
            auto radius = thumb.GetWidth();

            auto offset = radius + specificStyle.thumbOffset + specificStyle.trackBorderThickness;
            auto inthumb = thumb.Contains(io.mousepos);

            if (io.isLeftMouseDown() && ((state.state & WS_Dragged) || inthumb))
            {
                // If the drag is starting for first time, consider the center of thumb
                // otherwise, follow mouse position
                auto where = (state.state & WS_Dragged) ? (horizontal ? io.mousepos.x : io.mousepos.y) :
                    (horizontal ? center.x : center.y);
                auto space = horizontal ? width - (2.f * offset) : height - (2.f * offset);
                where = horizontal ? where - extent.Min.x - offset : where - extent.Min.y - offset;
                auto relative = where / space;
                state.data = relative * (state.max - state.min);
                state.state |= WS_Dragged;
            }
            else if (extent.Contains(io.mousepos))
            {
                state.state |= WS_Hovered;

                if (io.clicked() && !inthumb)
                {
                    auto space = horizontal ? width - (2.f * offset) : height - (2.f * offset);
                    auto where = horizontal ? io.mousepos.x - extent.Min.x - offset : io.mousepos.y - extent.Min.y - offset;
                    auto relative = where / space;
                    state.data = relative * (state.max - state.min);
                    state.state &= ~WS_Dragged;
                }
            }
            else
            {
                state.state &= ~WS_Hovered;
                state.state &= ~WS_Dragged;
            }

            if (state.out)
            {
                switch (state.outType)
                {
                case OutPtrType::i32: *(int32_t*)(state.out) = (int32_t)state.data; break;
                case OutPtrType::f32: *(float*)(state.out) = state.data; break;
                case OutPtrType::f64: *(double*)(state.out) = state.data; break;
                default: break;
                }
            }

            if (extent.Contains(io.mousepos)) WidgetContextData::CurrentWidgetId = id;
            ShowTooltip(state._hoverDuration, extent, state.tooltip, io);
            HandleContextMenu(id, extent, io);

            WITH_WIDGET_LOG(id, extent);
            LOG_STATE(state.state);
            LOG_NUM(state.data); LOG_NUM(state.min); LOG_NUM(state.max);
            LOG_STYLE2(state.state, id);
        }
        else context.deferedEvents.emplace_back(EventDeferInfo::ForSlider(id, extent, thumb));
    }

    WidgetDrawResult SliderImpl(int32_t id, SliderState& state, const StyleDescriptor& style, 
        const ImRect& extent, IRenderer& renderer, const IODescriptor& io)
    {
        WidgetDrawResult result;
        auto& context = GetContext();
        const auto& specificStyle = context.sliderStyles[log2((unsigned)state.state)].top();

        auto bgcolor = state.TrackColor ? state.TrackColor(state.data) : style.bgcolor;
        DrawBackground(extent.Min, extent.Max, bgcolor, style.gradient, style.border, renderer);
        DrawBorderRect(extent.Min, extent.Max, style.border, bgcolor, renderer);

        auto width = extent.GetWidth(), height = extent.GetHeight();
        auto horizontal = state.dir == DIR_Horizontal;
        auto radius = ((horizontal ? height : width) * 0.5f) - specificStyle.thumbOffset - specificStyle.trackBorderThickness;
        auto relative = state.data / (state.max - state.min);
        auto offset = radius + specificStyle.thumbOffset + specificStyle.trackBorderThickness;
        ImVec2 center{ radius, radius };
        horizontal ? center.x += (width - (2.f * offset)) * relative : center.y += (height - (2.f * offset)) * relative;
        center += extent.Min + ImVec2{ offset - radius, offset - radius };

        DrawStyledShape(renderer, style, specificStyle.thumbColor, center, radius);
        DrawFocusRect(state.state, extent.Min, extent.Max, renderer);
        ImRect thumb{ center - ImVec2{ radius, radius }, center + ImVec2{ radius, radius } };
        HandleSliderEvent(context, id, extent, thumb,  io, result);
        result.geometry = extent;
        return result;
    }

    WidgetDrawResult Slider(int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        return Widget(id, WT_Slider, geometry, neighbors);
    }

    WidgetDrawResult Slider(int32_t* value, std::pair<int32_t, int32_t> range, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto id = GetIdFromOutPtr(value, WT_Slider).first;
        auto& config = CreateWidgetConfig(id).state.slider;
        config.data = (float)*value; config.out = value; config.outType = OutPtrType::i32;
        config.min = range.first; config.max = range.second;
        return Widget(id, WT_Slider, geometry, neighbors);
    }

    WidgetDrawResult Slider(std::string_view id, int32_t* value, std::pair<int32_t, int32_t> range, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto wid = GetIdFromString(id, WT_Slider).first;
        auto& config = CreateWidgetConfig(wid).state.slider;
        config.data = (float)*value; config.out = value; config.outType = OutPtrType::i32;
        config.min = range.first; config.max = range.second;
        return Widget(wid, WT_Slider, geometry, neighbors);
    }

    WidgetDrawResult Slider(float* value, std::pair<float, float> range, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto id = GetIdFromOutPtr(value, WT_Slider).first;
        auto& config = CreateWidgetConfig(id).state.slider;
        config.data = *value; config.out = value; config.outType = OutPtrType::f32;
        config.min = range.first; config.max = range.second;
        return Widget(id, WT_Slider, geometry, neighbors);
    }

    WidgetDrawResult Slider(std::string_view id, float* value, std::pair<float, float> range, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto wid = GetIdFromString(id, WT_Slider).first;
        auto& config = CreateWidgetConfig(wid).state.slider;
        config.data = *value; config.out = value; config.outType = OutPtrType::f32;
        config.min = range.first; config.max = range.second;
        return Widget(wid, WT_Slider, geometry, neighbors);
    }

    WidgetDrawResult Slider(double* value, std::pair<float, float> range, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto id = GetIdFromOutPtr(value, WT_Slider).first;
        auto& config = CreateWidgetConfig(id).state.slider;
        config.data = (float)*value; config.out = value; config.outType = OutPtrType::f64;
        config.min = range.first; config.max = range.second;
        return Widget(id, WT_Slider, geometry, neighbors);
    }

    WidgetDrawResult Slider(std::string_view id, double* value, std::pair<float, float> range, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto wid = GetIdFromString(id, WT_Slider).first;
        auto& config = CreateWidgetConfig(wid).state.slider;
        config.data = (float)*value; config.out = value; config.outType = OutPtrType::f64;
        config.min = range.first; config.max = range.second;
        return Widget(wid, WT_Slider, geometry, neighbors);
    }

#pragma endregion

#pragma region Range Slider

    ImRect RangeSliderBounds(int32_t id, const ImRect& extent)
    {
        ImRect result;
        auto& context = GetContext();
        auto& state = context.GetState(id).state.rangeSlider;
        const auto style = context.GetStyle(state.state, state.id);
        auto slidersz = std::max(Config.sliderSize, style.font.size);
        auto width = (style.dimension.x > 0.f) ? style.dimension.x :
            (state.dir == DIR_Horizontal ? extent.GetWidth() : slidersz);
        auto height = (style.dimension.y > 0.f) ? style.dimension.y :
            (state.dir == DIR_Horizontal ? slidersz : extent.GetHeight());
        result.Min = extent.Min;
        result.Max = result.Min + ImVec2{ width, height };
        return result;
    }

    void HandleRangeSliderEvent(WidgetContextData& context, int32_t id, const ImRect& extent, const ImRect& thumbMin, const ImRect& thumbMax, const IODescriptor& io, WidgetDrawResult& result)
    {
        if (!context.deferEvents)
        {
            auto& state = context.GetState(id).state.rangeSlider;
            const auto style = context.GetStyle(state.state, state.id);
            const auto& specificStyle = context.rangeSliderStyles[log2((unsigned)state.state)].top();
            auto width = extent.GetWidth(), height = extent.GetHeight();
            auto horizontal = width > height;
            auto radius = thumbMin.GetWidth();

            bool inMin = thumbMin.Contains(io.mousepos);
            bool inMax = thumbMax.Contains(io.mousepos);

            if (io.isLeftMouseDown() && ((state.minState & WS_Dragged) || inMin))
            {
                // Drag min thumb
                auto offset = radius + specificStyle.minThumb.offset + specificStyle.trackBorderThickness;
                auto space = horizontal ? width - specificStyle.offset() : height - specificStyle.offset();
                auto where = horizontal ? io.mousepos.x - extent.Min.x - offset : io.mousepos.y - extent.Min.y - offset;
                auto relative = clamp(where / space, 0.f, 1.f);
                float value = state.min_range + relative * (state.max_range - state.min_range);
                state.min_val = clamp(value, state.min_range, state.max_val);
                state.minState |= WS_Dragged;
                state.maxState &= ~WS_Dragged;
                state.state |= WS_Pressed;
            }
            else if (io.isLeftMouseDown() && ((state.maxState & WS_Dragged) || inMax))
            {
                // Drag max thumb
                auto offset = radius + specificStyle.maxThumb.offset + specificStyle.trackBorderThickness;
                auto space = horizontal ? width - specificStyle.offset() : height - specificStyle.offset();
                auto where = horizontal ? io.mousepos.x - extent.Min.x - offset : io.mousepos.y - extent.Min.y - offset;
                auto relative = clamp(where / space, 0.f, 1.f);
                float value = state.min_range + relative * (state.max_range - state.min_range);
                state.max_val = clamp(value, state.min_val, state.max_range);
                state.maxState |= WS_Dragged;
                state.minState &= ~WS_Dragged;
                state.state |= WS_Pressed;
            }
            else if (extent.Contains(io.mousepos))
            {
                if (io.clicked() && !inMin && !inMax)
                {
                    // Click on track: move closest thumb
                    auto isForMin = horizontal ? (std::abs(io.mousepos.x - thumbMin.Max.x) >
                        std::abs(io.mousepos.x - thumbMax.Min.x)) || (io.mousepos.x < thumbMin.Min.x) :
                        (std::abs(io.mousepos.y - thumbMin.Max.y) >
                            std::abs(io.mousepos.y - thumbMax.Min.y)) || (io.mousepos.y < thumbMin.Min.y);

                    if (isForMin)
                    {
                        auto offset = specificStyle.minThumb.offset + radius + specificStyle.trackBorderThickness;
                        auto space = horizontal ? width - specificStyle.offset() : height - specificStyle.offset();
                        auto where = horizontal ? io.mousepos.x - extent.Min.x - offset : io.mousepos.y - extent.Min.y - offset;
                        auto relative = where / space;
                        float value = state.min_range + relative * (state.max_range - state.min_range);
                        state.min_val = clamp(value, state.min_range, state.max_val);
                    }
                    else
                    {
                        auto offset = specificStyle.maxThumb.offset + radius + specificStyle.trackBorderThickness;
                        auto space = horizontal ? width - specificStyle.offset() : height - specificStyle.offset();
                        auto where = horizontal ? io.mousepos.x - extent.Min.x - offset : io.mousepos.y - extent.Min.y - offset;
                        auto relative = where / space;
                        float value = state.min_range + relative * (state.max_range - state.min_range);
                        state.max_val = clamp(value, state.min_val, state.max_range);
                    }

                    state.minState &= ~WS_Dragged;
                    state.maxState &= ~WS_Dragged;
                }
                else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                {

                }

                state.state = WS_Hovered | WS_Default;
            }
            else
            {
                state.state = WS_Default;
            }

            // Output
            if (state.out_min && state.outType == OutPtrType::i32) *(int32_t*)(state.out_min) = (int32_t)state.min_val;
            if (state.out_max && state.outType == OutPtrType::i32) *(int32_t*)(state.out_max) = (int32_t)state.max_val;
            if (state.out_min && state.outType == OutPtrType::f32) *(float*)(state.out_min) = state.min_val;
            if (state.out_max && state.outType == OutPtrType::f32) *(float*)(state.out_max) = state.max_val;
            if (state.out_min && state.outType == OutPtrType::f64) *(double*)(state.out_min) = state.min_val;
            if (state.out_max && state.outType == OutPtrType::f64) *(double*)(state.out_max) = state.max_val;

            if (extent.Contains(io.mousepos)) WidgetContextData::CurrentWidgetId = id;
            ShowTooltip(state._hoverDuration, extent, state.tooltip, io);
            HandleContextMenu(id, extent, io);

            WITH_WIDGET_LOG(id, extent);
            LOG_STATE(state.state);
            LOG_NUM(state.min_val); LOG_NUM(state.max_val); LOG_NUM(state.min_range); LOG_NUM(state.max_range);
            LOG_STYLE2(state.state, id);
        }
        else context.deferedEvents.emplace_back(EventDeferInfo::ForRangeSlider(id, extent, thumbMin, thumbMax));
    }

    WidgetDrawResult RangeSliderImpl(int32_t id, RangeSliderState& state, const StyleDescriptor& style,
        const ImRect& extent, IRenderer& renderer, const IODescriptor& io)
    {
        WidgetDrawResult result;
        auto& context = GetContext();
        const auto& specificStyle = context.rangeSliderStyles[log2((unsigned)state.state)].top();

        // Track color based on average value
        auto bgcolor = state.TrackColor ? state.TrackColor((state.min_val + state.max_val) * 0.5f) : style.bgcolor;
        DrawBackground(extent.Min, extent.Max, bgcolor, style.gradient, style.border, renderer);
        DrawBorderRect(extent.Min, extent.Max, style.border, bgcolor, renderer);

        auto width = extent.GetWidth(), height = extent.GetHeight();
        auto horizontal = state.dir == DIR_Horizontal;
        auto minradius = ((horizontal ? height : width) * 0.5f) - specificStyle.minThumb.offset - specificStyle.trackBorderThickness;
        auto maxradius = ((horizontal ? height : width) * 0.5f) - specificStyle.maxThumb.offset - specificStyle.trackBorderThickness;
        auto minoffset = minradius + specificStyle.minThumb.offset + specificStyle.trackBorderThickness;
        auto maxoffset = maxradius + specificStyle.maxThumb.offset + specificStyle.trackBorderThickness;
        auto space = horizontal ? width - specificStyle.offset() : height - specificStyle.offset();

        // Relative positions for min/max
        float relMin = (state.min_val - state.min_range) / (state.max_range - state.min_range);
        float relMax = (state.max_val - state.min_range) / (state.max_range - state.min_range);

        ImVec2 centerMin{ minradius, minradius }, centerMax{ maxradius, maxradius };
        if (horizontal) 
        {
            centerMin.x += space * relMin;
            centerMax.x += space * relMax;
        }
        else 
        {
            centerMin.y += space * relMin;
            centerMax.y += space * relMax;
        }

        centerMin += extent.Min + ImVec2{ specificStyle.minThumb.offset - minradius, specificStyle.minThumb.offset - minradius };
        centerMax += extent.Min + ImVec2{ specificStyle.maxThumb.offset - maxradius, specificStyle.maxThumb.offset - maxradius };

        // Draw track between thumbs
        ImVec2 trackStart = centerMin, trackEnd = centerMax;
        if (horizontal) trackStart.y = trackEnd.y = centerMin.y;
        else trackStart.x = trackEnd.x = centerMin.x;
        renderer.DrawRect(trackStart, trackEnd, specificStyle.trackColor, true);

        // Draw thumbs
        DrawStyledShape(renderer, style, specificStyle.minThumb.color, centerMin, minradius);
        DrawStyledShape(renderer, style, specificStyle.maxThumb.color, centerMax, maxradius);
        DrawFocusRect(state.state, extent.Min, extent.Max, renderer);

        ImRect thumbMin{ centerMin - ImVec2{ minradius, minradius }, centerMin + ImVec2{ minradius, minradius } };
        ImRect thumbMax{ centerMax - ImVec2{ maxradius, maxradius }, centerMax + ImVec2{ maxradius, maxradius } };

        HandleRangeSliderEvent(context, id, extent, thumbMin, thumbMax, io, result);
        result.geometry = extent;
        return result;
    }

    WidgetDrawResult RangeSlider(int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        return Widget(id, WT_RangeSlider, geometry, neighbors);
    }

    WidgetDrawResult RangeSlider(int32_t* min_val, int32_t* max_val, std::pair<int32_t, int32_t> range, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto id = GetIdFromOutPtr(min_val, WT_RangeSlider).first;
        auto& config = CreateWidgetConfig(id).state.rangeSlider;
        config.min_val = (float)*min_val;
        config.max_val = (float)*max_val;
        config.out_min = min_val;
        config.out_max = max_val;
        config.outType = OutPtrType::i32;
        config.min_range = (float)range.first;
        config.max_range = (float)range.second;
        return Widget(id, WT_RangeSlider, geometry, neighbors);
    }

    WidgetDrawResult RangeSlider(std::string_view id, int32_t* min_val, int32_t* max_val, std::pair<int32_t, int32_t> range, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto wid = GetIdFromString(id, WT_RangeSlider).first;
        auto& config = CreateWidgetConfig(wid).state.rangeSlider;
        config.min_val = (float)*min_val;
        config.max_val = (float)*max_val;
        config.out_min = min_val;
        config.out_max = max_val;
        config.outType = OutPtrType::i32;
        config.min_range = (float)range.first;
        config.max_range = (float)range.second;
        return Widget(wid, WT_RangeSlider, geometry, neighbors);
    }

    WidgetDrawResult RangeSlider(float* min_val, float* max_val, std::pair<float, float> range, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto id = GetIdFromOutPtr(min_val, WT_RangeSlider).first;
        auto& config = CreateWidgetConfig(id).state.rangeSlider;
        config.min_val = *min_val;
        config.max_val = *max_val;
        config.out_min = min_val;
        config.out_max = max_val;
        config.outType = OutPtrType::f32;
        config.min_range = range.first;
        config.max_range = range.second;
        return Widget(id, WT_RangeSlider, geometry, neighbors);
    }

    WidgetDrawResult RangeSlider(std::string_view id, float* min_val, float* max_val, std::pair<float, float> range, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto wid = GetIdFromString(id, WT_RangeSlider).first;
        auto& config = CreateWidgetConfig(wid).state.rangeSlider;
        config.min_val = *min_val;
        config.max_val = *max_val;
        config.out_min = min_val;
        config.out_max = max_val;
        config.outType = OutPtrType::f32;
        config.min_range = range.first;
        config.max_range = range.second;
        return Widget(wid, WT_RangeSlider, geometry, neighbors);
    }

    WidgetDrawResult RangeSlider(double* min_val, double* max_val, std::pair<float, float> range, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto id = GetIdFromOutPtr(min_val, WT_RangeSlider).first;
        auto& config = CreateWidgetConfig(id).state.rangeSlider;
        config.min_val = (float)*min_val;
        config.max_val = (float)*max_val;
        config.out_min = min_val;
        config.out_max = max_val;
        config.outType = OutPtrType::f64;
        config.min_range = range.first;
        config.max_range = range.second;
        return Widget(id, WT_RangeSlider, geometry, neighbors);
    }

    WidgetDrawResult RangeSlider(std::string_view id, double* min_val, double* max_val, std::pair<float, float> range, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto wid = GetIdFromString(id, WT_RangeSlider).first;
        auto& config = CreateWidgetConfig(wid).state.rangeSlider;
        config.min_val = (float)*min_val;
        config.max_val = (float)*max_val;
        config.out_min = min_val;
        config.out_max = max_val;
        config.outType = OutPtrType::f64;
        config.min_range = range.first;
        config.max_range = range.second;
        return Widget(wid, WT_RangeSlider, geometry, neighbors);
    }

#pragma endregion

#pragma region TextInput

    static void UpdatePosition(const TextInputState& state, int index, InputTextPersistentState& input, const StyleDescriptor& style, IRenderer& renderer)
    {
        for (auto idx = index; idx < (int)state.text.size(); ++idx)
        {
            auto sz = renderer.GetTextSize(std::string_view{ state.text.data() + idx, 1 }, style.font.font, style.font.size).x;
            input.pixelpos[idx] = sz + (idx > 0 ? input.pixelpos[idx - 1] : 0.f);
        }
    }

    static void RemoveCharAt(int position, TextInputState& state, InputTextPersistentState& input)
    {
        auto diff = input.pixelpos[position] - (position == 0 ? 0.f : input.pixelpos[position - 1]);
        auto& op = input.ops.push();
        op.type = TextOpType::Deletion;
        op.opmem[0] = state.text[position - 1];
        op.opmem[1] = 0;
        op.caretpos = input.caretpos;
        op.range = std::make_pair(position - 1, 1);

        for (auto idx = position; idx < (int)state.text.size(); ++idx)
        {
            state.text[idx - 1] = state.text[idx];
            input.pixelpos[idx - 1] -= diff;
        }

        input.scroll.state.pos.x = std::max(0.f, input.scroll.state.pos.x - diff);
        state.text.pop_back();
        input.pixelpos.pop_back(true);
    }

    static void ClearAllText(TextInputState& state, InputTextPersistentState& input)
    {
        auto& op = input.ops.push();
        auto selectionsz = (int)state.text.size();
        op.type = TextOpType::Deletion;
        op.range = std::make_pair(0, selectionsz);
        op.caretpos = input.caretpos;
        memcpy(op.opmem, state.text.data(), selectionsz);
        op.opmem[selectionsz] = 0;

        state.text.clear();
        input.pixelpos.clear(true);

        input.scroll.state.pos.x = 0.f;
        input.caretpos = 0;
        state.selection.first = state.selection.second = -1;
        input.selectionStart = -1.f;
    }

    static void DeleteSelectedText(TextInputState& state, InputTextPersistentState& input)
    {
        auto from = std::max(state.selection.first, state.selection.second) + 1,
            to = std::min(state.selection.first, state.selection.second);

        if (from - to >= (int)state.text.size())
            ClearAllText(state, input);
        else
        {
            float shift = input.pixelpos[from - 1] - input.pixelpos[to - 1];
            auto textsz = (int)state.text.size();

            auto& op = input.ops.push();
            auto selectionsz = to - from;
            op.type = TextOpType::Deletion;
            op.range = std::make_pair(from, selectionsz);
            op.caretpos = input.caretpos;
            memcpy(op.opmem, state.text.data() + to, selectionsz);
            op.opmem[selectionsz] = 0;

            for (; from < textsz; ++from, ++to)
            {
                state.text[to] = state.text[from];
                input.pixelpos[to] = input.pixelpos[from] - shift;
            }

            for (auto idx = to; idx < textsz; ++idx)
            {
                state.text.pop_back();
                input.pixelpos.pop_back(true);
            }

            input.scroll.state.pos.x = std::max(0.f, input.scroll.state.pos.x - shift);
            input.caretpos = std::min(state.selection.first, state.selection.second);
            state.selection.first = state.selection.second = -1;
            input.selectionStart = -1.f;
        }
    }

    void HandleTextInputEvent(WidgetContextData& context, int32_t id, const ImRect& content, const ImRect& suffix, const IODescriptor& io,
        IRenderer& renderer, WidgetDrawResult& result)
    {
        if (!context.deferEvents)
        {
            auto& state = context.GetState(id).state.input;
            auto& input = context.InputTextState(id);
            auto style = context.GetStyle(state.state, state.id);

            auto mousepos = io.mousepos;
            auto mouseover = content.Contains(mousepos) || (state.state & WS_Pressed);
            auto ispressed = mouseover && io.isLeftMouseDown();
            auto hasclick = io.clicked();
            auto isclicked = (hasclick && mouseover) || (!hasclick && (state.state & WS_Focused));
            mouseover ? state.state |= WS_Hovered : state.state &= ~WS_Hovered;
            ispressed ? state.state |= WS_Pressed : state.state &= ~WS_Pressed;
            isclicked ? state.state |= WS_Focused : state.state &= ~WS_Focused;
            if (input.lastClickTime != -1.f) input.lastClickTime += io.deltaTime;
            if (mouseover) WidgetContextData::CurrentWidgetId = id;

            if (mouseover) 
                Config.platform->SetMouseCursor(MouseCursor::TextInput);

            // When mouse is down inside the input, text selection is in progress
            // Hence, either text selection is starting, in which case record the start position.
            // If mouse gets released at the same position, it is a click and not a selection,
            // in which case, move the caret to the respective char position.
            // If mouse gets dragged, select the region of text
            if (state.state & WS_Pressed)
            {
                if (!state.text.empty() && mousepos.y < (content.Max.y - (1.5f * 5.f)) && state.isSelectable)
                {
                    auto posx = mousepos.x - content.Min.x;
                    if (input.selectionStart == -1.f) input.selectionStart = posx;
                    else if ((fabsf(input.selectionStart - posx) > 5.f) || input.isSelecting)
                    {
                        if (state.selection.first == -1)
                        {
                            auto it = std::lower_bound(input.pixelpos.begin(), input.pixelpos.end(), input.selectionStart + input.scroll.state.pos.x);
                            if (it != input.pixelpos.end())
                            {
                                auto idx = it - input.pixelpos.begin();
                                if (idx > 0 && (*it - posx) > 0.f) --idx;
                                state.selection.first = it - input.pixelpos.begin();
                                input.isSelecting = true;
                                input.caretVisible = false;
                                input.caretpos = state.selection.first + 1;
                            }
                        }

                        auto it = std::lower_bound(input.pixelpos.begin(), input.pixelpos.end(), posx + input.scroll.state.pos.x);

                        if (it != input.pixelpos.end())
                        {
                            auto idx = it - input.pixelpos.begin();
                            if (idx > 0 && (*it - posx) > 0.f) --idx;

                            auto prevpos = input.caretpos;
                            state.selection.second = it - input.pixelpos.begin();
                            input.caretpos = state.selection.second + 1;

                            if (state.selection.second > state.selection.first)
                            {
                                if ((prevpos < input.caretpos) && (input.pixelpos[input.caretpos - 1] - input.scroll.state.pos.x > content.GetWidth()))
                                {
                                    auto width = fabsf(input.pixelpos[input.caretpos - 1] - (input.caretpos > 1 ? input.pixelpos[input.caretpos - 2] : 0.f));
                                    input.moveRight(width);
                                }
                            }
                            else
                            {
                                if (prevpos > input.caretpos && (input.pixelpos[input.caretpos - 1] - input.scroll.state.pos.x < 0.f))
                                {
                                    auto width = fabsf(input.pixelpos[prevpos - 1] - (prevpos > 1 ? input.pixelpos[prevpos - 2] : 0.f));
                                    input.moveLeft(width);
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                if (!state.text.empty() && mousepos.y < (content.Max.y - (1.5f * 5.f)) && state.isSelectable)
                {
                    auto posx = mousepos.x - content.Min.x;

                    // This means we have clicked, not selecting text
                    if (fabsf(input.selectionStart - posx) < 5.f)
                    {
                        auto it = std::lower_bound(input.pixelpos.begin(), input.pixelpos.end(), posx + input.scroll.state.pos.x);
                        auto idx = it - input.pixelpos.begin();

                        // This is a double click, select entire content
                        if (IsBetween(input.lastClickTime, 0.f, 1.f) && !state.text.empty())
                        {
                            state.selection.first = 0;
                            state.selection.second = (int)state.text.size() - 1;
                            input.selectionStart = -1.f;
                            input.caretVisible = false;
                            input.lastClickTime = -1.f;
                            result.event = WidgetEvent::Selected;
                        }
                        else
                        {
                            input.caretVisible = true;
                            state.selection.first = state.selection.second = -1;
                            input.caretpos = idx;
                            input.isSelecting = false;
                            input.selectionStart = -1.f;
                            input.lastClickTime = 0.f;
                            result.event = WidgetEvent::Focused;
                        }
                    }
                    else if (input.selectionStart != -1.f)
                    {
                        if (state.selection.first == -1)
                        {
                            auto it = std::lower_bound(input.pixelpos.begin(), input.pixelpos.end(), input.selectionStart + input.scroll.state.pos.x);
                            if (it != input.pixelpos.end())
                            {
                                auto idx = it - input.pixelpos.begin();
                                if (idx > 0 && (*it - posx) > 0.f) --idx;
                                state.selection.first = it - input.pixelpos.begin();
                                input.isSelecting = true;
                                input.caretVisible = false;
                            }
                        }

                        auto it = std::lower_bound(input.pixelpos.begin(), input.pixelpos.end(), posx + input.scroll.state.pos.x);

                        if (it != input.pixelpos.end())
                        {
                            auto idx = it - input.pixelpos.begin();
                            if (idx > 0 && (*it - posx) > 0.f) --idx;

                            state.selection.second = it - input.pixelpos.begin();
                            result.event = WidgetEvent::Selected;
                            input.caretVisible = false;
                            input.isSelecting = false;
                            input.caretpos = state.selection.second + 1;
                        }
                    }

                    input.selectionStart = -1.f;
                }

                if (state.state & WS_Focused)
                {
                    Config.platform->ToggleTextInputMode(true);

                    if (input.lastCaretShowTime > 0.5f && state.selection.second == -1)
                    {
                        input.caretVisible = !input.caretVisible;
                        input.lastCaretShowTime = 0.f;
                    }
                    else input.lastCaretShowTime += io.deltaTime;

                    for (auto kidx = 0; io.key[kidx] != Key_Invalid; ++kidx)
                    {
                        auto key = io.key[kidx];
                        input.lastCaretShowTime = 0.f;
                        input.caretVisible = true;

                        if (key == Key_LeftArrow)
                        {
                            auto prevpos = input.caretpos;

                            if (io.modifiers & ShiftKeyMod)
                            {
                                if (state.isSelectable)
                                    if (state.selection.second == -1)
                                    {
                                        input.selectionStart = input.pixelpos[input.caretpos];
                                        state.selection.first = input.caretpos;
                                        state.selection.second = input.caretpos;
                                    }
                                    else
                                        state.selection.second = std::max(state.selection.second - 1, 0);

                                input.caretpos = std::max(input.caretpos - 1, 0);
                                input.caretVisible = false;
                            }
                            else input.caretpos = std::max(input.caretpos - 1, 0);

                            if (prevpos > input.caretpos && (input.pixelpos[input.caretpos - 1] - input.scroll.state.pos.x < 0.f))
                            {
                                auto width = fabsf(input.pixelpos[prevpos - 1] - (prevpos > 1 ? input.pixelpos[prevpos - 2] : 0.f));
                                input.moveLeft(width);
                            }
                        }
                        else if (key == Key_RightArrow)
                        {
                            auto prevpos = input.caretpos;

                            if (io.modifiers & ShiftKeyMod)
                            {
                                if (state.isSelectable)
                                    if (state.selection.second == -1)
                                    {
                                        input.selectionStart = input.pixelpos[input.caretpos];
                                        state.selection.first = input.caretpos;
                                        state.selection.second = input.caretpos;
                                    }
                                    else
                                        state.selection.second = std::min(state.selection.second + 1, (int)state.text.size() - 1);

                                input.caretpos = std::min(input.caretpos + 1, (int)state.text.size());
                                input.caretVisible = false;
                            }
                            else input.caretpos = std::min(input.caretpos + 1, (int)state.text.size());

                            if ((prevpos < input.caretpos) && (input.pixelpos[input.caretpos - 1] - input.scroll.state.pos.x > content.GetWidth()))
                            {
                                auto width = fabsf(input.pixelpos[input.caretpos - 1] - (input.caretpos > 1 ? input.pixelpos[input.caretpos - 2] : 0.f));
                                input.moveRight(width);
                            }
                        }
                        else if (key == Key_Backspace)
                        {
                            if (state.selection.second == -1)
                            {
                                std::string_view text{ state.text.data(), state.text.size() };
                                auto caretAtEnd = input.caretpos == (int)text.size();
                                if (text.empty()) continue;

                                auto& op = input.ops.push();
                                op.type = TextOpType::Deletion;
                                op.opmem[0] = state.text[input.caretpos - 1];
                                op.opmem[1] = 0;
                                op.range = std::make_pair(input.caretpos - 1, 1);
                                op.caretpos = input.caretpos;

                                if (caretAtEnd)
                                {
                                    if (input.scroll.state.pos.x != 0.f)
                                    {
                                        auto width = input.pixelpos.back() - input.pixelpos[input.pixelpos.size() - 2];
                                        input.moveLeft(width);
                                    }

                                    state.text.pop_back();
                                    input.pixelpos.pop_back(true);
                                }
                                else RemoveCharAt(input.caretpos, state, input);

                                input.caretpos--;
                            }
                            else DeleteSelectedText(state, input);

                            result.event = WidgetEvent::Edited;
                        }
                        else if (key == Key_Delete)
                        {
                            if (state.selection.second == -1)
                            {
                                std::string_view text{ state.text.data(), state.text.size() };
                                auto caretAtEnd = input.caretpos == (int)text.size();
                                if (text.empty()) continue;

                                if (!caretAtEnd) RemoveCharAt(input.caretpos + 1, state, input);
                            }
                            else DeleteSelectedText(state, input);

                            result.event = WidgetEvent::Edited;
                        }
                        else if (key == Key_Space || (key >= Key_0 && key <= Key_Z) ||
                            (key >= Key_Apostrophe && key <= Key_GraveAccent) ||
                            (key >= Key_Keypad0 && key <= Key_KeypadEqual))
                        {
                            if (key == Key_V && io.modifiers & CtrlKeyMod)
                            {
                                auto content = Config.platform->GetClipboardText();
                                auto length = (int)content.size();

                                if (length > 0)
                                {
                                    auto caretAtEnd = input.caretpos == (int)state.text.size();
                                    input.pixelpos.expand(length, 0.f);

                                    if (caretAtEnd)
                                    {
                                        for (auto idx = 0; idx < length; ++idx)
                                        {
                                            state.text.push_back(content[idx]);
                                            auto sz = renderer.GetTextSize(std::string_view{ content.data() + idx, 1 }, style.font.font, style.font.size).x;
                                            input.pixelpos.push_back(sz + (state.text.size() > 1u ? input.pixelpos.back() : 0.f));
                                        }
                                    }
                                    else
                                    {
                                        for (auto idx = (int)state.text.size() - 1; idx >= input.caretpos; --idx)
                                            state.text[idx] = state.text[idx - length];
                                        for (auto idx = 0; idx < length; ++idx)
                                            state.text[idx + input.caretpos] = content[idx];

                                        UpdatePosition(state, input.caretpos, input, style, renderer);
                                    }

                                    auto& op = input.ops.push();
                                    op.type = TextOpType::Addition;
                                    op.range = std::make_pair(input.caretpos, std::min(length, 127));
                                    memcpy(op.opmem, content.data(), std::min(length, 127));
                                    op.opmem[length] = 0;

                                    input.caretpos += length;
                                    result.event = WidgetEvent::Edited;
                                }
                            }
                            else if (key == Key_C && (io.modifiers & CtrlKeyMod) && state.selection.second != -1)
                            {
                                CopyToClipboard(state.text, state.selection.first, state.selection.second);
                            }
                            else if (key == Key_X && (io.modifiers & CtrlKeyMod) && state.selection.second != -1)
                            {
                                CopyToClipboard(state.text, state.selection.first, state.selection.second);
                                DeleteSelectedText(state, input);
                            }
                            else if (key == Key_A && (io.modifiers & CtrlKeyMod))
                            {
                                if (!state.text.empty())
                                {
                                    state.selection.first = 0;
                                    state.selection.second = (int)state.text.size() - 1;
                                    input.selectionStart = -1.f;
                                    input.caretVisible = false;
                                }
                            }
                            else if (key == Key_Z && (io.modifiers & CtrlKeyMod))
                            {
                                if (!input.ops.empty())
                                {
                                    auto op = input.ops.undo().value();

                                    switch (op.type)
                                    {
                                    case TextOpType::Deletion:
                                    {
                                        auto length = op.range.second;
                                        input.pixelpos.expand(length, 0.f);
                                        state.text.resize(state.text.size() + length);

                                        for (auto idx = (int)state.text.size() - 1; idx >= op.range.first; --idx)
                                            state.text[idx] = state.text[idx - length];
                                        for (auto idx = 0; idx < length; ++idx)
                                            state.text[idx + op.range.first] = op.opmem[idx];

                                        UpdatePosition(state, op.range.first, input, style, renderer);
                                        input.caretpos = op.caretpos;
                                        break;
                                    }
                                    default:
                                        break;
                                    }
                                }
                            }
                            else
                            {
                                std::string_view text{ state.text.data(), state.text.size() };
                                auto ch = io.modifiers & ShiftKeyMod ? KeyMappings[key].second : KeyMappings[key].first;
                                ch = io.capslock ? std::toupper(ch) : std::tolower(ch);
                                auto caretAtEnd = input.caretpos == (int)text.size();

                                if (caretAtEnd)
                                {
                                    state.text.push_back(ch);
                                    std::string_view newtext{ state.text.data(), state.text.size() };
                                    auto lastpos = (int)state.text.size() - 1;
                                    auto width = renderer.GetTextSize(newtext.substr(lastpos, 1), style.font.font, style.font.size).x;
                                    auto nextw = width + (lastpos > 0 ? input.pixelpos[lastpos - 1] : 0.f);
                                    input.pixelpos.push_back(nextw);
                                    input.scroll.state.pos.x = std::max(0.f, input.pixelpos.back() - content.GetWidth());
                                }
                                else
                                {
                                    if (!io.insert)
                                    {
                                        state.text.push_back(0);
                                        input.pixelpos.push_back(0.f);
                                        for (auto from = (int)state.text.size() - 2; from >= input.caretpos; --from)
                                            state.text[from + 1] = state.text[from];
                                        state.text[input.caretpos] = (char)ch;
                                        UpdatePosition(state, input.caretpos, input, style, renderer);
                                    }
                                    else
                                    {
                                        // TODO: Position update is not required for monospace fonts, can optimize this
                                        state.text[input.caretpos] = (char)ch;
                                        UpdatePosition(state, input.caretpos, input, style, renderer);
                                    }
                                }

                                input.caretpos++;
                                result.event = WidgetEvent::Edited;
                            }
                        }
                    }

                    ShowTooltip(state._hoverDuration, content, state.tooltip, io);
                }
                else
                {
                    input.caretVisible = false;
                    Config.platform->ToggleTextInputMode(false);
                }
            }

            if (result.event == WidgetEvent::Edited && state.ShowList != nullptr)
            {
                ImRect maxrect{ { 0.f, 0.f }, context.WindowSize() };
                ImRect padding{ content.Min - ImVec2{ style.padding.left, style.padding.top }, 
                    content.Max + ImVec2{ style.padding.right, style.padding.bottom } };
                ImRect border{ padding.Min - ImVec2{ style.border.left.thickness, style.border.top.thickness },
                    padding.Max + ImVec2{ style.border.right.thickness, style.border.bottom.thickness } };
                auto maxw = maxrect.GetWidth(), maxh = maxrect.GetHeight();
                ImVec2 available1 = ImVec2{ maxw - border.Min.x, maxh - padding.Max.y };
                ImVec2 available2 = ImVec2{ maxw - border.Min.x, maxh - padding.Min.y };

                // Create the popup with its own context                
                if (BeginPopup(id, { border.Min.x, padding.Max.y }, { border.GetWidth(), state.overlayHeight }))
                {
                    state.ShowList(state, available1, available2);
                    EndPopUp();
                }
            }

            if (!state.text.empty())
            {
                if (input.pixelpos.size() < state.text.size())
                {
                    auto index = input.pixelpos.size();
                    input.pixelpos.expand_and_create(state.text.size() - index, true);
                    UpdatePosition(state, index, input, style, renderer);
                    input.caretpos = state.text.size();
                }

                input.scroll.content.x = input.pixelpos.back();
                input.scroll.viewport = content;
                HandleHScroll(input.scroll, renderer, io, 5.f, false);
            }

            if (suffix.Contains(io.mousepos) && (input.suffixState != WS_Disabled))
            {
                input.suffixState |= io.isLeftMouseDown() ? WS_Pressed | WS_Hovered : WS_Hovered;
                if (io.clicked()) ClearAllText(state, input);
            }
            else
                input.suffixState = state.text.empty() ? WS_Disabled : WS_Default;

            if (state.out.source)
            {
                memset(state.out.source, 0, state.out.size());
                memcpy(state.out.source, state.text.data(), std::min((int)state.text.size(), state.out.size()));
            }

            HandleContextMenu(id, content, io);

            WITH_WIDGET_LOG(id, content);
            LOG_STATE(state.state);
            LOG_TEXT(state.text);
            LOG_STYLE2(state.state, id);
        }
        else context.deferedEvents.emplace_back(EventDeferInfo::ForTextInput(id, content, suffix));
    }

    WidgetDrawResult TextInputImpl(int32_t id, TextInputState& state, const StyleDescriptor& style, const ImRect& extent, const ImRect& text,
        const ImRect& prefix, const ImRect& suffix, IRenderer& renderer, const IODescriptor& io)
    {
        WidgetDrawResult result;
        ImRect clear;
        auto& context = GetContext();
        auto& input = context.InputTextState(id);
        const auto& config = context.GetState(id).state.input;
        ImRect content = text;
        content.Min.x += prefix.GetWidth();

        if (state.state & WS_Focused)
            renderer.DrawRect(extent.Min, extent.Max, Config.focuscolor, false, 2.f);

        DrawBackground(extent.Min, extent.Max, style, renderer);
        DrawBorderRect(extent.Min, extent.Max, style.border, style.bgcolor, renderer);
        renderer.SetCurrentFont(style.font.font, style.font.size);

        // Add prefix/suffix
        renderer.DrawResource(config.prefixType, prefix.Min, prefix.GetSize(), style.fgcolor, config.prefix);
        renderer.DrawResource(config.suffixType, suffix.Min, suffix.GetSize(), style.fgcolor, config.suffix);
        
        if (config.suffixIcon != SymbolIcon::None)
        {
            auto left = !config.suffix.empty() ? suffix.Min.x - style.font.size : content.Max.x - style.font.size;
            auto btnsz = style.font.size * 0.25f;
            clear = ImRect{ { left, content.Min.y }, { left + style.font.size, content.Max.y } };

            if (input.suffixState & WS_Hovered)
            {
                auto radius = style.font.size * 0.5f;
                ImVec2 center{ left + radius, clear.Min.y + radius };
                renderer.DrawCircle(center, radius, ToRGBA(200, 0, 0), true);
                DrawSymbol(clear.Min, clear.GetSize(), { btnsz, btnsz }, config.suffixIcon, ToRGBA(255, 255, 255), 0, 2.f, renderer);
            }
            else DrawSymbol(clear.Min, clear.GetSize(), { btnsz, btnsz }, config.suffixIcon, style.fgcolor, 0, 2.f, renderer);

            content.Max.x -= style.font.size;
        }

        if (state.text.empty() && !(state.state & WS_Focused))
        {
            auto phstyle = style;
            auto [fr, fg, fb, fa] = DecomposeColor(phstyle.fgcolor);
            fa = 150;
            phstyle.fgcolor = ToRGBA(fr, fg, fb, fa);
            auto sz = renderer.GetTextSize(state.placeholder, style.font.font, style.font.size);
            DrawText(content.Min, content.Max, { content.Min, content.Min + sz }, state.placeholder, state.state & WS_Disabled,
                phstyle, renderer, FontStyleOverflowMarquee | TextIsPlainText);
        }
        else
        { 
            content.Max.x -= suffix.GetWidth();
            renderer.SetClipRect(content.Min, content.Max);
            static char buffer[256];

            if (state.isMasked)
            {
                memset(buffer, 0, 256);
                for (auto i = 0; i < 255; i += (int)state.maskchar.size())
                    memcpy(buffer + i, state.maskchar.data(), state.maskchar.size());
            }

            if (state.selection.second != -1)
            {
                std::string_view text{ state.isMasked ? buffer : state.text.data(), 
                    state.isMasked ? std::min((std::size_t)255, state.text.size()) : state.text.size() };
                auto selection = state.selection;
                selection = { std::max(0, std::min(state.selection.first, state.selection.second)),
                    std::min(std::max(state.selection.first, state.selection.second), (int32_t)text.size()) };
                std::string_view parts[3] = { text.substr(0, selection.first),
                    text.substr(selection.first, selection.second - selection.first + 1),
                    text.substr(selection.second + 1) };
                ImVec2 startpos{ content.Min.x - input.scroll.state.pos.x, content.Min.y }, textsz;

                if (!parts[0].empty())
                {
                    textsz = renderer.GetTextSize(parts[0], style.font.font, style.font.size);
                    renderer.DrawText(parts[0], startpos, style.fgcolor);
                    startpos.x += textsz.x;
                }

                const auto& selstyle = context.GetStyle(WS_Selected, state.id);
                textsz = renderer.GetTextSize(parts[1], style.font.font, style.font.size);
                renderer.DrawRect(startpos, startpos + textsz, selstyle.bgcolor, true);
                renderer.DrawText(parts[1], startpos, selstyle.fgcolor);
                startpos.x += textsz.x;

                if (!parts[2].empty())
                {
                    textsz = renderer.GetTextSize(parts[2], style.font.font, style.font.size);
                    renderer.DrawText(parts[2], startpos, style.fgcolor);
                }
            }
            else
            {
                ImVec2 startpos{ content.Min.x - input.scroll.state.pos.x, content.Min.y };
                std::string_view text{ state.isMasked ? buffer : state.text.data(),
                    state.isMasked ? std::min((std::size_t)255, state.text.size()) : state.text.size() };
                renderer.DrawText(text, startpos, style.fgcolor);
            }

            renderer.ResetClipRect();
        }

        if ((state.state & WS_Focused) && input.caretVisible)
        {
            auto isCaretAtEnd = input.caretpos == (int)state.text.size();
            auto offset = isCaretAtEnd && (input.scroll.state.pos.x == 0.f) ? 1.f : 0.f;
            auto cursorxpos = (!input.pixelpos.empty() ? input.pixelpos[input.caretpos - 1] - input.scroll.state.pos.x : 0.f) + offset;
            renderer.DrawLine(content.Min + ImVec2{ cursorxpos, 1.f }, content.Min + ImVec2{ cursorxpos, content.GetHeight() - 1.f }, style.fgcolor, 2.f);
        }

        DrawFocusRect(state.state, extent.Min, extent.Max, renderer);
        HandleTextInputEvent(context, id, content, clear, io, renderer, result);
        renderer.ResetFont();

        result.geometry = extent;
        return result;
    }

    WidgetDrawResult TextInput(int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        return Widget(id, WT_TextInput, geometry, neighbors);
    }

    WidgetDrawResult TextInput(char* out, int size, std::string_view placeholder, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto length = strlen(out);
        return TextInput(out, size, length, placeholder, geometry, neighbors);
    }

    WidgetDrawResult TextInput(std::string_view id, char* out, int size, std::string_view placeholder, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto length = strlen(out);
        return TextInput(id, out, size, length, placeholder, geometry, neighbors);
    }

    WidgetDrawResult TextInput(char* out, int size, int strlen, std::string_view placeholder, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto [id, initial] = GetIdFromOutPtr(out, WT_TextInput);
        auto& config = CreateWidgetConfig(id).state.input;
        config.placeholder = placeholder; config.out = Span<char>{ out, size };
        config.text.reserve(size); config.suffixIcon = SymbolIcon::Cross;
        config.text.assign(out, out + strlen);
        return Widget(id, WT_TextInput, geometry, neighbors);
    }

    WidgetDrawResult TextInput(std::string_view id, char* out, int size, int strlen, std::string_view placeholder, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto [wid, initial] = GetIdFromString(id, WT_TextInput);
        auto& config = CreateWidgetConfig(wid).state.input;
        config.placeholder = placeholder; config.out = Span<char>{ out, size };
        config.text.reserve(size); config.suffixIcon = SymbolIcon::Cross;
        config.text.assign(out, out + strlen);
        return Widget(wid, WT_TextInput, geometry, neighbors);
    }

#pragma endregion

#pragma region DropDown

    static void DrawItemBackground(IRenderer* renderer, const ImRect& geometry, uint32_t color)
    {
        renderer->BeginAdvance();
        renderer->DrawRect(geometry.Min, geometry.Max, color, true);
        renderer->EndAdvance();
    }

    void ShowDropDownOptions(WidgetContextData& parent, DropDownState& state, DropDownPersistentState& persistent, 
        int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding, const ImRect& content, 
        IRenderer& renderer)
    {
        if (persistent.context == nullptr) return;

        const auto& ddstyle = parent.dropdownStyles[log2((unsigned)state.state)].top();

        BEGIN_LOG_ARRAY("dropdown-items-geometry");
        ImVec2 maxsz = persistent.maxsz;
        maxsz.x += persistent.extra.x;
        persistent.context->popupOrigin = { margin.Min.x, margin.Max.y };
        auto maxw = 0.f;
        auto idx = 0;

        for (auto& item : persistent.items)
        {
            if (state.selected == idx)
                DrawItemBackground(persistent.context->deferedRenderer,
                    item.geometry, parent.dropdownStyles[WSI_Selected].top().optionBgColor);
            else if (state.hovered == idx)
                DrawItemBackground(persistent.context->deferedRenderer,
                    item.geometry, parent.dropdownStyles[WSI_Hovered].top().optionBgColor);

            item.geometry.Translate(persistent.context->popupOrigin);
            LOG_RECT(item.geometry);

            if (state.sizePolicy & DD_FitToLongestOption)
                maxw = std::max(maxw, item.geometry.GetWidth());

            ++idx;
        }

        if (maxw > maxsz.x)
            maxsz.x = maxw - persistent.extra.x;
        else
            maxsz.x -= persistent.extra.x;
        persistent.maxsz = maxsz;
        END_LOG_ARRAY();

        EndPopUp(*persistent.context, ddstyle.popupBgColor,
            POP_EnsureVisible | (ddstyle.occludeBg ? POP_Occlude : 0));
        PopNestedSource(persistent.context);

        idx = 0;
        state.hovered = -1;
		state.maxOptionSz = persistent.maxsz;
        auto io = Config.platform->CurrentIO(persistent.context);

        for (const auto& item : persistent.items)
        {
            if (item.geometry.Contains(io.mousepos))
            {
                Config.platform->SetMouseCursor(MouseCursor::Grab);
                state.hovered = idx;

                if (io.clicked())
                {
                    state.selected = idx;
                    state.opened = false;
                    state.selectedText = persistent.items[idx].text;
                    state.selectedTextType = persistent.items[idx].textType;
                    if (state.out) *state.out = idx;
                    WidgetContextData::RemovePopup();
                    break;
                }
            }

            ++idx;
        }
    }

    void HandleDropDownEvent(WidgetContextData& context, int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const IODescriptor& io, IRenderer& renderer, WidgetDrawResult& result)
    {
        if (!context.deferEvents)
        {
            auto& state = context.GetState(id).state.dropdown;
            auto& persistent = context.DropDownState(id);
            auto ismouseover = padding.Contains(io.mousepos);
            state.state = !ismouseover ? WS_Default :
                io.isLeftMouseDown() ? WS_Pressed | WS_Hovered : WS_Hovered;

            if (ismouseover)
            {
                Config.platform->SetMouseCursor(MouseCursor::Grab);
                WidgetContextData::CurrentWidgetId = id;
            }

            if (!state.opened && (state.state & WS_Focused) && io.isKeyPressed(Key::Key_Enter))
                state.opened = true;

            if (ismouseover && io.clicked())
            {
                result.event = WidgetEvent::Clicked; 
                state.opened = !state.opened;
            }
            else if (ismouseover && io.isLeftMouseDoubleClicked())
            {
                if (state.isComboBox)
                {
                    // TODO: Create text input here
                }
            }
            else if (!ismouseover && (io.clicked() || io.isKeyPressed(Key::Key_Escape)))
            {
                if (state.opened) WidgetContextData::ActivePopUpRegion = ImRect{};
                state.opened = false;
            }

            if (state.opened)
            {
                auto prev = state.selected;
                ShowDropDownOptions(context, state, persistent, id, margin, border, padding, content, renderer);
                if (state.selected != prev)
                {
                    result.event = WidgetEvent::Clicked;
                    result.optidx = state.selected;
                }
            }
            else
            {
                ShowTooltip(state._hoverDuration, padding, state.tooltip, io);
                HandleContextMenu(id, content, io);
            }

            WITH_WIDGET_LOG(id, margin);
            LOG_STATE(state.state);
            LOG_TEXT(state.text); LOG_NUM(state.opened); LOG_NUM(state.selected);
            LOG_STYLE2(state.state, id);
        }
        else context.deferedEvents.emplace_back(EventDeferInfo::ForDropDown(id, margin, border, padding, content));
    }

    WidgetDrawResult DropDownImpl(int32_t id, DropDownState& state, const StyleDescriptor& style, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io)
    {
        WidgetDrawResult result;
        auto& context = GetContext();
        const auto& config = context.GetState(id).state.dropdown;
        const auto& ddstyle = WidgetContextData::dropdownStyles[log2((unsigned)state.state)].top();

        DrawBoxShadow(border.Min, border.Max, style, renderer);
        DrawBackground(border.Min, border.Max, style, renderer);
        DrawBorderRect(border.Min, border.Max, style.border, style.bgcolor, renderer);

        if (!ddstyle.isIndicatorSuffix)
        {
            auto indicator = ImRect{ content.Min, content.Min + ImVec2{ style.font.size, style.font.size } };
            auto isz = style.font.size * ddstyle.indicatorScale;
            auto diff = (content.GetHeight() - isz) * 0.5f;

            indicator.Min += ImVec2{ diff, diff };
            indicator.Max -= ImVec2{ diff, diff };
            renderer.DrawResource(ddstyle.indicatorType, indicator.Min, indicator.GetSize(), style.fgcolor,
                ddstyle.indicators[state.opened]);
        }

        if (!(state.opened && state.isComboBox))
        {
            auto textcontent = config.selected != -1 ? config.selectedText : config.text;
            auto textype = config.selected != -1 ? config.selectedTextType : config.textType;
            auto txtflags = ToTextFlags(textype) | FontStyleOverflowMarquee;
            DrawText(content.Min, content.Max, text, textcontent, state.state & WS_Disabled, style, renderer, txtflags);
        }

        if (ddstyle.isIndicatorSuffix)
        {
            auto indicator = ImRect{ padding.Max - ImVec2{ style.font.size, style.font.size }, padding.Max };
            auto isz = style.font.size * ddstyle.indicatorScale;
            auto diff = (content.GetHeight() - isz) * 0.5f;

            indicator.Min += ImVec2{ diff, diff };
            indicator.Max -= ImVec2{ diff, diff };
            renderer.DrawResource(ddstyle.indicatorType, indicator.Min, indicator.GetSize(), style.fgcolor,
                ddstyle.indicators[state.opened]);
        }

        DrawFocusRect(state.state, border.Min, border.Max, renderer);
        HandleDropDownEvent(context, id, margin, border, padding, content, io, renderer, result);

        result.geometry = margin;
        return result;
    }

    static bool BeginDropDownImpl(WidgetContextData& context, DropDownState& config, int32_t id, std::string_view text, TextType type)
    {
		auto& persistent = context.DropDownState(id);
        persistent.items.clear(true);
        persistent.maxsz = ImVec2{};
        for (auto idx = 0; idx < WT_TotalTypes; ++idx)
            persistent.widgets[idx].clear(true);

        const auto style = context.GetStyle(config.state, id);
        persistent.extra = ImVec2{
            style.padding.h() + style.border.h() + style.margin.h(),
            style.padding.v() + style.border.v() + style.margin.v()
        };
        persistent.indicator = ImVec2{ style.font.size, style.font.size };

        // size comes from initial text
        if (config.sizePolicy & DD_FitToInitialContent)
            persistent.maxsz = GetTextSize(type, text, style.font, -1.f, *Config.renderer);

        if (config.opened)
        {
            BeginPopup(id, ImVec2{}, ImVec2{ FLT_MAX, FLT_MAX }, NestedContextSourceType::DropDownPopup);
            persistent.context = &GetContext();
            auto width = -1.f;

            if (config.sizePolicy & (DD_SetPopupWidthToInitial | DD_FitToInitialContent))
                width = persistent.maxsz.x + persistent.extra.x + style.font.size;

            memset(context.currentDropDown.idstr, 0, 255);
            auto idlen = snprintf(context.currentDropDown.idstr, 254, "dropdown-popup-%d", id);
            const auto& ddstyle = context.dropdownStyles[log2((unsigned)config.state)].top();
            PushStyle(WS_AllStates, StyleDescriptor{ ddstyle.popupBorder });
            BeginFlexRegion(std::string_view{ context.currentDropDown.idstr, (std::size_t)idlen }, DIR_Vertical, ddstyle.optionSpacing, 
                true, 0, { width, -1.f });
            return true;
        }

        return false;
    }

    bool BeginDropDown(int32_t id, std::string_view text, TextType type, int32_t spolicy, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto& context = GetContext();
        auto& config = CreateWidgetConfig(id).state.dropdown;
        config.text = text;
        config.textType = type;
        config.sizePolicy = spolicy;
        context.currentDropDown.geometry = geometry;
        context.currentDropDown.neighbors = neighbors;
        context.currentDropDown.id = id;
		return BeginDropDownImpl(context, config, id, text, type);
    }

    bool BeginDropDown(std::string_view id, std::string_view text, TextType type, int32_t spolicy, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto& context = GetContext();
        auto iid = GetIdFromString(id, WT_DropDown).first;
        auto& config = CreateWidgetConfig(iid).state.dropdown;
        config.text = text;
        config.textType = type;
        config.sizePolicy = spolicy;
        context.currentDropDown.geometry = geometry;
        context.currentDropDown.neighbors = neighbors;
        context.currentDropDown.id = iid;
        return BeginDropDownImpl(context, config, iid, text, type);
    }

    void BeginDropDownOption(std::string_view optionText, TextType type, bool isLongestOption)
    {
        auto& context = *GetContext().parentContext;
		auto& items = context.DropDownState(context.currentDropDown.id).items;
        auto& item = items.emplace_back();
        item.text = optionText;
        item.textType = type;
        if (isLongestOption) 
            context.currentDropDown.longestOption = items.size() - 1;
		PushStyle(WS_AllStates, "background-color: transparent;");
    }

    void AddDropDownOption(std::string_view optionText, TextType type, bool isLongestOption)
    {
        BeginDropDownOption(optionText, type, isLongestOption);

        static char idstr[255];
		memset(idstr, 0, 255);
        auto& context = *GetContext().parentContext;
        auto& builder = context.currentDropDown;
        auto& items = context.DropDownState(context.currentDropDown.id).items;
		auto state = context.GetState(builder.id).state.dropdown;
        const auto& defStyle = context.dropdownStyles[WSI_Default].top();
        const auto& hoverStyle = context.dropdownStyles[WSI_Hovered].top();
        const auto& pressedStyle = context.dropdownStyles[WSI_Pressed].top();

        auto idlen = snprintf(idstr, 254, "dropdown-%d-option-%d", builder.id, items.size() - 1);
		PushStyleFmt(WS_Default, "background-color: " COLOR_FMT "; color: " COLOR_FMT ";",
            COLOR_OUT(defStyle.optionBgColor), COLOR_OUT(defStyle.optionFgColor));
        PushStyleFmt(WS_Hovered, "background-color: " COLOR_FMT "; color: " COLOR_FMT ";",
            COLOR_OUT(hoverStyle.optionBgColor), COLOR_OUT(hoverStyle.optionFgColor));
        PushStyleFmt(WS_Pressed, "background-color: " COLOR_FMT "; color: " COLOR_FMT ";",
            COLOR_OUT(pressedStyle.optionBgColor), COLOR_OUT(pressedStyle.optionFgColor));

        const auto& config = context.GetState(builder.id).state.dropdown;
        auto geometry = ToBottom | ToRight;
        if (config.sizePolicy & (DD_SetPopupWidthToInitial | DD_FitToInitialContent))
            geometry |= ExpandH;

        Label(std::string_view{ idstr, (std::size_t)idlen }, optionText, type, geometry);
        PopStyle(1, WS_Default | WS_Hovered | WS_Pressed);

        EndDropDownOption();
    }

    void EndDropDownOption()
    {
        PopStyle(1, WS_AllStates);
    }

    WidgetDrawResult EndDropDown(int32_t* selection, std::optional<std::pair<std::string_view, TextType>> longestopt)
    {
		auto ddctx = GetContext().parentContext != nullptr ? GetContext().parentContext : &GetContext();
        auto& ddstate = ddctx->GetState(ddctx->currentDropDown.id).state.dropdown;
		auto& persistent = ddctx->DropDownState(ddctx->currentDropDown.id);

        if (GetContext().parentContext != nullptr)
        {
            if (ddstate.opened)
            {
                EndRegion();
                PopStyle(1, WS_AllStates);
                PopContext();
            }
        }
        
        ddstate.out = selection;

		if (ddstate.sizePolicy & DD_FitToLongestOption)
        {
            const auto style = ddctx->GetStyle(ddstate.state, ddctx->currentDropDown.id);

            if (ddstate.opened)
            {
                auto lidx = ddctx->currentDropDown.longestOption != -1 ? ddctx->currentDropDown.longestOption : 0;
                const auto& item = persistent.items[lidx];
                persistent.maxsz = GetTextSize(item.textType, item.text, style.font,
                    -1.f, *Config.renderer);
            }
			else if (longestopt.has_value())
                persistent.maxsz = GetTextSize(longestopt->second, longestopt->first, style.font,
                    -1.f, *Config.renderer);
            
            auto maxsz = GetTextSize(ddstate.textType, ddstate.text, style.font, -1.f, *Config.renderer);
            persistent.maxsz = ImMax(persistent.maxsz, maxsz);
        }

        ddstate.maxOptionSz = persistent.maxsz;
        return Widget(ddctx->currentDropDown.id, WT_DropDown, ddctx->currentDropDown.geometry, 
            ddctx->currentDropDown.neighbors);
    }

#pragma endregion

#pragma region TabBar

    ImRect TabBarBounds(int32_t id, ImRect content, IRenderer& renderer)
    {
        ImRect result;
        auto& context = GetContext();
        auto& state = context.TabBarState(id);
        const auto& currentTab = context.layoutStack.empty() ? context.currentTab : 
            context.layouts[context.layoutStack.top()].tabbar;
        const auto& config = context.GetState(id).state.tab;
        int16_t tabidx = 0, lastRowStart = 0;
        auto height = 0.f, width = 0.f;
        auto fontsz = 0.f;
        auto overflow = false;
        ImVec2 offset = result.Min = content.Min;

        if (config.direction == FD_Horizontal)
        {
            offset = { content.Max.x, content.Min.y };
            tabidx = 0;

            // Add tabs anchored to the end
            for (auto& item : currentTab.endItems)
            {
                auto& tab = state.endTabs[tabidx];
                const auto& specificStyle = context.tabBarStyles[log2((unsigned)tab.state)].top();
                const auto& style = item.style;
                auto txtsz = GetTextSize(item.nameType, item.name, style.font, tab.extent.GetWidth(), renderer);

                tab.extent.Max.x = offset.x;
                offset.x -= style.padding.right + style.border.right.thickness + style.margin.right;

                tab.text.Max.x = offset.x;
                offset.x -= txtsz.x;
                tab.text.Min.x = offset.x;
                tab.text.Min.y = content.Min.y + style.padding.top + style.border.top.thickness + style.margin.top;
                tab.text.Max.y = tab.text.Min.y + txtsz.y;
                offset.x -= specificStyle.btnspacing;

                if (!item.icon.empty())
                {
                    if (item.iconsz == ImVec2{})
                        item.iconsz = ImVec2{ txtsz.y, txtsz.y };
                    tab.icon.Max = ImVec2{ offset.x, tab.text.Min.y + item.iconsz.y };
                    tab.icon.Min = tab.icon.Max - item.iconsz;
                    offset.x -= item.iconsz.x;
                }

                offset.x -= style.padding.left + style.border.left.thickness + style.margin.left;
                tab.extent.Min = ImVec2{ offset.x, content.Min.y };
                tab.extent.Max.y = tab.extent.Min.y + style.padding.v() + style.border.v() + style.margin.v() +
                    std::max(item.iconsz.y, txtsz.y);
                offset.y = content.Min.y;
                ++tabidx;
            }

            // Add all created tabs
            content.Max.x = offset.x;
            offset = content.Min;
            tabidx = 0;

            for (auto& item : currentTab.items)
            {
                auto& tab = state.tabs[tabidx];
                const auto& style = item.style;
                const auto& specificStyle = context.tabBarStyles[log2((unsigned)tab.state)].top();
                auto txtsz = GetTextSize(item.nameType, item.name, style.font, tab.extent.GetWidth(), renderer);
                tab.extent.Min = offset;

                if (!item.icon.empty())
                {
                    if (item.iconsz == ImVec2{})
                        item.iconsz = ImVec2{ txtsz.y, txtsz.y };
                    tab.icon.Min = offset + ImVec2{ specificStyle.btnspacing, 0.f };
                    tab.icon.Max = tab.icon.Min + item.iconsz;
                    tab.text.Min.x = tab.icon.Max.x + specificStyle.btnspacing;
                    tab.text.Min.y = offset.y;
                    tab.text.Max = tab.text.Min + txtsz;
                    offset.x = tab.text.Min.x;
                }
                else
                {
                    tab.text.Min = offset;
                    tab.text.Max = tab.text.Min + txtsz;
                }

                switch (currentTab.sizing)
                {
                case TabBarItemSizing::Scrollable:
                {
                    auto txtorigin = result.Max.x;
                    offset.x += style.padding.h() + txtsz.x + specificStyle.spacing.x;
                    height = std::max(height, style.padding.v() + txtsz.y);
                    tab.extent.Max = ImVec2{ offset.x, offset.y + height };
                    break;
                }

                case TabBarItemSizing::DropDown:
                {
                    auto txtorigin = result.Max.x;
                    offset.x += style.padding.h() + txtsz.x + specificStyle.spacing.x;

                    if (txtorigin + txtsz.x > content.Max.x - txtsz.y)
                    {
                        tab.extent.Min = tab.extent.Max = ImVec2{};
                        overflow = true;
                    }
                    else
                    {
                        height = std::max(height, style.padding.v() + txtsz.y);
                        tab.extent.Max = ImVec2{ offset.x, offset.y + height };
                    }

                    break;
                }

                case TabBarItemSizing::MultiRow:
                    if (offset.x + txtsz.x > content.Max.x)
                    {
                        auto prevh = height;
                        auto leftover = style.padding.h() + txtsz.x - content.GetWidth();
                        auto toadd = leftover / (float)(tabidx - lastRowStart);

                        for (auto idx = tabidx; idx >= lastRowStart; --idx)
                        {
                            state.tabs[idx].extent.Max.x += toadd;
                            state.tabs[idx].extent.Max.y = state.tabs[idx].extent.Min.y + height;
                        }

                        offset.y += height + specificStyle.spacing.y;
                        offset.x = content.Min.x;
                        height = style.padding.v() + txtsz.y;
                        lastRowStart = tabidx;
                        state.lastRowStarty = offset.y;

                        // TODO: Expand size if expand tabs set to true
                    }
                    else
                    {
                        height = std::max(height, style.padding.v() + txtsz.y);
                        offset.x += txtsz.x + style.padding.h() + specificStyle.spacing.x;
                        tab.extent.Max.x = offset.x;
                    }
                    break;

                case TabBarItemSizing::ResizeToFit:
                    height = std::max(height, style.padding.v() + txtsz.y);
                    offset.x += txtsz.x + style.padding.h();
                    tab.extent.Max.x = offset.x;
                    break;

                default: break;
                }

                if (item.itemflags & TI_Pinnable)
                {
                    offset.x += specificStyle.btnspacing;
                    tab.pin.Min = ImVec2{ offset.x, offset.y + style.padding.top };
                    offset.x += specificStyle.btnsize * style.font.size;
                    tab.pin.Max = ImVec2{ offset.x, tab.pin.Min.y + style.font.size };
                }

                if (item.itemflags & TI_Closeable)
                {
                    offset.x += specificStyle.btnspacing;
                    tab.close.Min = ImVec2{ offset.x, offset.y + style.padding.top };
                    offset.x += specificStyle.btnsize * style.font.size;
                    tab.close.Max = ImVec2{ offset.x, tab.close.Min.y + style.font.size };
                }

                offset.x += style.padding.left;
                tab.extent.Max.x = offset.x;
                tab.extent.Max.y += style.padding.bottom;
                tab.text.Min = tab.extent.Min + ImVec2{ style.padding.left, style.padding.top };
                tab.text.Max = tab.text.Min + txtsz;
                offset.x += specificStyle.spacing.x;
                fontsz = std::max(fontsz, style.font.size);
                tabidx++;

                if (currentTab.sizing != TabBarItemSizing::MultiRow)
                    state.lastRowStarty = offset.y;
            }

            const auto& specificStyle = context.tabBarStyles[WSI_Default].top();

            if (currentTab.newTabButton)
            {
                const auto style = context.GetStyle(WS_Default, id);

                offset.x += specificStyle.spacing.x;
                state.create = ImRect{ offset, offset + ImVec2{ fontsz + style.padding.h(), fontsz + style.padding.v() } };
                offset.x += state.create.GetWidth();
            }

            if (overflow)
            {
                const auto style = context.GetStyle(WS_Default, id);

                if (config.addNavigationButtons)
                {
                    offset.x += specificStyle.spacing.x;
                    state.moveBackward = ImRect{ offset, offset + ImVec2{ fontsz + style.padding.h(), fontsz + style.padding.v() } };
                    offset.x += state.create.GetWidth();

                    offset.x += specificStyle.spacing.x;
                    state.moveForward = ImRect{ offset, offset + ImVec2{ fontsz + style.padding.h(), fontsz + style.padding.v() } };
                    offset.x += state.create.GetWidth();
                }

                offset.x += specificStyle.spacing.x;
                state.dropdown = ImRect{ offset, offset + ImVec2{ fontsz + style.padding.h(), fontsz + style.padding.v() } };
                offset.x += state.create.GetWidth();
            }

            if (currentTab.sizing == TabBarItemSizing::ResizeToFit)
            {
                auto extrah = content.GetWidth() - ((offset.x - content.Min.x) +
                    state.create.GetWidth() + state.dropdown.GetWidth() +
                    state.moveBackward.GetWidth() + state.moveForward.GetWidth());

                if ((extrah > 0.f) || config.expandTabs)
                {
                    auto cumulative = 0.f;
                    auto totalTabs = tabidx;
                    extrah /= (float)totalTabs;

                    for (auto idx = 0; idx < totalTabs; ++idx)
                    {
                        state.tabs[idx].extent.Min.x += cumulative;
                        state.tabs[idx].extent.Max.x += (cumulative + extrah);
                        state.tabs[idx].pin.TranslateX(cumulative + extrah);
                        state.tabs[idx].close.TranslateX(cumulative + extrah);
                        state.tabs[idx].text.TranslateX(cumulative);
                        cumulative += extrah;
                    }

                    if (currentTab.newTabButton)
                    {
                        const auto& last = state.tabs.back();
                        auto sz = state.create.GetSize();
                        state.create.Min.x = last.extent.Max.x + specificStyle.spacing.x;
                        state.create.Min.y = last.extent.Min.y;
                        state.create.Max = state.create.Min + sz;
                    }
                }
            }

            for (auto idx = tabidx - 1; idx >= lastRowStart; --idx)
                state.tabs[idx].extent.Max.y = state.tabs[idx].extent.Min.y + height;
        }
        else
        {
            assert(false); // Vertical tab bars not yet implemented
        }

        result.Max = state.tabs.back().extent.Max;
        return result;
    }

    void HandleTabBarEvent(WidgetContextData& context, int32_t id, const ImRect& content, const IODescriptor& io, IRenderer& renderer, WidgetDrawResult& result)
    {
        if (!context.deferEvents)
        {
            auto& state = context.TabBarState(id);
            auto tabidx = 0;
            state.hovered = InvalidTabIndex;
            if (content.Contains(io.mousepos)) WidgetContextData::CurrentWidgetId = id;
#ifndef GLIMMER_ENABLE_TESTING
            else if (state.tabBeingDragged == -1) return;
#endif

            if (state.expand.Contains(io.mousepos))
            {
                state.hovered = ExpandTabsIndex;

                if (io.clicked())
                {
                    state.expanded = !state.expanded;
                    result.event = WidgetEvent::Clicked;
                    result.tabtype = TabButtonType::ExpandTabs;
                    return;
                }
            }

            WITH_WIDGET_LOG(id, content);
            BEGIN_LOG_ARRAY("items");

            for (auto& tab : state.tabs)
            {
                auto& rect = tab.extent;
                const auto& style = tab.descriptor.style;
                auto done = false;
                
                if (tab.close.Contains(io.mousepos) &&
                    (state.tabBeingDragged == -1))
                {
                    result.event = io.clicked() ? WidgetEvent::Clicked : WidgetEvent::Hovered;
                    tab.closeState = io.isLeftMouseDown() ? WS_Pressed | WS_Hovered : WS_Hovered;
                    tab.pinState = WS_Default; tab.state = tab.state | WS_Hovered;
                    result.tabidx = tabidx;
                    result.tabtype = TabButtonType::CloseTab;
                    state.current = state.previous;
                    done = true;
                }
                else if (tab.pin.Contains(io.mousepos) &&
                    (state.tabBeingDragged == -1))
                {
                    result.event = io.clicked() ? WidgetEvent::Clicked : WidgetEvent::Hovered;
                    tab.pinState = io.isLeftMouseDown() ? WS_Pressed | WS_Hovered : WS_Hovered;
                    tab.closeState = WS_Default; tab.state = tab.state | WS_Hovered;
                    tab.pinned = !tab.pinned;
                    result.tabtype = TabButtonType::PinTab;
                    done = true;
                }
                else if (rect.Contains(io.mousepos) && (state.current != tabidx) && 
                    (state.tabBeingDragged == -1))
                {
                    state.hovered = tabidx;
                    tab.state = tab.state | WS_Hovered;
                    tab.closeState = WS_Default; tab.pinState = WS_Default;
                    if (io.isLeftMouseDown()) tab.state = tab.state | WS_Pressed;

                    if (io.clicked())
                    {
                        result.event = WidgetEvent::Clicked;
                        state.previous = state.current;
                        state.current = tabidx;
                        result.tabidx = tabidx;
                        result.tabtype = TabButtonType::AddedTab;
                        state.dragPosition = state.dragStart = ImVec2{};
                        state.tabBeingDragged = -1;
                        tab.state = tab.state | WS_Selected;
                        if (state.tabidx != nullptr) *(state.tabidx) = tabidx;
                    }
                    else if (io.isLeftMouseDown())
                    {
                        state.dragPosition = io.mousepos;
                        state.tabBeingDragged = tabidx;
                        state.dragPosition.x = clamp(state.dragPosition.x, content.Min.x, content.Max.x);
                        state.dragPosition.y = clamp(state.dragPosition.y, content.Min.y, state.lastRowStarty);
                        state.dragStart = state.dragPosition;
                    }

                    done = true;
                }

                ShowTooltip(tab.tabHoverDuration, rect, tab.descriptor.tooltip, io);
                ShowTooltip(tab.pinHoverDuration, tab.pin, Config.pinTabsTooltip, io);
                ShowTooltip(tab.closeHoverDuration, tab.close, Config.closeTabsTooltip, io);

                if (HandleContextMenu(id, rect, io))
                    WidgetContextData::RightClickContext.tabidx = tabidx;

                if (done)
                {
                    auto tidx = 0;
                    for (auto& tab : state.tabs)
                    {
                        if (tidx != tabidx)
                        {
                            tab.state = WS_Default;
                            tab.pinState = WS_Default;
                            tab.closeState = WS_Default;
                        }
                        ++tidx;
                    }

                    for (auto& tab : state.endTabs)
                        tab.state = WS_Default;

#ifndef GLIMMER_ENABLE_TESTING
                    return;
#endif
                }

                BEGIN_LOG_OBJECT("item");
                LOG_STATE(tab.state);
                LOG_TEXT(tab.descriptor.name); 
                LOG_NUM(tab.pinned);
                LOG_STYLE(style);
                END_LOG_OBJECT();
                ++tabidx;
            }

            END_LOG_ARRAY();

            BEGIN_LOG_ARRAY("end-items");
            tabidx = 0;

            for (auto& tab : state.endTabs)
            {
                auto done = false;
                auto& rect = tab.extent;
                const auto& style = tab.descriptor.style;

                if (rect.Contains(io.mousepos) && (state.tabBeingDragged == -1))
                {
                    state.hovered = -tabidx - 1;
                    tab.state = tab.state | WS_Hovered;
                    if (io.isLeftMouseDown()) tab.state = tab.state | WS_Pressed;

                    if ((state.current != (-tabidx - 1)) && io.clicked())
                    {
                        result.event = WidgetEvent::Clicked;
                        state.previous = state.current;
                        state.current = -tabidx - 1;
                        result.tabidx = -tabidx - 1;
                        result.tabtype = TabButtonType::EndTab;
                        state.dragPosition = state.dragStart = ImVec2{};
                        state.tabBeingDragged = -1;
                        if (state.tabidx != nullptr) *(state.tabidx) = -tabidx - 1;
                        tab.state = tab.state | WS_Selected;
                        done = true;
                    }
                }

                ShowTooltip(tab.tabHoverDuration, rect, tab.descriptor.tooltip, io);

                if (HandleContextMenu(id, rect, io))
                    WidgetContextData::RightClickContext.tabidx = tabidx;

                if (done)
                {
                    auto tidx = 0;
                    for (auto& tab : state.endTabs)
                    {
                        if (tidx != tabidx)
                        {
                            tab.state = WS_Default;
                            tab.pinState = WS_Default;
                            tab.closeState = WS_Default;
                        }
                        ++tidx;
                    }

                    for (auto& tab : state.tabs)
                        tab.state = WS_Default;

#ifndef GLIMMER_ENABLE_TESTING
                    return;
#endif
                }

                BEGIN_LOG_OBJECT("item");
                LOG_STATE(tab.state);
                LOG_TEXT(tab.descriptor.name);
                LOG_STYLE(style);
                END_LOG_OBJECT();
                ++tabidx;
            }

            END_LOG_ARRAY();

            if (content.Contains(io.mousepos) && io.isLeftMouseDown() && state.tabBeingDragged != -1)
            {
                state.dragPosition = io.mousepos;
                state.dragPosition.x = clamp(state.dragPosition.x, content.Min.x, content.Max.x);
                state.dragPosition.y = clamp(state.dragPosition.y, content.Min.y, state.lastRowStarty);
            }
            else if (!io.isLeftMouseDown() && state.tabBeingDragged != -1)
            {
                if (state.dragStart != state.dragPosition)
                {
                    auto otabidx = 0, tabDraggedTo = -1;
                    for (const auto& tab : state.tabs)
                    {
                        if (tab.extent.Contains(io.mousepos))
                            tabDraggedTo = otabidx;
                        ++otabidx;
                    }

                    if (tabDraggedTo != -1 && tabDraggedTo != state.tabBeingDragged)
                    {
                        result.event = WidgetEvent::Reordered;
                        result.range.first = state.tabBeingDragged;
                        result.range.second = tabDraggedTo;

                        state.dragPosition = state.dragStart = ImVec2{};
                        state.tabBeingDragged = -1;
                        return;
                    }
                }

                state.dragPosition = state.dragStart = ImVec2{};
                state.tabBeingDragged = -1;
            }

            if (state.create.Contains(io.mousepos))
            {
                state.hovered = NewTabIndex;

                if (io.clicked())
                {
                    result.event = WidgetEvent::Clicked;
                    result.tabtype = TabButtonType::NewTab;
                    return;
                }
            }

            const auto& config = CreateWidgetConfig(id).state.tab;
            ShowTooltip(state.createHoverDuration, state.create, config.newTabTooltip, io);

            if (state.moveBackward.Contains(io.mousepos))
            {
                state.hovered = MoveBackwardIndex;
                if (io.clicked())
                {
                    result.event = WidgetEvent::Clicked;
                    result.tabtype = TabButtonType::MoveBackward;
                    return;
                }
            }

            if (state.moveForward.Contains(io.mousepos))
            {
                state.hovered = MoveForwardIndex;
                if (io.clicked())
                {
                    result.event = WidgetEvent::Clicked;
                    result.tabtype = TabButtonType::MoveForward;
                    return;
                }
            }

            if (state.dropdown.Contains(io.mousepos))
            {
                state.hovered = DropDownTabIndex;

                if (io.clicked())
                {
                    result.event = WidgetEvent::Clicked;
                    result.tabtype = TabButtonType::MoreTabs;
                    return;
                }
            }

            if (state.scroll.type & ST_Horizontal)
            {
                auto width = state.tabs.back().extent.Max.x - state.tabs.front().extent.Min.x;
                state.scroll.viewport = content;
                state.scroll.content.x = width + content.Min.x;
                HandleHScroll(state.scroll, renderer, io, 5.f, false);
            }

            LOG_NUM(state.current);
        }
        else context.deferedEvents.emplace_back(EventDeferInfo::ForTabBar(id, content));
    }

    static void DrawTab(const TabBarState& config, const TabBarPersistentState& state, 
        const TabBarPersistentState::ItemDescriptor& tab, int tabidx, IRenderer& renderer)
    {
        auto& context = GetContext();
        const auto& rect = tab.extent;
        const auto& style = tab.descriptor.style;
        const auto& specificStyle = context.tabBarStyles[log2((unsigned)tab.state)].top();
        auto darker = DarkenColor(style.bgcolor);

        renderer.SetClipRect(rect.Min, rect.Max);
        DrawBackground(rect.Min, rect.Max, style, renderer);
        DrawBorderRect(rect.Min, rect.Max, style.border, style.bgcolor, renderer);

        const auto& currtab = tab.descriptor;
        if (!currtab.icon.empty())
        {
            renderer.DrawResource(currtab.iconType, tab.icon.Min, tab.icon.GetSize(), style.fgcolor, currtab.icon);
        }

        if (currtab.nameType == TextType::SVG)
            renderer.DrawResource(RT_SVG, tab.text.Min, tab.text.GetSize(), style.fgcolor, currtab.name);
        else
        {
            auto startpos = tab.text.Min, endpos = tab.extent.Max;
            if (currtab.itemflags & TI_Pinnable)
                endpos.x = tab.pin.Min.x - specificStyle.btnspacing;
            else if (currtab.itemflags & TI_Closeable)
                endpos.x = tab.close.Min.x - specificStyle.btnspacing;
            DrawText(startpos, endpos, tab.text, currtab.name, tab.state & WS_Disabled, style, renderer);
        }

        if (tab.pinned || (((tabidx == state.current) || (tabidx == state.hovered)) &&
            currtab.itemflags & TI_Pinnable))
        {
            const auto& pinBtnStyle = context.tabBarStyles[log2((unsigned)tab.pinState)].top();

            if (specificStyle.circularButtons)
            {
                ImVec2 center{ tab.pin.Min.x + (tab.pin.GetWidth() * 0.5f), tab.pin.Min.y +
                    (tab.pin.GetHeight() * 0.5f) };
                auto radius = (1.f / sqrtf(2)) * tab.pin.GetWidth();
                renderer.DrawCircle(center, radius, pinBtnStyle.pinbgcolor, true);
            }
            else
                renderer.DrawRect(tab.pin.Min, tab.pin.Max, pinBtnStyle.pinbgcolor, true);

            DrawSymbol(tab.pin.Min, tab.pin.GetSize(),
                { pinBtnStyle.pinPadding, pinBtnStyle.pinPadding }, SymbolIcon::Pin,
                pinBtnStyle.pincolor, pinBtnStyle.pinbgcolor, 2.f, renderer);
        }

        if ((currtab.itemflags & TI_Closeable) && ((tabidx == state.current) ||
            (tabidx == state.hovered)))
        {
            const auto& closeBtnStyle = context.tabBarStyles[log2((unsigned)tab.closeState)].top();

            if (specificStyle.circularButtons)
            {
                ImVec2 center{ tab.close.Min.x + (tab.close.GetWidth() * 0.5f), tab.close.Min.y +
                    (tab.close.GetHeight() * 0.5f) };
                auto radius = (1.f / sqrtf(2)) * tab.close.GetWidth();
                renderer.DrawCircle(center, radius, closeBtnStyle.closebgcolor, true);
            }
            else
                renderer.DrawRect(tab.close.Min, tab.close.Max, closeBtnStyle.closebgcolor, true);

            DrawSymbol(tab.close.Min, tab.close.GetSize(),
                { closeBtnStyle.closePadding, closeBtnStyle.closePadding },
                SymbolIcon::Cross, closeBtnStyle.closecolor, closeBtnStyle.closebgcolor, 2.f, renderer);
        }

        renderer.ResetClipRect();
    }

    WidgetDrawResult TabBarImpl(int32_t id, const ImRect& content, const StyleDescriptor& style, const IODescriptor& io, 
        IRenderer& renderer)
    {
        WidgetDrawResult result;
        auto& context = GetContext();
        auto& state = context.TabBarState(id);
        const auto& config = context.GetState(id).state.tab;
        auto tabidx = 0;

        DrawBackground(content.Min, content.Max, style, renderer);
        DrawBorderRect(content.Min, content.Max, style.border, style.bgcolor, renderer);
        
        for (const auto& tab : state.tabs)
        {
            if (state.tabBeingDragged != tabidx) 
                DrawTab(config, state, tab, tabidx, renderer);
            tabidx++;
        }

        if (state.tabBeingDragged != -1)
        {
            auto movedTab = state.tabs[state.tabBeingDragged];
            auto diff = state.dragPosition - state.dragStart;
            movedTab.extent.Translate(diff);
            movedTab.text.Translate(diff);
            movedTab.close.Translate(diff);
            movedTab.pin.Translate(diff);
            DrawTab(config, state, movedTab, state.tabBeingDragged, renderer);
        }

        if (state.create.GetArea() > 0.f)
        {
            auto flag = state.hovered == NewTabIndex ? WS_Hovered : WS_Default;
            const auto style = context.GetStyle(flag, id);

            DrawBackground(state.create.Min, state.create.Max, style, renderer);
            DrawBorderRect(state.create.Min, state.create.Max, style.border, style.bgcolor, renderer);
            DrawSymbol(state.create.Min, state.create.GetSize(),
                { style.padding.left, style.padding.top },
                SymbolIcon::Plus, style.fgcolor, 0, 2.f, renderer);
        }

        if (state.dropdown.GetArea() > 0.f)
        {
            auto flag = state.hovered == DropDownTabIndex ? WS_Hovered : WS_Default;
            const auto style = context.GetStyle(flag, id);

            if (config.addNavigationButtons)
            {
                // Move Backward Button
                if (state.moveBackward.GetArea() > 0.f)
                {
                    auto mbflag = state.hovered == MoveBackwardIndex ? WS_Hovered : WS_Default;
                    const auto mbstyle = context.GetStyle(mbflag, id);
                    DrawBackground(state.moveBackward.Min, state.moveBackward.Max, mbstyle, renderer);
                    DrawBorderRect(state.moveBackward.Min, state.moveBackward.Max, mbstyle.border, mbstyle.bgcolor, renderer);
                    DrawSymbol(state.moveBackward.Min, state.moveBackward.GetSize(),
                        { mbstyle.padding.left, mbstyle.padding.top },
                        SymbolIcon::LeftTriangle, mbstyle.fgcolor, 0, 2.f, renderer);
                }

                // Move Forward Button
                if (state.moveForward.GetArea() > 0.f)
                {
                    auto mfflag = state.hovered == MoveForwardIndex ? WS_Hovered : WS_Default;
                    const auto mfstyle = context.GetStyle(mfflag, id);
                    DrawBackground(state.moveForward.Min, state.moveForward.Max, mfstyle, renderer);
                    DrawBorderRect(state.moveForward.Min, state.moveForward.Max, mfstyle.border, mfstyle.bgcolor, renderer);
                    DrawSymbol(state.moveForward.Min, state.moveForward.GetSize(),
                        { mfstyle.padding.left, mfstyle.padding.top },
                        SymbolIcon::RightTriangle, mfstyle.fgcolor, 0, 2.f, renderer);
                }
            }

            DrawSymbol(state.create.Min, state.create.Max, {}, SymbolIcon::DownTriangle, style.fgcolor, 
                style.fgcolor, 1.f, renderer);
        }

        tabidx = 0;
        for (const auto& tab : state.endTabs)
        {
            DrawTab(config, state, tab, tabidx, renderer);
            tabidx++;
        }

        HandleTabBarEvent(context, id, content, io, renderer, result);
        result.geometry = content;
        return result;
    }

    bool BeginTabBar(int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto& context = GetContext();
        auto& builder = context.layoutStack.empty() ? context.currentTab : 
            context.layouts[context.layoutStack.top()].tabbar;
        builder.id = id;
        builder.geometry = geometry; builder.neighbors = neighbors;

        const auto& config = context.GetState(id).state.tab;
        builder.sizing = config.sizing;
        builder.newTabButton = config.createNewTabs;
        return true;
    }

    bool BeginTabBar(std::string_view id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto [iid, __] = GetIdFromString(id, WT_TabBar);
        return BeginTabBar(iid, geometry, neighbors);
    }

    static void UpdateTabStyle(WidgetContextData& context, const TabBarBuilder& tab, TabItemDescriptor& item, int32_t flags)
    {
        const auto& state = context.TabBarState(tab.id);
        if (flags & TI_AnchoredToEnd)
        {
            if (state.endTabs.size() >= tab.endItems.size())
                item.style = context.GetStyle(state.endTabs[tab.endItems.size() - 1].state);
            else
                item.style = context.GetStyle(WS_Default);
        }
        else
            if (state.tabs.size() >= tab.items.size())
                item.style = context.GetStyle(state.tabs[tab.items.size() - 1].state);
            else
                item.style = context.GetStyle(WS_Default);
    }

    void AddTab(std::string_view name, std::string_view tooltip, int32_t flags)
    {
        auto& context = GetContext();
        auto& builder = context.layoutStack.empty() ? context.currentTab : 
            context.layouts[context.layoutStack.top()].tabbar;
        auto& item = flags & TI_AnchoredToEnd ? builder.endItems.emplace_back() : builder.items.emplace_back();
        item.name = name;
        item.itemflags = flags;
        item.tooltip = tooltip;
        UpdateTabStyle(context, builder, item, flags);
    }

    void AddTab(int32_t resflags, std::string_view icon, TextType extype, std::string_view text, int32_t flags, ImVec2 iconsz)
    {
        auto& context = GetContext();
        auto& builder = context.layoutStack.empty() ? context.currentTab : 
            context.layouts[context.layoutStack.top()].tabbar;
        auto& item = flags & TI_AnchoredToEnd ? builder.endItems.emplace_back() : builder.items.emplace_back();
        item.name = text;
        item.nameType = extype;
        item.icon = icon;
        item.iconType = resflags;
        item.itemflags = flags;
        item.iconsz = iconsz;
        UpdateTabStyle(context, builder, item, flags);
    }

    void AddTab(int32_t resflags, std::string_view icon, int32_t flags, ImVec2 iconsz)
    {
        auto& context = GetContext();
        auto& builder = context.layoutStack.empty() ? context.currentTab :
            context.layouts[context.layoutStack.top()].tabbar;
        auto& item = flags & TI_AnchoredToEnd ? builder.endItems.emplace_back() : builder.items.emplace_back();
        item.icon = icon;
        item.iconType = resflags;
        item.itemflags = flags;
        item.iconsz = iconsz;
        UpdateTabStyle(context, builder, item, flags);
    }

    WidgetDrawResult EndTabBar(int32_t* outidx, std::optional<bool> canAddTab)
    {
        auto& context = GetContext();
        auto& builder = context.layoutStack.empty() ? context.currentTab : 
            context.layouts[context.layoutStack.top()].tabbar;
        builder.newTabButton = canAddTab.has_value() ? canAddTab.value() : builder.newTabButton;
        auto& state = context.TabBarState(builder.id);
        state.tabs.resize(builder.items.size());
        state.endTabs.resize(builder.endItems.size());
        state.tabidx = outidx;
        if (outidx) state.current = (int16_t)(*outidx);

        auto tabidx = 0;
        for (auto& tab : state.tabs)
        {
            tab.descriptor = builder.items[tabidx];
            tabidx++;
        }

        tabidx = 0;
        for (auto& tab : state.endTabs)
        {
            tab.descriptor = builder.endItems[tabidx];
            tabidx++;
        }

        auto result = Widget(builder.id, WT_TabBar, builder.geometry, builder.neighbors);
        context.currentTab.reset();
        return result;
    }

#pragma endregion

#pragma region Navigation Drawer

    ImRect NavDrawerBounds(int32_t id, const ImRect& available, IRenderer& renderer)
    {
        auto& context = GetContext();
        auto& nav = context.currentNavDrawer;
        auto& navstate = context.NavDrawerState(id);
        auto state = navstate.current != -1 ? WS_Hovered : WS_Default;
        auto navDrawerStyle = context.navDrawerStyles[state].top();
        auto style = context.GetStyle(state, id);
        ImRect content;
        float maxwidth = 0.f, occupied = 0.f, total = 0.f;
        navstate.visiblew = 0.f;

        if (nav.direction == DIR_Vertical)
        {
            auto idx = 0;

            if (nav.showText)
            {
                float curry = available.Min.y + style.border.top.thickness + style.padding.top,
                    startx = available.Min.x + style.border.left.thickness + style.padding.left;

                for (const auto& item : nav.items)
                {
                    auto iconwidth = item.style.font.size * item.iconFontSzRatio;
                    auto& itemgeom = navstate.items[idx];

                    itemgeom.border.Min = ImVec2{ startx, curry };
                    curry += item.style.padding.top + item.style.border.top.thickness;

                    itemgeom.icon.Min = ImVec2{
                        startx + item.style.border.left.thickness + item.style.padding.left,
                        curry };
                    itemgeom.icon.Max = ImVec2{
                        startx + iconwidth + item.style.border.right.thickness + item.style.padding.right,
                        curry + iconwidth };

                    auto textsz = renderer.GetTextSize(item.text, item.style.font.font, item.style.font.size);
                    itemgeom.text.Min = ImVec2{ itemgeom.icon.Min.x, itemgeom.icon.Max.y + navDrawerStyle.iconSpacing };
                    itemgeom.text.Max = itemgeom.text.Min + textsz;
                    maxwidth = std::max(maxwidth, std::max(iconwidth, textsz.x) + item.style.padding.h() + 
                        item.style.border.h());
                    curry += iconwidth + navDrawerStyle.iconSpacing + textsz.y;
                    itemgeom.border.Max = itemgeom.text.Max + ImVec2{
                        item.style.padding.right + item.style.border.right.thickness, 
                        item.style.padding.bottom + item.style.border.bottom.thickness,
                    };

                    occupied = maxwidth;
                    curry += navDrawerStyle.itemGap + item.style.border.bottom.thickness + item.style.padding.bottom;
                    ++idx;
                }

                for (auto& item : navstate.items)
                {
                    auto hdiff = (maxwidth - item.icon.GetWidth()) * 0.5f;
                    item.icon.TranslateX(hdiff);

                    hdiff = (maxwidth - item.text.GetWidth()) * 0.5f;
                    item.text.TranslateX(hdiff);

                    item.border.Max.x = item.border.Min.x + maxwidth;
                    total = std::max(total, item.border.GetWidth());
                }
            }
            else
            {
                float curry = (navstate.isOpen ? 0.f : available.Min.y) + style.border.top.thickness + style.padding.top,
                    startx = available.Min.x + style.border.left.thickness + style.padding.left;

                for (const auto& item : nav.items)
                {
                    auto iconwidth = item.style.font.size * item.iconFontSzRatio;
                    auto& itemgeom = navstate.items[idx];

                    itemgeom.border.Min = ImVec2{ startx, curry };
                    curry += item.style.padding.top + item.style.border.top.thickness;

                    itemgeom.icon.Min = ImVec2{
                        startx + item.style.border.left.thickness + item.style.padding.left,
                        curry };
                    itemgeom.icon.Max = itemgeom.icon.Min + ImVec2{ iconwidth, iconwidth };

                    if (navstate.isOpen)
                    {
                        auto textsz = renderer.GetTextSize(item.text, item.style.font.font, item.style.font.size);
                        itemgeom.text.Min = ImVec2{ itemgeom.icon.Max.x + navDrawerStyle.iconSpacing, itemgeom.icon.Min.y };
                        itemgeom.text.Max = itemgeom.text.Min + textsz;
                        maxwidth = std::max(maxwidth, iconwidth + textsz.x + navDrawerStyle.iconSpacing);
                        curry += std::max(iconwidth, textsz.y);
                        itemgeom.border.Max = itemgeom.text.Max + ImVec2{
                            0.f, item.style.padding.bottom + item.style.border.bottom.thickness,
                        };
                    }
                    else
                    {
                        curry += iconwidth;
                        maxwidth = std::max(maxwidth, iconwidth);
                        itemgeom.border.Max = itemgeom.icon.Max + ImVec2{
                            item.style.padding.right + item.style.border.right.thickness,
                            item.style.padding.bottom + item.style.border.bottom.thickness,
                        };
                    }

                    if (iconwidth < itemgeom.text.GetHeight())
                    {
                        auto vdiff = (itemgeom.text.GetHeight() - iconwidth) * 0.5f;
                        itemgeom.icon.TranslateY(vdiff);
                    }
                    else if (iconwidth > itemgeom.text.GetHeight())
                    {
                        auto vdiff = (iconwidth - itemgeom.text.GetHeight()) * 0.5f;
                        itemgeom.text.TranslateY(vdiff);
                    }

                    occupied = std::max(occupied, itemgeom.icon.Max.x);
                    curry += navDrawerStyle.itemGap + item.style.border.bottom.thickness + item.style.padding.bottom;
                    navstate.visiblew = std::max(navstate.visiblew, iconwidth + item.style.border.h() +
                        item.style.padding.h());
                    ++idx;
                }

                idx = 0; 
                for (auto& item : navstate.items)
                {
                    item.border.Max.x = item.border.Min.x + maxwidth + nav.items[idx].style.padding.h() + 
                        nav.items[idx].style.border.h();
                    total = std::max(total, item.border.GetWidth());
                    ++idx;
                }
            }

            content.Min = available.Min;
            content.Max = ImVec2{ 
                content.Min.x + occupied + style.padding.h() + style.border.h(),
                available.Max.y
            };
            navstate.extent = content;
            navstate.extent.Max.x = content.Min.x + total + style.padding.h() + style.border.h();
            navstate.visiblew += style.padding.h() + style.border.h();
        }

        return content;
    }

    void HandleNavDrawerEvents(WidgetContextData& context, const NavDrawerBuilder& nav, NavDrawerPersistentState& navstate,
        WidgetDrawResult& result, const IODescriptor& io, ImVec2 offset)
    {
        if (!context.deferEvents)
        {
            navstate.current = -1;
            auto idx = 0;

            WITH_WIDGET_LOG(nav.id, navstate.extent);
            BEGIN_LOG_ARRAY("nav-items");

            for (auto& item : navstate.items)
            {
                item.border.Translate(offset);
                item.icon.Translate(offset);
                item.text.Translate(offset);

                ImRect entry = item.border;
                if (entry.Contains(io.mousepos))
                {
                    navstate.current = idx;
                    item.state = io.isLeftMouseDown() ? WS_Hovered | WS_Pressed : WS_Hovered;

                    if (io.clicked() && idx != navstate.selected)
                    {
                        if (navstate.selected != -1)
                        {
                            auto& previtem = navstate.items[navstate.selected];
                            previtem.state &= ~WS_Selected;
                        }

                        navstate.selected = idx;
                        result.event = WidgetEvent::Clicked;
                        result.tabidx = navstate.selected;
                        item.state |= WS_Selected;
                        if (navstate.index != nullptr) *(navstate.index) = idx;
                    }
                }
                else
                    item.state = WS_Default;

                BEGIN_LOG_OBJECT("nav-item");
                LOG_STATE(item.state);
                LOG_TEXT(nav.items[idx].text);
                LOG_STYLE(nav.items[idx].style);
                END_LOG_OBJECT();

                ++idx;
            }

            END_LOG_ARRAY();
            navstate.state = navstate.extent.Contains(io.mousepos) ? WS_Hovered : WS_Default;

            if (navstate.state != WS_Hovered)
            {
                navstate.isOpen = false;
                navstate.currw = 0.018f;
            }
            else
                navstate.isOpen = true;

            LOG_NUM(navstate.selected); LOG_NUM(navstate.current);
            LOG_NUM(navstate.isOpen);
        }
        else
            context.deferedEvents.emplace_back(EventDeferInfo::ForNavDrawer(nav.id));
    }

    WidgetDrawResult NavDrawerImpl(int32_t wid, const ImRect& border, const StyleDescriptor& style,
        const IODescriptor& io, IRenderer& renderer)
    {
        WidgetDrawResult result;
        auto& context = GetContext();
        auto& navstate = context.NavDrawerState(wid);
        auto navDrawerStyle = context.navDrawerStyles[navstate.current != -1 ? WS_Hovered : WS_Default].top();
        auto idx = 0;
        auto isHovered = navstate.extent.Contains(io.mousepos), hasPopup = false;
        auto extent = navstate.extent;

        if (!navstate.isOpen)
        {
            renderer.SetClipRect(extent.Min, extent.Max);
            DrawBackground(border.Min, border.Max, style, renderer);
            DrawBorderRect(border.Min, border.Max, style.border, style.bgcolor, renderer);

            for (auto& item : navstate.items)
            {
                auto state = idx == navstate.current ? WS_Hovered : WS_Default;
                const auto& desc = context.currentNavDrawer.items[idx];

                ImRect entry = item.border;
                auto isItemHovered = entry.Contains(io.mousepos);

                renderer.SetClipRect(entry.Min, entry.Max);
                DrawBoxShadow(border.Min, border.Max, desc.style, renderer);
                DrawBackground(border.Min, border.Max, desc.style, renderer);
                DrawBorderRect(border.Min, border.Max, desc.style.border, desc.style.bgcolor, renderer);
                renderer.DrawResource(desc.resflags, item.icon.Min, item.icon.GetSize(), desc.style.bgcolor, desc.icon);
                renderer.ResetClipRect();
                ++idx;
            }

            HandleNavDrawerEvents(context, context.currentNavDrawer, navstate, result, io, {});
            renderer.ResetClipRect();
        }
        else
        {
            struct Data
            {
                NavDrawerBuilder& builder;
                NavDrawerPersistentState& state;
                const StyleDescriptor& style;
                ImRect border;
            };

            Data data{ context.currentNavDrawer, navstate, style, border };

            if (navDrawerStyle.openAnimationTime > 0 && navstate.currw < 1.0)
            {
                auto ratio = anim::EaseOut(navstate.currw, navDrawerStyle.openAnimationTime);
                extent.Max.x = extent.Min.x + ((extent.GetWidth() - navstate.visiblew) * ratio) + navstate.visiblew;
            }

            auto selected = navstate.selected;
            BeginPopup(wid, extent.Min, extent.GetSize());

            SetPopupCallback(PCB_GeneratePrimitives, [](void* ptr, IRenderer& renderer, ImVec2 origin, const ImRect&) {
                auto data = (Data*)ptr;
                auto& nav = data->builder;
                auto& navstate = data->state;
                const auto& style = data->style;
                auto idx = 0;

                auto rect = navstate.extent;
                rect.Translate(-origin); // This is required as pop-up translates all coordinates by origin
                renderer.SetClipRect(rect.Min, rect.Max);
                DrawBackground(rect.Min, rect.Max, style, renderer);
                DrawBorderRect(rect.Min, rect.Max, style.border, style.bgcolor, renderer);

                for (const auto& item : navstate.items)
                {
                    auto state = idx == navstate.current ? WS_Hovered : WS_Default;
                    const auto& desc = nav.items[idx];
                    ImRect entry = item.border;

                    renderer.SetClipRect(entry.Min, entry.Max);
                    DrawBoxShadow(item.border.Min, item.border.Max, desc.style, renderer);
                    DrawBackground(item.border.Min, item.border.Max, desc.style, renderer);
                    DrawBorderRect(item.border.Min, item.border.Max, desc.style.border, desc.style.bgcolor, renderer);
                    renderer.DrawResource(desc.resflags, item.icon.Min, item.icon.GetSize(), desc.style.fgcolor, desc.icon);

                    auto txtflags = desc.style.font.flags;
                    txtflags |= ToTextFlags(desc.textType);
                    DrawText(item.text.Min, item.text.Max, item.text, desc.text, false, desc.style, renderer, txtflags);
                    renderer.ResetClipRect();

                    ++idx;
                }

                renderer.ResetClipRect();
            }, (void*)&data);

            SetPopupCallback(PCB_HandleEvents, [](void* ptr, IRenderer&, ImVec2 offset, const ImRect&) {
                auto data = (Data*)ptr;
                auto& nav = data->builder;
                auto& navstate = data->state;
                const auto& io = Config.platform->desc;

                WidgetDrawResult temp{}; // Will be unused, maybe use std::optional param?
                HandleNavDrawerEvents(GetContext(), nav, navstate, temp, io, offset);
                
                if (navstate.state != WS_Hovered)
                    WidgetContextData::RemovePopup();
                
            }, (void*)&data);

            result = EndPopUp(false);
        }

        return result;
    }

    bool BeginNavDrawer(int32_t id, bool expandable, Direction dir, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto& context = GetContext();
        auto& nav = context.currentNavDrawer;
        nav.geometry = geometry;
        nav.neighbors = neighbors;
        nav.showText = !expandable;
        nav.direction = dir;
        nav.id = id;
        nav.items.clear(true);
        return true;
    }

    bool BeginNavDrawer(std::string_view id, bool expandable, Direction dir, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto [iid, __] = GetIdFromString(id, WT_NavDrawer);
        return BeginNavDrawer(iid, expandable, dir, geometry, neighbors);
    }

    void AddNavDrawerEntry(int32_t resflags, std::string_view icon, TextType extype, 
        std::string_view text, bool atStart, float iconFontSzRatio)
    {
        auto& context = GetContext();
        auto& nav = context.currentNavDrawer;
        auto& navstate = context.NavDrawerState(nav.id);
        auto& item = nav.items.emplace_back(text, icon, resflags, extype, iconFontSzRatio, StyleDescriptor{}, atStart);
        item.style = context.GetStyle(
            ((nav.items.size() - 1) >= navstate.items.size() || nav.items.empty()) ? WS_Default :
            navstate.items[nav.items.size() - 1].state);
    }

    void AddNavDrawerEntry(int32_t resflags, std::string_view icon, std::string_view text, bool atStart, float iconFontSzRatio)
    {
        auto& context = GetContext();
        auto& nav = context.currentNavDrawer;
        auto& navstate = context.NavDrawerState(nav.id);
        auto& item = nav.items.emplace_back(text, icon, resflags, TextType::PlainText, iconFontSzRatio, StyleDescriptor{}, atStart);
        item.style = context.GetStyle(
            ((nav.items.size() - 1) >= navstate.items.size() || nav.items.empty()) ? WS_Default :
            navstate.items[nav.items.size() - 1].state);
    }

    WidgetDrawResult EndNavDrawer(int32_t* index)
    {
        auto& context = GetContext();
        auto& nav = context.currentNavDrawer;
        auto& navstate = context.NavDrawerState(nav.id);

        navstate.items.resize(nav.items.size(), true);
        navstate.index = index;
        return Widget(nav.id, WT_NavDrawer, nav.geometry, nav.neighbors);
    }

#pragma endregion

#pragma region Accordion

    bool BeginAccordion(int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto& context = GetContext();
        auto& accordion = context.accordions.push();
        auto& state = context.AccordionState(id);
        accordion.id = id;
        accordion.geometry = geometry;
        
        const auto style = context.GetStyle(WS_Default, id);
        LayoutItemDescriptor item;
        item.id = id;
        item.sizing = geometry;
        item.wtype = WT_Accordion;
        AddExtent(item, style, neighbors);
        accordion.origin = item.margin.Min;
        accordion.size = item.margin.GetSize();
        accordion.content = item.content;
        accordion.spacing.top = style.padding.top + style.margin.top + style.border.top.thickness;
        accordion.spacing.bottom = style.padding.bottom + style.margin.bottom + style.border.bottom.thickness;
        accordion.spacing.left = style.padding.left + style.margin.left + style.border.left.thickness;
        accordion.spacing.right = style.padding.right + style.margin.right + style.border.right.thickness;

        context.ToggleDeferedRendering(true, false);
        context.deferEvents = true;
        BEGIN_WIDGET_LOG(id, accordion.content);
        BEGIN_LOG_ARRAY("accordion-section");

        return true;
    }

    bool BeginAccordion(std::string_view id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto [iid, __] = GetIdFromString(id, WT_Accordion);
        return BeginAccordion(iid, geometry, neighbors);
    }

    bool BeginAccordionHeader()
    {
        auto& context = GetContext();
        auto& accordion = context.accordions.top();
        auto& state = context.AccordionState(accordion.id);

        if (state.hstates.size() == accordion.totalRegions)
            state.hstates.emplace_back(WS_Default);

        if (accordion.regions.size() == accordion.totalRegions)
            accordion.regions.emplace_back();

        const auto style = context.GetStyle(state.hstates[accordion.totalRegions]);
        context.RecordDeferRange(accordion.regions[accordion.totalRegions].hrange, true);
        accordion.border = style.border;
        accordion.bgcolor = style.bgcolor;
        BEGIN_LOG_OBJECT("accordion-header");
        return true;
    }

    void AddAccordionHeaderExpandedIcon(int32_t resflags, std::string_view res)
    {
        auto& context = GetContext();
        auto& accordion = context.accordions.top();
        accordion.icon[0] = res;
        accordion.resflags[0] = resflags;
    }

    void AddAccordionHeaderCollapsedIcon(int32_t resflags, std::string_view res)
    {
        auto& context = GetContext();
        auto& accordion = context.accordions.top();
        accordion.icon[1] = res;
        accordion.resflags[1] = resflags;
    }

    void AddAccordionHeaderText(std::string_view content, TextType textType)
    {
        auto& context = GetContext();
        auto& accordion = context.accordions.top();
        auto& state = context.AccordionState(accordion.id);
        const auto style = context.GetStyle(state.hstates[accordion.totalRegions]);
        auto haswrap = !(style.font.flags & FontStyleNoWrap) && !(style.font.flags & FontStyleOverflowEllipsis) &&
            !(style.font.flags & FontStyleOverflowMarquee);
        accordion.textsz = GetTextSize(textType, content, style.font, haswrap ? accordion.content.GetWidth() : -1.f, context.GetRenderer());
        accordion.headerHeight = accordion.textsz.y;
        accordion.text = content;
        accordion.textType = textType;
        LOG_TEXT(accordion.text);
    }

    void HandleAccordionEvent(WidgetContextData& context, int32_t id, const ImRect& region, int ridx, const IODescriptor& io, WidgetDrawResult& result)
    {
        auto& accordion = context.accordions.top();

        if (!context.deferEvents)
        {
            auto& state = context.AccordionState(accordion.id);
            auto contains = region.Contains(io.mousepos);
            if (contains) WidgetContextData::CurrentWidgetId = id;

            if (contains && io.isLeftMouseDown()) state.hstates[ridx] = WS_Hovered | WS_Pressed;
            else if (contains) state.hstates[ridx] = WS_Hovered;
            else state.hstates[ridx] = WS_Default;

            if (contains)
            {
                if (io.clicked())
                {
                    state.opened = state.opened == ridx ? -1 : ridx;
                    result.event = WidgetEvent::Clicked;
                }
                
                Config.platform->SetMouseCursor(MouseCursor::Grab);
            }
        }
        else
            context.deferedEvents.emplace_back(EventDeferInfo::ForAccordion(accordion.id, region, ridx));
    }

    void EndAccordionHeader(std::optional<bool> expanded)
    {
        auto& context = GetContext();
        auto& accordion = context.accordions.top();
        auto& state = context.AccordionState(accordion.id);
        auto isExpanded = expanded.has_value() ? expanded.value() : state.opened == accordion.totalRegions;
        const auto style = context.GetStyle(state.hstates[accordion.totalRegions]);
        if (isExpanded) state.opened = accordion.totalRegions;

        auto iconidx = accordion.totalRegions == state.opened ? 1 : 0;
        auto& renderer = context.GetRenderer();
        ImRect bg{ accordion.content.Min, { accordion.content.Max.x,
            accordion.content.Min.y + style.padding.v() + accordion.textsz.y } };
        DrawBackground(bg.Min, bg.Max, style, renderer);
        auto nextpos = bg.Min + ImVec2{ style.padding.left, style.padding.top };
        auto iconsz = 0.5f * accordion.headerHeight;
        auto prevy = nextpos.y;
        nextpos.y += 0.25f * accordion.headerHeight;
        nextpos.x += 0.25f * accordion.headerHeight;
        if (!accordion.icon[iconidx].empty())
        {
            renderer.DrawResource(accordion.resflags[iconidx], nextpos, { iconsz, iconsz }, style.fgcolor,
                accordion.icon[iconidx]);
        }
        else
            DrawSymbol(nextpos, { iconsz, iconsz }, {}, expanded ? SymbolIcon::DownTriangle : SymbolIcon::RightTriangle,
                style.fgcolor, style.fgcolor, 1.f, renderer);
        nextpos.x += accordion.headerHeight + style.padding.left;
        nextpos.y = prevy;
        DrawText(nextpos, nextpos + ImVec2{ accordion.size.x, accordion.headerHeight }, 
            ImRect{ nextpos, nextpos + accordion.textsz }, accordion.text, false, style, renderer, 
            ToTextFlags(accordion.textType));
        context.RecordDeferRange(accordion.regions[accordion.totalRegions].hrange, false);

        accordion.regions[accordion.totalRegions].header = bg.GetSize();
        accordion.totalsz.y += bg.GetHeight();
        LOG_RECT2("header-size", bg);
        END_LOG_OBJECT();
    }

    bool BeginAccordionContent(float height, int32_t scrollflags, ImVec2 maxsz)
    {
        auto& context = GetContext();
        auto& accordion = context.accordions.top();
        auto& state = context.AccordionState(accordion.id);
        auto& scroll = state.scrolls.size() == accordion.totalRegions ? state.scrolls.emplace_back() : 
            state.scrolls[accordion.totalRegions];

        scroll.type = scrollflags;
        scroll.extent = maxsz;
        scroll.viewport.Min = accordion.content.Min;
        scroll.viewport.Max = scroll.viewport.Min + ImVec2{ accordion.size.x, height };
        context.RecordDeferRange(accordion.regions[accordion.totalRegions].crange, true);
        auto isOpen = state.opened == accordion.totalRegions;

        if (isOpen)
        {
            auto& cont = context.containerStack.push();
            cont = accordion.id;

            if ((scroll.type & ST_Horizontal) || (scroll.type & ST_Vertical))
                context.adhocLayout.top().insideContainer = true;
        }

        BEGIN_LOG_OBJECT("accordion-content");
        LOG_POS(maxsz); LOG_NUM(height); LOG_NUM(scrollflags);
        return isOpen;
    }

    void EndAccordionContent()
    {
        auto& context = GetContext();
        auto& accordion = context.accordions.top();
        auto& state = context.AccordionState(accordion.id);

        if (state.opened == accordion.totalRegions)
        {
            auto lrect = context.GetLayoutSize();
            state.scrolls[accordion.totalRegions].content = lrect.Max;
            if (state.scrolls[accordion.totalRegions].viewport.Max.y == FLT_MAX)
                state.scrolls[accordion.totalRegions].viewport.Max.y = lrect.Max.y;
            accordion.regions[accordion.totalRegions].content = state.scrolls[accordion.totalRegions].viewport.GetSize();
            accordion.totalsz.y += accordion.regions[accordion.totalRegions].content.y;
            context.RecordDeferRange(accordion.regions[accordion.totalRegions].crange, false);
            context.containerStack.pop(1, true);
            context.adhocLayout.top().insideContainer = false;
            context.adhocLayout.top().addedOffset = false;
        }

        accordion.totalRegions++;
        END_LOG_OBJECT();
    }

    WidgetDrawResult EndAccordion()
    {
        auto& context = GetContext();
        auto& accordion = context.accordions.top();
        auto& state = context.AccordionState(accordion.id);
        auto res = accordion.event;
        context.deferEvents = false;

        auto& renderer = context.GetRenderer();
        auto io = Config.platform->CurrentIO();
        accordion.totalsz.y += 2.f * (float)(accordion.totalRegions - 1);
        auto offset = (accordion.geometry & ToBottom) ? ImVec2{ 0, 0 } : ImVec2{ 0, 
            accordion.content.GetHeight() - accordion.totalsz.y - 
            (accordion.origin.y + accordion.size.y - accordion.content.Max.y) };
        ImRect content;
        content.Min = { FLT_MAX, FLT_MAX };

        for (auto idx = 0; idx < accordion.totalRegions; ++idx)
        {
            const auto& region = accordion.regions[idx];
            ImVec2 headerstart = accordion.content.Min + offset;
            content.Min = ImMin(content.Min, headerstart);
            ImRect header{ headerstart, headerstart + region.header };
            renderer.Render(*Config.renderer, offset, region.hrange.primitives.first,
                region.hrange.primitives.second);
            HandleAccordionEvent(context, accordion.id, header, idx, io, res);

            if (HandleContextMenu(accordion.id, header, io))
                WidgetContextData::RightClickContext.tabidx = idx;

            if (header.Contains(io.mousepos))
            {
                if (io.clicked()) res.event = WidgetEvent::Clicked;
                else if (io.isLeftMouseDown()) state.hstates[idx] |= WS_Pressed;
                else state.hstates[idx] &= ~WS_Pressed;
                state.hstates[idx] |= WS_Hovered;
            }
            else state.hstates[idx] &= ~WS_Hovered;

            if (idx == state.opened)
            {
                offset.y += region.header.y;
                auto scsz = state.scrolls[idx].viewport.GetSize();
                state.scrolls[idx].viewport.Min = accordion.content.Min + offset;
                state.scrolls[idx].viewport.Max = state.scrolls[idx].viewport.Min + scsz;

                renderer.Render(*Config.renderer, offset, region.crange.primitives.first,
                    region.crange.primitives.second);
                context.HandleEvents(offset, region.crange.events.first, region.crange.events.second);

                for (auto eidx = region.crange.events.first; eidx < region.crange.events.second; ++eidx)
                {
                    auto id = context.deferedEvents[eidx].id;
                    auto geometry = context.GetGeometry(id);
                    geometry.Translate(offset);
                    context.AddItemGeometry(id, geometry);
                }

                ImRect border{ headerstart, state.scrolls[idx].viewport.Max };
                DrawBorderRect(border.Min, border.Max, accordion.border, accordion.bgcolor, *Config.renderer);

                auto hscroll = (state.scrolls[idx].type & ST_Horizontal) ? HandleHScroll(state.scrolls[accordion.totalRegions], 
                    renderer, io, Config.scrollbar.width) : false;
                if (state.scrolls[idx].type & ST_Vertical) HandleVScroll(state.scrolls[accordion.totalRegions], 
                    renderer, io, Config.scrollbar.width, hscroll);
                offset.y += scsz.y;
            }

            offset.y += 2.f;
        }

        content.Max = { accordion.content.Max.x, accordion.content.Min.y + offset.y };
        content.Max += ImVec2{ accordion.spacing.right, accordion.spacing.bottom };
        content.Min -= ImVec2{ accordion.spacing.left, accordion.spacing.top };
        context.deferedEvents.clear(true);
        context.ToggleDeferedRendering(false, true);
        context.AddItemGeometry(accordion.id, content);
        context.accordions.pop(1, false);
        accordion.reset();
        END_LOG_ARRAY();
        LOG_NUM(state.opened);
        END_WIDGET_LOG();
        return res;
    }

#pragma endregion

#pragma region ItemGrid

    void ItemGridConfig::setColumnResizable(int16_t col, bool resizable)
    {
        setColumnProps(col, COL_Resizable, resizable);
    }

    void ItemGridConfig::setColumnProps(int16_t col, ColumnProperty prop, bool set)
    {
        if (col >= 0)
        {
            auto lastLevel = (int)config.headers.size() - 1;
            set ? config.headers[lastLevel][col].props |= prop : config.headers[lastLevel][col].props &= ~prop;
            while (lastLevel > 0)
            {
                auto parent = config.headers[lastLevel][col].parent;
                lastLevel--;
                set ? config.headers[lastLevel][parent].props |= prop : config.headers[lastLevel][parent].props &= ~prop;
            }
        }
        else
        {
            for (auto level = 0; level < (int)config.headers.size(); ++level)
                for (auto lcol = 0; lcol < (int)config.headers[level].size(); ++lcol)
                    set ? config.headers[level][lcol].props |= prop : config.headers[level][lcol].props &= ~prop;
        }
    }

    template <typename ContainerT>
    static bool UpdateSubHeadersResize(Span<ContainerT> headers, ItemGridPersistentState& gridstate,
        const ImRect& rect, int parent, int chlevel, bool mouseDown)
    {
        if (chlevel >= (int)headers.size()) return true;

        auto cchcol = 0, chcount = 0, startch = -1;
        while (cchcol < (int)headers[chlevel].size() && headers[chlevel][cchcol].parent != parent)
            cchcol++;

        startch = cchcol;
        while (cchcol < (int)headers[chlevel].size() && headers[chlevel][cchcol].parent == parent)
        {
            cchcol++;
            if (headers[chlevel][cchcol].props & COL_Resizable) chcount++;
        }

        if (chcount > 0)
        {
            auto hdiff = rect.GetWidth() / (float)chcount;

            while (startch < cchcol)
            {
                auto& hdr = headers[chlevel][startch];
                if ((hdr.parent == parent) && (hdr.props & COL_Resizable))
                {
                    auto& props = gridstate.cols[chlevel][startch];
                    props.modified += hdiff;
                    startch++;
                }
            }
        }

        return chcount > 0;
    }

    template <typename ContainerT>
    static bool HandleColumnResize(Span<ContainerT> headers, const ImRect& content,
        ItemGridPersistentState& gridstate, ImVec2 mousepos, int level, int col, const IODescriptor& io)
    {
        bool res = false;
        if (gridstate.state != ItemGridCurrentState::Default && gridstate.state != ItemGridCurrentState::ResizingColumns) return res;

        auto isMouseNearColDrag = IsBetween(mousepos.x, content.Min.x, content.Min.x, 5.f) &&
            IsBetween(mousepos.y, content.Min.y, content.Max.y);
        auto& evprop = gridstate.cols[level][col - 1];

        if (!evprop.mouseDown && isMouseNearColDrag)
        {
            Config.platform->SetMouseCursor(MouseCursor::ResizeHorizontal);
            res = true;
        }

        if (io.isLeftMouseDown())
        {
            if (!evprop.mouseDown)
            {
                if (isMouseNearColDrag)
                {
                    evprop.mouseDown = true;
                    evprop.lastPos = mousepos;
                    gridstate.state = ItemGridCurrentState::ResizingColumns;
                    res = true;
                }
            }
            else
            {
                ImRect extendRect{ evprop.lastPos, mousepos };
                evprop.modified += (mousepos.x - evprop.lastPos.x);
                evprop.lastPos = mousepos;
                UpdateSubHeadersResize(headers, gridstate, extendRect, col - 1, level + 1, true);
                Config.platform->SetMouseCursor(MouseCursor::ResizeHorizontal);
                gridstate.state = ItemGridCurrentState::ResizingColumns;
                res = true;
            }
        }
        else if (!io.isLeftMouseDown() && evprop.mouseDown)
        {
            if (mousepos.x != -FLT_MAX && mousepos.y != -FLT_MAX)
            {
                ImRect extendRect{ evprop.lastPos, mousepos };
                evprop.modified += (mousepos.x - evprop.lastPos.x);
                evprop.lastPos = mousepos;
                UpdateSubHeadersResize(headers, gridstate, extendRect, col - 1, level + 1, false);
            }

            evprop.mouseDown = false;
            gridstate.state = ItemGridCurrentState::Default;
        }

        return res;
    }

    template <typename ContainerT>
    static void HandleColumnReorder(Span<ContainerT> headers, ItemGridPersistentState& gridstate,
        ImVec2 mousepos, int level, int vcol, const IODescriptor& io)
    {
        if (gridstate.state != ItemGridCurrentState::Default && gridstate.state != ItemGridCurrentState::ReorderingColumns) return;

        auto col = gridstate.colmap[level].vtol[vcol];
        auto& hdr = headers[level][col];
        auto isMouseDown = io.isLeftMouseDown();
        ImRect moveTriggerRect{ hdr.extent.Min + ImVec2{ 5.5f, 0.f }, hdr.extent.Max - ImVec2{ 5.5f, 0.f } };

        if (isMouseDown && moveTriggerRect.Contains(mousepos) && !gridstate.drag.mouseDown)
        {
            auto movingcol = vcol, siblingCount = 0;
            auto parent = hdr.parent;

            // This is required to "go up" he header hierarchy in case the sibling count 
            // at current level is 1 i.e. only the current header is a child of parent
            // In this case, consider the parent to be moving as well.
            // NOTE: Keep going up the hierarchy till we are at root level
            while (level > 0)
            {
                // TODO: Use logical index instead
                siblingCount = 0;
                for (int16_t col = 0; col < (int16_t)headers[level].size(); ++col)
                    if (headers[level][col].parent == parent)
                    {
                        siblingCount++; movingcol = col;
                    }

                if (siblingCount > 1) break;
                else if (level > 0) { parent = headers[level - 1][parent].parent; level--; }
            }

            movingcol = siblingCount == 1 ? parent : movingcol;
            level = siblingCount == 1 ? level - 1 : level;

            auto lcol = col;
            auto& mcol = headers[level][lcol];
            gridstate.drag.config = mcol;
            gridstate.drag.mouseDown = true;
            gridstate.drag.lastPos = mousepos;
            gridstate.drag.startPos = mousepos;
            gridstate.drag.potentialColumn = movingcol;
            gridstate.drag.level = level;
            gridstate.state = ItemGridCurrentState::ReorderingColumns;
            LOGERROR("\nMarking column (v: %d, l: %d) as moving (%f -> %f)\n", vcol, lcol, mcol.content.Min.x, mcol.content.Max.x);
        }
        else if (isMouseDown && gridstate.drag.mouseDown && gridstate.drag.potentialColumn == vcol && gridstate.drag.level == level &&
            mousepos.x != gridstate.drag.startPos.x)
        {
            gridstate.drag.column = gridstate.drag.potentialColumn;
        }
        else if (isMouseDown && gridstate.drag.mouseDown && gridstate.drag.column == vcol && gridstate.drag.level == level)
        {
            // This implying we are dragging the column around
            auto diff = mousepos.x - gridstate.drag.lastPos.x;

            if (diff > 0.f && (int16_t)headers[level].size() > (vcol + 1))
            {
                auto ncol = gridstate.colmap[level].vtol[vcol + 1];
                auto& next = headers[level][ncol];

                if ((mousepos.x - gridstate.drag.startPos.x) >= next.extent.GetWidth())
                    gridstate.swapColumns(vcol, vcol + 1, headers, level);
            }
            else if (diff < 0.f && (col - 1) >= 0)
            {
                auto& prev = headers[level][col - 1];

                if ((gridstate.drag.startPos.x - mousepos.x) >= prev.extent.GetWidth())
                    gridstate.swapColumns(col - 1, col, headers, level);
            }

            gridstate.drag.lastPos = mousepos;
        }
        else if (!isMouseDown)
        {
            gridstate.drag = ItemGridPersistentState::HeaderCellDragState{};
            gridstate.state = ItemGridCurrentState::Default;
            gridstate.drag.potentialColumn = -1;
        }
    }

    static void AddItemGridScrollBars(ItemGridBuilder& builder, ItemGridPersistentState& state, 
        const ItemGridConfig& config, IRenderer& renderer, ImRect viewport, const IODescriptor& io)
    {
        auto& context = *WidgetContextData::CurrentItemGridContext;

        if (config.frozencols > 0)
        {
            const auto& lastFrozenHeader = builder.headers[builder.currlevel][config.frozencols - 1];
            viewport.Min.x = lastFrozenHeader.extent.Max.x + config.gridwidth;
            builder.totalsz.x -= (viewport.Min.x - builder.origin.x);

            // Determine if any col content in frozen part exceeds the available space w.r.t column widths
            // Enable horizontal scrollbar in that case
            if (builder.maxColWidth > lastFrozenHeader.extent.GetWidth())
            {
                state.altscroll.viewport.Min = builder.origin;
                state.altscroll.viewport.Max = { lastFrozenHeader.extent.Max.x, builder.size.y + builder.origin.y };

                ImVec2 sz;
                state.altscroll.content.y = state.altscroll.viewport.GetHeight();
                state.altscroll.content.x = (config.frozencols - 1) > 0 ? builder.headers[builder.currlevel][config.frozencols - 2].extent.Max.x +
                    builder.maxColWidth : builder.maxColWidth;
                state.altscroll.type = config.scrollprops;
                HandleHScroll(state.altscroll, renderer, io, Config.scrollbar.width);
            }
            else if (config.scrollprops & ST_Always_H)
            {
                auto max = builder.origin + builder.size;
                renderer.DrawRect(ImVec2{ builder.origin.x, max.y - Config.scrollbar.width }, ImVec2{ lastFrozenHeader.extent.Max.x, max.y },
                    ToRGBA(255, 255, 255), true);
            }
        }

        // builder.totalsz is in absolute coordinates, convert it to size
        // state.scroll.content is adjusted in y-axis because y-absolute coordinates are
        // shifted by scroll amount in y-direction, whereas x-coordiantes are not
        auto sz = builder.totalsz - builder.origin - ImVec2{ 0.f, builder.headerHeight };
        if (config.scrollprops & ST_Always_H) viewport.Max.y += Config.scrollbar.width;
        if (config.scrollprops & ST_Always_V) viewport.Max.x += Config.scrollbar.width;
        state.scroll.viewport = viewport;
        state.scroll.content = sz + ImVec2{ 0.f,  state.scroll.state.pos.y };
        state.scroll.type = config.scrollprops;

        auto hasHScroll = HandleHScroll(state.scroll, renderer, io, Config.scrollbar.width);
        HandleVScroll(state.scroll, renderer, io, Config.scrollbar.width, hasHScroll,
            std::make_pair(builder.origin.y, builder.origin.y + builder.size.y));
    }

    bool BeginItemGrid(int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto& context = GetContext();
        auto& builder = context.itemGrids.push();
        const auto& config = context.GetState(id).state.grid;
        const auto style = context.GetStyle(WS_Default, id);

        builder.id = id;
        builder.geometry = geometry;
        builder.neighbors = neighbors;

        LayoutItemDescriptor item;
        item.wtype = WT_ItemGrid;
        item.id = id;
        AddExtent(item, style, neighbors);
        if (config.scrollprops & ST_Always_H) item.content.Max.y -= Config.scrollbar.width;
        if (config.scrollprops & ST_Always_V) item.content.Max.x -= Config.scrollbar.width;
        builder.size = item.content.GetSize();
        builder.origin = builder.nextpos = item.content.Min;

        context.CurrentItemGridContext = &context;
        PushContext(id, NestedContextSourceType::ItemGrid);
        
        return true;
    }

    bool BeginItemGrid(std::string_view id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto [iid, __] = GetIdFromString(id, WT_ItemGrid);
        return BeginItemGrid(iid, geometry, neighbors);
    }

    void SetItemGridBehavior(int16_t sortedcol, int32_t highlights, int32_t selection, int32_t scrollprops,
        bool uniformRowHeights, bool isTree)
    {
        auto& context = *WidgetContextData::CurrentItemGridContext;
        auto& builder = context.itemGrids.top();
        auto& config = context.GetState(builder.id).state.grid;
        auto& state = context.GridState(builder.id);
        assert(builder.phase == ItemGridConstructPhase::None);

        config.highlights = highlights;
        config.selection = selection;
        config.scrollprops = scrollprops;
        config.uniformRowHeights = uniformRowHeights;
        config.isTree = isTree;
        state.sortedCol = sortedcol;
    }

    void SetItemGridProviders(ItemGridConfig::CellPropertiesProviderT cellprops,
        ItemGridConfig::CellWidgetProviderT cellwidget,
        ItemGridConfig::CellContentProviderT cellcontent,
        ItemGridConfig::HeaderProviderT header)
    {
        auto& context = *WidgetContextData::CurrentItemGridContext;
        auto& builder = context.itemGrids.top();
        auto& config = context.GetState(builder.id).state.grid;
        assert(builder.phase == ItemGridConstructPhase::None);
        assert(cellprops != nullptr);
        assert(cellwidget != nullptr || cellcontent != nullptr);

        config.cellprops = cellprops;
        config.cellwidget = cellwidget;
        config.cellcontent = cellcontent;
        config.header = header;
    }

    bool BeginItemGridHeader(int levels)
    {
        assert(levels > 0 && levels <= GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL);
        auto& context = *WidgetContextData::CurrentItemGridContext;
        auto& builder = context.itemGrids.top();
        auto& itemcfg = context.GetState(builder.id).state.grid;

        builder.phase = ItemGridConstructPhase::Headers;
        builder.levels = levels;
        builder.currlevel = builder.levels - 1;
        builder.currCol = 0;
        builder.nextpos = builder.origin + ImVec2{ itemcfg.gridwidth, itemcfg.gridwidth };

        auto& ctx = GetContext();
        ctx.ToggleDeferedRendering(true, false);
        ctx.deferEvents = true;

        return true;
    }

    static void InitColumnResizeData(WidgetContextData& context, ItemGridBuilder& builder, ColumnProps& header)
    {
        auto& state = context.GridState(builder.id);
        if (state.cols[builder.currlevel].empty())
            state.cols[builder.currlevel].fill(ItemGridPersistentState::HeaderCellResizeState{});
        if (state.cols[builder.currlevel].size() <= builder.currCol)
            state.cols[builder.currlevel].expand(128, true);
    }

    static void AddUserColumnResize(WidgetContextData& context, ItemGridBuilder& builder, ColumnProps& header)
    {
        auto& state = context.GridState(builder.id);
        InitColumnResizeData(context, builder, header);
        auto modified = state.cols[builder.currlevel][builder.currCol].modified;
        header.extent.Max.x += modified;
    }

    // Only compute bounds, position them in header end (for visual order)
    // Integrate layout mechanism, add max computation?
    void StartHeaderColumn(const ItemGridConfig::ColumnConfig& config)
    {
        auto& context = *WidgetContextData::CurrentItemGridContext;
        auto& builder = context.itemGrids.top();
        auto& header = builder.headers[builder.currlevel].emplace_back(config);
        auto& itemcfg = context.GetState(builder.id).state.grid;
        auto& renderer = GetContext().GetRenderer();
        const auto style = context.GetStyle(itemcfg.state, builder.id);

        builder.phase = ItemGridConstructPhase::HeaderCells;
        header.extent.Min = builder.nextpos;
        header.content.Min = header.extent.Min + itemcfg.cellpadding;
        static char buffer[256] = { 0 };

        if (builder.currlevel == (builder.levels - 1))
        {
            memset(buffer, ' ', 255);

            auto width = (config.props & COL_WidthAbsolute) ? config.width :
                config.width > 0.f ? GetTextSize(ToTextTypeEnum(style.font.flags), std::string_view{ buffer, (size_t)config.width },
                    style.font, style.font.size, renderer).x : FLT_MAX;

            header.content.Max.x = header.content.Min.x + width;
            header.extent.Max.x = width == FLT_MAX ? FLT_MAX : header.content.Max.x + itemcfg.cellpadding.x;
            if (width != FLT_MAX) AddUserColumnResize(context, builder, header);
        }
        else assert(false); // For higher levels, use column category APIs

        if (config.width != 0.f)
        {
            auto& ctx = GetContext();
            auto& id = ctx.containerStack.push();
            id = builder.id;
            builder.addedBounds = true;
        }

        GetContext().adhocLayout.top().nextpos = builder.nextpos;
    }

    void EndHeaderColumn()
    {
        auto& context = *WidgetContextData::CurrentItemGridContext;
        auto& builder = context.itemGrids.top();
        const auto& itemcfg = context.GetState(builder.id).state.grid;
        const auto& header = builder.currentHeader();

        builder.phase = ItemGridConstructPhase::Headers;
        builder.headerHeights[builder.currlevel] = std::max(builder.headerHeights[builder.currlevel], header.extent.GetHeight());
        builder.currCol++;
        builder.maxHeaderExtent = ImVec2{};
        builder.nextpos.x = header.extent.Max.x + itemcfg.gridwidth;
        GetContext().adhocLayout.top().nextpos = builder.nextpos;

        if (builder.addedBounds)
        {
            auto& ctx = GetContext();
            ctx.containerStack.pop(1, true);
            builder.addedBounds = false;
        }
    }

    static void DrawSortIndicators(WidgetContextData& context, WidgetContextData& ctx, const ItemGridConfig::ColumnConfig& config, const StyleDescriptor& style,
        ColumnProps& header, const ItemGridConfig& itemcfg, ItemGridBuilder& builder, IRenderer& renderer)
    {
        if (config.props & COL_Sortable)
        {
            auto& state = context.GridState(builder.id);
            ImVec2 btnsz{ style.font.size * 0.5f, style.font.size * 0.5f };
            ImRect bounds{};
            bounds.Min = header.content.Min;
            bounds.Max = bounds.Min + btnsz;
            ctx.RecordDeferRange(header.sortIndicatorRange, true);

            if (state.sortedCol == builder.currCol && state.sortedLevel == builder.currlevel)
            {
                DrawSymbol(bounds.Min, bounds.GetSize(), { 1.f, 1.f }, state.sortedAscending ? SymbolIcon::UpTriangle : SymbolIcon::DownTriangle,
                    style.fgcolor, style.fgcolor, 1.f, renderer);
            }
            else
            {
                auto halfh = 0.5f * bounds.GetHeight();
                auto iconw = 0.2f * bounds.GetWidth();
                ImRect upper{ bounds.Min, ImVec2{ bounds.Max.x - iconw, bounds.Min.y + halfh - 1.f } };
                ImRect lower{ ImVec2{ bounds.Min.x, bounds.Min.y + halfh + 1.f }, bounds.Max - ImVec2{ iconw, 0.f } };

                auto color = (config.props & COL_SortOnlyDescending) ? LightenColor(style.fgcolor) : style.fgcolor;
                DrawSymbol(upper.Min, upper.GetSize(), {}, SymbolIcon::UpTriangle, color, color, 0.f, renderer);

                color = (config.props & COL_SortOnlyAscending) ? LightenColor(style.fgcolor) : style.fgcolor;
                DrawSymbol(lower.Min, lower.GetSize(), {}, SymbolIcon::DownTriangle, color, color, 0.f, renderer);
            }

            builder.btnsz = bounds.GetHeight();
            ctx.RecordDeferRange(header.sortIndicatorRange, false);
            ctx.adhocLayout.top().nextpos.x += builder.btnsz + itemcfg.cellpadding.x;
        }
    }

    void AddHeaderColumn(const ItemGridConfig::ColumnConfig& config)
    {
        StartHeaderColumn(config);
        auto& context = *WidgetContextData::CurrentItemGridContext;
        auto& builder = context.itemGrids.top();
        auto& itemcfg = context.GetState(builder.id).state.grid;
        auto& ctx = GetContext();
        auto& renderer = *ctx.deferedRenderer;
        const auto style = context.GetStyle(itemcfg.state, builder.id);
        auto& header = builder.currentHeader();

        DrawSortIndicators(context, ctx, config, style, header, itemcfg, builder, renderer);
        ctx.RecordDeferRange(header.range, true);
        if (itemcfg.header != nullptr)
            itemcfg.header(header.content.Min, header.content.GetWidth(),
                builder.currlevel, builder.currCol, config.parent);
        else
        {
            static char buffer[64] = { 0 };
            std::memset(buffer, 0, 64);
            auto sz = std::snprintf(buffer, 63, "[itemgrid-%d][header-%dx%d]",
                builder.id, builder.currCol, builder.currlevel);
            PushStyle(WS_AllStates, "background-color: transparent");
            Label(std::string_view{ buffer, (size_t)sz }, config.name);
            PopStyle(1, WS_AllStates);
        }

        ctx.RecordDeferRange(header.range, false);

        if (config.width == 0.f)
        {
            header.content.Max = builder.maxHeaderExtent;
            header.extent.Max.x = header.content.Max.x + itemcfg.cellpadding.x;
            AddUserColumnResize(context, builder, header);
        }
        else
            header.content.Max.y = builder.maxHeaderExtent.y;

        header.extent.Max.y = header.content.Max.y + itemcfg.cellpadding.y;
        EndHeaderColumn();
    }

    void CategorizeColumns()
    {
        auto& context = *WidgetContextData::CurrentItemGridContext;
        auto& builder = context.itemGrids.top();
        auto& itemcfg = context.GetState(builder.id).state.grid;

        builder.nextpos.x = builder.origin.x + itemcfg.gridwidth;
        builder.nextpos.y = builder.origin.y + itemcfg.gridwidth;
        builder.maxHeaderExtent = ImVec2{};
        builder.currlevel--;
        builder.currCol = 0;
    }

    void AddColumnCategory(const ItemGridConfig::ColumnConfig& config, int16_t from, int16_t to)
    {
        auto& context = *WidgetContextData::CurrentItemGridContext;
        auto& builder = context.itemGrids.top();
        auto& ctx = GetContext();
        auto& renderer = *ctx.deferedRenderer;
        auto& itemcfg = context.GetState(builder.id).state.grid;
        auto& header = builder.headers[builder.currlevel].emplace_back(config);
        auto parent = builder.headers[builder.currlevel].size() - 1;
        auto width = 0.f;

        for (auto idx = from; idx <= to; ++idx)
        {
            builder.headers[builder.currlevel + 1][idx].parent = parent;
            width += builder.headers[builder.currlevel + 1][idx].extent.GetWidth();
        }

        width += (float)(to - from) * itemcfg.gridwidth;

        builder.phase = ItemGridConstructPhase::HeaderCells;
        header.extent.Min = builder.nextpos;
        header.content.Min = header.extent.Min + itemcfg.cellpadding;
        header.extent.Max.x = header.extent.Min.x + width;
        GetContext().adhocLayout.top().nextpos = header.content.Min;
        InitColumnResizeData(context, builder, header);

        if (header.content.GetWidth() > 0.f)
        {
            auto& id = ctx.containerStack.push();
            id = builder.id;
            builder.addedBounds = true;
        }

        const auto style = context.GetStyle(itemcfg.state, builder.id);
        DrawSortIndicators(context, ctx, config, style, header, itemcfg, builder, renderer);
        ctx.RecordDeferRange(header.range, true);

        if (itemcfg.header != nullptr)
            itemcfg.header(header.content.Min, header.content.GetWidth(),
                builder.currlevel, builder.currCol, config.parent);
        else
        {
            static char buffer[64] = { 0 };
            std::memset(buffer, 0, 64);
            auto sz = std::snprintf(buffer, 63, "[itemgrid-%d][header-%dx%d]",
                builder.id, builder.currCol, builder.currlevel);
            Label(std::string_view{ buffer, (size_t)sz }, config.name);
        }

        ctx.RecordDeferRange(header.range, false);

        header.content.Max = builder.maxHeaderExtent;
        header.extent.Max.y = header.content.Max.y + itemcfg.cellpadding.y;
        builder.nextpos = ImVec2{ itemcfg.gridwidth + header.extent.Max.x, header.extent.Min.y };

        EndHeaderColumn();
    }

    WidgetDrawResult EndItemGridHeader()
    {
        WidgetDrawResult result;
        auto& context = *WidgetContextData::CurrentItemGridContext;
        auto& builder = context.itemGrids.top();
        auto& state = context.GridState(builder.id);
        auto& config = context.GetState(builder.id).state.grid;
        auto& headers = builder.headers;
        auto& renderer = context.GetRenderer();
        auto io = Config.platform->CurrentIO();

        CategorizeColumns();
        assert(builder.currlevel < 0);
        ImRect hextent{ { FLT_MAX, FLT_MAX }, {} };

        // Center align header contents
        auto ypos = builder.origin.y + config.gridwidth;

        for (auto level = 0; level < builder.levels; ++level)
        {
            auto totalw = 0.f;

            for (auto col = 0; col < builder.headers[level].size(); ++col)
            {
                if (state.colmap[level].ltov[col] == -1)
                {
                    state.colmap[level].ltov[col] = col;
                    state.colmap[level].vtol[col] = col;
                }

                auto& header = builder.headers[level][col];
                auto hdiff = header.extent.GetWidth() - header.content.GetWidth() - ((header.props & COL_Sortable) ? builder.btnsz : 0.f);
                auto vdiff = builder.headerHeights[level] - header.content.GetHeight();

                header.offset = header.content.Min;

                auto height = header.extent.GetHeight();
                header.extent.Min.y = ypos;
                header.extent.Max.y = header.extent.Min.y + height;

                height = header.content.GetHeight();
                header.content.Min.y = ypos;
                header.content.Max.y = header.content.Min.y + height;

                if (hdiff >= 2.f) header.content.TranslateX(hdiff * 0.5f);
                if (vdiff >= 2.f) header.content.TranslateY(vdiff * 0.5f);

                header.offset.x = header.content.Min.x - header.offset.x;
                header.offset.y = header.content.Min.y - header.offset.y;
                hextent.Min = ImMin(hextent.Min, header.extent.Min);
                hextent.Max = ImMax(hextent.Max, header.extent.Max);
            }

            ypos += builder.headerHeights[level] + config.gridwidth;
            builder.headerHeight += builder.headerHeights[level] + config.gridwidth;
            if (state.headerStates[level].size() <= builder.headers[level].size())
                state.headerStates[level].expand_and_create(builder.headers[level].size());
        }

        auto& ctx = GetContext();
        builder.phase = ItemGridConstructPhase::HeaderPlacement;
        std::pair<int16_t, int16_t> movingColRange = { INT16_MAX, -1 }, nextMovingRange = { INT16_MAX, -1 };
        ctx.ToggleDeferedRendering(false, false);
        ctx.deferEvents = false;
        BEGIN_WIDGET_LOG(builder.id, hextent);
        BEGIN_LOG_ARRAY("itemgrid-headers");

        for (auto level = 0; level < builder.levels; ++level)
        {
            auto frozenWidth = 0.f;

            for (int16_t vcol = 0; vcol < (int16_t)headers[level].size(); ++vcol)
            {
                auto col = state.colmap[level].vtol[vcol];

                auto& hdr = headers[level][col];
                auto isBeingMoved = state.drag.column == vcol && state.drag.level == level;
                auto hshift = (config.frozencols == col + 1) ? -state.altscroll.state.pos.x :
                    (config.frozencols <= col) ? -state.scroll.state.pos.x : 0.f;
                builder.currCol = col;

                if (isBeingMoved)
                {
                    auto movex = io.mousepos.x - state.drag.startPos.x;
                    hdr.extent.TranslateX(movex);
                    hdr.content.TranslateX(movex);
                    nextMovingRange = { col, col };
                }
                else if (hdr.parent >= movingColRange.first && hdr.parent <= movingColRange.second)
                {
                    auto movex = io.mousepos.x - state.drag.startPos.x;
                    hdr.extent.TranslateX(movex);
                    hdr.content.TranslateX(movex);
                    nextMovingRange.first = std::min(nextMovingRange.first, col);
                    nextMovingRange.second = std::max(nextMovingRange.second, col);

                    if (level == builder.levels - 1)
                        builder.movingCols = nextMovingRange;
                }
                else
                {
                    if (col == config.frozencols)
                        renderer.SetClipRect(builder.origin + ImVec2{ frozenWidth, 0.f }, builder.origin + builder.size);
                    else if (col < config.frozencols)
                        frozenWidth = (hdr.extent.Max.x - builder.origin.x);

                    ImVec2 shift{ hshift, 0.f }, gridline{ config.gridwidth, config.gridwidth };
                    hdr.extent.Translate(shift);
                    Config.renderer->DrawRect(hdr.extent.Min - gridline, hdr.extent.Max + gridline,
                        config.gridcolor, false, config.gridwidth);
                    renderer.SetClipRect(hdr.extent.Min, hdr.extent.Max);

                    if (config.header == nullptr)
                    {
                        const auto style = context.GetStyle(state.headerStates[level][col]);
                        DrawBackground(hdr.extent.Min, hdr.extent.Max, style, renderer);
                    }
                    else
                    {
                        const auto style = context.GetStyle(config.state);
                        DrawBackground(hdr.extent.Min, hdr.extent.Max, style, renderer);
                    }

                    if (hdr.props & COL_Sortable)
                    {
                        auto vdiff = builder.headerHeights[level] - builder.btnsz;
                        if (vdiff >= 2.f) vdiff *= 0.5f;
                        ctx.deferedRenderer->Render(*Config.renderer, ImVec2{ hdr.offset.x + hshift, vdiff },
                            hdr.sortIndicatorRange.primitives.first, hdr.sortIndicatorRange.primitives.second);
                    }

                    ctx.deferedRenderer->Render(*Config.renderer, hdr.offset + shift,
                        hdr.range.primitives.first, hdr.range.primitives.second);
                    auto res = ctx.HandleEvents(hdr.offset + shift, hdr.range.events.first, hdr.range.events.second);
                    auto interacted = res.event != WidgetEvent::None;
                    renderer.ResetClipRect();

                    auto ishovered = ImRect{ hdr.extent.Min, hdr.extent.Max }.Contains(io.mousepos);
                    state.headerStates[level][col] = io.isLeftMouseDown() && ishovered ? WS_Pressed | WS_Hovered :
                        ishovered ? WS_Hovered : WS_Default;
                    if (HandleContextMenu(builder.id, hdr.extent, io))
                    {
                        WidgetContextData::RightClickContext.row = level;
                        WidgetContextData::RightClickContext.col = col;
                        WidgetContextData::RightClickContext.isHeader = true;
                    }

                    if (!interacted)
                    {
                        if (vcol > 0)
                        {
                            auto prevcol = state.colmap[level].vtol[vcol - 1];
                            if (headers[level][prevcol].props & COL_Resizable)
                                if (HandleColumnResize(Span{ headers, headers + GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL },
                                    hdr.extent, state, io.mousepos, level, col, io))
                                    builder.resizecol = col;
                        }

                        if (hdr.props & COL_Moveable)
                            HandleColumnReorder(Span{ headers, headers + GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL },
                                state, io.mousepos, level, vcol, io);

                        if (hdr.extent.Contains(io.mousepos) && (hdr.props & COL_Sortable))
                        {
                            if (io.clicked())
                            {
                                if (state.sortedCol != col || state.sortedLevel != level)
                                    state.sortedAscending = (hdr.props & COL_InitialSortedAscending);
                                else
                                    state.sortedAscending = !state.sortedAscending;
                                state.sortedCol = col;
                                state.sortedLevel = level;
                                result.event = WidgetEvent::Clicked;
                                result.col = col; result.row = -1;
                                result.order = state.sortedAscending;
                            }

                            Config.platform->SetMouseCursor(MouseCursor::Grab);
                        }
                    }
                }

                BEGIN_LOG_OBJECT("itemgrid-header");
                LOG_TEXT(hdr.name); LOG_TEXT(hdr.id);
                LOG_COLOR(hdr.bgcolor);
                LOG_COLOR(hdr.fgcolor);
                LOG_NUM(hdr.highlighted);
                LOG_NUM(hdr.selected);
                END_LOG_OBJECT();
            }

            if (config.frozencols != -1) renderer.ResetClipRect();
        }
        
        END_LOG_ARRAY();
        builder.nextpos.y = ypos - state.scroll.state.pos.y;
        builder.nextpos.x = builder.origin.x;
        builder.startY = builder.nextpos.y;
        builder.phase = ItemGridConstructPhase::Headers;
        builder.currlevel = builder.levels - 1;
        builder.totalsz.x = builder.headers[builder.currlevel].back().extent.Max.x + config.gridwidth;
        ctx.ToggleDeferedRendering(false, false);
        ctx.deferEvents = false;
        return result;
    }

    static void ExtractColumnProps(ColumnProps& colprops, const ItemGridPersistentState& state, ItemGridBuilder& builder,
        const ItemGridConfig& config, const ItemGridItemProps& props, const std::pair<float, float>& bounds, int16_t col,
        int32_t row, int16_t depth, bool selected = false, bool highlighted = false);
    static ImVec2 RenderItemGridFilterCell(WidgetContextData& context, ItemGridBuilder& builder,
        ItemGridPersistentState& state, const ItemGridConfig& config, float maxh, int16_t col,
        WidgetDrawResult& result);

    WidgetDrawResult AddFilterRow()
    {
        WidgetDrawResult result;
        auto& context = *WidgetContextData::CurrentItemGridContext;
        auto& builder = context.itemGrids.top();
        auto& state = context.GridState(builder.id);
        const auto& config = context.GetState(builder.id).state.grid;
        auto& ctx = GetContext();
        auto startx = builder.headers[builder.levels - 1][0].content.Min.x;
		builder.phase = ItemGridConstructPhase::FilterRow;

        auto coloffset = 1;
        auto maxh = 0.f, starty = builder.nextpos.y;
        builder.currentY = builder.nextpos.y;
        builder.nextpos.y += config.cellpadding.y;

        if (builder.headers[GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL].empty())
            builder.headers[GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL].resize(builder.headers[builder.levels - 1].size());

        // Add filter row for each column
        for (auto vcol = 0; vcol < builder.headers[builder.levels - 1].size(); vcol += coloffset)
        {
            auto col = state.colmap[builder.levels - 1].vtol[vcol];

            if (col < builder.movingCols.first || col > builder.movingCols.second)
            {
                auto& colprops = builder.headers[GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL][col];

                builder.currCol = col;
                builder.currRow = -1;
                ctx.ToggleDeferedRendering(true, false);
                ctx.deferEvents = true;
                std::pair<float, float> bounds;

				ItemGridItemProps props{};
                auto& header = builder.headers[builder.levels - 1][col];
                bounds.first = header.extent.Min.x + ((col == 0) ? builder.cellIndent : 0.f) + config.cellpadding.x;
                bounds.second = header.extent.Max.x - config.cellpadding.x;
                ExtractColumnProps(colprops, state, builder, config, props, bounds, col, -1, builder.depth);

                builder.nextpos.x = bounds.first;
                ctx.adhocLayout.top().nextpos = builder.nextpos;
                ctx.RecordDeferRange(header.range, true);

                auto id = [&] {
                    if (header.id.empty())
                    {
                        static char buffer[256] = { 0 };
                        auto sz = std::snprintf(buffer, 255, "[itemgrid-%d][filter-%d]", builder.id, col);
                        auto id = GetIdFromString(std::string_view{ buffer, (std::size_t)sz }, WT_TextInput).first;
                        header.genid = id;
                        return id;
                    }
                    else
                    {
                        auto id = GetIdFromString(header.id, WT_TextInput).first;
                        header.genid = id;
                        return id;
                    }
                }();
				auto& filterstate = CreateWidgetConfig(id).state.input;
                filterstate.out = Span<char>{ colprops.filterout.data(), (int)colprops.filterout.size() };
				auto style = ctx.GetStyle(props.disabled ? WS_Disabled : filterstate.state, id);
				ImRect cellextent{ { bounds.first, header.extent.Max.y + config.cellpadding.y + config.gridwidth },
                    { bounds.second, header.extent.Max.y + style.font.size + config.cellpadding.y + config.gridwidth } };
                cellextent.Min += { config.gridwidth, config.gridwidth };
				cellextent.Max -= { config.gridwidth, config.gridwidth };
                LayoutItemDescriptor fitem;
                DetermineBounds({ style.dimension.x, style.font.size }, {}, filterstate.suffix, cellextent.Min, 
                    fitem, style, *Config.renderer, ToBottomRight, {});
                TextInputImpl(id, filterstate, style, cellextent, cellextent, fitem.prefix, fitem.suffix,
                    ctx.GetRenderer(), Config.platform->CurrentIO());

                ctx.RecordDeferRange(header.range, false);

                builder.maxCellExtent = cellextent.Max;
                colprops.content = colprops.extent = cellextent;
                colprops.offset = cellextent.GetSize();

                auto height = cellextent.GetHeight();
                maxh = std::max(maxh, height);
                coloffset = props.colspan;
                builder.maxCellExtent = ImVec2{};
            }
        }

        ctx.ToggleDeferedRendering(false, false);
        ctx.deferEvents = false;
        BEGIN_LOG_ARRAY("itemgrid-filters");

        // Draw frozen columns first
        ImVec2 frozensz;
        auto vcol = 0;
        for (; vcol < builder.headers[builder.levels - 1].size(); vcol += coloffset)
        {
            auto col = state.colmap[builder.levels - 1].vtol[vcol];
            if (col < config.frozencols)
            {
                auto sz = RenderItemGridFilterCell(ctx, builder, state, config, maxh, col, result);
                frozensz = ImMax(frozensz, sz);
            }
            else break;
        }

        // Draw non-frozen columns after setting appropriate clipping rect for frozen part
        Config.renderer->SetClipRect(builder.origin + ImVec2{ frozensz.x, 0.f }, builder.origin + builder.size);
        for (; vcol < builder.headers[builder.levels - 1].size(); vcol += coloffset)
        {
            auto col = state.colmap[builder.levels - 1].vtol[vcol];
            if (col < builder.movingCols.first || col > builder.movingCols.second)
                RenderItemGridFilterCell(ctx, builder, state, config, maxh, col, result);
        }

        END_LOG_ARRAY();
        Config.renderer->ResetClipRect();
        builder.nextpos.y += maxh + config.cellpadding.y + config.gridwidth;
        builder.nextpos.x = startx;
        ctx.adhocLayout.top().nextpos = builder.nextpos;
        ctx.ClearDeferredData();

        builder.totalsz.y = builder.nextpos.y;
        builder.totalsz.x = builder.headers[builder.currlevel].back().extent.Max.x + config.gridwidth;
        builder.filterRowHeight = builder.nextpos.y - starty;

        return result;
    }

    void PopulateItemGrid(int totalRows, ItemGridPopulateMethod method)
    {
        auto& context = *WidgetContextData::CurrentItemGridContext;
        auto& builder = context.itemGrids.top();
        builder.method = method;
        builder.rowcount = totalRows;
    }

    void AddEpilogueRows(int count, const ItemGridConfig::EpilogueRowProviders& providers)
    {
        auto& context = *WidgetContextData::CurrentItemGridContext;
        auto& builder = context.itemGrids.top();
        auto& config = context.GetState(builder.id).state.grid;
        builder.epilogueRowCount = count;
        config.epilogue = providers;
    }

    static float HAlignCellContent(ItemGridBuilder& builder, const ItemGridConfig& config, int16_t col,
        float required, float available)
    {
        auto alignment = builder.headers[builder.levels - 1][col].alignment;
        auto hdiff = 0.f;

        if (alignment & TextAlignHCenter)
        {
            hdiff = std::max(0.f, available - required) * 0.5f;
        }
        else if (alignment & TextAlignRight)
        {
            hdiff = std::max(0.f, available - required);
        }

        return hdiff;
    }

    static float VAlignCellContent(ItemGridBuilder& builder, const ItemGridConfig& config, int16_t col,
        float required, float available)
    {
        auto alignment = builder.headers[builder.levels - 1][col].alignment;
        auto vdiff = 0.f;

        if (alignment & TextAlignVCenter)
        {
            vdiff = std::max(0.f, available - required) * 0.5f;
        }
        else if (alignment & TextAlignBottom)
        {
            vdiff = std::max(0.f, available - required);
        }

        return vdiff;
    }

    static void DrawItemDescendentSymbol(WidgetContextData& context, ItemGridBuilder& builder,
        const ItemGridItemProps& props, const ColumnProps& colprops)
    {
        if (builder.currCol == 0)
        {
            ImVec2 start = builder.nextpos;
            auto style = context.GetStyle(props.disabled ? WS_Disabled :
                colprops.selected ? WS_Selected : colprops.highlighted ? WS_Hovered : WS_Default);
            ImVec2 size = ImVec2{ style.font.size * 0.4f, style.font.size * 0.5f };
            auto vdiff = ((props.alignment & TextAlignVCenter) ? (style.font.size - size.y) * 0.5f :
                (props.alignment & TextAlignBottom) ? (style.font.size - size.y) : 0.f) + (0.2f * style.font.size);

            if (props.vstate != ItemDescendentVisualState::NoDescendent)
            {
                auto& renderer = context.GetRenderer();
                renderer.SetClipRect(start, start + ImVec2{ style.font.size, style.font.size });
                DrawSymbol(start + ImVec2{ 0.f, vdiff }, size, {}, props.vstate == ItemDescendentVisualState::Collapsed ?
                    SymbolIcon::RightTriangle : SymbolIcon::DownTriangle, colprops.fgcolor, colprops.fgcolor, 1.f,
                    renderer);
                builder.btnsz = style.font.size;
                renderer.ResetClipRect();
            }

            builder.nextpos.x += size.x;
        }
    }

    static WidgetDrawResult PopulateData(int totalRows);

    static bool IsItemHighlighted(const ItemGridPersistentState& state, const ItemGridConfig& config, int32_t row, int16_t col, int16_t depth)
    {
        auto highlightRow = (config.highlights & IG_HighlightRows) != 0,
            highlightCol = (config.highlights & IG_HighlightColumns) != 0,
            highlightCell = (config.highlights & IG_HighlightCell) != 0;
        if ((state.cellstate.state & WS_Hovered) && (depth == state.cellstate.depth))
            if ((highlightRow && !highlightCol && (state.cellstate.row == row)) ||
                (highlightCol && !highlightRow && (state.cellstate.col == col)) ||
                (highlightCol && highlightRow && ((state.cellstate.col == col) || (state.cellstate.row == row))) ||
                (highlightCell && ((state.cellstate.col == col) && (state.cellstate.row == row))))
                return true;
        return false;
    }

    static bool IsItemSelected(const ItemGridPersistentState& state, const ItemGridConfig& config, int32_t row, int16_t col, int16_t depth)
    {
        auto selectRow = (config.selection & IG_SelectRow) != 0;
        auto selectCol = (config.selection & IG_SelectColumn) != 0;
        auto selectCell = (config.selection & IG_SelectCell) != 0;

        for (auto idx = 0; idx < state.selections.size(); ++idx)
        {
            if (selectCell)
            {
                if (state.selections[idx].row == row && state.selections[idx].col == col &&
                    state.selections[idx].depth == depth)
                    return true;
            }
            else if (selectRow)
            {
                if (state.selections[idx].row == row && state.selections[idx].depth == depth)
                    return true;
            }
            else
            {
                if (state.selections[idx].col == col && state.selections[idx].depth == depth)
                    return true;
            }
        }

        return false;
    }

    static void ExtractColumnProps(ColumnProps& colprops, const ItemGridPersistentState& state, ItemGridBuilder& builder,
        const ItemGridConfig& config, const ItemGridItemProps& props, const std::pair<float, float>& bounds, int16_t col,
        int32_t row, int16_t depth, bool selected, bool highlighted)
    {
        colprops.selected = selected;
        colprops.highlighted = highlighted;
        auto style = WidgetContextData::GetStyle(props.disabled ? WS_Disabled :
            colprops.selected ? WS_Selected : colprops.highlighted ? WS_Hovered : WS_Default);

        if (props.highlightCell)
        {
            colprops.bgcolor = props.highlightBgColor;
            colprops.fgcolor = props.highlightFgColor;
        }
        else if (props.selectCell)
        {
            colprops.bgcolor = props.selectionBgColor;
            colprops.fgcolor = props.selectionFgColor;
        }
        else if (colprops.selected)
        {
            colprops.bgcolor = config.selectionBgColor;
            colprops.fgcolor = config.selectionFgColor;
        }
        else if (colprops.highlighted) [[likely]]
        {
            colprops.bgcolor = config.highlightBgColor;
            colprops.fgcolor = config.highlightFgColor;
        }
        else
        {
            colprops.bgcolor = style.bgcolor;
            colprops.fgcolor = style.fgcolor;
        }
    }

    static std::string_view InvokeItemGridCellContent(WidgetContextData& context, ItemGridBuilder& builder,
        const ItemGridPersistentState& state, const ItemGridConfig& config, const ItemGridItemProps& props,
        ColumnProps& colprops, const std::pair<float, float>& bounds, int16_t col, int32_t row, int32_t rowid,
        int32_t itemprops, ItemGridConfig::CellWidgetProviderT cellwidget, ItemGridConfig::CellContentProviderT cellcontent)
    {
        assert(cellwidget || cellcontent);
        assert(cellwidget || (!props.isContentWidget && cellcontent));
        std::string_view result;

        if (props.isContentWidget || (!cellcontent && cellwidget))
            cellwidget({ builder.parentId, rowid, itemprops, row, col, builder.depth, bounds });
        else
        {
            auto [text, txtype] = cellcontent({ builder.parentId, rowid, itemprops, row, col, builder.depth, bounds });
            auto style = context.GetStyle(props.disabled ? WS_Disabled :
                colprops.selected ? WS_Selected : colprops.highlighted ? WS_Hovered : WS_Default);
            auto textsz = GetTextSize(txtype, text, style.font, props.wrapText ? 
                (bounds.second - builder.nextpos.x - config.cellpadding.x) : -1.f, *Config.renderer);
            builder.maxCellExtent = builder.nextpos + textsz;
            ImRect textrect{ builder.nextpos, builder.nextpos + textsz };
            ImVec2 textend{ bounds.second, builder.nextpos.y + textsz.y };
            textrect.Max = ImMax(textrect.Max, textend);
            style.fgcolor = colprops.fgcolor;
            DrawText(builder.nextpos, textend, textrect, text, props.disabled, style, *context.deferedRenderer);
            result = text;
        }

        if (config.frozencols == col + 1)
            builder.maxColWidth = std::max(builder.maxColWidth, builder.maxCellExtent.x - builder.nextpos.x);
    
        return result;
    }

    static void UpdateSingleSelection(ItemGridPersistentState& state, const ItemGridConfig& config, int32_t col, int32_t row, int32_t depth)
    {
        if (config.selection & IG_SelectRow)
            state.selections.emplace_back(row, -1, depth);
        else if (config.selection & IG_SelectColumn)
            state.selections.emplace_back(-1, col, depth);
        else
            state.selections.emplace_back(row, col, depth);
    }

    static void UpdateContiguosSelection(ItemGridPersistentState& state, ItemGridBuilder& builder, const ItemGridConfig& config, int32_t index, int32_t depth)
    {
        static Vector<ItemGridPersistentState::ItemId, int16_t> temp;
        auto selectRow = (config.selection & IG_SelectRow) != 0;

        const auto last = state.selections.back();
        auto start = std::min(selectRow ? last.row : last.col, index);
        auto end = std::max(selectRow ? last.row : last.col, index);
        auto idx = 0;

        for (; idx < state.selections.size(); ++idx)
            if (state.selections[idx].col < start)
                temp.emplace_back(state.selections[idx]);
            else break;

        if (!selectRow)
        {
            for (; start <= end; ++start)
                temp.emplace_back(-1, start, depth);
        }
        else
        {
            auto firstPoint = state.lastSelection == -1.f;
            auto from = firstPoint ? state.currentSelection : std::min(state.lastSelection, state.currentSelection);
            auto to = firstPoint ? state.currentSelection : std::max(state.lastSelection, state.currentSelection);

            for (const auto& range : builder.rowYs)
            {
                if ((range.from <= from && range.to >= from) || (range.to >= to && range.from <= to) ||
                    (range.from > from && range.to < to))
                {
                    auto exists = false;
                    for (const auto& select : state.selections)
                        if (select.depth == range.depth && select.row == range.row)
                        {
                            exists = true; break;
                        }

                    if (!exists)
                        temp.emplace_back(range.row, -1, range.depth);
                }
            }
        }

        for (; idx < state.selections.size(); ++idx)
            if (state.selections[idx].col > end)
                temp.emplace_back(state.selections[idx]);

        state.selections.clear(true);
        state.selections.resize(temp.size(), false);
        for (idx = 0; idx < temp.size(); ++idx)
            state.selections.emplace_back(temp[idx]);
    }

    static void UpdateItemSelection(ItemGridPersistentState& state, ItemGridBuilder& builder, const ItemGridConfig& config, const IODescriptor& io, int32_t col, int32_t row, int32_t depth)
    {
        if (config.selection & IG_SelectMultiItem)
        {
            if (io.modifiers & CtrlKeyMod)
                UpdateSingleSelection(state, config, col, row, depth);
            else if (io.modifiers & ShiftKeyMod)
                if (config.selection & IG_SelectRow)
                {
                    if (state.selections.empty()) state.selections.emplace_back(row, -1);
                    else UpdateContiguosSelection(state, builder, config, row, depth);
                }
                else if (config.selection & IG_SelectColumn)
                {
                    if (state.selections.empty()) state.selections.emplace_back(col, -1);
                    else UpdateContiguosSelection(state, builder, config, col, -1);
                }
                else // TODO: Ambiguous here, need to decide
                    state.selections.emplace_back(row, col, depth);
            else
            {
                state.selections.clear(true);
                UpdateSingleSelection(state, config, col, row, depth);
            }
        }
        else if (config.selection & IG_SelectContiguosItem)
        {
            if (io.modifiers & ShiftKeyMod)
                if (config.selection & IG_SelectRow)
                {
                    if (state.selections.empty()) state.selections.emplace_back(row, -1);
                    else UpdateContiguosSelection(state, builder, config, row, depth);
                }
                else if (config.selection & IG_SelectColumn)
                {
                    if (state.selections.empty()) state.selections.emplace_back(col, -1);
                    else UpdateContiguosSelection(state, builder, config, col, -1);
                }
                else
                    state.selections.emplace_back(row, col);
            else
            {
                state.selections.clear(true);
                UpdateSingleSelection(state, config, col, row, depth);
            }
        }
        else
        {
            state.selections.clear(true);
            UpdateSingleSelection(state, config, col, row, depth);
        }

        if ((config.selection & IG_SelectRow) && !(io.modifiers & ShiftKeyMod))
        {
            state.lastSelection = -1.f;
        }

        LOG_NUM2("selected-row-count", state.selections.size());
    }

    static int32_t GetRowId(const ItemGridBuilder& builder, const ItemGridConfig& config, int32_t row, bool epilogue)
    {
        return epilogue ? builder.rowcount + row - 1 : config.isTree ? builder.perDepthRowCount[builder.depth] : row;
    }

    static ImVec2 RenderItemGridCell(WidgetContextData& context, ItemGridBuilder& builder,
        ItemGridPersistentState& state, const ItemGridConfig& config, float maxh, int16_t col,
        int32_t row, bool inverted, WidgetDrawResult& result)
    {
        const auto& header = builder.headers[builder.levels - 1][col];
        auto& cellGeometry = builder.headers[GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL][col];
        ImVec2 available{ header.content.GetWidth(), maxh }, required = cellGeometry.offset;
        if (col == 0) available.x -= builder.cellIndent;

        cellGeometry.extent.Max.y = cellGeometry.extent.Min.y + maxh + (2.f * config.cellpadding.y);
        cellGeometry.content.Max.y = cellGeometry.content.Min.y + maxh;

        auto hdiff = HAlignCellContent(builder, config, col, required.x, available.x);
        auto vdiff = VAlignCellContent(builder, config, col, required.y, available.y);
        const auto& range = header.range;

        if (inverted)
        {
            auto shifty = -(maxh + (2.f * config.cellpadding.y));
            cellGeometry.extent.TranslateY(shifty);
            cellGeometry.content.TranslateY(shifty);
			vdiff += shifty;
        }

        ImVec2 shift{ 0.f, 0.f }, gridline{ config.gridwidth, config.gridwidth };
        Config.renderer->DrawRect(cellGeometry.extent.Min - gridline + shift, cellGeometry.extent.Max + gridline + shift,
            config.gridcolor, false, config.gridwidth);

        if (IsColorVisible(cellGeometry.bgcolor))
            Config.renderer->DrawRect(cellGeometry.extent.Min + shift, cellGeometry.extent.Max + shift,
                cellGeometry.bgcolor, true);

        Config.renderer->SetClipRect(cellGeometry.content.Min + shift, cellGeometry.content.Max + shift);
        context.deferedRenderer->Render(*Config.renderer, ImVec2{ hdiff, vdiff },
            range.primitives.first, range.primitives.second);
        Config.renderer->ResetClipRect();

        auto res = context.HandleEvents(ImVec2{ hdiff, vdiff }, range.events.first, range.events.second);
        //cellGeometry.extent.TranslateX(hshift); cellGeometry.content.TranslateX(hshift);
        if (res.event != WidgetEvent::None) result = res;
        else
        {
            auto io = Config.platform->CurrentIO();
            if (ImRect{ builder.origin, builder.origin + builder.size }.Contains(io.mousepos))
            {
                auto itemToggled = false;
                auto isClicked = io.clicked();

                if (col == 0)
                {
                    auto offset = ImVec2{ hdiff, vdiff };
                    auto btnstart = cellGeometry.content.Min + offset;
                    if (ImRect{ btnstart, btnstart + ImVec2{ builder.btnsz, builder.btnsz } }.Contains(io.mousepos))
                    {
                        if (isClicked)
                        {
                            result.event = WidgetEvent::Clicked;
                            result.row = row; result.col = -1;
                            itemToggled = true;
                        }
                    }
                }

                if (cellGeometry.extent.Contains(io.mousepos) && !itemToggled)
                {
                    if (isClicked && !state.scroll.state.mouseDownOnVGrip && !state.scroll.state.mouseDownOnHGrip)
                    {
                        state.lastSelection = state.currentSelection;
                        state.currentSelection = io.mousepos.y - builder.startY;
                        state.cellstate.state |= WS_Selected;
                        builder.clickedItem.row = GetRowId(builder, config, row, inverted);
                        builder.clickedItem.col = col;
                        builder.clickedItem.depth = builder.depth;
                    }
                    else
                        state.cellstate.state = io.isLeftMouseDown() ? WS_Pressed : WS_Hovered;

                    result.event = isClicked ? WidgetEvent::Clicked : WidgetEvent::Hovered;
                    result.row = row; result.col = col; result.depth = builder.depth;
                    state.cellstate.row = row; state.cellstate.col = col;
                    state.cellstate.depth = builder.depth;
                }

                if (HandleContextMenu(builder.id, cellGeometry.content, io))
                {
                    WidgetContextData::RightClickContext.row = row;
                    WidgetContextData::RightClickContext.col = col;
                }
            }
        }

        BEGIN_LOG_OBJECT("itemgrid-cell");
        LOG_NUM(row); LOG_NUM(col);
        LOG_TEXT2("cell-text", builder.cellvals[col].first);
        LOG_RECT(cellGeometry.extent); LOG_RECT(cellGeometry.content);
        LOG_COLOR(cellGeometry.bgcolor); LOG_COLOR(cellGeometry.fgcolor);
        LOG_NUM2("descendant", builder.cellvals[col].second);
        END_LOG_OBJECT();
        return cellGeometry.extent.GetSize();
    }

    static ImVec2 RenderItemGridFilterCell(WidgetContextData& context, ItemGridBuilder& builder,
        ItemGridPersistentState& state, const ItemGridConfig& config, float maxh, int16_t col,
        WidgetDrawResult& result)
    {
        const auto& header = builder.headers[builder.levels - 1][col];
        auto& cellGeometry = builder.headers[GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL][col];
        ImVec2 available{ header.content.GetWidth(), maxh }, required = cellGeometry.offset;
        if (col == 0) available.x -= builder.cellIndent;
        cellGeometry.extent.Max.y = cellGeometry.extent.Min.y + maxh + (2.f * config.cellpadding.y);
        cellGeometry.content.Max.y = cellGeometry.content.Min.y + maxh;

        auto hdiff = HAlignCellContent(builder, config, col, required.x, available.x);
        auto vdiff = VAlignCellContent(builder, config, col, required.y, available.y);
        auto hshift = 0.f;
        const auto& range = header.range;

        ImVec2 shift{ hshift, 0.f }, gridline{ config.gridwidth, config.gridwidth };
        Config.renderer->DrawRect(cellGeometry.extent.Min - gridline + shift, cellGeometry.extent.Max + gridline + shift,
            config.gridcolor, false, config.gridwidth);

        if (IsColorVisible(cellGeometry.bgcolor))
            Config.renderer->DrawRect(cellGeometry.extent.Min + shift, cellGeometry.extent.Max + shift,
                cellGeometry.bgcolor, true);

        Config.renderer->SetClipRect(cellGeometry.content.Min + shift, cellGeometry.content.Max + shift);
        context.deferedRenderer->Render(*Config.renderer, ImVec2{ hdiff + hshift, vdiff },
            range.primitives.first, range.primitives.second);
        Config.renderer->ResetClipRect();

        auto res = context.HandleEvents(ImVec2{ hdiff + hshift, vdiff }, range.events.first, range.events.second);
        cellGeometry.extent.TranslateX(hshift); cellGeometry.content.TranslateX(hshift);
        if (res.event != WidgetEvent::None) result = res;
        else
        {
            auto io = Config.platform->CurrentIO();
            if (ImRect{ builder.origin, builder.origin + builder.size }.Contains(io.mousepos))
            {
                if (HandleContextMenu(builder.id, cellGeometry.content, io))
                {
                    WidgetContextData::RightClickContext.row = -1;
                    WidgetContextData::RightClickContext.col = col;
                }
            }
        }

        BEGIN_LOG_OBJECT("itemgrid-filter-cell");
        LOG_NUM(col);
        const auto& filtertext = context.GetState(header.genid).state.input.text;
        LOG_TEXT(filtertext);
        LOG_RECT(cellGeometry.extent); LOG_RECT(cellGeometry.content);
        LOG_COLOR(cellGeometry.bgcolor); LOG_COLOR(cellGeometry.fgcolor);
        END_LOG_OBJECT();
        return cellGeometry.extent.GetSize();
    }

    static void RecordRowYRange(ItemGridBuilder& builder, const ItemGridConfig& config, float height, int32_t totalRows, int32_t row, bool epilogue)
    {
        if (config.selection & IG_SelectRow)
        {
            if (builder.rowYs.empty()) builder.rowYs.expand(totalRows, true);
            auto& range = builder.rowYs.emplace_back();
            range.depth = builder.depth; range.row = GetRowId(builder, config, row, epilogue);
            range.from = builder.currentY - builder.startY; range.to = range.from + height + config.cellpadding.y;
            builder.currentY = range.to;
			if(config.isTree)
            	builder.perDepthRowCount[builder.depth]++;
        }
    }

    static void RecordCellContentDimension(ItemGridBuilder& builder, ColumnProps& colprops, const ItemGridConfig& config,
        const std::pair<float, float>& bounds, const ImRect& extent, float height)
    {
        colprops.offset = { builder.maxCellExtent.x - bounds.first, height };
        colprops.content.Min.x = extent.Min.x + config.cellpadding.x;
        colprops.content.Min.y = builder.nextpos.y;
        colprops.content.Max.x = extent.Max.x - config.cellpadding.x;
        colprops.extent.Min.x = extent.Min.x;
        colprops.extent.Min.y = builder.nextpos.y - config.cellpadding.y;
        colprops.extent.Max.x = extent.Max.x;
    }

    static void AddRowData(WidgetContextData& context, ItemGridBuilder& builder,
        ItemGridPersistentState& state, const ItemGridConfig& config, WidgetDrawResult& result,
        int totalRows)
    {
        auto row = 0;
        auto startx = builder.headers[builder.levels - 1][0].content.Min.x;
        builder.phase = ItemGridConstructPhase::Rows;
        if (builder.headers[GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL].empty())
            builder.headers[GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL].resize(builder.headers[builder.levels - 1].size());
        
        if ((config.selection & IG_SelectRow) && (builder.perDepthRowCount.empty()) && config.isTree)
        {
            builder.perDepthRowCount.resize(32, false);
            builder.perDepthRowCount[builder.depth] = 0;
        }

        builder.cellvals.resize(builder.headers[builder.levels - 1].size(), true);
        BEGIN_LOG_ARRAY("itemgrid-rows");

        while (totalRows > 0)
        {
            auto coloffset = 1;
            auto maxh = 0.f;
            auto rowid = GetRowId(builder, config, row, false);
            builder.currentY = builder.nextpos.y;
            builder.nextpos.y += config.cellpadding.y;

            // Determine cell geometry for current row
            for (auto vcol = 0; vcol < builder.headers[builder.levels - 1].size(); vcol += coloffset)
            {
                auto col = state.colmap[builder.levels - 1].vtol[vcol];

                if (col < builder.movingCols.first || col > builder.movingCols.second)
                {
                    auto selected = IsItemSelected(state, config, rowid, col, builder.depth);
                    auto highlighted = IsItemHighlighted(state, config, row, col, builder.depth);
                    auto itemprops = selected ? IG_Selected : 0;
                    itemprops |= highlighted ? IG_Highlighted : 0;
                    auto props = config.cellprops ? config.cellprops({ builder.parentId, rowid, itemprops, row, col, builder.depth })
                        : ItemGridItemProps{};
                    auto& colprops = builder.headers[GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL][col];

                    builder.currCol = col;
                    builder.currRow = row;
                    context.ToggleDeferedRendering(true, false);
                    context.deferEvents = true;
                    std::pair<float, float> bounds;

                    auto& header = builder.headers[builder.levels - 1][col];
                    bounds.first = header.extent.Min.x + ((col == 0) ? builder.cellIndent : 0.f) + config.cellpadding.x;
                    bounds.second = header.extent.Max.x - config.cellpadding.x;
                    ExtractColumnProps(colprops, state, builder, config, props, bounds, col, row, builder.depth, selected, highlighted);

                    builder.nextpos.x = bounds.first;
                    context.adhocLayout.top().nextpos = builder.nextpos;
                    context.RecordDeferRange(header.range, true);

                    if (vcol == 0 && props.vstate != ItemDescendentVisualState::NoDescendent)
                    {
                        DrawItemDescendentSymbol(context, builder, props, colprops);
                        builder.nextpos.x += config.cellpadding.x;
                    }

                    auto text = InvokeItemGridCellContent(context, builder, state, config, props, 
                        colprops, bounds, col, row, rowid, itemprops, config.cellwidget, config.cellcontent);
                    builder.cellvals.emplace_back(text, props.vstate);
                    context.RecordDeferRange(header.range, false);

                    auto height = builder.maxCellExtent.y - builder.nextpos.y;
                    maxh = std::max(maxh, height);
                    RecordCellContentDimension(builder, colprops, config, bounds, header.extent, height);
                    header.alignment = props.alignment;

                    if (vcol == 0)
                        builder.childState = std::make_pair(props.vstate, props.children);
                    coloffset = props.colspan;
                    builder.maxCellExtent = ImVec2{};
                }
            }

            context.ToggleDeferedRendering(false, false);
            context.deferEvents = false;
            BEGIN_LOG_ARRAY("itemgrid-columns");

            // Draw frozen columns first
            ImVec2 frozensz;
            auto vcol = 0;
            for (; vcol < builder.headers[builder.levels - 1].size(); vcol += coloffset)
            {
                auto col = state.colmap[builder.levels - 1].vtol[vcol];
                if (col < config.frozencols)
                {
                    auto sz = RenderItemGridCell(context, builder, state, config, maxh, col, row, false, result);
                    frozensz = ImMax(frozensz, sz);
                }
                else break;
            }

            // Draw non-frozen columns after setting appropriate clipping rect for frozen part
            Config.renderer->SetClipRect(builder.origin + ImVec2{ frozensz.x, 0.f }, builder.origin + builder.size);
            for (; vcol < builder.headers[builder.levels - 1].size(); vcol += coloffset)
            {
                auto col = state.colmap[builder.levels - 1].vtol[vcol];
                if (col < builder.movingCols.first || col > builder.movingCols.second)
                    RenderItemGridCell(context, builder, state, config, maxh, col, row, false, result);
            }
            
            END_LOG_ARRAY();
            Config.renderer->ResetClipRect();
            builder.nextpos.y += maxh + config.cellpadding.y + config.gridwidth;
            builder.nextpos.x = startx;
            context.adhocLayout.top().nextpos = builder.nextpos;

            RecordRowYRange(builder, config, maxh, totalRows, row, false);

            // Draw child rows
            if (builder.childState.first == ItemDescendentVisualState::Expanded &&
                builder.childState.second > 0)
            {
                builder.cellIndent += config.config.indent;
                builder.depth++;
				builder.parentId = rowid;
                auto res = PopulateData(builder.childState.second);
                if (res.event != WidgetEvent::None) result = res;
                builder.cellIndent -= config.config.indent;
                builder.depth--;
            }

            context.ClearDeferredData();
            --totalRows;
            ++row;
        }

        END_LOG_ARRAY();
        builder.totalsz.y = builder.nextpos.y;
        builder.totalsz.x = builder.headers[builder.currlevel].back().extent.Max.x + config.gridwidth;
    }

    static void PopulateEpilogueRows(WidgetContextData& context, ItemGridBuilder& builder,
        ItemGridPersistentState& state, const ItemGridConfig& config, WidgetDrawResult& result)
    {
        auto row = builder.epilogueRowCount;
        auto startx = builder.headers[builder.levels - 1][0].content.Min.x;
		auto nextpos = builder.nextpos;
        auto maxh = 0.f;
        builder.phase = ItemGridConstructPhase::EpilogRows;
        if (builder.headers[GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL].empty())
            builder.headers[GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL].resize(builder.headers[builder.levels - 1].size());

        builder.cellvals.resize(builder.headers[builder.levels - 1].size(), true);
        builder.currentY = builder.origin.y + builder.size.y;
		builder.epilogueHeight = builder.currentY;
        BEGIN_LOG_ARRAY("itemgrid-epilogue-rows");

        while (row > 0)
        {
            auto coloffset = 1;
            auto rowid = GetRowId(builder, config, row, true);
            nextpos.y = builder.currentY - (config.cellpadding.y + config.gridwidth);

            // Determine cell geometry for current row
            for (auto vcol = 0; vcol < builder.headers[builder.levels - 1].size(); vcol += coloffset)
            {
                auto col = state.colmap[builder.levels - 1].vtol[vcol];

                if (col < builder.movingCols.first || col > builder.movingCols.second)
                {
                    auto selected = IsItemSelected(state, config, rowid, col, builder.depth);
                    auto highlighted = IsItemHighlighted(state, config, row, col, builder.depth);
                    auto itemprops = selected ? IG_Selected : 0;
                    itemprops |= highlighted ? IG_Highlighted : 0;
                    auto props = config.epilogue.cellprops ? config.epilogue.cellprops({ builder.parentId, rowid, itemprops, row, col, builder.depth })
                        : ItemGridItemProps{};
                    auto& colprops = builder.headers[GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL][col];

                    builder.currCol = col;
                    builder.currRow = row;
                    context.ToggleDeferedRendering(true, false);
                    context.deferEvents = true;
                    std::pair<float, float> bounds;

                    auto& header = builder.headers[builder.levels - 1][col];
                    bounds.first = header.extent.Min.x + ((col == 0) ? builder.cellIndent : 0.f) + config.cellpadding.x;
                    bounds.second = header.extent.Max.x - config.cellpadding.x;
                    ExtractColumnProps(colprops, state, builder, config, props, bounds, col, row, builder.depth, selected, highlighted);

                    builder.nextpos.x = bounds.first;
                    context.adhocLayout.top().nextpos = nextpos;
                    context.RecordDeferRange(header.range, true);

                    auto text = InvokeItemGridCellContent(context, builder, state, config, props, 
                        colprops, bounds, col, row, rowid, itemprops, config.epilogue.cellwidget, config.epilogue.cellcontent);
                    builder.cellvals.emplace_back(text, props.vstate);
                    context.RecordDeferRange(header.range, false);

                    auto height = builder.maxCellExtent.y - nextpos.y;
                    maxh = std::max(maxh, height);
                    RecordCellContentDimension(builder, colprops, config, bounds, header.extent, height);
                    header.alignment = props.alignment;

                    if (vcol == 0)
                        builder.childState = std::make_pair(props.vstate, props.children);
                    coloffset = props.colspan;
                    builder.maxCellExtent = ImVec2{};
                }
            }

            context.ToggleDeferedRendering(false, false);
            context.deferEvents = false;
            BEGIN_LOG_ARRAY("itemgrid-epilogue-columns");

            // Draw frozen columns first
            ImVec2 frozensz;
            auto vcol = 0;
            for (; vcol < builder.headers[builder.levels - 1].size(); vcol += coloffset)
            {
                auto col = state.colmap[builder.levels - 1].vtol[vcol];
                if (col < config.frozencols)
                {
                    auto sz = RenderItemGridCell(context, builder, state, config, maxh, col, row, true, result);
                    frozensz = ImMax(frozensz, sz);
                }
                else break;
            }

            // Draw non-frozen columns after setting appropriate clipping rect for frozen part
            Config.renderer->SetClipRect(builder.origin + ImVec2{ frozensz.x, 0.f }, builder.origin + builder.size);
            for (; vcol < builder.headers[builder.levels - 1].size(); vcol += coloffset)
            {
                auto col = state.colmap[builder.levels - 1].vtol[vcol];
                if (col < builder.movingCols.first || col > builder.movingCols.second)
                    RenderItemGridCell(context, builder, state, config, maxh, col, row, true, result);
            }
            
            END_LOG_ARRAY();
            Config.renderer->ResetClipRect();
            builder.nextpos.y -= (maxh + config.cellpadding.y + config.gridwidth);
            builder.nextpos.x = startx;
            context.adhocLayout.top().nextpos = builder.nextpos;

            RecordRowYRange(builder, config, maxh, builder.epilogueRowCount, row, true);
            context.ClearDeferredData();
            --row; maxh = 0.f;
        }

        END_LOG_ARRAY();
		builder.epilogueHeight -= (builder.nextpos.y - maxh);
		builder.nextpos = nextpos;
		builder.totalsz.y += builder.epilogueHeight;
    }

    static void AddColumnData(WidgetContextData& context, ItemGridBuilder& builder,
        ItemGridPersistentState& state, const ItemGridConfig& config, WidgetDrawResult& result,
        const IODescriptor& io, int totalRows, int col)
    {
        std::pair<float, float> bounds;
        auto& header = builder.headers[builder.levels - 1][col];
        auto extent = header.extent;
        const auto totalw = extent.GetWidth() - (2.f * config.cellpadding.x);
        bounds.first = extent.Min.x + config.cellpadding.x + ((col == 0) ? builder.cellIndent : 0.f);
        bounds.second = extent.Max.x - config.cellpadding.x;
        BEGIN_LOG_ARRAY("itemgrid-column");

        for (auto row = 0; row < totalRows; ++row)
        {
            auto rowid = GetRowId(builder, config, row, false);
            auto selected = IsItemSelected(state, config, rowid, col, builder.depth);
            auto highlighted = IsItemHighlighted(state, config, row, col, builder.depth);
            auto itemprops = selected ? IG_Selected : 0;
            itemprops |= highlighted ? IG_Highlighted : 0;
            auto props = config.cellprops ? config.cellprops({ builder.parentId, rowid, itemprops, row, (int16_t)col, builder.depth }) 
                : ItemGridItemProps{};

            builder.currCol = col;
            builder.currRow = row;
            builder.nextpos.y += config.cellpadding.y;
            context.adhocLayout.top().nextpos = builder.nextpos;

            auto& colprops = builder.headers[GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL][col];
            ExtractColumnProps(colprops, state, builder, config, props, bounds, col, row, builder.depth, selected, highlighted);

            context.ToggleDeferedRendering(true, false);
            context.deferEvents = true;

            context.RecordDeferRange(header.range, true);
            InvokeItemGridCellContent(context, builder, state, config, props, colprops, bounds, col, row, 
                rowid, itemprops, config.cellwidget, config.cellcontent);
            context.RecordDeferRange(header.range, false);

            auto rowh = builder.maxCellExtent.y - builder.nextpos.y;
            RecordCellContentDimension(builder, colprops, config, bounds, extent, rowh);
            header.alignment = props.alignment;

            RenderItemGridCell(context, builder, state, config, rowh, col, row, false, result);
            RecordRowYRange(builder, config, rowh, totalRows, row, false);

            builder.nextpos.y += rowh + config.cellpadding.y + config.gridwidth;
            builder.maxCellExtent = ImVec2{};
        }

        END_LOG_ARRAY();
        builder.totalsz.y = builder.nextpos.y;
    }

    static WidgetDrawResult PopulateData(int totalRows)
    {
        WidgetDrawResult result;
        auto& context = *WidgetContextData::CurrentItemGridContext;
        auto& builder = context.itemGrids.top();
        auto& state = context.GridState(builder.id);
        auto& config = context.GetState(builder.id).state.grid;
        auto& renderer = context.GetRenderer();
        auto io = Config.platform->CurrentIO();
        auto& ctx = GetContext();
        assert(config.cellwidget != nullptr || config.cellcontent != nullptr);
        builder.rowcount = totalRows;

        if (builder.method == ItemGridPopulateMethod::ByRows) 
            AddRowData(ctx, builder, state, config, result, totalRows);
        else
        {
            if (builder.headers[GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL].empty())
                builder.headers[GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL].resize(builder.headers[builder.levels - 1].size());

            builder.phase = ItemGridConstructPhase::Columns;
            builder.nextpos.x += config.cellpadding.x + config.gridwidth;
            Config.renderer->SetClipRect({ builder.origin.x, builder.origin.y + builder.headerHeight },
                builder.origin + builder.size);
            BEGIN_LOG_ARRAY("itemgrid-columns");

            auto vcol = 0;
            float frozenWidth = 0.f;
            for (; vcol < builder.headers[builder.levels - 1].size(); vcol++)
            {
                auto col = state.colmap[builder.levels - 1].vtol[vcol];
                if (col < config.frozencols)
                {
                    auto ystart = builder.nextpos.y;
                    if (col < builder.movingCols.first || col > builder.movingCols.second)
                        AddColumnData(ctx, builder, state, config, result, io, totalRows, col);
                    builder.nextpos.y = ystart;
                    auto width = builder.headers[builder.levels - 1][col].extent.GetWidth() +
                        config.gridwidth - config.cellpadding.x;
                    builder.nextpos.x += width;
                    ctx.adhocLayout.top().nextpos = builder.nextpos;
                    frozenWidth += width;
                }
                else break;
            }

            Config.renderer->SetClipRect(builder.origin + ImVec2{ frozenWidth, 0.f }, builder.origin + builder.size);
            for (; vcol < builder.headers[builder.levels - 1].size(); vcol++)
            {
                auto col = state.colmap[builder.levels - 1].vtol[vcol];
                if (col < builder.movingCols.first || col > builder.movingCols.second)
                {
                    auto ystart = builder.nextpos.y;
                    if (col < builder.movingCols.first || col > builder.movingCols.second)
                        AddColumnData(ctx, builder, state, config, result, io, totalRows, col);
                    builder.nextpos.y = ystart;
                    auto width = builder.headers[builder.levels - 1][col].extent.GetWidth() +
                        config.gridwidth - config.cellpadding.x;
                    builder.nextpos.x += width;
                    ctx.adhocLayout.top().nextpos = builder.nextpos;
                }
            }
            Config.renderer->ResetClipRect();

            builder.totalsz.x = builder.nextpos.x;
            Config.renderer->ResetClipRect();
            END_LOG_ARRAY();
        }

        // Add epilogue rows
        if (builder.epilogueRowCount > 0)
		    PopulateEpilogueRows(ctx, builder, state, config, result);

        // Fix column dragging
        /*if (state.drag.column != -1)
        {
            auto col = state.colmap[builder.levels - 1].vtol[state.drag.column];
            auto hshift = -state.scroll.state.pos.x;
            auto hdrheight = 0.f;

            builder.phase = ItemGridConstructPhase::HeaderPlacement;
            std::pair<int16_t, int16_t> nextMovingRange = { INT16_MAX, -1 };

            for (auto level = 0; level < builder.levels; ++level)
            {
                auto maxh = 0.f, posx = builder.origin.x + hshift;

                for (int16_t vcol = 0; vcol < (int16_t)builder.headers[level].size(); ++vcol)
                {
                    auto& hdr = builder.headers[level][col];
                    auto isBeingMoved = state.drag.column == vcol && state.drag.level == level;
                    auto render = false;
                    builder.currCol = col;

                    if (isBeingMoved)
                    {
                        maxh = std::max(maxh, hdr.content.GetHeight());
                        nextMovingRange = { col, col };
                        render = true;
                    }
                    else if (hdr.parent >= nextMovingRange.first && hdr.parent <= nextMovingRange.second)
                    {
                        maxh = std::max(maxh, hdr.content.GetHeight());
                        nextMovingRange.first = std::min(nextMovingRange.first, col);
                        nextMovingRange.second = std::max(nextMovingRange.second, col);
                        render = true;
                    }

                    if (render)
                    {
                        renderer.SetClipRect(hdr.extent.Min, hdr.extent.Max);
                        builder.phase = ItemGridConstructPhase::HeaderPlacement;
                        ctx.deferedRenderer->Render(*Config.renderer, hdr.offset + ImVec2{ hshift, 0.f },
                            hdr.range.primitives.first, hdr.range.primitives.second);
                        auto res = ctx.HandleEvents(hdr.offset, hdr.range.events.first, hdr.range.events.second);
                        auto interacted = res.event != WidgetEvent::None;
                        renderer.ResetClipRect();
                    }

                    maxh = std::max(maxh, hdr.extent.GetHeight());
                }

                hdrheight += maxh;
            }

            builder.phase = ItemGridConstructPhase::Columns;

            for (auto vcol = 0; vcol < builder.headers[builder.levels - 1].size(); vcol++)
            {
                auto col = state.colmap[builder.levels - 1].vtol[vcol];
                if (col >= builder.movingCols.first && col <= builder.movingCols.second)
                    AddColumnData(context, builder, state, config, result, io, totalRows, col);
            }
        }*/

        if (builder.clickedItem.depth != -1)
            UpdateItemSelection(state, builder, config, io, builder.clickedItem.col, builder.clickedItem.row, builder.clickedItem.depth);

        ctx.ToggleDeferedRendering(false, true);
        ctx.deferedEvents.clear(true);
        return result;
    }

    WidgetDrawResult EndItemGrid()
    {
        WidgetDrawResult result;
        auto& context = *WidgetContextData::CurrentItemGridContext;
        auto& builder = context.itemGrids.top();
        auto& state = context.GridState(builder.id);
        const auto& config = context.GetState(builder.id).state.grid;
        auto& renderer = context.GetRenderer();
        auto io = Config.platform->CurrentIO();

        ImRect viewport{ builder.origin + ImVec2{ 0.f, builder.headerHeight + builder.filterRowHeight }, 
            builder.origin + builder.size };
        renderer.SetClipRect(viewport.Min, viewport.Max);
        result = PopulateData(builder.rowcount);
        renderer.ResetClipRect();
        END_WIDGET_LOG();

        builder.phase = ItemGridConstructPhase::None;
        auto scrollable = viewport;
		scrollable.Max.y -= builder.epilogueHeight;
        AddItemGridScrollBars(builder, state, config, renderer, scrollable, io);
        if (!ImRect{ builder.origin, builder.origin + builder.size }.Contains(io.mousepos))
            state.cellstate.state &= ~WS_Hovered;

        renderer.DrawLine(viewport.Min, { viewport.Min.x, viewport.Max.y }, config.gridcolor, config.gridwidth);
        renderer.DrawLine({ viewport.Max.x, viewport.Min.y }, viewport.Max, config.gridcolor, config.gridwidth);
        context.adhocLayout.top().nextpos = builder.origin;
        context.adhocLayout.top().lastItemId = builder.id;

        // Highlight the column border which is being dragged for resize
        if (builder.resizecol != -1)
        {
            auto hshift = builder.resizecol < config.frozencols ? -state.altscroll.state.pos.x : -state.scroll.state.pos.x;
            auto xpos = builder.headers[builder.currlevel][builder.resizecol - 1].extent.Max.x + config.gridwidth;
            Config.renderer->DrawLine(ImVec2{ xpos, builder.origin.y }, ImVec2{ xpos, builder.origin.y + builder.size.y }, 
                ToRGBA(100, 100, 255), config.gridwidth);
        }

        auto sz = builder.origin + builder.size;
        //if (config.scrollprops & ST_Always_H) sz.y += Config.scrollbar.width;
        //if (config.scrollprops & ST_Always_V) sz.x += Config.scrollbar.width;
        context.AddItemGeometry(builder.id, { builder.origin, sz });

        builder.reset();
        context.itemGrids.pop(1, false);
        if (context.itemGrids.empty()) WidgetContextData::CurrentItemGridContext = nullptr;

        auto& ctx = GetContext();
        PopNestedSource();
        for (const auto& nested : ctx.nestedContextStack)
            if (nested.source == NestedContextSourceType::ItemGrid)
            {
                context.CurrentItemGridContext = nested.base;
                break;
            }

        PopContext();
        return result;
    }

    WidgetDrawResult ItemGridImpl(int32_t id, const StyleDescriptor& style, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io)
    {
        static Vector<std::pair<int16_t, int16_t>, int16_t> colmap[GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL];

        auto& context = GetContext();
        const auto& config = context.GetState(id).state.grid;
        auto& gridstate = context.GridState(id);
        auto& builder = context.itemGrids.push();

        builder.id = id;
        builder.origin = content.Min;
        builder.nextpos = builder.origin;
        builder.size = content.GetSize();

        context.CurrentItemGridContext = &context;
        auto& ctx = PushContext(id);
        auto& el = ctx.nestedContextStack.push();
        el.base = &context;
        el.source = NestedContextSourceType::ItemGrid;
        
        int16_t currparent = -1;
        auto hlevels = (int32_t)config.config.headers.size();
        BeginItemGridHeader(hlevels);

        for (auto level = hlevels - 1; level >= 0; --level)
        {
            int16_t col = 0;
            colmap[level - 1].resize(config.config.headers[level].size());

            for (const auto& hconfig : config.config.headers[level])
            {
                if (level == hlevels - 1)
                {
                    AddHeaderColumn(hconfig);
                    if (currparent != hconfig.parent)
                        colmap[level - 1][hconfig.parent] = std::make_pair(col, col);
                    else
                        colmap[level - 1][hconfig.parent].second = col;
                    currparent = hconfig.parent;
                }
                else
                    AddColumnCategory(hconfig, colmap[level][col].first, colmap[level][col].second);
                ++col;
            }

            if (level < hlevels - 1) CategorizeColumns();
        }

        for (auto level = hlevels - 1; level >= 0; --level)
            colmap[level].clear(false);

        EndItemGridHeader();
        PopulateItemGrid(config.config.rows, config.populateMethod);
        return EndItemGrid();
    }

    WidgetDrawResult StaticItemGrid(int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        return Widget(id, WT_ItemGrid, geometry, neighbors);
    }

    WidgetDrawResult StaticItemGrid(std::string_view id, const std::initializer_list<std::string_view>& headers,
        std::pair<std::string_view, TextType>(*cell)(int32_t, int16_t), int32_t totalRows, int32_t geometry, const NeighborWidgets& neighbors)
    {
        static auto fptr = cell;

        auto wid = GetIdFromString(id, WT_ItemGrid).first;
        auto& grid = CreateWidgetConfig(wid).state.grid;
        grid.cellcontent = [](const ItemGridConfig::CellGeometry& cell) { return fptr(cell.row, cell.col); };

        BeginItemGrid(wid, geometry, neighbors);
        BeginItemGridHeader(1);

        for (const auto hname : headers)
        {
            ItemGridConfig::ColumnConfig col;
            col.name = hname;
            col.props = COL_Resizable;
            AddHeaderColumn(col);
        }

        EndItemGridHeader();
        PopulateItemGrid(totalRows);
        return EndItemGrid();
    }

#pragma endregion

#pragma region Splitter

    void BeginSplitRegion(int32_t id, Direction dir, const std::initializer_list<SplitRegion>& splits,
        int32_t geometry, const NeighborWidgets& neighbors)
    {
        assert((int)splits.size() < GLIMMER_MAX_SPLITTER_REGIONS);

        LayoutItemDescriptor layoutItem;
        AddExtent(layoutItem, neighbors);
        auto& context = GetContext();
        auto& el = context.splitterStack.push();
        el.dir = dir;
        el.extent = layoutItem.margin;
        el.id = id;

        auto& state = context.SplitterState(id);
        const auto style = context.GetStyle(WS_Default, id);
        state.current = 0;

        auto& renderer = context.GetRenderer();
        renderer.SetClipRect(layoutItem.margin.Min, layoutItem.margin.Max);

        auto idx = 0;
        auto width = el.extent.GetWidth(), height = el.extent.GetHeight();
        auto splittersz = el.dir == DIR_Vertical ? (style.dimension.y > 0.f ? style.dimension.y : Config.splitterSize) :
            style.dimension.x > 0.f ? style.dimension.x : Config.splitterSize;
        if (el.dir == DIR_Horizontal) width -= splittersz * (float)splits.size();
        if (el.dir == DIR_Vertical) height -= splittersz * (float)splits.size();
        auto prev = el.extent.Min;

        for (const auto& split : splits)
        {
            if (state.spacing[idx].curr == -1.f)
            {
                state.spacing[idx].curr = split.initial;      
            }

            auto regionEnd = prev + (el.dir == DIR_Vertical ? ImVec2{ width, state.spacing[idx].curr * height } :
                ImVec2{ state.spacing[idx].curr * width, height });

            auto scid = GetNextId(WT_SplitterRegion);
            state.viewport[idx] = ImRect{ prev, regionEnd };
            state.containers[idx] = scid;
            context.AddItemGeometry(state.containers[idx], state.viewport[idx], true);

            state.spacing[idx].min = split.min;
            state.spacing[idx].max = split.max;
            prev = regionEnd;
            el.dir == DIR_Vertical ? prev.x = layoutItem.margin.Min.x : prev.y = layoutItem.margin.Min.y;
            el.dir == DIR_Vertical ? prev.y += splittersz : prev.x += splittersz;
            ++idx;
        }

        auto& layout = context.adhocLayout.top();
        layout.nextpos = state.viewport[state.current].Min;
        context.containerStack.push() = state.containers[state.current];
        renderer.SetClipRect(state.viewport[state.current].Min, state.viewport[state.current].Max);

        BEGIN_WIDGET_LOG(id, state.viewport[state.current]);
        LOG_NUM2("direction", dir);
        BEGIN_LOG_ARRAY("split-ratios");
        for (auto split : splits)
        {
            BEGIN_LOG_OBJECT("split");
            LOG_NUM(split.initial); LOG_NUM(split.min); LOG_NUM(split.max);
            END_LOG_OBJECT();
        }
        END_LOG_ARRAY();
        BEGIN_LOG_ARRAY("split-panes");
    }

    void BeginSplitRegion(std::string_view id, Direction dir, const std::initializer_list<SplitRegion>& splits, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto [iid, __] = GetIdFromString(id, WT_Splitter);
        BeginSplitRegion(iid, dir, splits, geometry, neighbors);
    }

    void NextSplitRegion()
    {
        auto& context = GetContext();
        auto& el = context.splitterStack.top();
        auto& state = context.SplitterState(el.id);
        auto io = Config.platform->CurrentIO();
        auto mousepos = io.mousepos;
        const auto style = context.GetStyle(WS_Default, el.id);
        const auto width = el.extent.GetWidth(), height = el.extent.GetHeight();
        auto& renderer = context.GetRenderer();

        assert(state.current < GLIMMER_MAX_SPLITTER_REGIONS);

        auto splittersz = el.dir == DIR_Vertical ? (style.dimension.y > 0.f ? style.dimension.y : Config.splitterSize) :
            style.dimension.x > 0.f ? style.dimension.x : Config.splitterSize;
        auto scid = state.containers[state.current];
        renderer.ResetClipRect();
        context.containerStack.pop(1, true);

        auto nextpos = el.dir == DIR_Vertical ? ImVec2{ el.extent.Min.x, state.viewport[state.current].Max.y } :
            ImVec2{ state.viewport[state.current].Max.x, el.extent.Min.y };
        auto sz = (el.dir == DIR_Vertical ? ImVec2{ width, splittersz } : ImVec2{ splittersz, height });

        renderer.SetClipRect(nextpos, nextpos + sz);
        DrawBorderRect(nextpos, nextpos + sz, style.border, style.bgcolor, renderer);
        DrawBackground(nextpos, nextpos + sz, style, renderer);
        auto radius = std::max(splittersz - 2.f, 1.f);
        if (el.dir == DIR_Vertical)
        {
            auto xstart = std::max((sz.x - (8.f * radius)) * 0.5f, 0.f);
            auto startpos = ImVec2{ nextpos.x + xstart, nextpos.y + 1.f };
            startpos.x += radius;
            renderer.DrawCircle(startpos, radius, ToRGBA(100, 100, 100), true);
            startpos.x += 3.f * radius;
            renderer.DrawCircle(startpos, radius, ToRGBA(100, 100, 100), true);
            startpos.x += 3.f * radius;
            renderer.DrawCircle(startpos, radius, ToRGBA(100, 100, 100), true);
        }
        else
        {
            auto ystart = std::max(nextpos.y + ((sz.y - (8.f * radius)) * 0.5f), 0.f);
            auto startpos = ImVec2{ nextpos.x + 1.f, ystart };
            startpos.y += radius;
            renderer.DrawCircle(startpos, radius, ToRGBA(100, 100, 100), true);
            startpos.y += 3.f * radius;
            renderer.DrawCircle(startpos, radius, ToRGBA(100, 100, 100), true);
            startpos.y += 3.f * radius;
            renderer.DrawCircle(startpos, radius, ToRGBA(100, 100, 100), true);
        }
        renderer.ResetClipRect();

        if (ImRect{ nextpos, nextpos + sz }.Contains(mousepos) || state.isdragged[state.current])
        {
            Config.platform->SetMouseCursor(el.dir == DIR_Vertical ? MouseCursor::ResizeVertical :
                MouseCursor::ResizeHorizontal);
            auto isDrag = io.isLeftMouseDown();
            state.states[state.current] = isDrag ? WS_Pressed | WS_Hovered : WS_Hovered;

            if (isDrag)
            {
                if (!state.isdragged[state.current])
                {
                    state.isdragged[state.current] = true;
                    state.dragstart[state.current] = el.dir == DIR_Vertical ? mousepos.y : mousepos.x;
                }
                else
                {
                    auto amount = el.dir == DIR_Vertical ? (mousepos.y - state.dragstart[state.current]) / height :
                        (mousepos.x - state.dragstart[state.current]) / width;
                    auto prev = state.spacing[state.current].curr;
                    state.spacing[state.current].curr = clamp(prev + amount, state.spacing[state.current].min, state.spacing[state.current].max);
                    auto diff = prev - state.spacing[state.current].curr;

                    if (diff != 0.f)
                    {
                        state.dragstart[state.current] = el.dir == DIR_Vertical ? mousepos.y : mousepos.x;
                        state.spacing[state.current + 1].curr += diff;
                    } 
                }
            }
            else state.isdragged[state.current] = false;
        }
        else if (!io.isLeftMouseDown())
        {
            state.states[state.current] = WS_Default;
            state.isdragged[state.current] = false;
        }

        if (state.isdragged[state.current]) InvalidateLayout();

        state.current++;
        assert(state.current < GLIMMER_MAX_SPLITTER_REGIONS);

        auto& layout = context.adhocLayout.top();
        layout.nextpos = state.viewport[state.current].Min;
        context.containerStack.push() = state.containers[state.current];
        renderer.SetClipRect(state.viewport[state.current].Min, state.viewport[state.current].Max);

        BEGIN_LOG_OBJECT("split-pane");
        LOG_RECT2("current-viewport", state.viewport[state.current]); LOG_NUM(state.current);
        END_LOG_OBJECT();
    }

    void EndSplitRegion()
    {
        auto& context = GetContext();
        auto el = context.splitterStack.top();
        auto& state = context.SplitterState(el.id);
        auto& renderer = context.GetRenderer();

        renderer.ResetClipRect();
        context.containerStack.pop(1, true);
        context.splitterStack.pop(1, true);
        context.AddItemGeometry(el.id, el.extent);
        renderer.ResetClipRect();

        END_LOG_ARRAY();
        END_WIDGET_LOG();
    }

#pragma endregion

#pragma region Popups

    bool BeginPopup(int32_t id, ImVec2 size)
    {
		return BeginPopup(id, { -1.f, -1.f }, size, NestedContextSourceType::GenericPopup);
    }

    bool BeginPopup(ImVec2 origin, ImVec2 size)
    {
        return BeginPopup(-1, origin, size, NestedContextSourceType::GenericPopup);
    }

    bool BeginPopup(int32_t id, ImVec2 origin, ImVec2 size, NestedContextSourceType type)
    {
        auto io = Config.platform->CurrentIO();

        if (!io.isKeyPressed(Key_Escape))
        {
            auto& overlayctx = type != NestedContextSourceType::None ? PushContext(id, type) : PushContext(id);
            overlayctx.ToggleDeferedRendering(true);
            overlayctx.deferEvents = true;
            overlayctx.popupOrigin = origin;
            overlayctx.popupSize = size;
            overlayctx.popupSource = type;
            overlayctx.popupTargetId = id;
            overlayctx.RecordDeferRange(overlayctx.popupRange, true);

            WidgetContextData::PopupContext = &overlayctx;
            WidgetContextData::PopupTarget = id;
            return true;
        }
        else
            WidgetContextData::RemovePopup();

        return false;
    }

    bool BeginPopup(int32_t id, ImVec2 origin, ImVec2 size)
    {
		return BeginPopup(id, origin, size, NestedContextSourceType::GenericPopup);
    }

    void SetPopupCallback(PopupCallback phase, PopUpCallbackT callback, void* data)
    {
        auto& context = GetContext();
        context.popupCallbacks[phase] = callback;
        context.popupCallbackData[phase] = data;
    }

    WidgetDrawResult EndPopUp(WidgetContextData& overlayctx, std::optional<uint32_t> bgcoloropt, int32_t flags)
    {
        WidgetDrawResult result;

        if (overlayctx.popupCallbacks[PCB_GeneratePrimitives])
            overlayctx.popupCallbacks[PCB_GeneratePrimitives](overlayctx.popupCallbackData[PCB_GeneratePrimitives],
                *overlayctx.deferedRenderer, overlayctx.popupOrigin, {});

        overlayctx.RecordDeferRange(overlayctx.popupRange, false);

        auto posflags = overlayctx.parentContext->popupFlags;
        if (posflags >= POP_AnchorLeft && overlayctx.popupTargetId != -1)
        {
            auto geometry = overlayctx.parentContext->GetGeometry(overlayctx.popupTargetId);

            if (posflags & POP_AnchorLeft)
                overlayctx.popupOrigin.x = geometry.Min.x;
            else if (posflags & POP_AnchorRight)
                overlayctx.popupOrigin.x = geometry.Max.x;
            if (posflags & POP_AnchorBottom)
                overlayctx.popupOrigin.y = geometry.Max.y;
            else if (posflags & POP_AnchorTop)
                overlayctx.popupOrigin.y = geometry.Min.y;

            if (posflags & POP_EndH)
                overlayctx.popupOrigin.x -= overlayctx.deferedRenderer->size.x;
            if (posflags & POP_EndV)
                overlayctx.popupOrigin.y -= overlayctx.deferedRenderer->size.y;

            if (posflags & POP_AnchorHCenter)
            {
				auto hdiff = geometry.GetWidth() - overlayctx.deferedRenderer->size.x;
				overlayctx.popupOrigin.x = geometry.Min.x + (hdiff * 0.5f);
            }

            if (posflags & POP_AnchorVCenter)
            {
				auto vdiff = geometry.GetHeight() - overlayctx.deferedRenderer->size.y;
				overlayctx.popupOrigin.y = geometry.Min.y + (vdiff * 0.5f);
            }
        }

        if (overlayctx.deferedRenderer->size.y > 0.f)
        {
            auto& renderer = *Config.renderer; // overlay cannot be deferred TODO: Figure out if it can be
            auto available = overlayctx.parentContext->WindowSize();
            ImVec2 origin = overlayctx.popupOrigin, size;
            size.x = overlayctx.popupSize.x != FLT_MAX ? overlayctx.popupSize.x : overlayctx.deferedRenderer->size.x;
            size.y = overlayctx.popupSize.y != FLT_MAX ? overlayctx.popupSize.y : overlayctx.deferedRenderer->size.y;

            if (flags & POP_EnsureVisible)
            {
                if (((origin.y + size.y) > available.y) && ((overlayctx.PopupTarget >> WidgetTypeBits) != WT_ContextMenu))
                    origin.y = origin.y - size.y - overlayctx.parentContext->GetGeometry(overlayctx.PopupTarget).GetHeight();

                if ((origin.x + size.x) > available.x)
                    origin.x = origin.x - size.x;
            }

            auto style = overlayctx.GetStyle(WS_Default);
            auto bgcolor = bgcoloropt.has_value() ? bgcoloropt.value() : style.bgcolor;

            if (flags & POP_Occlude)
            {
                renderer.BeginDefer();
                renderer.DrawRect(ImVec2{}, available, Config.popupOcclusionColor, true);
                renderer.EndDefer();
            }

            if (renderer.StartOverlay(overlayctx.PopupTarget, origin, size, bgcolor))
            {
                WidgetContextData::ActivePopUpRegion = ImRect{ origin, origin + size };
                DrawBorderRect(origin, origin + size, style.border, bgcolor, renderer);

                if (overlayctx.popupCallbacks[PCB_BeforeRender])
                    overlayctx.popupCallbacks[PCB_BeforeRender](overlayctx.popupCallbackData[PCB_BeforeRender], renderer, origin,
                        WidgetContextData::ActivePopUpRegion);

                overlayctx.deferedRenderer->Render(renderer, origin, overlayctx.popupRange.primitives.first,
                    overlayctx.popupRange.primitives.second);

                if (overlayctx.popupCallbacks[PCB_AfterRender])
                    overlayctx.popupCallbacks[PCB_AfterRender](overlayctx.popupCallbackData[PCB_AfterRender], renderer, origin,
                        WidgetContextData::ActivePopUpRegion);

                overlayctx.deferEvents = false;
                result = overlayctx.HandleEvents(origin, overlayctx.popupRange.events.first,
                    overlayctx.popupRange.events.second);

                if (overlayctx.popupCallbacks[PCB_HandleEvents])
                    overlayctx.popupCallbacks[PCB_HandleEvents](overlayctx.popupCallbackData[PCB_HandleEvents], renderer, origin,
                        WidgetContextData::ActivePopUpRegion);

                renderer.EndOverlay();
            }

            if (result.event == WidgetEvent::None)
                result.geometry = ImRect{ origin, origin + size };
        }

        overlayctx.ToggleDeferedRendering(false);
        overlayctx.deferedEvents.clear(true);
        return result;
    }

    WidgetDrawResult EndPopUp(std::optional<uint32_t> bgcoloropt, int32_t flags)
    {
        auto& overlayctx = GetContext();

        // If the popup wasn't triggered inside a layout, draw it immediately.
        // Otherwise, delay rendering till layout is resolved...
        if (overlayctx.parentContext->layoutStack.empty())
        {
            auto res = EndPopUp(overlayctx, bgcoloropt, flags);
            if (overlayctx.popupSource != NestedContextSourceType::None)
                PopNestedSource();
            PopContext();
            return res;
        }
        else
        {
			overlayctx.parentContext->popupContext = &overlayctx;
			overlayctx.parentContext->popupBgColor = bgcoloropt;
			overlayctx.parentContext->popupFlags = flags;
			return WidgetDrawResult{};
        }
    }

#pragma endregion

#ifndef GLIMMER_DISABLE_PLOTS
#pragma region Charts

    bool BeginPlot(std::string_view str, ImVec2 size, int32_t flags)
    {
        auto& context = GetContext();
        auto extent = context.MaximumExtent();
        auto pos = context.NextAdHocPos();
        size.x = size.x == FLT_MAX ? extent.x - pos.x : size.x * Config.scaling;
        size.y = (size.y == FLT_MAX) ? extent.y - pos.y : size.y * Config.scaling;

        auto id = GetNextId(WT_Charts);
        auto style = context.GetStyle(context.GetState(id).data.state, id);
        ImRect bounds{ pos, pos + size };
        bounds.Min = bounds.Min + ImVec2{ style.margin.left, style.margin.top };
        bounds.Max = bounds.Max - ImVec2{ style.margin.right, style.margin.bottom };
        auto& renderer = context.GetRenderer();

        context.AddItemGeometry(id, ImRect{ pos, pos + size });
        DrawBorderRect(bounds.Min, bounds.Max, style.border, style.bgcolor, renderer);
        bounds.Min = bounds.Min + ImVec2{ style.border.left.thickness + style.padding.left, 
            style.border.top.thickness + style.padding.top };
        bounds.Max = bounds.Max - ImVec2{ style.border.right.thickness + style.padding.right, 
            style.border.bottom.thickness + style.padding.bottom };
        ImGui::SetCursorPos(bounds.Min);
        return ImPlot::BeginPlot(str.data(), bounds.GetSize(), flags);
    }

    WidgetDrawResult EndPlot()
    {
        ImPlot::EndPlot();

        auto& context = GetContext();
        WidgetDrawResult res;
        auto id = (WT_Charts << 16) | (context.maxids[WT_Charts] - 1);
        auto io = Config.platform->CurrentIO();
        res.geometry = context.GetGeometry(id);

        if (res.geometry.Contains(io.mousepos))
        {
            auto& state = context.GetState(id);

            if (io.mouseWheel != 0.f)
                res.event = WidgetEvent::Scrolled;
            else if (io.isLeftMouseDown())
            {
                state.data.state |= WS_Pressed;
                res.event = WidgetEvent::Pressed;
            }
            else if (state.data.state & WS_Pressed)
            {

            }

            res.wheel = io.mouseWheel;
            HandleContextMenu(id, res.geometry, io);
        }

        return res;
    }

#pragma endregion
#endif

#pragma region Media Resource (Image/SVG/etc.)

    static ImVec2 GetIconSize(IconSizingType sztype)
    {
        ImVec2 size;
        auto& context = GetContext();
        auto style = context.GetStyle(WS_Default);

        switch (sztype)
        {
        case IconSizingType::Fixed:
            size = style.dimension;
            break;
        case IconSizingType::CurrentFontSz:
            size = ImVec2{ style.font.size, style.font.size };
            break;
        case IconSizingType::DefaultFontSz:
            size = ImVec2{ Config.defaultFontSz, Config.defaultFontSz };
            break;
        }

        return size;
    }

    static void DetermineIconBounds(ImVec2 pos, const MediaState& state, const StyleDescriptor& style, LayoutItemDescriptor& layoutItem)
    {
        ImVec2 size = GetIconSize(state.sztype);
        layoutItem.content = ImRect{ pos, pos + size };
        layoutItem.padding = ImRect{ layoutItem.content.Min - ImVec2{ style.padding.left, style.padding.top },
            layoutItem.content.Max + ImVec2{ style.padding.right, style.padding.bottom } };
        layoutItem.border = ImRect{ layoutItem.padding.Min - ImVec2{ style.border.left.thickness, style.border.top.thickness },
            layoutItem.padding.Max + ImVec2{ style.border.right.thickness, style.border.bottom.thickness } };
        layoutItem.margin = ImRect{ layoutItem.border.Min - ImVec2{ style.margin.left, style.margin.top },
            layoutItem.border.Max + ImVec2{ style.margin.right, style.margin.bottom } };
    }

    void HandleMediaResourceEvent(WidgetContextData& context, int32_t id, const ImRect& padding, const ImRect& content, const IODescriptor& io, WidgetDrawResult& result)
    {
        auto& state = context.GetState(id).state.media;
        const auto style = context.GetStyle(state.state, id);

        if (!context.deferEvents)
        {
            auto ismouseover = padding.Contains(io.mousepos);
            state.state = !ismouseover ? WS_Default :
                io.isLeftMouseDown() ? WS_Pressed | WS_Hovered : WS_Hovered;
            if (ismouseover && io.clicked())
                result.event = WidgetEvent::Clicked;
            if (ismouseover)
            {
                Config.platform->SetMouseCursor(MouseCursor::Grab);
                WidgetContextData::CurrentWidgetId = id;
            }
            ShowTooltip(state._hoverDuration, padding, state.tooltip, io);
            HandleContextMenu(id, content, io);

            WITH_WIDGET_LOG(id, content);
            LOG_NUM(state.resflags);
        }
        else context.deferedEvents.emplace_back(EventDeferInfo::ForMediaResource(id, padding, content));
    }

    WidgetDrawResult MediaResourceImpl(int32_t id, const StyleDescriptor& style, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, IRenderer& renderer, const IODescriptor& io)
    {
        WidgetDrawResult result;
        auto& context = GetContext();
        const auto& state = context.GetState(id).state.media;

        DrawBoxShadow(border.Min, border.Max, style, renderer);
        DrawBackground(border.Min, border.Max, style, renderer);
        DrawBorderRect(border.Min, border.Max, style.border, style.bgcolor, renderer);
        if (state.symbol != SymbolIcon::None)
            DrawSymbol(content.Min, content.GetSize(), { 0.f, 0.f }, state.symbol, style.fgcolor, style.fgcolor, 1.f, context.GetRenderer());
        else
            context.GetRenderer().DrawResource(state.resflags, content.Min, content.GetSize(), style.fgcolor, state.content);

        HandleMediaResourceEvent(context, id, padding, content, io, result);
        result.geometry = margin;
        return result;
    }

#if !defined(GLIMMER_DISABLE_SVG) || !defined(GLIMMER_DISABLE_ICONS)

    WidgetDrawResult Icon(int32_t rtype, IconSizingType sztype, std::string_view resource, int32_t geometry, const NeighborWidgets& neighbors)
    {
        assert((rtype & RT_PATH) != 0);
        auto id = GetIdFromString(resource, WT_MediaResource);
        auto& context = GetContext();
        auto& media = context.GetState(id.first).state.media;

        media.resflags = rtype;
        media.content = resource;
        media.sztype = sztype;
        return Widget(id.first, WT_MediaResource, geometry, neighbors);
    }

    WidgetDrawResult Icon(int32_t id, int32_t rtype, IconSizingType sztype, std::string_view resource, int32_t geometry, const NeighborWidgets& neighbors)
    {
        assert((rtype & RT_PATH) != 0);
        auto& context = GetContext();
        auto& media = context.GetState(id).state.media;

        media.resflags = rtype;
        media.content = resource;
        media.sztype = sztype;
        return Widget(id, WT_MediaResource, geometry, neighbors);
    }

    WidgetDrawResult Icon(std::string_view id, int32_t rtype, IconSizingType sztype, std::string_view resource, int32_t geometry, const NeighborWidgets& neighbors)
    {
        assert((rtype & RT_PATH) != 0);
        auto wid = GetIdFromString(id, WT_MediaResource).first;
        auto& context = GetContext();
        auto& media = context.GetState(wid).state.media;

        media.resflags = rtype;
        media.content = resource;
        media.sztype = sztype;
        return Widget(wid, WT_MediaResource, geometry, neighbors);
    }

#endif

#ifdef GLIMMER_ENABLE_ICON_FONT

    WidgetDrawResult Icon(std::string_view resource, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto wid = GetIdFromString(resource, WT_MediaResource).first;
        auto& context = GetContext();
        auto& media = context.GetState(wid).state.media;

        media.resflags = RT_ICON_FONT;
        media.content = resource;
        media.sztype = IconSizingType::CurrentFontSz;
        return Widget(wid, WT_MediaResource, geometry, neighbors);
    }

#endif

    WidgetDrawResult Icon(int32_t id, SymbolIcon icon, IconSizingType sztype, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto& context = GetContext();
        auto& media = context.GetState(id).state.media;

        media.resflags = RT_SYMBOL;
        media.symbol = icon;
        media.sztype = sztype;
        return Widget(id, WT_MediaResource, geometry, neighbors);
    }

    WidgetDrawResult Icon(std::string_view id, SymbolIcon icon, IconSizingType sztype, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto wid = GetIdFromString(id, WT_MediaResource).first;
        auto& context = GetContext();
        auto& media = context.GetState(wid).state.media;

        media.resflags = RT_SYMBOL;
        media.symbol = icon;
        media.sztype = sztype;
        return Widget(wid, WT_MediaResource, geometry, neighbors);
    }

#pragma endregion

#pragma region Custom Widget

    void HandleCustomWidgetEvent(WidgetContextData& context, int32_t id, ImVec2 offset, const IODescriptor& io, WidgetDrawResult& result)
    {
        if (!context.deferEvents)
            Config.CustomWidgetProvider((int16_t)(id >> WidgetTypeBits))->HandleEvents(id, offset, io, result);
        else
            context.deferedEvents.emplace_back(EventDeferInfo::ForCustom(id));
    }

    WidgetDrawResult DrawCustomWidget(int32_t id, const StyleDescriptor& style, const LayoutItemDescriptor& layoutItem,
        IRenderer& renderer, const IODescriptor& io)
    {
        WidgetDrawResult result;
        Config.CustomWidgetProvider((int16_t)(id >> WidgetTypeBits))->DrawWidget(style, layoutItem, renderer, io);
        HandleCustomWidgetEvent(GetContext(), id, layoutItem.margin.Min, io, result);
        return result;
    }

    void* ICustomWidget::PushContext(int32_t id)
    {
        return (void*)&glimmer::PushContext(id, NestedContextSourceType::Custom);
    }

    void ICustomWidget::PopContext(void* ctx)
    {
        glimmer::PopNestedSource((WidgetContextData*)ctx);
        glimmer::PopContext();
    }

    StyleDescriptor ICustomWidget::ComputeStyle(int32_t id, int32_t state, const StyleStackT& stack)
    {
        return glimmer::GetStyle(GetContext(), id, &stack, state);
    }

    bool ICustomWidget::IsInState(int32_t id, WidgetState state)
    {
        auto& sdata = GetContext().GetState(id).state;
        auto type = id >> WidgetTypeBits;

        if (type < WT_TotalTypes)
        {
            switch (type)
            {
            case WT_Region: return (sdata.region.state & state) != 0;
            case WT_Button: return (sdata.button.state & state) != 0;
            case WT_Label: return (sdata.label.state & state) != 0;
            case WT_Checkbox: return (sdata.checkbox.state & state) != 0;
            case WT_RadioButton: return (sdata.radio.state & state) != 0;
            case WT_ToggleButton: return (sdata.toggle.state & state) != 0;
            case WT_Slider: return (sdata.slider.state & state) != 0;
            case WT_RangeSlider: return (sdata.rangeSlider.state & state) != 0;
            case WT_TextInput: return (sdata.input.state & state) != 0;
            case WT_MediaResource: return (sdata.media.state & state) != 0;
            default: return false;
            }
        }
        else
            return Config.CustomWidgetProvider ? (Config.CustomWidgetProvider(type)->GetState(id) & state) != 0 : false;
    }

    ImRect ICustomWidget::GetBounds(int32_t id)
    {
        return GetContext().GetGeometry(id);
    }

    ImVec2 ICustomWidget::GetMaximumSize()
    {
        return GetContext().MaximumSize();
    }

    ImVec2 ICustomWidget::GetMaximumExtent()
    {
        return GetContext().MaximumExtent();
    }

    std::pair<int16_t, int16_t> ICustomWidget::GetTypeAndIndex(int32_t id)
    {
        return { (int16_t)(id >> WidgetTypeBits), (int16_t)(id & WidgetIndexMask) };
    }

    std::tuple<ImRect, ImRect, ImRect, ImRect> ICustomWidget::GetBoxModelBounds(ImRect content,
        const StyleDescriptor& style)
    {
        return glimmer::GetBoxModelBounds(content, style);
    }

    std::tuple<ImRect, ImRect, ImRect, ImRect, ImRect> ICustomWidget::GetBoxModelBounds(ImVec2 pos,
        const StyleDescriptor& style, std::string_view text, IRenderer& renderer, int32_t geometry,
        TextType type, const NeighborWidgets& neighbors, float width, float height)
    {
        return glimmer::GetBoxModelBounds(pos, style, text, renderer, geometry, type, neighbors, width, height);
    }

#pragma endregion

#pragma region Implementation Details

#ifndef GLIMMER_DISABLE_RICHTEXT

    struct RichTextPlatformSupport : public ImRichText::IPlatform
    {
        ImVec2 GetCurrentMousePos() { return Config.platform->CurrentIO().mousepos; }
        bool IsMouseClicked() { return Config.platform->CurrentIO().clicked(); }

        // TODO: Implement these methods to provide full hyperlink and frame support
        void HandleHyperlink(std::string_view) {}
        void RequestFrame() {}
        void HandleHover(bool) {}

        float DeltaTime() { return Config.platform->CurrentIO().deltaTime; }
    };

#endif

    UIConfig& GetUIConfig()
    {
        return Config;
    }

    UIConfig& CreateUIConfig(bool needsRichText, IWidgetLogger* logger)
    {
#ifndef GLIMMER_DISABLE_RICHTEXT
        if (needsRichText && Config.richTextConfig == nullptr)
        {
            ImRichText::DefaultConfigParams rtparams;
            rtparams.Charset = TextContentCharset::UTF8Simple;
            rtparams.DefaultFontSize = Config.defaultFontSz;
            rtparams.FontScale = Config.fontScaling;

            Config.richTextConfig = ImRichText::GetDefaultConfig(rtparams);
            Config.richTextConfig->Renderer = Config.renderer;
#ifdef _DEBUG
            Config.richTextConfig->OverlayRenderer = Config.renderer;
#endif
            Config.richTextConfig->RTRenderer = new ImRichText::IRenderer{};
            Config.richTextConfig->RTRenderer->UserData = Config.renderer;
            Config.richTextConfig->Platform = new RichTextPlatformSupport{};
            Config.logger = logger;
        }
#endif

        Config.implicitInheritedProps = ~(StyleBackground | StyleHeight | StyleWidth | StyleMaxWidth |
            StyleMaxHeight | StyleMinWidth | StyleMinHeight);
        return Config;
    }

    IWidgetLogger* CreateJSONLogger(std::string_view path, bool separateFrames)
    {
#ifdef GLIMMER_ENABLE_TESTING
        static WidgetLogger logger{ path, separateFrames };
        return &logger;
#else
        return nullptr;
#endif
    }

    // Widget rendering are of three types:
    // 1. Capture only the bounds to add to a layout
    // 2. Capture only bounds to add to item grid widget
    // 3. Render immediately
    WidgetDrawResult Widget(int32_t id, WidgetType type, int32_t geometry, const NeighborWidgets& neighbors, bool onlyComputeGeometry)
    {
        auto& context = GetContext();
        assert((id & WidgetIndexMask) <= (int)context.states[type].size());

        //If inside layouts, scroll region itself needs a child layout (widgets can't be added in ad-hoc fashion)
        assert(context.layoutStack.empty() || context.layouts[context.layoutStack.top()].type != Layout::ScrollRegion);

        WidgetDrawResult result;
        auto& renderer = context.GetRenderer();
        auto& platform = *Config.platform;
        LayoutItemDescriptor layoutItem;
        layoutItem.wtype = type;
        layoutItem.id = id;
        layoutItem.sizing = geometry;

        auto wid = (type << 16) | id;
        auto maxxy = context.MaximumExtent();
        auto io = Config.platform->CurrentIO();
        auto& nestedCtx = !context.nestedContextStack.empty() ? context.nestedContextStack.top() : 
            InvalidSource;

        UpdateMaxAvailableExtent(context, maxxy, layoutItem.id);

        if (type < WT_TotalTypes)
        {
            switch (type)
            {
            case WT_Region: {
                auto& state = context.GetState(wid).state.region;
                auto style = context.GetStyle(state.state, wid);
                const auto& region = context.regions[context.regionBuilders.top()];
                UpdateTooltip(state.tooltip);

                if (context.layoutStack.empty())
                {
                    std::tie(layoutItem.content, layoutItem.padding, layoutItem.border, layoutItem.margin) =
                        GetBoxModelBounds(ImRect{ region.origin, region.origin + region.size }, style);
                    
                    if (onlyComputeGeometry) break;
                    context.AddItemGeometry(wid, layoutItem.margin);
                    result = RegionImpl(wid, style, layoutItem.margin, layoutItem.border, layoutItem.padding,
                        layoutItem.content, renderer, io, -1);
                    UpdateParentWidgetGeometry(layoutItem, style);
                }
                break;
            }
            case WT_Label: {
                auto& state = context.GetState(wid).state.label;
                auto style = context.GetStyle(state.state, wid);
                UpdateTooltip(state.tooltip);

                if (nestedCtx.source == NestedContextSourceType::Layout && !context.layoutStack.empty())
                {
                    auto& layout = context.layouts[context.layoutStack.top()];
                    auto pos = layout.geometry.Min;

                    if (geometry & ExpandH) layoutItem.sizing |= ExpandH;
                    if (geometry & ExpandV) layoutItem.sizing |= ExpandV;
                    std::tie(layoutItem.content, layoutItem.padding, layoutItem.border, layoutItem.margin, layoutItem.text) = GetBoxModelBounds(pos,
                        style, state.text, renderer, geometry, state.type, neighbors, maxxy.x, maxxy.y);
                    if (onlyComputeGeometry) break;
                    AddItemToLayout(layout, layoutItem, style);
                }
                else
                {
                    auto pos = context.NextAdHocPos();
                    std::tie(layoutItem.content, layoutItem.padding, layoutItem.border, layoutItem.margin, layoutItem.text) = GetBoxModelBounds(
                        pos, style, state.text, renderer, geometry, state.type, neighbors, maxxy.x, maxxy.y);
                    if (onlyComputeGeometry) break;
                    context.AddItemGeometry(wid, layoutItem.margin);
                    auto flags = ToTextFlags(state.type);
                    result = LabelImpl(wid, style, layoutItem.margin, layoutItem.border, layoutItem.padding,
                        layoutItem.content, layoutItem.text, renderer, io, flags);
                    UpdateParentWidgetGeometry(layoutItem, style);
                }
                break;
            }
            case WT_Button: {
                auto& state = context.GetState(wid).state.button;
                auto style = context.GetStyle(state.state, wid);
                UpdateTooltip(state.tooltip);

                if (nestedCtx.source == NestedContextSourceType::Layout && !context.layoutStack.empty())
                {
                    auto& layout = context.layouts[context.layoutStack.top()];
                    auto pos = layout.geometry.Min;
                    ButtonBounds(wid, layout.nextpos, layoutItem, style, renderer, geometry, neighbors, maxxy.x);
                    if (onlyComputeGeometry) break;
                    AddItemToLayout(layout, layoutItem, style);
                }
                else
                {
                    auto pos = context.NextAdHocPos();
                    ButtonBounds(wid, pos, layoutItem, style, renderer, geometry, neighbors, maxxy.x);
                    if (onlyComputeGeometry) break;
                    context.AddItemGeometry(wid, layoutItem.margin);
                    result = ButtonImpl(wid, style, layoutItem.margin, layoutItem.border, layoutItem.padding, layoutItem.content,
                        layoutItem.text, layoutItem.prefix, renderer, io);
                    UpdateParentWidgetGeometry(layoutItem, style);
                }

                break;
            }
            case WT_RadioButton: {
                auto& state = context.GetState(wid).state.radio;
                auto style = context.GetStyle(state.state, wid);
                UpdateTooltip(state.tooltip);

                AddExtent(layoutItem, style, neighbors, { style.font.size, style.font.size }, maxxy);
                if (onlyComputeGeometry) break;
                auto bounds = RadioButtonBounds(state, layoutItem.margin);

                if (nestedCtx.source == NestedContextSourceType::Layout && !context.layoutStack.empty())
                {
                    auto& layout = context.layouts[context.layoutStack.top()];
                    AddItemToLayout(layout, layoutItem, style);
                }
                else
                {
                    renderer.SetClipRect(layoutItem.margin.Min, layoutItem.margin.Max);
                    result = RadioButtonImpl(wid, state, style, bounds, renderer, io);
                    context.AddItemGeometry(wid, bounds);
                    renderer.ResetClipRect();
                    UpdateParentWidgetGeometry(layoutItem, style);
                }

                break;
            }
            case WT_ToggleButton: {
                auto& state = context.GetState(wid).state.toggle;
                auto style = context.GetStyle(state.state, wid);
                UpdateTooltip(state.tooltip);

                AddExtent(layoutItem, style, neighbors, { style.font.size, style.font.size }, maxxy);
                if (onlyComputeGeometry) break;
                auto [bounds, textsz] = ToggleButtonBounds(state, layoutItem.content, renderer);

                if (bounds.GetArea() != layoutItem.margin.GetArea())
                    layoutItem.margin = layoutItem.border = layoutItem.padding = layoutItem.content = bounds;

                layoutItem.text.Min = layoutItem.margin.Min;
                layoutItem.text.Max = layoutItem.text.Min + textsz;

                if (nestedCtx.source == NestedContextSourceType::Layout && !context.layoutStack.empty())
                {
                    auto& layout = context.layouts[context.layoutStack.top()];
                    AddItemToLayout(layout, layoutItem, style);
                }
                else
                {
                    renderer.SetClipRect(bounds.Min, bounds.Max);
                    result = ToggleButtonImpl(wid, state, style, bounds, textsz, renderer, io);
                    context.AddItemGeometry(wid, bounds);
                    renderer.ResetClipRect();
                    UpdateParentWidgetGeometry(layoutItem, style);
                }

                break;
            }
            case WT_Checkbox: {
                auto& state = context.GetState(wid).state.checkbox;
                auto style = context.GetStyle(state.state, wid);
                UpdateTooltip(state.tooltip);

                AddExtent(layoutItem, style, neighbors, { style.font.size, style.font.size }, maxxy);
                if (onlyComputeGeometry) break;
                auto bounds = CheckboxBounds(state, layoutItem.margin);

                if (bounds.GetArea() != layoutItem.margin.GetArea())
                {
                    layoutItem.margin = bounds;
                    layoutItem.border.Min = layoutItem.margin.Min + ImVec2{ style.margin.left, style.margin.top };
                    layoutItem.border.Max = layoutItem.margin.Max - ImVec2{ style.margin.right, style.margin.bottom };

                    layoutItem.padding.Min = layoutItem.border.Min + ImVec2{ style.border.left.thickness, style.border.top.thickness };
                    layoutItem.padding.Min = layoutItem.border.Max + ImVec2{ style.border.right.thickness, style.border.bottom.thickness };
                }

                if (nestedCtx.source == NestedContextSourceType::Layout && !context.layoutStack.empty())
                {
                    auto& layout = context.layouts[context.layoutStack.top()];
                    AddItemToLayout(layout, layoutItem, style);
                }
                else
                {
                    renderer.SetClipRect(layoutItem.margin.Min, layoutItem.margin.Max);
                    result = CheckboxImpl(wid, state, style, layoutItem.margin, layoutItem.padding, renderer, io);
                    context.AddItemGeometry(wid, bounds);
                    renderer.ResetClipRect();
                    UpdateParentWidgetGeometry(layoutItem, style);
                }

                break;
            }
            case WT_Spinner: {
                auto& state = context.GetState(wid).state.spinner;
                auto style = context.GetStyle(state.state, wid);
                UpdateTooltip(state.tooltip);

                AddExtent(layoutItem, style, neighbors, {
                    0.f, style.font.size + style.margin.v() + style.border.v() + style.padding.v() }, maxxy);
                auto bounds = SpinnerBounds(wid, state, renderer, layoutItem.padding);

                layoutItem.border.Max = bounds.Max + ImVec2{ style.border.right.thickness, style.border.bottom.thickness };
                layoutItem.margin.Max = layoutItem.border.Max + ImVec2{ style.margin.right, style.margin.bottom };
                layoutItem.content = layoutItem.padding = bounds;
                if (onlyComputeGeometry) break;

                if (nestedCtx.source == NestedContextSourceType::Layout && !context.layoutStack.empty())
                {
                    auto& layout = context.layouts[context.layoutStack.top()];
                    AddItemToLayout(layout, layoutItem, style);
                }
                else
                {
                    renderer.SetClipRect(layoutItem.margin.Min, layoutItem.margin.Max);
                    result = SpinnerImpl(wid, state, style, layoutItem.padding, io, renderer);
                    context.AddItemGeometry(wid, bounds);
                    renderer.ResetClipRect();
                    UpdateParentWidgetGeometry(layoutItem, style);
                }

                break;
            }
            case WT_Slider: {
                auto& state = context.GetState(wid).state.slider;
                auto style = context.GetStyle(state.state, wid);
                UpdateTooltip(state.tooltip);

                auto deltav = style.margin.v() + style.border.v() + style.padding.v();
                auto deltah = style.margin.h() + style.border.h() + style.padding.h();
                AddExtent(layoutItem, style, neighbors, { state.dir == DIR_Horizontal ? 0.f : style.font.size + deltah,
                    state.dir == DIR_Vertical ? 0.f : style.font.size + deltav }, maxxy);
                auto bounds = SliderBounds(wid, layoutItem.margin);

                if (bounds.GetArea() != layoutItem.margin.GetArea())
                {
                    layoutItem.margin = bounds;
                    layoutItem.border.Min = layoutItem.margin.Min + ImVec2{ style.margin.left, style.margin.top };
                    layoutItem.border.Max = layoutItem.margin.Max - ImVec2{ style.margin.right, style.margin.bottom };

                    layoutItem.padding.Min = layoutItem.border.Min + ImVec2{ style.border.left.thickness, style.border.top.thickness };
                    layoutItem.padding.Min = layoutItem.border.Max + ImVec2{ style.border.right.thickness, style.border.bottom.thickness };
                }

                layoutItem.content = layoutItem.padding;
                if (onlyComputeGeometry) break;

                if (nestedCtx.source == NestedContextSourceType::Layout && !context.layoutStack.empty())
                {
                    auto& layout = context.layouts[context.layoutStack.top()];
                    auto pos = layout.geometry.Min;
                    if ((geometry & ExpandH) && (state.dir == DIR_Horizontal)) layoutItem.sizing |= ExpandH;
                    if ((geometry & ExpandV) && (state.dir == DIR_Vertical)) layoutItem.sizing |= ExpandV;
                    layoutItem.sizing |= state.dir == DIR_Horizontal ? ShrinkH : ShrinkV;
                    AddItemToLayout(layout, layoutItem, style);
                }
                else
                {
                    renderer.SetClipRect(layoutItem.margin.Min, layoutItem.margin.Max);
                    result = SliderImpl(wid, state, style, layoutItem.border, renderer, io);
                    context.AddItemGeometry(wid, bounds);
                    renderer.ResetClipRect();
                    UpdateParentWidgetGeometry(layoutItem, style);
                }

                break;
            }
            case WT_RangeSlider: {
                auto& state = context.GetState(wid).state.rangeSlider;
                auto style = context.GetStyle(state.state, wid);
                UpdateTooltip(state.tooltip);

                auto deltav = style.margin.v() + style.border.v() + style.padding.v();
                auto deltah = style.margin.h() + style.border.h() + style.padding.h();
                AddExtent(layoutItem, style, neighbors, { state.dir == DIR_Horizontal ? 0.f : style.font.size + deltah,
                    state.dir == DIR_Vertical ? 0.f : style.font.size + deltav }, maxxy);
                auto bounds = RangeSliderBounds(wid, layoutItem.margin);

                if (bounds.GetArea() != layoutItem.margin.GetArea())
                {
                    layoutItem.margin = bounds;
                    layoutItem.border.Min = layoutItem.margin.Min + ImVec2{ style.margin.left, style.margin.top };
                    layoutItem.border.Max = layoutItem.margin.Max - ImVec2{ style.margin.right, style.margin.bottom };
                    layoutItem.padding.Min = layoutItem.border.Min + ImVec2{ style.border.left.thickness, style.border.top.thickness };
                    layoutItem.padding.Min = layoutItem.border.Max + ImVec2{ style.border.right.thickness, style.border.bottom.thickness };
                }

                layoutItem.content = layoutItem.padding;
                if (onlyComputeGeometry) break;

                if (nestedCtx.source == NestedContextSourceType::Layout && !context.layoutStack.empty())
                {
                    auto& layout = context.layouts[context.layoutStack.top()];
                    auto pos = layout.geometry.Min;
                    if ((geometry & ExpandH) && (state.dir == DIR_Horizontal)) layoutItem.sizing |= ExpandH;
                    if ((geometry & ExpandV) && (state.dir == DIR_Vertical)) layoutItem.sizing |= ExpandV;
                    layoutItem.sizing |= state.dir == DIR_Horizontal ? ShrinkH : ShrinkV;
                    AddItemToLayout(layout, layoutItem, style);
                }
                else
                {
                    renderer.SetClipRect(layoutItem.margin.Min, layoutItem.margin.Max);
                    result = RangeSliderImpl(wid, state, style, layoutItem.border, renderer, io);
                    context.AddItemGeometry(wid, bounds);
                    renderer.ResetClipRect();
                    UpdateParentWidgetGeometry(layoutItem, style);
                }
                break;
            }
            case WT_TextInput: {
                auto& state = context.GetState(wid).state.input;
                auto style = context.GetStyle(state.state, wid);
                UpdateTooltip(state.tooltip);

                if (nestedCtx.source == NestedContextSourceType::Layout && !context.layoutStack.empty())
                {
                    auto& layout = context.layouts[context.layoutStack.top()];
                    auto pos = layout.nextpos;
                    if (geometry & ExpandH) layoutItem.sizing |= ExpandH;
                    if (geometry & ExpandV) layoutItem.sizing |= ExpandV;
                    DetermineBounds({ style.dimension.x, style.font.size }, state.prefix, state.suffix, pos, layoutItem, style, renderer, geometry, neighbors);
                    if (onlyComputeGeometry) break;
                    AddItemToLayout(layout, layoutItem, style);
                }
                else
                {
                    auto pos = context.NextAdHocPos();
                    DetermineBounds({ style.dimension.x, style.font.size }, state.prefix, state.suffix, pos, layoutItem, style, renderer, geometry, neighbors);
                    if (onlyComputeGeometry) break;
                    renderer.SetClipRect(layoutItem.margin.Min, layoutItem.margin.Max);
                    result = TextInputImpl(wid, state, style, layoutItem.border, layoutItem.content, layoutItem.prefix, layoutItem.suffix, renderer, io);
                    context.AddItemGeometry(wid, layoutItem.margin);
                    renderer.ResetClipRect();
                    UpdateParentWidgetGeometry(layoutItem, style);
                }

                break;
            }
            case WT_DropDown: {
                auto& state = context.GetState(wid).state.dropdown;
                const auto& ddstyle = WidgetContextData::dropdownStyles[log2((unsigned)state.state)].top();
                auto style = context.GetStyle(state.state, wid);
                auto textsz = (state.sizePolicy == DD_FitToInitialContent) ? GetTextSize(state.textType, state.text, style.font, maxxy.x, renderer) : 
                    state.maxOptionSz;
                UpdateTooltip(state.tooltip);

                if (nestedCtx.source == NestedContextSourceType::Layout && !context.layoutStack.empty())
                {
                    auto& layout = context.layouts[context.layoutStack.top()];
                    auto pos = layout.geometry.Min;
                    DetermineBounds(textsz, ddstyle.isIndicatorSuffix ? std::string_view{} : ddstyle.indicators[state.opened],
                        !ddstyle.isIndicatorSuffix ? std::string_view{} : ddstyle.indicators[state.opened],
                        pos, layoutItem, style, renderer, geometry, neighbors);
                    if (onlyComputeGeometry) break;
                    if (geometry & ExpandH) layoutItem.sizing |= ExpandH;
                    if (geometry & ExpandV) layoutItem.sizing |= ExpandV;
                    AddItemToLayout(layout, layoutItem, style);
                }
                else
                {
                    auto pos = context.NextAdHocPos();
                    DetermineBounds(textsz, ddstyle.isIndicatorSuffix ? std::string_view{} : ddstyle.indicators[state.opened],
                        !ddstyle.isIndicatorSuffix ? std::string_view{} : ddstyle.indicators[state.opened],
                        pos, layoutItem, style, renderer, geometry, neighbors);
                    if (onlyComputeGeometry) break;
                    renderer.SetClipRect(layoutItem.margin.Min, layoutItem.margin.Max);
                    result = DropDownImpl(wid, state, style, layoutItem.margin, layoutItem.border, layoutItem.padding,
                        layoutItem.content, layoutItem.text, renderer, io);
                    context.AddItemGeometry(wid, layoutItem.margin);
                    renderer.ResetClipRect();
                    UpdateParentWidgetGeometry(layoutItem, style);
                }

                break;
            }
            case WT_TabBar: {
                const auto style = context.GetStyle(WS_Default, wid);

                if (nestedCtx.source == NestedContextSourceType::Layout && !context.layoutStack.empty())
                {
                    auto& layout = context.layouts[context.layoutStack.top()];
                    const auto& currentTab = layout.tabbar;
                    if (geometry & ExpandH) layoutItem.sizing |= ExpandH;
                    AddExtent(layoutItem, style, currentTab.neighbors, { 0.f, 0.f }, maxxy);
                    auto bounds = TabBarBounds(currentTab.id, layoutItem.padding, renderer);
                    bounds.Max.x = std::max(bounds.Max.x, layoutItem.border.Max.x);
                    layoutItem.border = layoutItem.padding = layoutItem.content = bounds;
                    if (onlyComputeGeometry) break;
                    AddItemToLayout(layout, layoutItem, style);
                }
                else
                {
                    AddExtent(layoutItem, style, context.currentTab.neighbors, { 0.f, 0.f }, maxxy);
                    if (onlyComputeGeometry) break;
                    auto bounds = TabBarBounds(context.currentTab.id, layoutItem.border, renderer);
                    bounds.Max.x = std::max(bounds.Max.x, layoutItem.border.Max.x);

                    result = TabBarImpl(wid, bounds, style, io, renderer);
                    context.AddItemGeometry(wid, bounds);
                    UpdateParentWidgetGeometry(layoutItem, style);
                }

                break;
            }
            case WT_NavDrawer: {
                auto& navstate = context.NavDrawerState(wid);
                const auto style = context.GetStyle(navstate.state, wid);
                assert(context.layoutStack.empty());

                layoutItem.sizing = context.currentNavDrawer.direction == DIR_Vertical ?
                    AlignLeft | ExpandV : AlignTop | ExpandH;
                AddExtent(layoutItem, style, context.currentNavDrawer.neighbors, { 0.f, 0.f }, maxxy);
                auto bounds = NavDrawerBounds(context.currentNavDrawer.id, layoutItem.padding, renderer);
                layoutItem.margin = layoutItem.border = layoutItem.padding = layoutItem.content = bounds;
                if (onlyComputeGeometry) break;
                result = NavDrawerImpl(wid, bounds, style, io, renderer);
                context.AddItemGeometry(wid, bounds);
                UpdateParentWidgetGeometry(layoutItem, style);

                break;
            }
            case WT_ItemGrid: {
                auto& state = context.GetState(wid).state.grid;
                auto style = context.GetStyle(state.state, wid);

                AddExtent(layoutItem, style, neighbors);

                if (context.nestedContextStack.empty())
                {
                    renderer.SetClipRect(layoutItem.margin.Min, layoutItem.margin.Max);
                    result = ItemGridImpl(wid, style, layoutItem.margin, layoutItem.border, layoutItem.padding,
                        layoutItem.content, layoutItem.text, renderer, io);
                    context.AddItemGeometry(wid, layoutItem.margin);
                    renderer.ResetClipRect();
                }
                else if (nestedCtx.source == NestedContextSourceType::Layout && !context.layoutStack.empty())
                {
                    /*auto& layout = context.layouts[context.layoutStack.top()];
                    auto pos = layout.geometry.Min;
                    if (geometry & ExpandH) layoutItem.sizing |= ExpandH;
                    if (geometry & ExpandV) layoutItem.sizing |= ExpandV;
                    AddItemToLayout(layout, layoutItem, style);*/
                    assert(false); // Not implemented...
                }
                else if (nestedCtx.source == NestedContextSourceType::ItemGrid && !nestedCtx.base->itemGrids.empty())
                {
                    assert(false); // Nested item grid not implemented yet...
                }

                break;
            }
            case WT_MediaResource: {
                auto& state = context.GetState(id).state.media;
                auto style = context.GetStyle(state.state, wid);
                UpdateTooltip(state.tooltip);

                if (nestedCtx.source == NestedContextSourceType::Layout && !context.layoutStack.empty())
                {
                    auto& layout = context.layouts[context.layoutStack.top()];
                    auto pos = layout.geometry.Min;

                    if (geometry & ExpandH) layoutItem.sizing |= ExpandH;
                    if (geometry & ExpandV) layoutItem.sizing |= ExpandV;
                    DetermineIconBounds(pos, state, style, layoutItem);
                    if (onlyComputeGeometry) break;
                    AddItemToLayout(layout, layoutItem, style);
                }
                else
                {
                    auto pos = context.NextAdHocPos();
                    DetermineIconBounds(pos, state, style, layoutItem);
                    if (onlyComputeGeometry) break;
                    context.AddItemGeometry(wid, layoutItem.margin);
                    result = MediaResourceImpl(wid, style, layoutItem.margin, layoutItem.border, layoutItem.padding,
                        layoutItem.content, renderer, io);
                    UpdateParentWidgetGeometry(layoutItem, style);
                }
                break;
            }
            default: break;
            }
        }
        else if (Config.CustomWidgetProvider != nullptr)
        {
            auto customType = (int16_t)(wid >> WidgetTypeBits);
            auto wfactory = Config.CustomWidgetProvider(customType);

            if (wfactory != nullptr)
            {
                auto state = wfactory->GetState(wid);
                const auto style = wfactory->GetStyle(wid, state, WidgetContextData::StyleStack[WSI_Default]);

                if (nestedCtx.source == NestedContextSourceType::Layout && !context.layoutStack.empty())
                {
                    auto& layout = context.layouts[context.layoutStack.top()];
                    auto pos = layout.geometry.Min;
                    wfactory->ComputeGeometry(pos, layoutItem, neighbors, maxxy);
                    if (!onlyComputeGeometry)
                        AddItemToLayout(layout, layoutItem, style);
                }
                else
                {
                    auto pos = context.NextAdHocPos();
                    wfactory->ComputeGeometry(pos, layoutItem, neighbors, maxxy);

                    if (!onlyComputeGeometry)
                    {
                        result = DrawCustomWidget(wid, style, layoutItem, renderer, io);
                        UpdateParentWidgetGeometry(layoutItem, style);
                    }
                }
            }
        }

        if (onlyComputeGeometry) result.geometry = layoutItem.margin;
        result.id = wid;
        PreviousWidget = wid;
        return result;
    }

#pragma endregion
}

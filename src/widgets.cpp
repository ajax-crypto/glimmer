#include "widgets.h"

#ifdef IM_RICHTEXT_TARGET_IMGUI
#include "libs/inc/imgui/imgui.h"
#endif
#ifdef IM_RICHTEXT_TARGET_BLEND2D
#include "libs/inc/blend2d/blend2d.h"
#endif

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

namespace glimmer
{
    // This is required, as for some widgets, double clicking leads to a editor, and hence a text input widget
    WidgetDrawResult TextInputImpl(int32_t id, TextInputState& state, const ImRect& extent, const ImRect& content,
        IRenderer& renderer, const IODescriptor& io);

    WidgetStateData::WidgetStateData(WidgetType ty)
        : type{ ty }
    {
        switch (type)
        {
        case WT_Label: ::new (&state.label) LabelState{}; break;
        case WT_Button: ::new (&state.button) ButtonState{}; break;
        case WT_RadioButton: new (&state.radio) RadioButtonState{}; break;
        case WT_ToggleButton: new (&state.toggle) ToggleButtonState{}; break;
        case WT_Checkbox: new (&state.checkbox) CheckboxState{}; break;
        case WT_Spinner: new (&state.spinner) SpinnerState{}; break;
        case WT_Slider: new (&state.slider) SliderState{}; break;
        case WT_TextInput: new (&state.input) TextInputState{}; break;
        case WT_DropDown: new (&state.dropdown) DropDownState{}; break;
        case WT_SplitterScrollRegion: [[fallthrough]];
        case WT_Scrollable: new (&state.scroll) ScrollableRegion{}; break;
        case WT_TabBar: new (&state.tab) TabBarState{}; break;
        case WT_ItemGrid: ::new (&state.grid) ItemGridState{}; break;
        default: break;
        }
    }

    WidgetStateData::WidgetStateData(const WidgetStateData& src)
        : WidgetStateData{ src.type }
    {
        switch (type)
        {
        case WT_Label: state.label = src.state.label; break;
        case WT_Button: state.button = src.state.button; break;
        case WT_RadioButton: state.radio = src.state.radio; break;
        case WT_ToggleButton: state.toggle = src.state.toggle; break;
        case WT_Checkbox: state.checkbox = src.state.checkbox; break;
        case WT_Spinner: state.spinner = src.state.spinner; break;
        case WT_Slider: state.slider = src.state.slider; break;
        case WT_TextInput: state.input = src.state.input; break;
        case WT_DropDown: state.dropdown = src.state.dropdown; break;
        case WT_SplitterScrollRegion: [[fallthrough]];
        case WT_Scrollable: state.scroll = src.state.scroll; break;
        case WT_TabBar: state.tab = src.state.tab; break;
        case WT_ItemGrid: state.grid = src.state.grid; break;
        default: break;
        }
    }

    WidgetStateData& WidgetStateData::operator=(const WidgetStateData& src)
    {
        type = src.type;
        switch (src.type)
        {
        case WT_Label: state.label = src.state.label; break;
        case WT_Button: state.button = src.state.button; break;
        case WT_RadioButton: state.radio = src.state.radio; break;
        case WT_ToggleButton: state.toggle = src.state.toggle; break;
        case WT_Checkbox: state.checkbox = src.state.checkbox; break;
        case WT_Spinner: state.spinner = src.state.spinner; break;
        case WT_Slider: state.slider = src.state.slider; break;
        case WT_TextInput: state.input = src.state.input; break;
        case WT_DropDown: state.dropdown = src.state.dropdown; break;
        case WT_SplitterScrollRegion: [[fallthrough]];
        case WT_Scrollable: state.scroll = src.state.scroll; break;
        case WT_TabBar: state.tab = src.state.tab; break;
        case WT_ItemGrid: state.grid = src.state.grid; break;
        default: break;
        }
        return *this;
    }

    WidgetStateData::~WidgetStateData()
    {
        switch (type)
        {
        case WT_Label: state.label.~LabelState(); break;
        case WT_Button: state.button.~ButtonState(); break;
        case WT_RadioButton: break;
        case WT_ToggleButton: break;
        case WT_ItemGrid: state.grid.~ItemGridState(); break;
        default: break;
        }
    }

    WidgetDrawResult Widget(int32_t id, WidgetType type, int32_t geometry, const NeighborWidgets& neighbors);

    static bool IsBetween(float point, float min, float max, float tolerance = 0.f)
    {
        return (point < (max + tolerance)) && (point > (min - tolerance));
    }

    static ImVec2 GetTextSize(TextType type, std::string_view text, const FontStyle& font, float width, IRenderer& renderer)
    {
        switch (type)
        {
        case glimmer::TextType::PlainText: return renderer.GetTextSize(text, font.font, font.size, width);
        case glimmer::TextType::RichText: return ImVec2{ 0, 0 }; // TODO...
        case glimmer::TextType::SVG: return ImVec2{ font.size, font.size };
        default: break;
        }
    }

    void ShowTooltip(float& hoverDuration, const ImRect& margin, ImVec2 pos, std::string_view tooltip, const IODescriptor& io, IRenderer& renderer)
    {
        hoverDuration += io.deltaTime;
        
        if (hoverDuration >= Config.tooltipDelay)
        {
            auto font = GetFont(Config.tooltipFontFamily, Config.tooltipFontSz, FT_Normal);
            auto textsz = renderer.GetTextSize(tooltip, font, Config.tooltipFontSz);
            auto offsetx = std::max(0.f, margin.GetWidth() - textsz.x) * 0.5f;
            auto tooltippos = pos + ImVec2{ offsetx, -(textsz.y + 2.f) };
            renderer.DrawTooltip(tooltippos, tooltip);
        }
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
        else layoutItem.margin.Max.x = layoutItem.margin.Min.x + (sz.x - nextpos.x);

        layoutItem.border.Max.x = layoutItem.margin.Max.x;
        layoutItem.padding.Max.x = layoutItem.border.Max.x;
        layoutItem.content.Max.x = layoutItem.padding.Max.x;

        if (neighbors.bottom != -1) layoutItem.margin.Max.y = context.GetGeometry(neighbors.bottom).Min.y;
        else layoutItem.margin.Max.y = layoutItem.margin.Min.y + (sz.y - nextpos.y);

        layoutItem.border.Max.y = layoutItem.margin.Max.y;
        layoutItem.padding.Max.y = layoutItem.border.Max.y;
        layoutItem.content.Max.y = layoutItem.padding.Max.y;
    }

#pragma region Scrollbars

    static bool HandleHScroll(ScrollBarState& scroll, float width, const ImRect& viewport,
        ImVec2 mousepos, IRenderer& renderer, const IODescriptor& io, float btnsz, bool showButtons = true)
    {
        auto hasHScroll = false;
        float gripExtent = 0.f;
        const float opacityRatio = (256.f / Config.scrollAppearAnimationDuration);
        const float vwidth = viewport.GetWidth();
        const float posrange = width - vwidth;

        if (width > vwidth)
        {
            auto hasOpacity = scroll.opacity > 0.f;
            auto hasMouseInteraction = (viewport.Contains(mousepos) && (mousepos.y <= viewport.Max.y) && mousepos.y >= (viewport.Max.y - (1.5f * btnsz)))
                || scroll.mouseDownOnHGrip;

            if (hasMouseInteraction || hasOpacity)
            {
                if (hasMouseInteraction && scroll.opacity < 255.f)
                    scroll.opacity = std::min((opacityRatio * io.deltaTime) + scroll.opacity, 255.f);
                else if (!hasMouseInteraction && scroll.opacity > 0.f)
                    scroll.opacity = std::max(scroll.opacity - (opacityRatio * io.deltaTime), 0.f);

                auto lrsz = showButtons ? btnsz : 0.f;
                ImRect left{ { viewport.Min.x, viewport.Max.y - lrsz }, { viewport.Min.x + lrsz, viewport.Max.y } };
                ImRect right{ { viewport.Max.x - lrsz, viewport.Max.y }, viewport.Max };
                ImRect path{ { left.Max.x, left.Min.y }, { right.Min.x, right.Max.y } };

                auto pathsz = path.GetWidth();
                auto sizeOfGrip = (vwidth / width) * pathsz;
                auto spos = ((pathsz - sizeOfGrip) / posrange) * scroll.pos.x;
                ImRect grip{ { left.Max.x + spos, viewport.Max.y - btnsz },
                    { left.Max.x + spos + sizeOfGrip, viewport.Max.y } };

                if (showButtons)
                {
                    renderer.DrawRect(left.Min, left.Max, ToRGBA(175, 175, 175, (int)scroll.opacity), true);
                    renderer.DrawTriangle({ left.Min.x + (btnsz * 0.25f), left.Min.y + (0.5f * btnsz) },
                        { left.Max.x - (0.125f * btnsz), left.Min.y + (0.125f * btnsz) },
                        { left.Max.x - (0.125f * btnsz), left.Max.y - (0.125f * btnsz) }, ToRGBA(100, 100, 100, (int)scroll.opacity), true);

                    renderer.DrawRect(right.Min, right.Max, ToRGBA(175, 175, 175, (int)scroll.opacity), true);
                    renderer.DrawTriangle({ right.Min.x + (btnsz * 0.25f), right.Min.y + (0.125f * btnsz) },
                        { right.Max.x - (0.125f * btnsz), right.Min.y + (0.5f * btnsz) },
                        { right.Min.x + (btnsz * 0.25f), right.Max.y - (0.125f * btnsz) }, ToRGBA(100, 100, 100, (int)scroll.opacity), true);
                }

                if (grip.Contains(mousepos))
                {
                    if (io.isLeftMouseDown())
                    {
                        if (!scroll.mouseDownOnHGrip)
                        {
                            scroll.mouseDownOnHGrip = true;
                            scroll.lastMousePos.y = mousepos.y;
                        }

                        auto step = mousepos.x - scroll.lastMousePos.x;
                        step = (posrange / (pathsz - sizeOfGrip)) * step;
                        scroll.pos.x = ImClamp(scroll.pos.x + step, 0.f, posrange);
                        renderer.DrawRect(grip.Min, grip.Max, ToRGBA(100, 100, 100), true);
                        scroll.lastMousePos.x = mousepos.x;
                    }
                    else
                        renderer.DrawRect(grip.Min, grip.Max, ToRGBA(150, 150, 150, (int)scroll.opacity), true);
                }
                else
                {
                    if (scroll.mouseDownOnHGrip)
                    {
                        auto step = mousepos.x - scroll.lastMousePos.x;
                        step = (posrange / (pathsz - sizeOfGrip)) * step;
                        scroll.pos.x = ImClamp(scroll.pos.x + step, 0.f, posrange);
                        renderer.DrawRect(grip.Min, grip.Max, ToRGBA(100, 100, 100), true);
                        scroll.lastMousePos.x = mousepos.x;
                    }
                    else
                        renderer.DrawRect(grip.Min, grip.Max, ToRGBA(150, 150, 150, (int)scroll.opacity), true);

                    if (left.Contains(mousepos) && io.isLeftMouseDown())
                        scroll.pos.x = ImClamp(scroll.pos.x - 1.f, 0.f, posrange);
                    else if (right.Contains(mousepos) && io.isLeftMouseDown())
                        scroll.pos.x = ImClamp(scroll.pos.x + 1.f, 0.f, posrange);
                }

                if (!io.isLeftMouseDown() && scroll.mouseDownOnHGrip)
                {
                    scroll.mouseDownOnHGrip = false;
                    renderer.DrawRect(grip.Min, grip.Max, ToRGBA(100, 100, 100), true);
                }
            }

            hasHScroll = true;
        }

        return hasHScroll;
    }

    static bool HandleVScroll(ScrollBarState& scroll, float height, const ImRect& viewport, ImVec2 mousepos,
        IRenderer& renderer, const IODescriptor& io, float btnsz, bool hasHScroll = false)
    {
        const float opacityRatio = (256.f / Config.scrollAppearAnimationDuration);
        const auto vheight = viewport.GetHeight();
        const float posrange = height - vheight;

        if (height > vheight)
        {
            auto hasOpacity = scroll.opacity > 0.f;
            auto hasMouseInteraction = (viewport.Contains(mousepos) && (mousepos.x <= viewport.Max.x) && mousepos.x >= (viewport.Max.x - (1.5f * btnsz)) &&
                (!hasHScroll || mousepos.y < (viewport.Max.y - btnsz))) || scroll.mouseDownOnVGrip;

            if (hasMouseInteraction || hasOpacity)
            {
                if (hasMouseInteraction && scroll.opacity < 255.f)
                    scroll.opacity = std::min((opacityRatio * io.deltaTime) + scroll.opacity, 255.f);
                else if (!hasMouseInteraction && scroll.opacity > 0.f)
                    scroll.opacity = std::max(scroll.opacity - (opacityRatio * io.deltaTime), 0.f);

                auto extrah = hasHScroll ? btnsz : 0.f;
                ImRect top{ { viewport.Max.x - btnsz, viewport.Min.y }, { viewport.Max.x, viewport.Min.y + btnsz } };
                ImRect bottom{ { viewport.Max.x - btnsz, viewport.Max.y - btnsz - extrah }, viewport.Max };
                ImRect path{ { top.Min.x, top.Max.y }, { bottom.Max.x, bottom.Min.y } };

                auto pathsz = path.GetHeight();
                auto sizeOfGrip = (vheight / height) * pathsz;
                auto spos = ((pathsz - sizeOfGrip) / posrange) * scroll.pos.y;
                ImRect grip{ { viewport.Max.x - btnsz, top.Max.y + spos },
                    { viewport.Max.x, sizeOfGrip + top.Max.y + spos } };

                renderer.DrawRect(top.Min, top.Max, ToRGBA(175, 175, 175, (int)scroll.opacity), true);
                renderer.DrawTriangle({ top.Min.x + (btnsz * 0.5f), top.Min.y + (0.25f * btnsz) },
                    { top.Max.x - (0.125f * btnsz), top.Min.y + (0.75f * btnsz) },
                    { top.Min.x + (0.125f * btnsz), top.Min.y + (0.75f * btnsz) }, ToRGBA(100, 100, 100, (int)scroll.opacity), true);

                renderer.DrawRect(bottom.Min, bottom.Max, ToRGBA(175, 175, 175, (int)scroll.opacity), true);
                renderer.DrawTriangle({ bottom.Min.x + (btnsz * 0.125f), bottom.Min.y + (0.25f * btnsz) },
                    { bottom.Max.x - (0.125f * btnsz), bottom.Min.y + (0.25f * btnsz) },
                    { bottom.Max.x - (0.5f * btnsz), bottom.Max.y - (0.25f * btnsz) }, ToRGBA(100, 100, 100, (int)scroll.opacity), true);

                if (grip.Contains(mousepos))
                {
                    if (io.isLeftMouseDown())
                    {
                        if (!scroll.mouseDownOnVGrip)
                        {
                            scroll.mouseDownOnVGrip = true;
                            scroll.lastMousePos.y = mousepos.y;
                        }

                        auto step = mousepos.y - scroll.lastMousePos.y;
                        step = (posrange / (pathsz - sizeOfGrip)) * step;
                        scroll.pos.y = ImClamp(scroll.pos.y + step, 0.f, posrange);
                        renderer.DrawRect(grip.Min, grip.Max, ToRGBA(100, 100, 100), true);
                        scroll.lastMousePos.y = mousepos.y;
                    }
                    else
                        renderer.DrawRect(grip.Min, grip.Max, ToRGBA(150, 150, 150, (int)scroll.opacity), true);
                }
                else
                {
                    if (scroll.mouseDownOnVGrip)
                    {
                        auto step = mousepos.y - scroll.lastMousePos.y;
                        step = (posrange / (pathsz - sizeOfGrip)) * step;
                        scroll.pos.y = ImClamp(scroll.pos.y + step, 0.f, posrange);
                        renderer.DrawRect(grip.Min, grip.Max, ToRGBA(100, 100, 100), true);
                        scroll.lastMousePos.y = mousepos.y;
                    }
                    else
                        renderer.DrawRect(grip.Min, grip.Max, ToRGBA(150, 150, 150, (int)scroll.opacity), true);

                    if (top.Contains(mousepos) && io.isLeftMouseDown())
                        scroll.pos.y = ImClamp(scroll.pos.y - 1.f, 0.f, posrange);
                    else if (bottom.Contains(mousepos) && io.isLeftMouseDown())
                        scroll.pos.y = ImClamp(scroll.pos.y + 1.f, 0.f, posrange);
                }

                if (!io.isLeftMouseDown() && scroll.mouseDownOnVGrip)
                {
                    scroll.mouseDownOnVGrip = false;
                    renderer.DrawRect(grip.Min, grip.Max, ToRGBA(100, 100, 100), true);
                }
            }

            if (viewport.Contains(mousepos))
            {
                auto rotation = io.mouseWheel;
                scroll.pos.y = ImClamp(rotation + scroll.pos.y, 0.f, posrange);
            }

            return true;
        }

        return false;
    }

    void StartScrollableRegion(int32_t id, bool hscroll, bool vscroll, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto& context = GetContext();
        auto& region = context.ScrollRegion(id);
        const auto& style = context.GetStyle(WS_Default);

        auto& renderer = context.usingDeferred ? *context.deferedRenderer : *Config.renderer;
        LayoutItemDescriptor layoutItem;
        AddExtent(layoutItem, style, neighbors);
        DrawBorderRect(layoutItem.border.Min, layoutItem.border.Max, style.border, style.bgcolor, renderer);

        region.viewport = layoutItem.content;
        region.enabled = { hscroll, vscroll };
        renderer.SetClipRect(layoutItem.content.Min, layoutItem.content.Max);
        context.containerStack.push(id);
    }

    void EndScrollableRegion()
    {
        auto& context = GetContext();
        auto id = context.containerStack.top();
        auto& region = context.ScrollRegion(id);
        auto& renderer = context.usingDeferred ? *context.deferedRenderer : *Config.renderer;
        renderer.ResetClipRect();

        auto hasHScroll = false;
        auto io = Config.platform->CurrentIO();
        auto mousepos = io.mousepos;
        if (region.viewport.Max.x < region.max.x && region.enabled.first)
        {
            hasHScroll = true;
            HandleHScroll(region.state, region.max.x - region.viewport.Min.x, region.viewport, mousepos, renderer, io,
                Config.scrollbarSz);
        }

        if (region.viewport.Max.y < region.max.y && region.enabled.second)
            HandleVScroll(region.state, region.max.y - region.viewport.Min.y, region.viewport, mousepos, renderer, io,
                Config.scrollbarSz, hasHScroll);

        context.containerStack.pop();
        context.AddItemGeometry(id, region.viewport);
    }

#pragma endregion

#pragma region Button & Lables

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

        auto hastextw = (style.specified & StyleWidth) && !((style.font.flags & FontStyleOverflowEllipsis) ||
            (style.font.flags & FontStyleOverflowMarquee));
        ImVec2 textsz{ 0.f, 0.f };
        auto textMetricsComputed = false, hexpanded = false, vexpanded = false;

        auto setHFromContent = [&] {
            if (geometry & ToLeft)
            {
                content.Max.x = (style.specified & StyleWidth) ? style.dimension.x :
                    content.Min.x - clamp(textsz.x, style.mindim.x, style.maxdim.x);
                padding.Max.x = content.Max.x - style.padding.left;
                border.Max.x = padding.Max.x - borderstyle.left.thickness;
                margin.Max.x = border.Max.x - style.margin.left;
            }
            else
            {
                content.Max.x = (style.specified & StyleWidth) ? style.dimension.x :
                    content.Min.x + clamp(textsz.x, style.mindim.x, style.maxdim.x);
                padding.Max.x = content.Max.x + style.padding.right;
                border.Max.x = padding.Max.x + borderstyle.right.thickness;
                margin.Max.x = border.Max.x + style.margin.right;
            }
            };

        auto setVFromContent = [&] {
            if (geometry & ToTop)
            {
                content.Max.y = (style.specified & StyleHeight) ? style.dimension.y :
                    content.Min.y - clamp(textsz.y, style.mindim.y, style.maxdim.y);
                padding.Max.y = content.Max.y - style.padding.top;
                border.Max.y = padding.Max.y - borderstyle.top.thickness;
                margin.Max.y = border.Max.y - style.margin.top;
            }
            else
            {
                content.Max.y = (style.specified & StyleHeight) ? style.dimension.y :
                    content.Min.y + clamp(textsz.y, style.mindim.y, style.maxdim.y);
                padding.Max.y = content.Max.y + style.padding.bottom;
                border.Max.y = padding.Max.y + borderstyle.bottom.thickness;
                margin.Max.y = border.Max.y + style.margin.bottom;
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
            if (!context.layouts.empty())
            {
                auto isLayoutFit = (context.layouts.top().fill & FD_Horizontal) == 0;

                if (!isLayoutFit)
                {
                    setHFromExpansion((geometry & ToLeft) ? context.layouts.top().prevpos.x :
                        context.layouts.top().nextpos.x);
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
            if (!context.layouts.empty())
            {
                auto isLayoutFit = (context.layouts.top().fill & FD_Vertical) == 0;

                if (!isLayoutFit)
                {
                    setVFromExpansion((geometry & ToTop) ? context.layouts.top().prevpos.y :
                        context.layouts.top().nextpos.y);
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

        return std::make_tuple(content, padding, border, margin, ImRect{ textpos, textpos + textsz });
    }

    void HandleLabelEvent(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io, WidgetDrawResult& result)
    {
        auto& context = GetContext();
        if (!context.deferEvents)
        {
            auto& state = context.GetState(id).state.label;
            auto ismouseover = padding.Contains(io.mousepos);
            if (ismouseover && !state.tooltip.empty() && !io.isMouseDown())
                ShowTooltip(state._hoverDuration, margin, margin.Min, state.tooltip, io, renderer);
            else state._hoverDuration == 0;
        }
        else context.deferedEvents.emplace_back(WT_Label, id, margin, border, padding, content, text);
    }

    void HandleButtonEvent(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io, WidgetDrawResult& result)
    {
        auto& context = GetContext();
        if (!context.deferEvents)
        {
            auto& state = context.GetState(id).state.button;
            auto ismouseover = padding.Contains(io.mousepos);
            state.state = !ismouseover ? WS_Default :
                io.isLeftMouseDown() ? WS_Pressed | WS_Hovered : WS_Hovered;
            if (ismouseover && io.clicked())
                result.event = WidgetEvent::Clicked;
            else if (ismouseover && !state.tooltip.empty() && !io.isMouseDown())
                ShowTooltip(state._hoverDuration, margin, margin.Min, state.tooltip, io, renderer);
            else state._hoverDuration == 0;
        }
        else context.deferedEvents.emplace_back(WT_Button, id, margin, border, padding, content, text);
    }

    WidgetDrawResult LabelImpl(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io, int32_t textflags)
    {
        auto& context = GetContext();
        assert((id & 0xffff) <= (int)context.states[WT_Label].size());

        WidgetDrawResult result;
        auto& state = context.GetState(id).state.label;
        auto& style = context.GetStyle(state.state);

        DrawBoxShadow(border.Min, border.Max, style, renderer);
        DrawBackground(border.Min, border.Max, style, renderer);
        DrawBorderRect(border.Min, border.Max, style.border, style.bgcolor, renderer);
        DrawText(content.Min, content.Max, text, state.text, state.state & WS_Disabled, style, renderer, textflags | style.font.flags);
        HandleLabelEvent(id, margin, border, padding, content, text, renderer, io, result);
        
        result.geometry = margin;
        return result;
    }

    WidgetDrawResult ButtonImpl(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io)
    {
        WidgetDrawResult result;
        auto& context = GetContext();
        auto& state = context.GetState(id).state.button;
        auto& style = context.GetStyle(state.state);

        DrawBoxShadow(border.Min, border.Max, style, renderer);
        DrawBackground(border.Min, border.Max, style, renderer);
        DrawBorderRect(border.Min, border.Max, style.border, style.bgcolor, renderer);
        DrawText(content.Min, content.Max, text, state.text, state.state & WS_Disabled, style, renderer);
        HandleButtonEvent(id, margin, border, padding, content, text, renderer, io, result);

        result.geometry = margin;
        return result;
    }

#pragma endregion

#pragma region Toggle Button

    static std::pair<ImRect, ImVec2> ToggleButtonBounds(ToggleButtonState& state, const ImRect& extent, IRenderer& renderer)
    {
        auto& context = GetContext();
        auto& style = context.GetStyle(state.state);
        auto& specificStyle = context.toggleButtonStyles[log2((unsigned)state.state)].top();
        ImRect result;
        ImVec2 text;
        result.Min = result.Max = extent.Min;

        if (specificStyle.showText)
        {
            if (specificStyle.fontptr == nullptr) specificStyle.fontptr = GetFont(IM_RICHTEXT_DEFAULT_FONTFAMILY,
                specificStyle.fontsz, FT_Bold);

            renderer.SetCurrentFont(specificStyle.fontptr, specificStyle.fontsz);
            text = renderer.GetTextSize("ONOFF", specificStyle.fontptr, specificStyle.fontsz);
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

    void HandleToggleButtonEvent(int32_t id, const ImRect& extent, ImVec2 center, IRenderer& renderer, const IODescriptor& io,
        WidgetDrawResult& result)
    {
        auto& context = GetContext();
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
                toggle.progress = 0.f;
            }

            toggle.btnpos = toggle.animate ? center.x : -1.f;
            state.state = mouseover && io.isLeftMouseDown() ? WS_Hovered | WS_Pressed :
                mouseover ? WS_Hovered : WS_Default;
            state.state = state.checked ? state.state | WS_Checked : state.state & ~WS_Checked;
        }
        else context.deferedEvents.emplace_back(WT_ToggleButton, id, center, extent);
    }

    WidgetDrawResult ToggleButtonImpl(int32_t id, ToggleButtonState& state, const ImRect& extent, ImVec2 textsz, 
        IRenderer& renderer, const IODescriptor& io)
    {
        WidgetDrawResult result;
        auto& context = GetContext();
        auto& style = context.GetStyle(state.state);
        auto& specificStyle = context.toggleButtonStyles[log2((unsigned)state.state)].top();
        auto& toggle = context.ToggleState(id);

        auto extra = (-specificStyle.thumbOffset + specificStyle.trackBorderThickness);
        auto radius = (extent.GetHeight() * 0.5f) - (2.f * extra);
        auto movement = extent.GetWidth() - (2.f * (radius + extra));
        auto moveAmount = toggle.animate ? (io.deltaTime / specificStyle.animate) * movement * (state.checked ? 1.f : -1.f) : 0.f;
        toggle.progress += std::fabsf(moveAmount / movement);

        auto center = toggle.btnpos == -1.f ? state.checked ? extent.Max - ImVec2{ extra + radius, extra + radius }
            : extent.Min + ImVec2{ radius + extra, extra + radius }
        : ImVec2{ toggle.btnpos, extra + radius };
        center.x = ImClamp(center.x + moveAmount, extent.Min.x + (extra * 0.5f), extent.Max.x - extra);
        center.y = extent.Min.y + (extent.GetHeight() * 0.5f);
        toggle.animate = (center.x < (extent.Max.x - extra - radius)) && (center.x > (extent.Min.x + extra + radius));

        auto rounded = extent.GetHeight() * 0.5f;
        auto tcol = specificStyle.trackColor;
        if (toggle.animate)
        {
            auto prevTCol = state.checked ? context.toggleButtonStyles[WSI_Default].top().trackColor :
                context.toggleButtonStyles[WSI_Checked].top().trackColor;
            auto [fr, fg, fb, fa] = DecomposeColor(prevTCol);
            auto [tr, tg, tb, ta] = DecomposeColor(tcol);
            tr = (int)((1.f - toggle.progress) * (float)fr + toggle.progress * (float)tr);
            tg = (int)((1.f - toggle.progress) * (float)fg + toggle.progress * (float)tg);
            tb = (int)((1.f - toggle.progress) * (float)fb + toggle.progress * (float)tb);
            ta = (int)((1.f - toggle.progress) * (float)fa + toggle.progress * (float)ta);
            tcol = ToRGBA(tr, tg, tb, ta);
        }

        auto [tr, tg, tb, ta] = DecomposeColor(tcol);
        renderer.DrawRoundedRect(extent.Min, extent.Max, tcol, true, rounded, rounded, rounded, rounded);
        renderer.DrawRoundedRect(extent.Min, extent.Max, specificStyle.trackBorderColor, false, rounded, rounded, rounded, rounded,
            specificStyle.trackBorderThickness);

        if (specificStyle.showText && !toggle.animate)
        {
            renderer.SetCurrentFont(specificStyle.fontptr, specificStyle.fontsz);
            auto texth = ((extent.GetHeight() - textsz.y) * 0.5f) - 2.f;
            state.checked ? renderer.DrawText("ON", extent.Min + ImVec2{ extra, texth }, specificStyle.indicatorTextColor) :
                renderer.DrawText("OFF", extent.Min + ImVec2{ (extent.GetWidth() * 0.5f) - 5.f, texth }, specificStyle.indicatorTextColor);
            renderer.ResetFont();
        }

        renderer.DrawCircle(center, radius + specificStyle.thumbExpand, style.fgcolor, true);
        HandleToggleButtonEvent(id, extent, center, renderer, io, result);

        result.geometry = extent;
        return result;
    }

#pragma endregion

#pragma region Radio Button

    static ImRect RadioButtonBounds(const RadioButtonState& state, const ImRect& extent)
    {
        auto& context = GetContext();
        const auto& style = context.GetStyle(state.state);
        return ImRect{ extent.Min, extent.Min + ImVec2{ style.font.size, style.font.size } };
    }

    void HandleRadioButtonEvent(int32_t id, const ImRect& extent, float maxrad, IRenderer& renderer, const IODescriptor& io,
        WidgetDrawResult& result)
    {
        auto& context = GetContext();
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
                radio.progress = 0.f;
                radio.radius = state.checked ? 0.f : maxrad;
            }

            state.state = mouseover && io.isLeftMouseDown() ? WS_Hovered | WS_Pressed :
                mouseover ? WS_Hovered : WS_Default;
            state.state = state.checked ? state.state | WS_Checked : state.state & ~WS_Checked;
        }
        else context.deferedEvents.emplace_back(WT_RadioButton, id, extent, maxrad);
    }

    WidgetDrawResult RadioButtonImpl(int32_t id, RadioButtonState& state, const ImRect& extent, 
        IRenderer& renderer, const IODescriptor& io)
    {
        WidgetDrawResult result;
        auto& context = GetContext();
        auto& style = context.GetStyle(state.state);
        auto& specificStyle = context.radioButtonStyles[log2((unsigned)state.state)].top();
        auto& radio = context.RadioState(id);

        auto radius = (extent.GetWidth() - 2.f) * 0.5f;
        auto center = extent.Min + ImVec2{ radius + 1.f, radius + 1.f };
        renderer.DrawCircle(center, radius, specificStyle.outlineColor, false, specificStyle.outlineThickness);
        auto maxrad = radius * specificStyle.checkedRadius;
        radio.radius = radio.radius == -1.f ? state.checked ? maxrad : 0.f : radio.radius;
        radius = radio.radius;

        if (radius > 0.f) renderer.DrawCircle(center, radius, specificStyle.checkedColor, true);

        auto ratio = radio.animate ? (io.deltaTime / specificStyle.animate) : 0.f;
        radio.progress += ratio;
        radio.radius += ratio * maxrad * (state.checked ? 1.f : -1.f);
        radio.animate = radio.radius > 0.f && radio.radius < maxrad;
        HandleRadioButtonEvent(id, extent, maxrad, renderer, io, result);
        
        result.geometry = extent;
        return result;
    }

#pragma endregion

#pragma region Checkbox

    static ImRect CheckboxBounds(const CheckboxState& state, const ImRect& extent)
    {
        auto& context = GetContext();
        const auto& style = context.GetStyle(state.state);
        return ImRect{ extent.Min, extent.Min + ImVec2{ style.font.size, style.font.size } };
    }

    void HandleCheckboxEvent(int32_t id, const ImRect& extent, const IODescriptor& io,
        WidgetDrawResult& result)
    {
        auto& context = GetContext();

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
        }
        else context.deferedEvents.emplace_back(WT_Checkbox, id, extent);
    }

    WidgetDrawResult CheckboxImpl(int32_t id, CheckboxState& state, const ImRect& extent, const ImRect& padding, 
        IRenderer& renderer, const IODescriptor& io)
    {
        auto& context = GetContext();
        auto& style = context.GetStyle(state.state);
        auto& check = context.CheckboxState(id);
        WidgetDrawResult result;

        DrawBorderRect(extent.Min, extent.Max, style.border, style.bgcolor, renderer);
        DrawBackground(extent.Min, extent.Max, style, renderer);
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

        HandleCheckboxEvent(id, extent, io, result);
        
        result.geometry = extent;
        return result;
    }

#pragma endregion

#pragma region Spinner

    static ImRect SpinnerBounds(int32_t id, const SpinnerState& state, IRenderer& renderer, const ImRect& extent)
    {
        auto digits = (int)(std::ceilf(std::log10f(state.max) + 1.f)) + (!state.isInteger ? state.precision + 1 : 0);
        auto& context = GetContext();
        const auto& style = context.GetStyle(state.state);

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
        if (style.specified & StyleWidth) result.Max.x = result.Min.x + style.dimension.x;
        if (style.specified & StyleHeight) result.Max.y = result.Min.y + style.dimension.y;
        return result;
    }

    void HandleSpinnerEvent(int32_t id, const ImRect& extent, const ImRect& incbtn, const ImRect& decbtn, const IODescriptor& io, 
        WidgetDrawResult& result)
    {
        auto& context = GetContext();

        if (!context.deferEvents)
        {
            auto& state = context.GetState(id).state.spinner;

            if (incbtn.Contains(io.mousepos))
            {
                if (io.isLeftMouseDown() || io.clicked())
                {
                    state.data = std::min(state.data + state.delta, state.max);
                    result.event = WidgetEvent::Edited;
                }
            }
            else if (decbtn.Contains(io.mousepos))
            {
                if (io.isLeftMouseDown() || io.clicked())
                {
                    state.data = std::max(state.data - state.delta, state.min);
                    result.event = WidgetEvent::Edited;
                }
            }
        }
        else
            context.deferedEvents.emplace_back(WT_Spinner, id, extent, incbtn, decbtn);
    }

    uint32_t DarkenColor(uint32_t rgba)
    {
        auto [r, g, b, a] = DecomposeColor(rgba);
        r = r / 2; g = g / 2; b = b / 2;
        return ToRGBA(r, g, b, a);
    }

    WidgetDrawResult SpinnerImpl(int32_t id, const SpinnerState& state, const ImRect& extent, const IODescriptor& io, IRenderer& renderer)
    {
        WidgetDrawResult result;
        ImRect incbtn, decbtn;
        auto& context = GetContext();
        const auto& style = context.GetStyle(state.state);
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

            if (!specificStyle.upDownArrows)
            {
                if (inc)
                {
                    auto halfw = rect.GetWidth() * 0.5f, halfh = rect.GetHeight() * 0.5f;
                    renderer.DrawLine(ImVec2{ rect.Min.x + halfw - 1.f, rect.Min.y + 2.f },
                        ImVec2{ rect.Min.x + halfw - 1.f, rect.Max.y - 2.f }, darker, 2.f);
                    renderer.DrawLine(ImVec2{ rect.Min.x + 2.f, rect.Min.y + halfh - 1.f },
                        ImVec2{ rect.Max.x - 2.f, rect.Min.y + halfh - 1.f }, darker, 2.f);
                }
                else
                {
                    auto halfh = rect.GetHeight() * 0.5f;
                    renderer.DrawLine(ImVec2{ rect.Min.x + 2.f, rect.Min.y + halfh - 1.f },
                        ImVec2{ rect.Max.x - 2.f, rect.Min.y + halfh - 1.f }, darker, 2.f);
                }
            }
            else
            {

            }

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
        HandleSpinnerEvent(id, extent, incbtn, decbtn, io, result);
        result.geometry = extent;
        return result;
    }

#pragma endregion

#pragma region Slider Widget

    ImRect SliderBounds(int32_t id, const ImRect& extent)
    {
        ImRect result;
        auto& context = GetContext();
        auto& state = context.GetState(id).state.slider;
        const auto& style = context.GetStyle(state.state);
        auto slidersz = std::max(Config.sliderSize, style.font.size);
        auto width = (style.specified & StyleWidth) ? style.dimension.x : 
            (state.dir == DIR_Horizontal ? extent.GetWidth() : slidersz);
        auto height = (style.specified & StyleHeight) ? style.dimension.y :
            (state.dir == DIR_Horizontal ? slidersz : extent.GetHeight());

        result.Min = extent.Min;
        result.Max = result.Min + ImVec2{ width, height };

        return result;
    }

    void HandleSliderEvent(int32_t id, const ImRect& extent, const ImRect& thumb, const IODescriptor& io, WidgetDrawResult& result)
    {
        auto& context = GetContext();
        
        if (!context.deferEvents)
        {
            auto& state = context.GetState(id).state.slider;
            const auto& style = context.GetStyle(state.state);
            const auto& specificStyle = context.sliderStyles[log2((unsigned)state.state)].top();
            auto center = thumb.Min + ImVec2{ thumb.GetWidth(), thumb.GetWidth() };
            auto width = extent.GetWidth(), height = extent.GetHeight();
            auto horizontal = width > height;
            auto radius = thumb.GetWidth();

            if (extent.Contains(io.mousepos))
            {
                auto offset = radius + specificStyle.thumbOffset + specificStyle.trackBorderThickness;
                auto inthumb = thumb.Contains(io.mousepos);
                state.state |= WS_Hovered;

                if (io.clicked() && !inthumb)
                {
                    auto space = horizontal ? width - (2.f * offset) : height - (2.f * offset);
                    auto where = horizontal ? io.mousepos.x - extent.Min.x - offset : io.mousepos.y - extent.Min.y - offset;
                    auto relative = where / space;
                    state.data = relative * (state.max - state.min);
                    state.state &= ~WS_Dragged;
                }
                else if (io.isLeftMouseDown() && ((state.state & WS_Dragged) || inthumb))
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
                else
                    state.state &= ~WS_Dragged;
            }
            else
            {
                state.state &= ~WS_Hovered;
                state.state &= ~WS_Dragged;
            }  
        }
        else context.deferedEvents.emplace_back(WT_Slider, id, extent, thumb);
    }

    WidgetDrawResult SliderImpl(int32_t id, SliderState& state, const ImRect& extent, IRenderer& renderer, const IODescriptor& io)
    {
        WidgetDrawResult result;
        auto& context = GetContext();
        const auto& style = context.GetStyle(state.state);
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

        if (style.border.isRounded())
            if ((style.border.cornerRadius[TopLeftCorner] >= radius) && (style.border.cornerRadius[TopRightCorner] >= radius) &&
                (style.border.cornerRadius[BottomRightCorner] >= radius) && (style.border.cornerRadius[BottomLeftCorner] >= radius))
                renderer.DrawCircle(center, radius, specificStyle.thumbColor, true);
            else
                renderer.DrawRoundedRect(center - ImVec2{ radius, radius }, center + ImVec2{ radius, radius }, specificStyle.thumbColor, true,
                    style.border.cornerRadius[TopLeftCorner], style.border.cornerRadius[TopRightCorner], style.border.cornerRadius[BottomRightCorner],
                    style.border.cornerRadius[BottomLeftCorner]);
        else
            renderer.DrawRect(center - ImVec2{ radius, radius }, center + ImVec2{ radius, radius }, specificStyle.thumbColor, true);

        ImRect thumb{ center - ImVec2{ radius, radius }, center + ImVec2{ radius, radius } };
        HandleSliderEvent(id, extent, thumb,  io, result);
        result.geometry = extent;
        return result;
    }

#pragma endregion

#pragma region TextInput

    static void UpdatePosition(const TextInputState& state, int index, InputTextInternalState& input, const StyleDescriptor& style, IRenderer& renderer)
    {
        for (auto idx = index; idx < (int)state.text.size(); ++idx)
        {
            auto sz = renderer.GetTextSize(std::string_view{ state.text.data() + idx, 1 }, style.font.font, style.font.size).x;
            input.pixelpos[idx] = sz + (idx > 0 ? input.pixelpos[idx - 1] : 0.f);
        }
    }

    static void RemoveCharAt(int position, TextInputState& state, InputTextInternalState& input)
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

        input.scroll.pos.x = std::max(0.f, input.scroll.pos.x - diff);
        state.text.pop_back();
        input.pixelpos.pop_back();
    }

    static void DeleteSelectedText(TextInputState& state, InputTextInternalState& input, const StyleDescriptor& style, IRenderer& renderer)
    {
        auto from = std::max(state.selection.first, state.selection.second) + 1,
            to = std::min(state.selection.first, state.selection.second);
        float shift = input.pixelpos[from - 1] - input.pixelpos[to - 1];
        auto textsz = (int)state.text.size();

        auto& op = input.ops.push();
        auto selectionsz = std::min(state.selection.second - state.selection.first + 1, 127);
        op.type = TextOpType::Deletion;
        op.range = std::make_pair(from, selectionsz);
        op.caretpos = input.caretpos;
        memcpy(op.opmem, state.text.data() + to, selectionsz);
        op.opmem[selectionsz] = 0;

        for (auto idx = 0; from < textsz; ++from, ++to, ++idx)
        {
            state.text[to] = state.text[from];
            input.pixelpos[to] = input.pixelpos[from] - shift;
        }

        for (auto idx = to; idx < textsz; ++idx)
        {
            state.text.pop_back();
            input.pixelpos.pop_back();
        }

        input.scroll.pos.x = std::max(0.f, input.scroll.pos.x - shift);
        input.caretpos = std::min(state.selection.first, state.selection.second);
        state.selection.first = state.selection.second = -1;
        input.selectionStart = -1.f;
    }

    void HandleTextInputEvent(int32_t id, const ImRect& content, const IODescriptor& io,
        IRenderer& renderer, WidgetDrawResult& result)
    {
        auto& context = GetContext();

        if (!context.deferEvents)
        {
            auto& state = context.GetState(id).state.input;
            auto& input = context.InputTextState(id);
            auto& style = context.GetStyle(state.state);

            auto mousepos = io.mousepos;
            auto mouseover = content.Contains(mousepos) || (state.state & WS_Pressed);
            auto ispressed = mouseover && io.isLeftMouseDown();
            auto hasclick = io.clicked();
            auto isclicked = (hasclick && mouseover) || (!hasclick && (state.state & WS_Focused));
            Config.platform->SetMouseCursor(mouseover ? MouseCursor::TextInput : MouseCursor::Arrow);
            mouseover ? state.state |= WS_Hovered : state.state &= ~WS_Hovered;
            ispressed ? state.state |= WS_Pressed : state.state &= ~WS_Pressed;
            isclicked ? state.state |= WS_Focused : state.state &= ~WS_Focused;
            if (input.lastClickTime != -1.f) input.lastClickTime += io.deltaTime;

            // When mouse is down inside the input, text selection is in progress
            // Hence, either text selection is starting, in which case record the start position.
            // If mouse gets released at the same position, it is a click and not a selection,
            // in which case, move the caret to the respective char position.
            // If mouse gets dragged, select the region of text
            if (state.state & WS_Pressed)
            {
                if (!state.text.empty() && mousepos.y < (content.Max.y - (1.5f * 5.f)))
                {
                    auto posx = mousepos.x - content.Min.x;
                    if (input.selectionStart == -1.f) input.selectionStart = posx;
                    else if ((std::fabsf(input.selectionStart - posx) > 5.f) || input.isSelecting)
                    {
                        if (state.selection.first == -1)
                        {
                            auto it = std::lower_bound(input.pixelpos.begin(), input.pixelpos.end(), input.selectionStart + input.scroll.pos.x);
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

                        auto it = std::lower_bound(input.pixelpos.begin(), input.pixelpos.end(), posx + input.scroll.pos.x);

                        if (it != input.pixelpos.end())
                        {
                            auto idx = it - input.pixelpos.begin();
                            if (idx > 0 && (*it - posx) > 0.f) --idx;

                            auto prevpos = input.caretpos;
                            state.selection.second = it - input.pixelpos.begin();
                            input.caretpos = state.selection.second + 1;

                            if (state.selection.second > state.selection.first)
                            {
                                if ((prevpos < input.caretpos) && (input.pixelpos[input.caretpos - 1] - input.scroll.pos.x > content.GetWidth()))
                                {
                                    auto width = std::fabsf(input.pixelpos[input.caretpos - 1] - (input.caretpos > 1 ? input.pixelpos[input.caretpos - 2] : 0.f));
                                    input.moveRight(width);
                                }
                            }
                            else
                            {
                                if (prevpos > input.caretpos && (input.pixelpos[input.caretpos - 1] - input.scroll.pos.x < 0.f))
                                {
                                    auto width = std::fabsf(input.pixelpos[prevpos - 1] - (prevpos > 1 ? input.pixelpos[prevpos - 2] : 0.f));
                                    input.moveLeft(width);
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                if (!state.text.empty() && mousepos.y < (content.Max.y - (1.5f * 5.f)))
                {
                    auto posx = mousepos.x - content.Min.x;

                    // This means we have clicked, not selecting text
                    if (std::fabsf(input.selectionStart - posx) < 5.f)
                    {
                        auto it = std::lower_bound(input.pixelpos.begin(), input.pixelpos.end(), posx + input.scroll.pos.x);
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
                            auto it = std::lower_bound(input.pixelpos.begin(), input.pixelpos.end(), input.selectionStart + input.scroll.pos.x);
                            if (it != input.pixelpos.end())
                            {
                                auto idx = it - input.pixelpos.begin();
                                if (idx > 0 && (*it - posx) > 0.f) --idx;
                                state.selection.first = it - input.pixelpos.begin();
                                input.isSelecting = true;
                                input.caretVisible = false;
                            }
                        }

                        auto it = std::lower_bound(input.pixelpos.begin(), input.pixelpos.end(), posx + input.scroll.pos.x);

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

                            if (prevpos > input.caretpos && (input.pixelpos[input.caretpos - 1] - input.scroll.pos.x < 0.f))
                            {
                                auto width = std::fabsf(input.pixelpos[prevpos - 1] - (prevpos > 1 ? input.pixelpos[prevpos - 2] : 0.f));
                                input.moveLeft(width);
                            }
                        }
                        else if (key == Key_RightArrow)
                        {
                            auto prevpos = input.caretpos;

                            if (io.modifiers & ShiftKeyMod)
                            {
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

                            if ((prevpos < input.caretpos) && (input.pixelpos[input.caretpos - 1] - input.scroll.pos.x > content.GetWidth()))
                            {
                                auto width = std::fabsf(input.pixelpos[input.caretpos - 1] - (input.caretpos > 1 ? input.pixelpos[input.caretpos - 2] : 0.f));
                                input.moveRight(width);
                            }
                        }
                        else if (key == Key_CapsLock) input.capslock = !input.capslock;
                        else if (key == Key_Insert) input.replace = !input.replace;
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
                                    if (input.scroll.pos.x != 0.f)
                                    {
                                        auto width = input.pixelpos.back() - input.pixelpos[input.pixelpos.size() - 2];
                                        input.moveLeft(width);
                                    }

                                    state.text.pop_back();
                                    input.pixelpos.pop_back();
                                }
                                else RemoveCharAt(input.caretpos, state, input);

                                input.caretpos--;
                            }
                            else DeleteSelectedText(state, input, style, renderer);

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
                            else DeleteSelectedText(state, input, style, renderer);

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
                                DeleteSelectedText(state, input, style, renderer);
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
                                ch = input.capslock ? std::toupper(ch) : std::tolower(ch);
                                auto caretAtEnd = input.caretpos == (int)text.size();

                                if (caretAtEnd)
                                {
                                    state.text.push_back(ch);
                                    std::string_view newtext{ state.text.data(), state.text.size() };
                                    auto lastpos = (int)state.text.size() - 1;
                                    auto width = renderer.GetTextSize(newtext.substr(lastpos, 1), style.font.font, style.font.size).x;
                                    auto nextw = width + (lastpos > 0 ? input.pixelpos[lastpos - 1] : 0.f);
                                    input.pixelpos.push_back(nextw);
                                    input.scroll.pos.x = std::max(0.f, input.pixelpos.back() - content.GetWidth());
                                }
                                else
                                {
                                    if (!input.replace)
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
                }
                else input.caretVisible = false;
            }

            if (result.event == WidgetEvent::Edited && state.ShowList != nullptr)
            {
                context.ToggleDeferedRendering(true);
                state.ShowList(state, { content.GetWidth(), content.GetHeight() });

                if (context.deferedRenderer->size.y > 0.f && !io.isKeyPressed(Key_Escape))
                {
                    char buffer[32];
                    memset(buffer, 0, 32);
                    ImVec2 origin{ content.Min.x, content.Max.y }, size{ content.GetWidth(), context.deferedRenderer->size.y };

                    if ((origin + size) > context.WindowSize())
                        origin.y = origin.y - size.y;

                    std::to_chars(buffer, buffer + 31, id);
                    ImGui::SetNextWindowPos(origin);
                    ImGui::SetNextWindowSize(size);

                    if (ImGui::Begin(buffer, nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
                    {
                        renderer.SetClipRect(origin, origin + size, false);
                        context.deferedRenderer->Render(renderer, origin);
                        renderer.ResetClipRect();
                    }

                    ImGui::End();
                }

                context.ToggleDeferedRendering(false);
            }

            if (!state.text.empty()) HandleHScroll(input.scroll, input.pixelpos.back(), content, mousepos, renderer, io, 5.f, false);
        }
        else context.deferedEvents.emplace_back(WT_TextInput, id, content);
    }

    // TODO: capslock and insert state is global -> poll and find out...
    WidgetDrawResult TextInputImpl(int32_t id, TextInputState& state, const ImRect& extent, const ImRect& content, 
        IRenderer& renderer, const IODescriptor& io)
    {
        WidgetDrawResult result;
        auto& context = GetContext();
        const auto& style = context.GetStyle(state.state);
        auto& input = context.InputTextState(id);

        if (state.state & WS_Focused)
            renderer.DrawRect(extent.Min, extent.Max, Config.focuscolor, false, 2.f);

        DrawBackground(extent.Min, extent.Max, style, renderer);
        DrawBorderRect(extent.Min, extent.Max, style.border, style.bgcolor, renderer);
        renderer.SetCurrentFont(style.font.font, style.font.size);

        if (state.text.empty() && !(state.state & WS_Focused))
        {
            auto phstyle = style;
            auto [fr, fg, fb, fa] = DecomposeColor(phstyle.fgcolor);
            fa = 150;
            phstyle.fgcolor = ToRGBA(fr, fg, fb, fa);
            DrawText(content.Min, content.Max, { content.Min, content.Min }, state.placeholder, state.state & WS_Disabled,
                phstyle, renderer, FontStyleOverflowMarquee);
        }
        else
        {
            if (state.selection.second != -1)
            {
                std::string_view text{ state.text.data(), state.text.size() };
                auto selection = state.selection;
                selection = { std::min(state.selection.first, state.selection.second),
                    std::max(state.selection.first, state.selection.second) };
                std::string_view parts[3] = { text.substr(0, selection.first),
                    text.substr(selection.first, selection.second - selection.first + 1),
                    text.substr(selection.second + 1) };
                ImVec2 startpos{ content.Min.x - input.scroll.pos.x, content.Min.y }, textsz;

                if (!parts[0].empty())
                {
                    textsz = renderer.GetTextSize(parts[0], style.font.font, style.font.size);
                    renderer.DrawText(parts[0], startpos, style.fgcolor);
                    startpos.x += textsz.x;
                }

                const auto& selstyle = context.GetStyle(WS_Selected);
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
                ImVec2 startpos{ content.Min.x - input.scroll.pos.x, content.Min.y };
                std::string_view text{ state.text.data(), state.text.size() };
                renderer.DrawText(text, startpos, style.fgcolor);
            }
        }

        if ((state.state & WS_Focused) && input.caretVisible)
        {
            auto isCaretAtEnd = input.caretpos == (int)state.text.size();
            auto offset = isCaretAtEnd && (input.scroll.pos.x == 0.f) ? 1.f : 0.f;
            auto cursorxpos = (!input.pixelpos.empty() ? input.pixelpos[input.caretpos - 1] - input.scroll.pos.x : 0.f) + offset;
            renderer.DrawLine(content.Min + ImVec2{ cursorxpos, 1.f }, content.Min + ImVec2{ cursorxpos, content.GetHeight() - 1.f }, style.fgcolor, 2.f);
        }

        HandleTextInputEvent(id, content, io, renderer, result);
        renderer.ResetFont();

        result.geometry = extent;
        return result;
    }

#pragma endregion

#pragma region DropDown

    static std::pair<ImRect, ImRect> DropDownBounds(int32_t id, DropDownState& state, const ImRect& content, IRenderer& renderer)
    {
        auto& context = GetContext();
        auto& style = context.GetStyle(state.state);
        auto size = renderer.GetTextSize(state.text, style.font.font, style.font.size);
        ImRect bounds = content;

        if (bounds.GetWidth() < size.x + style.font.size)
            bounds.Max.x = bounds.Min.x + size.x + style.font.size;

        return { bounds, { content.Min, content.Min + size } };
    }

    void HandleDropDownEvent(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const IODescriptor& io, IRenderer& renderer, WidgetDrawResult& result)
    {
        auto& context = GetContext();

        if (!context.deferEvents)
        {
            auto& state = context.GetState(id).state.dropdown;
            auto ismouseover = padding.Contains(io.mousepos);
            state.state = !ismouseover ? WS_Default :
                io.isLeftMouseDown() ? WS_Pressed | WS_Hovered : WS_Hovered;
            if (ismouseover && io.clicked())
            {
                result.event = WidgetEvent::Clicked; state.opened = !state.opened;
            }
            else if (ismouseover && io.isLeftMouseDoubleClicked())
            {
                if (state.isComboBox)
                {
                    if (state.inputId == -1)
                    {
                        state.inputId = GetNextId(WT_TextInput);
                        auto& text = GetWidgetState(state.inputId).state.input.text;
                        for (auto ch : state.text) text.push_back(ch);
                    }

                    auto res = TextInputImpl(state.inputId, state.input, margin, content, renderer, io);
                    if (res.event == WidgetEvent::Edited)
                        state.text = std::string_view{ state.input.text.data(), state.input.text.size() };
                    state.opened = true;
                }
            }
            else if (ismouseover && !state.tooltip.empty() && !io.isLeftMouseDown())
                ShowTooltip(state._hoverDuration, margin, margin.Min, state.tooltip, io, renderer);
            else state._hoverDuration == 0;

            if (state.opened)
            {
                ImRect maxrect{ { 0.f, 0.f }, context.WindowSize() };
                auto maxw = maxrect.GetWidth(), maxh = maxrect.GetHeight();
                ImVec2 available1 = state.dir == DIR_Vertical ? ImVec2{ maxw - border.Min.x, maxh - padding.Max.y } :
                    ImVec2{ padding.Max.x, maxh };
                ImVec2 available2 = state.dir == DIR_Vertical ? ImVec2{ maxw - border.Min.x, maxh - padding.Min.y } :
                    ImVec2{ padding.Min.x, maxh };

                // Create the popup with its own context                
                if (StartPopUp(id, { border.Min.x, padding.Max.y }))
                {
                    if (state.ShowList)
                        state.ShowList(available1, available2, state);
                    else
                    {
                        auto& optstates = GetContext().parentContext->dropDownOptions[id & 0xffff];
                        if (optstates.empty()) optstates.reserve(state.options.size());
                        auto optidx = 0;

                        for (const auto& option : state.options)
                        {
                            if ((int)optstates.size() <= optidx)
                            {
                                auto& optstate = optstates.emplace_back();
                                switch (option.first)
                                {
                                case WT_Checkbox:
                                {
                                    optstate.first = GetNextId(WT_Checkbox);
                                    optstate.second = GetNextId(WT_Label);
                                    GetWidgetState(optstate.first).state.checkbox.check = state.selected == optidx ?
                                        CheckState::Checked : CheckState::Unchecked;
                                    GetWidgetState(optstate.second).state.label.text = option.second;
                                    break;
                                }
                                case WT_ToggleButton:
                                {
                                    optstate.first = GetNextId(WT_ToggleButton);
                                    optstate.second = GetNextId(WT_Label);
                                    GetWidgetState(optstate.first).state.toggle.checked = state.selected == optidx;
                                    GetWidgetState(optstate.second).state.label.text = option.second;
                                    break;
                                }
                                case WT_Invalid:
                                {
                                    optstate.first = -1;
                                    optstate.second = GetNextId(WT_Label);
                                    GetWidgetState(optstate.first).state.toggle.checked = state.selected == optidx;
                                    break;
                                }
                                default: break;
                                }
                            }

                            auto& optstate = optstates[optidx];
                            if (optstate.first != -1)
                            {
                                auto type = (WidgetType)(optstate.first >> 16);
                                Widget(optstate.first, type, ToBottomRight, {});
                                Move(FD_Horizontal);
                            }
                            Widget(optstate.second, WT_Label, ToBottomRight, {});
                            Move(FD_Vertical);
                            GetContext().adhocLayout.top().nextpos.x = 0.f;
                            optidx++;
                        }
                    }

                    EndPopUp();
                }
            }
            else context.activePopUpRegion = ImRect{};
        }
        else context.deferedEvents.emplace_back(WT_DropDown, id, margin, border, padding, content);
    }

    WidgetDrawResult DropDownImpl(int32_t id, DropDownState& state, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io)
    {
        WidgetDrawResult result;
        auto& context = GetContext();
        auto& style = context.GetStyle(state.state);

        LOG("Margin: (%f, %f, %f, %f) | Padding: (%f, %f, %f, %f)\n",
            style.margin.left, style.margin.top, style.margin.right, style.margin.bottom,
            style.padding.left, style.padding.top, style.padding.right, style.padding.bottom);

        DrawBoxShadow(border.Min, border.Max, style, renderer);
        DrawBackground(border.Min, border.Max, style, renderer);
        DrawBorderRect(border.Min, border.Max, style.border, style.bgcolor, renderer);

        if (!(state.opened && state.isComboBox))
            DrawText(content.Min, content.Max, text, state.text, state.state & WS_Disabled, style, renderer);

        auto arrowh = style.font.size * 0.33333f;
        auto arroww = style.font.size * 0.25f;
        renderer.DrawLine(ImVec2{ content.Max.x - style.font.size + arroww, content.Min.y + arrowh },
            ImVec2{ content.Max.x - (style.font.size * 0.5f), content.Min.y + (2.f * arrowh)}, style.fgcolor, 2.f);
        renderer.DrawLine(ImVec2{ content.Max.x - (style.font.size * 0.5f), content.Min.y + (2.f * arrowh) },
            ImVec2{ content.Max.x - arroww, content.Min.y + arrowh }, style.fgcolor, 2.f);
        HandleDropDownEvent(id, margin, border, padding, content, io, renderer, result);

        result.geometry = margin;
        return result;
    }

#pragma endregion

#pragma region TabBar

    WidgetDrawResult TabBarImpl(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io)
    {
        WidgetDrawResult result;
        auto& context = GetContext();
        auto& state = context.GetState(id).state.tab;
        auto& map = context.TabStates(id);

        if (state.horizontal)
        {
            for (auto vcol = 0; vcol < (int)state.tabs.size(); ++vcol)
            {
                if (map.map.vtol[vcol] == -1)
                {
                    map.map.vtol[vcol] = vcol;
                    map.map.ltov[vcol] = vcol;
                }

                auto col = map.map.vtol[vcol];
                auto& tab = state.tabs[col];
                auto& style = context.GetStyle(tab.state.state);

                auto ismouseover = padding.Contains(io.mousepos);
                tab.state.state = !ismouseover ? WS_Default :
                    io.isLeftMouseDown() ? WS_Pressed | WS_Hovered : WS_Hovered;

                DrawBoxShadow(border.Min, border.Max, style, renderer);
                DrawBackground(border.Min, border.Max, style, renderer);
                DrawBorderRect(border.Min, border.Max, style.border, style.bgcolor, renderer);
                DrawText(content.Min, content.Max, text, tab.state.text, tab.state.state & WS_Disabled, style, renderer);

                if (ismouseover && io.clicked())
                    result.event = WidgetEvent::Clicked;
                else if (ismouseover && !tab.state.tooltip.empty() && !io.isLeftMouseDown())
                    ShowTooltip(tab.state._hoverDuration, margin, margin.Min, tab.state.tooltip, io, renderer);
                else tab.state._hoverDuration == 0;

                result.geometry = margin;
                return result;
            }
        }
    }

#pragma endregion

#pragma region ItemGrid

    void ItemGridState::setColumnResizable(int16_t col, bool resizable)
    {
        setColumnProps(col, COL_Resizable, resizable);
    }

    void ItemGridState::setColumnProps(int16_t col, ColumnProperty prop, bool set)
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

    static bool UpdateSubHeadersResize(std::vector<std::vector<ItemGridState::ColumnConfig>>& headers, ItemGridInternalState& gridstate,
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

            while (startch < (int)headers[chlevel].size())
            {
                auto& hdr = headers[chlevel][startch];
                if (hdr.parent == parent)
                {
                    auto& props = gridstate.cols[chlevel][startch];
                    props.modified += hdiff;
                    startch++;
                }
                else break;
            }
        }

        return chcount > 0;
    }

    static void HandleColumnResize(std::vector<std::vector<ItemGridState::ColumnConfig>>& headers, ItemGridState::ColumnConfig& hdr,
        ItemGridInternalState& gridstate, ImVec2 mousepos, int level, int col, const IODescriptor& io)
    {
        if (gridstate.state != ItemGridCurrentState::Default && gridstate.state != ItemGridCurrentState::ResizingColumns) return;

        auto isMouseNearColDrag = IsBetween(mousepos.x, hdr.content.Min.x, hdr.content.Min.x, 5.f) &&
            IsBetween(mousepos.y, hdr.content.Min.y, hdr.content.Max.y);
        auto& evprop = gridstate.cols[level][col - 1];

        if (!evprop.mouseDown)
            Config.platform->SetMouseCursor(isMouseNearColDrag ? MouseCursor::Grab : MouseCursor::Arrow);

        if (io.isLeftMouseDown())
        {
            if (!evprop.mouseDown)
            {
                if (isMouseNearColDrag)
                {
                    evprop.mouseDown = true;
                    evprop.lastPos = mousepos;
                    Config.platform->SetMouseCursor(MouseCursor::ResiveHorizontal);
                    gridstate.state = ItemGridCurrentState::ResizingColumns;
                    LOG("Drag start at %f for column #%d\n", mousepos.x, col);
                }
            }
            else
            {
                ImRect extendRect{ evprop.lastPos, mousepos };
                evprop.modified += (mousepos.x - evprop.lastPos.x);
                evprop.lastPos = mousepos;
                UpdateSubHeadersResize(headers, gridstate, extendRect, col - 1, level + 1, true);
                Config.platform->SetMouseCursor(MouseCursor::ResiveHorizontal);
                gridstate.state = ItemGridCurrentState::ResizingColumns;
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
                LOG("Drag end at %f for column #%d\n", mousepos.x, col);
            }

            evprop.mouseDown = false;
            Config.platform->SetMouseCursor(MouseCursor::Arrow);
            gridstate.state = ItemGridCurrentState::Default;
        }
    }

    static void HandleColumnReorder(std::vector<std::vector<ItemGridState::ColumnConfig>>& headers, ItemGridInternalState& gridstate,
        ImVec2 mousepos, int level, int vcol, const IODescriptor& io)
    {
        if (gridstate.state != ItemGridCurrentState::Default && gridstate.state != ItemGridCurrentState::ReorderingColumns) return;

        auto col = gridstate.colmap[level].vtol[vcol];
        auto& hdr = headers[level][col];
        auto isMouseDown = io.isLeftMouseDown();
        ImRect moveTriggerRect{ hdr.content.Min + ImVec2{ 5.5f, 0.f }, hdr.content.Max - ImVec2{ 5.5f, 0.f } };

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
            gridstate.drag.column = movingcol;
            gridstate.drag.level = level;
            gridstate.state = ItemGridCurrentState::ReorderingColumns;
            ERROR("\nMarking column (v: %d, l: %d) as moving (%f -> %f)\n", vcol, lcol, mcol.content.Min.x, mcol.content.Max.x);
        }
        else if (isMouseDown && gridstate.drag.mouseDown && gridstate.drag.column == vcol && gridstate.drag.level == level)
        {
            // This implying we are dragging the column around
            auto diff = mousepos.x - gridstate.drag.lastPos.x;

            if (diff > 0.f && (int16_t)headers[level].size() > (vcol + 1))
            {
                auto ncol = gridstate.colmap[level].vtol[vcol + 1];
                auto& next = headers[level][ncol];

                if ((mousepos.x - gridstate.drag.startPos.x) >= next.content.GetWidth())
                    gridstate.swapColumns(vcol, vcol + 1, headers, level);
            }
            else if (diff < 0.f && (col - 1) >= 0)
            {
                auto& prev = headers[level][col - 1];

                if ((gridstate.drag.startPos.x - mousepos.x) >= prev.content.GetWidth())
                    gridstate.swapColumns(col - 1, col, headers, level);
            }

            gridstate.drag.lastPos = mousepos;
        }
        else if (!isMouseDown)
        {
            gridstate.drag = ItemGridInternalState::HeaderCellDragState{};
            gridstate.state = ItemGridCurrentState::Default;
        }
    }

    static void HandleScrollBars(ScrollBarState& scroll, IRenderer& renderer, ImVec2 mousepos, const ImRect& content,
        ImVec2 sz, const IODescriptor& io)
    {
        auto hasHScroll = HandleHScroll(scroll, sz.x, content, mousepos, renderer, io, Config.scrollbarSz);
        HandleVScroll(scroll, sz.y, content, mousepos, renderer, io, Config.scrollbarSz, hasHScroll);
    }

    void HandleItemGridEvent(int32_t id, const ImRect& padding, const ImRect& content,
        const IODescriptor& io, IRenderer& renderer, WidgetDrawResult& result)
    {
        auto& context = GetContext();

        if (!context.deferEvents)
        {
            auto& state = context.GetState(id).state.grid;
            auto& headers = state.config.headers;
            auto& gridstate = context.GridState(id);

            auto mousepos = io.mousepos;
            auto ismouseover = padding.Contains(mousepos);
            state.state = !ismouseover ? WS_Default :
                io.isLeftMouseDown() ? WS_Pressed | WS_Hovered : WS_Hovered;
            HandleScrollBars(gridstate.scroll, renderer, mousepos, content, gridstate.totalsz, io);

            // Reset header calculations for next frame
            for (auto level = 0; level < (int)headers.size(); ++level)
            {
                for (int16_t col = 0; col < (int16_t)headers[level].size(); ++col)
                {
                    auto& hdr = headers[level][col];
                    hdr.content = ImRect{};
                    hdr.textrect = ImRect{};
                }
            }

            if (ismouseover && io.clicked())
                result.event = WidgetEvent::Clicked;
            else if (ismouseover && io.isLeftMouseDoubleClicked())
                result.event = WidgetEvent::DoubleClicked;
        }
        else context.deferedEvents.emplace_back(WT_ItemGrid, id, padding, content);
    }

    static void AlignVCenter(float height, int level, ItemGridState& state, const StyleDescriptor& style)
    {
        auto& headers = state.config.headers;

        for (int16_t col = 0; col < (int16_t)headers[level].size(); ++col)
        {
            auto& hdr = headers[level][col];
            hdr.content.Max.y = height;

            auto vdiff = (height - hdr.textrect.GetHeight() - style.padding.v()) * 0.5f;
            if (vdiff > 0.f)
                hdr.textrect.TranslateY(vdiff);
        }
    }

    static ImRect DrawCells(ItemGridState& state, std::vector<std::vector<ItemGridState::ColumnConfig>>& headers, int32_t row, int16_t col,
        float miny, const StyleDescriptor& style, IRenderer& renderer)
    {
        auto& hdr = headers.back()[col];
        auto& model = state.cell(row, col, 0);
        auto itemcontent = hdr.content;
        auto& context = GetContext();
        itemcontent.Min.y = miny;

        switch (model.wtype)
        {
        case WT_Label:
        {
            auto& itemstyle = context.GetStyle(model.state.label.state);
            if (itemstyle.font.font == nullptr) itemstyle.font.font = GetFont(itemstyle.font.family,
                itemstyle.font.size, FT_Normal);

            auto textsz = renderer.GetTextSize(model.state.label.text, itemstyle.font.font,
                itemstyle.font.size, -1.f);
            itemcontent.Max.y = itemcontent.Min.y + textsz.y + style.padding.v();

            ImVec2 textstart{ itemcontent.Min.x + style.padding.left,
                itemcontent.Min.y + style.padding.top };

            if (textsz.x < itemcontent.GetWidth())
            {
                auto hdiff = (itemcontent.GetWidth() - textsz.x) * 0.5f;
                textstart.x += hdiff;
            }

            auto textend = itemcontent.Max - ImVec2{ style.padding.right, style.padding.bottom };
            DrawBackground(itemcontent.Min, itemcontent.Max, itemstyle, renderer);
            renderer.DrawRect(itemcontent.Min, itemcontent.Max, ToRGBA(100, 100, 100), false);
            DrawText(textstart, textend, { textstart, textstart + textsz }, model.state.label.text,
                model.state.label.state & WS_Disabled, itemstyle, renderer);
            break;
        }
        /*case WT_Custom:
        {
            renderer.SetClipRect(itemcontent.Min, itemcontent.Max);
            auto sz = model.state.CustomWidget(itemcontent.Min, itemcontent.Max);
            renderer.ResetClipRect();

            itemcontent.Max.y = itemcontent.Min.y + style.padding.v() + sz.y;
            break;
        }*/
        default:
            break;
        }

        return itemcontent;
    }

    WidgetDrawResult ItemGridImpl(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io)
    {
        WidgetDrawResult result;
        auto& context = GetContext();
        auto& state = context.GetState(id).state.grid;
        auto& style = context.GetStyle(state.state);
        auto& gridstate = context.GridState(id);
        auto mousepos = io.mousepos;

        DrawBoxShadow(border.Min, border.Max, style, renderer);
        DrawBackground(border.Min, border.Max, style, renderer);
        DrawBorderRect(border.Min, border.Max, style.border, style.bgcolor, renderer);

        auto& headers = state.config.headers;

        // Determine header geometry
        for (auto level = (int)headers.size() - 1; level >= 0; --level)
        {
            // If this is last level headers, it represents leaves, the upper levels
            // represent grouping of the headers, whose width is determined by the 
            // lower levels, hence start with bottom most levels.
            // We compute the local x-coordinate of each "header cell" and local
            // "text position" for text content. 
            // The y-coordinate will be assigned while actual rendering, which will 
            // start from highest level i.e. root to leaves.
            if (level == ((int)headers.size() - 1))
            {
                auto sum = 0.f;
                auto totalEx = 0;
                auto height = 0.f;

                for (int16_t vcol = 0; vcol < (int16_t)headers[level].size(); ++vcol)
                {
                    if (gridstate.cols[level].empty())
                        gridstate.cols[level].fill(ItemGridInternalState::HeaderCellResizeState{});

                    if (gridstate.colmap[level].vtol[vcol] == -1)
                    {
                        gridstate.colmap[level].ltov[vcol] = vcol;
                        gridstate.colmap[level].vtol[vcol] = vcol;
                    }

                    auto col = gridstate.colmap[level].vtol[vcol];
                    auto& hdr = headers[level][col];
                    if (style.font.font == nullptr) style.font.font = GetFont(style.font.family,
                        style.font.size, FT_Normal);

                    static char buffer[256];
                    memset(buffer, ' ', 255);
                    buffer[255] = 0;

                    auto colwidth = hdr.props & COL_WidthAbsolute ? (float)hdr.width :
                        renderer.GetTextSize(std::string_view{ buffer, buffer + hdr.width },
                            style.font.font, style.font.size).x;
                    auto hasWidth = colwidth > 0.f;
                    colwidth += gridstate.cols[level][col].modified;

                    auto wrap = (hdr.props & COL_WrapHeader) && hasWidth ? colwidth : -1.f;
                    auto textsz = renderer.GetTextSize(hdr.name, style.font.font, style.font.size, wrap);
                    hdr.content.Min.x = sum;
                    hdr.textrect.Min.x = hdr.content.Min.x + style.padding.left;
                    hdr.textrect.Max.x = hdr.textrect.Min.x + textsz.x;
                    hdr.textrect.Min.y = style.padding.top;
                    hdr.textrect.Max.y = textsz.y;
                    hdr.content.Max.x = hdr.content.Min.x + (hasWidth ? colwidth : colwidth +
                        textsz.x + style.padding.h());

                    if (hdr.content.GetWidth() - style.padding.h() > textsz.x)
                    {
                        auto hdiff = (hdr.content.GetWidth() - textsz.x - style.padding.h()) * 0.5f;
                        hdr.textrect.TranslateX(hdiff);
                    }

                    hdr.content.Max.y = textsz.y + style.padding.v();
                    sum += hdr.content.GetWidth();
                    height = std::max(height, hdr.content.GetHeight());

                    // If user has resized column, do not expand it
                    if ((hdr.props & COL_Expandable) && gridstate.cols[level][col].modified == 0.f) totalEx++;
                }

                if (totalEx > 0)
                {
                    auto extra = (content.GetWidth() - sum) / (float)totalEx;
                    sum = 0.f;

                    if (extra > 0.f)
                        for (int16_t col = 0; col < (int16_t)headers[level].size(); ++col)
                        {
                            auto& hdr = headers[level][col];
                            auto width = hdr.content.Min.x;
                            auto isColExpandable = ((hdr.props & COL_Expandable) &&
                                gridstate.cols[level][col].modified == 0.f);
                            hdr.content.Min.x = sum;
                            hdr.content.Max.x = hdr.content.Max.x +
                                isColExpandable ? extra : 0.f;
                            if (!(hdr.props & COL_WrapHeader) && hdr.textrect.GetWidth() <
                                (hdr.content.GetWidth() - style.padding.h()))
                            {
                                auto diff = (hdr.content.GetWidth() - style.padding.h() - hdr.textrect.GetWidth()) * 0.5f;
                                hdr.textrect.TranslateX(diff);
                            }
                            sum = hdr.content.Max.x;

                            hdr.content.Max.y = height;
                            auto vdiff = (height - hdr.textrect.GetHeight() - style.padding.v()) * 0.5f;
                            if (vdiff > 0.f)
                                hdr.textrect.TranslateY(vdiff);
                        }
                    else AlignVCenter(height, level, state, style);
                }
                else AlignVCenter(height, level, state, style);
            }
            else
            {
                float lastx = 0.f, height = 0.f;
                if (gridstate.cols[level].empty())
                    gridstate.cols[level].fill(ItemGridInternalState::HeaderCellResizeState{});

                for (int16_t vcol = 0; vcol < (int16_t)headers[level].size(); ++vcol)
                {
                    float sum = 0.f;
                    if (gridstate.colmap[level].vtol[vcol] == -1)
                    {
                        gridstate.colmap[level].ltov[vcol] = vcol;
                        gridstate.colmap[level].vtol[vcol] = vcol;
                    }

                    auto col = gridstate.colmap[level].vtol[vcol];
                    auto& hdr = headers[level][col];
                    hdr.content.Min.x = lastx;
                    if (style.font.font == nullptr) style.font.font = GetFont(style.font.family,
                        style.font.size, FT_Normal);

                    // Find sum of all descendant headers, which will be this headers width
                    for (int16_t chcol = 0; chcol < (int16_t)headers[level + 1].size(); ++chcol)
                        if (headers[level + 1][chcol].parent == col)
                            sum += headers[level + 1][chcol].content.GetWidth();

                    auto wrap = hdr.props & COL_WrapHeader ? sum : -1.f;
                    auto textsz = renderer.GetTextSize(hdr.name, style.font.font, style.font.size, wrap);

                    // The available horizontal size is fixed by sum of descendant widths
                    // The height of the current level of headers is decided by the max height across all
                    hdr.content.Max.x = hdr.content.Min.x + sum;
                    hdr.textrect.Min.x = hdr.content.Min.x + style.padding.left;
                    hdr.textrect.Min.y = hdr.content.Min.y + style.padding.top;
                    hdr.textrect.Max.x = hdr.textrect.Min.x + std::min(sum, textsz.x);

                    if (!(hdr.props & COL_WrapHeader) && textsz.x < (sum - style.padding.h()))
                    {
                        auto diff = (sum - style.padding.h() - textsz.x) * 0.5f;
                        hdr.textrect.TranslateX(diff);
                    }

                    hdr.textrect.Max.y = hdr.textrect.Min.y + textsz.y;
                    hdr.content.Max.y = hdr.content.Min.y + textsz.y + style.padding.v();
                    height = std::max(height, hdr.content.GetHeight());
                    lastx += sum;
                }

                AlignVCenter(height, level, state, style);
            }
        }

        // Draw Headers
        auto posy = content.Min.y;
        auto totalh = 0.f;
        auto width = headers.back().back().content.Max.x - headers.back().front().content.Min.x;
        auto hshift = -gridstate.scroll.pos.x;
        std::pair<int16_t, int16_t> movingColRange = { INT16_MAX, -1 }, nextMovingRange = { INT16_MAX, -1 };

        for (auto level = 0; level < (int)headers.size(); ++level)
        {
            auto maxh = 0.f, posx = content.Min.x + hshift;

            for (int16_t vcol = 0; vcol < (int16_t)headers[level].size(); ++vcol)
            {
                auto col = gridstate.colmap[level].vtol[vcol];
                auto& hdr = headers[level][col];
                auto isBeingMoved = gridstate.drag.column == vcol && gridstate.drag.level == level;

                if (isBeingMoved)
                {
                    auto movex = mousepos.x - gridstate.drag.startPos.x;
                    gridstate.drag.config = hdr;
                    auto& cfg = gridstate.drag.config;

                    cfg.content.TranslateY(posy);
                    cfg.textrect.TranslateY(posy);
                    cfg.content.TranslateX(posx + movex);
                    cfg.textrect.TranslateX(posx + movex);

                    maxh = std::max(maxh, hdr.content.GetHeight());
                    nextMovingRange = { col, col };
                }
                else if (hdr.parent >= movingColRange.first && hdr.parent <= movingColRange.second)
                {
                    // The parent column is being moved in this case, record the range of child columns mapped to that parent range
                    auto movex = mousepos.x - gridstate.drag.startPos.x;
                    auto& cfg = gridstate.drag.config;

                    hdr.content.TranslateY(posy);
                    hdr.textrect.TranslateY(posy);
                    hdr.content.TranslateX(posx + movex);
                    hdr.textrect.TranslateX(posx + movex);

                    maxh = std::max(maxh, hdr.content.GetHeight());
                    nextMovingRange.first = std::min(nextMovingRange.first, col);
                    nextMovingRange.second = std::max(nextMovingRange.second, col);
                }
                else
                {
                    hdr.content.TranslateY(posy);
                    hdr.textrect.TranslateY(posy);
                    hdr.content.TranslateX(posx);
                    hdr.textrect.TranslateX(posx);
                    renderer.DrawRect(hdr.content.Min, hdr.content.Max, ToRGBA(100, 100, 100), false);

                    ImVec2 textend{ hdr.content.Max - ImVec2{ style.padding.right, style.padding.bottom } };
                    DrawText(hdr.textrect.Min, textend, hdr.textrect, hdr.name, false, style, renderer);
                    maxh = std::max(maxh, hdr.content.GetHeight());
                }

                if (vcol > 0)
                {
                    auto prevcol = gridstate.colmap[level].vtol[vcol - 1];
                    if (headers[level][prevcol].props & COL_Resizable)
                        HandleColumnResize(headers, hdr, gridstate, mousepos, level, col, io);
                }

                if (hdr.props & COL_Moveable)
                    HandleColumnReorder(headers, gridstate, mousepos, level, vcol, io);
            }

            posy += maxh;
            totalh += maxh;
            movingColRange = nextMovingRange;
            nextMovingRange = { INT16_MAX, -1 };
        }

        float height = 0.f;

        if (!state.uniformRowHeights)
        {
            // As each cell's height may be different, capture cell heights first,
            // then determine row height as max cell height, align and draw contents
            //ImVec2 cellGeometries[128];
            //ItemGridState::CellData data[128];

            //for (auto row = 0; row < state.config.rows; ++row)
            //{
            //    auto height = 0.f;
            //    for (int16_t col = 0; col < (int16_t)headers.back().size(); ++col)
            //    {
            //        auto& hdr = headers.back()[col];
            //        data[col] = state.cell(row, col, 0);

            //        switch (data[col].wtype)
            //        {
            //        case WT_Label:
            //        {
            //            auto& itemstyle = context.GetStyle(data[col].state.label.state);
            //            if (itemstyle.font.font == nullptr) itemstyle.font.font = GetFont(itemstyle.font.family,
            //                itemstyle.font.size, FT_Normal);
            //            auto wrap = itemstyle.font.flags & FontStyleNoWrap ? -1.f : hdr.content.GetWidth();
            //            auto textsz = renderer.GetTextSize(data[col].state.label.text, itemstyle.font.font,
            //                itemstyle.font.size, wrap);
            //            cellGeometries[col] = textsz;
            //            height = std::max(height, textsz.y + style.padding.v());
            //            break;
            //        }
            //        default:
            //            break;
            //        }
            //    }

            //    for (int16_t col = 0; col < (int16_t)headers.back().size(); ++col)
            //    {
            //        auto& hdr = headers.back()[col];
            //        auto content = hdr.content;
            //        content.Max.y = height;
            //        ImVec2 textstart{ content.Min.x + style.padding.left,
            //            content.Min.y + style.padding.top };

            //        if (cellGeometries[col].y < height)
            //        {
            //            auto vdiff = (height - cellGeometries[col].y) * 0.5f;
            //            textstart.y += vdiff;
            //        }

            //        if (cellGeometries[col].x < content.GetWidth())
            //        {
            //            auto hdiff = (content.GetWidth() - cellGeometries[col].x) * 0.5f;
            //            textstart.x += hdiff;
            //        }

            //        auto textend = content.Max - ImVec2{ style.padding.right, style.padding.bottom };
            //        renderer.DrawRect(content.Min, content.Max, ToRGBA(100, 100, 100), false);
            //        
            //        switch (data[col].wtype)
            //        {
            //        case WT_Label:
            //        {
            //            const auto& itemstyle = context.GetStyle(data[col].state.label.state);
            //            DrawText(textstart, textend, { textstart, textstart + cellGeometries[col] }, data[col].state.label.text,
            //                data[col].state.label.state& WS_Disabled, itemstyle, renderer);
            //            break;
            //        }
            //        }

            //        if (content.Contains(mousepos))
            //        {
            //            result.col = col;
            //            result.row = row;
            //            // ... add depth
            //        }
            //    }

            //    posy += height;
            //}
        }
        else
        {
            // Populate column wise for uniform row heights
            ImVec2 startpos{};
            startpos.y = -gridstate.scroll.pos.y;
            renderer.SetClipRect(ImVec2{ content.Min.x, posy }, content.Max);
            auto maxlevel = (int)headers.size() - 1;

            for (int16_t vcol = 0; vcol < (int16_t)headers.back().size(); ++vcol)
            {
                height = 0.f;
                auto col = gridstate.colmap[maxlevel].vtol[vcol];

                for (auto row = 0; row < state.config.rows; ++row)
                {
                    if (col < movingColRange.first || col > movingColRange.second)
                    {
                        auto starty = height + posy + startpos.y;
                        auto itemrect = DrawCells(state, headers, row, col, starty, style, renderer);
                        height += itemrect.GetHeight();

                        if (itemrect.Contains(mousepos))
                        {
                            result.col = col;
                            result.row = row;
                            // ... add depth
                        }

                        //if (height + posy >= content.Max.y) break; 
                    }
                }
            }

            renderer.ResetClipRect();
        }

        totalh += height;

        if (gridstate.drag.column != -1 && gridstate.drag.level != -1)
        {
            const auto& cfg = gridstate.drag.config;
            LOG("Rendering column (v: %d) as moving (%f -> %f)\n", gridstate.drag.column, cfg.content.Min.x, cfg.content.Max.x);

            renderer.DrawRectGradient(cfg.content.Min + ImVec2{ -10.f, 0.f }, { cfg.content.Min.x, content.Max.y },
                ToRGBA(0, 0, 0, 0), ToRGBA(100, 100, 100), Direction::DIR_Horizontal);
            renderer.DrawRectGradient({ cfg.content.Max.x, cfg.content.Min.y }, ImVec2{ cfg.content.Max.x + 10.f, content.Max.y },
                ToRGBA(100, 100, 100), ToRGBA(0, 0, 0, 0), Direction::DIR_Horizontal);

            renderer.DrawRect(cfg.content.Min, cfg.content.Max, ToRGBA(100, 100, 100), false);
            ImVec2 textend{ cfg.content.Max - ImVec2{ style.padding.right, style.padding.bottom } };
            DrawText(cfg.textrect.Min, textend, cfg.textrect, cfg.name, false, style, renderer);

            auto level = gridstate.drag.level + 1;

            if (level < (int16_t)headers.size())
            {
                auto movex = mousepos.x - gridstate.drag.lastPos.x;
                auto lcol = gridstate.colmap[level - 1].vtol[gridstate.drag.column];
                std::pair<int16_t, int16_t> movingColRange = { lcol, lcol }, nextMovingRange = { INT16_MAX, -1 };

                for (; level < (int16_t)headers.size(); ++level)
                {
                    for (int16_t col = 0; col < (int16_t)headers[level].size(); ++col)
                    {
                        auto& hdr = headers[level][col];

                        if (hdr.parent >= movingColRange.first && hdr.parent <= movingColRange.second)
                        {
                            hdr.content.TranslateX(movex);
                            hdr.textrect.TranslateX(movex);

                            renderer.DrawRect(hdr.content.Min, hdr.content.Max, ToRGBA(100, 100, 100), false);
                            ImVec2 textend{ hdr.content.Max - ImVec2{ style.padding.right, style.padding.bottom } };
                            DrawText(hdr.textrect.Min, textend, hdr.textrect, hdr.name, false, style, renderer);

                            nextMovingRange.first = std::min(nextMovingRange.first, col);
                            nextMovingRange.second = std::max(nextMovingRange.second, col);
                        }
                    }

                    movingColRange = nextMovingRange;
                    nextMovingRange = { INT16_MAX, -1 };
                }
            }

            ImVec2 startpos{};
            startpos.y = -gridstate.scroll.pos.y;
            renderer.SetClipRect(ImVec2{ content.Min.x, posy }, content.Max);

            for (int16_t col = movingColRange.first; col <= movingColRange.second; ++col)
            {
                height = 0.f;

                for (auto row = 0; row < state.config.rows; ++row)
                {
                    if (col >= movingColRange.first && col <= movingColRange.second)
                    {
                        auto itemrect = DrawCells(state, headers, row, col, height + posy + startpos.y, style, renderer);
                        height += itemrect.GetHeight();

                        if (itemrect.Contains(mousepos))
                        {
                            result.col = col;
                            result.row = row;
                            // ... add depth
                        }
                    }
                }
            }
        }

        gridstate.totalsz.y = totalh;
        gridstate.totalsz.x = width;
        HandleItemGridEvent(id, padding, content, io, renderer, result);

        result.geometry = margin;
        return result;
    }

#pragma endregion

#pragma region Splitter

    void StartSplitterScrollableRegion(int32_t id, int32_t nextid, int32_t parentid, Direction dir, ScrollableRegion& region)
    {
        auto& context = GetContext();
        const auto& style = context.GetStyle(WS_Default);
        auto& renderer = context.usingDeferred ? *context.deferedRenderer : *Config.renderer;
        LayoutItemDescriptor layoutItem;
        NeighborWidgets neighbors;
        if (dir == DIR_Vertical) neighbors.bottom = nextid;
        else neighbors.right = nextid;

        AddExtent(layoutItem, style, neighbors);
        DrawBorderRect(layoutItem.border.Min, layoutItem.border.Max, style.border, style.bgcolor, renderer);

        region.viewport = layoutItem.content;
        region.enabled = { dir == DIR_Horizontal, dir == DIR_Vertical };
        renderer.SetClipRect(layoutItem.content.Min, layoutItem.content.Max);
        context.PushContainer(parentid, id);
    }

    static void EndSplitterScrollableRegion(ScrollableRegion& region, Direction dir, const ImRect& parent)
    {
        auto& context = GetContext();
        auto id = context.containerStack.top();
        auto& renderer = context.usingDeferred ? *context.deferedRenderer : *Config.renderer;
        renderer.ResetClipRect();

        auto hasHScroll = false;
        auto io = Config.platform->CurrentIO();
        auto mousepos = io.mousepos;
        if (region.viewport.Max.x < region.max.x && region.enabled.first)
        {
            hasHScroll = true;
            HandleHScroll(region.state, region.max.x - region.viewport.Min.x, region.viewport, mousepos, renderer, io,
                Config.scrollbarSz);
        }

        if (region.viewport.Max.y < region.max.y && region.enabled.second)
            HandleVScroll(region.state, region.max.y - region.viewport.Min.y, region.viewport, mousepos, renderer, io,
                Config.scrollbarSz, hasHScroll);

        context.PopContainer(id);

        auto& layout = context.adhocLayout.top();
        layout.nextpos = region.viewport.Max;
        dir == DIR_Horizontal ? layout.nextpos.y = parent.Min.y : layout.nextpos.x = parent.Min.x;
    }

    void StartSplitRegion(int32_t id, Direction dir, const std::initializer_list<SplitRegion>& splits,
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
        const auto& style = context.GetStyle(WS_Default);
        state.current = 0;

        auto& renderer = context.usingDeferred ? *context.deferedRenderer : *Config.renderer;
        renderer.SetClipRect(layoutItem.margin.Min, layoutItem.margin.Max);

        auto idx = 0;
        const auto width = el.extent.GetWidth(), height = el.extent.GetHeight();
        auto splittersz = el.dir == DIR_Vertical ? (style.specified & StyleHeight ? style.dimension.y : Config.splitterSize) :
            style.specified & StyleWidth ? style.dimension.x : Config.splitterSize;
        auto prev = el.extent.Min;

        for (const auto& split : splits)
        {
            auto regionEnd = prev + (el.dir == DIR_Vertical ? ImVec2{ width, state.spacing[idx].curr * height } :
                ImVec2{ state.spacing[idx].curr * width, height });

            if (state.spacing[idx].curr == -1.f)
            {
                state.spacing[idx].curr = split.initial;
                regionEnd = prev + (el.dir == DIR_Vertical ? ImVec2{ width, state.spacing[idx].curr * height } :
                    ImVec2{ state.spacing[idx].curr * width, height });
            }

            if (split.scrollable)
            {
                auto scid = GetNextId(WT_SplitterScrollRegion);
                auto& scroll = state.scrolldata[idx];
                scroll.viewport = ImRect{ prev, regionEnd };
                scroll.enabled = { el.dir == DIR_Horizontal, el.dir == DIR_Vertical };
                state.scrollids[idx] = scid;
                context.AddItemGeometry(scid, scroll.viewport, true);
            }
            else
                state.viewport[idx] = ImRect{ prev, regionEnd };

            state.spacing[idx].min = split.min;
            state.spacing[idx].max = split.max;
            prev = regionEnd;
            el.dir == DIR_Vertical ? prev.x = layoutItem.margin.Min.x : prev.y = layoutItem.margin.Min.y;
            el.dir == DIR_Vertical ? prev.y += splittersz : prev.x += splittersz;
            ++idx;
        }

        if (state.scrollids[state.current] == -1)
        {
            auto& layout = context.adhocLayout.top();
            layout.nextpos = state.viewport[state.current].Min;
            renderer.SetClipRect(state.viewport[state.current].Min, state.viewport[state.current].Max);
        }
        else
        {
            auto& scroll = state.scrolldata[0];
            StartSplitterScrollableRegion(state.scrollids[0], state.scrollids[1], el.id, el.dir, scroll);
        }
    }

    void NextSplitRegion()
    {
        auto& context = GetContext();
        auto& el = context.splitterStack.top();
        auto& state = context.SplitterState(el.id);
        auto io = Config.platform->CurrentIO();
        auto mousepos = io.mousepos;
        const auto& style = context.GetStyle(WS_Default);
        const auto width = el.extent.GetWidth(), height = el.extent.GetHeight();
        auto regionStart = el.extent.Min + (el.dir == DIR_Vertical ? ImVec2{ width, state.spacing[state.current].curr * height } :
            ImVec2{ state.spacing[state.current].curr * width, height });
        auto& renderer = context.usingDeferred ? *context.deferedRenderer : *Config.renderer;

        assert(state.current < GLIMMER_MAX_SPLITTER_REGIONS);

        auto splittersz = el.dir == DIR_Vertical ? (style.specified & StyleHeight ? style.dimension.y : Config.splitterSize) :
            style.specified & StyleWidth ? style.dimension.x : Config.splitterSize;
        auto scid = state.scrollids[state.current];
        if (scid != -1) EndSplitterScrollableRegion(state.scrolldata[state.current], el.dir, el.extent);
        else renderer.ResetClipRect();

        auto& layout = context.adhocLayout.top();
        auto nextpos = layout.nextpos;
        auto sz = (el.dir == DIR_Vertical ? ImVec2{ width, splittersz } :
            ImVec2{ splittersz, el.extent.GetHeight() });

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
                MouseCursor::ResiveHorizontal);
            auto isDrag = io.isLeftMouseDown();
            if (!state.isdragged[state.current]) state.isdragged[state.current] = true;
            state.states[state.current] = isDrag ? WS_Pressed | WS_Hovered : WS_Hovered;

            if (isDrag)
            {
                auto amount = el.dir == DIR_Vertical ? (mousepos.y - el.extent.Min.y) / height :
                    (mousepos.x - el.extent.Min.x) / width;
                auto prev = state.spacing[state.current].curr;
                state.spacing[state.current].curr = clamp(amount, state.spacing[state.current].min, state.spacing[state.current].max);
                auto diff = prev - state.spacing[state.current].curr;
                LOG("Moving to %f (relative amount: %f)\n", mousepos.y, state.spacing[state.current].curr);

                if (diff != 0.f)
                    state.spacing[state.current + 1].curr += diff;
            }
            else state.isdragged[state.current] = false;
        }
        else
        {
            state.states[state.current] = WS_Default;
            Config.platform->SetMouseCursor(MouseCursor::Arrow);
        }

        state.current++;
        assert(state.current < GLIMMER_MAX_SPLITTER_REGIONS);

        scid = state.scrollids[state.current];

        if (scid != -1)
        {
            auto& scroll = state.scrolldata[state.current];
            layout.nextpos = scroll.viewport.Min;
            layout.lastItemId = scid;

            if (state.current + 1 < GLIMMER_MAX_SPLITTER_REGIONS)
                StartSplitterScrollableRegion(scid, state.scrollids[state.current + 1], el.id, el.dir, scroll);
            else
                StartSplitterScrollableRegion(scid, el.id, el.id, el.dir, scroll);
        }
        else
        {
            layout.nextpos = state.viewport[state.current].Min;
            renderer.SetClipRect(state.viewport[state.current].Min, state.viewport[state.current].Max);
        }
    }

    void EndSplitRegion()
    {
        auto& context = GetContext();
        auto el = context.splitterStack.top();
        auto& state = context.SplitterState(el.id);
        auto& renderer = context.usingDeferred ? *context.deferedRenderer : *Config.renderer;

        auto scid = state.scrollids[state.current];
        if (scid != -1) EndSplitterScrollableRegion(state.scrolldata[state.current], el.dir, el.extent);
        else renderer.ResetClipRect();

        context.splitterStack.pop();
        context.AddItemGeometry(el.id, el.extent);
        renderer.ResetClipRect();
    }

#pragma endregion

#pragma region Popups

    bool StartPopUp(int32_t id, ImVec2 origin)
    {
        auto io = Config.platform->CurrentIO();
        if (!io.isKeyPressed(Key_Escape))
        {
            auto& overlayctx = PushContext(id);
            overlayctx.ToggleDeferedRendering(true);
            overlayctx.deferEvents = true;
            overlayctx.popupOrigin = origin;
            overlayctx.popupTarget = id;
            return true;
        }
        else
            GetContext().activePopUpRegion = ImRect{};

        return false;
    }

    WidgetDrawResult EndPopUp()
    {
        auto& overlayctx = GetContext();
        WidgetDrawResult result;

        if (overlayctx.deferedRenderer->size.y > 0.f)
        {
            auto& renderer = overlayctx.parentContext->usingDeferred ? *overlayctx.parentContext->deferedRenderer : *Config.renderer;
            ImVec2 origin = overlayctx.popupOrigin, size{ overlayctx.deferedRenderer->size };

            if ((origin + size) > overlayctx.WindowSize())
                origin.y = origin.y - size.y;

            if (renderer.StartOverlay(overlayctx.popupTarget, origin, size, ToRGBA(255, 255, 255)))
            {
                overlayctx.parentContext->activePopUpRegion = ImRect{ origin, origin + size };
                overlayctx.deferedRenderer->Render(renderer, origin);
                overlayctx.ToggleDeferedRendering(false);
                overlayctx.deferEvents = false;

                result = overlayctx.HandleEvents(origin);
                renderer.EndOverlay();
            }
        }

        PopContext();
        return result;
    }

#pragma endregion

    WindowConfig& GetWindowConfig()
    {
        return Config;
    }

    int32_t GetNextId(WidgetType type)
    {
        int32_t id = GetNextCount(type);
        id = id | (type << 16);
        return id;
    }

    int16_t GetNextCount(WidgetType type)
    {
        auto& context = GetContext();
        auto id = context.maxids[type]; 
        if (type == WT_SplitterScrollRegion) context.maxids[type]++;
        return id;
    }

    WidgetDrawResult Widget(int32_t id, WidgetType type, int32_t geometry, const NeighborWidgets& neighbors)
    {
        auto& context = GetContext();
        assert((id & 0xffff) <= (int)context.states[type].size());
        WidgetDrawResult result;

        auto& renderer = context.usingDeferred ? *context.deferedRenderer : *Config.renderer;
        auto& platform = *Config.platform;
        LayoutItemDescriptor layoutItem;
        layoutItem.wtype = type;
        layoutItem.id = id;
        layoutItem.sizing = geometry;

        auto wid = (type << 16) | id;
        auto maxxy = context.MaximumAbsExtent();
        auto io = Config.platform->CurrentIO();

        switch (type)
        {
        case WT_Label: {
            auto& state = context.GetState(wid).state.label;
            CopyStyle(context.GetStyle(WS_Default), context.GetStyle(state.state));
            auto& style = context.GetStyle(state.state);

            if (!context.layouts.empty())
            {
                auto& layout = context.layouts.top();
                auto pos = layout.geometry.Min;
                
                if (geometry & ExpandH) layoutItem.sizing |= ExpandH;
                if (geometry & ExpandV) layoutItem.sizing |= ExpandV;
                std::tie(layoutItem.content, layoutItem.padding, layoutItem.border, layoutItem.margin, layoutItem.text) = GetBoxModelBounds(pos,
                    style, state.text, renderer, ToBottom | ToRight, state.type, neighbors, maxxy.x, maxxy.y);
                AddItemToLayout(layout, layoutItem);
            }
            else
            {
                std::tie(layoutItem.content, layoutItem.padding, layoutItem.border, layoutItem.margin, layoutItem.text) = GetBoxModelBounds(
                    context.NextAdHocPos(), style, state.text, renderer, geometry, state.type, neighbors, maxxy.x, maxxy.y);
                context.AddItemGeometry(wid, layoutItem.margin);
                auto flags = ToTextFlags(state.type);
                result = LabelImpl(wid, layoutItem.margin, layoutItem.border, layoutItem.padding, layoutItem.content, layoutItem.text, renderer, io, flags);
            }
            break;
        }
        case WT_Button: {
            auto& state = context.GetState(wid).state.button;
            CopyStyle(context.GetStyle(WS_Default), context.GetStyle(state.state));
            auto& style = context.GetStyle(state.state);

            if (!context.layouts.empty())
            {
                auto& layout = context.layouts.top();
                auto pos = layout.geometry.Min;
                if (geometry & ExpandH) layoutItem.sizing |= ExpandH;
                if (geometry & ExpandV) layoutItem.sizing |= ExpandV;
                std::tie(layoutItem.content, layoutItem.padding, layoutItem.border, layoutItem.margin, layoutItem.text) = GetBoxModelBounds(pos,
                    style, state.text, renderer, ToBottom | ToRight, state.type, neighbors, maxxy.x, maxxy.y);
                AddItemToLayout(layout, layoutItem);
            }
            else
            {
                std::tie(layoutItem.content, layoutItem.padding, layoutItem.border, layoutItem.margin, layoutItem.text) = GetBoxModelBounds(
                    context.NextAdHocPos(), style, state.text, renderer, geometry, state.type, neighbors, maxxy.x, maxxy.y);
                context.AddItemGeometry(wid, layoutItem.margin);
                result = ButtonImpl(wid, layoutItem.margin, layoutItem.border, layoutItem.padding, layoutItem.content, layoutItem.text, renderer, io);
            }
            break;
        }
        case WT_RadioButton: {
            auto& state = context.GetState(wid).state.radio;
            auto& style = context.GetStyle(state.state);
            CopyStyle(context.GetStyle(WS_Default), style);
            AddExtent(layoutItem, style, neighbors, style.font.size, style.font.size);
            auto bounds = RadioButtonBounds(state, layoutItem.margin);

            if (!context.layouts.empty())
            {
                auto& layout = context.layouts.top();
                auto pos = layout.geometry.Min;
                //if (geometry & ExpandH) layoutItem.sizing |= ExpandH;
                //if (geometry & ExpandV) layoutItem.sizing |= ExpandV;
                AddItemToLayout(layout, layoutItem);
            }
            else
            {
                renderer.SetClipRect(layoutItem.margin.Min, layoutItem.margin.Max);
                result = RadioButtonImpl(wid, state, bounds, renderer, io);
                context.AddItemGeometry(wid, bounds);
                renderer.ResetClipRect();
            }
            break;
        }
        case WT_ToggleButton: {
            auto& state = context.GetState(wid).state.toggle;
            auto& style = context.GetStyle(state.state);
            CopyStyle(context.GetStyle(WS_Default), style);
            AddExtent(layoutItem, style, neighbors, style.font.size, style.font.size);
            auto [bounds, textsz] = ToggleButtonBounds(state, layoutItem.content, renderer);

            if (bounds.GetArea() != layoutItem.margin.GetArea())
                layoutItem.margin = layoutItem.border = layoutItem.padding = layoutItem.content = bounds;

            layoutItem.text.Min = layoutItem.margin.Min;
            layoutItem.text.Max = layoutItem.text.Min + textsz;

            if (!context.layouts.empty())
            {
                auto& layout = context.layouts.top();
                auto pos = layout.geometry.Min;
                //if (geometry & ExpandH) layoutItem.sizing |= ExpandH;
                //if (geometry & ExpandV) layoutItem.sizing |= ExpandV;
                AddItemToLayout(layout, layoutItem);
            }
            else
            {
                renderer.SetClipRect(bounds.Min, bounds.Max);
                result = ToggleButtonImpl(wid, state, bounds, textsz, renderer, io);
                context.AddItemGeometry(wid, bounds);
                renderer.ResetClipRect();
            }
            
            break;
        }
        case WT_Checkbox: {
            auto& state = context.GetState(wid).state.checkbox;
            auto& style = context.GetStyle(state.state);
            CopyStyle(context.GetStyle(WS_Default), style);
            AddExtent(layoutItem, style, neighbors, style.font.size, style.font.size);
            auto bounds = CheckboxBounds(state, layoutItem.margin);

            if (bounds.GetArea() != layoutItem.margin.GetArea())
            {
                layoutItem.margin = bounds;
                layoutItem.border.Min = layoutItem.margin.Min + ImVec2{ style.margin.left, style.margin.top };
                layoutItem.border.Max = layoutItem.margin.Max - ImVec2{ style.margin.right, style.margin.bottom };

                layoutItem.padding.Min = layoutItem.border.Min + ImVec2{ style.border.left.thickness, style.border.top.thickness };
                layoutItem.padding.Min = layoutItem.border.Max + ImVec2{ style.border.right.thickness, style.border.bottom.thickness };
            }

            if (!context.layouts.empty())
            {
                auto& layout = context.layouts.top();
                auto pos = layout.geometry.Min;
                //if (geometry & ExpandH) layoutItem.sizing |= ExpandH;
                //if (geometry & ExpandV) layoutItem.sizing |= ExpandV;
                AddItemToLayout(layout, layoutItem);
            }
            else
            {
                renderer.SetClipRect(layoutItem.margin.Min, layoutItem.margin.Max);
                result = CheckboxImpl(wid, state, layoutItem.margin, layoutItem.padding, renderer, io);
                context.AddItemGeometry(wid, bounds);
                renderer.ResetClipRect();
            }
            
            break;
        }
        case WT_Spinner: {
            auto& state = context.GetState(wid).state.spinner;
            auto& style = context.GetStyle(state.state);
            CopyStyle(context.GetStyle(WS_Default), style);
            AddExtent(layoutItem, style, neighbors, 0.f, style.font.size + style.margin.v() + style.border.v() + style.padding.v());
            auto bounds = SpinnerBounds(wid, state, renderer, layoutItem.padding);

            layoutItem.border.Max = bounds.Max + ImVec2{ style.border.right.thickness, style.border.bottom.thickness };
            layoutItem.margin.Max = layoutItem.border.Max + ImVec2{ style.margin.right, style.margin.bottom };
            layoutItem.content = layoutItem.padding = bounds;

            if (!context.layouts.empty())
            {
                auto& layout = context.layouts.top();
                auto pos = layout.geometry.Min;
                //if (geometry & ExpandH) layoutItem.sizing |= ExpandH;
                //if (geometry & ExpandV) layoutItem.sizing |= ExpandV;
                AddItemToLayout(layout, layoutItem);
            }
            else
            {
                renderer.SetClipRect(layoutItem.margin.Min, layoutItem.margin.Max);
                result = SpinnerImpl(wid, state, layoutItem.padding, io, renderer);
                context.AddItemGeometry(wid, bounds);
                renderer.ResetClipRect();
            }

            break;
        }
        case WT_Slider: {
            auto& state = context.GetState(wid).state.slider;
            auto& style = context.GetStyle(state.state);
            CopyStyle(context.GetStyle(WS_Default), style);
            AddExtent(layoutItem, style, neighbors, state.dir == DIR_Horizontal ? 0.f : style.font.size, 
                state.dir == DIR_Vertical ? 0.f : style.font.size);
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

            if (!context.layouts.empty())
            {
                auto& layout = context.layouts.top();
                auto pos = layout.geometry.Min;
                if ((geometry & ExpandH) && (state.dir == DIR_Horizontal)) layoutItem.sizing |= ExpandH;
                if ((geometry & ExpandV) && (state.dir == DIR_Vertical)) layoutItem.sizing |= ExpandV;
                AddItemToLayout(layout, layoutItem);
            }
            else
            {
                renderer.SetClipRect(layoutItem.margin.Min, layoutItem.margin.Max);
                result = SliderImpl(wid, state, layoutItem.border, renderer, io);
                context.AddItemGeometry(wid, bounds);
                renderer.ResetClipRect();
            }

            break;
        }
        case WT_TextInput: {
            auto& state = context.GetState(wid).state.input;
            auto& style = context.GetStyle(state.state);
            auto vdelta = style.margin.v() + style.padding.v() + style.border.v();
            CopyStyle(context.GetStyle(WS_Default), style);
            AddExtent(layoutItem, style, neighbors, 0.f, style.font.size + vdelta);

            if (!context.layouts.empty())
            {
                auto& layout = context.layouts.top();
                auto pos = layout.geometry.Min;
                if (geometry & ExpandH) layoutItem.sizing |= ExpandH;
                if (geometry & ExpandV) layoutItem.sizing |= ExpandV;
                AddItemToLayout(layout, layoutItem);
            }
            else
            {
                renderer.SetClipRect(layoutItem.margin.Min, layoutItem.margin.Max);
                result = TextInputImpl(wid, state, layoutItem.border, layoutItem.content, renderer, io);
                context.AddItemGeometry(wid, layoutItem.margin);
                renderer.ResetClipRect();
            }
            
            break;
        }
        case WT_DropDown: {
            auto& state = context.GetState(wid).state.dropdown;
            auto& style = context.GetStyle(state.state);
            auto vdelta = style.margin.v() + style.padding.v() + style.border.v();
            CopyStyle(context.GetStyle(WS_Default), style);
            AddExtent(layoutItem, style, neighbors, 0.f, style.font.size + vdelta);
            auto [bounds, textrect] = DropDownBounds(id, state, layoutItem.content, renderer);

            if (textrect.GetWidth() < layoutItem.content.GetWidth())
            {
                layoutItem.content.Max.x = layoutItem.content.Min.x + textrect.GetWidth() + style.font.size;
                layoutItem.padding.Max.x = layoutItem.content.Max.x + style.padding.right;
                layoutItem.border.Max.x = layoutItem.padding.Max.x + style.border.right.thickness;
                layoutItem.margin.Max.x = layoutItem.border.Max.x + style.margin.right;
            }

            if (!context.layouts.empty())
            {
                auto& layout = context.layouts.top();
                auto pos = layout.geometry.Min;
                if (geometry & ExpandH) layoutItem.sizing |= ExpandH;
                if (geometry & ExpandV) layoutItem.sizing |= ExpandV;
                AddItemToLayout(layout, layoutItem);
            }
            else
            {
                renderer.SetClipRect(layoutItem.margin.Min, layoutItem.margin.Max);
                result = DropDownImpl(wid, state, layoutItem.margin, layoutItem.border, layoutItem.padding, 
                    layoutItem.content, textrect, renderer, io);
                context.AddItemGeometry(wid, layoutItem.margin);
                renderer.ResetClipRect();
            }
            
            break;
        }
        case WT_TabBar: {
            auto& state = context.GetState(wid).state.grid;
            auto& style = context.GetStyle(state.state);
            CopyStyle(context.GetStyle(WS_Default), style);
            AddExtent(layoutItem, style, neighbors, 0.f, 0.f);

            if (!context.layouts.empty())
            {
                auto& layout = context.layouts.top();
                auto pos = layout.geometry.Min;
                if (geometry & ExpandH) layoutItem.sizing |= ExpandH;
                if (geometry & ExpandV) layoutItem.sizing |= ExpandV;
                AddItemToLayout(layout, layoutItem);
            }
            else
            {
                renderer.SetClipRect(layoutItem.margin.Min, layoutItem.margin.Max);
                // TODO: ...
                renderer.ResetClipRect();
            }
            
            break;
        }
        case WT_ItemGrid: {
            auto& state = context.GetState(wid).state.grid;
            auto& style = context.GetStyle(state.state);
            CopyStyle(context.GetStyle(WS_Default), style);
            AddExtent(layoutItem, style, neighbors);

            if (!context.layouts.empty())
            {
                auto& layout = context.layouts.top();
                auto pos = layout.geometry.Min;
                if (geometry & ExpandH) layoutItem.sizing |= ExpandH;
                if (geometry & ExpandV) layoutItem.sizing |= ExpandV;
                AddItemToLayout(layout, layoutItem);
            }
            else
            {
                renderer.SetClipRect(layoutItem.margin.Min, layoutItem.margin.Max);
                result = ItemGridImpl(wid, layoutItem.margin, layoutItem.border, layoutItem.padding, 
                    layoutItem.content, layoutItem.text, renderer, io);
                context.AddItemGeometry(wid, layoutItem.margin);
                renderer.ResetClipRect();
            }
            
            break;
        }
        default: break;
        }

        context.currStyleStates = 0;
        for (auto idx = 0; idx < WSI_Total; ++idx) context.currStyle[idx] = StyleDescriptor{};
        return result;
    }

    WidgetDrawResult Label(int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        return Widget(id, WT_Label, geometry, neighbors);
    }

    WidgetDrawResult Button(int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        return Widget(id, WT_Button, geometry, neighbors);
    }

    WidgetDrawResult ToggleButton(int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        return Widget(id, WT_ToggleButton, geometry, neighbors);
    }

    WidgetDrawResult RadioButton(int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        return Widget(id, WT_RadioButton, geometry, neighbors);
    }

    WidgetDrawResult Checkbox(int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        return Widget(id, WT_Checkbox, geometry, neighbors);
    }

    WidgetDrawResult Spinner(int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        return Widget(id, WT_Spinner, geometry, neighbors);
    }

    WidgetDrawResult Slider(int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        return Widget(id, WT_Slider, geometry, neighbors);
    }

    WidgetDrawResult TextInput(int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        return Widget(id, WT_TextInput, geometry, neighbors);
    }

    WidgetDrawResult DropDown(int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        return Widget(id, WT_DropDown, geometry, neighbors);
    }

    WidgetDrawResult TabBar(int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        return Widget(id, WT_TabBar, geometry, neighbors);
    }

    WidgetDrawResult ItemGrid(int32_t id, int32_t geometry, const NeighborWidgets& neighbors)
    {
        return Widget(id, WT_ItemGrid, geometry, neighbors);
    }

    WidgetStateData& GetWidgetState(WidgetType type, int16_t id)
    {
        auto& context = GetContext();
        int32_t wid = id;
        wid = wid | (type << 16);
        context.maxids[type]++;
        if (context.InsideFrame)
            context.tempids[type] = std::min(context.tempids[type], context.maxids[type]);

        auto& state = context.GetState(wid);
        return state;
    }

    WidgetStateData& GetWidgetState(int32_t id)
    {
        auto wtype = (WidgetType)(id >> 16);
        return GetWidgetState(wtype, (int16_t)(id & 0xffff));
    }
}

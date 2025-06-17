#include "draw.h"
#include "context.h"
#include "style.h"

namespace glimmer
{
    IntersectRects ComputeIntersectRects(ImRect rect, ImVec2 startpos, ImVec2 endpos)
    {
        IntersectRects res;

        res.intersects[0].Max.x = startpos.x;
        res.intersects[1].Max.y = startpos.y;
        res.intersects[2].Min.x = endpos.x;
        res.intersects[3].Min.y = endpos.y;

        if (rect.Min.x < startpos.x)
        {
            res.intersects[0].Min.x = res.intersects[1].Min.x = res.intersects[3].Min.x = rect.Min.x;
        }
        else
        {
            res.intersects[1].Min.x = res.intersects[3].Min.x = rect.Min.x;
            res.visibleRect[0] = false;
        }

        if (rect.Min.y < startpos.y)
        {
            res.intersects[0].Min.y = res.intersects[1].Min.y = res.intersects[2].Min.y = rect.Min.y;
        }
        else
        {
            res.intersects[0].Min.y = res.intersects[2].Min.y = rect.Min.y;
            res.visibleRect[1] = false;
        }

        if (rect.Max.x > endpos.x)
        {
            res.intersects[1].Max.x = res.intersects[2].Max.x = res.intersects[3].Max.x = rect.Max.x;
        }
        else
        {
            res.intersects[1].Max.x = res.intersects[3].Max.x = rect.Max.x;
            res.visibleRect[2] = false;
        }

        if (rect.Max.y > endpos.y)
        {
            res.intersects[0].Max.y = res.intersects[2].Max.y = res.intersects[3].Max.y = rect.Max.y;
        }
        else
        {
            res.intersects[0].Max.y = res.intersects[2].Max.y = rect.Max.y;
            res.visibleRect[3] = false;
        }

        return res;
    }

    RectBreakup ComputeRectBreakups(ImRect rect, float amount)
    {
        RectBreakup res;

        res.rects[0].Min = ImVec2{ rect.Min.x - amount, rect.Min.y };
        res.rects[0].Max = ImVec2{ rect.Min.x, rect.Max.y };

        res.rects[1].Min = ImVec2{ rect.Min.x, rect.Min.y - amount };
        res.rects[1].Max = ImVec2{ rect.Max.x, rect.Min.y };

        res.rects[2].Min = ImVec2{ rect.Max.x, rect.Min.y };
        res.rects[2].Max = ImVec2{ rect.Max.x + amount, rect.Max.y };

        res.rects[3].Min = ImVec2{ rect.Min.x, rect.Max.y };
        res.rects[3].Max = ImVec2{ rect.Max.x, rect.Max.y + amount };

        ImVec2 delta{ amount, amount };
        res.corners[0].Min = rect.Min - delta;
        res.corners[0].Max = rect.Min;
        res.corners[1].Min = ImVec2{ rect.Max.x, rect.Min.y - amount };
        res.corners[1].Max = res.corners[1].Min + delta;
        res.corners[2].Min = rect.Max;
        res.corners[2].Max = rect.Max + delta;
        res.corners[3].Min = ImVec2{ rect.Min.x - amount, rect.Max.y };
        res.corners[3].Max = res.corners[3].Min + delta;

        return res;
    }

    void DrawBorderRect(ImVec2 startpos, ImVec2 endpos, const FourSidedBorder& border, uint32_t bgcolor, IRenderer& renderer)
    {
        if (border.isUniform && border.top.thickness > 0.f && IsColorVisible(border.top.color) &&
            border.top.color != bgcolor)
        {
            if (!border.isRounded())
                renderer.DrawRect(startpos, endpos, border.top.color, false, border.top.thickness);
            else
                renderer.DrawRoundedRect(startpos, endpos, border.top.color, false,
                    border.cornerRadius[TopLeftCorner], border.cornerRadius[TopRightCorner],
                    border.cornerRadius[BottomRightCorner], border.cornerRadius[BottomLeftCorner],
                    border.top.thickness);
        }
        else
        {
            auto width = endpos.x - startpos.x, height = endpos.y - startpos.y;

            if (border.top.thickness > 0.f && border.top.color != bgcolor && IsColorVisible(border.top.color))
                renderer.DrawLine(startpos, startpos + ImVec2{ width, 0.f }, border.top.color, border.top.thickness);
            if (border.right.thickness > 0.f && border.right.color != bgcolor && IsColorVisible(border.right.color))
                renderer.DrawLine(startpos + ImVec2{ width - border.right.thickness, 0.f }, endpos -
                    ImVec2{ border.right.thickness, 0.f }, border.right.color, border.right.thickness);
            if (border.left.thickness > 0.f && border.left.color != bgcolor && IsColorVisible(border.left.color))
                renderer.DrawLine(startpos, startpos + ImVec2{ 0.f, height }, border.left.color, border.left.thickness);
            if (border.bottom.thickness > 0.f && border.bottom.color != bgcolor && IsColorVisible(border.bottom.color))
                renderer.DrawLine(startpos + ImVec2{ 0.f, height - border.bottom.thickness }, endpos -
                    ImVec2{ 0.f, border.bottom.thickness }, border.bottom.color, border.bottom.thickness);
        }
    }

    void DrawBoxShadow(ImVec2 startpos, ImVec2 endpos, const StyleDescriptor& style, IRenderer& renderer)
    {
        // In order to draw box-shadow, the following steps are used:
        // 1. Draw the underlying rectangle with shadow color, spread and offset.
        // 2. Decompose the blur region into 8 rects i.e. 4 for corners, 4 for the sides
        // 3. For each rect, determine the vertex color i.e. shadow color or transparent,
        //    and draw a rect gradient accordingly.
        if ((style.shadow.blur > 0.f || style.shadow.spread > 0.f || style.shadow.offset.x != 0.f ||
            style.shadow.offset.y != 0) && IsColorVisible(style.shadow.color))
        {
            ImRect rect{ startpos, endpos };
            rect.Expand(style.shadow.spread);
            rect.Translate(style.shadow.offset);

            if (style.shadow.blur > 0.f)
            {
                auto outercol = style.shadow.color & ~IM_COL32_A_MASK;
                auto brk = ComputeRectBreakups(rect, style.shadow.blur);

                // Sides: Left -> Top -> Right -> Bottom
                renderer.DrawRectGradient(brk.rects[0].Min, brk.rects[0].Max, outercol, style.shadow.color, DIR_Horizontal);
                renderer.DrawRectGradient(brk.rects[1].Min, brk.rects[1].Max, outercol, style.shadow.color, DIR_Vertical);
                renderer.DrawRectGradient(brk.rects[2].Min, brk.rects[2].Max, style.shadow.color, outercol, DIR_Horizontal);
                renderer.DrawRectGradient(brk.rects[3].Min, brk.rects[3].Max, style.shadow.color, outercol, DIR_Vertical);

                // Corners: Top-left -> Top-right -> Bottom-right -> Bottom-left
                switch (Config.shadowQuality)
                {
                case BoxShadowQuality::Balanced:
                {
                    ImVec2 center = brk.corners[0].Max;
                    auto radius = brk.corners[0].GetHeight() - 0.5f; // all corners of same size
                    ImVec2 offset{ radius / 2.f, radius / 2.f };
                    radius *= 1.75f;

                    renderer.SetClipRect(brk.corners[0].Min, brk.corners[0].Max);
                    renderer.DrawRadialGradient(center + offset, radius, style.shadow.color, outercol, 180, 270);
                    renderer.ResetClipRect();

                    center = ImVec2{ brk.corners[1].Min.x, brk.corners[1].Max.y };
                    renderer.SetClipRect(brk.corners[1].Min, brk.corners[1].Max);
                    renderer.DrawRadialGradient(center + ImVec2{ -offset.x, offset.y }, radius, style.shadow.color, outercol, 270, 360);
                    renderer.ResetClipRect();

                    center = brk.corners[2].Min;
                    renderer.SetClipRect(brk.corners[2].Min, brk.corners[2].Max);
                    renderer.DrawRadialGradient(center - offset, radius, style.shadow.color, outercol, 0, 90);
                    renderer.ResetClipRect();

                    center = ImVec2{ brk.corners[3].Max.x, brk.corners[3].Min.y };
                    renderer.SetClipRect(brk.corners[3].Min, brk.corners[3].Max);
                    renderer.DrawRadialGradient(center + ImVec2{ offset.x, -offset.y }, radius, style.shadow.color, outercol, 90, 180);
                    renderer.ResetClipRect();
                    break;
                }
                default:
                    break;
                }
            }

            rect.Expand(style.shadow.blur > 0.f ? 1.f : 0.f);
            if (!style.border.isRounded())
                renderer.DrawRect(rect.Min, rect.Max, style.shadow.color, true);
            else
                renderer.DrawRoundedRect(rect.Min, rect.Max, style.shadow.color, true,
                    style.border.cornerRadius[TopLeftCorner], style.border.cornerRadius[TopRightCorner],
                    style.border.cornerRadius[BottomRightCorner], style.border.cornerRadius[BottomLeftCorner]);

            auto diffcolor = Config.bgcolor - 1;
            auto border = style.border;
            border.setColor(Config.bgcolor);
            DrawBorderRect(startpos, endpos, style.border, diffcolor, renderer);
            if (!border.isRounded())
                renderer.DrawRect(startpos, endpos, Config.bgcolor, true);
            else
                renderer.DrawRoundedRect(startpos, endpos, Config.bgcolor, true,
                    style.border.cornerRadius[TopLeftCorner], style.border.cornerRadius[TopRightCorner],
                    style.border.cornerRadius[BottomRightCorner], style.border.cornerRadius[BottomLeftCorner]);
        }
    }

    void DrawBackground(ImVec2 startpos, ImVec2 endpos, const StyleDescriptor& style, IRenderer& renderer)
    {
        DrawBackground(startpos, endpos, style.bgcolor, style.gradient, style.border, renderer);
    }

    void DrawBackground(ImVec2 startpos, ImVec2 endpos, uint32_t bgcolor, const ColorGradient& gradient, const FourSidedBorder& border, IRenderer& renderer)
    {
        if (gradient.totalStops != 0)
            (gradient.dir == ImGuiDir_Down || gradient.dir == ImGuiDir_Left) ?
            DrawLinearGradient(startpos, endpos, gradient.angleDegrees, gradient.dir,
                std::begin(gradient.colorStops), std::begin(gradient.colorStops) + gradient.totalStops, border.isRounded(), renderer) :
            DrawLinearGradient(startpos, endpos, gradient.angleDegrees, gradient.dir,
                std::rbegin(gradient.colorStops), std::rbegin(gradient.colorStops) + gradient.totalStops, border.isRounded(), renderer);
        else if (IsColorVisible(bgcolor))
            if (!border.isRounded())
                renderer.DrawRect(startpos, endpos, bgcolor, true);
            else
                renderer.DrawRoundedRect(startpos, endpos, bgcolor, true,
                    border.cornerRadius[TopLeftCorner], border.cornerRadius[TopRightCorner],
                    border.cornerRadius[BottomRightCorner], border.cornerRadius[BottomLeftCorner]);
    }

    void DrawText(ImVec2 startpos, ImVec2 endpos, const ImRect& textrect, std::string_view text, bool disabled, 
        const StyleDescriptor& style, IRenderer& renderer, std::optional<int32_t> txtflags)
    {
        ImRect content{ startpos, endpos };
        renderer.SetClipRect(content.Min, content.Max);
        auto flags = txtflags.has_value() ? txtflags.value() : style.font.flags;

        if (flags & TextIsSVG)
        {
            auto sz = content.GetSize();
            sz.x = std::min(sz.x, sz.y);
            sz.y = sz.x;
            renderer.DrawSVG(content.Min, sz, style.bgcolor, text, false);
        }
        else if (flags & TextIsPlainText)
        {
            renderer.SetCurrentFont(style.font.font, style.font.size);
            if ((flags & FontStyleOverflowMarquee) != 0 &&
                !disabled && style.index.animation != InvalidIdx)
            {
                auto& context = GetContext();
                auto& animation = context.animations[style.index.animation];
                ImVec2 textsz = textrect.GetWidth() == 0.f ? renderer.GetTextSize(text, style.font.font, style.font.size) :
                    textrect.GetSize();

                if (textsz.x > content.GetWidth())
                {
                    animation.moveByPixel(1.f, content.GetWidth(), -textsz.x);
                    content.Min.x += animation.offset;
                    renderer.DrawText(text, content.Min, style.fgcolor, style.dimension.x > 0 ? style.dimension.x : -1.f);
                }
                else
                    renderer.DrawText(text, textrect.Min, style.fgcolor, textrect.GetWidth());
            }
            else if (flags & FontStyleOverflowEllipsis)
            {
                ImVec2 textsz = textrect.GetWidth() == 0.f ? renderer.GetTextSize(text, style.font.font, style.font.size) :
                    textrect.GetSize();

                if (textsz.x > content.GetWidth())
                {
                    float width = 0.f, available = content.GetWidth() - renderer.EllipsisWidth(style.font.font, style.font.size);

                    for (auto chidx = 0; chidx < (int)text.size(); ++chidx)
                    {
                        // TODO: This is only valid for ASCII, this should be fixed for UTF-8
                        auto w = renderer.GetTextSize(text.substr(chidx, 1), style.font.font, style.font.size).x;
                        if (width + w > available)
                        {
                            renderer.DrawText(text.substr(0, chidx), content.Min, style.fgcolor, -1.f);
                            renderer.DrawText("...", content.Min + ImVec2{ width, 0.f }, style.fgcolor, -1.f);
                            break;
                        }
                        width += w;
                    }
                }
                else renderer.DrawText(text, textrect.Min, style.fgcolor, textrect.GetWidth());
            }
            else
                renderer.DrawText(text, textrect.Min, style.fgcolor, textrect.GetWidth());

            renderer.ResetFont();
        }
        
        renderer.ResetClipRect();
    }

    void DrawSymbol(ImVec2 startpos, ImVec2 size, ImVec2 padding, SymbolIcon symbol, uint32_t outlineColor, 
        uint32_t fillColor, float thickness, IRenderer& renderer)
    {
        startpos += padding;
        size -= (padding + padding);

        switch (symbol)
        {
        case glimmer::SymbolIcon::DownArrow:
        {
            ImVec2 start{ startpos.x, startpos.y }, end{ startpos.x + (size.x * 0.5f), startpos.y + size.y };
            renderer.DrawLine(start, end, outlineColor, thickness);
            renderer.DrawLine(end, ImVec2{ startpos.x + size.x, startpos.y }, outlineColor, thickness);
            break;
        }
        case glimmer::SymbolIcon::UpArrow:
        {
            ImVec2 start{ startpos.x, startpos.y + size.y }, end{ startpos.x + (size.x * 0.5f), startpos.y };
            renderer.DrawLine(start, end, outlineColor, thickness);
            renderer.DrawLine(end, ImVec2{ startpos.x + size.x, startpos.y + size.y }, outlineColor, thickness);
            break;
        }
        case glimmer::SymbolIcon::DownTriangle:
        {
            ImVec2 pos1{ startpos.x, startpos.y };
            ImVec2 pos2{ startpos.x + size.x, startpos.y };
            ImVec2 pos3{ startpos.x + (0.5f * size.x), startpos.y + size.y };
            renderer.DrawTriangle(pos1, pos2, pos3, fillColor, true);
            if (thickness > 0.f) renderer.DrawTriangle(pos1, pos2, pos3, outlineColor, false, thickness);
            break;
        }
        case glimmer::SymbolIcon::UpTriangle:
        {
            ImVec2 pos1{ startpos.x, startpos.y + size.y };
            ImVec2 pos2{ startpos.x + (0.5f * size.x), startpos.y };
            ImVec2 pos3{ startpos.x + size.x, startpos.y + size.y };
            renderer.DrawTriangle(pos1, pos2, pos3, fillColor, true);
            if (thickness > 0.f) renderer.DrawTriangle(pos1, pos2, pos3, outlineColor, false, thickness);
            break;
        }
        case glimmer::SymbolIcon::RightTriangle:
        {
            ImVec2 pos1{ startpos.x, startpos.y };
            ImVec2 pos2{ startpos.x + size.x, startpos.y + (0.5f * size.y) };
            ImVec2 pos3{ startpos.x, startpos.y + size.y };
            renderer.DrawTriangle(pos1, pos2, pos3, fillColor, true);
            if (thickness > 0.f) renderer.DrawTriangle(pos1, pos2, pos3, outlineColor, false, thickness);
            break;
        }
        case glimmer::SymbolIcon::Plus:
        {
            auto halfw = size.x * 0.5f, halfh = size.y * 0.5f;
            renderer.DrawLine(ImVec2{ startpos.x + halfw, startpos.y },
                ImVec2{ startpos.x + halfw , startpos.y + size.y }, outlineColor, thickness);
            renderer.DrawLine(ImVec2{ startpos.x, startpos.y + halfh },
                ImVec2{ startpos.x + size.x, startpos.y + halfh }, outlineColor, thickness);
            break;
        }
        case glimmer::SymbolIcon::Minus:
        {
            auto halfh = size.y * 0.5f;
            renderer.DrawLine(ImVec2{ startpos.x, startpos.y + halfh },
                ImVec2{ startpos.x + size.x, startpos.y + halfh }, outlineColor, thickness);
            break;
        }
        case glimmer::SymbolIcon::Cross:
        {
            renderer.DrawLine(ImVec2{ startpos.x, startpos.y },
                ImVec2{ startpos.x + size.x, startpos.y + size.y }, outlineColor, thickness);
            renderer.DrawLine(ImVec2{ startpos.x + size.x, startpos.y },
                ImVec2{ startpos.x, startpos.y + size.y }, outlineColor, thickness);
            break;
        }
        default:
            break;
        }
    }
}
#include "renderer.h"

#include <cstdio>

#define _USE_MATH_DEFINES
#include <math.h>
#include <lunasvg.h>
#include <style.h>

#include <stb_image/stb_image.h>
#include "imgui_impl_opengl3_loader.h"
#undef min
#undef max
#undef DrawText

// TODO: Test out pluto svg
/*auto doc = plutosvg_document_load_from_data(buffer, csz, size.x, size.y, nullptr, nullptr);
assert(doc != nullptr);

plutovg_color_t col;
auto [r, g, b, a] = DecomposeColor(color);
plutovg_color_init_rgba8(&col, r, g, b, a);

auto surface = plutosvg_document_render_to_surface(doc, nullptr, -1, -1, &col, nullptr, nullptr);
auto pixels = plutovg_surface_get_data(surface);

auto stride = plutovg_surface_get_stride(surface);
auto width = plutovg_surface_get_width(surface), height = plutovg_surface_get_height(surface);
plutovg_convert_argb_to_rgba(pixels, pixels, width, height, stride);*/

namespace glimmer
{
    ImVec2 ImGuiMeasureText(std::string_view text, void* fontptr, float sz, float wrapWidth)
    {
        auto imfont = (ImFont*)fontptr;
        ImVec2 txtsz;

        if ((int)text.size() > 4 && wrapWidth == -1.f && IsFontMonospace(fontptr))
            txtsz = ImVec2{ (float)text.size() * imfont->IndexAdvanceX.front(), sz };
        else
        {
            ImGui::PushFont(imfont);
            txtsz = ImGui::CalcTextSize(text.data(), text.data() + text.size(), false, wrapWidth);
            ImGui::PopFont();
        }

        auto ratio = (sz / imfont->FontSize);
        txtsz.x *= ratio;
        txtsz.y *= ratio;
        return txtsz;
    }

#pragma region ImGui Renderer

    ImTextureID UploadImage(ImVec2 pos, ImVec2 size, unsigned char* pixels, ImDrawList& dl)
    {
        GLint last_texture;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);

        GLuint image_texture;
        glGenTextures(1, &image_texture);
        glBindTexture(GL_TEXTURE_2D, image_texture);

        // Setup filtering parameters for display
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Upload pixels into texture
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

        //dl.AddImage((ImTextureID)(intptr_t)image_texture, pos, pos + size);
        glBindTexture(GL_TEXTURE_2D, last_texture);
        return (ImTextureID)(intptr_t)image_texture;
    }

    struct ImGuiRenderer final : public IRenderer
    {
        ImGuiRenderer();

        void SetClipRect(ImVec2 startpos, ImVec2 endpos, bool intersect);
        void ResetClipRect();

        void DrawLine(ImVec2 startpos, ImVec2 endpos, uint32_t color, float thickness = 1.f);
        void DrawPolyline(ImVec2* points, int sz, uint32_t color, float thickness);
        void DrawTriangle(ImVec2 pos1, ImVec2 pos2, ImVec2 pos3, uint32_t color, bool filled, float thickness = 1.f);
        void DrawRect(ImVec2 startpos, ImVec2 endpos, uint32_t color, bool filled, float thickness = 1.f);
        void DrawRoundedRect(ImVec2 startpos, ImVec2 endpos, uint32_t color, bool filled, float topleftr, float toprightr, float bottomrightr, float bottomleftr, float thickness = 1.f);
        void DrawRectGradient(ImVec2 startpos, ImVec2 endpos, uint32_t colorfrom, uint32_t colorto, Direction dir);
        void DrawRoundedRectGradient(ImVec2 startpos, ImVec2 endpos, float topleftr, float toprightr, float bottomrightr, float bottomleftr,
            uint32_t colorfrom, uint32_t colorto, Direction dir);
        void DrawPolygon(ImVec2* points, int sz, uint32_t color, bool filled, float thickness = 1.f);
        void DrawPolyGradient(ImVec2* points, uint32_t* colors, int sz);
        void DrawCircle(ImVec2 center, float radius, uint32_t color, bool filled, float thickness = 1.f);
        void DrawSector(ImVec2 center, float radius, int start, int end, uint32_t color, bool filled, bool inverted, float thickness = 1.f);
        void DrawRadialGradient(ImVec2 center, float radius, uint32_t in, uint32_t out, int start, int end);

        bool SetCurrentFont(std::string_view family, float sz, FontType type) override;
        bool SetCurrentFont(void* fontptr, float sz) override;
        void ResetFont() override;
        [[nodiscard]] ImVec2 GetTextSize(std::string_view text, void* fontptr, float sz, float wrapWidth);
        void DrawText(std::string_view text, ImVec2 pos, uint32_t color, float wrapWidth = -1.f);
        void DrawTooltip(ImVec2 pos, std::string_view text);
        [[nodiscard]] float EllipsisWidth(void* fontptr, float sz) override;

        void DrawSVG(ImVec2 pos, ImVec2 size, uint32_t color, std::string_view content, bool fromFile) override;
        void DrawImage(ImVec2 pos, ImVec2 size, std::string_view file) override;

    private:

        void ConstructRoundedRect(ImVec2 startpos, ImVec2 endpos, float topleftr, float toprightr, float bottomrightr, float bottomleftr);

        struct ImageLookupKey
        {
            ImVec2 size;
            uint32_t color;
            std::string data;
            bool isFile;
        };

        float _currentFontSz = 0.f;
        std::vector<std::pair<ImageLookupKey, ImTextureID>> bitmaps;
    };

    ImGuiRenderer::ImGuiRenderer()
    {}

    void ImGuiRenderer::SetClipRect(ImVec2 startpos, ImVec2 endpos, bool intersect)
    {
        ImGui::PushClipRect(startpos, endpos, intersect);
    }

    void ImGuiRenderer::ResetClipRect()
    {
        ImGui::PopClipRect();
    }

    void ImGuiRenderer::DrawLine(ImVec2 startpos, ImVec2 endpos, uint32_t color, float thickness)
    {
        ((ImDrawList*)UserData)->AddLine(startpos, endpos, color, thickness);
    }

    void ImGuiRenderer::DrawPolyline(ImVec2* points, int sz, uint32_t color, float thickness)
    {
        ((ImDrawList*)UserData)->AddPolyline(points, sz, color, 0, thickness);
    }

    void ImGuiRenderer::DrawTriangle(ImVec2 pos1, ImVec2 pos2, ImVec2 pos3, uint32_t color, bool filled, float thickness)
    {
        filled ? ((ImDrawList*)UserData)->AddTriangleFilled(pos1, pos2, pos3, color) :
            ((ImDrawList*)UserData)->AddTriangle(pos1, pos2, pos3, color, thickness);
    }

    void ImGuiRenderer::DrawRect(ImVec2 startpos, ImVec2 endpos, uint32_t color, bool filled, float thickness)
    {
        if (thickness > 0.f || filled)
        {
            filled ? ((ImDrawList*)UserData)->AddRectFilled(startpos, endpos, color) :
                ((ImDrawList*)UserData)->AddRect(startpos, endpos, color, 0.f, 0, thickness);
        }
    }

    void ImGuiRenderer::DrawRoundedRect(ImVec2 startpos, ImVec2 endpos, uint32_t color, bool filled,
        float topleftr, float toprightr, float bottomrightr, float bottomleftr, float thickness)
    {
        auto isUniformRadius = (topleftr == toprightr && toprightr == bottomrightr && bottomrightr == bottomleftr) ||
            ((topleftr + toprightr + bottomrightr + bottomleftr) == 0.f);

        if (isUniformRadius)
        {
            auto drawflags = 0;

            if (topleftr > 0.f) drawflags |= ImDrawFlags_RoundCornersTopLeft;
            if (toprightr > 0.f) drawflags |= ImDrawFlags_RoundCornersTopRight;
            if (bottomrightr > 0.f) drawflags |= ImDrawFlags_RoundCornersBottomRight;
            if (bottomleftr > 0.f) drawflags |= ImDrawFlags_RoundCornersBottomLeft;

            filled ? ((ImDrawList*)UserData)->AddRectFilled(startpos, endpos, color, toprightr, drawflags) :
                ((ImDrawList*)UserData)->AddRect(startpos, endpos, color, toprightr, drawflags, thickness);
        }
        else
        {
            auto& dl = *((ImDrawList*)UserData);
            ConstructRoundedRect(startpos, endpos, topleftr, toprightr, bottomrightr, bottomleftr);
            filled ? dl.PathFillConvex(color) : dl.PathStroke(color, ImDrawFlags_Closed, thickness);
        }
    }

    void ImGuiRenderer::DrawRectGradient(ImVec2 startpos, ImVec2 endpos, uint32_t colorfrom, uint32_t colorto, Direction dir)
    {
        dir == DIR_Horizontal ? ((ImDrawList*)UserData)->AddRectFilledMultiColor(startpos, endpos, colorfrom, colorto, colorto, colorfrom) :
            ((ImDrawList*)UserData)->AddRectFilledMultiColor(startpos, endpos, colorfrom, colorfrom, colorto, colorto);
    }

    void ImGuiRenderer::DrawRoundedRectGradient(ImVec2 startpos, ImVec2 endpos, float topleftr, float toprightr, float bottomrightr, float bottomleftr, uint32_t colorfrom, uint32_t colorto, Direction dir)
    {
        auto& dl = *((ImDrawList*)UserData);
        ConstructRoundedRect(startpos, endpos, topleftr, toprightr, bottomrightr, bottomleftr);

        DrawPolyGradient(dl._Path.Data, NULL, dl._Path.Size);
    }

    void ImGuiRenderer::DrawCircle(ImVec2 center, float radius, uint32_t color, bool filled, float thickness)
    {
        filled ? ((ImDrawList*)UserData)->AddCircleFilled(center, radius, color) :
            ((ImDrawList*)UserData)->AddCircle(center, radius, color, 0, thickness);
    }

    void ImGuiRenderer::DrawSector(ImVec2 center, float radius, int start, int end, uint32_t color, bool filled, bool inverted, float thickness)
    {
        constexpr float DegToRad = M_PI / 180.f;

        if (inverted)
        {
            auto& dl = *((ImDrawList*)UserData);
            dl.PathClear();
            dl.PathArcTo(center, radius, DegToRad * (float)start, DegToRad * (float)end, 32);
            auto start = dl._Path.front(), end = dl._Path.back();

            ImVec2 exterior[4] = { { std::min(start.x, end.x), std::min(start.y, end.y) },
                { std::max(start.x, end.x), std::min(start.y, end.y) },
                { std::min(start.x, end.x), std::max(start.y, end.y) },
                { std::max(start.x, end.x), std::max(start.y, end.y) }
            };

            auto maxDistIdx = 0;
            float dist = 0.f;
            for (auto idx = 0; idx < 4; ++idx)
            {
                auto curr = sqrt((end.x - start.x) * (end.x - start.x) +
                    (end.y - start.y) * (end.y - start.y));
                if (curr > dist)
                {
                    dist = curr;
                    maxDistIdx = idx;
                }
            }

            dl.PathLineTo(exterior[maxDistIdx]);
            filled ? dl.PathFillConcave(color) : dl.PathStroke(color, ImDrawFlags_Closed, thickness);
        }
        else
        {
            auto& dl = *((ImDrawList*)UserData);
            dl.PathClear();
            dl.PathArcTo(center, radius, DegToRad * (float)start, DegToRad * (float)end, 32);
            dl.PathLineTo(center);
            filled ? dl.PathFillConcave(color) : dl.PathStroke(color, ImDrawFlags_Closed, thickness);
        }
    }

    void ImGuiRenderer::DrawPolygon(ImVec2* points, int sz, uint32_t color, bool filled, float thickness)
    {
        filled ? ((ImDrawList*)UserData)->AddConvexPolyFilled(points, sz, color) :
            ((ImDrawList*)UserData)->AddPolyline(points, sz, color, ImDrawFlags_Closed, thickness);
    }

    void ImGuiRenderer::DrawPolyGradient(ImVec2* points, uint32_t* colors, int sz)
    {
        auto drawList = ((ImDrawList*)UserData);
        const ImVec2 uv = drawList->_Data->TexUvWhitePixel;

        if (drawList->Flags & ImDrawListFlags_AntiAliasedFill)
        {
            // Anti-aliased Fill
            const float AA_SIZE = 1.0f;
            const int idx_count = (sz - 2) * 3 + sz * 6;
            const int vtx_count = (sz * 2);
            drawList->PrimReserve(idx_count, vtx_count);

            // Add indexes for fill
            unsigned int vtx_inner_idx = drawList->_VtxCurrentIdx;
            unsigned int vtx_outer_idx = drawList->_VtxCurrentIdx + 1;
            for (int i = 2; i < sz; i++)
            {
                drawList->_IdxWritePtr[0] = (ImDrawIdx)(vtx_inner_idx);
                drawList->_IdxWritePtr[1] = (ImDrawIdx)(vtx_inner_idx + ((i - 1) << 1));
                drawList->_IdxWritePtr[2] = (ImDrawIdx)(vtx_inner_idx + (i << 1));
                drawList->_IdxWritePtr += 3;
            }

            // Compute normals
            ImVec2* temp_normals = (ImVec2*)alloca(sz * sizeof(ImVec2));
            for (int i0 = sz - 1, i1 = 0; i1 < sz; i0 = i1++)
            {
                const ImVec2& p0 = points[i0];
                const ImVec2& p1 = points[i1];
                ImVec2 diff = p1 - p0;
                diff *= ImInvLength(diff, 1.0f);
                temp_normals[i0].x = diff.y;
                temp_normals[i0].y = -diff.x;
            }

            for (int i0 = sz - 1, i1 = 0; i1 < sz; i0 = i1++)
            {
                // Average normals
                const ImVec2& n0 = temp_normals[i0];
                const ImVec2& n1 = temp_normals[i1];
                ImVec2 dm = (n0 + n1) * 0.5f;
                float dmr2 = dm.x * dm.x + dm.y * dm.y;
                if (dmr2 > 0.000001f)
                {
                    float scale = 1.0f / dmr2;
                    if (scale > 100.0f) scale = 100.0f;
                    dm *= scale;
                }
                dm *= AA_SIZE * 0.5f;

                // Add vertices
                drawList->_VtxWritePtr[0].pos = (points[i1] - dm);
                drawList->_VtxWritePtr[0].uv = uv; drawList->_VtxWritePtr[0].col = colors[i1];        // Inner
                drawList->_VtxWritePtr[1].pos = (points[i1] + dm);
                drawList->_VtxWritePtr[1].uv = uv; drawList->_VtxWritePtr[1].col = colors[i1] & ~IM_COL32_A_MASK;  // Outer
                drawList->_VtxWritePtr += 2;

                // Add indexes for fringes
                drawList->_IdxWritePtr[0] = (ImDrawIdx)(vtx_inner_idx + (i1 << 1));
                drawList->_IdxWritePtr[1] = (ImDrawIdx)(vtx_inner_idx + (i0 << 1));
                drawList->_IdxWritePtr[2] = (ImDrawIdx)(vtx_outer_idx + (i0 << 1));
                drawList->_IdxWritePtr[3] = (ImDrawIdx)(vtx_outer_idx + (i0 << 1));
                drawList->_IdxWritePtr[4] = (ImDrawIdx)(vtx_outer_idx + (i1 << 1));
                drawList->_IdxWritePtr[5] = (ImDrawIdx)(vtx_inner_idx + (i1 << 1));
                drawList->_IdxWritePtr += 6;
            }

            drawList->_VtxCurrentIdx += (ImDrawIdx)vtx_count;
        }
        else
        {
            // Non Anti-aliased Fill
            const int idx_count = (sz - 2) * 3;
            const int vtx_count = sz;
            drawList->PrimReserve(idx_count, vtx_count);
            for (int i = 0; i < vtx_count; i++)
            {
                drawList->_VtxWritePtr[0].pos = points[i];
                drawList->_VtxWritePtr[0].uv = uv;
                drawList->_VtxWritePtr[0].col = colors[i];
                drawList->_VtxWritePtr++;
            }
            for (int i = 2; i < sz; i++)
            {
                drawList->_IdxWritePtr[0] = (ImDrawIdx)(drawList->_VtxCurrentIdx);
                drawList->_IdxWritePtr[1] = (ImDrawIdx)(drawList->_VtxCurrentIdx + i - 1);
                drawList->_IdxWritePtr[2] = (ImDrawIdx)(drawList->_VtxCurrentIdx + i);
                drawList->_IdxWritePtr += 3;
            }

            drawList->_VtxCurrentIdx += (ImDrawIdx)vtx_count;
        }

        drawList->_Path.Size = 0;
    }

    void ImGuiRenderer::DrawRadialGradient(ImVec2 center, float radius, uint32_t in, uint32_t out, int start, int end)
    {
        auto drawList = ((ImDrawList*)UserData);
        if (((in | out) & IM_COL32_A_MASK) == 0 || radius < 0.5f)
            return;
        auto startrad = ((float)M_PI / 180.f) * (float)start;
        auto endrad = ((float)M_PI / 180.f) * (float)end;

        // Use arc with 32 segment count
        drawList->PathArcTo(center, radius, startrad, endrad, 32);
        const int count = drawList->_Path.Size - 1;

        unsigned int vtx_base = drawList->_VtxCurrentIdx;
        drawList->PrimReserve(count * 3, count + 1);

        // Submit vertices
        const ImVec2 uv = drawList->_Data->TexUvWhitePixel;
        drawList->PrimWriteVtx(center, uv, in);
        for (int n = 0; n < count; n++)
            drawList->PrimWriteVtx(drawList->_Path[n], uv, out);

        // Submit a fan of triangles
        for (int n = 0; n < count; n++)
        {
            drawList->PrimWriteIdx((ImDrawIdx)(vtx_base));
            drawList->PrimWriteIdx((ImDrawIdx)(vtx_base + 1 + n));
            drawList->PrimWriteIdx((ImDrawIdx)(vtx_base + 1 + ((n + 1) % count)));
        }

        drawList->_Path.Size = 0;
    }

    bool ImGuiRenderer::SetCurrentFont(std::string_view family, float sz, FontType type)
    {
        auto font = GetFont(family, sz, type);

        if (font != nullptr)
        {
            _currentFontSz = sz;
            ImGui::PushFont((ImFont*)font);
            return true;
        }

        return false;
    }

    bool ImGuiRenderer::SetCurrentFont(void* fontptr, float sz)
    {
        if (fontptr != nullptr)
        {
            _currentFontSz = sz;
            ImGui::PushFont((ImFont*)fontptr);
            return true;
        }

        return false;
    }

    void ImGuiRenderer::ResetFont()
    {
        ImGui::PopFont();
    }

    ImVec2 ImGuiRenderer::GetTextSize(std::string_view text, void* fontptr, float sz, float wrapWidth)
    {
        return ImGuiMeasureText(text, fontptr, sz, wrapWidth);
    }

    void ImGuiRenderer::DrawText(std::string_view text, ImVec2 pos, uint32_t color, float wrapWidth)
    {
        auto font = ImGui::GetFont();
        ((ImDrawList*)UserData)->AddText(font, _currentFontSz, pos, color, text.data(), text.data() + text.size(),
            wrapWidth);
    }

    void ImGuiRenderer::DrawTooltip(ImVec2 pos, std::string_view text)
    {
        if (!text.empty())
        {
            //SetCurrentFont(config.DefaultFontFamily, Config.defaultFontSz, FT_Normal);
            ImGui::SetTooltip("%.*s", (int)text.size(), text.data());
            ResetFont();
        }
    }

    float ImGuiRenderer::EllipsisWidth(void* fontptr, float sz)
    {
        return ((ImFont*)fontptr)->EllipsisWidth;
    }

    void ImGuiRenderer::DrawSVG(ImVec2 pos, ImVec2 size, uint32_t color, std::string_view content, bool fromFile)
    {
        constexpr int bufsz = 1 << 13;
        static char buffer[bufsz] = { 0 };

        auto& dl = *((ImDrawList*)UserData);
        bool found = false;

        for (const auto& [key, texid] : bitmaps)
        {
            if (key.size == size && key.color == color && key.isFile == fromFile && key.data == content)
            {
                found = true;
                dl.AddImage(texid, pos, pos + size);
                break;
            }
        }

        if (!found)
        {
            int csz = fromFile ? 0 : (int)content.size();

            if (fromFile)
            {
                auto fptr = std::fopen(content.data(), "r");
                
                if (fptr != nullptr)
                {
                    csz = (int)std::fread(buffer, 1, bufsz - 1, fptr);
                    std::fclose(fptr);
                }
            }
            else
                std::memcpy(buffer, content.data(), std::min(csz, bufsz - 1));

            if (csz > 0)
            {
                auto& entry = bitmaps.emplace_back();
                entry.first = ImageLookupKey{ size, color, std::string{ content.data(), content.size() }, fromFile };
                auto document = lunasvg::Document::loadFromData(buffer);
                auto bitmap = document->renderToBitmap((int)size.x, (int)size.y, color);
                bitmap.convertToRGBA();
                auto pixels = bitmap.data();
                auto texid = UploadImage(pos, size, pixels, dl);
                entry.second = texid;
                dl.AddImage(texid, pos, pos + size);
            }
        }
    }

    void ImGuiRenderer::DrawImage(ImVec2 pos, ImVec2 size, std::string_view file)
    {
        auto& dl = *((ImDrawList*)UserData);
        bool found = false;

        for (const auto& [key, texid] : bitmaps)
        {
            if (key.size == size && key.data == file)
            {
                found = true;
                dl.AddImage(texid, pos, pos + size);
                break;
            }
        }

        if (!found)
        {
            auto fptr = std::fopen(file.data(), "r");
            
            if (fptr != nullptr)
            {
                std::fseek(fptr, 0, SEEK_END);
                auto bufsz = (int)std::ftell(fptr);
                std::fseek(fptr, 0, SEEK_SET);

                if (bufsz > 0)
                {
                    auto buffer = (char*)malloc(bufsz + 1);
                    assert(buffer != nullptr);
                    auto csz = (int)std::fread(buffer, 1, bufsz, fptr);

                    auto& entry = bitmaps.emplace_back();
                    entry.first.data.assign(file.data(), file.size());
                    entry.first.size = size;

                    int width = 0, height = 0;
                    auto pixels = stbi_load_from_memory((const unsigned char*)buffer, csz, &width, &height, NULL, 4);
                    auto texid = UploadImage(pos, size, pixels, dl);
                    entry.second = texid;
                    dl.AddImage(texid, pos, pos + size);
                    std::free(buffer);
                }

                std::fclose(fptr);
            }
        }
    }

    void ImGuiRenderer::ConstructRoundedRect(ImVec2 startpos, ImVec2 endpos, float topleftr, float toprightr, float bottomrightr, float bottomleftr)
    {
        auto& dl = *((ImDrawList*)UserData);
        auto minlength = std::min(endpos.x - startpos.x, endpos.y - startpos.y);
        topleftr = std::min(topleftr, minlength);
        toprightr = std::min(toprightr, minlength);
        bottomrightr = std::min(bottomrightr, minlength);
        bottomleftr = std::min(bottomleftr, minlength);

        dl.PathClear();
        dl.PathLineTo(ImVec2{ startpos.x, endpos.y - bottomleftr });
        dl.PathLineTo(ImVec2{ startpos.x, startpos.y + topleftr });
        if (topleftr > 0.f) dl.PathArcToFast(ImVec2{ startpos.x + topleftr, startpos.y + topleftr }, topleftr, 6, 9);
        dl.PathLineTo(ImVec2{ endpos.x - toprightr, startpos.y });
        if (toprightr > 0.f) dl.PathArcToFast(ImVec2{ endpos.x - toprightr, startpos.y + toprightr }, toprightr, 9, 12);
        dl.PathLineTo(ImVec2{ endpos.x, endpos.y - bottomrightr });
        if (bottomrightr > 0.f) dl.PathArcToFast(ImVec2{ endpos.x - bottomrightr, endpos.y - bottomrightr }, bottomrightr, 0, 3);
        dl.PathLineTo(ImVec2{ startpos.x - bottomleftr, endpos.y });
        if (bottomleftr > 0.f) dl.PathArcToFast(ImVec2{ startpos.x + bottomleftr, endpos.y - bottomleftr }, bottomleftr, 3, 6);
    }

    float IRenderer::EllipsisWidth(void* fontptr, float sz)
    {
        return GetTextSize("...", fontptr, sz).x;
    }

#pragma endregion

#pragma region Deferred Renderer

    enum class DrawingOps
    {
        Line, Triangle, Rectangle, RoundedRectangle, Circle, Sector,
        RectGradient, RoundedRectGradient,
        Text, Tooltip,
        SVG, Image,
        PushClippingRect, PopClippingRect,
        PushFont, PopFont
    };

    union DrawParams
    {
        struct {
            ImVec2 start, end;
            uint32_t color;
            float thickness;
        } line;

        struct {
            ImVec2 pos1, pos2, pos3;
            uint32_t color;
            float thickness;
            bool filled;
        } triangle;

        struct {
            ImVec2 start, end;
            uint32_t color;
            float thickness;
            bool filled;
        } rect;

        struct {
            ImVec2 start, end;
            float topleftr, toprightr, bottomleftr, bottomrightr;
            uint32_t color;
            float thickness;
            bool filled;
        } roundedRect;

        struct {
            ImVec2 start, end;
            uint32_t from, to;
            Direction dir;
        } rectGradient;

        struct {
            ImVec2 start, end;
            float topleftr, toprightr, bottomleftr, bottomrightr;
            uint32_t from, to;
            Direction dir;
        } roundedRectGradient;

        struct {
            ImVec2 center;
            float radius;
            uint32_t color;
            float thickness;
            bool filled;
        } circle;

        struct {
            ImVec2 center;
            float radius;
            int start, end;
            uint32_t color;
            float thickness;
            bool filled, inverted;
        } sector;

        struct {
            std::string_view text;
            ImVec2 pos;
            uint32_t color;
            float wrapWidth;
        } text;

        struct {
            ImVec2 pos;
            std::string_view text;
        } tooltip;

        struct {
            ImVec2 start, end;
            bool intersect;
        } clippingRect;

        struct {
            void* fontptr;
            float size;
        } font;

        struct {
            ImVec2 pos, size;
            uint32_t color;
            std::string_view content;
            bool isFile;
        } svg;

        DrawParams() {}
    };

    struct DeferredRenderer final : public IRenderer
    {
        Vector<std::pair<DrawingOps, DrawParams>, int32_t, 64> queue;
        ImVec2(*TextMeasure)(std::string_view text, void* fontptr, float sz, float wrapWidth);

        DeferredRenderer(ImVec2(*tm)(std::string_view text, void* fontptr, float sz, float wrapWidth))
            : TextMeasure{ tm } {
        }

        void Render(IRenderer& renderer, ImVec2 offset) override
        {
            auto prevdl = renderer.UserData;
            renderer.UserData = ImGui::GetWindowDrawList();

            for (const auto& entry : queue)
            {
                switch (entry.first)
                {
                case DrawingOps::Line:
                    renderer.DrawLine(entry.second.line.start + offset, entry.second.line.end + offset, entry.second.line.color, 
                        entry.second.line.thickness); 
                    break;

                case DrawingOps::Triangle:
                    renderer.DrawTriangle(entry.second.triangle.pos1, entry.second.triangle.pos2, entry.second.triangle.pos3,
                        entry.second.triangle.color, entry.second.triangle.filled, entry.second.triangle.thickness);
                    break;

                case DrawingOps::Rectangle:
                    renderer.DrawRect(entry.second.rect.start + offset, entry.second.rect.end + offset, entry.second.rect.color,
                        entry.second.rect.filled, entry.second.rect.thickness);
                    break;

                case DrawingOps::RoundedRectangle:
                    renderer.DrawRoundedRect(entry.second.roundedRect.start + offset, entry.second.roundedRect.end + offset, entry.second.roundedRect.color,
                        entry.second.roundedRect.filled, entry.second.roundedRect.topleftr, entry.second.roundedRect.toprightr,
                        entry.second.roundedRect.bottomrightr, entry.second.roundedRect.bottomleftr, entry.second.roundedRect.thickness);
                    break;

                case DrawingOps::Circle:
                    renderer.DrawCircle(entry.second.circle.center + offset, entry.second.circle.radius, entry.second.circle.color,
                        entry.second.circle.filled, entry.second.circle.thickness);
                    break;

                case DrawingOps::Sector:
                    renderer.DrawSector(entry.second.sector.center + offset, entry.second.sector.radius, entry.second.sector.start, entry.second.sector.end,
                        entry.second.sector.color, entry.second.sector.filled, entry.second.sector.inverted, entry.second.sector.thickness);
                    break;

                case DrawingOps::RectGradient:
                    renderer.DrawRectGradient(entry.second.rectGradient.start + offset, entry.second.rectGradient.end + offset, entry.second.rectGradient.from,
                        entry.second.rectGradient.to, entry.second.rectGradient.dir);
                    break;

                case DrawingOps::RoundedRectGradient:
                    renderer.DrawRoundedRectGradient(entry.second.roundedRectGradient.start + offset, entry.second.roundedRectGradient.end + offset,
                        entry.second.roundedRectGradient.topleftr, entry.second.roundedRectGradient.toprightr,
                        entry.second.roundedRectGradient.bottomrightr, entry.second.roundedRectGradient.bottomleftr,
                        entry.second.roundedRectGradient.from, entry.second.roundedRectGradient.to, entry.second.roundedRectGradient.dir);
                    break;

                case DrawingOps::Text:
                    renderer.DrawText(entry.second.text.text, entry.second.text.pos + offset, entry.second.text.color, entry.second.text.wrapWidth);
                    break;

                case DrawingOps::Tooltip:
                    renderer.DrawTooltip(entry.second.tooltip.pos + offset, entry.second.tooltip.text);
                    break;

                case DrawingOps::SVG:
                    renderer.DrawSVG(entry.second.svg.pos, entry.second.svg.size, entry.second.svg.color, entry.second.svg.content, entry.second.svg.isFile);
                    break;

                case DrawingOps::PushClippingRect:
                    renderer.SetClipRect(entry.second.clippingRect.start + offset, entry.second.clippingRect.end + offset, 
                        entry.second.clippingRect.intersect);
                    break;

                case DrawingOps::PopClippingRect:
                    renderer.ResetClipRect();
                    break;

                case DrawingOps::PushFont:
                    renderer.SetCurrentFont(entry.second.font.fontptr, entry.second.font.size);
                    break;

                case DrawingOps::PopFont:
                    renderer.ResetFont();
                    break;

                default: break;
                }
            }

            renderer.UserData = prevdl;
        }

        void Reset() { queue.clear(); size = { 0.f, 0.f }; }

        void SetClipRect(ImVec2 startpos, ImVec2 endpos, bool intersect)
        {
            auto& val = queue.emplace_back(); val.first = DrawingOps::PushClippingRect;
            val.second.clippingRect = { startpos, endpos, intersect };
            size = ImMax(size, endpos);
        }

        void ResetClipRect() { auto& val = queue.emplace_back(); val.first = DrawingOps::PopClippingRect; }

        void DrawLine(ImVec2 startpos, ImVec2 endpos, uint32_t color, float thickness = 1.f)
        {
            auto& val = queue.emplace_back(); val.first = DrawingOps::Line;
            val.second.line = { startpos, endpos, color, thickness };
            size = ImMax(size, endpos);
        }

        void DrawPolyline(ImVec2* points, int sz, uint32_t color, float thickness)
        {
            // TODO ...
        }

        void DrawTriangle(ImVec2 pos1, ImVec2 pos2, ImVec2 pos3, uint32_t color, bool filled, float thickness = 1.f)
        {
            auto& val = queue.emplace_back(); val.first = DrawingOps::Triangle;
            val.second.triangle = { pos1, pos2, pos3, color, thickness, filled };
            size = ImMax(size, pos1);
            size = ImMax(size, pos2);
            size = ImMax(size, pos3);
        }

        void DrawRect(ImVec2 startpos, ImVec2 endpos, uint32_t color, bool filled, float thickness = 1.f)
        {
            auto& val = queue.emplace_back(); val.first = DrawingOps::Rectangle;
            val.second.rect = { startpos, endpos, color, thickness, filled };
            size = ImMax(size, endpos);
        }

        void DrawRoundedRect(ImVec2 startpos, ImVec2 endpos, uint32_t color, bool filled, float topleftr, float toprightr,
            float bottomrightr, float bottomleftr, float thickness = 1.f)
        {
            auto& val = queue.emplace_back(); val.first = DrawingOps::RoundedRectangle;
            val.second.roundedRect = { startpos, endpos, topleftr, toprightr, bottomleftr, bottomrightr, color, thickness, filled };
            size = ImMax(size, endpos);
        }

        void DrawRectGradient(ImVec2 startpos, ImVec2 endpos, uint32_t colorfrom, uint32_t colorto, Direction dir)
        {
            auto& val = queue.emplace_back(); val.first = DrawingOps::RectGradient;
            val.second.rectGradient = { startpos, endpos, colorfrom, colorto, dir };
            size = ImMax(size, endpos);
        }

        void DrawRoundedRectGradient(ImVec2 startpos, ImVec2 endpos, float topleftr, float toprightr, float bottomrightr,
            float bottomleftr, uint32_t colorfrom, uint32_t colorto, Direction dir)
        {
            auto& val = queue.emplace_back(); val.first = DrawingOps::RoundedRectGradient;
            val.second.roundedRectGradient = { startpos, endpos, topleftr, toprightr, bottomleftr, bottomrightr, colorfrom, colorto, dir };
            size = ImMax(size, endpos);
        }

        void DrawPolygon(ImVec2* points, int sz, uint32_t color, bool filled, float thickness = 1.f) {}

        void DrawPolyGradient(ImVec2* points, uint32_t* colors, int sz) {}

        void DrawCircle(ImVec2 center, float radius, uint32_t color, bool filled, float thickness = 1.f)
        {
            auto& val = queue.emplace_back(); val.first = DrawingOps::Circle;
            val.second.circle = { center, radius, color, thickness, filled };
            size = ImMax(size, center + ImVec2{ radius, radius });
        }

        void DrawSector(ImVec2 center, float radius, int start, int end, uint32_t color, bool filled, bool inverted, float thickness = 1.f)
        {
            auto& val = queue.emplace_back(); val.first = DrawingOps::Sector;
            val.second.sector = { center, radius, start, end, color, thickness, filled, inverted };
            size = ImMax(size, center + ImVec2{ radius, radius });
        }

        void DrawRadialGradient(ImVec2 center, float radius, uint32_t in, uint32_t out, int start, int end) {}

        bool SetCurrentFont(std::string_view family, float sz, FontType type) override
        {
            auto& val = queue.emplace_back(); val.first = DrawingOps::PushFont;
            val.second.font = { GetFont(family, sz, type), sz };
            return true;
        }

        bool SetCurrentFont(void* fontptr, float sz) override
        {
            auto& val = queue.emplace_back(); val.first = DrawingOps::PushFont;
            val.second.font = { fontptr, sz };
            return true;
        }

        void ResetFont() override
        {
            auto& val = queue.emplace_back(); val.first = DrawingOps::PopFont;
        }

        ImVec2 GetTextSize(std::string_view text, void* fontptr, float sz, float wrapWidth = -1.f)
        {
            return TextMeasure(text, fontptr, sz, wrapWidth);
        }

        void DrawText(std::string_view text, ImVec2 pos, uint32_t color, float wrapWidth = -1.f)
        {
            auto& val = queue.emplace_back(); val.first = DrawingOps::Text;
            ::new (&val.second.text.text) std::string_view{ text };
            val.second.text.color = color;
            val.second.text.pos = pos;
            val.second.text.wrapWidth = wrapWidth;
            size = ImMax(size, pos);
        }

        void DrawTooltip(ImVec2 pos, std::string_view text)
        {
            auto& val = queue.emplace_back(); val.first = DrawingOps::Tooltip;
            val.second.tooltip.pos = pos;
            ::new (&val.second.tooltip.text) std::string_view{ text };
        }

        void DrawSVG(ImVec2 pos, ImVec2 size, uint32_t color, std::string_view content, bool fromFile)
        {
            auto& val = queue.emplace_back(); val.first = DrawingOps::SVG;
            val.second.svg = { pos, size, color, content, fromFile };
        }
    };

#pragma endregion

    IRenderer* CreateDeferredRenderer(ImVec2(*tm)(std::string_view text, void* fontptr, float sz, float wrapWidth))
    {
        static thread_local DeferredRenderer renderer{ tm };
        return &renderer;
    }

    IRenderer* CreateImGuiRenderer()
    {
        static thread_local ImGuiRenderer renderer{};
        return &renderer;
    }
}
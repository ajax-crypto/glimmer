#include "renderer.h"

#include <cstdio>
#include <charconv>

#define _USE_MATH_DEFINES
#include <math.h>
#include <libs/inc/lunasvg/lunasvg.h>
#include <style.h>

#define STB_IMAGE_IMPLEMENTATION
#include <libs/inc/stb_image/stb_image.h>
#include "libs/inc/imgui/imgui_impl_opengl3_loader.h"
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
    ImVec2& Round(ImVec2& v) { v.x = roundf(v.x); v.y = roundf(v.y); return v; };

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

        bool StartOverlay(int32_t id, ImVec2 pos, ImVec2 size, uint32_t color) override;
        void EndOverlay() override;

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
        ImDrawList* prevlist = nullptr;
    };

    ImGuiRenderer::ImGuiRenderer()
    {}

    void ImGuiRenderer::SetClipRect(ImVec2 startpos, ImVec2 endpos, bool intersect)
    {
        Round(startpos); Round(endpos);
        ImGui::PushClipRect(startpos, endpos, intersect);
    }

    void ImGuiRenderer::ResetClipRect()
    {
        ImGui::PopClipRect();
    }

    void ImGuiRenderer::DrawLine(ImVec2 startpos, ImVec2 endpos, uint32_t color, float thickness)
    {
        Round(startpos); Round(endpos); thickness = roundf(thickness);
        ((ImDrawList*)UserData)->AddLine(startpos, endpos, color, thickness);
    }

    void ImGuiRenderer::DrawPolyline(ImVec2* points, int sz, uint32_t color, float thickness)
    {
        ((ImDrawList*)UserData)->AddPolyline(points, sz, color, 0, thickness);
    }

    void ImGuiRenderer::DrawTriangle(ImVec2 pos1, ImVec2 pos2, ImVec2 pos3, uint32_t color, bool filled, float thickness)
    {
        Round(pos1); Round(pos2); Round(pos3); thickness = roundf(thickness);
        filled ? ((ImDrawList*)UserData)->AddTriangleFilled(pos1, pos2, pos3, color) :
            ((ImDrawList*)UserData)->AddTriangle(pos1, pos2, pos3, color, thickness);
    }

    void ImGuiRenderer::DrawRect(ImVec2 startpos, ImVec2 endpos, uint32_t color, bool filled, float thickness)
    {
        if (thickness > 0.f || filled)
        {
            Round(startpos); Round(endpos); thickness = roundf(thickness);
            filled ? ((ImDrawList*)UserData)->AddRectFilled(startpos, endpos, color) :
                ((ImDrawList*)UserData)->AddRect(startpos, endpos, color, 0.f, 0, thickness);
        }
    }

    void ImGuiRenderer::DrawRoundedRect(ImVec2 startpos, ImVec2 endpos, uint32_t color, bool filled,
        float topleftr, float toprightr, float bottomrightr, float bottomleftr, float thickness)
    {
        auto isUniformRadius = (topleftr == toprightr && toprightr == bottomrightr && bottomrightr == bottomleftr) ||
            ((topleftr + toprightr + bottomrightr + bottomleftr) == 0.f);

        Round(startpos); Round(endpos);
        thickness = roundf(thickness);
        topleftr = roundf(topleftr);
        toprightr = roundf(toprightr);
        bottomrightr = roundf(bottomrightr);
        bottomleftr = roundf(bottomleftr);

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
        Round(startpos); Round(endpos);
        dir == DIR_Horizontal ? ((ImDrawList*)UserData)->AddRectFilledMultiColor(startpos, endpos, colorfrom, colorto, colorto, colorfrom) :
            ((ImDrawList*)UserData)->AddRectFilledMultiColor(startpos, endpos, colorfrom, colorfrom, colorto, colorto);
    }

    void ImGuiRenderer::DrawRoundedRectGradient(ImVec2 startpos, ImVec2 endpos, float topleftr, float toprightr, float bottomrightr, float bottomleftr, 
        uint32_t colorfrom, uint32_t colorto, Direction dir)
    {
        auto& dl = *((ImDrawList*)UserData);
        ConstructRoundedRect(startpos, endpos, topleftr, toprightr, bottomrightr, bottomleftr);
        // TODO: Create color array per vertex
        DrawPolyGradient(dl._Path.Data, NULL, dl._Path.Size);
    }

    void ImGuiRenderer::DrawCircle(ImVec2 center, float radius, uint32_t color, bool filled, float thickness)
    {
        Round(center); radius = roundf(radius); thickness = roundf(thickness);
        filled ? ((ImDrawList*)UserData)->AddCircleFilled(center, radius, color) :
            ((ImDrawList*)UserData)->AddCircle(center, radius, color, 0, thickness);
    }

    void ImGuiRenderer::DrawSector(ImVec2 center, float radius, int start, int end, uint32_t color, bool filled, bool inverted, float thickness)
    {
        constexpr float DegToRad = M_PI / 180.f;
        Round(center); radius = roundf(radius); thickness = roundf(thickness);

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
        Round(center); radius = roundf(radius);

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
        Round(pos);
        auto font = ImGui::GetFont();
        ((ImDrawList*)UserData)->AddText(font, _currentFontSz, pos, color, text.data(), text.data() + text.size(),
            wrapWidth);
    }

    void ImGuiRenderer::DrawTooltip(ImVec2 pos, std::string_view text)
    {
        if (!text.empty())
        {
            //SetCurrentFont(config.DefaultFontFamily, Config.defaultFontSz, FT_Normal);
            //ImGui::SetTooltip("%.*s", (int)text.size(), text.data());
            //ResetFont();
        }
    }

    float ImGuiRenderer::EllipsisWidth(void* fontptr, float sz)
    {
        return ((ImFont*)fontptr)->EllipsisWidth;
    }

    bool ImGuiRenderer::StartOverlay(int32_t id, ImVec2 pos, ImVec2 size, uint32_t color)
    {
        Round(pos); Round(size);

        char buffer[32];
        memset(buffer, 0, 32);
        std::to_chars(buffer, buffer + 31, id);

        ImGui::SetNextWindowPos(pos);
        ImGui::SetNextWindowSize(size);

        auto res = ImGui::BeginPopup(buffer, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
        if (res)
        {
            auto dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(pos, pos + size, color);
            prevlist = (ImDrawList*)UserData;
            UserData = dl;
            SetClipRect(pos, pos + size, false);
        }
        return res;
    }

    void ImGuiRenderer::EndOverlay()
    {
        ResetClipRect();
        ImGui::EndPopup();
        UserData = prevlist;
        prevlist = nullptr;
    }

    void ImGuiRenderer::DrawSVG(ImVec2 pos, ImVec2 size, uint32_t color, std::string_view content, bool fromFile)
    {
        Round(pos); Round(size);

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
#ifdef WIN32
                FILE* fptr = nullptr;
                fopen_s(&fptr, content.data(), "r");
#else
                auto fptr = std::fopen(content.data(), "r");
#endif
                
                if (fptr != nullptr)
                {
                    csz = (int)std::fread(buffer, 1, bufsz - 1, fptr);
                    std::fclose(fptr);
                }
            }
            else
                memcpy(buffer, content.data(), std::min(csz, bufsz - 1));

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
        Round(pos); Round(size);

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
#ifdef WIN32
            FILE* fptr = nullptr;
            fopen_s(&fptr, file.data(), "r");
#else
            auto fptr = std::fopen(file.data(), "r");
#endif
            
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
        Vector<std::pair<DrawingOps, DrawParams>, int32_t, 32> queue{ 32 };
        ImVec2(*TextMeasure)(std::string_view text, void* fontptr, float sz, float wrapWidth);

        DeferredRenderer(ImVec2(*tm)(std::string_view text, void* fontptr, float sz, float wrapWidth))
            : TextMeasure{ tm } {
        }

        int TotalEnqueued() const override { return queue.size(); }

        void Render(IRenderer& renderer, ImVec2 offset, int from, int to) override
        {
            auto prevdl = renderer.UserData;
            renderer.UserData = ImGui::GetWindowDrawList();
            to = to == -1 ? queue.size() : to;

            for (auto idx = from; idx < to; ++idx)
            {
                const auto& entry = queue[idx];

                switch (entry.first)
                {
                case DrawingOps::Line:
                    renderer.DrawLine(entry.second.line.start + offset, entry.second.line.end + offset, entry.second.line.color, 
                        entry.second.line.thickness); 
                    break;

                case DrawingOps::Triangle:
                    renderer.DrawTriangle(entry.second.triangle.pos1 + offset, entry.second.triangle.pos2 + offset, 
                        entry.second.triangle.pos3 + offset, entry.second.triangle.color, entry.second.triangle.filled, 
                        entry.second.triangle.thickness);
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

        void Reset() { queue.clear(true); size = { 0.f, 0.f }; }

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

#pragma region SVG Renderer

    // Helper to format uint32_t color to SVG "rgb(r,g,b)" or "rgba(r,g,b,a_float)" string into a buffer
    // Returns the number of characters written (excluding null terminator).
    inline int formatColorToSvg(char* buffer, size_t bufferSize, uint32_t colorInt)
    {
        auto [r, g, b, a] = DecomposeColor(colorInt); // Assumes DecomposeColor is available
        if (a == 255)
        {
            return snprintf(buffer, bufferSize, "rgb(%d,%d,%d)", r, g, b);
        }
        else
        {
            return snprintf(buffer, bufferSize, "rgba(%d,%d,%d,%.3f)", r, g, b, static_cast<float>(a) / 255.0f);
        }
    }

    // Helper to format uint32_t color's alpha to SVG opacity string into a buffer
    // Returns the number of characters written.
    inline int formatOpacityToSvg(char* buffer, size_t bufferSize, uint32_t colorInt) 
    {
        auto [r, g, b, a] = DecomposeColor(colorInt);
        return snprintf(buffer, bufferSize, "%.3f", static_cast<float>(a) / 255.0f);
    }

    struct SVGRenderer final : public IRenderer
    {
        ImVec2(*textMeasureFunc)(std::string_view text, void* fontPtr, float sz, float wrapWidth);

        int defsIdCounter;
        std::string currentClipPathId; // Kept as std::string for convenience of ID management
        bool clippingActive;
        ImVec2 svgDimensions;

        std::string currentFontFamily; // Kept as std::string
        float currentFontSizePixels;

        static constexpr size_t svgMainBufferSize = 1024 * 64; // 64KB for main content
        static constexpr size_t svgDefsBufferSize = 1024 * 8;  // 8KB for <defs> content
        static constexpr size_t scratchBufferSize = 1024 * 2;  // 2KB for temporary formatting

        char mainSvgBuffer[svgMainBufferSize];
        size_t mainSvgBufferOffset;

        char defsBuffer[svgDefsBufferSize];
        size_t defsBufferOffset;

        char scratchBuffer[scratchBufferSize]; // For formatting individual elements

        // Helper to append data to a buffer, handling potential overflow.
        void appendToBuffer(char* destBuffer, size_t& destOffset, size_t destCapacity, const char* srcData, int srcLen) 
        {
            if (srcLen <= 0) return;
            if (destOffset + srcLen < destCapacity) {
                memcpy(destBuffer + destOffset, srcData, srcLen);
                destOffset += srcLen;
                destBuffer[destOffset] = '\0'; // Keep null-terminated for safety if used as C-string
            }
            else {
                // Handle overflow: for now, just don't append to prevent overrun.
                // Optionally, log an error or append an overflow marker if space allows.
            }
        }

        void appendStringViewToBuffer(char* destBuffer, size_t& destOffset, size_t destCapacity, std::string_view sv) 
        {
            appendToBuffer(destBuffer, destOffset, destCapacity, sv.data(), static_cast<int>(sv.length()));
        }

        SVGRenderer(ImVec2(*measureFunc)(std::string_view text, void* fontPtr, float sz, float wrapWidth), ImVec2 dimensionsVal = { 800, 600 })
            : textMeasureFunc(measureFunc),
            defsIdCounter(0),
            clippingActive(false),
            svgDimensions(dimensionsVal),
            currentFontFamily("sans-serif"),
            currentFontSizePixels(16.f),
            mainSvgBufferOffset(0),
            defsBufferOffset(0)
        {
            mainSvgBuffer[0] = '\0';
            defsBuffer[0] = '\0';
            Reset();
        }

        void Reset() override
        {
            mainSvgBufferOffset = 0;
            mainSvgBuffer[0] = '\0';
            defsBufferOffset = 0;
            defsBuffer[0] = '\0';

            defsIdCounter = 0;
            currentClipPathId.clear();
            clippingActive = false;

            this->size = svgDimensions;
        }

        std::string GetSVG()
        {
            std::string finalSvgStr;
            // Estimate size
            size_t headerLen = 200; // Approx
            size_t footerLen = 20;  // Approx
            finalSvgStr.reserve(headerLen + defsBufferOffset + mainSvgBufferOffset + footerLen);

            float svgW = (svgDimensions.x > 0.001f) ? svgDimensions.x : 1.0f;
            float svgH = (svgDimensions.y > 0.001f) ? svgDimensions.y : 1.0f;

            int writtenHeader = snprintf(scratchBuffer, scratchBufferSize,
                "<svg width=\"%.2f\" height=\"%.2f\" viewBox=\"0 0 %.2f %.2f\" "
                "xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\">\n"
                "  <defs>\n",
                svgW, svgH, svgW, svgH);
            if (writtenHeader > 0) finalSvgStr.append(scratchBuffer, writtenHeader);

            if (defsBufferOffset > 0) 
            {
                finalSvgStr.append(defsBuffer, defsBufferOffset);
            }
            finalSvgStr.append("  </defs>\n"); // Close defs

            if (mainSvgBufferOffset > 0) 
            {
                finalSvgStr.append(mainSvgBuffer, mainSvgBufferOffset);
            }

            finalSvgStr.append("</svg>\n");
            return finalSvgStr;
        }

        // --- IRenderer Implementation ---

        void SetClipRect(ImVec2 startPos, ImVec2 endPos, bool intersect) override
        {
            if (clippingActive) 
            { // Close previous clipping group in main content
                appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, "  </g>\n", strlen("  </g>\n"));
            }

            defsIdCounter++;
            char clipIdCstr[64];
            snprintf(clipIdCstr, sizeof(clipIdCstr), "clipPathDef%d", defsIdCounter);
            currentClipPathId = clipIdCstr;

            int defsWritten = snprintf(scratchBuffer, scratchBufferSize,
                "    <clipPath id=\"%s\">\n"
                "      <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" />\n"
                "    </clipPath>\n",
                currentClipPathId.c_str(),
                startPos.x, startPos.y,
                std::max(0.0f, endPos.x - startPos.x),
                std::max(0.0f, endPos.y - startPos.y));
            if (defsWritten > 0) 
            {
                appendToBuffer(defsBuffer, defsBufferOffset, svgDefsBufferSize, scratchBuffer, defsWritten);
            }

            int gWritten = snprintf(scratchBuffer, scratchBufferSize, "  <g clip-path=\"url(#%s)\">\n", currentClipPathId.c_str());
            if (gWritten > 0) 
            {
                appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, scratchBuffer, gWritten);
            }
            clippingActive = true;
        }

        void ResetClipRect() override
        {
            if (clippingActive) 
            {
                appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, "  </g>\n", strlen("  </g>\n"));
                clippingActive = false;
                currentClipPathId.clear();
            }
        }

        void DrawLine(ImVec2 startPos, ImVec2 endPos, uint32_t color, float thickness = 1.f) override
        {
            if (thickness <= 0.f) return;
            char colorBuf[64];
            formatColorToSvg(colorBuf, sizeof(colorBuf), color);

            int written = snprintf(scratchBuffer, scratchBufferSize,
                "  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" stroke=\"%s\" stroke-width=\"%.2f\" />\n",
                startPos.x, startPos.y, endPos.x, endPos.y, colorBuf, thickness);
            if (written > 0) 
            {
                appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, scratchBuffer, written);
            }
        }

        void DrawPolyline(ImVec2* points, int numPoints, uint32_t color, float thickness) override
        {
            if (numPoints < 2 || thickness <= 0.f) return;

            char pointsListStr[scratchBufferSize / 2];
            int pointsListOffset = 0;
            char pointItemBuf[40];

            for (int i = 0; i < numPoints; ++i) 
            {
                int currentPtLen = snprintf(pointItemBuf, sizeof(pointItemBuf), "%.2f,%.2f%s",
                    points[i].x, points[i].y, (i == numPoints - 1 ? "" : " "));
                if (pointsListOffset + currentPtLen < (int)sizeof(pointsListStr) - 1) { // -1 for null terminator
                    memcpy(pointsListStr + pointsListOffset, pointItemBuf, currentPtLen);
                    pointsListOffset += currentPtLen;
                }
                else { break; }
            }
            pointsListStr[pointsListOffset] = '\0';

            char colorBuf[64];
            formatColorToSvg(colorBuf, sizeof(colorBuf), color);

            int written = snprintf(scratchBuffer, scratchBufferSize,
                "  <polyline points=\"%s\" stroke=\"%s\" stroke-width=\"%.2f\" fill=\"none\" />\n",
                pointsListStr, colorBuf, thickness);
            if (written > 0) 
            {
                appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, scratchBuffer, written);
            }
        }

        void DrawTriangle(ImVec2 pos1, ImVec2 pos2, ImVec2 pos3, uint32_t color, bool filled, float thickness = 1.f) override
        {
            char colorBuf[64];
            formatColorToSvg(colorBuf, sizeof(colorBuf), color);

            int written = 0;
            if (filled) 
            {
                if (thickness > 0.0f) {
                    written = snprintf(scratchBuffer, scratchBufferSize,
                        "  <polygon points=\"%.2f,%.2f %.2f,%.2f %.2f,%.2f\" fill=\"%s\" stroke=\"%s\" stroke-width=\"%.2f\" />\n",
                        pos1.x, pos1.y, pos2.x, pos2.y, pos3.x, pos3.y, colorBuf, colorBuf, thickness);
                }
                else {
                    written = snprintf(scratchBuffer, scratchBufferSize,
                        "  <polygon points=\"%.2f,%.2f %.2f,%.2f %.2f,%.2f\" fill=\"%s\" />\n",
                        pos1.x, pos1.y, pos2.x, pos2.y, pos3.x, pos3.y, colorBuf);
                }
            }
            else 
            {
                if (thickness <= 0.f) return;
                written = snprintf(scratchBuffer, scratchBufferSize,
                    "  <polygon points=\"%.2f,%.2f %.2f,%.2f %.2f,%.2f\" fill=\"none\" stroke=\"%s\" stroke-width=\"%.2f\" />\n",
                    pos1.x, pos1.y, pos2.x, pos2.y, pos3.x, pos3.y, colorBuf, thickness);
            }

            if (written > 0) 
            {
                appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, scratchBuffer, written);
            }
        }

        void DrawRect(ImVec2 startPos, ImVec2 endPos, uint32_t color, bool filled, float thickness = 1.f) override
        {
            float w = endPos.x - startPos.x;
            float h = endPos.y - startPos.y;
            if (w <= 0.001f && h <= 0.001f) 
            {
                if (!filled && thickness > 0.f && (std::abs(w) < 0.001f || std::abs(h) < 0.001f)) { /* line */ }
                else return;
            }
            
            w = std::max(0.0f, w);
            h = std::max(0.0f, h);

            char colorBuf[64];
            formatColorToSvg(colorBuf, sizeof(colorBuf), color);
            int written = 0;

            if (filled) 
            {
                if (thickness > 0.0f) 
                {
                    written = snprintf(scratchBuffer, scratchBufferSize,
                        "  <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" fill=\"%s\" stroke=\"%s\" stroke-width=\"%.2f\" />\n",
                        startPos.x, startPos.y, w, h, colorBuf, colorBuf, thickness);
                }
                else 
                {
                    written = snprintf(scratchBuffer, scratchBufferSize,
                        "  <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" fill=\"%s\" />\n",
                        startPos.x, startPos.y, w, h, colorBuf);
                }
            }
            else 
            {
                if (thickness <= 0.f) return;
                written = snprintf(scratchBuffer, scratchBufferSize,
                    "  <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" fill=\"none\" stroke=\"%s\" stroke-width=\"%.2f\" />\n",
                    startPos.x, startPos.y, w, h, colorBuf, thickness);
            }

            if (written > 0) 
            {
                appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, scratchBuffer, written);
            }
        }

        void DrawRoundedRect(ImVec2 startPos, ImVec2 endPos, uint32_t color, bool filled,
            float topLeftR, float topRightR, float bottomRightR, float bottomLeftR,
            float thickness) override
        {
            float w = endPos.x - startPos.x;
            float h = endPos.y - startPos.y;
            if (w <= 0.001f || h <= 0.001f) return;
            w = std::max(0.0f, w);
            h = std::max(0.0f, h);

            char colorBuf[64];
            formatColorToSvg(colorBuf, sizeof(colorBuf), color);
            int written = 0;

            bool uniformRadii = (std::abs(topLeftR - topRightR) < 0.01f &&
                std::abs(topRightR - bottomRightR) < 0.01f &&
                std::abs(bottomRightR - bottomLeftR) < 0.01f);

            if (uniformRadii && topLeftR >= 0.f) 
            {
                float radius = std::min({ topLeftR, w / 2.0f, h / 2.0f });
                radius = std::max(0.0f, radius);
                if (filled) {
                    if (thickness > 0.0f) 
                    {
                        written = snprintf(scratchBuffer, scratchBufferSize,
                            "  <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" rx=\"%.2f\" ry=\"%.2f\" fill=\"%s\" stroke=\"%s\" stroke-width=\"%.2f\" />\n",
                            startPos.x, startPos.y, w, h, radius, radius, colorBuf, colorBuf, thickness);
                    }
                    else 
                    {
                        written = snprintf(scratchBuffer, scratchBufferSize,
                            "  <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" rx=\"%.2f\" ry=\"%.2f\" fill=\"%s\" />\n",
                            startPos.x, startPos.y, w, h, radius, radius, colorBuf);
                    }
                }
                else 
                {
                    if (thickness <= 0.f) return;
                    written = snprintf(scratchBuffer, scratchBufferSize,
                        "  <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" rx=\"%.2f\" ry=\"%.2f\" fill=\"none\" stroke=\"%s\" stroke-width=\"%.2f\" />\n",
                        startPos.x, startPos.y, w, h, radius, radius, colorBuf, thickness);
                }
            }
            else 
            {
                float tlr = std::min({ std::max(0.0f, topLeftR), w / 2.0f, h / 2.0f });
                float trr = std::min({ std::max(0.0f, topRightR), w / 2.0f, h / 2.0f });
                float brr = std::min({ std::max(0.0f, bottomRightR), w / 2.0f, h / 2.0f });
                float blr = std::min({ std::max(0.0f, bottomLeftR), w / 2.0f, h / 2.0f });

                char pathDataBuf[1024];
                int pathOffset = 0;
                pathOffset += snprintf(pathDataBuf + pathOffset, sizeof(pathDataBuf) - pathOffset, "M %.2f,%.2f ", startPos.x + tlr, startPos.y);
                pathOffset += snprintf(pathDataBuf + pathOffset, sizeof(pathDataBuf) - pathOffset, "L %.2f,%.2f ", endPos.x - trr, startPos.y);
                if (trr > 0.001f) pathOffset += snprintf(pathDataBuf + pathOffset, sizeof(pathDataBuf) - pathOffset, "A %.2f,%.2f 0 0 1 %.2f,%.2f ", trr, trr, endPos.x, startPos.y + trr);
                pathOffset += snprintf(pathDataBuf + pathOffset, sizeof(pathDataBuf) - pathOffset, "L %.2f,%.2f ", endPos.x, endPos.y - brr);
                if (brr > 0.001f) pathOffset += snprintf(pathDataBuf + pathOffset, sizeof(pathDataBuf) - pathOffset, "A %.2f,%.2f 0 0 1 %.2f,%.2f ", brr, brr, endPos.x - brr, endPos.y);
                pathOffset += snprintf(pathDataBuf + pathOffset, sizeof(pathDataBuf) - pathOffset, "L %.2f,%.2f ", startPos.x + blr, endPos.y);
                if (blr > 0.001f) pathOffset += snprintf(pathDataBuf + pathOffset, sizeof(pathDataBuf) - pathOffset, "A %.2f,%.2f 0 0 1 %.2f,%.2f ", blr, blr, startPos.x, endPos.y - blr);
                pathOffset += snprintf(pathDataBuf + pathOffset, sizeof(pathDataBuf) - pathOffset, "L %.2f,%.2f ", startPos.x, startPos.y + tlr);
                if (tlr > 0.001f) pathOffset += snprintf(pathDataBuf + pathOffset, sizeof(pathDataBuf) - pathOffset, "A %.2f,%.2f 0 0 1 %.2f,%.2f ", tlr, tlr, startPos.x + tlr, startPos.y);
                pathOffset += snprintf(pathDataBuf + pathOffset, sizeof(pathDataBuf) - pathOffset, "Z");

                if (pathOffset > 0 && pathOffset < (int)sizeof(pathDataBuf)) 
                {
                    if (filled) 
                    {
                        if (thickness > 0.0f) 
                        {
                            written = snprintf(scratchBuffer, scratchBufferSize, "  <path d=\"%s\" fill=\"%s\" stroke=\"%s\" stroke-width=\"%.2f\" />\n",
                                pathDataBuf, colorBuf, colorBuf, thickness);
                        }
                        else 
                        {
                            written = snprintf(scratchBuffer, scratchBufferSize, "  <path d=\"%s\" fill=\"%s\" />\n",
                                pathDataBuf, colorBuf);
                        }
                    }
                    else 
                    {
                        if (thickness <= 0.f) return;
                        written = snprintf(scratchBuffer, scratchBufferSize, "  <path d=\"%s\" fill=\"none\" stroke=\"%s\" stroke-width=\"%.2f\" />\n",
                            pathDataBuf, colorBuf, thickness);
                    }
                }
            }

            if (written > 0) 
            {
                appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, scratchBuffer, written);
            }
        }

        void DrawRectGradient(ImVec2 startPos, ImVec2 endPos, uint32_t colorFrom, uint32_t colorTo, Direction dir) override
        {
            float w = endPos.x - startPos.x;
            float h = endPos.y - startPos.y;
            if (w <= 0.001f || h <= 0.001f) return;
            w = std::max(0.0f, w); h = std::max(0.0f, h);

            defsIdCounter++;
            char gradientIdCstr[64];
            snprintf(gradientIdCstr, sizeof(gradientIdCstr), "gradRectDef%d", defsIdCounter);

            char colorFromBuf[64], colorToBuf[64];
            char opacityFromBuf[16], opacityToBuf[16];
            formatColorToSvg(colorFromBuf, sizeof(colorFromBuf), colorFrom);
            formatColorToSvg(colorToBuf, sizeof(colorToBuf), colorTo);
            formatOpacityToSvg(opacityFromBuf, sizeof(opacityFromBuf), colorFrom);
            formatOpacityToSvg(opacityToBuf, sizeof(opacityToBuf), colorTo);

            int defsWritten = snprintf(scratchBuffer, scratchBufferSize,
                "    <linearGradient id=\"%s\" %s>\n"
                "      <stop offset=\"0%%\" style=\"stop-color:%s;stop-opacity:%s\" />\n"
                "      <stop offset=\"100%%\" style=\"stop-color:%s;stop-opacity:%s\" />\n"
                "    </linearGradient>\n",
                gradientIdCstr,
                (dir == DIR_Horizontal ? "x1=\"0%\" y1=\"0%\" x2=\"100%\" y2=\"0%\"" : "x1=\"0%\" y1=\"0%\" x2=\"0%\" y2=\"100%\""),
                colorFromBuf, opacityFromBuf,
                colorToBuf, opacityToBuf);
            if (defsWritten > 0) 
            {
                appendToBuffer(defsBuffer, defsBufferOffset, svgDefsBufferSize, scratchBuffer, defsWritten);
            }

            int rectWritten = snprintf(scratchBuffer, scratchBufferSize,
                "  <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" fill=\"url(#%s)\" />\n",
                startPos.x, startPos.y, w, h, gradientIdCstr);
            if (rectWritten > 0) 
            {
                appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, scratchBuffer, rectWritten);
            }
        }

        void DrawRoundedRectGradient(ImVec2 startPos, ImVec2 endPos,
            float topLeftR, float topRightR, float bottomRightR, float bottomLeftR,
            uint32_t colorFrom, uint32_t colorTo, Direction dir) override
        {
            float w = endPos.x - startPos.x;
            float h = endPos.y - startPos.y;
            if (w <= 0.001f || h <= 0.001f) return;
            w = std::max(0.0f, w); h = std::max(0.0f, h);

            defsIdCounter++;
            char gradientIdCstr[64];
            snprintf(gradientIdCstr, sizeof(gradientIdCstr), "gradRoundRectDef%d", defsIdCounter);

            char colorFromBuf[64], colorToBuf[64];
            char opacityFromBuf[16], opacityToBuf[16];
            formatColorToSvg(colorFromBuf, sizeof(colorFromBuf), colorFrom);
            formatColorToSvg(colorToBuf, sizeof(colorToBuf), colorTo);
            formatOpacityToSvg(opacityFromBuf, sizeof(opacityFromBuf), colorFrom);
            formatOpacityToSvg(opacityToBuf, sizeof(opacityToBuf), colorTo);

            int defsWritten = snprintf(scratchBuffer, scratchBufferSize,
                "    <linearGradient id=\"%s\" %s>\n"
                "      <stop offset=\"0%%\" style=\"stop-color:%s;stop-opacity:%s\" />\n"
                "      <stop offset=\"100%%\" style=\"stop-color:%s;stop-opacity:%s\" />\n"
                "    </linearGradient>\n",
                gradientIdCstr,
                (dir == DIR_Horizontal ? "x1=\"0%\" y1=\"0%\" x2=\"100%\" y2=\"0%\"" : "x1=\"0%\" y1=\"0%\" x2=\"0%\" y2=\"100%\""),
                colorFromBuf, opacityFromBuf,
                colorToBuf, opacityToBuf);

            if (defsWritten > 0) 
            {
                appendToBuffer(defsBuffer, defsBufferOffset, svgDefsBufferSize, scratchBuffer, defsWritten);
            }

            int shapeWritten = 0;
            bool uniformRadii = (std::abs(topLeftR - topRightR) < 0.01f &&
                std::abs(topRightR - bottomRightR) < 0.01f &&
                std::abs(bottomRightR - bottomLeftR) < 0.01f);

            if (uniformRadii && topLeftR >= 0.f) 
            {
                float radius = std::min({ topLeftR, w / 2.0f, h / 2.0f });
                radius = std::max(0.0f, radius);
                shapeWritten = snprintf(scratchBuffer, scratchBufferSize,
                    "  <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" rx=\"%.2f\" ry=\"%.2f\" fill=\"url(#%s)\" />\n",
                    startPos.x, startPos.y, w, h, radius, radius, gradientIdCstr);
            }
            else 
            {
                float tlr = std::min({ std::max(0.0f, topLeftR), w / 2.0f, h / 2.0f });
                float trr = std::min({ std::max(0.0f, topRightR), w / 2.0f, h / 2.0f });
                float brr = std::min({ std::max(0.0f, bottomRightR), w / 2.0f, h / 2.0f });
                float blr = std::min({ std::max(0.0f, bottomLeftR), w / 2.0f, h / 2.0f });

                char pathDataBuf[1024];
                int pathOffset = 0;
                pathOffset += snprintf(pathDataBuf + pathOffset, sizeof(pathDataBuf) - pathOffset, "M %.2f,%.2f ", startPos.x + tlr, startPos.y);
                pathOffset += snprintf(pathDataBuf + pathOffset, sizeof(pathDataBuf) - pathOffset, "L %.2f,%.2f ", endPos.x - trr, startPos.y);
                if (trr > 0.001f) pathOffset += snprintf(pathDataBuf + pathOffset, sizeof(pathDataBuf) - pathOffset, "A %.2f,%.2f 0 0 1 %.2f,%.2f ", trr, trr, endPos.x, startPos.y + trr);
                pathOffset += snprintf(pathDataBuf + pathOffset, sizeof(pathDataBuf) - pathOffset, "L %.2f,%.2f ", endPos.x, endPos.y - brr);
                if (brr > 0.001f) pathOffset += snprintf(pathDataBuf + pathOffset, sizeof(pathDataBuf) - pathOffset, "A %.2f,%.2f 0 0 1 %.2f,%.2f ", brr, brr, endPos.x - brr, endPos.y);
                pathOffset += snprintf(pathDataBuf + pathOffset, sizeof(pathDataBuf) - pathOffset, "L %.2f,%.2f ", startPos.x + blr, endPos.y);
                if (blr > 0.001f) pathOffset += snprintf(pathDataBuf + pathOffset, sizeof(pathDataBuf) - pathOffset, "A %.2f,%.2f 0 0 1 %.2f,%.2f ", blr, blr, startPos.x, endPos.y - blr);
                pathOffset += snprintf(pathDataBuf + pathOffset, sizeof(pathDataBuf) - pathOffset, "L %.2f,%.2f ", startPos.x, startPos.y + tlr);
                if (tlr > 0.001f) pathOffset += snprintf(pathDataBuf + pathOffset, sizeof(pathDataBuf) - pathOffset, "A %.2f,%.2f 0 0 1 %.2f,%.2f ", tlr, tlr, startPos.x + tlr, startPos.y);
                pathOffset += snprintf(pathDataBuf + pathOffset, sizeof(pathDataBuf) - pathOffset, "Z");

                if (pathOffset > 0 && pathOffset < (int)sizeof(pathDataBuf)) 
                {
                    shapeWritten = snprintf(scratchBuffer, scratchBufferSize, "  <path d=\"%s\" fill=\"url(#%s)\" />\n",
                        pathDataBuf, gradientIdCstr);
                }
            }

            if (shapeWritten > 0) 
            {
                appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, scratchBuffer, shapeWritten);
            }
        }

        void DrawPolygon(ImVec2* points, int numPoints, uint32_t color, bool filled, float thickness = 1.f) override
        {
            if (numPoints < 3) return;

            char pointsListStr[scratchBufferSize / 2];
            int pointsListOffset = 0;
            char pointItemBuf[40];
            for (int i = 0; i < numPoints; ++i) 
            {
                int currentPtLen = snprintf(pointItemBuf, sizeof(pointItemBuf), "%.2f,%.2f%s",
                    points[i].x, points[i].y, (i == numPoints - 1 ? "" : " "));
                if (pointsListOffset + currentPtLen < (int)sizeof(pointsListStr) - 1) 
                {
                    memcpy(pointsListStr + pointsListOffset, pointItemBuf, currentPtLen);
                    pointsListOffset += currentPtLen;
                }
                else { break; }
            }
            pointsListStr[pointsListOffset] = '\0';

            char colorBuf[64];
            formatColorToSvg(colorBuf, sizeof(colorBuf), color);
            int written = 0;

            if (filled) {
                if (thickness > 0.0f) 
                {
                    written = snprintf(scratchBuffer, scratchBufferSize, "  <polygon points=\"%s\" fill=\"%s\" stroke=\"%s\" stroke-width=\"%.2f\" />\n",
                        pointsListStr, colorBuf, colorBuf, thickness);
                }
                else 
                {
                    written = snprintf(scratchBuffer, scratchBufferSize, "  <polygon points=\"%s\" fill=\"%s\" />\n",
                        pointsListStr, colorBuf);
                }
            }
            else 
            {
                if (thickness <= 0.f) return;
                written = snprintf(scratchBuffer, scratchBufferSize, "  <polygon points=\"%s\" fill=\"none\" stroke=\"%s\" stroke-width=\"%.2f\" />\n",
                    pointsListStr, colorBuf, thickness);
            }

            if (written > 0) 
            {
                appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, scratchBuffer, written);
            }
        }

        void DrawPolyGradient(ImVec2* points, uint32_t* colors, int numPoints) override
        {
            if (numPoints > 0 && colors) 
            { // Simplified: use first color for solid fill
                DrawPolygon(points, numPoints, colors[0], true, 0.f);
            }
        }

        void DrawCircle(ImVec2 center, float radius, uint32_t color, bool filled, float thickness = 1.f) override
        {
            if (radius <= 0.001f) return;
            radius = std::max(0.0f, radius);
            char colorBuf[64];
            formatColorToSvg(colorBuf, sizeof(colorBuf), color);
            int written = 0;

            if (filled) 
            {
                if (thickness > 0.0f) 
                {
                    written = snprintf(scratchBuffer, scratchBufferSize, "  <circle cx=\"%.2f\" cy=\"%.2f\" r=\"%.2f\" fill=\"%s\" stroke=\"%s\" stroke-width=\"%.2f\" />\n",
                        center.x, center.y, radius, colorBuf, colorBuf, thickness);
                }
                else 
                {
                    written = snprintf(scratchBuffer, scratchBufferSize, "  <circle cx=\"%.2f\" cy=\"%.2f\" r=\"%.2f\" fill=\"%s\" />\n",
                        center.x, center.y, radius, colorBuf);
                }
            }
            else 
            {
                if (thickness <= 0.f) return;
                written = snprintf(scratchBuffer, scratchBufferSize, "  <circle cx=\"%.2f\" cy=\"%.2f\" r=\"%.2f\" fill=\"none\" stroke=\"%s\" stroke-width=\"%.2f\" />\n",
                    center.x, center.y, radius, colorBuf, thickness);
            }

            if (written > 0) 
            {
                appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, scratchBuffer, written);
            }
        }

        void DrawSector(ImVec2 center, float radius, int startAngleDeg, int endAngleDeg, uint32_t color, bool filled, bool inverted, float thickness = 1.f) override
        {
            if (radius <= 0.001f) return;
            radius = std::max(0.0f, radius);
            float startRad = static_cast<float>(startAngleDeg) * M_PI / 180.0f;
            float endRad = static_cast<float>(endAngleDeg) * M_PI / 180.0f;

            ImVec2 pStart = { center.x + radius * cosf(startRad), center.y + radius * sinf(startRad) };
            ImVec2 pEnd = { center.x + radius * cosf(endRad), center.y + radius * sinf(endRad) };

            float angleDiff = static_cast<float>(endAngleDeg - startAngleDeg);
            while (angleDiff <= -360.0f) angleDiff += 360.0f;
            while (angleDiff > 360.0f) angleDiff -= 360.0f;

            int largeArcFlag = (std::abs(angleDiff) > 180.0f) ? 1 : 0;
            int sweepFlag = (angleDiff >= 0.0f) ? 1 : 0;

            if (std::abs(angleDiff) >= 359.99f) 
            {
                DrawCircle(center, radius, color, filled, thickness);
                return;
            }

            if (inverted) 
            {
                appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, "\n", strlen("\n"));
            }

            char pathDataBuf[512];
            snprintf(pathDataBuf, sizeof(pathDataBuf), "M %.2f,%.2f L %.2f,%.2f A %.2f,%.2f 0 %d,%d %.2f,%.2f Z",
                center.x, center.y, pStart.x, pStart.y, radius, radius, largeArcFlag, sweepFlag, pEnd.x, pEnd.y);

            char colorBuf[64];
            formatColorToSvg(colorBuf, sizeof(colorBuf), color);
            int written = 0;

            if (filled) 
            {
                if (thickness > 0.0f) 
                {
                    written = snprintf(scratchBuffer, scratchBufferSize, "  <path d=\"%s\" fill=\"%s\" stroke=\"%s\" stroke-width=\"%.2f\" />\n",
                        pathDataBuf, colorBuf, colorBuf, thickness);
                }
                else 
                {
                    written = snprintf(scratchBuffer, scratchBufferSize, "  <path d=\"%s\" fill=\"%s\" />\n",
                        pathDataBuf, colorBuf);
                }
            }
            else 
            {
                if (thickness <= 0.f) return;
                written = snprintf(scratchBuffer, scratchBufferSize, "  <path d=\"%s\" fill=\"none\" stroke=\"%s\" stroke-width=\"%.2f\" />\n",
                    pathDataBuf, colorBuf, thickness);
            }

            if (written > 0) 
            {
                appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, scratchBuffer, written);
            }
        }

        void DrawRadialGradient(ImVec2 center, float radius, uint32_t colorIn, uint32_t colorOut, int startAngleDeg, int endAngleDeg) override
        {
            if (radius <= 0.001f) return;
            radius = std::max(0.0f, radius);
            defsIdCounter++;
            char gradientIdCstr[64];
            snprintf(gradientIdCstr, sizeof(gradientIdCstr), "gradRadialDef%d", defsIdCounter);

            char colorInBuf[64], colorOutBuf[64];
            char opacityInBuf[16], opacityOutBuf[16];
            formatColorToSvg(colorInBuf, sizeof(colorInBuf), colorIn);
            formatColorToSvg(colorOutBuf, sizeof(colorOutBuf), colorOut);
            formatOpacityToSvg(opacityInBuf, sizeof(opacityInBuf), colorIn);
            formatOpacityToSvg(opacityOutBuf, sizeof(opacityOutBuf), colorOut);

            int defsWritten = snprintf(scratchBuffer, scratchBufferSize,
                "    <radialGradient id=\"%s\" cx=\"50%%\" cy=\"50%%\" r=\"50%%\" fx=\"50%%\" fy=\"50%%\">\n"
                "      <stop offset=\"0%%\" style=\"stop-color:%s;stop-opacity:%s\" />\n"
                "      <stop offset=\"100%%\" style=\"stop-color:%s;stop-opacity:%s\" />\n"
                "    </radialGradient>\n",
                gradientIdCstr, colorInBuf, opacityInBuf, colorOutBuf, opacityOutBuf);

            if (defsWritten > 0) 
            {
                appendToBuffer(defsBuffer, defsBufferOffset, svgDefsBufferSize, scratchBuffer, defsWritten);
            }

            int shapeWritten = 0;
            float angleDiffAbs = std::abs(static_cast<float>(endAngleDeg - startAngleDeg));
            while (angleDiffAbs >= 360.0f) angleDiffAbs -= 360.0f;

            if (angleDiffAbs < 359.99f && !(startAngleDeg == 0 && endAngleDeg == 0)) 
            {
                float startRad = static_cast<float>(startAngleDeg) * M_PI / 180.0f;
                float endRad = static_cast<float>(endAngleDeg) * M_PI / 180.0f;
                ImVec2 pStart = { center.x + radius * cosf(startRad), center.y + radius * sinf(startRad) };
                ImVec2 pEnd = { center.x + radius * cosf(endRad), center.y + radius * sinf(endRad) };

                float angleDiffSweep = static_cast<float>(endAngleDeg - startAngleDeg);
                while (angleDiffSweep <= -360.0f) angleDiffSweep += 360.0f;
                while (angleDiffSweep > 360.0f) angleDiffSweep -= 360.0f;
                int largeArcFlag = (std::abs(angleDiffSweep) > 180.0f) ? 1 : 0;
                int sweepFlag = (angleDiffSweep >= 0.0f) ? 1 : 0;

                char pathDataBuf[512];
                snprintf(pathDataBuf, sizeof(pathDataBuf), "M %.2f,%.2f L %.2f,%.2f A %.2f,%.2f 0 %d,%d %.2f,%.2f Z",
                    center.x, center.y, pStart.x, pStart.y, radius, radius, largeArcFlag, sweepFlag, pEnd.x, pEnd.y);
                shapeWritten = snprintf(scratchBuffer, scratchBufferSize, "  <path d=\"%s\" fill=\"url(#%s)\" />\n",
                    pathDataBuf, gradientIdCstr);
            }
            else 
            {
                shapeWritten = snprintf(scratchBuffer, scratchBufferSize, "  <circle cx=\"%.2f\" cy=\"%.2f\" r=\"%.2f\" fill=\"url(#%s)\" />\n",
                    center.x, center.y, radius, gradientIdCstr);
            }

            if (shapeWritten > 0) 
            {
                appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, scratchBuffer, shapeWritten);
            }
        }

        bool SetCurrentFont(std::string_view family, float sz, FontType type) override { return false; }
        bool SetCurrentFont(void* fontPtr, float sz) override { return false; }
        void ResetFont() override {}

        ImVec2 GetTextSize(std::string_view text, void* fontPtr, float sz, float wrapWidth = -1.f) override
        {
            if (textMeasureFunc) 
            {
                return textMeasureFunc(text, fontPtr, sz, wrapWidth);
            }

            return ImVec2{ static_cast<float>(text.length()) * sz * 0.6f, sz }; // Basic fallback
        }

        void DrawText(std::string_view text, ImVec2 pos, uint32_t color, float wrapWidth = -1.f) override
        {
            float adjustedY = pos.y + currentFontSizePixels * 0.8f;

            char colorBuf[64];
            formatColorToSvg(colorBuf, sizeof(colorBuf), color);

            int writtenOffset = snprintf(scratchBuffer, scratchBufferSize,
                "  <text x=\"%.2f\" y=\"%.2f\" font-family=\"%s\" font-size=\"%.0fpx\" fill=\"%s\">",
                pos.x, adjustedY, currentFontFamily.c_str(), currentFontSizePixels, colorBuf);
            if (writtenOffset <= 0) return;
            appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, scratchBuffer, writtenOffset);

            char escCharBuf[10];
            for (char c : text) 
            {
                const char* escSeq = nullptr;
                int seqLen = 1;
                switch (c) 
                {
                case '&':  escSeq = "&amp;"; seqLen = 5; break;
                case '<':  escSeq = "&lt;"; seqLen = 4; break;
                case '>':  escSeq = "&gt;"; seqLen = 4; break;
                case '"':  escSeq = "&quot;"; seqLen = 6; break;
                case '\'': escSeq = "&apos;"; seqLen = 6; break;
                default:   escCharBuf[0] = c; escCharBuf[1] = '\0'; escSeq = escCharBuf; break;
                }
                appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, escSeq, seqLen);
            }

            appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, "</text>\n", strlen("</text>\n"));
        }

        void DrawTooltip(ImVec2 pos, std::string_view text) override
        {
            if (text.empty()) return;

            const uint32_t bgColorVal = 0xE0FFFFE0;
            const uint32_t textColorVal = 0xFF000000;
            const uint32_t borderColorVal = 0xFFCCCCCC;
            const float padding = 5.0f;
            const float defaultTooltipFontSize = 12.0f;
            const char* defaultTooltipFontFamily = "sans-serif";

            ImVec2 textDim = { 0.0f, 0.0f };
            if (textMeasureFunc) 
            {
                textDim = textMeasureFunc(text, nullptr, defaultTooltipFontSize, -1.f);
            }
            else 
            {
                textDim = ImVec2{ static_cast<float>(text.length()) * defaultTooltipFontSize * 0.6f, defaultTooltipFontSize };
            }

            float rectX = pos.x;
            float rectY = pos.y;
            float rectW = textDim.x + 2 * padding;
            float rectH = textDim.y + 2 * padding;

            float textXPos = pos.x + padding;
            float textYPos = pos.y + padding + textDim.y * 0.8f;

            char bgColorBuf[64], textColorBuf[64], borderColorBuf[64];
            formatColorToSvg(bgColorBuf, sizeof(bgColorBuf), bgColorVal);
            formatColorToSvg(textColorBuf, sizeof(textColorBuf), textColorVal);
            formatColorToSvg(borderColorBuf, sizeof(borderColorBuf), borderColorVal);

            int initialOffset = 0;
            initialOffset += snprintf(scratchBuffer + initialOffset, scratchBufferSize - initialOffset, "  <g>\n");
            initialOffset += snprintf(scratchBuffer + initialOffset, scratchBufferSize - initialOffset,
                "    <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" rx=\"3\" ry=\"3\" fill=\"%s\" stroke=\"%s\" stroke-width=\"1\" />\n",
                rectX, rectY, rectW, rectH, bgColorBuf, borderColorBuf);
            appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, scratchBuffer, initialOffset);

            int textPartOffset = snprintf(scratchBuffer, scratchBufferSize,
                "    <text x=\"%.2f\" y=\"%.2f\" font-family=\"%s\" font-size=\"%.0fpx\" fill=\"%s\">",
                textXPos, textYPos, defaultTooltipFontFamily, defaultTooltipFontSize, textColorBuf);
            if (textPartOffset > 0) appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, scratchBuffer, textPartOffset);

            char escCharBuf[10];
            for (char c : text) 
            {
                const char* escSeq = nullptr; int seqLen = 1;
                switch (c) 
                {
                case '&':  escSeq = "&amp;"; seqLen = 5; break; case '<':  escSeq = "&lt;"; seqLen = 4; break;
                case '>':  escSeq = "&gt;"; seqLen = 4; break; case '"':  escSeq = "&quot;"; seqLen = 6; break;
                case '\'': escSeq = "&apos;"; seqLen = 6; break; default:   escCharBuf[0] = c; escCharBuf[1] = '\0'; escSeq = escCharBuf; break;
                }
                appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, escSeq, seqLen);
            }

            appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, "</text>\n  </g>\n", strlen("</text>\n  </g>\n"));
        }


        float EllipsisWidth(void* fontPtr, float sz) override {
            if (textMeasureFunc) 
            {
                return textMeasureFunc("...", fontPtr, sz, -1.f).x;
            }

            return 3 * sz * 0.6f; // Basic fallback
        }

        bool StartOverlay(int32_t id, ImVec2 pos, ImVec2 size, uint32_t color) override { return true; }
        void EndOverlay() override {}

        void DrawSVG(ImVec2 pos, ImVec2 size, uint32_t color, std::string_view svgContentToEmbed, bool fromFile) override
        {
            if (fromFile) 
            {
                int written = snprintf(scratchBuffer, scratchBufferSize, "  \n", (int)svgContentToEmbed.length(), svgContentToEmbed.data());
                if (written > 0) appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, scratchBuffer, written);
                return;
            }
            if (svgContentToEmbed.empty()) return;

            int writtenOpen = snprintf(scratchBuffer, scratchBufferSize,
                "  <svg x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\">\n",
                pos.x, pos.y, size.x, size.y);
            if (writtenOpen > 0) appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, scratchBuffer, writtenOpen);

            appendStringViewToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, svgContentToEmbed);
            appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, "\n  </svg>\n", strlen("\n  </svg>\n"));
        }

        void DrawImage(ImVec2 pos, ImVec2 size, std::string_view filePathOrDataUri) override
        {
            if (size.x <= 0.001f || size.y <= 0.001f || filePathOrDataUri.empty()) return;
            size.x = std::max(0.0f, size.x); size.y = std::max(0.0f, size.y);

            int written = snprintf(scratchBuffer, scratchBufferSize,
                "  <image x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" xlink:href=\"%.*s\" />\n",
                pos.x, pos.y, size.x, size.y,
                (int)filePathOrDataUri.length(), filePathOrDataUri.data());
            if (written > 0) 
            {
                appendToBuffer(mainSvgBuffer, mainSvgBufferOffset, svgMainBufferSize, scratchBuffer, written);
            }
        }
    };

#pragma endregion

    IRenderer* CreateDeferredRenderer(TextMeasureFuncT tmfunc)
    {
        static thread_local DeferredRenderer renderer{ tmfunc };
        return &renderer;
    }

    IRenderer* CreateImGuiRenderer()
    {
        static thread_local ImGuiRenderer renderer{};
        return &renderer;
    }

    IRenderer* CreateSVGRenderer(TextMeasureFuncT tmfunc, ImVec2 dimensions)
    {
        return new SVGRenderer(tmfunc, dimensions);
    }
}
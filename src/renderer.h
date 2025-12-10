#pragma once

#include "types.h"

#include "im_font_manager.h"

namespace glimmer
{
    // Implement this to draw primitives in your favorite graphics API
    // TODO: Separate gradient creation vs. drawing
    struct IRenderer
    {
        void* UserData = nullptr;
        ImVec2 size{ 0.f, 0.f };

        virtual void SetClipRect(ImVec2 startpos, ImVec2 endpos, bool intersect = true) = 0;
        virtual void ResetClipRect() = 0;

        virtual void DrawLine(ImVec2 startpos, ImVec2 endpos, uint32_t color, float thickness = 1.f) = 0;
        virtual void DrawPolyline(ImVec2* points, int sz, uint32_t color, float thickness) = 0;
        virtual void DrawTriangle(ImVec2 pos1, ImVec2 pos2, ImVec2 pos3, uint32_t color, bool filled, float thickness = 1.f) = 0;
        virtual void DrawRect(ImVec2 startpos, ImVec2 endpos, uint32_t color, bool filled, float thickness = 1.f) = 0;
        virtual void DrawRoundedRect(ImVec2 startpos, ImVec2 endpos, uint32_t color, bool filled, float topleftr, float toprightr, float bottomrightr, float bottomleftr, float thickness = 1.f) = 0;
        virtual void DrawRectGradient(ImVec2 startpos, ImVec2 endpos, uint32_t colorfrom, uint32_t colorto, Direction dir) = 0;
        virtual void DrawRoundedRectGradient(ImVec2 startpos, ImVec2 endpos, float topleftr, float toprightr, float bottomrightr, float bottomleftr,
            uint32_t colorfrom, uint32_t colorto, Direction dir) = 0;
        virtual void DrawPolygon(ImVec2* points, int sz, uint32_t color, bool filled, float thickness = 1.f) = 0;
        virtual void DrawPolyGradient(ImVec2* points, uint32_t* colors, int sz) = 0;
        virtual void DrawCircle(ImVec2 center, float radius, uint32_t color, bool filled, float thickness = 1.f) = 0;
        virtual void DrawSector(ImVec2 center, float radius, int start, int end, uint32_t color, bool filled, bool inverted, float thickness = 1.f) = 0;
        virtual void DrawRadialGradient(ImVec2 center, float radius, uint32_t in, uint32_t out, int start, int end) = 0;

        virtual bool SetCurrentFont(std::string_view family, float sz, FontType type) { return false; };
        virtual bool SetCurrentFont(void* fontptr, float sz) { return false; };
        virtual void ResetFont() {};

        virtual ImVec2 GetTextSize(std::string_view text, void* fontptr, float sz, float wrapWidth = -1.f) = 0;
        virtual void DrawText(std::string_view text, ImVec2 pos, uint32_t color, float wrapWidth = -1.f) = 0;
        virtual void DrawTooltip(ImVec2 pos, std::string_view text) = 0;
        virtual float EllipsisWidth(void* fontptr, float sz);

        virtual bool StartOverlay(int32_t id, ImVec2 pos, ImVec2 size, uint32_t color) { return true; }
        virtual void EndOverlay() {}

        virtual bool DrawResource(int32_t resflags, ImVec2 pos, ImVec2 size, uint32_t color, std::string_view content) {}

        virtual void Render(IRenderer& renderer, ImVec2 offset, int from = 0, int to = -1) {}
        virtual int TotalEnqueued() const { return 0; }
        virtual void Reset() {}
    };

    // =============================================================================================
    // IMPLEMENTATIONS
    // =============================================================================================

    using TextMeasureFuncT = ImVec2(*)(std::string_view text, void* fontptr, float sz, float wrapWidth);

    ImVec2 ImGuiMeasureText(std::string_view text, void* fontptr, float sz, float wrapWidth);

    IRenderer* CreateDeferredRenderer(TextMeasureFuncT tmfunc);
    IRenderer* CreateImGuiRenderer();
}

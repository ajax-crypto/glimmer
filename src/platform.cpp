#include "platform.h"
#include "context.h"
#include "renderer.h"

namespace glimmer
{
    int64_t FramesRendered()
    {
        return Config.platform->frameCount;
    }

    IODescriptor::IODescriptor()
    {
        for (auto k = 0; k <= GLIMMER_NKEY_ROLLOVER_MAX; ++k) key[k] = Key_Invalid;
        for (auto ks = 0; ks < (GLIMMER_KEY_ENUM_END - GLIMMER_KEY_ENUM_START + 1); ++ks)
            keyStatus[ks] = ButtonStatus::Default;
    }

    IODescriptor IPlatform::CurrentIO() const
    {
        auto& context = GetContext();
        IODescriptor result{};
        result.deltaTime = desc.deltaTime;

        if (!WidgetContextData::ActivePopUpRegion.Contains(desc.mousepos)) 
            result = desc;

        return result;
    }

    void IPlatform::SetMouseCursor(MouseCursor _cursor)
    {
        cursor = _cursor;
    }

    void IPlatform::EnterFrame()
    {
        auto& io = ImGui::GetIO();
        auto rollover = 0;
        auto escape = false, clicked = false;

        desc.deltaTime = io.DeltaTime;
        desc.mousepos = io.MousePos;
        desc.mouseWheel = io.MouseWheel;
        desc.modifiers = io.KeyMods;
        totalTime += io.DeltaTime;

        for (auto idx = 0; idx < ImGuiMouseButton_COUNT; ++idx)
        {
            desc.mouseButtonStatus[idx] =
                ImGui::IsMouseDown(idx) ? ButtonStatus::Pressed :
                ImGui::IsMouseReleased(idx) ? ButtonStatus::Released :
                ImGui::IsMouseDoubleClicked(idx) ? ButtonStatus::DoubleClicked :
                ButtonStatus::Default;
            clicked = clicked || ImGui::IsMouseDown(idx);
        }

        for (int key = Key_Tab; key != Key_Total; ++key)
        {
            auto imkey = ImGuiKey_NamedKey_BEGIN + key;
            if (ImGui::IsKeyPressed((ImGuiKey)imkey))
            {
                if ((ImGuiKey)imkey == ImGuiKey_CapsLock) desc.capslock = !desc.capslock;
                else if ((ImGuiKey)imkey == ImGuiKey_Insert) desc.insert = !desc.insert;
                else
                {
                    if (rollover < GLIMMER_NKEY_ROLLOVER_MAX)
                        desc.key[rollover++] = (Key)key;
                    desc.keyStatus[key] = ButtonStatus::Pressed;
                    escape = imkey == ImGuiKey_Escape;
                }
            }
            else if (ImGui::IsKeyReleased((ImGuiKey)imkey))
                desc.keyStatus[key] = ButtonStatus::Released;
            else
                desc.keyStatus[key] = ButtonStatus::Default;
        }

        while (rollover <= GLIMMER_NKEY_ROLLOVER_MAX)
            desc.key[rollover++] = Key_Invalid;

        InitFrameData();
        cursor = MouseCursor::Arrow;
    }

    void IPlatform::ExitFrame()
    {
        ++frameCount; ++deltaFrames;
        totalDeltaTime += desc.deltaTime;
        maxFrameTime = std::max(maxFrameTime, desc.deltaTime);
        ImGui::SetMouseCursor((ImGuiMouseCursor)cursor);

        for (auto idx = 0; idx < GLIMMER_NKEY_ROLLOVER_MAX; ++idx)
            desc.key[idx] = Key_Invalid;

        ResetFrameData();

        if (totalDeltaTime > 1.f)
        {
#ifdef _DEBUG
            auto fps = ((float)deltaFrames / totalDeltaTime);
            if (fps >= 50.f)
                LOG("Total Frames: %d | Current FPS: %.0f | Max Frame Time: %.0fms\n", deltaFrames,
                    fps, (maxFrameTime * 1000.f));
            else
                LOGERROR("Total Frames: %d | Current FPS: %.0f | Max Frame Time: %.0fms\n", deltaFrames,
                    fps, (maxFrameTime * 1000.f));
            LOG("*alloc calls in last 1s: %d | Allocated: %d bytes\n", TotalMallocs, AllocatedBytes);
            TotalMallocs = 0;
            AllocatedBytes = 0;
#endif
            maxFrameTime = 0.f;
            totalDeltaTime = 0.f;
            deltaFrames = 0;
        }
    }

    float IPlatform::fps() const
    {
        return (float)frameCount / totalTime;
    }

    UIConfig* IPlatform::config() const
    {
        return &Config;
    }
}

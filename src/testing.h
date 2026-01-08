#pragma once

#ifdef GLIMMER_ENABLE_TESTING

#include <string_view>
#include <cstdio>

#include "context.h"
#include "platform.h"

struct WidgetLoggerRAIIWrapper
{
    template <typename... ArgsT>
    WidgetLoggerRAIIWrapper(ArgsT... args)
    {
        if (glimmer::Config.logger)
            glimmer::Config.logger->StartWidget(args...);
    }

    ~WidgetLoggerRAIIWrapper()
    {
        if (glimmer::Config.logger)
            glimmer::Config.logger->EndWidget();
    }
};

#define WITH_WIDGET_LOG(...) WidgetLoggerRAIIWrapper __logger__{ __VA_ARGS__ }
#define BEGIN_WIDGET_LOG(...) if (glimmer::Config.logger) glimmer::Config.logger->StartWidget(__VA_ARGS__);
#define END_WIDGET_LOG() if (glimmer::Config.logger) glimmer::Config.logger->EndWidget();

#define LOG_RECT(X) if (glimmer::Config.logger) glimmer::Config.logger->Log("\"%s\": { \"min\": { \"x\": %.1f, \"y\": %.1f }, \"max\": { \"x\": %.1f, \"y\": %.1f } },", #X, X.Min.x, X.Min.y, X.Max.x, X.Max.y)
#define LOG_POS(X)  if (glimmer::Config.logger) glimmer::Config.logger->Log("\"%s\": { \"x\": %.1f, \"y\": %.1f },", #X, X.x, X.y)
#define LOG_TEXT(X)  if (glimmer::Config.logger) glimmer::Config.logger->Log("\"%s\": \"%.*s\",", #X, (int)X.size(), X.data())
#define LOG_NUM(X)  if (glimmer::Config.logger) glimmer::Config.logger->Log("\"%s\": %d,", #X, (int)X)
#define LOG_COLOR(X)  if (glimmer::Config.logger) { auto [r, g, b, a] = glimmer::DecomposeColor(X); glimmer::Config.logger->Log("\"%s\": [ %d, %d, %d, %d ],", #X, r, g, b, a); }

#define LOG_RECT2(N, X) if (glimmer::Config.logger) glimmer::Config.logger->Log("\"%s\": { \"min\": { \"x\": %.1f, \"y\": %.1f }, \"max\": { \"x\": %.1f, \"y\": %.1f } },", N, X.Min.x, X.Min.y, X.Max.x, X.Max.y)
#define LOG_POS2(N, X)  if (glimmer::Config.logger) glimmer::Config.logger->Log("\"%s\": { \"x\": %.1f, \"y\": %.1f },", N, X.x, X.y)
#define LOG_TEXT2(N, X)  if (glimmer::Config.logger) glimmer::Config.logger->Log("\"%s\": \"%.*s\",", N, (int)X.size(), X.data())
#define LOG_NUM2(N, X)  if (glimmer::Config.logger) glimmer::Config.logger->Log("\"%s\": %d,", N, (int)X)
#define LOG_COLOR2(N, X)  if (glimmer::Config.logger) { auto [r, g, b, a] = glimmer::DecomposeColor(X); glimmer::Config.logger->Log("\"%s\": [ %d, %d, %d, %d ],", N, r, g, b, a); }

#define LOG_STYLE(X)  if (glimmer::Config.logger) glimmer::Config.logger->LogStyle(X)
#define LOG_STYLE2(STATE, ID) if (glimmer::Config.logger) glimmer::Config.logger->LogStyle(context.GetStyle(STATE, ID))

#define BEGIN_LOG_ARRAY(X) if (glimmer::Config.logger) glimmer::Config.logger->StartArray(X)
#define END_LOG_ARRAY() if (glimmer::Config.logger) glimmer::Config.logger->EndArray()
#define BEGIN_LOG_OBJECT(X) if (glimmer::Config.logger) glimmer::Config.logger->StartObject(X)
#define END_LOG_OBJECT() if (glimmer::Config.logger) glimmer::Config.logger->EndObject()

static const std::unordered_map<int32_t, std::string_view> WS_StateStrings{
    { glimmer::WSI_Default, "default" },
    { glimmer::WSI_Focused, "focused" },
    { glimmer::WSI_Hovered, "hovered" },
    { glimmer::WSI_Pressed, "pressed" },
    { glimmer::WSI_Checked, "checked" },
    { glimmer::WSI_PartiallyChecked, "partial" },
    { glimmer::WSI_Selected, "selected" },
    { glimmer::WSI_Dragged, "dragged" },
    { glimmer::WSI_Disabled, "disabled" },
};

#define LOG_STATE(X) LOG_TEXT2(#X, WS_StateStrings.at(glimmer::log2((unsigned)X)))

namespace glimmer
{
    struct WidgetLogger : public IWidgetLogger
    {
        WidgetLogger(std::string_view path, bool separateFrames);
        ~WidgetLogger();

        void EnterFrame(ImVec2 size);
        void ExitFrame();
        void Finish();

        void StartWidget(WidgetType type, int16_t index, ImRect extent);
        void StartWidget(int32_t id, ImRect extent);
        void EndWidget();

        void Log(std::string_view fmt, ...);
        void LogStyle(const StyleDescriptor& style);
        void StartObject(std::string_view name);
        void EndObject();
        void StartArray(std::string_view name);
        void EndArray();

        void RegisterId(int32_t id, std::string_view name) override;
        void RegisterId(int32_t id, void* ptr) override;

		void* implData = nullptr;
    };

    struct TestPlatform final : public IPlatform
    {
        enum class EventSource
        {
            Mouse, Keyboard, System
        };

        enum class SystemEventType
        {
            Resize, ModeChange, Close, Minimize, Maximize
        };

        struct Event
        {
            EventSource source;
            union
            {
                struct
                {
                    ImVec2 pos;
                    MouseButton button = MouseButton::Total;
                    ButtonStatus status = ButtonStatus::Default;
                    float wheelDelta = 0.f;
                } mouse;
                struct
                {
                    Key key;
                    ButtonStatus status = ButtonStatus::Default;
                    char character;
                } keyboard;
                struct
                {
                    SystemEventType type;
                    struct
                    {
                        int32_t width;
                        int32_t height;
                    } resize;
                } system;
            };
        };

        TestPlatform(ImVec2 size);
        ~TestPlatform();

        bool CreateWindow(const WindowParams& params) override;
        bool PollEvents(bool (*runner)(ImVec2, IPlatform&, void*), void* data) override;
        ImTextureID UploadTexturesToGPU(ImVec2 size, unsigned char* pixels) override;
        void SetClipboardText(std::string_view input) override;
        std::string_view GetClipboardText() override;
        void PushEventHandler(bool (*callback)(void* data, const IODescriptor& desc), void* data) override;

        // Test event APIs
        void PushMouseEvent(ImVec2 pos, MouseButton button, ButtonStatus status, float wheelDelta = 0.f);
        void PushKeyboardEvent(Key key, ButtonStatus status, char character = 0);
        void PushWindowResizeEvent(int32_t width, int32_t height);

        void* implData = nullptr;
    };

    TestPlatform* InitTestPlatform(ImVec2 size = { -1.f, -1.f });

    // TODO: Implement scenarios
    struct TestScenarioBuilder
    {
        TestScenarioBuilder(TestPlatform* p);

        void Click(std::string_view id);
        void Hover(std::string_view id);
        void Edited(std::string_view id, std::string_view text);

        TestPlatform* platform = nullptr;
    };
}

#else

#define WITH_WIDGET_LOG(...) 
#define BEGIN_WIDGET_LOG(...)
#define END_WIDGET_LOG()

#define LOG_RECT(X)
#define LOG_POS(X)
#define LOG_TEXT(X)
#define LOG_NUM(X)
#define LOG_COLOR(X)

#define LOG_RECT2(N, X)
#define LOG_POS2(N, X) 
#define LOG_TEXT2(N, X) 
#define LOG_NUM2(N, X)
#define LOG_COLOR2(N, X)

#define LOG_STYLE(X)
#define LOG_STYLE2(STATE, ID)

#define BEGIN_LOG_ARRAY(X)
#define END_LOG_ARRAY()
#define BEGIN_LOG_OBJECT(X)
#define END_LOG_OBJECT()

#define LOG_STATE(X)

#endif

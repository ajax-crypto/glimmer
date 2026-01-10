#include "testing.h"

#ifdef GLIMMER_ENABLE_TESTING

#include "libs/inc/json/json.hpp"

#include <unordered_map>
#include <charconv>
#include <string>
#include <deque>
#include <varargs.h>

#include "utils.h"

namespace glimmer
{
#pragma region TestPlatform implementation

    struct TestPlatformData
    {
        bool (*runner)(ImVec2, IPlatform&, void*) = nullptr;
        void* data = nullptr;
        std::string clipboard;
        std::deque<TestPlatform::Event> eventQueue;
        std::vector<std::pair<void*, bool(*)(void*, const IODescriptor&)>> handlers;
        WindowParams wparams;
    };

    TestPlatform::TestPlatform(ImVec2 size)
    {
        implData = new TestPlatformData{};
        auto& d = *(TestPlatformData*)implData;
        d.wparams.size = size;
        DetermineInitialKeyStates(desc);
    }

    TestPlatform::~TestPlatform()
    {
        auto d = (TestPlatformData*)implData;
        delete d;
    }

    bool TestPlatform::CreateWindow(const WindowParams& params)
    {
        auto& d = *(TestPlatformData*)implData;
        d.wparams = params;
        return true;
    }

    bool TestPlatform::PollEvents(bool (*runner)(ImVec2, IPlatform&, void*), void* data)
    {
        auto& d = *(TestPlatformData*)implData;
        d.runner = runner;
        d.data = data;
        return true;
    }

    ImTextureID TestPlatform::UploadTexturesToGPU(ImVec2 size, unsigned char* pixels)
    {
        assert(pixels != nullptr);
        return -1;
    }

    void TestPlatform::SetClipboardText(std::string_view input)
    {
        auto& d = *(TestPlatformData*)implData;
        d.clipboard = input;
    }

    std::string_view TestPlatform::GetClipboardText()
    {
        auto& d = *(TestPlatformData*)implData;
        return d.clipboard;
    }

    void TestPlatform::PushEventHandler(bool (*callback)(void* data, const IODescriptor& desc), void* data)
    {
        auto& d = *(TestPlatformData*)implData;
        d.handlers.emplace_back(data, callback);
    }

    void TestPlatform::PushMouseMoveEvent(ImVec2 pos)
    {
        auto& d = *(TestPlatformData*)implData;
        Event ev{};
        ev.source = EventSource::Mouse;
        ev.mouse.pos = pos;
        ev.mouse.wheelDelta = 0.f;
        d.eventQueue.push_back(ev);
    }

    void TestPlatform::PushMouseButtonEvent(ImVec2 pos, MouseButton button, ButtonStatus status)
    {
        auto& d = *(TestPlatformData*)implData;
        Event ev{};
        ev.source = EventSource::Mouse;
        ev.mouse.pos = pos;
        ev.mouse.button = button;
        ev.mouse.status = status;
        ev.mouse.wheelDelta = 0.f;
        d.eventQueue.push_back(ev);
    }

    void TestPlatform::PushMouseClickEvent(ImVec2 pos, MouseButton button)
    {
        auto& d = *(TestPlatformData*)implData;
        Event ev{};
        ev.source = EventSource::Mouse;
        ev.mouse.pos = pos;
        ev.mouse.button = button;
        ev.mouse.status = ButtonStatus::Released;
        ev.mouse.wheelDelta = 0.f;
        d.eventQueue.push_back(ev);
    }

    void TestPlatform::PushMouseWheelEvent(ImVec2 pos, float delta)
    {
        auto& d = *(TestPlatformData*)implData;
        Event ev{};
        ev.source = EventSource::Mouse;
        ev.mouse.pos = pos;
        ev.mouse.button = MouseButton::Total;
        ev.mouse.status = ButtonStatus::Default;
        ev.mouse.wheelDelta = delta;
        d.eventQueue.push_back(ev);
    }

    void TestPlatform::PushKeyboardEvent(Key key, ButtonStatus status, KeyboardModifiers modifiers)
    {
        auto& d = *(TestPlatformData*)implData;
        Event ev{};
        ev.source = EventSource::Keyboard;
        ev.keyboard.key = key;
        ev.keyboard.status = status;
        ev.keyboard.modifiers = modifiers;
        d.eventQueue.push_back(ev);
    }

    void TestPlatform::PushKeyboardEvent(Key* key, int count, ButtonStatus status, KeyboardModifiers modifiers)
    {
        for (auto idx = 0; idx < count; ++idx)
            PushKeyboardEvent(key[idx], status, modifiers);
    }

    void TestPlatform::PushWindowResizeEvent(int32_t width, int32_t height)
    {
        auto& d = *(TestPlatformData*)implData;
        Event ev{};
        ev.source = EventSource::System;
        ev.system.type = SystemEventType::Resize;
        ev.system.resize.width = width;
        ev.system.resize.height = height;
        d.eventQueue.push_back(ev);
    }

    void TestPlatform::PushCustomEvent(const CustomEventData& event)
    {
        auto& d = *(TestPlatformData*)implData;
        Event ev{};
        ev.source = EventSource::Custom;
        ev.custom = event;
        d.eventQueue.push_back(ev);
    }

    std::pair<int, bool> TestPlatform::NextFrame(int count)
    {
        auto& d = *(TestPlatformData*)implData;
        int done = 0;
        bool completed = false;

        while (done < count && !d.eventQueue.empty() && !completed)
        {
            auto& ev = d.eventQueue.front();
            auto width = d.wparams.size.x, height = d.wparams.size.y;
            desc.deltaTime = 1000.f / (float)d.wparams.targetFPS;

            switch (ev.source)
            {
            case EventSource::Mouse:
                desc.mousepos = ev.mouse.pos;

                for (auto idx = 0; idx < (int)MouseButton::Total; ++idx)
                {
                    if (idx != (int)ev.mouse.button)
                        desc.mouseButtonStatus[idx] = ButtonStatus::Default;
                }

                if (ev.mouse.button != MouseButton::Total)
                    desc.mouseButtonStatus[(int)ev.mouse.button] = ev.mouse.status;

                desc.mouseWheel = ev.mouse.wheelDelta;
                break;
            case EventSource::Keyboard:
                desc.keyStatus[(int)ev.keyboard.key] = ev.keyboard.status;
                if (ev.keyboard.modifiers.ctrl) desc.modifiers |= CtrlKeyMod;
                if (ev.keyboard.modifiers.shift) desc.modifiers |= ShiftKeyMod;
                if (ev.keyboard.modifiers.alt) desc.modifiers |= AltKeyMod;
                if (ev.keyboard.modifiers.super) desc.modifiers |= SuperKeyMod;

                if (ev.keyboard.status == ButtonStatus::Pressed)
                {
                    for (auto idx = 0; idx < GLIMMER_NKEY_ROLLOVER_MAX; ++idx)
                    {
                        if (desc.key[idx] == Key_Invalid)
                        {
                            desc.key[idx] = ev.keyboard.key;
                            break;
                        }
                    }
                }
                break;
            case EventSource::System:
                if (ev.system.type == SystemEventType::Resize)
                {
                    width = (float)ev.system.resize.width;
                    height = (float)ev.system.resize.height;
                }
                break;
            }

            if (EnterFrame(width, height, ev.source == EventSource::Custom ? ev.custom : CustomEventData{}))
            {
                completed = !d.runner(ImVec2{ (float)width, (float)height }, *this, d.data);

                for (auto [data, handler] : d.handlers)
                    completed = !handler(data, desc) && completed;
            }

            ExitFrame();
            d.eventQueue.pop_front();
            done++;
        }

        return { done, completed };
    }

    TestPlatform* InitTestPlatform(ImVec2 size)
    {
        return new TestPlatform{ size };
    }

#pragma endregion

#pragma region Widget JSON Recorder

    struct JsonWriter
    {
        char* buffer = nullptr;
        size_t capacity = 0;
        size_t offset = 0;

        template <typename... Args>
        void print(const char* fmt, Args... args)
        {
            if (offset < capacity)
            {
                int written = std::snprintf(buffer + offset, capacity - offset, fmt, args...);
                if (written > 0) offset += static_cast<size_t>(written);
            }
        }

        void printColor(uint32_t col)
        {
            auto [r, g, b, a] = DecomposeColor(col);
            print("[%d, %d, %d, %d]", r, g, b, a);
        }

        void printVec2(const ImVec2& v)
        {
            print("{\"x\": %.1f, \"y\": %.1f}", v.x, v.y);
        }

        void printMeasure(const FourSidedMeasure& m)
        {
            print("{\"top\": %.1f, \"right\": %.1f, \"bottom\": %.1f, \"left\": %.1f}",
                m.top, m.right, m.bottom, m.left);
        }

        void printBorder(const Border& b)
        {
            print("{\"color\": ");
            printColor(b.color);
            print(", \"thickness\": %.1f, \"lineType\": %d}", b.thickness, (int)b.lineType);
        }

        void printRect(const ImRect& rect)
        {
            print("{ \"from\": { \"x\": %.1f, \"y\": %.1f }, \"to\": { \"x\": %.1f, \"y\": %.1f } }",
                rect.Min.x, rect.Min.y, rect.Max.x, rect.Max.y);
        }
    };

    enum class CurrentLogContext
    {
        None,
        Frame,
        Widget,
        Object,
        Array
    };

    struct WidgetLoggerData
    {
        FILE* fptr = nullptr;
        int frames = 0;

        std::string_view _path;
        std::string json[2];
        bool _separateFrames = false;
        bool _hasDiff = false;

        FixedSizeStack<CurrentLogContext, 256> context{ CurrentLogContext::None };
        std::unordered_map<int32_t, std::pair<void*, std::string_view>> idmap;
        std::unordered_map<std::string_view, int32_t> namedIds;
        std::unordered_map<void*, int32_t> outptrIds;
        std::unordered_map<int32_t, ImRect> bounds;

        WidgetLoggerData(std::string_view path, bool separateFrames, bool hasDiff)
			: _path{ path }, _separateFrames{ separateFrames }, _hasDiff{ hasDiff }
        { 
            if (!separateFrames)
            {
#ifdef _WIN32
                fopen_s(&fptr, path.data(), "w");
#else
                d.fptr = std::fopen(path.data(), "w");
#endif
                std::fprintf(fptr, "[ ");
            }
        }

        void EnterFrame()
        {
            auto& d = *this;
            d.json[0] = d.json[1];
            d.json[1].clear();

            if (d._separateFrames)
            {
                static char buffer[_MAX_PATH * 2];
                auto sz = std::min(_MAX_PATH - 1, (int)d._path.size());
                memset(buffer, 0, _MAX_PATH);
                memcpy(buffer, d._path.data(), sz);
                buffer[sz] = '/';
                auto buf = std::to_chars(buffer + sz + 1, buffer + _MAX_PATH, d.frames).ptr;
                memcpy(buf, ".json", 5);
#ifdef _WIN32
                fopen_s(&d.fptr, buffer, "w");
#else
                d.fptr = std::fopen(buffer, "w");
#endif
                WriteData("[ ");
            }
            else
                std::fprintf(d.fptr, "{ \"frame\": %d, \"contents\": [ ", d.frames);

            d.context.push() = CurrentLogContext::Frame;
        }

        void ExitFrame()
        {
            auto& d = *this;
            d.context.pop(1, true);

            if (d.fptr)
            {
                Rewind(1);

                if (d._separateFrames)
                {
                    WriteData(" ]");
                    std::fclose(d.fptr);
                }
                else
                    std::fprintf(d.fptr, " ] },");
            }

            ++d.frames;

            if (d.frames > 1)
            {
                // Diff the json and dump in separate file
                using namespace nlohmann;
                basic_json parent;
                parent["from"] = d.frames - 1;
                parent["to"] = d.frames;
                parent["diff"] = json::diff(json::parse(json[0]), json::parse(json[1]));

                static char buffer[_MAX_PATH * 2];
                auto sz = std::min(_MAX_PATH - 1, (int)d._path.size());
                memset(buffer, 0, _MAX_PATH);
                memcpy(buffer, d._path.data(), sz);
                buffer[sz] = '/';
                auto buf = std::to_chars(buffer + sz + 1, buffer + _MAX_PATH, d.frames - 1).ptr;
                *buf = 'x'; buf++;
                buf = std::to_chars(buf, buffer + _MAX_PATH, d.frames).ptr;
                memcpy(buf, ".json", 5);

                FILE* diffptr = nullptr;
#ifdef _WIN32
                fopen_s(&diffptr, buffer, "w");
#else
                diffptr = std::fopen(buffer, "w");
#endif

                auto out = parent.dump(4);
                std::fprintf(diffptr, out.data());
                std::fclose(diffptr);
            }
        }

        void WriteData(std::string_view content)
        {
            if (_hasDiff)
            {
				if (json[1].empty()) json[1].reserve(1 << 16);
                json[1].append(content);
            }

			std::fprintf(fptr, "%.*s", (int)content.size(), content.data());
        }

        void Rewind(int offset)
        {
            if (_hasDiff)
            {
                if (offset < (int)json[1].size())
                    json[1].resize(json[1].size() - offset);
			}

			std::fseek(fptr, -offset, SEEK_CUR);
        }
    };

    WidgetLogger::WidgetLogger(std::string_view path, bool separateFrames)
    {
		implData = new WidgetLoggerData{ path, separateFrames, separateFrames };
    }

    WidgetLogger::~WidgetLogger()
    {
        delete (WidgetLoggerData*)implData;
    }

    void WidgetLogger::EnterFrame(ImVec2 size)
    {
        auto& d = *(WidgetLoggerData*)implData;
        d.EnterFrame();
    }

    void WidgetLogger::ExitFrame()
    {
        auto& d = *(WidgetLoggerData*)implData;
        d.ExitFrame();
    }

    void WidgetLogger::Finish()
    {
        auto& d = *(WidgetLoggerData*)implData;
        d.context.clear(true);

        if (!d._separateFrames)
        {
            d.Rewind(1);
            d.WriteData(" ]");
        }
    }

    void WidgetLogger::StartWidget(WidgetType type, int16_t index, ImRect extent)
    {
        auto& d = *(WidgetLoggerData*)implData;

        static char buffer[512];
        memset(buffer, 0, 512);
        JsonWriter w{ buffer, 511 };
        auto id = (type << WidgetTypeBits) | (int32_t)index;

        w.print("{ \"widget\": { ");
        w.print("\"info\": { \"type\": \"%.*s\", \"count\": %d",
            (int)Config.widgetNames[type].size(), Config.widgetNames[type].data(), index);

        if (d.idmap.count(id) != 0)
        {
            auto val = d.idmap.at(id);
            if (val.first != nullptr)
                w.print(", \"name\": \"%p\" },", val.first);
            else
                w.print(", \"name\": \"%.*s\" },", (int)val.second.size(), val.second.data());
        }
        else w.print(" },");

        w.print("\"geometry\": ");
        w.printRect(extent);
        w.print(" }, \"details\": { ");
        d.WriteData(buffer);

        d.context.push() = CurrentLogContext::Widget;
        d.bounds[id] = extent;
    }

    void WidgetLogger::StartWidget(int32_t id, ImRect extent)
    {
        auto wtype = (WidgetType)(id >> WidgetTypeBits);
        auto index = id & WidgetIndexMask;
        StartWidget(wtype, index, extent);
    }

    void WidgetLogger::EndWidget()
    {
        auto& d = *(WidgetLoggerData*)implData;
        d.context.pop(1, true);
        d.Rewind(1);
        d.WriteData(" } },");
    }

    void WidgetLogger::Log(std::string_view fmt, ...)
    {
        static char buffer[1 << 12];
        memset(buffer, 0, 1 << 12);
        auto& d = *(WidgetLoggerData*)implData;
        va_list va;
        va_start(va, fmt);
        std::vsnprintf(buffer, 1 << 12 - 1, fmt.data(), va);
        va_end(va);

        d.WriteData(buffer);
    }

    void WidgetLogger::LogStyle(const StyleDescriptor& style)
    {
        auto& d = *(WidgetLoggerData*)implData;

        constexpr int bufsz = 1 << 12;
        static char buffer[bufsz];
        memset(buffer, 0, bufsz);
        JsonWriter w{ buffer, bufsz - 1 };

        w.print("\"style\": {");

        // Primitive fields
        w.print("\"specified\": %llu, ", style.specified);

        w.print("\"bgcolor\": ");
        w.printColor(style.bgcolor);
        w.print(", ");

        w.print("\"fgcolor\": ");
        w.printColor(style.fgcolor);
        w.print(", ");

        if (style.dimension != ImVec2{ -1.f, -1.f })
        {
            w.print("\"dimension\": ");
            w.printVec2(style.dimension);
            w.print(", ");
        }

        if (style.mindim != ImVec2{})
        {
            w.print("\"mindim\": ");
            w.printVec2(style.mindim);
            w.print(", ");
        }

        if (style.maxdim != ImVec2{ FLT_MAX, FLT_MAX })
        {
            w.print("\"maxdim\": ");
            w.printVec2(style.maxdim);
            w.print(", ");
        }

        w.print("\"alignment\": %d, ", style.alignment);
        w.print("\"relativeProps\": %u, ", style.relativeProps);

        // Measures
        w.print("\"padding\": ");
        w.printMeasure(style.padding);
        w.print(", ");

        w.print("\"margin\": ");
        w.printMeasure(style.margin);
        w.print(", ");

        // FourSidedBorder
        w.print("\"border\": {");

        if (style.border.top.thickness > 0.f)
        {
            w.print("\"top\": "); w.printBorder(style.border.top); w.print(", ");
        }

        if (style.border.right.thickness > 0.f)
        {
            w.print("\"right\": "); w.printBorder(style.border.right); w.print(", ");
        }

        if (style.border.bottom.thickness > 0.f)
        {
            w.print("\"bottom\": "); w.printBorder(style.border.bottom); w.print(", ");
        }

        if (style.border.left.thickness > 0.f)
        {
            w.print("\"left\": "); w.printBorder(style.border.left); w.print(", ");
        }

        w.print("\"cornerRadius\": [%.1f, %.1f, %.1f, %.1f] ",
            style.border.cornerRadius[0], style.border.cornerRadius[1],
            style.border.cornerRadius[2], style.border.cornerRadius[3]);
        w.print(" }, ");

        // FontStyle
        w.print("\"font\": {");
        // Use %.*s for string_view to ensure we don't read past end if not null-terminated
        w.print("\"family\": \"%.*s\", ", (int)style.font.family.size(), style.font.family.data());
        w.print("\"size\": %.1f, ", style.font.size);
        w.print("\"flags\": %d", style.font.flags);
        w.print(" }");

        // BoxShadow
        if (style.shadow.spread > 0.f)
        {
            w.print("\", shadow\": {");
            w.print("\"offset\": "); w.printVec2(style.shadow.offset); w.print(", ");
            w.print("\"spread\": %.1f, ", style.shadow.spread);
            w.print("\"blur\": %.1f, ", style.shadow.blur);
            w.print("\"color\": "); w.printColor(style.shadow.color);
            w.print(" }");
        }

        // ColorGradient
        if (style.gradient.totalStops > 0)
        {
            w.print("\", gradient\": {");
            w.print("\"totalStops\": %d, ", style.gradient.totalStops);
            w.print("\"angleDegrees\": %.1f, ", style.gradient.angleDegrees);
            w.print("\"dir\": %d, ", (int)style.gradient.dir);
            w.print("\"colorStops\": [");
            for (int i = 0; i < GLIMMER_MAX_COLORSTOPS; ++i) {
                const auto& stop = style.gradient.colorStops[i];
                w.print("{\"from\": "); w.printColor(stop.from);
                w.print(", \"to\": "); w.printColor(stop.to);
                w.print(", \"pos\": %.1f}", stop.pos);
                if (i < GLIMMER_MAX_COLORSTOPS - 1) w.print(", ");
            }
            w.print(" ] }");
        }

        w.print(" } ");
        d.WriteData(buffer);
    }

    void WidgetLogger::StartObject(std::string_view name)
    {
        auto& d = *(WidgetLoggerData*)implData;
		if (d.context.size() > 0 && d.context.top() == CurrentLogContext::Array)
            d.WriteData("{ ");
        else
            Log("\"%.*s\": { ", (int)name.size(), name.data());
        
        d.context.push() = CurrentLogContext::Object;
    }

    void WidgetLogger::EndObject()
    {
        auto& d = *(WidgetLoggerData*)implData;
        d.context.pop(1, true);
        d.Rewind(1);
        d.WriteData(" },");
    }

    void WidgetLogger::StartArray(std::string_view name)
    {
        auto& d = *(WidgetLoggerData*)implData;
		
        if (d.context.size() > 0 && d.context.top() == CurrentLogContext::Array)
            d.WriteData("[ ");
        else
            Log("\"%.*s\": [ ", (int)name.size(), name.data());

        d.context.push() = CurrentLogContext::Array;
    }

    void WidgetLogger::EndArray()
    {
        auto& d = *(WidgetLoggerData*)implData;
		d.context.pop(1, true);
        d.Rewind(1);
        d.WriteData(" ],");
    }

    void WidgetLogger::RegisterId(int32_t id, std::string_view name)
    {
        auto& d = *(WidgetLoggerData*)implData;
        d.idmap[id].second = name;
        d.idmap[id].first = nullptr;
        d.namedIds[name] = id;
    }

    void WidgetLogger::RegisterId(int32_t id, void* ptr)
    {
        auto& d = *(WidgetLoggerData*)implData;
        d.idmap[id].first = ptr;
        d.outptrIds[ptr] = id;
    }

    ImRect WidgetLogger::Bounds(std::string_view id) const
    {
        auto& d = *(WidgetLoggerData*)implData;
        auto nid = d.namedIds.at(id);
        return d.bounds.at(nid);
    }

    ImRect WidgetLogger::Bounds(int32_t id) const
    {
        auto& d = *(WidgetLoggerData*)implData;
        return d.bounds.at(id);
    }

    ImRect WidgetLogger::Bounds(void* outptr) const
    {
        auto& d = *(WidgetLoggerData*)implData;
        auto nid = d.outptrIds.at(outptr);
        return d.bounds.at(nid);
    }

#pragma endregion

#pragma region Test Scenarios

    static std::pair<Key, bool> CharToKey(char c)
    {
        if (c >= 'a' && c <= 'z') return { (Key)(Key_A + (c - 'a')), false };
        if (c >= 'A' && c <= 'Z') return { (Key)(Key_A + (c - 'A')), true }; // Shift
        if (c >= '0' && c <= '9') return { (Key)(Key_0 + (c - '0')), false };
        return { Key_Invalid, false };
    }

    void TestScenario::Run()
    {
        auto* widgetLogger = dynamic_cast<WidgetLogger*>(Config.logger);

        if (!platform || !widgetLogger) return;

        // 1. Process Actions
        for (const auto& action : actions)
        {
            ImVec2 center{ 0.f, 0.f };
            bool hasBounds = !action.id.empty();

            if (hasBounds)
            {
                ImRect bounds = action.outptr ? widgetLogger->Bounds(action.outptr) : 
                    widgetLogger->Bounds(action.id);
                center = { bounds.Min.x + (bounds.GetWidth() * action.relpos.x),
                    bounds.Min.y + (bounds.GetHeight() * action.relpos.y) };
            }

            switch (action.type)
            {
            case ActionType::Click:
                if (hasBounds) 
                {
                    platform->PushMouseMoveEvent(center);
                    platform->PushMouseClickEvent(center, action.button);
                }
                break;
            case ActionType::Hover:
                if (hasBounds) 
                {
                    platform->PushMouseMoveEvent(center);
                }
                break;
            case ActionType::Edit:
                if (hasBounds) 
                {
                    // Focus first
                    platform->PushMouseMoveEvent(center);
                    platform->PushMouseClickEvent(center, MouseButton::LeftMouseButton);

                    // Type text
                    for (char c : action.text)
                    {
                        auto [key, shift] = CharToKey(c);
                        if (key != Key_Invalid)
                        {
                            TestPlatform::KeyboardModifiers mods{ 0, (unsigned)shift, 0, 0 };
                            platform->PushKeyboardEvent(key, ButtonStatus::Pressed, mods);
                            platform->PushKeyboardEvent(key, ButtonStatus::Released, mods);
                        }
                    }
                }
                break;
            case ActionType::MouseWheel:
                if (hasBounds) 
                {
                    platform->PushMouseMoveEvent(center);
                    platform->PushMouseWheelEvent(center, action.wheelDelta);
                }
                break;
            case ActionType::KeyPress:
                // Keyboard events don't strictly require mouse position if focused, 
                // but usually follow previous interaction
                platform->PushKeyboardEvent(action.key, ButtonStatus::Pressed, action.modifiers);
                platform->PushKeyboardEvent(action.key, ButtonStatus::Released, action.modifiers);
                break;
            }
        }

        // 2. Next Frame
        platform->NextFrame(framesToAdvance);

        // 3. Process Assertions
        auto logData = (WidgetLoggerData*)widgetLogger->implData;
        if (logData->json[1].empty()) return;

        using json = nlohmann::json;
        json currentFrame;

        try {
            currentFrame = json::parse(logData->json[1]);
        }
        catch (...) {
            failures.push_back("{ \"error\": \"Failed to parse frame JSON\" }");
            return;
        }

        for (const auto& assert : assertions)
        {
            // Convert dot notation "button.state" to json pointer "/button/state"
            std::string pathStr = "/";
            for (char c : assert.prop) pathStr += (c == '.' ? '/' : c);

            json::json_pointer ptr(pathStr);

            if (!currentFrame.contains(ptr))
            {
                failures.push_back(json
                {
                    {"assertion", assert.prop},
                    {"error", "Property not found"}
                }.dump());
                continue;
            }

            auto& val = currentFrame[ptr];
            bool failed = false;
            json expectedJson;
            json actualJson = val;

            if (assert.type == AssertType::Int)
            {
                if (!val.is_number_integer() || val.get<int64_t>() != assert.i_val) failed = true;
                expectedJson = assert.i_val;
            }
            else if (assert.type == AssertType::UInt)
            {
                if (!val.is_number_integer() || val.get<uint64_t>() != assert.u_val) failed = true;
                expectedJson = assert.u_val;
            }
            else if (assert.type == AssertType::Float)
            {
                if (!val.is_number() || std::abs(val.get<float>() - assert.f_val) > 0.001f) failed = true;
                expectedJson = assert.f_val;
            }
            else // String
            {
                if (!val.is_string() || val.get<std::string>() != assert.s_val) failed = true;
                expectedJson = assert.s_val;
            }

            if (failed)
            {
                json failureRecord;
                failureRecord["assertion"] = assert.prop;
                failureRecord["expected"] = expectedJson;
                failureRecord["actual"] = actualJson;
                failures.push_back(failureRecord.dump());
            }
        }
    }

    TestScenarioBuilder::TestScenarioBuilder(TestPlatform* p)
    {
        scenario.platform = p;
    }

    TestScenarioBuilder& TestScenarioBuilder::Create(std::string_view name)
    {
        scenario.name = name;
        return *this;
    }

    TestScenarioBuilder& TestScenarioBuilder::Click(std::string_view id, MouseButton button)
    {
        TestScenario::Action a;
        a.type = TestScenario::ActionType::Click;
        a.id = id;
        a.button = button;
        scenario.actions.push_back(a);
        return *this;
    }

    TestScenarioBuilder& TestScenarioBuilder::Click(void* outptr, MouseButton button)
    {
        TestScenario::Action a;
        a.type = TestScenario::ActionType::Click;
        a.outptr = outptr;
        a.button = button;
        scenario.actions.push_back(a);
        return *this;
    }

    TestScenarioBuilder& TestScenarioBuilder::Hover(std::string_view id, ImVec2 relpos)
    {
        TestScenario::Action a;
        a.type = TestScenario::ActionType::Hover;
        a.id = id;
        a.relpos = relpos;
        scenario.actions.push_back(a);
        return *this;
    }

    TestScenarioBuilder& TestScenarioBuilder::Hover(void* outptr, ImVec2 relpos)
    {
        TestScenario::Action a;
        a.type = TestScenario::ActionType::Hover;
        a.outptr = outptr;
        a.relpos = relpos;
        scenario.actions.push_back(a);
        return *this;
    }

    TestScenarioBuilder& TestScenarioBuilder::Edited(std::string_view id, std::string_view text)
    {
        TestScenario::Action a;
        a.type = TestScenario::ActionType::Edit;
        a.id = id;
        a.text = text;
        scenario.actions.push_back(a);
        return *this;
    }

    TestScenarioBuilder& TestScenarioBuilder::Edited(void* outptr, std::string_view text)
    {
        TestScenario::Action a;
        a.type = TestScenario::ActionType::Edit;
        a.outptr = outptr;
        a.text = text;
        scenario.actions.push_back(a);
        return *this;
    }

    TestScenarioBuilder& TestScenarioBuilder::Scroll(std::string_view id, float delta)
    {
        TestScenario::Action a;
        a.type = TestScenario::ActionType::MouseWheel;
        a.id = id;
        a.wheelDelta = delta;
        scenario.actions.push_back(a);
        return *this;
    }

    TestScenarioBuilder& TestScenarioBuilder::Scroll(void* outptr, float delta)
    {
        TestScenario::Action a;
        a.type = TestScenario::ActionType::MouseWheel;
        a.outptr = outptr;
        a.wheelDelta = delta;
        scenario.actions.push_back(a);
        return *this;
    }

    TestScenarioBuilder& TestScenarioBuilder::Press(Key key, TestPlatform::KeyboardModifiers modifiers)
    {
        TestScenario::Action a;
        a.type = TestScenario::ActionType::KeyPress;
        a.key = key;
        a.modifiers = modifiers;
        scenario.actions.push_back(a);
        return *this;
    }

    TestScenarioBuilder& TestScenarioBuilder::Assert(std::string_view prop, std::string_view value)
    {
        scenario.assertions.push_back({ TestScenario::AssertType::String, prop, value });
        return *this;
    }

    TestScenarioBuilder& TestScenarioBuilder::Assert(std::string_view prop, int64_t value)
    {
        TestScenario::Assertion a;
        a.type = TestScenario::AssertType::Int;
        a.prop = prop;
        a.i_val = value;
        scenario.assertions.push_back(a);
        return *this;
    }

    TestScenarioBuilder& TestScenarioBuilder::Assert(std::string_view prop, uint64_t value)
    {
        TestScenario::Assertion a;
        a.type = TestScenario::AssertType::UInt;
        a.prop = prop;
        a.u_val = value;
        scenario.assertions.push_back(a);
        return *this;
    }

    TestScenarioBuilder& TestScenarioBuilder::Assert(std::string_view prop, float value)
    {
        TestScenario::Assertion a;
        a.type = TestScenario::AssertType::Float;
        a.prop = prop;
        a.f_val = value;
        scenario.assertions.push_back(a);
        return *this;
    }

    TestScenario TestScenarioBuilder::Done(int frames)
    {
        scenario.framesToAdvance = frames;
        TestScenario copy = scenario;

        // Reset original but keep platform
        scenario = TestScenario();
        scenario.platform = copy.platform;

        return copy;
    }

#pragma endregion
}

#endif

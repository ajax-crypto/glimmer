#include "testing.h"

#ifdef GLIMMER_ENABLE_TESTING

#include "libs/inc/json/json.hpp"

#include <unordered_map>
#include <charconv>
#include <string>
#include <deque>
#include <thread>
#include <mutex>
#include <future>
#include <varargs.h>

#include "utils.h"

namespace glimmer
{
#pragma region TestPlatform implementation

    struct TestPlatformData
    {
        std::string clipboard;
        std::deque<TestPlatform::Event> eventQueue;
        std::vector<std::pair<void*, bool(*)(void*, const IODescriptor&)>> handlers;
        WindowParams wparams;
        std::mutex eventMutex;
        std::promise<void> complete;
        bool done = false;
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
        d->done = true;
        d->complete.get_future().wait();
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

        while (!d.done)
        {
            if (d.eventQueue.empty())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            std::lock_guard<std::mutex> lock(d.eventMutex);
            auto& ev = d.eventQueue.front();
            auto width = d.wparams.size.x, height = d.wparams.size.y;

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

            if (EnterFrame(width, height))
            {
                d.done = !runner(ImVec2{ (float)width, (float)height }, *this, data);

                for (auto [data, handler] : d.handlers)
                    d.done = !handler(data, desc) && d.done;
            }

            ExitFrame();
            d.eventQueue.pop_front();
        }

        d.complete.set_value();
        return d.done;
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

    // Add test event APIs

    void TestPlatform::PushMouseEvent(ImVec2 pos, MouseButton button, ButtonStatus status, float wheelDelta)
    {
        auto& d = *(TestPlatformData*)implData;
        Event ev{};
        ev.source = EventSource::Mouse;
        ev.mouse.pos = pos;
        ev.mouse.button = button;
        ev.mouse.status = status;
        ev.mouse.wheelDelta = wheelDelta;

        std::lock_guard<std::mutex> lock(d.eventMutex);
        d.eventQueue.push_back(ev);
    }

    void TestPlatform::PushKeyboardEvent(Key key, ButtonStatus status, char character)
    {
        auto& d = *(TestPlatformData*)implData;
        Event ev{};
        ev.source = EventSource::Keyboard;
        ev.keyboard.key = key;
        ev.keyboard.status = status;
        ev.keyboard.character = character;

        std::lock_guard<std::mutex> lock(d.eventMutex);
        d.eventQueue.push_back(ev);
    }

    void TestPlatform::PushWindowResizeEvent(int32_t width, int32_t height)
    {
        auto& d = *(TestPlatformData*)implData;
        Event ev{};
        ev.source = EventSource::System;
        ev.system.type = SystemEventType::Resize;
        ev.system.resize.width = width;
        ev.system.resize.height = height;

        std::lock_guard<std::mutex> lock(d.eventMutex);
        d.eventQueue.push_back(ev);
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
        int cnt = 0;
        va_list va;
        va_start(va, cnt);
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
    }

    void WidgetLogger::RegisterId(int32_t id, void* ptr)
    {
        auto& d = *(WidgetLoggerData*)implData;
        d.idmap[id].first = ptr;
    }

#pragma endregion
}

#endif

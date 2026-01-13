#include "platform.h"
#include "context.h"
#include "renderer.h"
#include "layout.h"

#include <cstring>
#include <list>
#include <deque>

#include "libs/inc/SDL3/SDL.h"
#include "libs/inc/imguisdl3/imgui_impl_sdl3.h"
#include "libs/inc/imguisdl3/imgui_impl_sdlrenderer3.h"

#ifdef __EMSCRIPTEN__
#include "libs/emscripten/emscripten_mainloop_stub.h"
#endif

#if defined(GLIMMER_ENABLE_NFDEXT) && !defined(__EMSCRIPTEN__)
#include <mutex>
#include "nfd-ext/src/include/nfd.h"
#endif

#ifdef _WIN32
#include <Windows.h>
#undef CreateWindow
#endif

namespace glimmer
{
    static void RegisterKeyBindings()
    {
        KeyMappings.resize(512, { 0, 0 });
        KeyMappings[Key_0] = { '0', ')' }; KeyMappings[Key_1] = { '1', '!' }; KeyMappings[Key_2] = { '2', '@' };
        KeyMappings[Key_3] = { '3', '#' }; KeyMappings[Key_4] = { '4', '$' }; KeyMappings[Key_5] = { '5', '%' };
        KeyMappings[Key_6] = { '6', '^' }; KeyMappings[Key_7] = { '7', '&' }; KeyMappings[Key_8] = { '8', '*' };
        KeyMappings[Key_9] = { '9', '(' };

        KeyMappings[Key_A] = { 'A', 'a' }; KeyMappings[Key_B] = { 'B', 'b' }; KeyMappings[Key_C] = { 'C', 'c' };
        KeyMappings[Key_D] = { 'D', 'd' }; KeyMappings[Key_E] = { 'E', 'e' }; KeyMappings[Key_F] = { 'F', 'f' };
        KeyMappings[Key_G] = { 'G', 'g' }; KeyMappings[Key_H] = { 'H', 'h' }; KeyMappings[Key_I] = { 'I', 'i' };
        KeyMappings[Key_J] = { 'J', 'j' }; KeyMappings[Key_K] = { 'K', 'k' }; KeyMappings[Key_L] = { 'L', 'l' };
        KeyMappings[Key_M] = { 'M', 'm' }; KeyMappings[Key_N] = { 'N', 'n' }; KeyMappings[Key_O] = { 'O', 'o' };
        KeyMappings[Key_P] = { 'P', 'p' }; KeyMappings[Key_Q] = { 'Q', 'q' }; KeyMappings[Key_R] = { 'R', 'r' };
        KeyMappings[Key_S] = { 'S', 's' }; KeyMappings[Key_T] = { 'T', 't' }; KeyMappings[Key_U] = { 'U', 'u' };
        KeyMappings[Key_V] = { 'V', 'v' }; KeyMappings[Key_W] = { 'W', 'w' }; KeyMappings[Key_X] = { 'X', 'x' };
        KeyMappings[Key_Y] = { 'Y', 'y' }; KeyMappings[Key_Z] = { 'Z', 'z' };

        KeyMappings[Key_Apostrophe] = { '\'', '"' }; KeyMappings[Key_Backslash] = { '\\', '|' };
        KeyMappings[Key_Slash] = { '/', '?' }; KeyMappings[Key_Comma] = { ',', '<' };
        KeyMappings[Key_Minus] = { '-', '_' }; KeyMappings[Key_Period] = { '.', '>' };
        KeyMappings[Key_Semicolon] = { ';', ':' }; KeyMappings[Key_Equal] = { '=', '+' };
        KeyMappings[Key_LeftBracket] = { '[', '{' }; KeyMappings[Key_RightBracket] = { ']', '}' };
        KeyMappings[Key_Space] = { ' ', ' ' }; KeyMappings[Key_Tab] = { '\t', '\t' };
        KeyMappings[Key_GraveAccent] = { '`', '~' };
    }

    static std::list<SDL_GPUTextureSamplerBinding> SamplerBindings;

    bool SDLCALL SDL_CustomWindowsMessageHook(void* userdata, MSG* msg)
    {
        if (msg->message == WM_HOTKEY)
        {
            auto& data = ((std::deque<CustomEventData>*)userdata)->emplace_back();
            data.data.hotkey.key = GetGlimmerKey(HIWORD(msg->wParam));
            
            auto modifiers = LOWORD(msg->wParam);
            if (modifiers & MOD_CONTROL) data.data.hotkey.modifiers |= CtrlKeyMod;
            if (modifiers & MOD_SHIFT) data.data.hotkey.modifiers |= ShiftKeyMod;
            if (modifiers & MOD_ALT) data.data.hotkey.modifiers |= AltKeyMod;
            if (modifiers & MOD_WIN) data.data.hotkey.modifiers |= SuperKeyMod;

            SDL_Event event;
            event.type = SDL_EVENT_USER;
            event.user.data1 = userdata;
            SDL_PushEvent(&event);
            return false;
        }

        return true;
    }

    struct ImGuiSDL3SoftwarePlatform final : public IPlatform
    {
        ImGuiSDL3SoftwarePlatform()
        {
            DetermineInitialKeyStates(desc);
        }

        void SetClipboardText(std::string_view input)
        {
            SDL_SetClipboardText(input.data());
        }

        std::string_view GetClipboardText()
        {
            return SDL_GetClipboardText();
        }

        bool CreateWindow(const WindowParams& params)
        {
            if (!SDL_Init(SDL_INIT_VIDEO))
            {
                printf("Error: SDL_Init(): %s\n", SDL_GetError());
                return false;
            }

            float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
            SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_MAXIMIZED;

            window = SDL_CreateWindow(params.title.data(), (int)params.size.x, (int)params.size.y, window_flags);
            if (window == nullptr)
            {
                printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
                return false;
            }

            renderer = SDL_CreateRenderer(window, "software");
            targetFPS = params.targetFPS;

            if (params.targetFPS == -1)
            {
                SDL_SetRenderVSync(renderer, 1);
                auto display = SDL_GetDisplayForWindow(window);
                auto mode = SDL_GetCurrentDisplayMode(display);
                targetFPS = (int)mode->refresh_rate;
            }

            if (renderer == nullptr)
            {
                SDL_Log("Error: SDL_CreateRenderer(): %s\n", SDL_GetError());
                return false;
            }

            if (params.icon.size() > 0)
            {
                SDL_Surface* iconSurface = nullptr;

                if (params.iconType == (RT_PATH | RT_BMP))
                    iconSurface = SDL_LoadBMP(params.icon.data());
                else
                    assert(false);

                if (iconSurface)
                {
                    SDL_SetWindowIcon(window, iconSurface);
                    SDL_DestroySurface(iconSurface);
                }
            }

            SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            if (params.size.x == -1.f || params.size.y == -1.f) SDL_MaximizeWindow(window);
            SDL_ShowWindow(window);

            if (totalCustomEvents > 0)
                SDL_SetWindowsMessageHook(&SDL_CustomWindowsMessageHook, &custom);
            
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            io.IniFilename = nullptr;
            bgcolor[0] = (float)params.bgcolor[0] / 255.f;
            bgcolor[1] = (float)params.bgcolor[1] / 255.f;
            bgcolor[2] = (float)params.bgcolor[2] / 255.f;
            bgcolor[3] = (float)params.bgcolor[3] / 255.f;
            softwareCursor = params.softwareCursor;

#ifdef _DEBUG
            _CrtSetDbgFlag(_CRTDBG_DELAY_FREE_MEM_DF);
#endif //  _DEBUG

            ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
            return true;
        }

        bool PollEvents(bool (*runner)(ImVec2, IPlatform&, void*), void* data)
        {
            ImGui_ImplSDLRenderer3_Init(renderer);

            bool done = false;
#ifdef __EMSCRIPTEN__
            // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
            // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
            io.IniFilename = nullptr;
            EMSCRIPTEN_MAINLOOP_BEGIN
#else
            while (!done)
#endif
            {
                auto resetCustom = false;
                int width = 0, height = 0;
                SDL_GetWindowSize(window, &width, &height);

                SDL_Event event;
                
                while (SDL_PollEvent(&event))
                {
                    ImGui_ImplSDL3_ProcessEvent(&event);
                    if (event.type == SDL_EVENT_QUIT)
                        done = true;
                    else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
                        done = true;
                    else if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_DISPLAY_CHANGED)
                        InvalidateLayout();
                    else if (event.type == SDL_EVENT_USER)
                        resetCustom = true;
                }

                if (done) break;

                // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppIterate() function]
                if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
                {
                    SDL_Delay(10);
                    continue;
                }

                // Start the Dear ImGui frame
                ImGui_ImplSDLRenderer3_NewFrame();
                ImGui_ImplSDL3_NewFrame();

                if (EnterFrame(static_cast<float>(width), static_cast<float>(height), custom.empty() ? CustomEventData{} : custom.front()))
                {
                    done = !runner(ImVec2{ static_cast<float>(width), static_cast<float>(height) }, *this, data);

                    for (auto [data, handler] : handlers)
                        done = !handler(data, desc) && done;
                }

                ExitFrame();

                auto& io = ImGui::GetIO();
                SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
                SDL_SetRenderDrawColorFloat(renderer, bgcolor[0], bgcolor[1], bgcolor[2], bgcolor[3]);
                SDL_RenderClear(renderer);
                ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
                SDL_RenderPresent(renderer);

                if (resetCustom) custom.clear();
            }
#ifdef __EMSCRIPTEN__
            EMSCRIPTEN_MAINLOOP_END;
#endif

            ImGui_ImplSDLRenderer3_Shutdown();
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();

            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
#if defined(GLIMMER_ENABLE_NFDEXT) && !defined(__EMSCRIPTEN__)
            NFD_Quit();
#endif
            SDL_Quit();
            Cleanup();
            return done;
        }

        ImTextureID UploadTexturesToGPU(ImVec2 size, unsigned char* pixels)
        {
            auto texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, (int)size.x, (int)size.y);
            SDL_UpdateTexture(texture, nullptr, pixels, 4 * (int)size.x);
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
            return (ImTextureID)(intptr_t)(texture);
        }

        void PushEventHandler(bool (*callback)(void* data, const IODescriptor& desc), void* data) override
        {
            handlers.emplace_back(data, callback);
        }

#if defined(GLIMMER_ENABLE_NFDEXT) && !defined(__EMSCRIPTEN__)

        void GetWindowHandle(void* out) override
        {
            std::call_once(nfdInitialized, [] { NFD_Init(); });
            auto nativeWindow = (nfdwindowhandle_t*)out;
#if defined(__WIN32__)
            nativeWindow->type = NFD_WINDOW_HANDLE_TYPE_WINDOWS;
            nativeWindow->handle = SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);;
#elif defined(__MACOSX__)
            nativeWindow->type = NFD_WINDOW_HANDLE_TYPE_COCOA;
            nativeWindow->handle = SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
#elif defined(__LINUX__)
            if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "x11") == 0)
            {
                nativeWindow->type = NFD_WINDOW_HANDLE_TYPE_X11;
                nativeWindow->handle = SDL_GetNumberProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);;
            }
#endif
        }

#elif !defined(__EMSCRIPTEN__)

        int32_t ShowFileDialog(std::span<char>* out, int32_t outsz, int32_t target,
            std::string_view location, std::pair<std::string_view, std::string_view>* filters,
            int totalFilters, const DialogProperties& props) override
        {
            struct PathSet
            {
                std::span<char>* out = nullptr;
                int32_t outsz = 0;
                int32_t filled = 0;
                bool done = false;
            };

            static PathSet pathset{};
            static Vector<SDL_DialogFileFilter, int16_t> sdlfilters;

            pathset = PathSet{ out, outsz, 0, false };
            SDL_DialogFileCallback callback = [](void* userdata, const char* const* filelist, int) {
                auto& pathset = *(PathSet*)userdata;
                auto idx = 0;

                while (filelist[idx] != nullptr && idx < pathset.outsz)
                {
                    auto pathsz = strlen(filelist[idx]);

                    if (pathsz < pathset.out[idx].size() - 1)
                    {
                        memcpy(pathset.out[idx].data(), filelist[idx], pathsz);
                        pathset.out[idx][pathsz] = 0;
                    }

                    pathset.filled++;
                    idx++;
                }

                pathset.done = true;
            };

            if ((target & OneFile) || (target & MultipleFiles))
            {
                sdlfilters.clear(true);
                for (auto idx = 0; idx < totalFilters; ++idx)
                {
                    auto& sdlfilter = sdlfilters.emplace_back();
                    sdlfilter.name = filters[idx].first.data();
                    sdlfilter.pattern = filters[idx].second.data();
                }

                auto allowMany = (target & MultipleFiles) != 0;
                auto pset = SDL_CreateProperties();
                SDL_SetPointerProperty(pset, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, sdlfilters.data());
                SDL_SetNumberProperty(pset, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, totalFilters);
                SDL_SetPointerProperty(pset, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, window);
                SDL_SetStringProperty(pset, SDL_PROP_FILE_DIALOG_LOCATION_STRING, location.data());
                SDL_SetBooleanProperty(pset, SDL_PROP_FILE_DIALOG_MANY_BOOLEAN, allowMany);
                if (!props.title.empty())
                    SDL_SetStringProperty(pset, SDL_PROP_FILE_DIALOG_TITLE_STRING, props.title.data());
                SDL_SetStringProperty(pset, SDL_PROP_FILE_DIALOG_ACCEPT_STRING, props.confirmBtnText.data());
                SDL_SetStringProperty(pset, SDL_PROP_FILE_DIALOG_CANCEL_STRING, props.cancelBtnText.data());

                modalDialog = true;
                SDL_ShowFileDialogWithProperties(SDL_FILEDIALOG_OPENFILE, callback, &pathset, pset);
                SDL_DestroyProperties(pset);
            }
            else
            {
                auto allowMany = (target & MultipleDirectories) != 0;
                auto pset = SDL_CreateProperties();
                SDL_SetPointerProperty(pset, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, window);
                SDL_SetStringProperty(pset, SDL_PROP_FILE_DIALOG_LOCATION_STRING, location.data());
                SDL_SetBooleanProperty(pset, SDL_PROP_FILE_DIALOG_MANY_BOOLEAN, allowMany);
                if (!props.title.empty())
                    SDL_SetStringProperty(pset, SDL_PROP_FILE_DIALOG_TITLE_STRING, props.title.data());
                SDL_SetStringProperty(pset, SDL_PROP_FILE_DIALOG_ACCEPT_STRING, props.confirmBtnText.data());
                SDL_SetStringProperty(pset, SDL_PROP_FILE_DIALOG_CANCEL_STRING, props.cancelBtnText.data());

                modalDialog = true;
                SDL_ShowFileDialogWithProperties(SDL_FILEDIALOG_OPENFOLDER, callback, &pathset, pset);
                SDL_DestroyProperties(pset);
            }

            SDL_Event event;
            while (!pathset.done)
            {
                SDL_PollEvent(&event);
                SDL_Delay(10);
            }

            modalDialog = false;
            return pathset.filled;
        }

#else

    int32_t ShowFileDialog(std::string_view* out, int32_t outsz, int32_t target,
        std::string_view location, std::pair<std::string_view, std::string_view>* filters,
        int totalFilters, const DialogProperties& props) override
    {
        return -1;
    }

#endif

        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        std::vector<std::pair<void*, bool(*)(void*, const IODescriptor&)>> handlers;
        std::deque<CustomEventData> custom;
    };

    IPlatform* InitPlatform(ImVec2 size)
    {
        static ImGuiSDL3SoftwarePlatform platform;
        static bool initialized = false;

        if (!initialized)
        {
            initialized = true;
            Config.renderer = CreateSoftwareRenderer();
            RegisterKeyBindings();
            PushContext(-1);
        }

        return &platform;
    }
}

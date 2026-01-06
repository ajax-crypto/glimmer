#include "platform.h"
#include "context.h"
#include "renderer.h"

#include <cstring>

#ifndef GLIMMER_MAX_CLIPBOARD_TEXTSZ
#define GLIMMER_MAX_CLIPBOARD_TEXTSZ 4096
#endif

#include "libs/inc/imgui/imgui_impl_glfw.h"
#include "libs/inc/imgui/imgui_impl_opengl3.h"
#include "libs/inc/imgui/imgui_impl_opengl3_loader.h"

#if defined(GLIMMER_ENABLE_NFDEXT) && !defined(__EMSCRIPTEN__)
#include <mutex>
#include "nfd-ext/src/include/nfd.h"
#include "nfd-ext/src/include/nfd_glfw3.h"
#endif

#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include "libs/inc/GLFW/glfw3.h" // Will drag system OpenGL headers

#ifdef __EMSCRIPTEN__
#include "libs/emscripten/emscripten_mainloop_stub.h"
#endif

#include "libs/inc/stb_image/stb_image.h"

#ifdef _WIN32
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

    static void glfw_error_callback(int error, const char* description)
    {
        fprintf(stderr, "GLFW Error %d: %s\n", error, description);
    }

    struct ImGuiGLFWPlatform final : public IPlatform
    {
        ImGuiGLFWPlatform()
        {
            DetermineInitialKeyStates(desc);
        }

        void SetClipboardText(std::string_view input)
        {
            static char buffer[GLIMMER_MAX_CLIPBOARD_TEXTSZ];

            auto sz = (std::min)(input.size(), (size_t)(GLIMMER_MAX_CLIPBOARD_TEXTSZ - 1));
#ifdef WIN32
            strncpy_s(buffer, input.data(), sz);
#else
            std::strncpy(buffer, input.data(), sz);
#endif

            buffer[sz] = 0;

            ImGui::SetClipboardText(buffer);
        }

        std::string_view GetClipboardText()
        {
            auto str = ImGui::GetClipboardText();
            return std::string_view{ str };
        }

        bool CreateWindow(const WindowParams& params)
        {
            glfwSetErrorCallback(glfw_error_callback);
            if (!glfwInit()) return false;

            // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
            // GL ES 2.0 + GLSL 100
            const char* glsl_version = "#version 100";
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
            glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
            // GL 3.2 + GLSL 150
            const char* glsl_version = "#version 150";
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
            // GL 3.0 + GLSL 130
            const char* glsl_version = "#version 130";
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
            //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
            //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

            int width = 0, height = 0;

            if (params.size.x == FLT_MAX || params.size.y == FLT_MAX)
            {
                glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
                width = 640; height = 480;
            }
            else
            {
                width = (int)params.size.x;
                height = (int)params.size.y;
            }

            // Create window with graphics context
            window = glfwCreateWindow(width, height, params.title.data(), nullptr, nullptr);
            if (window == nullptr) return false;

            glfwMakeContextCurrent(window);
            glfwSwapInterval(1); // Enable vsync

            // Setup Dear ImGui context
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            io.IniFilename = nullptr;

            if (!params.icon.empty())
            {
                GLFWimage images[1];

                if (params.iconType & RT_PATH)
                    images[0].pixels = stbi_load(params.icon.data(), &images[0].width, &images[0].height, 0, 4); 

                glfwSetWindowIcon(window, 1, images);
                stbi_image_free(images[0].pixels);
            }

            // Setup Platform/Renderer backends
            ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
            ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
            ImGui_ImplOpenGL3_Init(glsl_version);

            bgcolor[0] = (float)params.bgcolor[0] / 255.f;
            bgcolor[1] = (float)params.bgcolor[1] / 255.f;
            bgcolor[2] = (float)params.bgcolor[2] / 255.f;
            bgcolor[3] = (float)params.bgcolor[3] / 255.f;
            softwareCursor = params.softwareCursor;

#ifdef _DEBUG
            _CrtSetDbgFlag(_CRTDBG_DELAY_FREE_MEM_DF);
#endif //  _DEBUG

            return true;
        }

        bool PollEvents(bool (*runner)(ImVec2, IPlatform&, void*), void* data)
        {
            auto close = false;

#ifdef _DEBUG
            LOG("Pre-rendering allocations: %d | Allocated: %d bytes\n", TotalMallocs, AllocatedBytes);
#endif

#ifdef __EMSCRIPTEN__
            // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
            // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.

            EMSCRIPTEN_MAINLOOP_BEGIN
#else
            while (!glfwWindowShouldClose(window) && !close)
#endif
            {
                glfwPollEvents();
                if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
                {
                    ImGui_ImplGlfw_Sleep(10);
                    continue;
                }

                int width, height;
                glfwGetWindowSize(window, &width, &height);

                // Start the Dear ImGui frame
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplGlfw_NewFrame();

                if (EnterFrame(width, height))
                {
                    close = !runner(ImVec2{ width, height }, *this, data);

                    for (auto [data, handler] : handlers)
                        close = !handler(data, desc) && close;
                }

                ExitFrame();

                int display_w, display_h;
                glfwGetFramebufferSize(window, &display_w, &display_h);
                glViewport(0, 0, display_w, display_h);
                glClearColor(bgcolor[0], bgcolor[1], bgcolor[2], bgcolor[3]);
                glClear(GL_COLOR_BUFFER_BIT);
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

                glfwSwapBuffers(window);

#ifdef __EMSCRIPTEN__
                EMSCRIPTEN_MAINLOOP_END;
#else
            }
#endif

#if defined(GLIMMER_ENABLE_NFDEXT) && !defined(__EMSCRIPTEN__)
            NFD_Quit();
#endif
            Cleanup();
            return true;
        }

        ImTextureID UploadTexturesToGPU(ImVec2 size, unsigned char* pixels)
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
            glBindTexture(GL_TEXTURE_2D, last_texture);

            return (ImTextureID)(intptr_t)image_texture;
        }

#if defined(GLIMMER_ENABLE_NFDEXT) && !defined(__EMSCRIPTEN__)

        void GetWindowHandle(void* out) override
        {
            std::call_once(nfdInitialized, [] { NFD_Init(); });
            NFD_GetNativeWindowFromGLFWWindow(window, (nfdwindowhandle_t*)out);
        }

#endif

        void PushEventHandler(bool (*callback)(void* data, const IODescriptor& desc), void* data) override
        {
            handlers.emplace_back(data, callback);
        }

        GLFWwindow* window = nullptr;
#if defined(GLIMMER_ENABLE_NFDEXT) && !defined(__EMSCRIPTEN__)
        std::once_flag nfdInitialized;
#endif
        std::vector<std::pair<void*, bool(*)(void*, const IODescriptor&)>> handlers;
    };

    IPlatform* InitPlatform(ImVec2 size)
    {
        static ImGuiGLFWPlatform platform;
        static bool initialized = false;

        if (!initialized)
        {
            initialized = true;
            Config.renderer = CreateImGuiRenderer();
            RegisterKeyBindings();
            PushContext(-1);
        }

        return &platform;
    }
}

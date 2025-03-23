#include "../src/glimmer.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include <cstdlib>
#include <unordered_map>
#include <map>

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
//#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
//#pragma comment(lib, "legacy_stdio_definitions")
//#endif

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

#include <string_view>
#include <cstring>
#include <cctype>
#include <stack>
#include <charconv>
#include <string>

class Application
{
public:

    Application()
    {
#ifdef IM_RICHTEXT_TARGET_IMGUI
        glfwSetErrorCallback(glfw_error_callback);
        if (!glfwInit())
            std::exit(0);

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

    // Create window with graphics context
        m_window = glfwCreateWindow(1280, 720, "Dear ImGui GLFW+OpenGL3 example", nullptr, nullptr);
        if (m_window == nullptr)
            std::exit(0);
        glfwMakeContextCurrent(m_window);
        glfwSwapInterval(1); // Enable vsync

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

        // Setup Dear ImGui style
        //ImGui::StyleColorsDark();
        ImGui::StyleColorsLight();

        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForOpenGL(m_window, true);
#ifdef __EMSCRIPTEN__
        ImGui_ImplGlfw_InstallEmscriptenCallbacks(m_window, "#canvas");
#endif
        ImGui_ImplOpenGL3_Init(glsl_version);
#endif
    }

    int run()
    {
        ImVec4 clear_color = ImVec4(1.f, 1.f, 1.f, 1.00f);

        glimmer::FontDescriptor desc;
        desc.flags = glimmer::FLT_Proportional;
        desc.sizes.push_back(12.f);
        desc.sizes.push_back(16.f);
        desc.sizes.push_back(24.f);
        desc.sizes.push_back(32.f);
        glimmer::LoadDefaultFonts(&desc);

        glimmer::ImGuiRenderer renderer{};
        bool state = false;

        // Main loop
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
        io.IniFilename = nullptr;
        EMSCRIPTEN_MAINLOOP_BEGIN
#else
        while (!glfwWindowShouldClose(m_window))
#endif
        {
            // Poll and handle events (inputs, window resize, etc.)
            // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
            // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
            // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
            // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
            glfwPollEvents();
            if (glfwGetWindowAttrib(m_window, GLFW_ICONIFIED) != 0)
            {
                ImGui_ImplGlfw_Sleep(10);
                continue;
            }

            int width, height;
            glfwGetWindowSize(m_window, &width, &height);

            // Start the Dear ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGui::SetNextWindowSize(ImVec2{ (float)width, (float)height }, ImGuiCond_Always);
            ImGui::SetNextWindowPos(ImVec2{ 0, 0 });

            // Render here
            if (ImGui::Begin("main-window", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
            {
                renderer.UserData = ImGui::GetWindowDrawList();
                renderer.DrawRect({ 0.f, 0.f }, ImVec2{ (float)width, (float)height }, glimmer::ToRGBA(255, 255, 255), true);
                if (glimmer::Button(0, state, "Test Button!", ImVec2{10.f, 10.f}, renderer).event == glimmer::WidgetEvent::Clicked)
                    state = true;
            }

            ImGui::End();

            // Rendering
            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(m_window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(m_window);
        }
#ifdef __EMSCRIPTEN__
        EMSCRIPTEN_MAINLOOP_END;
#endif

        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(m_window);
        glfwTerminate();

        return 0;
    }

private:

    GLFWwindow* m_window = nullptr;
    ImFont* m_defaultFont = nullptr;
};

#if !defined(_DEBUG) && defined(WIN32)
int CALLBACK WinMain(
    HINSTANCE   hInstance,
    HINSTANCE   hPrevInstance,
    LPSTR       lpCmdLine,
    int         nCmdShow
)
#else
int main(int argc, char** argv)
#endif
{
    Application app;
    return app.run();
}
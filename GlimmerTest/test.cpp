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

/*
                    BeginLayout(FillDirection::Horizontal, Layout::Horizontal, "content-align: center")
                        Label(id); Label(id); Label(id);
                        BeginLayout(FillDirection::None, Layout::Horizontal)
                            Label(id); Button(id);
                        EndLayout();
                    EndLayout();

                    SetGeometryMode();
                    auto pos1 = Label(...); ...
                    ResetGeometryMode();

                    auto diff = (total - pos.x) / (count + 1);
                    Translate(id1, idn, { diff, 0.f }); ...

                    <div layout="horizontal" fill="horizontal" alignment="center">
                        <label id="...">Some Text</label>
                        ...
                    </div>

                    div {
                        layout: horizontal;
                        fill: horizontal;
                        alignment: center;
                        label : {
                            text: "haha";
                        }
                    }
                */

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

        glimmer::FontFileNames names;
        names.BasePath = "C:\\Users\\ajax1\\Downloads\\font\\";
        names.Monospace.Files[glimmer::FT_Normal] = "Iosevka-Regular.ttf";
        names.Proportional.Files[glimmer::FT_Normal] = "Iosevka-Regular.ttf";
        glimmer::FontDescriptor desc;
        desc.flags = glimmer::FLT_Monospace | glimmer::FLT_Proportional;
        //desc.names = names;
        desc.sizes.push_back(12.f);
        desc.sizes.push_back(16.f);
        desc.sizes.push_back(24.f);
        desc.sizes.push_back(32.f);
        glimmer::LoadDefaultFonts(&desc);

        glimmer::ImGuiRenderer renderer{};
        glimmer::GetWindowConfig().renderer = &renderer;
        glimmer::GetWindowConfig().defaultFontSz = 32.f;

        enum Labels { UPPER, LEFT, CONTENT, BOTTOM };
        int32_t labels[4];

        auto lid = glimmer::GetNextId(glimmer::WT_Label);
        glimmer::CreateWidget(lid).state.label.text = "Upper";
        labels[UPPER] = lid;

        lid = glimmer::GetNextId(glimmer::WT_Label);
        glimmer::CreateWidget(lid).state.label.text = "Left";
        labels[LEFT] = lid;

        lid = glimmer::GetNextId(glimmer::WT_Label);
        glimmer::CreateWidget(lid).state.label.text = "Content";
        labels[CONTENT] = lid;

        lid = glimmer::GetNextId(glimmer::WT_Label);
        glimmer::CreateWidget(lid).state.label.text = "Bottom";
        labels[BOTTOM] = lid;

        auto gridid = glimmer::GetNextId(glimmer::WT_ItemGrid);
        auto& grid = glimmer::CreateWidget(gridid).state.grid;
        grid.cell = [](int32_t row, int16_t, int16_t) -> glimmer::ItemGridState::CellData& {
            static glimmer::ItemGridState::CellData data;
            row % 2 ? glimmer::SetNextStyle("background-color: white") :
                glimmer::SetNextStyle("background-color: rgb(200, 200, 200)");
            data.state.label.text = "Text";
            return data;
        };

        auto& root = grid.config.headers.emplace_back();
        root.push_back(glimmer::ItemGridState::ColumnConfig{ .name = "Header#1" });
        root.push_back(glimmer::ItemGridState::ColumnConfig{ .name = "Header#2" });

        auto& leaves = grid.config.headers.emplace_back();
        leaves.push_back(glimmer::ItemGridState::ColumnConfig{ .name = "Header#1.1", .parent = 0 });
        leaves.push_back(glimmer::ItemGridState::ColumnConfig{ .name = "Header#1.2", .parent = 0 });
        leaves.push_back(glimmer::ItemGridState::ColumnConfig{ .name = "Header#2.1", .parent = 1 });
        leaves.push_back(glimmer::ItemGridState::ColumnConfig{ .name = "Header#2.2", .parent = 1 });

        grid.config.rows = 16;
        grid.uniformRowHeights = true;
        grid.setCellPadding(5.f);

        // BuildStyle(id).When(WS_Default).Style("cjndkjckdcf").When("hover").Style(",dcdcd");
        // SetStyle(id, "{ ... } :hover { ... } :pressed { ... }");

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
            //ImGui::GetIO().MouseDrawCursor = true;
            ImGui::SetNextWindowSize(ImVec2{ (float)width, (float)height }, ImGuiCond_Always);
            ImGui::SetNextWindowPos(ImVec2{ 0, 0 });
            glimmer::BeginFrame();

            // Render here
            if (ImGui::Begin("main-window", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
            {
                renderer.UserData = ImGui::GetWindowDrawList();
                renderer.DrawRect({ 0.f, 0.f }, ImVec2{ (float)width, (float)height }, glimmer::ToRGBA(255, 255, 255), true);

                glimmer::PushStyle("border: 1px solid gray; padding: 5px; alignment: center; margin: 5px;", "border: 1px solid black");
                /*ImVec2 pos{ 0.f, 0.f };
                auto upperdim = glimmer::Label(labels[UPPER], pos, FLT_MAX, 0.f).geometry;
                pos.y += upperdim.GetHeight();
                pos.x += glimmer::Label(labels[LEFT], pos, 0.f, FLT_MAX).geometry.GetWidth();
                pos.y += glimmer::Label(labels[CONTENT], pos, FLT_MAX, (float)height - (2.f * upperdim.GetHeight())).geometry.GetHeight();
                glimmer::Label(labels[BOTTOM], pos, FLT_MAX, upperdim.GetHeight());*/
                // SpanH; Label; MoveDown; SpanV; Label; MoveLeft; AnchorBottom; SpanH; Label; MoveUp; SpanAll; Label;

                using namespace glimmer;

                PushStyle("background-color: rgb(200, 200, 200)");
                Label(labels[UPPER], ExpandH);
                Move(FD_Vertical);
                PushStyle("background-color: rgb(175, 175, 175)");
                Label(labels[LEFT], ExpandV);
                Move(FD_Horizontal | FD_Vertical);
                PushStyle("background-color: rgb(150, 150, 150)");
                Label(labels[BOTTOM], ExpandH | FromBottom);
                Move(labels[LEFT], labels[UPPER], true, true);
                PushStyle("background-color: rgb(200, 200, 200)");
                //Label(labels[CONTENT], ExpandAll, { .bottom = labels[BOTTOM] });
                ItemGrid(gridid, ExpandAll, { .bottom = labels[BOTTOM] });
                
                PopStyle(5);
            }

            ImGui::End();
            glimmer::EndFrame();

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
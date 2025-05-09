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
        names.BasePath = "C:\\Work\\Sessions\\fonts\\";
        names.Monospace.Files[glimmer::FT_Normal] = "IosevkaFixed-Regular.ttf";
        //names.Proportional.Files[glimmer::FT_Normal] = "IosevkaFixed-Regular.ttf";
        glimmer::FontDescriptor desc;
        desc.flags = glimmer::FLT_Monospace;
        //desc.names = names;
        desc.sizes.push_back(12.f);
        desc.sizes.push_back(16.f);
        desc.sizes.push_back(24.f);
        desc.sizes.push_back(32.f);
        glimmer::LoadDefaultFonts(&desc);

        glimmer::GetWindowConfig().renderer = glimmer::CreateImGuiRenderer();
        glimmer::GetWindowConfig().defaultFontSz = 32.f;

        enum Labels { UPPER, LEFT, LEFT2, CONTENT, BOTTOM, TOGGLE, RADIO, INPUT, DROPDOWN, CHECKBOX, SPLIT1, SPLIT2, TOTAL };
        int32_t widgets[TOTAL];

        auto lid = glimmer::GetNextId(glimmer::WT_Label);
        auto& st = glimmer::GetWidgetState(lid).state.label;
        st.text = R"SVG(
<svg viewBox='0 0 108 95' xmlns='http://www.w3.org/2000/svg'>
<g transform='scale(0.1)'>
<path d='M552,598c-5,10-11,20-12,32c8,11,16,7,25,0c-2-12-7-22-13-32'/>
<path d='M528,7c-17,3-32,13-41,28l-473,820c-10,18-10,39,0,56c10,18,29,29,49,29h947c20,0,39-11,49-29c10-17,10-38,0-56l-473-820c-12-21-35-31-58-28' fill='#fff000'/>
<path d='M532,30c-9,2-17,7-21,15l-477,826c-5,9-5,21,0,30c5,9,15,15,26,15h954c10,0,20-6,26-15c5-9,5-21,0-30l-477-826c-7-11-18-16-31-15M537,126l405,713h-811z'/>
<path d='M582,378l-5,1l1,4l-129,31h-1c-5,3-9,7-10,12c-2,4-2,10,0,14c3,9,12,17,22,16h1l128-28l1,5l5-1zM580,389l8,34l-24,5h-121c1-4,3-7,8-9z'/>
<path d='M383,485l5,2l-1,4l132,19c6,2,10,6,12,11c2,4,2,10,1,14c-2,10-10,18-21,18l-131-17v5h-5zM387,496l-6,35l25,3l120-10v-1c-1-3-4-6-8-8z'/>
<path d='M249,699v43h211v-43h-64l-2,3l-2,4l-4,3c0,0-1,2-2,2h-4c-2,0-3,0-4,1c-1,1-3,1-3,2l-3,4c0,1-1,2-2,2h-4c0,0-2,1-3,0l-3-1c-1,0-3-1-3-2c-1-1,0-2-1-3l-1-3c-1-1-2-1-3-1c-1,0-4,0-4-1c0-2,0-3-1-4v-3v-3z'/>
<path d='M385,593c0,9-6,15-13,15c-7,0-13-6-13-15c0-8,12-39,14-39c1,0,12,31,12,39'/>
<path d='M382,671c0,8-6,15-13,15c-7,0-13-7-13-15c0-8,12-39,13-39c1,0,13,31,13,39'/>
<path d='M614,493c0,9-6,15-13,15c-7,0-13-6-13-15c0-8,12-39,13-39c2,0,13,31,13,39'/>
<path d='M613,591c0,8-6,15-13,15c-7,0-13-7-13-15c0-8,12-39,13-39c1,0,13,31,13,39'/>
<path d='M627,609c-7,0-8,3-7,8c1,6,4,9,7,13c12,10,25,13,37,18v3h-47c-2,6-8,10-15,10c-7,0-13-4-15-10h-4c0,4-4,7-9,7c-4,0-8-3-9-7h-42c-8,0-14,5-14,13c0,8,6,14,14,14h118c1,2,2,4,0,6h-140c-7,0-13,6-13,13c0,8,6,13,13,13h125c2,3,2,6,0,8h-112c-7,0-13,5-13,12c0,7,6,12,13,12h136c1,1,2,4,0,6h-94c-6,0-11,5-11,12c0,6,5,12,11,12h95v1c59,10,129,11,173-11v-105c-49-22-102-34-155-44c-6,1-14-2-19,1c-7-6-15-5-23-5'/>
</g>
</svg>
)SVG";
        st.type = glimmer::TextType::SVG;

        //st.text = "Hello";
        widgets[UPPER] = lid;

        lid = glimmer::GetNextId(glimmer::WT_Label);
        glimmer::GetWidgetState(lid).state.label.text = "Left";
        widgets[LEFT] = lid;

        lid = glimmer::GetNextId(glimmer::WT_Label);
        glimmer::GetWidgetState(lid).state.label.text = "Left2";
        widgets[LEFT2] = lid;

        lid = glimmer::GetNextId(glimmer::WT_Splitter);
        glimmer::GetWidgetState(lid);
        widgets[SPLIT1] = lid;

        lid = glimmer::GetNextId(glimmer::WT_Splitter);
        glimmer::GetWidgetState(lid);
        widgets[SPLIT2] = lid;

        lid = glimmer::GetNextId(glimmer::WT_Label);
        glimmer::GetWidgetState(lid).state.label.text = "Content";
        widgets[CONTENT] = lid;

        lid = glimmer::GetNextId(glimmer::WT_Label);
        glimmer::GetWidgetState(lid).state.label.text = "Bottom";
        widgets[BOTTOM] = lid;

        auto tid = glimmer::GetNextId(glimmer::WT_ToggleButton);
        glimmer::GetWidgetState(tid).state.toggle.checked = false;
        widgets[TOGGLE] = tid;

        tid = glimmer::GetNextId(glimmer::WT_RadioButton);
        glimmer::GetWidgetState(tid).state.radio.checked = false;
        widgets[RADIO] = tid;

        tid = glimmer::GetNextId(glimmer::WT_Checkbox);
        glimmer::GetWidgetState(tid);
        widgets[CHECKBOX] = tid;

        tid = glimmer::GetNextId(glimmer::WT_TextInput);
        auto& model = glimmer::GetWidgetState(tid).state.input;
        model.placeholder = "This will be removed!";
        model.text.reserve(256);
        widgets[INPUT] = tid;

        tid = glimmer::GetNextId(glimmer::WT_DropDown);
        auto& dd = glimmer::GetWidgetState(tid).state.dropdown;
        dd.text = "DropDown";
        dd.ShowList = [](ImVec2, ImVec2 sz, glimmer::DropDownState&) {
            static int lid1 = glimmer::GetNextId(glimmer::WT_Label);
            static int lid2 = glimmer::GetNextId(glimmer::WT_Label);

            glimmer::GetWidgetState(lid1).state.label.text = "First";
            glimmer::Label(lid1);
            glimmer::Move(glimmer::FD_Vertical);
            glimmer::GetWidgetState(lid2).state.label.text = "Second";
            glimmer::Label(lid2);
        };
        widgets[DROPDOWN] = tid;

        auto gridid = glimmer::GetNextId(glimmer::WT_ItemGrid);
        auto& grid = glimmer::GetWidgetState(gridid).state.grid;
        grid.cell = [](int32_t row, int16_t col, int16_t) -> glimmer::ItemGridState::CellData& {
            static glimmer::ItemGridState::CellData data;
            static char buffer[128];
            auto sz = std::snprintf(buffer, 127, "Test-%d-%d", row, col);
            row % 2 ? glimmer::SetNextStyle("background-color: white") :
                glimmer::SetNextStyle("background-color: rgb(200, 200, 200)");
            data.state.label.text = std::string_view{ buffer, (size_t)sz };
            return data;
        };

        auto& root = grid.config.headers.emplace_back();
        root.push_back(glimmer::ItemGridState::ColumnConfig{ .name = "Headerdede#1" });
        root.push_back(glimmer::ItemGridState::ColumnConfig{ .name = "Headerfefe#2" });

        auto& leaves = grid.config.headers.emplace_back();
        leaves.push_back(glimmer::ItemGridState::ColumnConfig{ .name = "Header#1.1", .parent = 0 });
        leaves.push_back(glimmer::ItemGridState::ColumnConfig{ .name = "Header#1.2", .parent = 0 });
        leaves.push_back(glimmer::ItemGridState::ColumnConfig{ .name = "Header#2.1", .parent = 1 });
        leaves.push_back(glimmer::ItemGridState::ColumnConfig{ .name = "Header#2.2", .parent = 1 });

        grid.config.rows = 32;
        grid.uniformRowHeights = true;
        grid.setColumnResizable(-1, true);
        grid.setColumnProps(-1, glimmer::COL_Moveable, true);

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
            if (ImGui::Begin("main-window", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
            {
                glimmer::GetWindowConfig().renderer->UserData = ImGui::GetWindowDrawList();
                glimmer::PushStyle("border: 1px solid gray; padding: 5px; alignment: center; margin: 5px;", "border: 1px solid black");

                using namespace glimmer;

                PushStyle("background-color: rgb(200, 200, 200)");
                Label(widgets[UPPER], ExpandH);

                Move(FD_Vertical);

                StartSplitRegion(widgets[SPLIT1], DIR_Horizontal, { SplitRegion{ .min = 0.2f, .max = 0.3f, .initial = 0.2f },
                    SplitRegion{ .min = 0.7f, .max = 0.8f, .initial = 0.8f } }, ExpandH);

                    PushStyle("background-color: rgb(175, 175, 175)");
                    StartSplitRegion(widgets[SPLIT2], DIR_Vertical, { SplitRegion{ .min = 0.2f, .max = 0.8f },
                        SplitRegion{.min = 0.2f, .max = 0.8f } }, ExpandV);
                        Label(widgets[LEFT], ExpandAll);
                        NextSplitRegion();
                        Label(widgets[LEFT2], ExpandV);
                    EndSplitRegion();

                    NextSplitRegion();

                    Move(FD_Vertical);
                    PushStyle("background-color: rgb(150, 150, 150)");
                    Label(widgets[BOTTOM], FromBottom);
                    Move(FD_Horizontal);

                    PushStyle("padding: 0px; margin: 0px;");
                    ToggleButton(widgets[TOGGLE], ToRight);

                    Move(FD_Horizontal);
                    RadioButton(widgets[RADIO], ToRight);

                    Move(FD_Horizontal);
                    PushStyle("border: 1px solid black; width: 200px;");
                    PushStyle(WS_Selected, "background-color: rgb(50, 100, 255); color: white");
                    TextInput(widgets[INPUT], ToRight);
                    PopStyle(1, WS_Selected);

                    Move(FD_Horizontal);
                    DropDown(widgets[DROPDOWN], ToRight);
                    PopStyle(2);

                    PushStyle(WS_Default, "margin: 3px; padding: 5px; border: 1px solid gray; border-radius: 4px;");
                    PushStyle(WS_Checked, "background-color: blue; color: white;");
                    Move(FD_Horizontal);
                    Checkbox(widgets[CHECKBOX], ToRight);
                    PopStyle(1, WS_Default);
                    PopStyle(1, WS_Checked);

                    Move(widgets[LEFT], widgets[UPPER], true, true);
                    PushStyle("background-color: white; padding: 5px;");
                    ItemGrid(gridid, ExpandAll, { .bottom = widgets[BOTTOM] });
                
                EndSplitRegion();
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
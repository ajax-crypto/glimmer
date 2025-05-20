#include "../src/glimmer.h"

auto svg = R"SVG(
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
    auto& config = glimmer::GetWindowConfig();
    config.platform = glimmer::GetPlatform();
    
    if (config.platform->CreateWindow({ .size = { 1280.f, 720.f }, .title = "Glimmer Window" }))
    {
        glimmer::FontFileNames names;
        names.BasePath = "C:\\Work\\Sessions\\fonts\\";
        names.Monospace.Files[glimmer::FT_Normal] = "IosevkaFixed-Regular.ttf";
        //names.Proportional.Files[glimmer::FT_Normal] = "IosevkaFixed-Regular.ttf";
        glimmer::FontDescriptor desc;
        desc.flags = glimmer::FLT_Proportional;
        //desc.names = names;
        desc.sizes.push_back(12.f);
        desc.sizes.push_back(16.f);
        desc.sizes.push_back(24.f);
        desc.sizes.push_back(32.f);
        glimmer::LoadDefaultFonts(&desc);

        glimmer::GetWindowConfig().renderer = glimmer::CreateImGuiRenderer();
        glimmer::GetWindowConfig().defaultFontSz = 24.f;

        enum Labels { UPPER, LEFT, LEFT2, CONTENT, BOTTOM, TOGGLE, RADIO, INPUT, 
            DROPDOWN, CHECKBOX, SPLIT1, SPLIT2, GRID, SLIDER, TOTAL };
        int32_t widgets[TOTAL];

        auto lid = glimmer::GetNextId(glimmer::WT_Label);
        auto& st = glimmer::GetWidgetState(lid).state.label;
        st.text = svg;
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

        tid = glimmer::GetNextId(glimmer::WT_Slider);
        auto& slider = glimmer::GetWidgetState(tid).state.slider;
        slider.max = 100.f;
        widgets[SLIDER] = tid;

        tid = glimmer::GetNextId(glimmer::WT_DropDown);
        auto& dd = glimmer::GetWidgetState(tid).state.dropdown;
        dd.text = "DropDown";
        /*dd.ShowList = [](ImVec2, ImVec2 sz, glimmer::DropDownState&) {
            static int lid1 = glimmer::GetNextId(glimmer::WT_Label);
            static int lid2 = glimmer::GetNextId(glimmer::WT_Label);

            glimmer::GetWidgetState(lid1).state.label.text = "First";
            glimmer::Label(lid1);
            glimmer::Move(glimmer::FD_Vertical);
            glimmer::GetWidgetState(lid2).state.label.text = "Second";
            glimmer::Label(lid2);
        };*/
        std::vector<std::pair<glimmer::WidgetType, std::string_view>> options;
        options.emplace_back(glimmer::WT_Checkbox, "Option 1");
        options.emplace_back(glimmer::WT_Checkbox, "Option 2");
        dd.options = options;
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
        widgets[GRID] = gridid;

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
        void* data = widgets;

        config.platform->PollEvents([](void* data) {
            int32_t* widgets = (int32_t*)data;
            glimmer::PushStyle("background-color: rgb(200, 200, 200); border: 1px solid gray; padding: 5px; alignment: center; margin: 5px;", "border: 1px solid black");

            using namespace glimmer;

            Label(widgets[UPPER], ExpandH);
            Move(FD_Vertical);

            StartSplitRegion(widgets[SPLIT1], DIR_Horizontal, { SplitRegion{.min = 0.2f, .max = 0.3f, .initial = 0.2f },
                SplitRegion{.min = 0.7f, .max = 0.8f, .initial = 0.8f, .scrollable = false } }, ExpandH);

            {
                PushStyle("background-color: rgb(175, 175, 175)");
                StartSplitRegion(widgets[SPLIT2], DIR_Vertical, { SplitRegion{.min = 0.2f, .max = 0.8f },
                    SplitRegion{.min = 0.2f, .max = 0.8f } }, ExpandV);
                {
                    Label(widgets[LEFT], ExpandAll);
                    NextSplitRegion();
                    Label(widgets[LEFT2], ExpandV);
                }
                EndSplitRegion();

                NextSplitRegion();

                //Move(FD_Horizontal);
                BeginLayout(Layout::Horizontal, FD_Horizontal | FD_Vertical, TextAlignHCenter | TextAlignBottom, false, { 5.f, 5.f },
                    NeighborWidgets{ .top = widgets[UPPER] });

                {
                    Label(widgets[BOTTOM]);
                    Move(FD_Horizontal);

                    ToggleButton(widgets[TOGGLE], ToRight);

                    Move(FD_Horizontal);
                    RadioButton(widgets[RADIO], ToRight);

                    Move(FD_Horizontal);
                    PushStyle("border: 1px solid black; width: 200px; background-color: rgb(240, 240, 240)");
                    PushStyle(WS_Selected, "background-color: rgb(50, 100, 255); color: white");
                    TextInput(widgets[INPUT], ToRight);
                    PopStyle(1, WS_Selected);
                    PopStyle(1, WS_Default);

                    Move(FD_Horizontal);
                    DropDown(widgets[DROPDOWN], ToRight);

                    //PushStyle(WS_Default, "margin: 3px; padding: 5px; border: 1px solid gray; border-radius: 4px;");
                    PushStyle(WS_Checked, "background-color: blue; color: white;");
                    Move(FD_Horizontal);
                    //Checkbox(widgets[CHECKBOX], ToRight);
                    Slider(widgets[SLIDER], ToRight);
                    //PopStyle(1, WS_Default);
                    PopStyle(1, WS_Checked);
                }

                EndLayout();

                Move(widgets[LEFT], widgets[UPPER], true, true);
                PushStyle("background-color: white; padding: 5px;");
                ItemGrid(widgets[GRID], ExpandAll, { .bottom = widgets[BOTTOM] });
            }

            EndSplitRegion();
            PopStyle(5);
            return true;
        }, data);
    }

    return 0;
}
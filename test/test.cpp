#include "../src/glimmer.h"
#include "../src/custom/PathInput.h"

#if !defined(_DEBUG) && defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#undef CreateWindow
#endif

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

void TestWindow(glimmer::UIConfig& config)
{
    enum Labels {
        UPPER, LEFT, LEFT2, CONTENT, BOTTOM, TOGGLE, RADIO, INPUT,
        DROPDOWN, CHECKBOX, SPLIT1, SPLIT2, GRID, SLIDER, SPINNER, TAB, ACCORDION, TOTAL
    };
    int32_t widgets[TOTAL];

    auto lid = glimmer::GetNextId(glimmer::WT_Label);
    auto& st = glimmer::CreateWidgetConfig(lid).state.label;
    st.text = svg;
    st.type = glimmer::TextType::SVG;

    //st.text = "Hello";
    widgets[UPPER] = lid;

    lid = glimmer::GetNextId(glimmer::WT_Splitter);
    glimmer::CreateWidgetConfig(lid);
    widgets[SPLIT1] = lid;

    auto tid = glimmer::GetNextId(glimmer::WT_TabBar);
    auto& tab = glimmer::CreateWidgetConfig(tid).state.tab;
    tab.sizing = glimmer::TabBarItemSizing::ResizeToFit;
    tab.expandTabs = true;
    widgets[TAB] = tid;

    auto gridid = glimmer::GetNextId(glimmer::WT_ItemGrid);
    auto& grid = glimmer::CreateWidgetConfig(gridid).state.grid;
    grid.cellwidget = [](const glimmer::ItemGridConfig::CellGeometry& cell) {
        static char buffer[128];
        auto sz = std::snprintf(buffer, 127, "Test-%d-%d", cell.row, cell.col);

        auto id = glimmer::GetNextId(glimmer::WT_Label);
        glimmer::CreateWidgetConfig(id).state.label.text = std::string_view{ buffer, (size_t)sz };
        auto result = glimmer::Label(id);

        if (cell.col == 0)
        {
            glimmer::Move(glimmer::FD_Horizontal);
            id = glimmer::GetNextId(glimmer::WT_Checkbox);
            glimmer::CreateWidgetConfig(id);
            result = glimmer::Checkbox(id);
        }

        PopStyle(1, glimmer::WS_Default);
        };
    grid.cellprops = [](const glimmer::ItemGridConfig::CellProperties& cell) {
        glimmer::ItemGridItemProps props;
        cell.row % 2 ? glimmer::PushStyle("background-color: white") :
            glimmer::PushStyle("background-color: rgb(200, 200, 200)");
        return props;
        };
    grid.header = [](ImVec2, float, int16_t level, int16_t col, int16_t) {
        auto id = glimmer::GetNextId(glimmer::WT_Label);
        if (level == 0)
        {
            if (col == 0) glimmer::CreateWidgetConfig(id).state.label.text = "Header#1";
            else glimmer::CreateWidgetConfig(id).state.label.text = "Header#2";
        }
        else
        {
            switch (col)
            {
            case 0: glimmer::CreateWidgetConfig(id).state.label.text = "Header#1.1"; break;
            case 1: glimmer::CreateWidgetConfig(id).state.label.text = "Header#1.2"; break;
            case 2: glimmer::CreateWidgetConfig(id).state.label.text = "Header#2.1"; break;
            case 3: glimmer::CreateWidgetConfig(id).state.label.text = "Header#2.2"; break;
            }
        }
        glimmer::Label(id);
        };
    widgets[GRID] = gridid;
    void* data = widgets;

    config.platform->PollEvents([](ImVec2, glimmer::IPlatform&, void* data) {
        int32_t* widgets = (int32_t*)data;
        glimmer::PushStyle("background-color: rgb(200, 200, 200); padding: 5px; alignment: center; margin: 5px;",
            "border: 1px solid black");

        using namespace glimmer;

        Label(widgets[UPPER], ExpandH);
        Move(FD_Vertical);

        BeginTabBar(widgets[TAB]);
        AddTab("Tab 1", "", TI_Closeable);
        AddTab("Tab 2", "", TI_Closeable);
        AddTab("Tab 3", "", TI_Closeable);

        PushStyle("background-color: blue; color: white;");
        AddTab("Settings", "", TI_AnchoredToEnd);
        PopStyle();
        EndTabBar(nullptr, true);
        PopStyle(1, WS_Default);
        Move(FD_Vertical);

        glimmer::PushStyle("background-color: rgb(225, 225, 225); padding: 5px;",
            "background-color: rgb(175, 175, 175);");
        BeginNavDrawer(0, true);
        AddNavDrawerEntry(RT_SYMBOL, "warning", "First!!!");
        AddNavDrawerEntry(RT_SYMBOL, "error", "Second!!!");
        AddNavDrawerEntry(RT_SYMBOL, "home", "Third!!!");
        EndNavDrawer(nullptr);
        PopStyle(1, WS_Default | WS_Hovered);
        Move(FD_Horizontal);

        BeginSplitRegion(widgets[SPLIT1], DIR_Vertical, { SplitRegion{.min = 0.2f, .max = 0.3f, .initial = 0.2f },
            SplitRegion{.min = 0.7f, .max = 0.8f, .initial = 0.8f } }, ExpandH);

        {
			int selection1 = -1, selection2 = -1;
			BeginFlexLayout(DIR_Horizontal, ExpandH | AlignHCenter, false, { 10.f, 10.f }, { 0, 0 });
            {
                if (BeginDropDown("drop-down-1", "Drop Down 1"))
                    for (auto opt = 0; opt < 8; ++opt)
                        AddDropDownOption("option for 1");
                EndDropDown(&selection1);
                if (BeginDropDown("drop-down-2", "Drop Down 2"))
                    for (auto opt = 0; opt < 8; ++opt)
                        AddDropDownOption("option for 2");
                EndDropDown(&selection2);
                EndLayout();
            }

            NextSplitRegion();

            //Move(widgets[LEFT], widgets[TAB], true, true);
            PushStyle("background-color: white; padding: 5px; border: none;");

            BeginItemGrid(widgets[GRID], ExpandAll);
            BeginItemGridHeader(2);
            AddHeaderColumn(ItemGridConfig::ColumnConfig{ .id = "h1", .props = COL_Resizable | COL_Sortable, .parent = 0, });
            AddHeaderColumn(ItemGridConfig::ColumnConfig{ .id = "h2", .props = COL_Resizable | COL_Sortable, .parent = 0 });
            AddHeaderColumn(ItemGridConfig::ColumnConfig{ .id = "h3", .props = COL_Resizable | COL_Sortable, .parent = 0 });
            AddHeaderColumn(ItemGridConfig::ColumnConfig{ .id = "h4", .props = COL_Resizable | COL_Sortable, .parent = 0 });

            CategorizeColumns();
            AddColumnCategory(ItemGridConfig::ColumnConfig{ .props = COL_Resizable | COL_Sortable }, 0, 1);
            AddColumnCategory(ItemGridConfig::ColumnConfig{ .props = COL_Resizable | COL_Sortable }, 2, 3);

            EndItemGridHeader();
            AddFilterRow();
            PopulateItemGrid(100, ItemGridPopulateMethod::ByRows);
           /* AddEpilogueRows(2, ItemGridConfig::EpilogueRowProviders{
				.cellprops = [](const ItemGridConfig::CellProperties& cell) {
                    ItemGridItemProps props;
                    props.textType = TextType::PlainText;
                    return props;
                },
                .cellwidget = [](const ItemGridConfig::CellGeometry& cell) {
                    auto id = glimmer::GetNextId(glimmer::WT_Label);
                    glimmer::CreateWidgetConfig(id).state.label.text = "Epilogue";
                    glimmer::Label(id);
                }
            });*/
            EndItemGrid();
        }

        EndSplitRegion();
        PopStyle(5);
        //CacheLayout();
        return true;
        }, data);
}

#if !defined(_DEBUG) && defined(_WIN32)
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
    auto& config = glimmer::CreateUIConfig(true);
    config.defaultFontSz = 24.f;
    config.platform = glimmer::InitPlatform();

    if (config.platform->CreateWindow({ .title = "Glimmer Demo" }))
    {
        glimmer::FontFileNames names;
        names.BasePath = "C:\\Work\\Sessions\\fonts\\";
        names.Monospace.Files[glimmer::FT_Normal] = "IosevkaFixed-Regular.ttf";
        //names.Proportional.Files[glimmer::FT_Normal] = "IosevkaFixed-Regular.ttf";
        glimmer::FontDescriptor desc;
        desc.flags = glimmer::FLT_Proportional | glimmer::FLT_Antialias | glimmer::FLT_Hinting;
        //desc.names = names;
        desc.sizes.push_back(16.f);
        desc.sizes.push_back(24.f);
        desc.sizes.push_back(32.f);
        desc.sizes.push_back(48.f);
        glimmer::LoadDefaultFonts(&desc, 1, true);

        TestWindow(config);
    }

    return 0;
}
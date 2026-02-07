<h1>Glimmer: An Immediate Mode GUI Library (Under Progress 🚧)</h1>

Glimmer is a C++20 library for creating graphical user interfaces using an immediate mode paradigm. This approach simplifies UI development by allowing developers to define the UI structure and behavior directly within their application logic.

- **Immediate Mode**: Unlike retained mode GUI libraries where UI elements are persistent objects, Glimmer processes UI elements on each frame. This means the UI is redrawn every frame based on the current
   application state. 

- **Widgets**: Glimmer provides a set of common UI widgets such as labels, buttons, checkboxes, radio buttons, text inputs, dropdowns, sliders, spinners, and more complex ones like item grids, accordion and tab bars. 

- **Layout System**: The library includes a flexible layout system to arrange widgets. It supports: 
   - **Ad-hoc Positioning**: Widgets can be placed at specific coordinates. 
   - **Flexbox Layouts**: Flexbox layout scheme is implemented using [Yoga](https://github.com/facebook/yoga).
   - **Grid Layouts**: Row and Column specified layout scheme with multi-span row and column support. 
   - **Splitter Layouts**: Dividing regions into resizable sub-regions. 
   - **Fill and Alignment**: Controls for how widgets fill available space and align within their parent. 
   - **Spanning and Moving**: Fine-grained control over widget placement relative to others. 

- **Styling**: Widgets can be styled using CSS-like string definitions. This includes properties for colors,
   fonts, borders, padding, margins, and shadows. Styles can be applied globally, per widget type, or for specific widget states (default, hover, pressed, focused, checked, disabled). Id and classes are supported. Global themes can be installed before creating the window through global config.

- **SVG & Image Support**: Both SVG and PNG images can be rendered. [LunaSVG](https://github.com/sammycage/lunasvg) and [STB_Image](https://github.com/nothings/stb) is being used respectively.

- **Text Rendering**: Fonts are supported through the [freetype2](https://freetype.org/) library and basic support for extended ASCII exists. Complex scripts and bidirectional text is not yet supported.

- **Rich Text**: Rich text rendering is supported through [ImRichText](https://github.com/ajax-crypto/ImRichText)

- **Icon Fonts**: Enabled support for icon fonts, such as font awesome via [IconFontCppHeaders](https://github.com/juliettef/IconFontCppHeaders)

- **Native File Dialogs**: Native file dialogs are supported by using [nfd-extended](https://github.com/btzy/nativefiledialog-extended) library or can be defined by the platform layer i.e. SDL3.

- **Plots**: [ImPlot](https://github.com/epezent/implot) library is used to create plots/graphs.

<h2> Widgets </h2>

| Widget Name      | Purpose / Description                                                                |
|------------------|--------------------------------------------------------------------------------------|
| Label            | Displays static text or SVG/image content.                                           |
| Button           | Clickable button for user actions.                                                   |
| ToggleButton     | Switches between two states (on/off), like a checkbox but styled as a toggle.        |
| RadioButton      | Selects one option from a group of mutually exclusive choices.                       |
| Checkbox         | Allows toggling a boolean value (checked/unchecked/partial).                         |
| Spinner          | Numeric input with increment/decrement controls (stepper).                           |
| Slider           | Allows selection of a numeric value by sliding a handle along a track.               |
| RangeSlider      | Selects a numeric range using two handles (min/max).                                 |
| TextInput        | Single-line editable text field for user input.                                      |
| DropDown         | Selects one option from a collapsible list of choices.                               |
| TabBar           | Organizes content into tabs for easy navigation.                                     |
| Accordion        | Expandable/collapsible panel for showing/hiding content sections.                    |
| ItemGrid         | Displays tabular data with customizable columns, headers, and cell widgets.          |
| Splitter         | Divides a region into resizable panels (horizontal or vertical).                     |
| Region           | Generic container for grouping and layout of widgets.                                |
| NavDrawer        | Vertical navigation panel for switching between major sections or views.             |
| MediaResource    | Displays images, SVGs, or icon resources.                                            |
| ContextMenu      | Popup menu for context-specific actions (usually on right-click).                    |
| Chart/Plot       | (If enabled) Displays data visualizations using ImPlot integration.                  |

Take a look at `src/custom/PathInput.h` to see how custom widgets can be written.

<h2> Platform & Rendering </h2>

Glimmer abstracts the rendering process through the `glimmer::IRenderer` interface. There are following renderers available:
  - **ImGui Renderer** which uses ImGui to generate geometry primitives and use some ImGui platform integration to render.
  - **SVG Renderer** which dumps each frame as SVG markup in some SVG file.
  - (Internal) **Deferred Renderer** which enqueues draw commands and can be dequed at any point.
  - **Blend2D Renderer** which uses the [Blend2D](https://github.com/blend2d/blend2d) library to render which works without any GPU in the system.
  - **Test Renderer** which will enable creating automated tests (along with a test platform interface to pump events).
The following is the interface:

```c++
struct IRenderer
{
    virtual bool InitFrame(float width, float height, uint32_t bgcolor, bool softCursor) { return true; }
    virtual void FinalizeFrame(int32_t cursor) {}

    virtual void SetClipRect(ImVec2 startpos, ImVec2 endpos, bool intersect = true) = 0;
    virtual void ResetClipRect() = 0;

    virtual void BeginDefer() {}
    virtual void EndDefer() {}

    virtual void DrawLine(ImVec2 startpos, ImVec2 endpos, uint32_t color, float thickness = 1.f) = 0;
    virtual void DrawPolyline(ImVec2* points, int sz, uint32_t color, float thickness) = 0;
    virtual void DrawTriangle(ImVec2 pos1, ImVec2 pos2, ImVec2 pos3, uint32_t color, bool filled, float thickness = 1.f) = 0;
    virtual void DrawRect(ImVec2 startpos, ImVec2 endpos, uint32_t color, bool filled, float thickness = 1.f) = 0;
    virtual void DrawRoundedRect(ImVec2 startpos, ImVec2 endpos, uint32_t color, bool filled, float topleftr, float toprightr, float bottomrightr, float bottomleftr, float thickness = 1.f) = 0;
    virtual void DrawRectGradient(ImVec2 startpos, ImVec2 endpos, uint32_t colorfrom, uint32_t colorto, Direction dir) = 0;
    virtual void DrawRoundedRectGradient(ImVec2 startpos, ImVec2 endpos, float topleftr, float toprightr, float bottomrightr, float bottomleftr,
        uint32_t colorfrom, uint32_t colorto, Direction dir) = 0;
    virtual void DrawPolygon(ImVec2* points, int sz, uint32_t color, bool filled, float thickness = 1.f) = 0;
    virtual void DrawPolyGradient(ImVec2* points, uint32_t* colors, int sz) = 0;
    virtual void DrawCircle(ImVec2 center, float radius, uint32_t color, bool filled, float thickness = 1.f) = 0;
    virtual void DrawSector(ImVec2 center, float radius, int start, int end, uint32_t color, bool filled, bool inverted, float thickness = 1.f) = 0;
    virtual void DrawRadialGradient(ImVec2 center, float radius, uint32_t in, uint32_t out, int start, int end) = 0;

    virtual bool SetCurrentFont(std::string_view family, float sz, FontType type) { return false; };
    virtual bool SetCurrentFont(void* fontptr, float sz) { return false; };
    virtual void ResetFont() {};

    virtual ImVec2 GetTextSize(std::string_view text, void* fontptr, float sz, float wrapWidth = -1.f) = 0;
    virtual void DrawText(std::string_view text, ImVec2 pos, uint32_t color, float wrapWidth = -1.f) = 0;
    virtual void DrawTooltip(ImVec2 pos, std::string_view text) = 0;
    virtual float EllipsisWidth(void* fontptr, float sz);

    virtual bool StartOverlay(int32_t id, ImVec2 pos, ImVec2 size, uint32_t color) { return true; }
    virtual void EndOverlay() {}

    virtual bool DrawResource(int32_t resflags, ImVec2 pos, ImVec2 size, uint32_t color, std::string_view content, int32_t id = -1) { return false; }
    virtual int64_t PreloadResources(int32_t loadflags, ResourceData* resources, int totalsz) { return 0; }

    virtual void Render(IRenderer& renderer, ImVec2 offset, int from = 0, int to = -1) {}
    virtual int TotalEnqueued() const { return 0; }
    virtual void Reset() {}

    virtual void DrawDebugRect(ImVec2 startpos, ImVec2 endpos, uint32_t color, float thickness) {}
};
```

The platform integration is abstracted by `glimmer::IPlatform` interface. Integration is tested with SDL3, SDL3-GPU and GLFW + OpenGL3 combinations in the test projects. The following is the platform interface:

```c++
struct IPlatform
{
    enum FileDialogTarget
    {
        OneFile = 1, 
        OneDirectory = 2, 
        MultipleFiles = 4,
        MultipleDirectories = 8
    };

    struct DialogProperties
    {
        std::string_view title;
        std::string_view confirmBtnText = "OK";
        std::string_view cancelBtnText = "Cancel";
    };

    virtual void SetClipboardText(std::string_view input) = 0;
    virtual std::string_view GetClipboardText() = 0;
    virtual bool CreateWindow(const WindowParams& params) = 0;
    virtual bool PollEvents(bool (*runner)(ImVec2, IPlatform&, void*), void* data) = 0;
    virtual ImTextureID UploadTexturesToGPU(ImVec2 size, unsigned char* pixels) = 0;

    virtual void GetWindowHandle(void*);
    virtual int32_t ShowFileDialog(std::string_view* out, int32_t outsz, int32_t target,
        std::string_view location, std::pair<std::string_view, std::string_view>* filters = nullptr,
        int totalFilters = 0, const DialogProperties& props = DialogProperties{});

    virtual bool DetermineInitialKeyStates(IODescriptor& desc) { return false; }
    virtual bool RegisterHotkey(const HotKeyEvent& hotkey);
    virtual bool ToggleTextInputMode(bool isTextInputMode) { return false; }
};
```

Custom widgets can be added by either combining existing widgets in a layout with custom data binding or
can be done by implement the `ICustomWidget` interface, and providing it through `UIConfig::CustomWidgetProvider` member.

```c++
struct ICustomWidget
{
    virtual StyleDescriptor GetStyle(int32_t id, int32_t state, const StyleStackT& stack) = 0;
    virtual ImRect ComputeGeometry(const ImVec2& pos, const LayoutItemDescriptor& layoutItem, 
        const NeighborWidgets& neighbors, ImVec2 maxsz) = 0;
    virtual WidgetDrawResult DrawWidget(const StyleDescriptor& style, const LayoutItemDescriptor& layoutItem, 
        IRenderer& renderer, const IODescriptor& io) = 0;
    virtual void HandleEvents(int32_t id, ImVec2 offset, const IODescriptor& io, WidgetDrawResult& result) = 0;
    virtual void RecordItemGeometry(int32_t id, const ImRect& rect) = 0;
    virtual void RecordChildWidgetGeometry(const LayoutItemDescriptor& layoutItem) {}
    virtual void UpdateAvailableChildExtent(ImVec2& extent, int32_t id) {}

    virtual const ImRect& GetGeometry(int32_t id) const = 0;
    virtual int32_t GetState(int32_t id) const = 0;
    virtual std::string_view GetName() const = 0;
};
```

<h2> Getting Started</h2>

Add `glimmer.h` to your project. The application structure will be the following:

```c++
auto& config = glimmer::CreateUIConfig(true);
config.platform = glimmer::InitPlatform();

if (config.platform->CreateWindow({ .title = "Glimmer Demo" }))
{
    glimmer::FontFileNames names;
    names.BasePath = <base_path>;
    names.Monospace.Files[glimmer::FT_Normal] = "<font-name>.ttf";
    names.Proportional.Files[glimmer::FT_Normal] = "<font-name>.ttf";

    glimmer::FontDescriptor desc;
    desc.flags = glimmer::FLT_Proportional | glimmer::FLT_Antialias | glimmer::FLT_Hinting;
    desc.names = names;
    glimmer::LoadDefaultFonts(&desc, 1, true);

    config.platform->PollEvents([](ImVec2, glimmer::IPlatform&, void* data) 
    {
      ... // Code to create widgets + layouts
    }, <void pointer to user data>);
}
```

Look at `GlimmerTest/test.cpp` file for an example.

<h2> Library configuration macros </h2>

| Macro | Definition/Value | Purpose |
|-------|------------------|---------|
| `GLIMMER_DISABLE_SVG` | (conditional) | When defined, disables SVG rendering support (excludes lunasvg) |
| `GLIMMER_DISABLE_IMAGES` | (conditional) | When defined, disables image loading support (excludes stb_image) |
| `GLIMMER_DISABLE_RICHTEXT` | (conditional) | When defined, disables rich text rendering | 
| `GLIMMER_DISABLE_PLOTS` | (conditional) | When defined, disables plotting/graph library integration | 
| `GLIMMER_ENABLE_NFDEXT` | (conditional) | When defined, enables nfd-extended library to enable file pickers | 
| `GLIMMER_TOTAL_ID_SIZE` | (1 << 16) (65536) | Maximum size for widget ID string backing store |
| `GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL` | (not shown, likely 8-16) | Maximum nesting level for item grid column categories |
| `GLIMMER_MAX_SPLITTER_REGIONS` | (8-16) | Maximum number of regions in a splitter widget |
| `GLIMMER_MAX_STYLE_STACKSZ` | (8-16) | Maximum nested CSS styles allowed |
| `GLIMMER_MAX_WIDGET_SPECIFIC_STYLES` | (8-16) | Maximum distinct per widget styles allowed |
| `GLIMMER_MAX_LAYOUT_NESTING` | (8-16) | Maximum nesting of layouts allowed |
| `GLIMMER_MAX_REGION_NESTING` | (8-16) | Maximum nesting of containers/regions allowed |
| `GLIMMER_DEFAULT_FONTFAMILY` | String value | Default font family name for the UI |
| `IM_COL32_A_MASK` | 0xFF000000 | Alpha channel mask for 32-bit colors |
| `IM_COL32_WHITE` | 0xFFFFFFFF | White color constant |

<h2> Building the library </h2>

Use `build.py` file to build the library. The following options are available:

| Option | Description |
|--------|-------------|
| `--platform` | Specify the platform to build for (choices: test, sdl3, glfw). Default is sdl3. |
| `--release`, `-r` | Build in release mode. |
| `--debug`, `-d` | Build in debug mode. |
| `--update` | Force update of all dependencies. |
| `--disable-plots` | Disable ImPlot support. |
| `--disable-svg` | Disable SVG support. |
| `--disable-images` | Disable image support. |
| `--disable-icon-font` | Disable icon font support. |
| `--disable-richtext` | Disable rich text support. |
| `--enable-blend2d` | Enable Blend2D support. |
| `--clean` | Clean the build directory before building. |

The script will download dependencies and build a static library out of glimmer. The library will be located in `staticlib/combined/glimmer.lib`. X11 development libraries are required to build on Linux. *Wayland is work in progress* 

Supported platforms include Windows and Linux. (Mac support will come in future)
Tested compilers include GCC (Linux) and MSVC (Windows).

<h2> TODO(s) </h2>

- [ ] **Improved Focus Handling** : Focus handling for input widgets and handling focus shifts with tab button. (This also enables keyboard navigation of UI)

- [ ] **Wayland support on Linux** : Currently, X11 libraries are used on Linux for window handling, wayland support has to be added. (This might require small changes as SDL3 already integrates that)

- **Support for more Platforms** : 
  - [ ] Use emscripten to compile glimmer for web as a web-assembly file.
  - [ ] Add support for Mac (OS X).

- [ ] **Internationalization** : Support full Unicode standard with Unicode bidirectional text. This will require something like [Harfbuzz](https://github.com/harfbuzz/harfbuzz) and [SheenBidi](https://github.com/Tehreer/SheenBidi).

- [ ] **Accessibility** : Implement platform specific interfaces for accessbility i.e. UIA for Windows, etc.

- [ ] **Improve CSS-based Styling** : Support for application wide style sheets including selectors. (Even file watcher support to track external css file changes) Widget specific and more CSS-based properties can be added i.e. animations, color schemes, etc.

- [ ] **Improved platform integration** : Add support for more platform specific dialogs and other features.

- [ ] **SDF Text Rendering** : SDF-based text rendering and explore further improvements.

- [ ] **Better Multimedia Support** : Improve the limited support for images by supporting more formats (including compressed resources), add video and audio support.

- [ ] **Add more Renderers** : Add support for more renderers i.e. [Skia](https://github.com/google/skia) and [Rive](https://github.com/rive-app/rive-runtime).

At least 3 of the above items should be implemented for a 1.0 release.
<h1>Glimmer: An Immediate Mode GUI Library (Under Progress 🚧)</h1>
---------------------------------------

Glimmer is a C++20 library for creating graphical user interfaces using an immediate mode paradigm. This approach simplifies UI development by allowing developers to define the UI structure and behavior directly within their application logic.

 - **Immediate Mode**: Unlike retained mode GUI libraries where UI elements are persistent objects, Glimmer processes UI elements on each frame. This means the UI is redrawn every frame based on the current
   application state. 
  - **Widgets**: Glimmer provides a set of common UI widgets such as labels, buttons, checkboxes, radio buttons, text inputs, dropdowns, sliders, spinners, and more complex ones like item grids and tab bars. 
- **Layout System**: The library includes a flexible layout system to arrange widgets. It supports: 
   - **Ad-hoc Positioning**: Widgets can be placed at specific coordinates. 
   - **Directional Layouts**: Horizontal and vertical stacking of widgets. 
   - **Splitter Layouts**: Dividing regions into resizable sub-regions. 
   - **Fill and Alignment**: Controls for how widgets fill available space and align within their
   parent. 
   - **Spanning and Moving**: Fine-grained control over widget
   placement relative to others. 
- **Styling**: Widgets can be styled using CSS-like string definitions. This includes properties for colors,
   fonts, borders, padding, margins, and shadows. Styles can be applied globally, per widget type, or for specific widget states (default, hover, pressed, focused, checked, disabled). 
 - **Rendering**: Glimmer abstracts the rendering process. The provided test.cpp example uses
   an ImGui-based renderer. A deferred renderer is also available, which queues drawing operations to be executed by a concrete renderer.
   
 - **Platform Abstraction**: The library includes a platform abstraction layer to handle window creation and event polling. 

- **Context Management**: A central `WidgetContextData` manages the state of all widgets, styles,
   layouts, and other UI-related data, minimizes allocations within frames.

<h2>	Layout Usage:</h2>

 - **Split Regions**: `StartSplitRegion` and `EndSplitRegion` are used to create
   resizable horizontal and vertical splits. 
 - **Directional Layouts**:    `BeginLayout` and `EndLayout` with `Layout::Horizontal` or `Layout::Vertical`    to arrange widgets.
 - **Ad-hoc Positioning**: `Move()` is used to position    subsequent widgets relative to previous ones or absolute coordinates.
 - **Fill and Alignment**: `ExpandH`, `ExpandV`, `ExpandAll,` `TextAlignHCenter`,    `TextAlignBottom` are used to control how widgets occupy space.

<h2>	Styling & Events:</h2>

 - PushStyle and PopStyle are used to apply and remove styles for default and specific widget states (e.g., `WS_Selected`, `WS_Checked`).
 - CSS-like strings define properties like background-color, border, padding, alignment, width. SetNextStyle is used for one-time style application. 
- Event Handling: A polling loop processes UI interactions and redraws the UI. 
- SVG Rendering: Demonstrates rendering an SVG image.
- ItemGrid Customization: Defines column headers with
   hierarchical structure. Provides a callback (grid.cell) to
   dynamically populate cell data. Enables column resizing and
   reordering.

<h2>	Getting Started (Conceptual)</h2>

1.	Include Glimmer: Add glimmer.h to your project.
2.	Initialize Platform and Renderer:
•	Get the platform instance: `glimmer::GetPlatform()`.
•	Create a window using `platform->CreateWindow()`.
•	Create a renderer instance (e.g., `glimmer::CreateImGuiRenderer()`).
•	Set the renderer in the `WindowConfig`.
3.	Load Fonts: Use `glimmer::LoadDefaultFonts()` or other font management functions.
4.	Register widgets with specific IDs, using `glimmer::GetNextId` and `glimmer::GetWidgetState`
5.	Main Loop:
•	Poll platform events using `platform->PollEvents()`. Inside the event callback:
•	Define your UI structure by creating widgets (e.g., `glimmer::Label(id, ...)`), applying styles (`glimmer::PushStyle(...)`), and managing layouts (`glimmer::BeginLayout(...)`, `glimmer::Move(...)`, `glimmer::EndLayout()`).
6.	Widget Interaction:
•	Obtain unique IDs for widgets using `glimmer::GetNextId(WidgetType)`.
•	Access and modify widget-specific state using `glimmer::GetWidgetState(id).state.<widget_specific_state>`.
•	Widget functions (like `glimmer::Button()`) return a `WidgetDrawResult` which can indicate events like Clicked.
Look at `GlimmerTest/test.cpp` file for an example.

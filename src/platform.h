#pragma once

#include "types.h"

#ifndef GLIMMER_HIDE_IMGUI_DEPENDENCY
#define GLIMMER_KEY_ENUM_START ImGuiKey_NamedKey_BEGIN
#define GLIMMER_KEY_ENUM_END ImGuiKey_NamedKey_END
#else
#define GLIMMER_KEY_ENUM_START 512
#define GLIMMER_KEY_ENUM_END 667
#endif

#include <string_view>
#include <vector>
#include <stdint.h>

namespace glimmer
{
#ifndef GLIMMER_HIDE_IMGUI_DEPENDENCY

    enum class MouseButton
    {
        LeftMouseButton = ImGuiMouseButton_Left,
        RightMouseButton = ImGuiMouseButton_Right,
        MiddleMouseButton = ImGuiMouseButton_Middle,
        Total
    };

    enum KeyModifiers : int32_t
    {
        CtrlKeyMod = ImGuiMod_Ctrl,
        ShiftKeyMod = ImGuiMod_Shift,
        AltKeyMod = ImGuiMod_Alt,
        SuperKeyMod = ImGuiMod_Super,
    };

    enum class MouseCursor
    {
        None = ImGuiMouseCursor_None,
        Arrow = ImGuiMouseCursor_Arrow,
        TextInput = ImGuiMouseCursor_TextInput, 
        ResizeAll = ImGuiMouseCursor_ResizeAll,
        ResizeVertical = ImGuiMouseCursor_ResizeNS,
        ResizeHorizontal = ImGuiMouseCursor_ResizeEW,
        ResizeTopRight = ImGuiMouseCursor_ResizeNESW,
        ResizeTopLeft = ImGuiMouseCursor_ResizeNWSE,
        Grab = ImGuiMouseCursor_Hand,
        Waiting = ImGuiMouseCursor_Wait,
        Progress = ImGuiMouseCursor_Progress,
        NotAllowed = ImGuiMouseCursor_NotAllowed,
        TotalCursors
    };

#else

    enum class MouseButton
    {
        LeftMouseButton = 0,
        RightMouseButton,
        MiddleMouseButton,
        Total
    };

    enum KeyModifiers : int32_t
    {
        CtrlKeyMod = 4096,
        ShiftKeyMod = 8192,
        AltKeyMod = 16384,
        SuperKeyMod = 32768,
    };

    enum class MouseCursor
    {
        None = -1,
        Arrow,
        TextInput,
        ResizeAll,
        ResizeVertical,
        ResizeHorizontal,
        ResizeTopRight,
        ResizeTopLeft,
        Grab,
        Waiting,
        Progress,
        NotAllowed,
        TotalCursors
    };

#endif

    enum Key : int16_t
    {
        Key_Invalid = -1,
        Key_Tab,
        Key_LeftArrow,
        Key_RightArrow,
        Key_UpArrow,
        Key_DownArrow,
        Key_PageUp,
        Key_PageDown,
        Key_Home,
        Key_End,
        Key_Insert,
        Key_Delete,
        Key_Backspace,
        Key_Space,
        Key_Enter,
        Key_Escape,
        Key_LeftCtrl, Key_LeftShift, Key_LeftAlt, Key_LeftSuper,
        Key_RightCtrl, Key_RightShift, Key_RightAlt, Key_RightSuper,
        Key_Menu,
        Key_0, Key_1, Key_2, Key_3, Key_4, Key_5, Key_6, Key_7, Key_8, Key_9,
        Key_A, Key_B, Key_C, Key_D, Key_E, Key_F, Key_G, Key_H, Key_I, Key_J,
        Key_K, Key_L, Key_M, Key_N, Key_O, Key_P, Key_Q, Key_R, Key_S, Key_T,
        Key_U, Key_V, Key_W, Key_X, Key_Y, Key_Z,
        Key_F1, Key_F2, Key_F3, Key_F4, Key_F5, Key_F6,
        Key_F7, Key_F8, Key_F9, Key_F10, Key_F11, Key_F12,
        Key_F13, Key_F14, Key_F15, Key_F16, Key_F17, Key_F18,
        Key_F19, Key_F20, Key_F21, Key_F22, Key_F23, Key_F24,
        Key_Apostrophe,        // '
        Key_Comma,             // ,
        Key_Minus,             // -
        Key_Period,            // .
        Key_Slash,             // /
        Key_Semicolon,         // ;
        Key_Equal,             // =
        Key_LeftBracket,       // [
        Key_Backslash,
        Key_RightBracket,      // ]
        Key_GraveAccent,       // `
        Key_CapsLock,
        Key_ScrollLock,
        Key_NumLock,
        Key_PrintScreen,
        Key_Pause,
        Key_Keypad0, Key_Keypad1, Key_Keypad2, Key_Keypad3, Key_Keypad4,
        Key_Keypad5, Key_Keypad6, Key_Keypad7, Key_Keypad8, Key_Keypad9,
        Key_KeypadDecimal,
        Key_KeypadDivide,
        Key_KeypadMultiply,
        Key_KeypadSubtract,
        Key_KeypadAdd,
        Key_KeypadEnter,
        Key_KeypadEqual,
        Key_AppBack,       // Browser Back
        Key_AppForwardl,     // Browser Forward
        Key_Total
    };

#ifdef _WIN32
    int GetWin32VirtualKey(Key key);
    Key GetGlimmerKey(int vkCode);
#endif

    enum class ButtonStatus : int16_t
    {
        Default, Pressed, Released, DoubleClicked
    };

    enum class CustomEventType
    {
        INVALID, HOTKEY
    };

    struct HotKeyEvent
    {
        Key key;
        int32_t modifiers = 0;
    };

    struct CustomEventData
    {
        CustomEventType type = CustomEventType::INVALID;

        union Data
        {
            HotKeyEvent hotkey;

            Data() {}
        } data;

        CustomEventData() {}
    };

    struct IODescriptor
    {
        ImVec2 mousepos{ FLT_MAX, FLT_MAX };
        ButtonStatus mouseButtonStatus[(int)MouseButton::Total] 
            = { ButtonStatus::Default, ButtonStatus::Default, ButtonStatus::Default };
        float mouseWheel = 0.f;
        float deltaTime = 0.f; // in seconds

        int32_t modifiers = 0;
        Key key[GLIMMER_NKEY_ROLLOVER_MAX + 1];
        ButtonStatus keyStatus[GLIMMER_KEY_ENUM_END - GLIMMER_KEY_ENUM_START + 1];
        CustomEventData custom;
        bool capslock = false;
        bool insert = false;

        IODescriptor();

        bool isLeftMouseDown() const
        {
            return mouseButtonStatus[(int)MouseButton::LeftMouseButton] == ButtonStatus::Pressed;
        }

        bool isLeftMouseDoubleClicked() const
        {
            return mouseButtonStatus[(int)MouseButton::LeftMouseButton] == ButtonStatus::DoubleClicked;
        }

        bool isMouseDown() const
        {
            return (mouseButtonStatus[(int)MouseButton::LeftMouseButton] == ButtonStatus::Pressed) &&
                (mouseButtonStatus[(int)MouseButton::MiddleMouseButton] == ButtonStatus::Pressed) &&
                (mouseButtonStatus[(int)MouseButton::RightMouseButton] == ButtonStatus::Pressed);
        }

        bool clicked() const
        {
            return mouseButtonStatus[(int)MouseButton::LeftMouseButton] == ButtonStatus::Released;
        }

        bool isRightClicked() const
        {
            return mouseButtonStatus[(int)MouseButton::RightMouseButton] == ButtonStatus::Released;
        }

        bool isKeyPressed(Key key) const
        {
            return keyStatus[(int)key] == ButtonStatus::Pressed;
        }
    };

    inline std::vector<std::pair<char, char>> KeyMappings;

    enum class GraphicsAdapter
    {
        Software, Integrated, Dedicated
    };

    constexpr int VSyncFPS = -1;
    constexpr int UnlimitedFPS = 0;
    constexpr int MinimumFPS = 60;

    struct WindowParams
    {
        ImVec2 size{ FLT_MAX, FLT_MAX };
        ImVec2 minsz{ 1280.f, 800.f };
        ImVec2 maxsz{ -1.f, -1.f };
        std::string_view title;
        std::string_view icon;
        int32_t iconType = RT_PATH | RT_PNG;
        uint8_t bgcolor[4] = { 255, 255, 255, 255 };
        int targetFPS = VSyncFPS;
        GraphicsAdapter adapter = GraphicsAdapter::Integrated;
        bool fallbackSoftwareAdapter = true;
        bool softwareCursor = false;
    };

    struct UIConfig;

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
            std::string_view confirmBtnText;
            std::string_view cancelBtnText;
        };

        enum class EventHandlerTarget
        {
            ForUI, Generic
        };

        struct EventHandlerDescriptor
        {
            bool (*handler)(void* data, const IODescriptor& desc) = nullptr;
            void* data = nullptr;
            EventHandlerTarget target = EventHandlerTarget::ForUI;
        };

        friend int64_t FramesRendered();

        IPlatform();
        
        virtual void PopulateIODescriptor(const CustomEventData& custom);
        virtual void SetClipboardText(std::string_view input) = 0;
        virtual std::string_view GetClipboardText() = 0;
        virtual bool CreateWindow(const WindowParams& params) = 0;
        virtual bool PollEvents(bool (*runner)(ImVec2, IPlatform&, void*), void* data) = 0;
        virtual ImTextureID UploadTexturesToGPU(ImVec2 size, unsigned char* pixels) = 0;

        virtual void PushEventHandler(const EventHandlerDescriptor& descriptor) {}
        virtual void* GetWindowHandle(void* outptr = nullptr);
        virtual int32_t ShowFileDialog(std::span<char>* out, int32_t outsz, int32_t target,
            std::string_view location, std::pair<std::string_view, std::string_view>* filters = nullptr,
            int totalFilters = 0, const DialogProperties& props = DialogProperties{});

        virtual bool DetermineInitialKeyStates(IODescriptor& desc) { return false; }
        virtual bool RegisterHotkey(const HotKeyEvent& hotkey);
        virtual bool ToggleTextInputMode(bool isTextInputMode) { return false; }

        void SetMouseCursor(MouseCursor cursor);

        IODescriptor CurrentIO(void* ctx = nullptr) const;
        int64_t totalFrames() const { return frameCount; }
        float fps() const;
        UIConfig* config() const;
        bool hasModalDialog() const;

        IODescriptor desc;
        int32_t targetFPS = -1;

    protected:

        bool EnterFrame(float w, float h, const CustomEventData& event);
        void ExitFrame();

        int64_t frameCount = 0;
        int32_t deltaFrames = 0;
        int32_t totalCustomEvents = 0;
        float totalTime = 0.f;
        float totalDeltaTime = 0.f;
        float maxFrameTime = 0.f;
        MouseCursor cursor;
        float bgcolor[4];
        bool softwareCursor = false;
        bool modalDialog = false;
    };

    IPlatform* InitPlatform(ImVec2 size = { -1.f, -1.f });
    int64_t FramesRendered();
}

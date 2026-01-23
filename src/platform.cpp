#include "platform.h"
#include "context.h"
#include "renderer.h"
#include "layout.h"
#include "imrichtext.h"

#include <string>
#include <stdexcept>

#if defined(GLIMMER_ENABLE_NFDEXT) && !defined(__EMSCRIPTEN__)
#include <mutex>
#include "nfd-ext/src/include/nfd.h"
#endif

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinUser.h>
#undef CreateWindow

namespace glimmer
{
    int GetWin32VirtualKey(Key key)
    {
        switch (key)
        {
        case Key_Tab: return VK_TAB;
        case Key_LeftArrow: return VK_LEFT;
        case Key_RightArrow: return VK_RIGHT;
        case Key_UpArrow: return VK_UP;
        case Key_DownArrow: return VK_DOWN;
        case Key_PageUp: return VK_PRIOR;
        case Key_PageDown: return VK_NEXT;
        case Key_Home: return VK_HOME;
        case Key_End: return VK_END;
        case Key_Insert: return VK_INSERT;
        case Key_Delete: return VK_DELETE;
        case Key_Backspace: return VK_BACK;
        case Key_Space: return VK_SPACE;
        case Key_Enter: return VK_RETURN;
        case Key_Escape: return VK_ESCAPE;
        case Key_LeftCtrl: return VK_LCONTROL;
        case Key_LeftShift: return VK_LSHIFT;
        case Key_LeftAlt: return VK_LMENU;
        case Key_LeftSuper: return VK_LWIN;
        case Key_RightCtrl: return VK_RCONTROL;
        case Key_RightShift: return VK_RSHIFT;
        case Key_RightAlt: return VK_RMENU;
        case Key_RightSuper: return VK_RWIN;
        case Key_Menu: return VK_APPS;

        case Key_0: return '0';
        case Key_1: return '1';
        case Key_2: return '2';
        case Key_3: return '3';
        case Key_4: return '4';
        case Key_5: return '5';
        case Key_6: return '6';
        case Key_7: return '7';
        case Key_8: return '8';
        case Key_9: return '9';

        case Key_A: return 'A';
        case Key_B: return 'B';
        case Key_C: return 'C';
        case Key_D: return 'D';
        case Key_E: return 'E';
        case Key_F: return 'F';
        case Key_G: return 'G';
        case Key_H: return 'H';
        case Key_I: return 'I';
        case Key_J: return 'J';
        case Key_K: return 'K';
        case Key_L: return 'L';
        case Key_M: return 'M';
        case Key_N: return 'N';
        case Key_O: return 'O';
        case Key_P: return 'P';
        case Key_Q: return 'Q';
        case Key_R: return 'R';
        case Key_S: return 'S';
        case Key_T: return 'T';
        case Key_U: return 'U';
        case Key_V: return 'V';
        case Key_W: return 'W';
        case Key_X: return 'X';
        case Key_Y: return 'Y';
        case Key_Z: return 'Z';

        case Key_F1: return VK_F1;
        case Key_F2: return VK_F2;
        case Key_F3: return VK_F3;
        case Key_F4: return VK_F4;
        case Key_F5: return VK_F5;
        case Key_F6: return VK_F6;
        case Key_F7: return VK_F7;
        case Key_F8: return VK_F8;
        case Key_F9: return VK_F9;
        case Key_F10: return VK_F10;
        case Key_F11: return VK_F11;
        case Key_F12: return VK_F12;
        case Key_F13: return VK_F13;
        case Key_F14: return VK_F14;
        case Key_F15: return VK_F15;
        case Key_F16: return VK_F16;
        case Key_F17: return VK_F17;
        case Key_F18: return VK_F18;
        case Key_F19: return VK_F19;
        case Key_F20: return VK_F20;
        case Key_F21: return VK_F21;
        case Key_F22: return VK_F22;
        case Key_F23: return VK_F23;
        case Key_F24: return VK_F24;

        case Key_Apostrophe: return VK_OEM_7;      // '
        case Key_Comma: return VK_OEM_COMMA;       // ,
        case Key_Minus: return VK_OEM_MINUS;       // -
        case Key_Period: return VK_OEM_PERIOD;     // .
        case Key_Slash: return VK_OEM_2;           // /
        case Key_Semicolon: return VK_OEM_1;       // ;
        case Key_Equal: return VK_OEM_PLUS;        // =
        case Key_LeftBracket: return VK_OEM_4;     // [
        case Key_Backslash: return VK_OEM_5;       // \ (Backslash)
        case Key_RightBracket: return VK_OEM_6;    // ]
        case Key_GraveAccent: return VK_OEM_3;     // `

        case Key_CapsLock: return VK_CAPITAL;
        case Key_ScrollLock: return VK_SCROLL;
        case Key_NumLock: return VK_NUMLOCK;
        case Key_PrintScreen: return VK_SNAPSHOT;
        case Key_Pause: return VK_PAUSE;

        case Key_Keypad0: return VK_NUMPAD0;
        case Key_Keypad1: return VK_NUMPAD1;
        case Key_Keypad2: return VK_NUMPAD2;
        case Key_Keypad3: return VK_NUMPAD3;
        case Key_Keypad4: return VK_NUMPAD4;
        case Key_Keypad5: return VK_NUMPAD5;
        case Key_Keypad6: return VK_NUMPAD6;
        case Key_Keypad7: return VK_NUMPAD7;
        case Key_Keypad8: return VK_NUMPAD8;
        case Key_Keypad9: return VK_NUMPAD9;
        case Key_KeypadDecimal: return VK_DECIMAL;
        case Key_KeypadDivide: return VK_DIVIDE;
        case Key_KeypadMultiply: return VK_MULTIPLY;
        case Key_KeypadSubtract: return VK_SUBTRACT;
        case Key_KeypadAdd: return VK_ADD;
        case Key_KeypadEnter: return VK_RETURN; // Often distinguished by extended key flag
        case Key_KeypadEqual: return 0; // No standard VK code for Keypad Equal

        case Key_AppBack: return VK_BROWSER_BACK;
        case Key_AppForwardl: return VK_BROWSER_FORWARD;

        default: return 0;
        }
    }

    Key GetGlimmerKey(int vkCode)
    {
        if (vkCode >= '0' && vkCode <= '9')
            return static_cast<Key>(Key_0 + (vkCode - '0'));

        if (vkCode >= 'A' && vkCode <= 'Z')
            return static_cast<Key>(Key_A + (vkCode - 'A'));

        if (vkCode >= VK_F1 && vkCode <= VK_F24)
            return static_cast<Key>(Key_F1 + (vkCode - VK_F1));

        if (vkCode >= VK_NUMPAD0 && vkCode <= VK_NUMPAD9)
            return static_cast<Key>(Key_Keypad0 + (vkCode - VK_NUMPAD0));

        switch (vkCode)
        {
        case VK_TAB: return Key_Tab;
        case VK_LEFT: return Key_LeftArrow;
        case VK_RIGHT: return Key_RightArrow;
        case VK_UP: return Key_UpArrow;
        case VK_DOWN: return Key_DownArrow;
        case VK_PRIOR: return Key_PageUp;
        case VK_NEXT: return Key_PageDown;
        case VK_HOME: return Key_Home;
        case VK_END: return Key_End;
        case VK_INSERT: return Key_Insert;
        case VK_DELETE: return Key_Delete;
        case VK_BACK: return Key_Backspace;
        case VK_SPACE: return Key_Space;
        case VK_RETURN: return Key_Enter;
        case VK_ESCAPE: return Key_Escape;

            // Modifiers
        case VK_LSHIFT: return Key_LeftShift;
        case VK_RSHIFT: return Key_RightShift;
        case VK_LCONTROL: return Key_LeftCtrl;
        case VK_RCONTROL: return Key_RightCtrl;
        case VK_LMENU: return Key_LeftAlt;
        case VK_RMENU: return Key_RightAlt;
        case VK_LWIN: return Key_LeftSuper;
        case VK_RWIN: return Key_RightSuper;

            // Generic modifiers (fallback if L/R not distinguished)
        case VK_SHIFT: return Key_LeftShift;
        case VK_CONTROL: return Key_LeftCtrl;
        case VK_MENU: return Key_LeftAlt;

        case VK_APPS: return Key_Menu;

            // Punctuation and Symbols (Based on US Standard Layout)
        case VK_OEM_7: return Key_Apostrophe;      // '
        case VK_OEM_COMMA: return Key_Comma;       // ,
        case VK_OEM_MINUS: return Key_Minus;       // -
        case VK_OEM_PERIOD: return Key_Period;     // .
        case VK_OEM_2: return Key_Slash;           // /
        case VK_OEM_1: return Key_Semicolon;       // ;
        case VK_OEM_PLUS: return Key_Equal;        // =
        case VK_OEM_4: return Key_LeftBracket;     // [
        case VK_OEM_5: return Key_Backslash;       // \ (Backslash)
        case VK_OEM_6: return Key_RightBracket;    // ]
        case VK_OEM_3: return Key_GraveAccent;     // `

            // Locks
        case VK_CAPITAL: return Key_CapsLock;
        case VK_SCROLL: return Key_ScrollLock;
        case VK_NUMLOCK: return Key_NumLock;
        case VK_SNAPSHOT: return Key_PrintScreen;
        case VK_PAUSE: return Key_Pause;

            // Keypad
        case VK_DECIMAL: return Key_KeypadDecimal;
        case VK_DIVIDE: return Key_KeypadDivide;
        case VK_MULTIPLY: return Key_KeypadMultiply;
        case VK_SUBTRACT: return Key_KeypadSubtract;
        case VK_ADD: return Key_KeypadAdd;
            // Note: Key_KeypadEnter is usually handled by checking KF_EXTENDED on VK_RETURN

            // Browser / Navigation
        case VK_BROWSER_BACK: return Key_AppBack;
        case VK_BROWSER_FORWARD: return Key_AppForwardl; // Typo in original enum 'Forwardl' preserved

        default: return Key_Invalid;
        }
    }
}

static void DetermineKeyStatus(glimmer::IODescriptor& desc)
{
    desc.capslock = GetAsyncKeyState(VK_CAPITAL) < 0;
    desc.insert = GetAsyncKeyState(VK_INSERT) < 0;
}

#if (GLIMMER_TARGET_PLATFORM == GLIMMER_PLATFORM_GLFW) && defined(GLIMMER_FORCE_DEDICATED_GPU)
#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) DWORD NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#ifdef __cplusplus
}
#endif
#endif
#elif __APPLE__
#include <CoreGraphics/CGEventSource.h>

static void DetermineKeyStatus(glimmer::IODescriptor& desc)
{
    desc.capslock = ((CGEventSourceFlagsState(kCGEventSourceStateHIDSystemState) & kCGEventFlagMaskAlphaShift) != 0);
    desc.insert = false;
}

#endif

#if GLIMMER_TARGET_PLATFORM == GLIMMER_PLATFORM_PDCURSES
#include <curses.h>
#include <chrono>

extern "C" 
{
    int PDC_setclipboard(const char* contents, long length);
    int PDC_getclipboard(char** contents, long* length);
    int PDC_freeclipboard(char* contents);
}
#endif

#if GLIMMER_TARGET_PLATFORM == GLIMMER_PLATFORM_SDL3
#include "libs/inc/SDL3/SDL.h"
#include "libs/inc/SDL3/SDL_gpu.h"
#include "libs/inc/imguisdl3/imgui_impl_sdl3.h"
#include "libs/inc/imguisdl3/imgui_impl_sdlgpu3.h"
#include "libs/inc/imguisdl3/imgui_impl_sdlrenderer3.h"

#include <deque>
#include <list>
#endif

#if GLIMMER_TARGET_PLATFORM == GLIMMER_PLATFORM_GLFW
#include "libs/inc/imguiglfw/imgui_impl_glfw.h"
#include "libs/inc/imguiglfw/imgui_impl_opengl3.h"
#include "libs/inc/imguiglfw/imgui_impl_opengl3_loader.h"

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
#endif

namespace glimmer
{
    int64_t FramesRendered()
    {
        return Config.platform->frameCount;
    }

#pragma region IPlatform default implementation

    IODescriptor::IODescriptor()
    {
        for (auto k = 0; k <= GLIMMER_NKEY_ROLLOVER_MAX; ++k) key[k] = Key_Invalid;
        for (auto ks = 0; ks < (GLIMMER_KEY_ENUM_END - GLIMMER_KEY_ENUM_START + 1); ++ks)
            keyStatus[ks] = ButtonStatus::Default;
    }

    IPlatform::IPlatform()
    {
        DetermineInitialKeyStates(desc);
    }

    void IPlatform::PopulateIODescriptor(const CustomEventData& custom)
    {
        auto& io = ImGui::GetIO();
        auto rollover = 0;
        auto escape = false, clicked = false;

        desc.deltaTime = io.DeltaTime;
        desc.mousepos = io.MousePos;
        desc.mouseWheel = io.MouseWheel;
        desc.modifiers = io.KeyMods;
        desc.custom = custom;

        totalTime += io.DeltaTime;

        for (auto idx = 0; idx < ImGuiMouseButton_COUNT; ++idx)
        {
            desc.mouseButtonStatus[idx] =
                ImGui::IsMouseDown(idx) ? ButtonStatus::Pressed :
                ImGui::IsMouseReleased(idx) ? ButtonStatus::Released :
                ImGui::IsMouseDoubleClicked(idx) ? ButtonStatus::DoubleClicked :
                ButtonStatus::Default;
            clicked = clicked || ImGui::IsMouseDown(idx);
        }

        for (int key = Key_Tab; key != Key_Total; ++key)
        {
            auto imkey = ImGuiKey_NamedKey_BEGIN + key;
            if (ImGui::IsKeyPressed((ImGuiKey)imkey))
            {
                if ((ImGuiKey)imkey == ImGuiKey_CapsLock) desc.capslock = !desc.capslock;
                else if ((ImGuiKey)imkey == ImGuiKey_Insert) desc.insert = !desc.insert;
                else
                {
                    if (rollover < GLIMMER_NKEY_ROLLOVER_MAX)
                        desc.key[rollover++] = (Key)key;
                    desc.keyStatus[key] = ButtonStatus::Pressed;
                    escape = imkey == ImGuiKey_Escape;
                }
            }
            else if (ImGui::IsKeyReleased((ImGuiKey)imkey))
                desc.keyStatus[key] = ButtonStatus::Released;
            else
                desc.keyStatus[key] = ButtonStatus::Default;
        }

        while (rollover <= GLIMMER_NKEY_ROLLOVER_MAX)
            desc.key[rollover++] = Key_Invalid;
    }

#if defined(GLIMMER_ENABLE_NFDEXT) && !defined(__EMSCRIPTEN__)

    static int32_t ExtractPaths(std::string_view* out, int32_t outsz, const nfdpathset_t* outPaths)
    {
        nfdpathsetsize_t totalPaths = 0;
        NFD_PathSet_GetCount(outPaths, &totalPaths);
        auto pidx = 0;

        for (; pidx < std::min((int32_t)totalPaths, outsz); ++pidx)
        {
            nfdu8char_t* path = nullptr;
            NFD_PathSet_GetPathU8(outPaths, pidx, &path);
            if (pidx < outsz)
                out[pidx] = std::string_view((const char*)path);
            NFD_PathSet_FreePathU8(path);
        }

        NFD_PathSet_Free(outPaths);
        return pidx;
    }

    int32_t IPlatform::ShowFileDialog(std::span<char>* out, int32_t outsz, int32_t target,
        std::string_view location, std::pair<std::string_view, std::string_view>* filters,
        int totalFilters, const DialogProperties& props)
    {
        assert(out != nullptr && outsz >= 1);
        static Vector<nfdu8filteritem_t, int16_t, 16> filterItems{ false };

        filterItems.clear(true);
        modalDialog = true;

        for (int idx = 0; idx < totalFilters; ++idx)
        {
            nfdu8filteritem_t item{};
            item.name = filters[idx].first.data();
            item.spec = filters[idx].second.data();
            filterItems.emplace_back() = item;
        }

        nfdresult_t result = NFD_ERROR;

        if ((target & OneFile) || (target & MultipleFiles))
        {
            nfdopendialogu8args_t args = { 0 };
            args.filterList = filterItems.data();
            args.filterCount = totalFilters;
            args.defaultPath = location.data();
            GetWindowHandle(&args.parentWindow);

            if (target & MultipleFiles)
            {
                const nfdpathset_t* outPaths = nullptr;
                result = NFD_OpenDialogMultipleU8_With(&outPaths, &args);

                if (result == NFD_OKAY)
                {
                    return ExtractPaths(out, outsz, outPaths);
                }
            }
            else
            {
                nfdu8char_t* outPath = nullptr;
                result = NFD_OpenDialogU8_With(&outPath, &args);
                out[0] = std::string_view((const char*)outPath);
                NFD_FreePathU8(outPath);
                return 1;
            }
        }
        else if ((target & OneDirectory) || (target & MultipleDirectories))
        {
            nfdpickfolderu8args_t args = { 0 };
            args.defaultPath = location.data();
            GetWindowHandle(&args.parentWindow);

            if (target & OneDirectory)
            {
                nfdu8char_t* outPath = nullptr;
                result = NFD_PickFolderU8_With(&outPath, &args);
                out[0] = std::string_view((const char*)outPath);
                NFD_FreePathU8(outPath);
                return 1;
            }
            else
            {
                const nfdpathset_t* outPaths = nullptr;
                result = NFD_PickFolderMultipleU8_With(&outPaths, &args);

                if (result == NFD_OKAY)
                {
                    return ExtractPaths(out, outsz, outPaths);
                }
            }
        }
        else
        {
            LOGERROR("Invalid options...\n");
            return -1;
        }

        modalDialog = false;
        return 0;
    }

#else

    int32_t IPlatform::ShowFileDialog(std::span<char>* out, int32_t outsz, int32_t target,
        std::string_view location, std::pair<std::string_view, std::string_view>* filters,
        int totalFilters, const DialogProperties& props)
    {
        return 0;
    }

#endif

    IODescriptor IPlatform::CurrentIO() const
    {
        auto& context = GetContext();
        auto isWithinPopup = WidgetContextData::ActivePopUpRegion.Contains(desc.mousepos);
        IODescriptor result{};
        result.deltaTime = desc.deltaTime;

        // Either the current context is the popup's context in which case only events that
        // are within the popup matter or the current context is not for the popup and hence
        // ignore events that occured within it.
        if (!isWithinPopup || (isWithinPopup && WidgetContextData::PopupContext == &context))
            result = desc;

        return result;
    }

    bool IPlatform::RegisterHotkey(const HotKeyEvent& hotkey)
    {
#ifdef _WIN32
        HWND window;
        UINT modifiers = 0, vk = 0;
        GetWindowHandle(&window);
        if (hotkey.modifiers & CtrlKeyMod) modifiers |= MOD_CONTROL;
        if (hotkey.modifiers & ShiftKeyMod) modifiers |= MOD_SHIFT;
        if (hotkey.modifiers & AltKeyMod) modifiers |= MOD_ALT;
        if (hotkey.modifiers & SuperKeyMod) modifiers |= MOD_WIN;
        return RegisterHotKey(window, -1, modifiers, GetWin32VirtualKey(hotkey.key));
#endif

        totalCustomEvents++;
    }

    void IPlatform::SetMouseCursor(MouseCursor _cursor)
    {
        cursor = _cursor;
    }

    void* IPlatform::GetWindowHandle(void* out)
    {
        out = nullptr;
        return nullptr;
    }

    bool IPlatform::EnterFrame(float width, float height, const CustomEventData& custom)
    {
        auto color = ToRGBA(bgcolor[0], bgcolor[1], bgcolor[2], bgcolor[3]);

        if (Config.renderer->InitFrame(width, height, color, softwareCursor))
        {
            PopulateIODescriptor(custom);
            InitFrameData();
            cursor = MouseCursor::Arrow;
            return true;
        }

        return false;
    }

    void IPlatform::ExitFrame()
    {
        ++frameCount; ++deltaFrames;
        totalDeltaTime += desc.deltaTime;
        maxFrameTime = std::max(maxFrameTime, desc.deltaTime);

        for (auto idx = 0; idx < GLIMMER_NKEY_ROLLOVER_MAX; ++idx)
            desc.key[idx] = Key_Invalid;

        ResetFrameData();

        if (totalDeltaTime > 1.f)
        {
#ifdef _DEBUG
            auto fps = ((float)deltaFrames / totalDeltaTime);
            if (fps >= (float)targetFPS)
                LOG("Total Frames: %d | Current FPS: %.0f | Max Frame Time: %.0fms\n", deltaFrames,
                    fps, (maxFrameTime * 1000.f));
            else
                LOGERROR("Total Frames: %d | Current FPS: %.0f | Max Frame Time: %.0fms\n", deltaFrames,
                    fps, (maxFrameTime * 1000.f));
            LOG("*alloc calls in last 1s: %d | Allocated: %d bytes\n", TotalMallocs, AllocatedBytes);
            TotalMallocs = 0;
            AllocatedBytes = 0;
#endif
            maxFrameTime = 0.f;
            totalDeltaTime = 0.f;
            deltaFrames = 0;
        }

        Config.renderer->FinalizeFrame((int32_t)cursor);
    }

    float IPlatform::fps() const
    {
        return (float)frameCount / totalTime;
    }

    UIConfig* IPlatform::config() const
    {
        return &Config;
    }

    bool IPlatform::hasModalDialog() const
    {
        return modalDialog;
    }

#pragma endregion

#pragma region TUI platform
#if GLIMMER_TARGET_PLATFORM == GLIMMER_PLATFORM_PDCURSES

#ifdef __linux__
}

#include <sys/ioctl.h>
#include <linux/kd.h>
#include <unistd.h>

static bool IsLinuxCapsLockOn() 
{
    char flags = 0;
    if (ioctl(STDIN_FILENO, KDGKBLED, &flags) == 0) 
        return (flags & K_CAPSLOCK);
    return false;
}

namespace glimmer {
#endif

    static Key MapPDCursesKey(int ch)
    {
        if (ch >= 'a' && ch <= 'z') return (Key)(Key_A + (ch - 'a'));
        if (ch >= 'A' && ch <= 'Z') return (Key)(Key_A + (ch - 'A'));
        if (ch >= '0' && ch <= '9') return (Key)(Key_0 + (ch - '0'));

        switch (ch)
        {
        case KEY_UP:        return Key_UpArrow;
        case KEY_DOWN:      return Key_DownArrow;
        case KEY_LEFT:      return Key_LeftArrow;
        case KEY_RIGHT:     return Key_RightArrow;
        case KEY_HOME:      return Key_Home;
        case KEY_END:       return Key_End;
        case KEY_PPAGE:     return Key_PageUp;
        case KEY_NPAGE:     return Key_PageDown;
        case KEY_IC:        return Key_Insert;
        case KEY_DC:        return Key_Delete;
        case KEY_BACKSPACE: return Key_Backspace;
        case 127:           return Key_Backspace;
        case '\t':          return Key_Tab;
        case '\n':          return Key_Enter;
        case 27:            return Key_Escape;
        case ' ':           return Key_Space;
        case KEY_F(1): return Key_F1;
        case KEY_F(2): return Key_F2;
        case KEY_F(3): return Key_F3;
        case KEY_F(4): return Key_F4;
        case KEY_F(5): return Key_F5;
        case KEY_F(6): return Key_F6;
        case KEY_F(7): return Key_F7;
        case KEY_F(8): return Key_F8;
        case KEY_F(9): return Key_F9;
        case KEY_F(10): return Key_F10;
        case KEY_F(11): return Key_F11;
        case KEY_F(12): return Key_F12;
        default: return Key_Invalid;
        }
    }

    struct PDCursesPlatform : public IPlatform
    {
        std::string clipboardBuffer;
        bool initialized = false;

        PDCursesPlatform()
        {
            // Initialization is defered to CreateWindow usually, 
            // or we can do it here if we want the platform to own the context globally.
        }

        ~PDCursesPlatform()
        {
            if (initialized)
            {
                endwin();
            }
        }

        void PopulateIODescriptor(const CustomEventData& custom) override
        {
            // Reset per-frame ephemeral data
            desc.mouseWheel = 0.f;

            // Clear "Pressed" status from previous frame (simple logic)
            for (auto& status : desc.keyStatus)
            {
                if (status == ButtonStatus::Pressed) status = ButtonStatus::Default;
            }

            // Clear Key Queue
            int keyIndex = 0;
            for (int i = 0; i <= GLIMMER_NKEY_ROLLOVER_MAX; ++i) desc.key[i] = Key_Invalid;

            // Poll input (Non-blocking)
            int ch = wgetch(stdscr);

            while (ch != ERR)
            {
                if (ch == KEY_MOUSE)
                {
                    MEVENT event;
                    if (getmouse(&event) == OK)
                    {
                        desc.mousepos.x = (float)event.x;
                        desc.mousepos.y = (float)event.y;

                        // Left
                        if (event.bstate & BUTTON1_PRESSED)
                            desc.mouseButtonStatus[(int)MouseButton::LeftMouseButton] = ButtonStatus::Pressed;
                        else if (event.bstate & BUTTON1_RELEASED || event.bstate & BUTTON1_CLICKED)
                            desc.mouseButtonStatus[(int)MouseButton::LeftMouseButton] = ButtonStatus::Released;

                        // Right
                        if (event.bstate & BUTTON3_PRESSED)
                            desc.mouseButtonStatus[(int)MouseButton::RightMouseButton] = ButtonStatus::Pressed;
                        else if (event.bstate & BUTTON3_RELEASED || event.bstate & BUTTON3_CLICKED)
                            desc.mouseButtonStatus[(int)MouseButton::RightMouseButton] = ButtonStatus::Released;

                        // Wheel
                        if (event.bstate & 0x00080000) desc.mouseWheel = 1.0f;     // Up
                        else if (event.bstate & 0x00200000) desc.mouseWheel = -1.0f; // Down
                    }
                }
                else if (ch == KEY_RESIZE)
                {
                    resize_term(0, 0);
                }
                else
                {
                    Key key = MapPDCursesKey(ch);
                    if (key != Key_Invalid)
                    {
                        desc.keyStatus[key] = ButtonStatus::Pressed;
                        if (keyIndex < GLIMMER_NKEY_ROLLOVER_MAX) desc.key[keyIndex++] = key;
                    }
                }
                ch = wgetch(stdscr);
            }

            // Modifiers
            unsigned long pdc_mods = PDC_get_key_modifiers();
            desc.modifiers = 0;
            if (pdc_mods & PDC_KEY_MODIFIER_SHIFT) desc.modifiers |= ShiftKeyMod;
            if (pdc_mods & PDC_KEY_MODIFIER_CONTROL) desc.modifiers |= CtrlKeyMod;
            if (pdc_mods & PDC_KEY_MODIFIER_ALT) desc.modifiers |= AltKeyMod;

            // Time
            static auto lastTime = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            std::chrono::duration<float> diff = now - lastTime;
            desc.deltaTime = diff.count();
            lastTime = now;
        }

        void SetClipboardText(std::string_view input) override
        {
            PDC_setclipboard(input.data(), (long)input.size());
        }

        std::string_view GetClipboardText() override
        {
            char* ptr = nullptr;
            long len = 0;
            if (PDC_getclipboard(&ptr, &len) == PDC_CLIP_SUCCESS)
            {
                clipboardBuffer.assign(ptr, len);
                PDC_freeclipboard(ptr);
                return clipboardBuffer;
            }
            return "";
        }

        bool CreateWindow(const WindowParams& params) override
        {
            if (!initialized)
            {
                initscr();
                cbreak();
                noecho();
                nodelay(stdscr, TRUE); // Non-blocking input
                keypad(stdscr, TRUE);

                // Enable mouse events
                mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);

                // Colors
                if (has_colors())
                {
                    start_color();
                    use_default_colors();
                }

                curs_set(0);
                initialized = true;
            }

            // Set title if supported
            if (!params.title.empty())
            {
                PDC_set_title(std::string(params.title).c_str());
            }

            return true;
        }

        bool PollEvents(bool (*runner)(ImVec2, IPlatform&, void*), void* data) override
        {
            bool running = true;

            while (running)
            {
                float w = (float)COLS;
                float h = (float)LINES;

                if (!EnterFrame(w, h))
                {
                    running = false;
                    break;
                }

                running = runner(ImVec2(w, h), *this, data);
                ExitFrame();
                napms(16);
            }

            return true;
        }

        ImTextureID UploadTexturesToGPU(ImVec2 size, unsigned char* pixels) override
        {
            // TUI does not support GPU textures
            return (ImTextureID)0;
        }

        int32_t ShowFileDialog(std::span<char>* out, int32_t outsz, int32_t target,
            std::string_view location, std::pair<std::string_view, std::string_view>* filters,
            int totalFilters, const DialogProperties& props) override
        {
            return 0; // Not implemented
        }

        bool DetermineInitialKeyStates(IODescriptor& desc) override
        {
#ifdef __linux__
            desc.capslock = IsLinuxCapsLockOn();
#else
            DetermineKeyStatus(desc);
#endif
            return true;
        }

        bool RegisterHotkey(const HotKeyEvent& hotkey) override {}
    };

    IPlatform* InitPlatform(ImVec2 size)
    {
        static PDCursesPlatform platform;
        static bool initialized = false;

        if (!initialized)
        {
            initialized = true;
            Config.renderer = CreatePDCursesRenderer();
#ifndef GLIMMER_DISABLE_RICHTEXT
            Config.richTextConfig->Renderer = Config.renderer;
            Config.richTextConfig->RTRenderer->UserData = Config.renderer;
#endif
            PushContext(-1);
        }

        return &platform;
    }

#endif
#pragma endregion

#pragma region SDL3 platform
#if GLIMMER_TARGET_PLATFORM == GLIMMER_PLATFORM_SDL3

#ifdef __linux__
}

#if !defined(GLIMMER_NO_X11) || !defined(__EMSCRIPTEN__)
#include <X11/XKBlib.h>

static bool IsCapsLockOn(Display* display)
{
    XkbStateRec state;
    if (XkbGetState(display, XkbUseCoreKbd, &state) == Success)
        return (state.locked_mods & LockMask);
    return false;
}

static void DetermineKeyStatus(Display* display, glimmer::IODescriptor& desc)
{
    desc.capslock = IsCapsLockOn(display);
    desc.insert = false; // Unlike Windows, there is no gloabl "insert" state in Linux
}
#endif

namespace glimmer {
#endif

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

    static void SetPlatformDeviceProperties(SDL_PropertiesID props)
    {
#if defined(SDL_PLATFORM_WINDOWS)

        // Prefer Direct3D 12 on Windows
        SDL_SetStringProperty(props, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, "direct3d12"); 
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXBC_BOOLEAN, true);

#elif defined(SDL_PLATFORM_APPLE)

        // Prefer Metal on macOS/iOS
        SDL_SetStringProperty(props, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, "metal"); 
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_METALLIB_BOOLEAN, true);

#elif defined(SDL_PLATFORM_LINUX) || defined(SDL_PLATFORM_ANDROID)

        // Prefer Vulkan on Linux/Android
        SDL_SetStringProperty(props, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, "vulkan"); 
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true);
#endif
    }

    static std::list<SDL_GPUTextureSamplerBinding> SamplerBindings;
    
#ifdef _WIN32
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
#endif

    struct ImGuiSDL3Platform final : public IPlatform
    {
        ImGuiSDL3Platform()
        { }

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

            SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
            if (params.size.x == FLT_MAX && params.size.y == FLT_MAX) window_flags |= SDL_WINDOW_MAXIMIZED;

#if defined(SDL_PLATFORM_LINUX) || defined(SDL_PLATFORM_ANDROID)
            window_flags |= SDL_WINDOW_VULKAN;
#elif defined(SDL_PLATFORM_APPLE)
            window_flags |= SDL_WINDOW_METAL;
#endif

            window = SDL_CreateWindow(params.title.data(), (int)params.size.x, (int)params.size.y, window_flags);
            if (window == nullptr)
            {
                printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
                return false;
            }

            if (params.icon.size() > 0)
            {
                SDL_Surface* iconSurface = nullptr;

                if (params.iconType == (RT_PATH | RT_BMP))
                    iconSurface = SDL_LoadBMP(params.icon.data());
#if 0
                else if (params.iconType & RT_ICO)
                {
                    auto stream = (params.iconType & RT_PATH) ? SDL_IOFromFile(params.icon.data(), "r") :
                        SDL_IOFromConstMem(params.icon.data(), params.icon.size());
                    iconSurface = IMG_LoadICO_IO(stream);
                }
#endif
                else
                    assert(false);

                if (iconSurface)
                {
                    SDL_SetWindowIcon(window, iconSurface);
                    SDL_DestroySurface(iconSurface);
                }
            }

            if (!(window_flags & SDL_WINDOW_MAXIMIZED))
                SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

            SDL_ShowWindow(window);
#ifdef _WIN32
            if (totalCustomEvents > 0)
                SDL_SetWindowsMessageHook(&SDL_CustomWindowsMessageHook, &custom);
#endif

            // Create GPU Device
            if (params.adapter != GraphicsAdapter::Software)
            {
                SDL_PropertiesID props = SDL_CreateProperties();
                SetPlatformDeviceProperties(props);

#ifdef _DEBUG
                SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, true);
#else
                SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, false);
#endif

                SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOLEAN, 
                    params.adapter == GraphicsAdapter::Integrated);
                device = SDL_CreateGPUDeviceWithProperties(props);
                SDL_DestroyProperties(props);
            }

            if ((device == nullptr && params.fallbackSoftwareAdapter) || 
                (params.adapter == GraphicsAdapter::Software))
            {
                printf("Warning [Unable to create GPU device], falling back to software rendering : %s\n", SDL_GetError());
                fallback = SDL_CreateRenderer(window, "software");
                targetFPS = params.targetFPS;

                if (params.targetFPS == -1)
                {
                    SDL_SetRenderVSync(fallback, 1);
                    auto display = SDL_GetDisplayForWindow(window);
                    auto mode = SDL_GetCurrentDisplayMode(display);
                    targetFPS = (int)mode->refresh_rate;
                }

                Config.renderer = CreateSoftwareRenderer();
#ifndef GLIMMER_DISABLE_RICHTEXT
                Config.richTextConfig->Renderer = Config.renderer;
                Config.richTextConfig->RTRenderer->UserData = Config.renderer;
#endif

                if (fallback == nullptr)
                {
                    printf("Error: Could not create SDL renderer: %s\n", SDL_GetError());
                    return false;
                }
            }
            else
            {
                // Claim window for GPU Device
                if (!SDL_ClaimWindowForGPUDevice(device, window))
                {
                    printf("Error: SDL_ClaimWindowForGPUDevice(): %s\n", SDL_GetError());
                    return false;
                }

                targetFPS = params.targetFPS;

                if (params.targetFPS == -1)
                {
                    auto display = SDL_GetDisplayForWindow(window);
                    auto mode = SDL_GetCurrentDisplayMode(display);
                    targetFPS = (int)mode->refresh_rate;
                    SDL_SetGPUSwapchainParameters(device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);
                }
                else
                    SDL_SetGPUSwapchainParameters(device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_IMMEDIATE);

                Config.renderer = CreateImGuiRenderer();
            }

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

            device ? ImGui_ImplSDL3_InitForSDLGPU(window) : ImGui_ImplSDL3_InitForSDLRenderer(window, fallback);
            return true;
        }

        void PushEventHandler(bool (*callback)(void* data, const IODescriptor& desc), void* data) override
        {
            handlers.emplace_back(data, callback);
        }

        bool PollEvents(bool (*runner)(ImVec2, IPlatform&, void*), void* data)
        {
            // TODO: If using Blend2D renderer, additional changes are required to copy
            // from BLContext to SDL frame
            if (device)
            {
                ImGui_ImplSDLGPU3_InitInfo init_info = {};
                init_info.Device = device;
                init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(device, window);
                init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
                ImGui_ImplSDLGPU3_Init(&init_info);
            }
            else
                ImGui_ImplSDLRenderer3_Init(fallback);

            bool done = false;
            while (!done)
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
                device ? ImGui_ImplSDLGPU3_NewFrame() : ImGui_ImplSDLRenderer3_NewFrame();
                ImGui_ImplSDL3_NewFrame();

                if (EnterFrame(width, height, custom.empty() ? CustomEventData{} : custom.front()))
                {
                    done = !runner(ImVec2{ (float)width, (float)height }, *this, data);

                    for (auto [data, handler] : handlers)
                        done = !handler(data, desc) && done;
                }

                ExitFrame();

                if (device)
                {
                    ImDrawData* draw_data = ImGui::GetDrawData();
                    const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);

                    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device); // Acquire a GPU command buffer

                    SDL_GPUTexture* swapchain_texture;
                    SDL_AcquireGPUSwapchainTexture(command_buffer, window, &swapchain_texture, nullptr, nullptr); // Acquire a swapchain texture

                    if (swapchain_texture != nullptr && !is_minimized)
                    {
                        // This is mandatory: call ImGui_ImplSDLGPU3_PrepareDrawData() to upload the vertex/index buffer!
                        ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);

                        // Setup and start a render pass
                        SDL_GPUColorTargetInfo target_info = {};
                        target_info.texture = swapchain_texture;
                        target_info.clear_color = SDL_FColor{ bgcolor[0], bgcolor[1], bgcolor[2], bgcolor[3] };
                        target_info.load_op = SDL_GPU_LOADOP_CLEAR;
                        target_info.store_op = SDL_GPU_STOREOP_STORE;
                        target_info.mip_level = 0;
                        target_info.layer_or_depth_plane = 0;
                        target_info.cycle = false;
                        SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr);

                        // Render ImGui
                        ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, render_pass);

                        SDL_EndGPURenderPass(render_pass);
                    }

                    // Submit the command buffer
                    SDL_SubmitGPUCommandBuffer(command_buffer);
                }
                else
                {
                    auto& io = ImGui::GetIO();
                    SDL_SetRenderScale(fallback, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
                    SDL_SetRenderDrawColorFloat(fallback, bgcolor[0], bgcolor[1], bgcolor[2], bgcolor[3]);
                    SDL_RenderClear(fallback);
                    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), fallback);
                    SDL_RenderPresent(fallback);
                }

                if (resetCustom) custom.clear();
            }

            if (device)
            {
                SDL_WaitForGPUIdle(device);
                ImGui_ImplSDL3_Shutdown();
                ImGui_ImplSDLGPU3_Shutdown();
                ImGui::DestroyContext();

                SDL_ReleaseWindowFromGPUDevice(device, window);
                SDL_DestroyGPUDevice(device);
            }
            else
            {
                ImGui_ImplSDLRenderer3_Shutdown();
                ImGui_ImplSDL3_Shutdown();
                ImGui::DestroyContext();

                SDL_DestroyRenderer(fallback);
            }

            SDL_DestroyWindow(window);
#ifdef GLIMMER_ENABLE_NFDEXT
            NFD_Quit();
#endif
            SDL_Quit();
            Cleanup();
            return done;
        }

        ImTextureID UploadTexturesToGPU(ImVec2 size, unsigned char* pixels)
        {
            if (device)
            {
                SDL_GPUTextureCreateInfo textureInfo = {
                    .type = SDL_GPU_TEXTURETYPE_2D,
                    .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, // Adjust format as needed
                    .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
                    .width = (Uint32)size.x,
                    .height = (Uint32)size.y,
                    .layer_count_or_depth = 1,
                    .num_levels = 1,
                };

                SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &textureInfo);
                if (texture == nullptr)
                {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create destination texture.");
                    return 0;
                }

                SDL_GPUTransferBufferCreateInfo transferInfo{
                    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                    .size = textureInfo.width * textureInfo.height * 4
                };
                SDL_GPUTransferBuffer* transfer_buffer = SDL_CreateGPUTransferBuffer(device, &transferInfo);

                void* mapped_data = SDL_MapGPUTransferBuffer(device, transfer_buffer, false);
                memcpy(mapped_data, pixels, transferInfo.size);
                SDL_UnmapGPUTransferBuffer(device, transfer_buffer);

                SDL_GPUCommandBuffer* cmd_buffer = SDL_AcquireGPUCommandBuffer(device);
                SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(cmd_buffer);

                // Upload from transfer buffer to texture
                SDL_GPUTextureTransferInfo src_info = {
                    .transfer_buffer = transfer_buffer,
                    .offset = 0
                };

                SDL_GPUTextureRegion dst_region = {
                    .texture = texture,
                    .mip_level = 0,
                    .layer = 0,
                    .x = 0, .y = 0, .z = 0,
                    .w = textureInfo.width, .h = textureInfo.height, .d = 1
                };

                SDL_UploadToGPUTexture(copy_pass, &src_info, &dst_region, false);
                SDL_EndGPUCopyPass(copy_pass);
                SDL_SubmitGPUCommandBuffer(cmd_buffer);

                // Clean up transfer buffer (texture remains on GPU)
                SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);

                SDL_GPUSamplerCreateInfo sampler_info = {};
                sampler_info.min_filter = SDL_GPU_FILTER_LINEAR;
                sampler_info.mag_filter = SDL_GPU_FILTER_LINEAR;
                sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
                sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
                sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
                sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
                sampler_info.mip_lod_bias = 0.0f;
                sampler_info.min_lod = -1000.0f;
                sampler_info.max_lod = 1000.0f;
                sampler_info.enable_anisotropy = false;
                sampler_info.max_anisotropy = 1.0f;
                sampler_info.enable_compare = false;

                auto& binding = SamplerBindings.emplace_back();
                binding.sampler = SDL_CreateGPUSampler(device, &sampler_info);
                binding.texture = texture;

                return (ImTextureID)(intptr_t)(texture);
            }
            else
            {
                auto texture = SDL_CreateTexture(fallback, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, (int)size.x, (int)size.y);
                SDL_UpdateTexture(texture, nullptr, pixels, 4 * (int)size.x);
                SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
                SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
                return (ImTextureID)(intptr_t)(texture);
            }
        }

#if !defined(__EMSCRIPTEN__)

#ifdef GLIMMER_ENABLE_NFDEXT

        void* GetWindowHandle(void* out = nullptr) override
        {
            void* retptr = nullptr;
            std::call_once(nfdInitialized, [] { NFD_Init(); });
            auto nativeWindow = (nfdwindowhandle_t*)out;

#if defined(SDL_PLATFORM_WINDOWS)
            
            nativeWindow->type = NFD_WINDOW_HANDLE_TYPE_WINDOWS;
            nativeWindow->handle = (void*)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
            retptr = (void*)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

#elif defined(SDL_PLATFORM_APPLE)
            
            nativeWindow->type = NFD_WINDOW_HANDLE_TYPE_COCOA;
            nativeWindow->handle = (void*)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
            retptr = (void*)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);

#elif defined(SDL_PLATFORM_LINUX)
            
            if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "x11") == 0)
            {
                nativeWindow->type = NFD_WINDOW_HANDLE_TYPE_X11;
                nativeWindow->handle = (void*)SDL_GetNumberProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, NULL);
                retptr = (void*)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
            }
            else
                retptr = (void*)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL);
#endif
            return retptr;
        }

#else

        void* GetWindowHandle(void* out = nullptr) override
        {
#if defined(SDL_PLATFORM_WINDOWS)

            return (void*)SDL_GetPointerProperty(SDL_GetWindowProperties(window), 
                SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

#elif defined(SDL_PLATFORM_APPLE)

            return (void*)SDL_GetPointerProperty(SDL_GetWindowProperties(window), 
                SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);

#elif defined(SDL_PLATFORM_LINUX)

            if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "x11") == 0)
                (void*)SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                    SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
            else
                return (void*)SDL_GetPointerProperty(SDL_GetWindowProperties(window), 
                    SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL);
#endif
        }

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

#endif

        bool DetermineInitialKeyStates(IODescriptor& desc) override
        {
#if defined(SDL_PLATFORM_LINUX)
            if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "x11") == 0)
                DetermineKeyStatus((Display*)GetWindowHandle(), desc);
#else
            DetermineKeyStatus(desc);
#endif
            return true;
        }

#endif

        bool ToggleTextInputMode(bool isTextInputMode) override
        {
            if (!SDL_HasKeyboard())
            {
                isTextInputMode ? SDL_StartTextInput(window) : SDL_StopTextInput(window);
                return true;
            }
            else return false;
        }

        SDL_Window* window = nullptr;
        SDL_GPUDevice* device = nullptr;
        SDL_Renderer* fallback = nullptr;
#ifdef GLIMMER_ENABLE_NFDEXT
        std::once_flag nfdInitialized;
#endif

        std::vector<std::pair<void*, bool(*)(void*, const IODescriptor&)>> handlers;
        std::deque<CustomEventData> custom;
    };

    IPlatform* InitPlatform(ImVec2 size)
    {
        static ImGuiSDL3Platform platform;
        static bool initialized = false;

        if (!initialized)
        {
            initialized = true;
            RegisterKeyBindings();
            PushContext(-1);
        }

        return &platform;
    }

#endif
#pragma endregion

#pragma region GLFW platform
#if GLIMMER_TARGET_PLATFORM == GLIMMER_PLATFORM_GLFW

#ifdef __linux__
}

#if !defined(GLIMMER_NO_X11) || !defined(__EMSCRIPTEN__)
#include <X11/XKBlib.h>

#define GLFW_EXPOSE_NATIVE_X11
#include <libs/inc/GLFW/glfw3native.h>

static bool IsCapsLockOn(Display* display)
{
    XkbStateRec state;
    if (XkbGetState(display, XkbUseCoreKbd, &state) == Success)
        return (state.locked_mods & LockMask);
    return false;
}

static void DetermineKeyStatus(Display* display, glimmer::IODescriptor& desc)
{
    desc.capslock = IsCapsLockOn(display);
    desc.insert = false; // Unlike Windows, there is no gloabl "insert" state in Linux
}
#endif

namespace glimmer {
#elif defined(_WIN32)
}

#define GLFW_EXPOSE_NATIVE_WIN32
#include <libs/inc/GLFW/glfw3native.h>

namespace glimmer {
#else
}

#define GLFW_EXPOSE_NATIVE_COCOA
#include <libs/inc/GLFW/glfw3native.h>

namespace glimmer {
#endif

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
        ImGuiGLFWPlatform() {}

        void SetClipboardText(std::string_view input)
        {
            static char buffer[GLIMMER_MAX_GLFW_CLIPBOARD_TEXTSZ];

            auto sz = (std::min)(input.size(), (size_t)(GLIMMER_MAX_GLFW_CLIPBOARD_TEXTSZ - 1));
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
                glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
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

                if (EnterFrame(static_cast<float>(width), static_cast<float>(height), CustomEventData{}))
                {
                    close = !runner(ImVec2{ static_cast<float>(width), static_cast<float>(height) }, *this, data);

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

#if !defined(__EMSCRIPTEN__)
#if defined(GLIMMER_ENABLE_NFDEXT)

        void* GetWindowHandle(void* out) override
        {
            std::call_once(nfdInitialized, [] { NFD_Init(); });
            NFD_GetNativeWindowFromGLFWWindow(window, (nfdwindowhandle_t*)out);

#if defined(__linux__)
            return glfwGetX11Display();
#elif defined(_WIN32)
            return glfwGetWin32Window(window);
#else
            return glfwGetCocoaWindow(window);
#endif
        }

#else

        void* GetWindowHandle(void* out) override
        {
#if defined(__linux__)
            return glfwGetX11Display();
#elif defined(_WIN32)
            return glfwGetWin32Window(window);
#else
            return glfwGetCocoaWindow(window);
#endif
        }

#endif

        bool DetermineInitialKeyStates(IODescriptor& desc) override
        {
#if defined(__linux__)
            DetermineKeyStatus(glfwGetX11Display(), desc);
#else
            DetermineKeyStatus(desc);
#endif
            return true;
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
#ifndef GLIMMER_DISABLE_RICHTEXT
            Config.richTextConfig->Renderer = Config.renderer;
            Config.richTextConfig->RTRenderer->UserData = Config.renderer;
#endif
            RegisterKeyBindings();
            PushContext(-1);
        }

        return &platform;
    }

#endif
#pragma endregion
}

#include "platform.h"
#include "context.h"
#include "renderer.h"

#include <string>
#include <stdexcept>

#if defined(GLIMMER_ENABLE_NFDEXT) && !defined(__EMSCRIPTEN__)
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
#elif __linux__
#include <cstdio>
#include <unistd.h>

static std::string exec(const char* cmd)
{
    char buffer[128];
    std::string result = "";
    FILE* pipe = popen(cmd, "r");
    if (!pipe) throw std::runtime_error("popen() failed!");

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        result += buffer;

    pclose(pipe);
    return result;
}

static void DetermineKeyStatus(glimmer::IODescriptor& desc)
{
    std::string xset_output = exec("xset -q | grep Caps | sed -n 's/^.*Caps Lock:\\s*\\(\\S*\\).*$/\\1/p'");
    desc.capslock = xset_output.find("on") != std::string::npos;
    desc.insert = false;
}
#elif __APPLE__
#include <CoreGraphics/CGEventSource.h>
static void DetermineKeyStatus(glimmer::IODescriptor& desc)
{
    desc.capslock = ((CGEventSourceFlagsState(kCGEventSourceStateHIDSystemState) & kCGEventFlagMaskAlphaShift) != 0);
    desc.insert = false;
}
#endif

namespace glimmer
{
    int64_t FramesRendered()
    {
        return Config.platform->frameCount;
    }

    IODescriptor::IODescriptor()
    {
        for (auto k = 0; k <= GLIMMER_NKEY_ROLLOVER_MAX; ++k) key[k] = Key_Invalid;
        for (auto ks = 0; ks < (GLIMMER_KEY_ENUM_END - GLIMMER_KEY_ENUM_START + 1); ++ks)
            keyStatus[ks] = ButtonStatus::Default;
    }

#pragma region IPlatform default implementation

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
        int totalFilters, const DialogProperties* props)
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
        int totalFilters, const DialogProperties* props)
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

    void IPlatform::GetWindowHandle(void* out)
    {
        out = nullptr;
    }

    bool IPlatform::EnterFrame(float width, float height, const CustomEventData& custom)
    {
        auto color = ToRGBA(bgcolor[0], bgcolor[1], bgcolor[2], bgcolor[3]);

        if (Config.renderer->InitFrame(width, height, color, softwareCursor))
        {
#ifndef GLIMMER_CUSTOM_EVENT_BUFFER

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

#endif

            while (rollover <= GLIMMER_NKEY_ROLLOVER_MAX)
                desc.key[rollover++] = Key_Invalid;

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

    bool IPlatform::DetermineInitialKeyStates(IODescriptor& desc)
    {
        DetermineKeyStatus(desc);
        return true;
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
}

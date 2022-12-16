// enable touch functions - set version to windows 7
// mingw otherwise doesn't load touch stuff
#define _WIN32_WINNT 0x0601

#include "wintouchemu.h"

#include <algorithm>
#include <optional>

#include "hooks/graphics/graphics.h"
#include "overlay/overlay.h"
#include "touch/touch.h"
#include "util/detour.h"
#include "util/logging.h"
#include "util/time.h"
#include "util/utils.h"

namespace wintouchemu {

    // settings
    bool FORCE = false;
    bool LOG_FPS = false;

    static inline bool is_emu_enabled() {
        return FORCE || !is_touch_available() || GRAPHICS_SHOW_CURSOR;
    }

    static decltype(GetSystemMetrics) *GetSystemMetrics_orig = nullptr;
    static decltype(RegisterTouchWindow) *RegisterTouchWindow_orig = nullptr;

    static int WINAPI GetSystemMetrics_hook(int nIndex) {

        /*
         * fake touch screen
         * the game requires 0x01 and 0x02 flags to be set
         * 0x40 and 0x80 are set for completeness
         */
        if (nIndex == 94)
            return 0x01 | 0x02 | 0x40 | 0x80;

        // call original
        if (GetSystemMetrics_orig != nullptr) {
            return GetSystemMetrics_orig(nIndex);
        }

        // return error
        return 0;
    }

    static BOOL WINAPI RegisterTouchWindow_hook(HWND hwnd, ULONG ulFlags) {

        // don't register it if the emu is enabled
        if (is_emu_enabled()) {
            return true;
        }

        // call default
        return RegisterTouchWindow_orig(hwnd, ulFlags);
    }

    // state
    BOOL (WINAPI *GetTouchInputInfo_orig)(HANDLE, UINT, PTOUCHINPUT, int);
    bool USE_MOUSE = false;
    std::vector<TouchEvent> TOUCH_EVENTS;
    std::vector<TouchPoint> TOUCH_POINTS;
    HMODULE HOOKED_MODULE = nullptr;
    std::string WINDOW_TITLE_START = "";
    std::optional<std::string> WINDOW_TITLE_END = std::nullopt;
    bool INITIALIZED = false;

    void hook(const char *window_title, HMODULE module) {

        // hooks
        auto system_metrics_hook = detour::iat_try(
                "GetSystemMetrics", GetSystemMetrics_hook, module);
        auto register_touch_window_hook = detour::iat_try(
                "RegisterTouchWindow", RegisterTouchWindow_hook, module);

        // don't hook twice
        if (GetSystemMetrics_orig == nullptr) {
            GetSystemMetrics_orig = system_metrics_hook;
        }
        if (RegisterTouchWindow_orig == nullptr) {
            RegisterTouchWindow_orig = register_touch_window_hook;
        }

        // set module and title
        HOOKED_MODULE = module;
        WINDOW_TITLE_START = window_title;
        INITIALIZED = true;
    }

    void hook_title_ends(const char *window_title_start, const char *window_title_end, HMODULE module) {
        hook(window_title_start, module);

        WINDOW_TITLE_END = window_title_end;
    }

    static BOOL WINAPI GetTouchInputInfoHook(HANDLE hTouchInput, UINT cInputs, PTOUCHINPUT pInputs, int cbSize) {

        // check if original should be called
        if (hTouchInput != GetTouchInputInfoHook) {
            return GetTouchInputInfo_orig(hTouchInput, cInputs, pInputs, cbSize);
        }

        // set touch inputs
        bool result = false;
        bool mouse_used = false;
        static bool mouse_state_old = false;
        for (UINT input = 0; input < cInputs; input++) {
            auto *touch_input = &pInputs[input];

            // clear touch input
            touch_input->x = 0;
            touch_input->y = 0;
            touch_input->hSource = nullptr;
            touch_input->dwID = 0;
            touch_input->dwFlags = 0;
            touch_input->dwMask = 0;
            touch_input->dwTime = 0;
            touch_input->dwExtraInfo = 0;
            touch_input->cxContact = 0;
            touch_input->cyContact = 0;

            // get touch event
            TouchEvent *touch_event = nullptr;
            if (TOUCH_EVENTS.size() > input) {
                touch_event = &TOUCH_EVENTS.at(input);
            }

            // check touch point
            if (touch_event) {

                // set touch point
                result = true;
                touch_input->x = touch_event->x * 100;
                touch_input->y = touch_event->y * 100;
                touch_input->hSource = hTouchInput;
                touch_input->dwID = touch_event->id;
                touch_input->dwFlags = 0;
                switch (touch_event->type) {
                    case TOUCH_DOWN:
                        touch_input->dwFlags |= TOUCHEVENTF_DOWN;
                        break;
                    case TOUCH_MOVE:
                        touch_input->dwFlags |= TOUCHEVENTF_MOVE;
                        break;
                    case TOUCH_UP:
                        touch_input->dwFlags |= TOUCHEVENTF_UP;
                        break;
                }
                touch_input->dwMask = 0;
                touch_input->dwTime = 0;
                touch_input->dwExtraInfo = 0;
                touch_input->cxContact = 0;
                touch_input->cyContact = 0;

            } else if (USE_MOUSE && !mouse_used) {

                // disable further mouse inputs this call
                mouse_used = true;

                // get mouse state
                bool mouse_state = (GetKeyState(VK_LBUTTON) & 0x100) != 0;

                // check event needs to be generated
                if (mouse_state || mouse_state_old) {

                    // get current cursor position
                    POINT cursorPos {};
                    GetCursorPos(&cursorPos);

                    // set touch input to mouse
                    result = true;
                    touch_input->x = cursorPos.x * 100;
                    touch_input->y = cursorPos.y * 100;
                    touch_input->hSource = hTouchInput;
                    touch_input->dwID = 0;
                    touch_input->dwFlags = 0;
                    if (mouse_state && !mouse_state_old) {
                        touch_input->dwFlags |= TOUCHEVENTF_DOWN;
                    } else if (mouse_state && mouse_state_old) {
                        touch_input->dwFlags |= TOUCHEVENTF_MOVE;
                    } else if (!mouse_state && mouse_state_old) {
                        touch_input->dwFlags |= TOUCHEVENTF_UP;
                    }
                    touch_input->dwMask = 0;
                    touch_input->dwTime = 0;
                    touch_input->dwExtraInfo = 0;
                    touch_input->cxContact = 0;
                    touch_input->cyContact = 0;
                }

                // save old state
                mouse_state_old = mouse_state;
            } else {

                /*
                 * For some reason, Nostalgia won't show an active touch point unless a move event
                 * triggers in the same frame. To work around this, we just supply a fake move
                 * event if we didn't update the same pointer ID in the same call.
                 */

                // find touch point which has no associated input event
                TouchPoint *touch_point = nullptr;
                for (auto &tp : TOUCH_POINTS) {
                    bool found = false;
                    for (UINT i = 0; i < cInputs; i++) {
                        if (input > 0 && pInputs[i].dwID == tp.id) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        touch_point = &tp;
                        break;
                    }
                }

                // check if unused touch point was found
                if (touch_point) {

                    // set touch point
                    result = true;
                    touch_input->x = touch_point->x * 100;
                    touch_input->y = touch_point->y * 100;
                    touch_input->hSource = hTouchInput;
                    touch_input->dwID = touch_point->id;
                    touch_input->dwFlags = 0;
                    touch_input->dwFlags |= TOUCHEVENTF_MOVE;
                    touch_input->dwMask = 0;
                    touch_input->dwTime = 0;
                    touch_input->dwExtraInfo = 0;
                    touch_input->cxContact = 0;
                    touch_input->cyContact = 0;
                }
            }
        }

        // return success
        return result;
    }

    void update() {

        // check if initialized
        if (!INITIALIZED) {
            return;
        }

        // no need for hooks if touch is available
        if (!is_emu_enabled()) {
            return;
        }

        // get window handle
        static HWND hWnd = nullptr;
        if (hWnd == nullptr) {

            // start with the active foreground window
            hWnd = GetForegroundWindow();
            auto title = get_window_title(hWnd);

            // if the foreground window does not match the window title start, find a window
            // that does
            if (!string_begins_with(title, WINDOW_TITLE_START)) {
                hWnd = FindWindowBeginsWith(WINDOW_TITLE_START);
                title = get_window_title(hWnd);
            }

            // if a window title end is set, check to see if it matches
            if (WINDOW_TITLE_END.has_value() && !string_ends_with(title.c_str(), WINDOW_TITLE_END.value().c_str())) {
                hWnd = nullptr;
                title = "";

                for (auto &window : find_windows_beginning_with(WINDOW_TITLE_START)) {
                    auto check_title = get_window_title(window);
                    if (string_ends_with(check_title.c_str(), WINDOW_TITLE_END.value().c_str())) {
                        hWnd = std::move(window);
                        title = std::move(check_title);
                        break;
                    }
                }
            }

            // check window
            if (hWnd == nullptr) {
                return;
            }

            // check if windowed
            if (GRAPHICS_WINDOWED) {
                log_info("wintouchemu", "activating window hooks");

                // get client size
                RECT rect {};
                GetClientRect(hWnd, &rect);

                // remove style borders
                LONG lStyle = GetWindowLong(hWnd, GWL_STYLE);
                lStyle &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU);
                SetWindowLongPtr(hWnd, GWL_STYLE, lStyle);

                // remove ex style borders
                LONG lExStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
                lExStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
                SetWindowLongPtr(hWnd, GWL_EXSTYLE, lExStyle);

                // update window
                AdjustWindowRect(&rect, lStyle, FALSE);
                SetWindowPos(hWnd, nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                             SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOOWNERZORDER);

                // create touch window - create overlay if not yet existing at this point
                touch_create_wnd(hWnd, overlay::ENABLED && !overlay::OVERLAY);
                USE_MOUSE = false;
            } else {
                log_info("wintouchemu", "activating DirectX hooks");

                // mouse position based input only
                touch_attach_dx_hook();
                USE_MOUSE = true;
            }

            // hooks
            auto GetTouchInputInfo_orig_new = detour::iat_try(
                    "GetTouchInputInfo", GetTouchInputInfoHook, HOOKED_MODULE);
            if (GetTouchInputInfo_orig == nullptr) {
                GetTouchInputInfo_orig = GetTouchInputInfo_orig_new;
            }
        }

        // update touch events
        if (hWnd != nullptr) {

            // get touch events
            TOUCH_EVENTS.clear();
            touch_get_events(TOUCH_EVENTS);

            // get touch points
            TOUCH_POINTS.clear();
            touch_get_points(TOUCH_POINTS);

            // get event count
            auto event_count = TOUCH_EVENTS.size();
            if (USE_MOUSE) {
                event_count++;
            }

            // for the fake move events
            event_count += MAX(0, (int) (TOUCH_POINTS.size() - TOUCH_EVENTS.size()));

            // check if new events are available
            if (event_count > 0) {

                // send fake event to make the game update it's touch inputs
                auto wndProc = (WNDPROC) GetWindowLongPtr(hWnd, GWLP_WNDPROC);
                wndProc(hWnd, WM_TOUCH, MAKEWORD(event_count, 0), (LPARAM) GetTouchInputInfoHook);
            }

            // update frame logging
            if (LOG_FPS) {
                static int log_frames = 0;
                static uint64_t log_time = 0;
                log_frames++;
                if (log_time < get_system_seconds()) {
                    if (log_time > 0) {
                        log_info("wintouchemu", "polling at {} touch frames per second", log_frames);
                    }
                    log_frames = 0;
                    log_time = get_system_seconds();
                }
            }
        }
    }
}

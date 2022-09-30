#include "jb.h"

#include <windows.h>

#include "avs/game.h"
#include "hooks/graphics/graphics.h"
#include "touch/touch.h"
#include "util/logging.h"
#include "util/utils.h"
#include "util/detour.h"
#include "util/libutils.h"

namespace games::jb {

    // touch stuff
    static bool TOUCH_ENABLE = false;
    static bool TOUCH_ATTACHED = false;
    static bool IS_PORTRAIT = true;
    static std::vector<TouchPoint> TOUCH_POINTS;
    bool TOUCH_STATE[16];

    void touch_update() {

        // check if touch enabled
        if (!TOUCH_ENABLE) {
            return;
        }

        // attach touch module
        if (!TOUCH_ATTACHED) {

            /*
             * Find the game window.
             * We check the foreground window first, then fall back to searching for the window title
             * All game versions seem to have their model first in the window title
             */
            HWND wnd = GetForegroundWindow();
            if (!string_begins_with(GetActiveWindowTitle(), avs::game::MODEL)) {
                wnd = FindWindowBeginsWith(avs::game::MODEL);
            }

            // check if we have a window handle
            if (!wnd) {
                log_warning("jubeat", "could not find window handle for touch");
                TOUCH_ENABLE = false;
                return;
            }

            // attach touch hook
            log_info("jubeat", "using window handle for touch: {}", fmt::ptr(wnd));
            touch_create_wnd(wnd, true);

            // show cursor
            if (GRAPHICS_SHOW_CURSOR) {
                ShowCursor(TRUE);
            }

            // earlier games use a different screen orientation
            if (!avs::game::is_model("L44")) {
                IS_PORTRAIT = false;
            }

            // set attached
            TOUCH_ATTACHED = true;
        }

        // reset touch state
        memset(TOUCH_STATE, 0, sizeof(TOUCH_STATE));

        // check touch points
        TOUCH_POINTS.clear();
        touch_get_points(TOUCH_POINTS);
        auto offset = IS_PORTRAIT ? 580 : 0;
        for (auto &tp : TOUCH_POINTS) {

            // get grid coordinates
            int x = tp.x * 4 / 768;
            int y = (tp.y - offset) * 4 / (1360 - 580);

            // set the corresponding state
            int index = y * 4 + x;
            if (index >= 0 && index < 16) {
                TOUCH_STATE[index] = true;
            }
        }
    }

    /*
     * to fix "IP ADDR CHANGE" errors on boot and in-game when using weird network setups such as a VPN
     */
    static BOOL __stdcall network_addr_is_changed() {
        return 0;
    }

    /*
     * to fix lag spikes when game tries to ping "eamuse.konami.fun" every few minutes
     */
    static BOOL __stdcall network_get_network_check_info() {
        return 0;
    }

    /*
     * to fix network error on non DHCP interfaces
     */
    static BOOL __cdecl network_get_dhcp_result() {
        return 1;
    }

    static int __cdecl GFDbgSetReportFunc(void *func) {
        log_misc("jubeat", "GFDbgSetReportFunc hook hit");

        return 0;
    }

    JBGame::JBGame() : Game("Jubeat") {
    }

    void JBGame::attach() {
        Game::attach();

        // enable touch
        TOUCH_ENABLE = true;

        // enable debug logging of gftools
        HMODULE gftools = libutils::try_module("gftools.dll");
        detour::inline_hook((void *) GFDbgSetReportFunc, libutils::try_proc(
                gftools, "GFDbgSetReportFunc"));

        // apply patches
        HMODULE network = libutils::try_module("network.dll");
        detour::inline_hook((void *) network_addr_is_changed, libutils::try_proc(
                network, "network_addr_is_changed"));
        detour::inline_hook((void *) network_get_network_check_info, libutils::try_proc(
                network, "network_get_network_check_info"));
        detour::inline_hook((void *) network_get_dhcp_result, libutils::try_proc(
                network, "network_get_dhcp_result"));
    }

    void JBGame::detach() {
        Game::detach();

        // disable touch
        TOUCH_ENABLE = false;
    }
}

#pragma once

#include <vector>
#include "cfg/api.h"

namespace games {

    namespace OverlayButtons {
        enum {
            InsertCoin,
            ToggleConfig,
            ToggleVirtualKeypadP1,
            ToggleVirtualKeypadP2,
            ToggleCardManager,
            ToggleLog,
            ToggleControl,
            TogglePatchManager,
            ToggleSubScreen,
            ToggleVRControl,
            ToggleOverlay,
            NavigatorActivate,
            NavigatorCancel,
            NavigatorUp,
            NavigatorDown,
            NavigatorLeft,
            NavigatorRight,
            Screenshot,
            HotkeyEnable1,
            HotkeyEnable2,
            HotkeyToggle,
            ScreenResize,
            ToggleScreenResize,
        };
    }

    namespace KeypadButtons {
        enum {
            Keypad0,
            Keypad1,
            Keypad2,
            Keypad3,
            Keypad4,
            Keypad5,
            Keypad6,
            Keypad7,
            Keypad8,
            Keypad9,
            Keypad00,
            KeypadDecimal,
            InsertCard,
            Size,
        };
    }

    const std::vector<std::string> &get_games();
    std::vector<Button> *get_buttons(const std::string &game);
    std::vector<Button> *get_buttons_keypads(const std::string &game);
    std::vector<Button> *get_buttons_overlay(const std::string &game);
    std::vector<Analog> *get_analogs(const std::string &game);
    std::vector<Light> *get_lights(const std::string &game);
    std::vector<Option> *get_options(const std::string &game);
    std::vector<std::string> *get_game_file_hints(const std::string &game);
}

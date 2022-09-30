#include "buttons.h"
#include "external/LuaBridge.h"
#include "games/io.h"
#include "misc/eamuse.h"
#include "launcher/launcher.h"
#include "util/utils.h"

using namespace luabridge;

namespace script::api::buttons {
    typedef std::unordered_map<std::string, std::unordered_map<std::string, std::string>> button_data_t;

    static std::vector<Button> *get_buttons() {
        static thread_local std::vector<Button> *buttons = nullptr;
        if (!buttons) {
            if (!(buttons = games::get_buttons(eamuse_get_game()))) {
                static auto empty = std::vector<Button>();
                return &empty;
            }
        }
        return buttons;
    }

    auto read() {
        button_data_t ret;
        for (auto &button : *get_buttons()) {
            auto &map = ret[button.getName()];
            map["state"] = std::to_string(GameAPI::Buttons::getState(RI_MGR, button));
            map["enabled"] = std::to_string(button.override_enabled);
        }
        return ret;
    }

    void write(button_data_t data) {
        for (auto &[name, map] : data) {

            // get state
            auto state_it = map.find("state");
            if (state_it == map.end()) continue;
            try {
                float state;
                if (state_it->second == "true") {
                    state = 1.f;
                } else if (state_it->second == "false") {
                    state = 0.f;
                } else {
                    state = std::stof(state_it->second);
                }

                // find button
                for (auto &button : *get_buttons()) {
                    if (button.getName() == name) {
                        button.override_state = state > 0.f ?
                                GameAPI::Buttons::BUTTON_PRESSED : GameAPI::Buttons::BUTTON_NOT_PRESSED;
                        button.override_velocity = CLAMP(state, 0.f, 1.f);
                        button.override_enabled = true;
                        break;
                    }
                }
            } catch (...) {
                continue;
            }
        }
    }

    void write_reset(const std::string &name) {
        for (auto &button : *get_buttons()) {
            if (name.empty() || button.getName() == name) {
                button.override_enabled = false;
                break;
            }
        }
    }

    void init(lua_State *L) {
        getGlobalNamespace(L)
        .beginNamespace("buttons")
            .addFunction("read", read)
            .addFunction("write", write)
        .endNamespace();
    }
}

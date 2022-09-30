#include "keypads.h"
#include "external/LuaBridge.h"
#include "avs/game.h"
#include "misc/eamuse.h"

using namespace luabridge;

namespace script::api::keypads {

    struct KeypadMapping {
        char character;
        uint16_t state;
    };

    static KeypadMapping KEYPAD_MAPPINGS[] = {
        { '0', 1 << EAM_IO_KEYPAD_0 },
        { '1', 1 << EAM_IO_KEYPAD_1 },
        { '2', 1 << EAM_IO_KEYPAD_2 },
        { '3', 1 << EAM_IO_KEYPAD_3 },
        { '4', 1 << EAM_IO_KEYPAD_4 },
        { '5', 1 << EAM_IO_KEYPAD_5 },
        { '6', 1 << EAM_IO_KEYPAD_6 },
        { '7', 1 << EAM_IO_KEYPAD_7 },
        { '8', 1 << EAM_IO_KEYPAD_8 },
        { '9', 1 << EAM_IO_KEYPAD_9 },
        { 'A', 1 << EAM_IO_KEYPAD_00 },
        { 'D', 1 << EAM_IO_KEYPAD_DECIMAL },
    };

    void write(uint32_t keypad, const std::string &input) {

        // process all chars
        for (auto c : input) {
            uint16_t state = 0;

            // find mapping
            bool mapping_found = false;
            for (auto &mapping : KEYPAD_MAPPINGS) {
                if (_strnicmp(&mapping.character, &c, 1) == 0) {
                    state |= mapping.state;
                    mapping_found = true;
                    break;
                }
            }

            // check for error
            if (!mapping_found) {
                continue;
            }

            /*
             * Write input to keypad.
             * We try to make sure it was accepted by waiting a bit more than two frames.
             */
            DWORD sleep_time = 70;
            if (avs::game::is_model("MDX")) {

                // cuz fuck DDR
                sleep_time = 150;
            }

            // set
            eamuse_set_keypad_overrides(keypad, state);
            Sleep(sleep_time);

            // unset
            eamuse_set_keypad_overrides(keypad, 0);
            Sleep(sleep_time);
        }
    }

    void set(uint32_t keypad, const std::string &keys) {

        // iterate params
        uint16_t state = 0;
        for (auto key : keys) {

            // find mapping
            for (auto &mapping : KEYPAD_MAPPINGS) {
                if (_strnicmp(&mapping.character, &key, 1) == 0) {
                    state |= mapping.state;
                    break;
                }
            }
        }

        // set keypad state
        eamuse_set_keypad_overrides(keypad, state);
    }

    std::string get(uint32_t keypad) {

        // get keypad state
        auto state = eamuse_get_keypad_state(keypad);

        // add keys
        std::string res = "";
        for (auto &mapping : KEYPAD_MAPPINGS) {
            if (state & mapping.state) {
                res += std::to_string(mapping.character);
            }
        }
        return res;
    }

    void init(lua_State *L) {
        getGlobalNamespace(L)
        .beginNamespace("keypads")
            .addFunction("write", write)
            .addFunction("set", set)
            .addFunction("get", get)
        .endNamespace();
    }
}

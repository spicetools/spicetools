#include "analogs.h"
#include "external/LuaBridge.h"
#include "games/io.h"
#include "misc/eamuse.h"
#include "launcher/launcher.h"
#include "util/utils.h"

using namespace luabridge;

namespace script::api::analogs {
    typedef std::unordered_map<std::string, std::unordered_map<std::string, std::string>> analog_data_t;

    static std::vector<Analog> *get_analogs() {
        static thread_local std::vector<Analog> *analogs = nullptr;
        if (!analogs) {
            if (!(analogs = games::get_analogs(eamuse_get_game()))) {
                static auto empty = std::vector<Analog>();
                return &empty;
            }
        }
        return analogs;
    }

    auto read() {
        analog_data_t ret;
        for (auto &analog : *get_analogs()) {
            auto &map = ret[analog.getName()];
            map["state"] = std::to_string(GameAPI::Analogs::getState(RI_MGR, analog));
            map["enabled"] = std::to_string(analog.override_enabled);
        }
        return ret;
    }

    void write(analog_data_t data) {
        for (auto &[name, map] : data) {

            // get state
            auto state_it = map.find("state");
            if (state_it == map.end()) continue;
            try {
                float state = std::stof(state_it->second);

                // find analog
                for (auto &analog : *get_analogs()) {
                    if (analog.getName() == name) {
                        analog.override_state = CLAMP(state, 0.f, 1.f);
                        analog.override_enabled = true;
                        break;
                    }
                }
            } catch (...) {
                continue;
            }
        }
    }

    void write_reset(const std::string &name) {
        for (auto &analog : *get_analogs()) {
            if (name.empty() || analog.getName() == name) {
                analog.override_enabled = false;
                break;
            }
        }
    }

    void init(lua_State *L) {
        getGlobalNamespace(L)
        .beginNamespace("analogs")
            .addFunction("read", read)
            .addFunction("write", write)
        .endNamespace();
    }
}

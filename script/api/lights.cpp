#include "lights.h"
#include "external/LuaBridge.h"
#include "games/io.h"
#include "misc/eamuse.h"
#include "launcher/launcher.h"
#include "util/utils.h"
#include "rawinput/rawinput.h"

using namespace luabridge;

namespace script::api::lights {
    typedef std::unordered_map<std::string, std::unordered_map<std::string, std::string>> light_data_t;

    static std::vector<Light> *get_lights() {
        static thread_local std::vector<Light> *lights = nullptr;
        if (!lights) {
            if (!(lights = games::get_lights(eamuse_get_game()))) {
                static auto empty = std::vector<Light>();
                return &empty;
            }
        }
        return lights;
    }

    auto read() {
        light_data_t ret;
        for (auto &light : *get_lights()) {
            auto &map = ret[light.getName()];
            map["state"] = std::to_string(GameAPI::Lights::readLight(RI_MGR, light));
            map["enabled"] = std::to_string(light.override_enabled);
        }
        return ret;
    }

    auto read_original() {
        light_data_t ret;
        for (auto &light : *get_lights()) {
            auto &map = ret[light.getName()];
            map["state"] = std::to_string(light.last_state);
            map["enabled"] = std::to_string(light.override_enabled);
        }
        return ret;
    }

    void write(light_data_t data) {
        for (auto &[name, map] : data) {

            // get state
            auto state_it = map.find("state");
            if (state_it == map.end()) continue;
            try {
                float state = std::stof(state_it->second);

                // find light
                for (auto &light : *get_lights()) {
                    if (light.getName() == name) {
                        light.override_state = CLAMP(state, 0.f, 1.f);
                        light.override_enabled = true;
                        break;
                    }
                }
            } catch (...) {
                continue;
            }
        }
    }

    void write_reset(const std::string &name) {
        for (auto &light : *get_lights()) {
            if (name.empty() || light.getName() == name) {
                light.override_enabled = false;
                break;
            }
        }
    }

    void update() {
        RI_MGR->devices_flush_output();
    }

    void init(lua_State *L) {
        getGlobalNamespace(L)
        .beginNamespace("lights")
            .addFunction("read", read)
            .addFunction("read_original", read_original)
            .addFunction("write", write)
            .addFunction("write_reset", write_reset)
            .addFunction("update", update)
        .endNamespace();
    }
}

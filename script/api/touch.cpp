#include "touch.h"
#include "external/LuaBridge.h"
#include "touch/touch.h"

using namespace luabridge;

namespace script::api::touch {
    typedef std::vector<std::unordered_map<std::string, std::string>> touch_data_t;

    touch_data_t read() {

        // get touch points
        std::vector<TouchPoint> touch_points;
        touch_get_points(touch_points);

        // add state for each touch point
        touch_data_t ret;
        for (auto &touch : touch_points) {
            auto &state = ret.emplace_back();
            state["id"] = std::to_string(touch.id);
            state["x"] = std::to_string(touch.x);
            state["y"] = std::to_string(touch.y);
            state["mouse"] = std::to_string(touch.mouse);
        }
        return ret;
    }

    void write(touch_data_t data) {

        // get all touch points
        std::vector<TouchPoint> touch_points;
        for (auto &state : data) {
            try {
                uint32_t touch_id = std::stoul(state["id"]);
                int32_t touch_x = std::stol(state["x"]);
                int32_t touch_y = std::stol(state["y"]);
                TouchPoint tp {
                        .id = touch_id,
                        .x = touch_x,
                        .y = touch_y,
                        .mouse = false,
                };
                touch_points.emplace_back(tp);
            } catch (...) {
                continue;
            }
        }

        // apply touch points
        touch_write_points(&touch_points);
    }

    void write_reset(uint32_t id) {
        std::vector<DWORD> touch_point_ids;
        touch_point_ids.push_back(id);
        touch_remove_points(&touch_point_ids);
    }

    void init(lua_State *L) {
        getGlobalNamespace(L)
        .beginNamespace("touch")
            .addFunction("read", read)
            .addFunction("write", write)
            .addFunction("write_reset", write_reset)
        .endNamespace();
    }
}

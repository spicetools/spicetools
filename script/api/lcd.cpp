#include "lcd.h"
#include "external/LuaBridge.h"
#include "games/shared/lcdhandle.h"

using namespace luabridge;

namespace script::api::lcd {

    auto info() {
        std::unordered_map<std::string, std::string> map;
        map["enabled"] = std::to_string(games::shared::LCD_ENABLED);
        map["csm"] = games::shared::LCD_CSM;
        map["bri"] = std::to_string(games::shared::LCD_BRI);
        map["con"] = std::to_string(games::shared::LCD_CON);
        map["bl"] = std::to_string(games::shared::LCD_BL);
        map["red"] = std::to_string(games::shared::LCD_RED);
        map["green"] = std::to_string(games::shared::LCD_GREEN);
        map["blue"] = std::to_string(games::shared::LCD_BLUE);
        return map;
    }

    void init(lua_State *L) {
        getGlobalNamespace(L)
        .beginNamespace("lcd")
            .addFunction("info", info)
        .endNamespace();
    }
}

#include "drs.h"
#include "external/LuaBridge.h"
#include "games/drs/drs.h"
#include "util/utils.h"

using namespace luabridge;

namespace script::api::drs {

    std::string tapeled_get() {
        const size_t len = sizeof(games::drs::DRS_TAPELED);
        return bin2hex((uint8_t*) games::drs::DRS_TAPELED, len);
    }

    void init(lua_State *L) {
        getGlobalNamespace(L)
        .beginNamespace("drs")
            .addFunction("tapeled_get", tapeled_get)
        .endNamespace();
    }
}

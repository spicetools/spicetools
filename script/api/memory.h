#pragma once

#include <lua.hpp>

namespace script::api::memory {
    void init(lua_State *n, bool sandbox);
}

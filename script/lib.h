#pragma once
#include <lua.hpp>

namespace script {

    void load_libs(lua_State *L, bool sandbox = false);
}

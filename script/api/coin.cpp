#include "coin.h"
#include "external/LuaBridge.h"
#include "misc/eamuse.h"

using namespace luabridge;

namespace script::api::coin {

    int get() {
        return eamuse_coin_get_stock();
    }

    void set(int amount) {
        eamuse_coin_set_stock(amount);
    }

    void insert(int amount) {
        if (amount == 1) {
            eamuse_coin_add();
        } else if (amount > 1) {
            eamuse_coin_set_stock(eamuse_coin_get_stock() + amount);
        }
    }

    bool blocker_get() {
        return eamuse_coin_get_block();
    }

    void init(lua_State *L) {
        getGlobalNamespace(L)
        .beginNamespace("coin")
            .addFunction("get", get)
            .addFunction("set", set)
            .addFunction("insert", insert)
            .addFunction("blocker_get", blocker_get)
        .endNamespace();
    }
}

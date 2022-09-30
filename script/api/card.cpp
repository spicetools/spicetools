#include "card.h"
#include "external/LuaBridge.h"
#include "util/utils.h"
#include "misc/eamuse.h"

using namespace luabridge;

namespace script::api::card {

    void insert(int index, const std::string &card_hex) {

        // convert to binary
        uint8_t card_bin[8] {};
        if (!hex2bin(card_hex.c_str(), card_bin)) {
            return;
        }

        // insert card
        eamuse_card_insert(index & 1, card_bin);
    }

    void init(lua_State *L) {
        getGlobalNamespace(L)
        .beginNamespace("card")
            .addFunction("insert", insert)
        .endNamespace();
    }
}

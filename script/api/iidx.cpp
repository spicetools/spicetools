#include "iidx.h"
#include <cstring>
#include "external/LuaBridge.h"
#include "games/iidx/iidx.h"

using namespace luabridge;

namespace script::api::iidx {

    // settings
    static const size_t TICKER_SIZE = 9;

    std::string ticker_get() {
        std::lock_guard<std::mutex> lock(games::iidx::IIDX_LED_TICKER_LOCK);
        return std::string(games::iidx::IIDXIO_LED_TICKER);
    }

    void ticker_set(const std::string &text) {

        // lock
        std::lock_guard<std::mutex> ticker_lock(games::iidx::IIDX_LED_TICKER_LOCK);

        // set to read only
        games::iidx::IIDXIO_LED_TICKER_READONLY = true;

        // set led ticker
        memset(games::iidx::IIDXIO_LED_TICKER, ' ', TICKER_SIZE);
        for (size_t i = 0; i < TICKER_SIZE && i < text.size(); i++) {
            games::iidx::IIDXIO_LED_TICKER[i] = text[i];
        }
    }

    void ticker_reset() {
        std::lock_guard<std::mutex> ticker_lock(games::iidx::IIDX_LED_TICKER_LOCK);
        games::iidx::IIDXIO_LED_TICKER_READONLY = false;
    }

    void init(lua_State *L) {
        getGlobalNamespace(L)
        .beginNamespace("iidx")
            .addFunction("ticker_get", ticker_get)
            .addFunction("ticker_set", ticker_set)
            .addFunction("ticker_reset", ticker_reset)
        .endNamespace();
    }
}

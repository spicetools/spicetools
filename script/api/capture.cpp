#include "capture.h"
#include "external/LuaBridge.h"
#include "hooks/graphics/graphics.h"
#include "util/crypt.h"

using namespace luabridge;

namespace script::api::capture {
    static thread_local std::vector<uint8_t> CAPTURE_BUFFER;

    std::vector<int> get_screens() {

        // get screen IDs
        std::vector<int> screens;
        graphics_screens_get(screens);
        return screens;
    }

    std::string get_jpg(int screen, int quality, int divide) {
        CAPTURE_BUFFER.reserve(1024 * 128);

        // receive JPEG data
        uint64_t timestamp = 0;
        int width = 0;
        int height = 0;
        graphics_capture_trigger(screen);
        bool success = graphics_capture_receive_jpeg(screen, [] (uint8_t byte) {
            CAPTURE_BUFFER.push_back(byte);
        }, true, quality, true, divide, &timestamp, &width, &height);
        if (!success) {
            return std::string();
        }

        // encode to base64
        auto encoded = crypt::base64_encode(
                CAPTURE_BUFFER.data(),
                CAPTURE_BUFFER.size());

        // clear buffer
        CAPTURE_BUFFER.clear();

        // return base64
        return encoded;
    }

    void init(lua_State *L) {
        getGlobalNamespace(L)
        .beginNamespace("capture")
            .addFunction("get_screens", get_screens)
            .addFunction("get_jpg", get_jpg)
        .endNamespace();
    }
}

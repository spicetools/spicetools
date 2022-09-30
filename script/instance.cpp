#include "instance.h"
#include <lua.hpp>
#include "util/logging.h"
#include "lib.h"

namespace script {
    thread_local Instance *INSTANCE;

    struct LuaData {
        lua_State *L = nullptr;

        ~LuaData() {
            if (L) {
                lua_close(L);
                L = nullptr;
            }
        }
    };

    static void lua_hook(lua_State *L, lua_Debug *data) {
        auto instance = INSTANCE;
        do {

            // check if running
            if (!instance->is_running()) {
                luaL_error(L, "_stop_");
                return;
            }

            // check if paused
            if (instance->is_paused()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        } while (instance->is_paused());
    }

    Instance::Instance(const char *str, bool is_path, bool sandbox) {
        if (is_path) {
            log_misc("script", "loading {}", str);
        }

        // create state
        lua = new LuaData();
        lua->L = luaL_newstate();
        if (!lua->L) {
            error = true;
            log_warning("script", "unable to create state");
            return;
        }

        // load libraries
        load_libs(lua->L, sandbox);

        // load script
        int err;
        if (is_path) {
            err = luaL_loadfile(lua->L, str);
        } else {
            err = luaL_loadstring(lua->L, str);
        }

        // check for error
        if (err != 0) {
            error = true;
            auto err_str = std::string(lua_tostring(lua->L, -1));
            if (is_path) {
                log_warning("script", "error loading '{}':\n{}", str, err_str);
            } else {
                log_warning("script", "error parsing:\n{}", err_str);
            }
            return;
        }

        // start script thread
        running = true;
        thread = new std::thread([this, str, is_path] {

            // insert hooks
            INSTANCE = this;
            lua_sethook(lua->L, &lua_hook, LUA_MASKCOUNT, 1024);
            lua_sethook(lua->L, &lua_hook, LUA_MASKCALL, 1);
            lua_sethook(lua->L, &lua_hook, LUA_MASKRET, 1);

            // call script
            auto err = lua_pcall(lua->L, 0, LUA_MULTRET, 0);

            // display error
            if (err != 0) {
                auto err_str = std::string(lua_tostring(lua->L, -1));
                if (!err_str.ends_with("_stop_")) {
                    error = true;
                    if (is_path) {
                        log_warning("script", "runtime error in '{}':\n{}", str, err_str);
                    } else {
                        log_warning("script", "runtime error:\n{}", err_str);
                    }
                }
            }

            // done
            running = false;
        });
    }

    Instance::~Instance() {
        stop();
        delete lua;
    }

    void Instance::join() {
        if (thread) {
            if (thread->joinable()) {
                thread->join();
            }
            std::lock_guard<std::mutex> lock(thread_m);
            delete thread;
            thread = nullptr;
        }
    }

    void Instance::stop() {
        running = false;
        join();
    }

    void Instance::pause() {
        paused = true;
    }

    void Instance::resume() {
        paused = false;
    }
}

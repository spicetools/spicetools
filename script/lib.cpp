#include "lib.h"
#include <thread>
#include <external/LuaBridge.h>
#include "util/logging.h"
#include "util/time.h"
#include "instance.h"
#include "api/analogs.h"
#include "api/buttons.h"
#include "api/capture.h"
#include "api/card.h"
#include "api/coin.h"
#include "api/control.h"
#include "api/drs.h"
#include "api/iidx.h"
#include "api/info.h"
#include "api/keypads.h"
#include "api/lcd.h"
#include "api/lights.h"
#include "api/memory.h"
#include "api/touch.h"

using namespace luabridge;

extern const luaL_Reg *base_funcs_sandbox;
namespace script {

    LUAMOD_API int luaopen_base_sandbox(lua_State *L) {
        lua_pushglobaltable(L);
        luaL_setfuncs(L, base_funcs_sandbox, 0);
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, LUA_GNAME);
        lua_pushliteral(L, LUA_VERSION);
        lua_setfield(L, -2, "_VERSION");
        return 1;
    }

    static const luaL_Reg LUA_LIBRARIES[] = {
            {"_G", luaopen_base},
            {LUA_LOADLIBNAME, luaopen_package},
            {LUA_COLIBNAME, luaopen_coroutine},
            {LUA_TABLIBNAME, luaopen_table},
            {LUA_IOLIBNAME, luaopen_io},
            {LUA_OSLIBNAME, luaopen_os},
            {LUA_STRLIBNAME, luaopen_string},
            {LUA_MATHLIBNAME, luaopen_math},
            {LUA_UTF8LIBNAME, luaopen_utf8},
            {LUA_DBLIBNAME, luaopen_debug},
#if defined(LUA_COMPAT_BITLIB)
            {LUA_BITLIBNAME, luaopen_bit32},
#endif
            {NULL, NULL}
    };

    static const luaL_Reg LUA_SANDBOX[] = {
            {"_G", luaopen_base_sandbox},
            {LUA_COLIBNAME, luaopen_coroutine},
            {LUA_TABLIBNAME, luaopen_table},
            {LUA_STRLIBNAME, luaopen_string},
            {LUA_MATHLIBNAME, luaopen_math},
            {LUA_UTF8LIBNAME, luaopen_utf8},
#if defined(LUA_COMPAT_BITLIB)
            {LUA_BITLIBNAME, luaopen_bit32},
#endif
            {NULL, NULL}
    };

    LuaRef require_sandbox(const std::string &path, lua_State *L) {
        luaL_error(L, "require not available in sandbox");
        return LuaRef(L);
    }

    void sleep(double duration) {
        auto instance = script::INSTANCE;
        double cur_slept = 0.0;
        while (instance->is_running() && cur_slept < duration) {
            auto diff = std::min(0.02, duration);
            std::this_thread::sleep_for(std::chrono::microseconds(
                    (uint64_t) (diff * 1000000ULL)));
            cur_slept += diff;
        }
    }

    double time() {
        return get_performance_seconds();
    }

    void yield() {
        std::this_thread::yield();
    }

    void msgbox(const std::string &s) {
        MessageBoxA(nullptr, s.c_str(), "Script", 0);
    }

    void log_fatal_(const std::string &s) {
        log_fatal("script", "{}", s);
    }

    void log_warning_(const std::string &s) {
        log_warning("script", "{}", s);
    }

    void log_info_(const std::string &s) {
        log_info("script", "{}", s);
    }

    void log_misc_(const std::string &s) {
        log_misc("script", "{}", s);
    }

    uint16_t GetAsyncKeyState_(const int vKey) {
        return GetAsyncKeyState(vKey);
    }

    void load_libs(lua_State *L, bool sandbox) {

        // load STD libs
        for (const luaL_Reg *lib = sandbox ? LUA_SANDBOX : LUA_LIBRARIES; lib->func; ++lib) {
            luaL_requiref(L, lib->name, lib->func, 1);
            lua_pop(L, 1);
        }

        // add base functions
        auto G = getGlobalNamespace(L);
        G.addFunction("sleep", sleep);
        G.addFunction("time", time);
        G.addFunction("yield", yield);
        G.addFunction("msgbox", msgbox);
        G.addFunction("log_fatal", log_fatal_);
        G.addFunction("log_warning", log_warning_);
        G.addFunction("log_info", log_info_);
        G.addFunction("log_misc", log_misc_);
        if (sandbox) {
            G.addFunction("require", require_sandbox);
        } else {
            G.addFunction("GetAsyncKeyState", GetAsyncKeyState_);
        }

        // add API modules
        api::analogs::init(L);
        api::buttons::init(L);
        api::capture::init(L);
        api::card::init(L);
        api::coin::init(L);
        api::control::init(L);
        api::drs::init(L);
        api::iidx::init(L);
        api::info::init(L);
        api::keypads::init(L);
        api::lcd::init(L);
        api::lights::init(L);
        api::memory::init(L, sandbox);
        api::touch::init(L);
    }
}

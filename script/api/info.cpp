#include "coin.h"
#include "external/LuaBridge.h"
#include "avs/game.h"
#include "avs/ea3.h"
#include "build/defs.h"
#include "util/memutils.h"
#include "util/utils.h"

using namespace luabridge;

namespace script::api::info {

    auto avs() {
        std::unordered_map<std::string, std::string> map;
        map["model"] = std::string(avs::game::MODEL);
        map["dest"] = std::string(avs::game::DEST);
        map["spec"] = std::string(avs::game::SPEC);
        map["rev"] = std::string(avs::game::REV);
        map["ext"] = std::string(avs::game::EXT);
        map["services"] = avs::ea3::EA3_BOOT_URL;
        return map;
    }

    auto launcher() {

        // build args
        std::string args;
        for (int count = 0; count < LAUNCHER_ARGC; count++) {
            auto arg = LAUNCHER_ARGV[count];
            if (count > 0) args += " ";
            args += arg;
        }

        // get system time
        auto t_now = std::time(nullptr);
        auto tm_now = *std::gmtime(&t_now);
        auto tm_str = to_string(std::put_time(&tm_now, "%Y-%m-%dT%H:%M:%SZ"));

        // build info
        std::unordered_map<std::string, std::string> map;
        map["version"] = VERSION_STRING;
        map["compile_date"] = __DATE__;
        map["compile_time"] = __TIME__;
        map["system_time"] = tm_str.c_str();
        map["args"] = args;
        return map;
    }

    auto memory() {
        std::unordered_map<std::string, std::string> map;
        map["mem_total"] = memutils::mem_total();
        map["mem_total_used"] = memutils::mem_total_used();
        map["mem_used"] = memutils::mem_used();
        map["vmem_total"] = memutils::vmem_total();
        map["vmem_total_used"] = memutils::vmem_total_used();
        map["vmem_used"] = memutils::vmem_used();
        return map;
    }

    void init(lua_State *L) {
        getGlobalNamespace(L)
        .beginNamespace("info")
            .addFunction("avs", avs)
            .addFunction("launcher", launcher)
            .addFunction("memory", memory)
        .endNamespace();
    }
}

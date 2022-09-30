#include "memory.h"
#include "external/LuaBridge.h"
#include "util/fileutils.h"
#include "util/libutils.h"
#include "util/memutils.h"
#include "util/sigscan.h"
#include "util/utils.h"

using namespace luabridge;

namespace script::api::memory {

    std::string read(const std::string &dll_name, intptr_t offset, uint64_t size) {
        auto dll_path = MODULE_PATH / dll_name;

        // check if file exists in modules
        if (!fileutils::file_exists(dll_path)) {
            return std::string();
        }

        // get module
        auto module = libutils::try_module(dll_name);
        if (!module) {
            return std::string();
        }

        // convert offset to RVA
        offset = libutils::offset2rva(dll_path, offset);
        if (offset == ~0) {
            return std::string();
        }

        // get module information
        MODULEINFO module_info {};
        if (!GetModuleInformation(GetCurrentProcess(), module, &module_info, sizeof(MODULEINFO))) {
            return std::string();
        }

        // check bounds
        auto max = offset + size;
        if ((size_t) max >= (size_t) module_info.lpBaseOfDll + module_info.SizeOfImage) {
            return std::string();
        }

        // read memory to hex (without virtual protect)
        return bin2hex((uint8_t*) module_info.lpBaseOfDll + offset, size);
    }

    bool write(const std::string &dll_name, const std::string &data, intptr_t offset) {
        auto dll_path = MODULE_PATH / dll_name;

        // convert data to bin
        size_t data_bin_size = data.length() / 2;
        auto data_bin = std::make_unique<uint8_t[]>(data_bin_size);
        hex2bin(data.c_str(), data_bin.get());

        // check if file exists in modules
        if (!fileutils::file_exists(dll_path)) {
            return false;
        }

        // get module
        auto module = libutils::try_module(dll_name);
        if (!module) {
            return false;
        }

        // convert offset to RVA
        offset = libutils::offset2rva(dll_path, offset);
        if (offset == ~0) {
            return false;
        }

        // get module information
        MODULEINFO module_info {};
        if (!GetModuleInformation(GetCurrentProcess(), module, &module_info, sizeof(MODULEINFO))) {
            return false;
        }

        // check bounds
        if (offset + data_bin_size >= (size_t) module_info.lpBaseOfDll + module_info.SizeOfImage) {
            return false;
        }
        auto data_pos = reinterpret_cast<uint8_t *>(module_info.lpBaseOfDll) + offset;

        // replace data
        memutils::VProtectGuard guard(data_pos, data_bin_size);
        memcpy(data_pos, data_bin.get(), data_bin_size);
        return true;
    }

    uint64_t signature(const std::string &dll_name, const std::string &signature,
                       const std::string &replacement, uint64_t offset, uint64_t usage) {
        auto dll_path = MODULE_PATH / dll_name;

        // check if file exists in modules
        if (!fileutils::file_exists(dll_path)) {
            return 0;
        }

        // get module
        auto module = libutils::try_module(dll_name);
        if (!module) {
            return 0;
        }

        // execute
        auto result = replace_pattern(
                module,
                signature,
                replacement,
                offset,
                usage
        );

        // check result
        if (!result) {
            return 0;
        }

        // convert to offset
        auto rva = result - reinterpret_cast<intptr_t>(module);
        result = libutils::rva2offset(dll_path, rva);
        if (result == -1) {
            return 0;
        }

        // success
        return result;
    }

    void init(lua_State *L, bool sandbox) {
        auto n = getGlobalNamespace(L).beginNamespace("memory")
            .addFunction("read", read);
        if (!sandbox) {
            n.addFunction("write", write)
            .addFunction("signature", signature)
            .endNamespace();
        } else {
            n.endNamespace();
        }
    }
}

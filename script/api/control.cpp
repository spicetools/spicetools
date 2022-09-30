#include "control.h"
#include <csignal>
#include <cstring>
#include "external/LuaBridge.h"
#include "launcher/shutdown.h"

using namespace luabridge;

namespace script::api::control {

    struct SignalMapping {
        int signum;
        const char* name;
    };
    static SignalMapping SIGNAL_MAPPINGS[] = {
            { SIGABRT, "SIGABRT" },
            { SIGFPE, "SIGFPE" },
            { SIGILL, "SIGILL" },
            { SIGINT, "SIGINT" },
            { SIGSEGV, "SIGSEGV" },
            { SIGTERM, "SIGTERM" },
    };

    static inline bool acquire_shutdown_privs() {

        // check if already acquired
        static bool acquired = false;
        if (acquired)
            return true;

        // get process token
        HANDLE hToken;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
            return false;

        // get the LUID for the shutdown privilege
        TOKEN_PRIVILEGES tkp;
        LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
        tkp.PrivilegeCount = 1;
        tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        // get the shutdown privilege for this process
        AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES) NULL, 0);

        // check for error
        bool success = GetLastError() == ERROR_SUCCESS;
        if (success)
            acquired = true;
        return success;
    }

    void raise(const std::string &signal) {

        // get signal
        int signal_val = -1;
        for (auto mapping : SIGNAL_MAPPINGS) {
            if (_stricmp(mapping.name, signal.c_str()) == 0) {
                signal_val = mapping.signum;
                break;
            }
        }

        // raise if found
        if (signal_val >= 0) {
            ::raise(signal_val);
        }
    }

    void exit(int code) {
        launcher::shutdown(code);
    }

    void restart() {
        launcher::restart();
    }

    bool shutdown() {

        // acquire privileges
        if (!acquire_shutdown_privs())
            return false;

        // exit windows
        if (!ExitWindowsEx(EWX_POWEROFF | EWX_FORCE,
                           SHTDN_REASON_MAJOR_APPLICATION |
                           SHTDN_REASON_MINOR_MAINTENANCE))
            return false;

        // terminate this process
        launcher::shutdown(0);
        return true;
    }

    bool reboot() {

        // acquire privileges
        if (!acquire_shutdown_privs())
            return false;

        // exit windows
        if (!ExitWindowsEx(EWX_REBOOT | EWX_FORCE,
                           SHTDN_REASON_MAJOR_APPLICATION |
                           SHTDN_REASON_MINOR_MAINTENANCE))
            return false;

        // terminate this process
        launcher::shutdown(0);
        return true;
    }

    void init(lua_State *L) {
        getGlobalNamespace(L)
        .beginNamespace("control")
            .addFunction("raise", raise)
            .addFunction("exit", exit)
            .addFunction("restart", restart)
            .addFunction("shutdown", shutdown)
            .addFunction("reboot", reboot)
        .endNamespace();
    }
}

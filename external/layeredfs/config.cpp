#include <string.h>
#include <windows.h>
#include <shellapi.h>

#include "config.h"
#include "utils.h"

#define VERBOSE_FLAG L"--layered-verbose"
#define DEVMODE_FLAG L"--layered-devmode"

namespace layeredfs {

    config_t config{};

    void load_config(void) {
        LPWSTR *szArglist;
        int nArgs;
        int i;

        szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
        if (NULL == szArglist) {
            return;
        }

        for (i = 0; i < nArgs; i++) {
            if (lstrcmpW(szArglist[i], VERBOSE_FLAG) == 0) {
                config.verbose_logs = true;
            } else if (lstrcmpW(szArglist[i], DEVMODE_FLAG) == 0) {
                config.developer_mode = true;
            }
        }

        // Free memory allocated for CommandLineToArgvW arguments.
        LocalFree(szArglist);

        logf("Options: %ls=%d %ls=%d", VERBOSE_FLAG, config.verbose_logs, DEVMODE_FLAG, config.developer_mode);
    }
}

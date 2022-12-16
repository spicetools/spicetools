#pragma once

#include <windows.h>
#include "avs/core.h"

namespace layeredfs {

    extern time_t dll_time;
    extern bool initialized;

    int hook_avs_fs_lstat(const char *name, struct avs::core::avs_stat *st);
    avs::core::avs_file_t hook_avs_fs_open(const char *name, uint16_t mode, int flags);
    int init(void);
}

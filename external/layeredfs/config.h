#pragma once

namespace layeredfs {

    typedef struct config {
        bool verbose_logs = false;
        bool developer_mode = false;
    } config_t;

    extern config_t config;

    void load_config(void);
}

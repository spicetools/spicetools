#pragma once

#include <windows.h>

namespace wintouchemu {

    // settings
    extern bool FORCE;
    extern bool LOG_FPS;

    void hook(const char *window_title, HMODULE module = nullptr);
    void hook_title_ends(
        const char *window_title_start,
        const char *window_title_ends,
        HMODULE module = nullptr);
    void update();
}

#pragma once

#include <memory>
#include <string>

namespace cfg {

    class ScreenResize {
    private:
        std::string config_path;
        // bool config_dirty = false;

    public:
        ScreenResize();
        ~ScreenResize();
        
        int offset_x = 0;
        int offset_y = 0;
        float scale_x = 1.0;
        float scale_y = 1.0;
        bool enable_screen_resize = false;
        bool enable_linear_filter = false;
        bool keep_aspect_ratio = false;
        
        void config_load();
        void config_save();
    };

    // globals
    extern std::unique_ptr<cfg::ScreenResize> SCREENRESIZE;
}

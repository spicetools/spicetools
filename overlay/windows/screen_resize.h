#pragma once

#include "overlay/window.h"

namespace overlay::windows {

    class ScreenResize : public Window {
    public:
        ScreenResize(SpiceOverlay *overlay);
        ~ScreenResize() override;

        void build_content() override;
        void update();
        
    private:
        size_t toggle_screen_resize = ~0u;
        bool toggle_screen_resize_state = false;
    };
}

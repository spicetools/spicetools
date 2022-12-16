#pragma once

#include "overlay/window.h"

namespace overlay::windows {

    class VRWindow : public Window {
    public:

        VRWindow(SpiceOverlay *overlay);
        ~VRWindow() override;

        void build_content() override;
        void build_info();
        void build_dancefloor();
    };
}

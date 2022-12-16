#pragma once

#include "overlay/window.h"

namespace overlay::windows {

    class FPS : public Window {
    public:

        FPS(SpiceOverlay *overlay);

        const ImVec2 initial_pos() override;
        void build_content() override;
    };
}

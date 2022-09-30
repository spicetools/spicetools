#pragma once

#include <vector>
#include "cfg/api.h"

namespace games::drs {

    // all buttons in correct order
    namespace Buttons {
        enum {
            Service,
            Test,
            CoinMech,
            P1_Start,
            P1_Up,
            P1_Down,
            P1_Left,
            P1_Right,
            P2_Start,
            P2_Up,
            P2_Down,
            P2_Left,
            P2_Right
        };
    }

    // all lights in correct order
    namespace Lights {
        enum {
        };
    }

    // getters
    std::vector<Button> &get_buttons();
    std::vector<Light> &get_lights();
}

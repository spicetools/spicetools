#pragma once

#include <vector>

#include "cfg/api.h"

namespace games::bc {
    namespace Buttons {
        enum {
            Service,
            Test,
            Up,
            Down,
            Left,
            Right,
            JoystickButton,
            Trigger1,
            Trigger2,
            Button1,
            Button2,
            Button3,
            Button4,
        };
    }

    std::vector<Button> &get_buttons();
}

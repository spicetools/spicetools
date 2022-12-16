#pragma once

#include "games/game.h"

namespace games::rb {

    class RBGame : public games::Game {
    public:
        RBGame();
        virtual void attach();
        virtual void detach();
    };
}

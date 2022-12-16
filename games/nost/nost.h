#pragma once

#include "games/game.h"

namespace games::nost {

    class NostGame : public games::Game {
    public:
        NostGame();
        virtual void attach() override;
    };
}

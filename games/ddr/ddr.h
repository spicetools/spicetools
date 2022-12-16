#pragma once

#include "games/game.h"

namespace games::ddr {

    // settings
    extern bool SDMODE;

    class DDRGame : public games::Game {
    public:
        DDRGame();
        virtual void attach() override;
        virtual void detach() override;
    };
}

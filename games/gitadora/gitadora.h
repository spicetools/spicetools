#pragma once

#include <optional>

#include "games/game.h"

namespace games::gitadora {

    // settings
    extern bool TWOCHANNEL;
    extern std::optional<unsigned int> CAB_TYPE;

    class GitaDoraGame : public games::Game {
    public:
        GitaDoraGame();
        virtual void attach();
    };
}

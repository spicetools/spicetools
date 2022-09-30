#pragma once

#include "games/game.h"

namespace games::sdvx {

    // settings
    extern bool DISABLECAMS;
    extern bool NATIVETOUCH;

    class SDVXGame : public games::Game {
    public:
        SDVXGame();
        virtual void attach() override;
        virtual void post_attach() override;
        virtual void detach() override;
    };
}

#pragma once

#include <vector>
#include "cfg/api.h"

namespace games::gitadora {

    // all buttons in correct order
    namespace Buttons {
        enum {
            Service,
            Test,
            Coin,
            GuitarP1Start,
            GuitarP1Up,
            GuitarP1Down,
            GuitarP1Left,
            GuitarP1Right,
            GuitarP1Help,
            GuitarP1Effect1,
            GuitarP1Effect2,
            GuitarP1Effect3,
            GuitarP1EffectPedal,
            GuitarP1ButtonExtra1,
            GuitarP1ButtonExtra2,
            GuitarP1PickUp,
            GuitarP1PickDown,
            GuitarP1R,
            GuitarP1G,
            GuitarP1B,
            GuitarP1Y,
            GuitarP1P,
            GuitarP1KnobUp,
            GuitarP1KnobDown,
            GuitarP1WailUp,
            GuitarP1WailDown,
            GuitarP2Start,
            GuitarP2Up,
            GuitarP2Down,
            GuitarP2Left,
            GuitarP2Right,
            GuitarP2Help,
            GuitarP2Effect1,
            GuitarP2Effect2,
            GuitarP2Effect3,
            GuitarP2EffectPedal,
            GuitarP2ButtonExtra1,
            GuitarP2ButtonExtra2,
            GuitarP2PickUp,
            GuitarP2PickDown,
            GuitarP2R,
            GuitarP2G,
            GuitarP2B,
            GuitarP2Y,
            GuitarP2P,
            GuitarP2KnobUp,
            GuitarP2KnobDown,
            GuitarP2WailUp,
            GuitarP2WailDown,
            DrumStart,
            DrumUp,
            DrumDown,
            DrumLeft,
            DrumRight,
            DrumHelp,
            DrumButtonExtra1,
            DrumButtonExtra2,
            DrumHiHat,
            DrumHiHatClosed,
            DrumHiHatHalfOpen,
            DrumSnare,
            DrumHiTom,
            DrumLowTom,
            DrumRightCymbal,
            DrumBassPedal,
            DrumLeftCymbal,
            DrumLeftPedal,
            DrumFloorTom
        };
    }

    // all analogs in correct order
    namespace Analogs {
        enum {
            GuitarP1WailX,
            GuitarP1WailY,
            GuitarP1WailZ,
            GuitarP1Knob,
            GuitarP2WailX,
            GuitarP2WailY,
            GuitarP2WailZ,
            GuitarP2Knob
        };
    }

    // all lights in correct order
    namespace Lights {
        enum {
            GuitarP1Motor,
            GuitarP2Motor,
        };
    }

    // getters
    std::vector<Button> &get_buttons();
    std::vector<Analog> &get_analogs();
    std::vector<Light> &get_lights();
}

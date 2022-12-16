#include "io.h"

std::vector<Button> &games::ddr::get_buttons() {
    static std::vector<Button> analogs;

    if (analogs.empty()) {
        analogs = GameAPI::Buttons::getButtons("Dance Dance Revolution");

        GameAPI::Buttons::sortButtons(
                &analogs,
                "Service",
                "Test",
                "Coin Mech",
                "P1 Start",
                "P1 Panel Up",
                "P1 Panel Down",
                "P1 Panel Left",
                "P1 Panel Right",
                "P1 Menu Up",
                "P1 Menu Down",
                "P1 Menu Left",
                "P1 Menu Right",
                "P2 Start",
                "P2 Panel Up",
                "P2 Panel Down",
                "P2 Panel Left",
                "P2 Panel Right",
                "P2 Menu Up",
                "P2 Menu Down",
                "P2 Menu Left",
                "P2 Menu Right"
        );
    }

    return analogs;
}

std::vector<Light> &games::ddr::get_lights() {
    static std::vector<Light> lights;

    if (lights.empty()) {
        lights = GameAPI::Lights::getLights("Dance Dance Revolution");

        GameAPI::Lights::sortLights(
                &lights,
                "P1 Foot Left",
                "P1 Foot Up",
                "P1 Foot Right",
                "P1 Foot Down",
                "P2 Foot Left",
                "P2 Foot Up",
                "P2 Foot Right",
                "P2 Foot Down",
                "Spot Red",
                "Spot Blue",
                "Top Spot Red",
                "Top Spot Blue",
                "P1 Halogen Upper",
                "P1 Halogen Lower",
                "P2 Halogen Upper",
                "P2 Halogen Lower",
                "P1 Button",
                "P2 Button",
                "Neon",
                "HD P1 Start",
                "HD P1 Menu Left-Right",
                "HD P1 Menu Up-Down",
                "HD P2 Start",
                "HD P2 Menu Left-Right",
                "HD P2 Menu Up-Down",
                "HD P1 Speaker F R",
                "HD P1 Speaker F G",
                "HD P1 Speaker F B",
                "HD P1 Speaker W R",
                "HD P1 Speaker W G",
                "HD P1 Speaker W B",
                "HD P2 Speaker F R",
                "HD P2 Speaker F G",
                "HD P2 Speaker F B",
                "HD P2 Speaker W R",
                "HD P2 Speaker W G",
                "HD P2 Speaker W B"
        );
    }

    return lights;
}

#include "io.h"

std::vector<Button> &games::drs::get_buttons() {
    static std::vector<Button> buttons;

    if (buttons.empty()) {
        buttons = GameAPI::Buttons::getButtons("DANCERUSH");

        GameAPI::Buttons::sortButtons(
                &buttons,
                "Service",
                "Test",
                "Coin Mech",
                "P1 Start",
                "P1 Up",
                "P1 Down",
                "P1 Left",
                "P1 Right",
                "P2 Start",
                "P2 Up",
                "P2 Down",
                "P2 Left",
                "P2 Right"
        );
    }

    return buttons;
}

std::vector<Light> &games::drs::get_lights() {
    static std::vector<Light> lights;

    //if (lights.empty()) {
        //lights = GameAPI::Lights::getLights("DANCERUSH");
        //GameAPI::Lights::sortLights(
        //        &lights,
        //);
    //}

    return lights;
}

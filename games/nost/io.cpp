#include "io.h"

std::vector<Button> &games::nost::get_buttons() {
    static std::vector<Button> buttons;

    if (buttons.empty()) {
        buttons = GameAPI::Buttons::getButtons("Nostalgia");

        GameAPI::Buttons::sortButtons(
                &buttons,
                "Service",
                "Test",
                "Coin Mech",
                "Key 1",
                "Key 2",
                "Key 3",
                "Key 4",
                "Key 5",
                "Key 6",
                "Key 7",
                "Key 8",
                "Key 9",
                "Key 10",
                "Key 11",
                "Key 12",
                "Key 13",
                "Key 14",
                "Key 15",
                "Key 16",
                "Key 17",
                "Key 18",
                "Key 19",
                "Key 20",
                "Key 21",
                "Key 22",
                "Key 23",
                "Key 24",
                "Key 25",
                "Key 26",
                "Key 27",
                "Key 28"
        );
    }

    return buttons;
}

std::vector<Analog> &games::nost::get_analogs() {
    static std::vector<Analog> analogs;

    if (analogs.empty()) {
        analogs = GameAPI::Analogs::getAnalogs("Nostalgia");

        GameAPI::Analogs::sortAnalogs(
                &analogs,
                "Key 1",
                "Key 2",
                "Key 3",
                "Key 4",
                "Key 5",
                "Key 6",
                "Key 7",
                "Key 8",
                "Key 9",
                "Key 10",
                "Key 11",
                "Key 12",
                "Key 13",
                "Key 14",
                "Key 15",
                "Key 16",
                "Key 17",
                "Key 18",
                "Key 19",
                "Key 20",
                "Key 21",
                "Key 22",
                "Key 23",
                "Key 24",
                "Key 25",
                "Key 26",
                "Key 27",
                "Key 28"
        );
    }
    return analogs;
}

std::vector<Light> &games::nost::get_lights() {
    static std::vector<Light> lights;

    if (lights.empty()) {
        lights = GameAPI::Lights::getLights("Nostalgia");

        GameAPI::Lights::sortLights(
                &lights,
                "Title R",
                "Title G",
                "Title B",
                "Bottom R",
                "Bottom G",
                "Bottom B",
                "Key 1 R",
                "Key 1 G",
                "Key 1 B",
                "Key 2 R",
                "Key 2 G",
                "Key 2 B",
                "Key 3 R",
                "Key 3 G",
                "Key 3 B",
                "Key 4 R",
                "Key 4 G",
                "Key 4 B",
                "Key 5 R",
                "Key 5 G",
                "Key 5 B",
                "Key 6 R",
                "Key 6 G",
                "Key 6 B",
                "Key 7 R",
                "Key 7 G",
                "Key 7 B",
                "Key 8 R",
                "Key 8 G",
                "Key 8 B",
                "Key 9 R",
                "Key 9 G",
                "Key 9 B",
                "Key 10 R",
                "Key 10 G",
                "Key 10 B",
                "Key 11 R",
                "Key 11 G",
                "Key 11 B",
                "Key 12 R",
                "Key 12 G",
                "Key 12 B",
                "Key 13 R",
                "Key 13 G",
                "Key 13 B",
                "Key 14 R",
                "Key 14 G",
                "Key 14 B",
                "Key 15 R",
                "Key 15 G",
                "Key 15 B",
                "Key 16 R",
                "Key 16 G",
                "Key 16 B",
                "Key 17 R",
                "Key 17 G",
                "Key 17 B",
                "Key 18 R",
                "Key 18 G",
                "Key 18 B",
                "Key 19 R",
                "Key 19 G",
                "Key 19 B",
                "Key 20 R",
                "Key 20 G",
                "Key 20 B",
                "Key 21 R",
                "Key 21 G",
                "Key 21 B",
                "Key 22 R",
                "Key 22 G",
                "Key 22 B",
                "Key 23 R",
                "Key 23 G",
                "Key 23 B",
                "Key 24 R",
                "Key 24 G",
                "Key 24 B",
                "Key 25 R",
                "Key 25 G",
                "Key 25 B",
                "Key 26 R",
                "Key 26 G",
                "Key 26 B",
                "Key 27 R",
                "Key 27 G",
                "Key 27 B",
                "Key 28 R",
                "Key 28 G",
                "Key 28 B"
        );
    }

    return lights;
}

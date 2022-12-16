#include "io.h"

std::vector<Button> &games::gitadora::get_buttons() {
    static std::vector<Button> buttons;

    if (buttons.empty()) {
        buttons = GameAPI::Buttons::getButtons("GitaDora");

        GameAPI::Buttons::sortButtons(&buttons,
                "Service",
                "Test",
                "Coin",
                "Guitar P1 Start",
                "Guitar P1 Up",
                "Guitar P1 Down",
                "Guitar P1 Left",
                "Guitar P1 Right",
                "Guitar P1 Help",
                "Guitar P1 Effect 1",
                "Guitar P1 Effect 2",
                "Guitar P1 Effect 3",
                "Guitar P1 Effect Pedal",
                "Guitar P1 Button Extra 1",
                "Guitar P1 Button Extra 2",
                "Guitar P1 Pick Up",
                "Guitar P1 Pick Down",
                "Guitar P1 R",
                "Guitar P1 G",
                "Guitar P1 B",
                "Guitar P1 Y",
                "Guitar P1 P",
                "Guitar P1 Knob Up",
                "Guitar P1 Knob Down",
                "Guitar P1 Wail Up",
                "Guitar P1 Wail Down",
                "Guitar P2 Start",
                "Guitar P2 Up",
                "Guitar P2 Down",
                "Guitar P2 Left",
                "Guitar P2 Right",
                "Guitar P2 Help",
                "Guitar P2 Effect 1",
                "Guitar P2 Effect 2",
                "Guitar P2 Effect 3",
                "Guitar P2 Effect Pedal",
                "Guitar P2 Button Extra 1",
                "Guitar P2 Button Extra 2",
                "Guitar P2 Pick Up",
                "Guitar P2 Pick Down",
                "Guitar P2 R",
                "Guitar P2 G",
                "Guitar P2 B",
                "Guitar P2 Y",
                "Guitar P2 P",
                "Guitar P2 Knob Up",
                "Guitar P2 Knob Down",
                "Guitar P2 Wail Up",
                "Guitar P2 Wail Down",
                "Drum Start",
                "Drum Up",
                "Drum Down",
                "Drum Left",
                "Drum Right",
                "Drum Help",
                "Drum Button Extra 1",
                "Drum Button Extra 2",
                "Drum Hi-Hat",
                "Drum Hi-Hat Closed",
                "Drum Hi-Hat Half-Open",
                "Drum Snare",
                "Drum Hi-Tom",
                "Drum Low-Tom",
                "Drum Right Cymbal",
                "Drum Bass Pedal",
                "Drum Left Cymbal",
                "Drum Left Pedal",
                "Drum Floor Tom"
        );
    }

    return buttons;
}

std::vector<Analog> &games::gitadora::get_analogs() {
    static std::vector<Analog> analogs;

    if (analogs.empty()) {
        analogs = GameAPI::Analogs::getAnalogs("GitaDora");

        GameAPI::Analogs::sortAnalogs(&analogs,
                "Guitar P1 Wail X",
                "Guitar P1 Wail Y",
                "Guitar P1 Wail Z",
                "Guitar P1 Knob",
                "Guitar P2 Wail X",
                "Guitar P2 Wail Y",
                "Guitar P2 Wail Z",
                "Guitar P2 Knob"
        );
    }

    return analogs;
}

std::vector<Light> &games::gitadora::get_lights() {
    static std::vector<Light> lights;

    if (lights.empty()) {
        lights = GameAPI::Lights::getLights("GitaDora");

        GameAPI::Lights::sortLights(&lights,
                "Guitar P1 Motor",
                "Guitar P2 Motor"
        );
    }

    return lights;
}

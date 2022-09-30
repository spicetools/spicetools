#include "panb.h"
#include "launcher/launcher.h"
#include "rawinput/rawinput.h"
#include "games/nost/io.h"
#include "games/nost/nost.h"
#include "util/logging.h"
#include "avs/game.h"

using namespace GameAPI;

// static stuff
static uint8_t STATUS_BUFFER[277];
static bool STATUS_BUFFER_FREEZE = false;

/*
 * Implementations
 */

static long __cdecl ac_io_panb_control_led_bright(size_t index, uint8_t value) {

    // nostalgia
    if (avs::game::is_model("PAN")) {

        // get lights
        auto &lights = games::nost::get_lights();

        // mapping
        static const size_t mapping[] {
                games::nost::Lights::Key1R, games::nost::Lights::Key1G, games::nost::Lights::Key1B,
                games::nost::Lights::Key2R, games::nost::Lights::Key2G, games::nost::Lights::Key2B,
                games::nost::Lights::Key3R, games::nost::Lights::Key3G, games::nost::Lights::Key3B,
                games::nost::Lights::Key4R, games::nost::Lights::Key4G, games::nost::Lights::Key4B,
                games::nost::Lights::Key5R, games::nost::Lights::Key5G, games::nost::Lights::Key5B,
                games::nost::Lights::Key6R, games::nost::Lights::Key6G, games::nost::Lights::Key6B,
                games::nost::Lights::Key7R, games::nost::Lights::Key7G, games::nost::Lights::Key7B,
                games::nost::Lights::Key8R, games::nost::Lights::Key8G, games::nost::Lights::Key8B,
                games::nost::Lights::Key9R, games::nost::Lights::Key9G, games::nost::Lights::Key9B,
                games::nost::Lights::Key10R, games::nost::Lights::Key10G, games::nost::Lights::Key10B,
                games::nost::Lights::Key11R, games::nost::Lights::Key11G, games::nost::Lights::Key11B,
                games::nost::Lights::Key12R, games::nost::Lights::Key12G, games::nost::Lights::Key12B,
                games::nost::Lights::Key13R, games::nost::Lights::Key13G, games::nost::Lights::Key13B,
                games::nost::Lights::Key14R, games::nost::Lights::Key14G, games::nost::Lights::Key14B,
                games::nost::Lights::Key15R, games::nost::Lights::Key15G, games::nost::Lights::Key15B,
                games::nost::Lights::Key16R, games::nost::Lights::Key16G, games::nost::Lights::Key16B,
                games::nost::Lights::Key17R, games::nost::Lights::Key17G, games::nost::Lights::Key17B,
                games::nost::Lights::Key18R, games::nost::Lights::Key18G, games::nost::Lights::Key18B,
                games::nost::Lights::Key19R, games::nost::Lights::Key19G, games::nost::Lights::Key19B,
                games::nost::Lights::Key20R, games::nost::Lights::Key20G, games::nost::Lights::Key20B,
                games::nost::Lights::Key21R, games::nost::Lights::Key21G, games::nost::Lights::Key21B,
                games::nost::Lights::Key22R, games::nost::Lights::Key22G, games::nost::Lights::Key22B,
                games::nost::Lights::Key23R, games::nost::Lights::Key23G, games::nost::Lights::Key23B,
                games::nost::Lights::Key24R, games::nost::Lights::Key24G, games::nost::Lights::Key24B,
                games::nost::Lights::Key25R, games::nost::Lights::Key25G, games::nost::Lights::Key25B,
                games::nost::Lights::Key26R, games::nost::Lights::Key26G, games::nost::Lights::Key26B,
                games::nost::Lights::Key27R, games::nost::Lights::Key27G, games::nost::Lights::Key27B,
                games::nost::Lights::Key28R, games::nost::Lights::Key28G, games::nost::Lights::Key28B,
        };

        // write light
        if (index < std::size(mapping)) {
            Lights::writeLight(RI_MGR, lights.at(mapping[index]), value / 127.f);
        }
    }

    return 1;
}

static long __cdecl ac_io_panb_control_reset() {
    return 0;
}

static void* __cdecl ac_io_panb_get_control_status_buffer(uint8_t* buffer) {

    // copy buffer
    return memcpy(buffer, STATUS_BUFFER, sizeof(STATUS_BUFFER));
}

static bool __cdecl ac_io_panb_start_auto_input() {
    return true;
}

static bool __cdecl ac_io_panb_update_control_status_buffer() {

    // check freeze
    if (STATUS_BUFFER_FREEZE) {
        return true;
    }

    // clear buffer
    memset(STATUS_BUFFER, 0, 277);

    /*
     * first byte is number of input data
     * when it's set to 0 the game will not update it's key states
     * setting it too high will make the game read over the buffer
     *
     * unsure why you would send more than one set of data, so
     * we just set it to 1 and provide our current status
     */
    STATUS_BUFFER[0] = 1;

    // Nostalgia
    if (avs::game::is_model("PAN")) {

        // get buttons/analogs
        auto &buttons = games::nost::get_buttons();
        auto &analogs = games::nost::get_analogs();

        // mappings
        static const size_t button_mapping[] = {
                games::nost::Buttons::Key1, games::nost::Buttons::Key2,
                games::nost::Buttons::Key3, games::nost::Buttons::Key4,
                games::nost::Buttons::Key5, games::nost::Buttons::Key6,
                games::nost::Buttons::Key7, games::nost::Buttons::Key8,
                games::nost::Buttons::Key9, games::nost::Buttons::Key10,
                games::nost::Buttons::Key11, games::nost::Buttons::Key12,
                games::nost::Buttons::Key13, games::nost::Buttons::Key14,
                games::nost::Buttons::Key15, games::nost::Buttons::Key16,
                games::nost::Buttons::Key17, games::nost::Buttons::Key18,
                games::nost::Buttons::Key19, games::nost::Buttons::Key20,
                games::nost::Buttons::Key21, games::nost::Buttons::Key22,
                games::nost::Buttons::Key23, games::nost::Buttons::Key24,
                games::nost::Buttons::Key25, games::nost::Buttons::Key26,
                games::nost::Buttons::Key27, games::nost::Buttons::Key28,
        };
        static const size_t analog_mapping[] = {
                games::nost::Analogs::Key1, games::nost::Analogs::Key2,
                games::nost::Analogs::Key3, games::nost::Analogs::Key4,
                games::nost::Analogs::Key5, games::nost::Analogs::Key6,
                games::nost::Analogs::Key7, games::nost::Analogs::Key8,
                games::nost::Analogs::Key9, games::nost::Analogs::Key10,
                games::nost::Analogs::Key11, games::nost::Analogs::Key12,
                games::nost::Analogs::Key13, games::nost::Analogs::Key14,
                games::nost::Analogs::Key15, games::nost::Analogs::Key16,
                games::nost::Analogs::Key17, games::nost::Analogs::Key18,
                games::nost::Analogs::Key19, games::nost::Analogs::Key20,
                games::nost::Analogs::Key21, games::nost::Analogs::Key22,
                games::nost::Analogs::Key23, games::nost::Analogs::Key24,
                games::nost::Analogs::Key25, games::nost::Analogs::Key26,
                games::nost::Analogs::Key27, games::nost::Analogs::Key28,
        };

        // iterate pairs of keys
        for (size_t key_pair = 0; key_pair < 28 / 2; key_pair++) {

            // default states
            uint8_t state0 = 0;
            uint8_t state1 = 0;

            // check analogs
            auto &analog0 = analogs.at(analog_mapping[key_pair * 2 + 0]);
            auto &analog1 = analogs.at(analog_mapping[key_pair * 2 + 1]);
            if (analog0.isSet()) {
                state0 = (uint8_t) (Analogs::getState(RI_MGR, analog0) * 15.999f);
            }
            if (analog1.isSet()) {
                state1 = (uint8_t) (Analogs::getState(RI_MGR, analog1) * 15.999f);
            }

            // check buttons
            auto velocity0 = Buttons::getVelocity(RI_MGR, buttons.at(button_mapping[key_pair * 2 + 0]));
            auto velocity1 = Buttons::getVelocity(RI_MGR, buttons.at(button_mapping[key_pair * 2 + 1]));
            if (velocity0 > 0.f) {
                state0 = (uint8_t) (velocity0 * 15.999f);
            }
            if (velocity1 > 0.f) {
                state1 = (uint8_t) (velocity1 * 15.999f);
            }

            // build value
            uint8_t value = 0;
            value |= state0 << 4;
            value |= state1 & 0xF;

            // set value
            STATUS_BUFFER[key_pair + 3] = value;
        }
    }

    // return success
    return true;
}

/*
 * Module stuff
 */

acio::PANBModule::PANBModule(HMODULE module, acio::HookMode hookMode) : ACIOModule("PANB", module, hookMode) {
    this->status_buffer = STATUS_BUFFER;
    this->status_buffer_size = sizeof(STATUS_BUFFER);
    this->status_buffer_freeze = &STATUS_BUFFER_FREEZE;
}

void acio::PANBModule::attach() {
    ACIOModule::attach();

    // hooks
    ACIO_MODULE_HOOK(ac_io_panb_control_led_bright);
    ACIO_MODULE_HOOK(ac_io_panb_control_reset);
    ACIO_MODULE_HOOK(ac_io_panb_get_control_status_buffer);
    ACIO_MODULE_HOOK(ac_io_panb_start_auto_input);
    ACIO_MODULE_HOOK(ac_io_panb_update_control_status_buffer);
}

#include "mdxf.h"

#include "avs/game.h"
#include "games/ddr/io.h"
#include "launcher/launcher.h"
#include "rawinput/rawinput.h"
#include "util/logging.h"
#include "util/utils.h"

// constants
const size_t STATUS_BUFFER_SIZE = 32;

// static stuff
static uint8_t COUNTER = 0;

// buffers
#pragma pack(push, 1)
static struct {
    uint8_t STATUS_BUFFER_17[7][STATUS_BUFFER_SIZE] {};
    uint8_t STATUS_BUFFER_18[7][STATUS_BUFFER_SIZE] {};
} BUFFERS {};
#pragma pack(pop)

static bool STATUS_BUFFER_FREEZE = false;

/*
 * Implementations
 */

static bool __cdecl ac_io_mdxf_get_control_status_buffer(int node, void *buffer, uint8_t a3, uint8_t a4) {

    // Dance Dance Revolution
    if (avs::game::is_model("MDX")) {

        // get buffer index
        auto i = (COUNTER + a3) % std::size(BUFFERS.STATUS_BUFFER_17);

        // copy buffer
        if (node == 17) {
            memcpy(buffer, BUFFERS.STATUS_BUFFER_17[i], STATUS_BUFFER_SIZE);
        } else if (node == 18) {
            memcpy(buffer, BUFFERS.STATUS_BUFFER_18[i], STATUS_BUFFER_SIZE);
        } else {

            // fill with zeros on unknown node
            memset(buffer, 0, STATUS_BUFFER_SIZE);
            return false;
        }
    }

    // return success
    return true;
}

static bool __cdecl ac_io_mdxf_set_output_level(unsigned int a1, unsigned int a2, uint8_t a3) {
    // TODO(felix): DDR BIO2 lights

    return true;
}

static bool __cdecl ac_io_mdxf_update_control_status_buffer(int node) {

    // increase counter
    COUNTER = (COUNTER + 1) % std::size(BUFFERS.STATUS_BUFFER_17);

    // check freeze
    if (STATUS_BUFFER_FREEZE) {
        return true;
    }

    // clear buffer
    uint8_t *buffer = nullptr;
    switch (node) {
        case 17:
            buffer = BUFFERS.STATUS_BUFFER_17[COUNTER];
            break;
        case 18:
            buffer = BUFFERS.STATUS_BUFFER_18[COUNTER];
            break;
        default:

            // return failure on unknown node
            return false;
    }
    memset(buffer, 0, STATUS_BUFFER_SIZE);

    // Dance Dance Revolution
    if (avs::game::is_model("MDX")) {

        // Sensor Map (LDUR):
        // FOOT DOWN = bit 32-35 = byte 4, bit 0-3
        // FOOT UP = bit 36-39 = byte 4, bit 4-7
        // FOOT RIGHT = bit 40-43 = byte 5, bit 0-3
        // FOOT LEFT = bit 44-47 = byte 5, bit 4-7
        static const size_t buttons_17[] = {
                games::ddr::Buttons::P1_PANEL_UP,
                games::ddr::Buttons::P1_PANEL_DOWN,
                games::ddr::Buttons::P1_PANEL_LEFT,
                games::ddr::Buttons::P1_PANEL_RIGHT,
        };
        static const size_t buttons_18[] = {
                games::ddr::Buttons::P2_PANEL_UP,
                games::ddr::Buttons::P2_PANEL_DOWN,
                games::ddr::Buttons::P2_PANEL_LEFT,
                games::ddr::Buttons::P2_PANEL_RIGHT,
        };

        // decide on button map
        const size_t *button_map = nullptr;
        switch (node) {
            case 17:
                button_map = &buttons_17[0];
                break;
            case 18:
                button_map = &buttons_18[0];
                break;
        }

        // get buttons
        auto &buttons = games::ddr::get_buttons();

        if (GameAPI::Buttons::getState(RI_MGR, buttons.at(button_map[0]))) {
            buffer[4] |= 0xF0;
        }
        if (GameAPI::Buttons::getState(RI_MGR, buttons.at(button_map[1]))) {
            buffer[4] |= 0x0F;
        }
        if (GameAPI::Buttons::getState(RI_MGR, buttons.at(button_map[2]))) {
            buffer[5] |= 0xF0;
        }
        if (GameAPI::Buttons::getState(RI_MGR, buttons.at(button_map[3]))) {
            buffer[5] |= 0x0F;
        }
    }

    // return success
    return true;
}

/*
 * Module stuff
 */

acio::MDXFModule::MDXFModule(HMODULE module, acio::HookMode hookMode) : ACIOModule("MDXF", module, hookMode) {
    this->status_buffer = (uint8_t*) &BUFFERS;
    this->status_buffer_size = sizeof(BUFFERS);
    this->status_buffer_freeze = &STATUS_BUFFER_FREEZE;
}

void acio::MDXFModule::attach() {
    ACIOModule::attach();

    // hooks
    ACIO_MODULE_HOOK(ac_io_mdxf_get_control_status_buffer);
    ACIO_MODULE_HOOK(ac_io_mdxf_set_output_level);
    ACIO_MODULE_HOOK(ac_io_mdxf_update_control_status_buffer);
}

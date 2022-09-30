#include "bi2a.h"

#include "avs/game.h"
#include "games/ddr/io.h"
#include "games/sdvx/io.h"
#include "games/drs/io.h"
#include "games/drs/drs.h"
#include "misc/eamuse.h"
#include "util/logging.h"
#include "util/utils.h"

using namespace GameAPI;

// state
static uint8_t STATUS_BUFFER[272] {};
static bool STATUS_BUFFER_FREEZE = false;
static unsigned int BI2A_VOLL = 0;
static unsigned int BI2A_VOLR = 0;


static bool __cdecl ac_io_bi2a_init_is_finished() {
    return true;
}

static bool __cdecl ac_io_bi2a_get_control_status_buffer(void *buffer) {

    // copy buffer
    memcpy(buffer, STATUS_BUFFER, std::size(STATUS_BUFFER));
    return true;
}

static bool __cdecl ac_io_bi2a_update_control_status_buffer() {

    // check freeze
    if (STATUS_BUFFER_FREEZE) {
        return true;
    }

    // Sound Voltex
    if (avs::game::is_model("KFC")) {

        // clear buffer
        memset(STATUS_BUFFER, 0, std::size(STATUS_BUFFER));
        STATUS_BUFFER[0] = 1;

        /*
         * Unmapped Buttons
         *
         * Control      Bit
         * EX BUTTON 1  93
         * EX BUTTON 2  92
         * EX ANALOG 1  170-183
         * EX ANALOG 2  186-199
         */

        // get buttons
        auto &buttons = games::sdvx::get_buttons();

        if (Buttons::getState(RI_MGR, buttons.at(games::sdvx::Buttons::Test))) {
            ARRAY_SETB(STATUS_BUFFER, 19);
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::sdvx::Buttons::Service))) {
            ARRAY_SETB(STATUS_BUFFER, 18);
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::sdvx::Buttons::CoinMech))) {
            ARRAY_SETB(STATUS_BUFFER, 17);
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::sdvx::Buttons::Start))) {
            ARRAY_SETB(STATUS_BUFFER, 85);
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::sdvx::Buttons::BT_A))) {
            ARRAY_SETB(STATUS_BUFFER, 84);
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::sdvx::Buttons::BT_B))) {
            ARRAY_SETB(STATUS_BUFFER, 83);
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::sdvx::Buttons::BT_C))) {
            ARRAY_SETB(STATUS_BUFFER, 82);
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::sdvx::Buttons::BT_D))) {
            ARRAY_SETB(STATUS_BUFFER, 81);
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::sdvx::Buttons::FX_L))) {
            ARRAY_SETB(STATUS_BUFFER, 80);
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::sdvx::Buttons::FX_R))) {
            ARRAY_SETB(STATUS_BUFFER, 95);
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::sdvx::Buttons::Headphone))) {
            ARRAY_SETB(STATUS_BUFFER, 87);
        }

        // volume left
        if (Buttons::getState(RI_MGR, buttons.at(games::sdvx::Buttons::VOL_L_Left))) {
            BI2A_VOLL = (BI2A_VOLL - 16) & 1023;
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::sdvx::Buttons::VOL_L_Right))) {
            BI2A_VOLL = (BI2A_VOLL + 16) & 1023;
        }

        // volume right
        if (Buttons::getState(RI_MGR, buttons.at(games::sdvx::Buttons::VOL_R_Left))) {
            BI2A_VOLR = (BI2A_VOLR - 16) & 1023;
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::sdvx::Buttons::VOL_R_Right))) {
            BI2A_VOLR = (BI2A_VOLR + 16) & 1023;
        }

        // update volumes
        auto &analogs = games::sdvx::get_analogs();
        auto vol_left = BI2A_VOLL;
        auto vol_right = BI2A_VOLR;
        if (analogs.at(0).isSet() || analogs.at(1).isSet()) {
            vol_left += (unsigned int) (Analogs::getState(RI_MGR,
                    analogs.at(games::sdvx::Analogs::VOL_L)) * 1023.99f);
            vol_right += (unsigned int) (Analogs::getState(RI_MGR,
                    analogs.at(games::sdvx::Analogs::VOL_R)) * 1023.99f);
        }

        // proper loops
        vol_left %= 1024;
        vol_right %= 1024;

        // save volumes in buffer
        *((uint16_t*) &STATUS_BUFFER[17]) = (uint16_t) ((vol_left) << 2);
        *((uint16_t*) &STATUS_BUFFER[19]) = (uint16_t) ((vol_right) << 2);
    }

    // DanceDanceRevolution
    if (avs::game::is_model("MDX")) {

        // clear buffer
        memset(STATUS_BUFFER, 0, std::size(STATUS_BUFFER));
        STATUS_BUFFER[0] = 1;

        // get buttons
        auto &buttons = games::ddr::get_buttons();

        if (Buttons::getState(RI_MGR, buttons.at(games::ddr::Buttons::COIN_MECH)) == Buttons::BUTTON_PRESSED) {
            STATUS_BUFFER[2] |= 1 << 1;
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::ddr::Buttons::SERVICE)) == Buttons::BUTTON_PRESSED) {
            STATUS_BUFFER[2] |= 1 << 2;
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::ddr::Buttons::TEST)) == Buttons::BUTTON_PRESSED) {
            STATUS_BUFFER[2] |= 1 << 3;
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::ddr::Buttons::P1_START)) == Buttons::BUTTON_PRESSED) {
            STATUS_BUFFER[10] |= 1 << 7;
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::ddr::Buttons::P1_MENU_UP)) == Buttons::BUTTON_PRESSED) {
            STATUS_BUFFER[10] |= 1 << 6;
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::ddr::Buttons::P1_MENU_DOWN)) == Buttons::BUTTON_PRESSED) {
            STATUS_BUFFER[10] |= 1 << 5;
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::ddr::Buttons::P1_MENU_LEFT)) == Buttons::BUTTON_PRESSED) {
            STATUS_BUFFER[10] |= 1 << 4;
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::ddr::Buttons::P1_MENU_RIGHT)) == Buttons::BUTTON_PRESSED) {
            STATUS_BUFFER[10] |= 1 << 3;
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::ddr::Buttons::P2_START)) == Buttons::BUTTON_PRESSED) {
            STATUS_BUFFER[11] |= 1 << 5;
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::ddr::Buttons::P2_MENU_UP)) == Buttons::BUTTON_PRESSED) {
            STATUS_BUFFER[11] |= 1 << 4;
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::ddr::Buttons::P2_MENU_DOWN)) == Buttons::BUTTON_PRESSED) {
            STATUS_BUFFER[11] |= 1 << 3;
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::ddr::Buttons::P2_MENU_LEFT)) == Buttons::BUTTON_PRESSED) {
            STATUS_BUFFER[11] |= 1 << 2;
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::ddr::Buttons::P2_MENU_RIGHT)) == Buttons::BUTTON_PRESSED) {
            STATUS_BUFFER[11] |= 1 << 1;
        }
    }

    // DANCERUSH
    if (avs::game::is_model("REC")) {

        // clear buffer
        memset(STATUS_BUFFER, 0, std::size(STATUS_BUFFER));
        STATUS_BUFFER[0] = 1;

        // get buttons
        auto &buttons = games::drs::get_buttons();

        // test
        if (Buttons::getState(RI_MGR, buttons.at(games::drs::Buttons::Test)) == Buttons::State::BUTTON_PRESSED) {
            ARRAY_SETB(STATUS_BUFFER, 19);
        }

        // service
        if (Buttons::getState(RI_MGR, buttons.at(games::drs::Buttons::Service)) == Buttons::State::BUTTON_PRESSED) {
            ARRAY_SETB(STATUS_BUFFER, 18);
        }

        // coin
        if (Buttons::getState(RI_MGR, buttons.at(games::drs::Buttons::CoinMech)) == Buttons::State::BUTTON_PRESSED) {
            ARRAY_SETB(STATUS_BUFFER, 17);
        }

        if (Buttons::getState(RI_MGR, buttons.at(games::drs::Buttons::P1_Start)) == Buttons::State::BUTTON_PRESSED) {
            ARRAY_SETB(STATUS_BUFFER, 87);
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::drs::Buttons::P1_Up)) == Buttons::State::BUTTON_PRESSED) {
            ARRAY_SETB(STATUS_BUFFER, 86);
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::drs::Buttons::P1_Down)) == Buttons::State::BUTTON_PRESSED) {
            ARRAY_SETB(STATUS_BUFFER, 85);
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::drs::Buttons::P1_Left)) == Buttons::State::BUTTON_PRESSED) {
            ARRAY_SETB(STATUS_BUFFER, 84);
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::drs::Buttons::P1_Right)) == Buttons::State::BUTTON_PRESSED) {
            ARRAY_SETB(STATUS_BUFFER, 83);
        }

        if (Buttons::getState(RI_MGR, buttons.at(games::drs::Buttons::P2_Start)) == Buttons::State::BUTTON_PRESSED) {
            ARRAY_SETB(STATUS_BUFFER, 93);
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::drs::Buttons::P2_Up)) == Buttons::State::BUTTON_PRESSED) {
            ARRAY_SETB(STATUS_BUFFER, 92);
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::drs::Buttons::P2_Down)) == Buttons::State::BUTTON_PRESSED) {
            ARRAY_SETB(STATUS_BUFFER, 91);
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::drs::Buttons::P2_Left)) == Buttons::State::BUTTON_PRESSED) {
            ARRAY_SETB(STATUS_BUFFER, 90);
        }
        if (Buttons::getState(RI_MGR, buttons.at(games::drs::Buttons::P2_Right)) == Buttons::State::BUTTON_PRESSED) {
            ARRAY_SETB(STATUS_BUFFER, 89);
        }
    }

    return true;
}

static bool __cdecl ac_io_bi2a_current_coinstock(size_t index, DWORD *coins) {

    // check index
    if (index > 1)
        return false;

    // get coins and return success
    *coins = (DWORD) eamuse_coin_get_stock();
    return true;
}

static bool __cdecl ac_io_bi2a_consume_coinstock(size_t index, int amount) {

    // check index
    if (index > 1)
        return false;

    // calculate new stock
    auto stock = eamuse_coin_get_stock();
    auto stock_new = stock - amount;

    // check new stock
    if (stock_new < 0)
        return false;

    // apply new stock
    eamuse_coin_set_stock(stock_new);
    return true;
}

static bool __cdecl ac_io_bi2a_lock_coincounter(size_t index) {

    // check index
    if (index > 1)
        return false;

    // enable coin blocker
    eamuse_coin_set_block(true);
    return true;
}

static bool __cdecl ac_io_bi2a_unlock_coincounter(size_t index) {

    // check index
    if (index > 1)
        return false;

    // disable coin blocker
    eamuse_coin_set_block(false);
    return true;
}

static void __cdecl ac_io_bi2a_control_coin_blocker_close(size_t index) {

    // check index
    if (index > 1)
        return;

    // enable coin blocker
    eamuse_coin_set_block(true);
}

static void __cdecl ac_io_bi2a_control_coin_blocker_open(size_t index) {

    // check index
    if (index > 1)
        return;

    // disable coin blocker
    eamuse_coin_set_block(false);
}

static long __cdecl ac_io_bi2a_control_led_bright(size_t index, uint8_t brightness) {

    // Sound Voltex
    if (avs::game::is_model("KFC")) {

        /*
         * Control      R   G   B
         * =======================
         * WING UP      28  29  30
         * WING LOW     31  32  33
         * WOOFER       0   1   3
         * CONTROLLER   4   5   6
         *
         * Values go up to 255.
         *
         *
         * Control      Index
         * ==================
         * START BUTTON 8
         * A BUTTON     9
         * B BUTTON     10
         * C BUTTON     11
         * D BUTTON     12
         * FX L BUTTON  13
         * FX R BUTTON  14
         * POP          24
         * TITLE LEFT   25
         * TITLE RIGHT  26
         *
         * Values go up to 127.
         */

        static const struct {
            int light1, light2;
            float max;
        } mapping[] = {
                { games::sdvx::Lights::WOOFER_R, -1, 255 },
                { games::sdvx::Lights::WOOFER_G, -1, 255 },
                { -1, -1, 0 },
                { games::sdvx::Lights::WOOFER_B, -1, 255 },
                { games::sdvx::Lights::CONTROLLER_R, -1, 255 },
                { games::sdvx::Lights::CONTROLLER_G, -1, 255 },
                { games::sdvx::Lights::CONTROLLER_B, -1, 255 },
                { -1, -1, 0 },
                { games::sdvx::Lights::START, -1, 127 },
                { games::sdvx::Lights::BT_A, -1, 127 },
                { games::sdvx::Lights::BT_B, -1, 127 },
                { games::sdvx::Lights::BT_C, -1, 127 },
                { games::sdvx::Lights::BT_D, -1, 127 },
                { games::sdvx::Lights::FX_L, -1, 127 },
                { games::sdvx::Lights::FX_R, -1, 127 },
                { -1, -1, 0 }, { -1, -1, 0 }, { -1, -1, 0 },
                { games::sdvx::Lights::GENERATOR_R, -1, 255 },
                { games::sdvx::Lights::GENERATOR_G, -1, 255 },
                { games::sdvx::Lights::GENERATOR_B, -1, 255 },
                { -1, -1, 0 }, { -1, -1, 0 }, { -1, -1, 0 },
                { games::sdvx::Lights::POP, -1, 127 },
                { games::sdvx::Lights::TITLE_LEFT, -1, 127 },
                { games::sdvx::Lights::TITLE_RIGHT, -1, 127 },
                { -1, -1, 0 },
                { games::sdvx::Lights::WING_RIGHT_UP_R, games::sdvx::Lights::WING_LEFT_UP_R, 255 },
                { games::sdvx::Lights::WING_RIGHT_UP_G, games::sdvx::Lights::WING_LEFT_UP_G, 255 },
                { games::sdvx::Lights::WING_RIGHT_UP_B, games::sdvx::Lights::WING_LEFT_UP_B, 255 },
                { games::sdvx::Lights::WING_RIGHT_LOW_R, games::sdvx::Lights::WING_LEFT_LOW_R, 255 },
                { games::sdvx::Lights::WING_RIGHT_LOW_G, games::sdvx::Lights::WING_LEFT_LOW_G, 255 },
                { games::sdvx::Lights::WING_RIGHT_LOW_B, games::sdvx::Lights::WING_LEFT_LOW_B, 255 },
        };

        // ignore index out of range
        if (index > std::size(mapping)) {
            return true;
        }

        // get lights
        auto &lights = games::sdvx::get_lights();

        // get light from mapping
        auto light = mapping[index];

        // write lights
        if (light.light1 >= 0) {
            Lights::writeLight(RI_MGR, lights[light.light1], brightness / light.max);
        } else {
            log_warning("sdvx", "light unset {} {}", index, (int) brightness);
        }
        if (light.light2 >= 0) {
            Lights::writeLight(RI_MGR, lights[light.light2], brightness / light.max);
        }
    }

    // return success
    return true;
}

static long __cdecl ac_io_bi2a_get_watchdog_time_min() {
    return -1;
}

static long __cdecl ac_io_bi2a_get_watchdog_time_now() {
    return -1;
}

static void __cdecl ac_io_bi2a_watchdog_off() {
}

static bool __cdecl ac_io_bi2a_init(uint8_t param) {
    return true;
}

static bool __cdecl ac_io_bi2a_set_watchdog_time(uint16_t time) {
    return true;
}

static bool __cdecl ac_io_bi2a_get_watchdog_status() {
    return true;
}

static bool __cdecl ac_io_bi2a_set_amp_volume(uint8_t a1, uint8_t a2) {
    return true;
}

static bool __cdecl ac_io_bi2a_tapeled_init(uint8_t a1, uint8_t a2) {
    return true;
}

static bool __cdecl ac_io_bi2a_tapeled_init_is_finished() {
    return true;
}

static bool __cdecl ac_io_bi2a_control_tapeled_rec_set(uint8_t* data, size_t x_sz, size_t y_sz) {

    // check dimensions
    if (x_sz != 38 || y_sz != 49) {
        log_fatal("drs", "DRS tapeled wrong dimensions");
    }

    // copy data into our buffer - 4 bytes per pixel BGR
    for (size_t i = 0; i < x_sz * y_sz; i++) {
        games::drs::DRS_TAPELED[i][0] = data[i*4+2];
        games::drs::DRS_TAPELED[i][1] = data[i*4+1];
        games::drs::DRS_TAPELED[i][2] = data[i*4];
    }

    // success
    return true;
}

// TODO: this controls the upright RGB bars on the sides
static bool __cdecl ac_io_bi2a_control_tapeled_bright(size_t off1, size_t off2,
        uint8_t r, uint8_t g, uint8_t b, uint8_t bank) {
    return true;
}

static bool __cdecl ac_io_bi2a_tapeled_send() {
    return true;
}

static int __cdecl ac_io_bi2a_get_exbio2_status(uint8_t *info) {
    // surely this meme never gets old
    info[5] = 5;
    info[6] = 7;
    info[7] = 3;
    return 0;
}

acio::BI2AModule::BI2AModule(HMODULE module, acio::HookMode hookMode) : ACIOModule("BI2A", module, hookMode) {
    this->status_buffer = STATUS_BUFFER;
    this->status_buffer_size = sizeof(STATUS_BUFFER);
    this->status_buffer_freeze = &STATUS_BUFFER_FREEZE;
}

void acio::BI2AModule::attach() {
    ACIOModule::attach();

    ACIO_MODULE_HOOK(ac_io_bi2a_init_is_finished);
    ACIO_MODULE_HOOK(ac_io_bi2a_get_control_status_buffer);
    ACIO_MODULE_HOOK(ac_io_bi2a_update_control_status_buffer);
    ACIO_MODULE_HOOK(ac_io_bi2a_current_coinstock);
    ACIO_MODULE_HOOK(ac_io_bi2a_consume_coinstock);
    ACIO_MODULE_HOOK(ac_io_bi2a_lock_coincounter);
    ACIO_MODULE_HOOK(ac_io_bi2a_unlock_coincounter);
    ACIO_MODULE_HOOK(ac_io_bi2a_control_coin_blocker_close);
    ACIO_MODULE_HOOK(ac_io_bi2a_control_coin_blocker_open);
    ACIO_MODULE_HOOK(ac_io_bi2a_control_led_bright);
    ACIO_MODULE_HOOK(ac_io_bi2a_get_watchdog_time_min);
    ACIO_MODULE_HOOK(ac_io_bi2a_get_watchdog_time_now);
    ACIO_MODULE_HOOK(ac_io_bi2a_watchdog_off);
    ACIO_MODULE_HOOK(ac_io_bi2a_init);
    ACIO_MODULE_HOOK(ac_io_bi2a_set_watchdog_time);
    ACIO_MODULE_HOOK(ac_io_bi2a_get_watchdog_status);
    ACIO_MODULE_HOOK(ac_io_bi2a_set_amp_volume);
    ACIO_MODULE_HOOK(ac_io_bi2a_tapeled_init);
    ACIO_MODULE_HOOK(ac_io_bi2a_tapeled_init_is_finished);
    ACIO_MODULE_HOOK(ac_io_bi2a_get_exbio2_status);
    ACIO_MODULE_HOOK(ac_io_bi2a_control_tapeled_rec_set);
    ACIO_MODULE_HOOK(ac_io_bi2a_control_tapeled_bright);
    ACIO_MODULE_HOOK(ac_io_bi2a_tapeled_send);
}

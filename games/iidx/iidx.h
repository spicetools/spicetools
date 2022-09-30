#pragma once

#include <mutex>
#include <optional>

#include "games/game.h"

namespace games::iidx {

    // settings
    extern bool FLIP_CAMS;
    extern bool DISABLE_CAMS;
    extern bool TDJ_MODE;
    extern std::optional<std::string> SOUND_OUTPUT_DEVICE;
    extern std::optional<std::string> ASIO_DRIVER;

    // state
    extern char IIDXIO_LED_TICKER[10];
    extern bool IIDXIO_LED_TICKER_READONLY;
    extern std::mutex IIDX_LED_TICKER_LOCK;

    class IIDXGame : public games::Game {
    public:
        IIDXGame();
        virtual ~IIDXGame() override;

        virtual void attach() override;
        virtual void pre_attach() override;
        virtual void detach() override;
    };

    // helper methods
    uint32_t get_pad();
    void write_lamp(uint16_t lamp);
    void write_led(uint8_t led);
    void write_top_lamp(uint8_t top_lamp);
    void write_top_neon(uint8_t top_neon);
    unsigned char get_tt(int player, bool slow);
    unsigned char get_slider(uint8_t slider);
    const char* get_16seg();
}

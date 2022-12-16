#include "richpresence.h"
#include <external/robin_hood.h>
#include "external/discord-rpc/include/discord_rpc.h"
#include "util/logging.h"
#include "misc/eamuse.h"

namespace richpresence {

    namespace discord {

        // application IDs
        static robin_hood::unordered_map<std::string, std::string> APP_IDS = {
                {"Sound Voltex", "609803246617231377"},
                {"Beatmania IIDX", "609870727654539266"},
                {"Jubeat", "609870833032364035"},
                {"Dance Evolution", "609870916259938314"},
                {"Beatstream", "609871030609248256"},
                {"Metal Gear", "609871105213071449"},
                {"Reflec Beat", "609871162570178560"},
                {"Pop'n Music", "609871244782862387"},
                {"Steel Chronicle", "609871515974107136"},
                {"Road Fighters 3D", "609871634295291914"},
                {"Museca", "609871785323790336"},
                {"Bishi Bashi Channel", "609874994989760522"},
                {"GitaDora", "609875077416091734"},
                {"Dance Dance Revolution", "609875139890380800"},
                {"Nostalgia", "609875303711375475"},
                {"Quiz Magic Academy", "609875394610462720"},
                {"FutureTomTom", "609875462436421652"},
                {"Mahjong Fight Club", "609875523665002497"},
                {"HELLO! Pop'n Music", "609875610474381322"},
                {"LovePlus", "609875666279858199"},
        };

        // state
        std::string APPID_OVERRIDE = "";
        bool INITIALIZED = false;

        void ready(const DiscordUser *request) {
            log_warning("richpresence:discord", "ready");
        }

        void disconnected(int errorCode, const char *message) {
            log_warning("richpresence:discord", "disconnected");
        }

        void errored(int errorCode, const char *message) {
            log_warning("richpresence:discord", "error {}: {}", errorCode, message);
        }

        void joinGame(const char *joinSecret) {
            log_warning("richpresence:discord", "joinGame");
        }

        void spectateGame(const char *spectateSecret) {
            log_warning("richpresence:discord", "spectateGame");
        }

        void joinRequest(const DiscordUser *request) {
            log_warning("richpresence:discord", "joinRequest");
        }

        // handler object
        static DiscordEventHandlers handlers {
                .ready = discord::ready,
                .disconnected = discord::disconnected,
                .errored = discord::errored,
                .joinGame = discord::joinGame,
                .spectateGame = discord::spectateGame,
                .joinRequest = discord::joinRequest
        };

        void update() {

            // check state
            if (!INITIALIZED)
                return;

            // update presence
            DiscordRichPresence presence {};
            Discord_UpdatePresence(&presence);
        }

        void init() {

            // check state
            if (INITIALIZED) {
                return;
            }

            // get id
            std::string id = "";
            if (!APPID_OVERRIDE.empty()) {
                log_info("richpresence:discord", "using custom APPID: {}", APPID_OVERRIDE);
                id = APPID_OVERRIDE;
            } else {
                auto game_model = eamuse_get_game();
                if (game_model.empty()) {
                    log_warning("richpresence:discord", "could not get game model");
                    return;
                }

                id = APP_IDS[game_model];
                if (id.empty()) {
                    log_warning("richpresence:discord", "did not find app ID for {}", game_model);
                    return;
                }
            }

            // initialize discord
            Discord_Initialize(id.c_str(), &discord::handlers, 0, nullptr);

            // mark as initialized
            INITIALIZED = true;
            log_info("richpresence:discord", "initialized");

            // update once so the presence is displayed
            update();
        }

        void shutdown() {
            Discord_ClearPresence();
            Discord_Shutdown();
        }
    }

    // state
    static bool INITIALIZED = false;

    void init() {
        if (INITIALIZED)
            return;
        log_info("richpresence", "initializing");
        INITIALIZED = true;
        discord::init();
    }

    void update(const char *state) {
        if (!INITIALIZED)
            return;
        discord::update();
    }

    void shutdown() {
        if (!INITIALIZED)
            return;
        log_info("richpresence", "shutdown");
        discord::shutdown();
        INITIALIZED = false;
    }
}

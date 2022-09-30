#include "sdvx.h"

#include <external/robin_hood.h>

#include "avs/game.h"
#include "games/shared/lcdhandle.h"
#include "hooks/audio/audio.h"
#include "hooks/graphics/graphics.h"
#include "hooks/devicehook.h"
#include "hooks/libraryhook.h"
#include "hooks/powrprof.h"
#include "hooks/sleephook.h"
#include "util/detour.h"
#include "util/logging.h"
#include "util/sigscan.h"
#include "util/libutils.h"
#include "misc/wintouchemu.h"
#include "misc/eamuse.h"
#include "bi2x_hook.h"
#include "camera.h"
#include "io.h"
#include "acioemu/handle.h"

namespace games::sdvx {

    // settings
    bool DISABLECAMS = false;
    bool NATIVETOUCH = false;

    SDVXGame::SDVXGame() : Game("Sound Voltex") {
    }

    static LPWSTR __stdcall GetCommandLineW_hook() {
        static std::wstring lp_args = L"bootstrap.exe prop\\bootstrap.xml";
        return lp_args.data();
    }

#ifdef SPICE64
    static bool sdvx5_spam_remover(void *user, const std::string &data, logger::Style style, std::string &out) {
        if (data.empty() || data[0] != '[') {
            return false;
        }
        if (data.find("W:afpu-package: XE592acd000040 texture id invalid") != std::string::npos) {
            out = "";
            return true;
        }
        if (data.find("W:afpu-package: XE592acd000042 texture id invalid") != std::string::npos) {
            out = "";
            return true;
        }
        if (data.find("W:CTexture: no such texture: id 0") != std::string::npos) {
            out = "";
            return true;
        }
        if (data.find("M:autoDj: DEF phrase ") != std::string::npos) {
            out = "";
            return true;
        }
        if (data.find("W:afp-access: afp_mc_deep_goto_play frame no error") != std::string::npos) {
            out = "";
            return true;
        }
        if (data.find("W:afputils: CDirectX::SetRenderState") != std::string::npos) {
            out = "";
            return true;
        }
        if (data.find("W:CameraTexture: Camera error was detected. (err,detail) = (0,0)") != std::string::npos) {
            out = "";
            return true;
        }
        return false;
    }

    typedef void **(__fastcall *volume_set_t)(uint64_t, uint64_t, uint64_t);
    static volume_set_t volume_set_orig = nullptr;
    static void **__fastcall volume_set_hook(uint64_t vol_sound, uint64_t vol_woofer, uint64_t vol_headphone) {

        // volume level conversion tables
        static uint8_t SOUND_VOLUMES[] = {
                4, 55, 57, 59, 61, 63, 65, 67, 69, 71,
                73, 75, 77, 78, 79, 80, 81, 82, 83, 84,
                85, 86, 87, 88, 89, 90, 91, 92, 93, 95, 96,
        };
        static uint8_t WOOFER_VOLUMES[] = {
                4, 70, 72, 73, 74, 75, 76, 77, 79, 80,
                81, 82, 83, 84, 85, 86, 87, 88, 89, 90,
                91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 100,
        };
        static uint8_t HEADPHONE_VOLUMES[] = {
                4, 60, 62, 64, 66, 68, 70, 72, 76, 78,
                80, 82, 83, 84, 85, 86, 87, 88, 89, 90,
                91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 100,
        };

        // apply volumes
        auto &format = hooks::audio::FORMAT.Format;
        auto &lights = games::sdvx::get_lights();
        if (format.nChannels == 6 || vol_sound != 30) {
            if (vol_sound < std::size(SOUND_VOLUMES) && vol_sound != 30) {
                float value = (float) SOUND_VOLUMES[vol_sound] * 0.01f;
                GameAPI::Lights::writeLight(RI_MGR, lights[Lights::VOLUME_SOUND], value);
            }
        }
        if (vol_woofer < std::size(WOOFER_VOLUMES)) {
            float value = (float) WOOFER_VOLUMES[vol_woofer] * 0.01f;
            GameAPI::Lights::writeLight(RI_MGR, lights[Lights::VOLUME_WOOFER], value);
        }
        if (vol_headphone < std::size(HEADPHONE_VOLUMES)) {
            float value = (float) HEADPHONE_VOLUMES[vol_headphone] * 0.01f;
            GameAPI::Lights::writeLight(RI_MGR, lights[Lights::VOLUME_HEADPHONE], value);
        }

        // call original function to set volumes for the 6ch mode
        return volume_set_orig(format.nChannels == 6 ? vol_sound : 30, vol_woofer, vol_headphone);
    }
#endif

    void SDVXGame::attach() {
        Game::attach();

#ifdef SPICE64 // SDVX5+ specific code
        bool isValkyrieCabinetMode = avs::game::SPEC[0] == 'G' || avs::game::SPEC[0] == 'H';

        // LCD handle
        if (!isValkyrieCabinetMode) {
            devicehook_init();
            devicehook_add(new games::shared::LCDHandle());
        }
#else
        devicehook_init();
        devicehook_add(new games::shared::LCDHandle());
#endif
        hooks::sleep::init(1000, 1);

        // hooks for chinese SDVX
        if (libutils::try_module("unisintr.dll")) {
            detour::iat_try("GetCommandLineW", GetCommandLineW_hook);

            // skip 30 second timeout after NETWORK DEVICE check
            replace_pattern(
                    avs::game::DLL_INSTANCE,
                    "89F528003D????0000",
                    "89F528003D01000000",
                    0, 0);
        }

#ifdef SPICE64 // SDVX5+ specific code

        // check for new I/O DLL
        auto aio = libutils::try_library("libaio.dll");
        if (aio != nullptr) {

            // enable 9on12 for AMD
            if (!libutils::try_library("nvapi64.dll")) {
                log_info("sdvx", "nvapi64.dll not found, force enabling 9on12");
                GRAPHICS_9_ON_12 = true;
            }

            // check for Valkyrie cabinet mode
            if (isValkyrieCabinetMode) {
                // hook touch window
                if (!NATIVETOUCH) {
                    wintouchemu::FORCE = true;
                    wintouchemu::hook_title_ends(
                            "SOUND VOLTEX",
                            "Main Screen",
                            avs::game::DLL_INSTANCE);
                }

                // insert BI2X hooks
                bi2x_hook_init();

                // add card readers
                devicehook_init(aio);
                devicehook_add(new acioemu::ACIOHandle(L"COM1"));
            } else {
                // don't let nvapi mess with display settings (force failure)
                libraryhook_hook_proc("nvapi_QueryInterface", static_cast<void *>(nullptr));
                libraryhook_enable(avs::game::DLL_INSTANCE);
            }
        }

        powrprof_hook_init(avs::game::DLL_INSTANCE);

        // hook camera
        if (!DISABLECAMS) {
            camera_init();
        }

        // RGB CAMERA error ignore
        if (!replace_pattern(
                avs::game::DLL_INSTANCE,
                "418D480484C074218D51FD",
                "????????????9090??????",
                0, 0)) {
            log_warning("sdvx", "failed to insert camera error fix");
        }

        // remove log spam
        logger::hook_add(sdvx5_spam_remover, nullptr);

#endif
    }

    void SDVXGame::post_attach() {
        Game::post_attach();

#ifdef SPICE64 // SDVX5+ specific code

        /*
         * Volume Hook
         *
         * How to find the correct RVA:
         *
         * Method 1 (older versions):
         * Search for byte sequence 48 8B C4 48 81 EC 88 00 00 00 80 3D
         *
         * Method 2 (older versions):
         * 1. search for ac_io_bi2a_set_amp_volume
         * 2. move one function up (function where it does some calculations, that's ours)
         * 3. take the *file offset* of the *first* instruction of this function
         *
         * Method 3:
         * Search for the function with the 3 arguments which is being called from the sound options.
         * It is pretty obvious which one it is because it checks all 3 args to be <= 30.
         */
        static const robin_hood::unordered_map<std::string, intptr_t> VOLUME_HOOKS {
                { "2019100800", 0x414ED0 },
                { "2020011500", 0x417090 },
                { "2020022700", 0x4281A0 },
                { "2020122200", 0x40C030 },
                { "2021042800", 0x096EB0 },
                { "2021051802", 0x097930 },
                { "2021083100", 0x096E20 },
                { "2021102000", 0x097230 },
                { "2021121400", 0x09AD00 },
        };

        bool volume_hook_found = false;
        for (auto &[datecode, rva] : VOLUME_HOOKS) {
            if (avs::game::is_ext(datecode.c_str())) {

                // calculate target RVA
                auto volume_set_rva = libutils::offset2rva(MODULE_PATH / avs::game::DLL_NAME, rva);
                if (volume_set_rva == -1) {
                    log_warning("sdvx", "failed to insert set volume hook (convert rva {})", rva);
                    break;
                }

                // convert RVA to real target
                auto *volume_set_ptr = reinterpret_cast<uint16_t *>(
                        reinterpret_cast<intptr_t>(avs::game::DLL_INSTANCE) + volume_set_rva);
                if (volume_set_ptr[0] != 0x8B48) {
                    log_warning("sdvx", "failed to insert set volume hook (invalid target)");
                    break;
                }

                // insert trampoline
                if (!detour::trampoline(
                            reinterpret_cast<volume_set_t>(volume_set_ptr),
                            volume_set_hook,
                            &volume_set_orig))
                {
                    log_warning("sdvx", "failed to insert set volume hook (insert trampoline)");
                }

                // success
                volume_hook_found = true;
                break;
            }
        }

        // check if version not found
        if (!volume_hook_found) {
            log_warning("sdvx", "volume hook unavailable for this game version");

            // set volumes to sdvx 4 defaults
            auto &lights = games::sdvx::get_lights();
            GameAPI::Lights::writeLight(RI_MGR, lights[games::sdvx::Lights::VOLUME_SOUND],
                    (100 - 15) / 100.f);
            GameAPI::Lights::writeLight(RI_MGR, lights[games::sdvx::Lights::VOLUME_HEADPHONE],
                    (100 - 9) / 100.f);
            GameAPI::Lights::writeLight(RI_MGR, lights[games::sdvx::Lights::VOLUME_EXTERNAL],
                    (100 - 96) / 100.f);
            GameAPI::Lights::writeLight(RI_MGR, lights[games::sdvx::Lights::VOLUME_WOOFER],
                    (100 - 9) / 100.f);
        }
#endif
    }

    void SDVXGame::detach() {
        Game::detach();

        devicehook_dispose();
    }
}

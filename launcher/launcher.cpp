#include <condition_variable>
#include <iostream>
#include <memory>
#include <vector>

#include <shlwapi.h>
#include <cfg/configurator.h>

#include "acio/acio.h"
#include "acio/icca/icca.h"
#include "api/controller.h"
#include "avs/automap.h"
#include "avs/core.h"
#include "avs/ea3.h"
#include "avs/game.h"
#include "build/defs.h"
#include "cfg/spicecfg.h"
#include "cfg/config.h"
#include "easrv/easrv.h"
#include "external/cardio/cardio_runner.h"
#include "external/scard/scard.h"
#include "external/layeredfs/hook.h"
#include "games/game.h"
#include "games/io.h"
#include "games/bbc/bbc.h"
#include "games/bs/bs.h"
#include "games/ddr/ddr.h"
#include "games/dea/dea.h"
#include "games/drs/drs.h"
#include "games/gitadora/gitadora.h"
#include "games/hpm/hpm.h"
#include "games/iidx/iidx.h"
#include "games/jb/jb.h"
#include "games/mga/mga.h"
#include "games/nost/nost.h"
#include "games/popn/popn.h"
#include "games/qma/qma.h"
#include "games/rb/rb.h"
#include "games/rf3d/rf3d.h"
#include "games/sc/sc.h"
#include "games/scotto/scotto.h"
#include "games/sdvx/sdvx.h"
#include "games/shared/printer.h"
#include "games/silentscope/silentscope.h"
#include "games/mfc/mfc.h"
#include "games/ftt/ftt.h"
#include "games/loveplus/loveplus.h"
#include "games/we/we.h"
#include "games/otoca/otoca.h"
#include "games/shogikai/shogikai.h"
#include "games/pcm/pcm.h"
#include "games/onpara/onpara.h"
#include "games/bc/bc.h"
#include "hooks/avshook.h"
#include "hooks/audio/audio.h"
#include "hooks/debughook.h"
#include "hooks/devicehook.h"
#include "hooks/input/dinput8/hook.h"
#include "hooks/graphics/graphics.h"
#include "hooks/lang.h"
#include "hooks/networkhook.h"
#include "hooks/unisintrhook.h"
#include "launcher/launcher.h"
#include "launcher/logger.h"
#include "launcher/signal.h"
#include "launcher/superexit.h"
#include "launcher/richpresence.h"
#include "launcher/shutdown.h"
#include "launcher/options.h"
#include "script/manager.h"
#include "misc/bt5api.h"
#include "misc/device.h"
#include "misc/eamuse.h"
#include "misc/extdev.h"
#include "misc/sciunit.h"
#include "misc/sde.h"
#include "misc/vrutil.h"
#include "misc/wintouchemu.h"
#include "overlay/overlay.h"
#include "overlay/windows/patch_manager.h"
#include "rawinput/rawinput.h"
#include "rawinput/touch.h"
#include "reader/reader.h"
#include "stubs/stubs.h"
#include "touch/touch.h"
#include "util/crypt.h"
#include "util/fileutils.h"
#include "util/libutils.h"
#include "util/logging.h"
#include "util/peb.h"
#include "util/time.h"
#include "avs/ssl.h"

// std::max
#ifdef max
#undef max
#endif

// constants
static const char *STUBS[] = {"kbt.dll", "kld.dll"};

// general settings
static std::vector<std::string> game_hooks;
std::filesystem::path MODULE_PATH;
HANDLE LOG_FILE = INVALID_HANDLE_VALUE;
std::string LOG_FILE_PATH = "";
int LAUNCHER_ARGC = 0;
char **LAUNCHER_ARGV = nullptr;
std::unique_ptr<std::vector<Option>> LAUNCHER_OPTIONS;
std::string CARD_OVERRIDES[2];

// sub-systems
std::unique_ptr<api::Controller> API_CONTROLLER;
std::unique_ptr<rawinput::RawInputManager> RI_MGR;

// trigger NVIDIA Optimus & AMD Enduro High Performance Graphics
extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

static bool CHECK_DLL_IGNORE_ARCH = false;
static bool check_dll(const std::string &model) {
    if (cfg::CONFIGURATOR_STANDALONE || CHECK_DLL_IGNORE_ARCH) {
        return fileutils::file_exists(MODULE_PATH / model);
    } else {
        return fileutils::verify_header_pe(MODULE_PATH / model);
    }
}

int main_implementation(int argc, char *argv[]) {

    // remember argv, argv
    LAUNCHER_ARGC = argc;
    LAUNCHER_ARGV = argv;

    // register exception handler and control handler
    launcher::signal::init();

    // start logger
    logger::start();

    // get module path
    MODULE_PATH = libutils::module_file_name(nullptr).parent_path();

    // initialize crypt
    crypt::init();

    // initialize timer
    init_performance_counter();

    // api settings
    bool api_enable = false;
    bool api_pretty = false;
    bool api_debug = false;
    unsigned short api_port = 1337;
    std::string api_pass = "";
    std::vector<std::string> api_serial_port;
    std::vector<DWORD> api_serial_baud;

    // attach settings
    bool attach_io = false;
    bool attach_acio = false;
    bool attach_icca = false;
    bool attach_device = false;
    bool attach_extdev = false;
    bool attach_sciunit = false;
    bool attach_cpusbxpkm_printer = false;
    bool attach_iidx = false;
    bool attach_sdvx = false;
    bool attach_jb = false;
    bool attach_rb = false;
    bool attach_shogikai = false;
    bool attach_mga = false;
    bool attach_sc = false;
    bool attach_popn = false;
    bool attach_ddr = false;
    bool attach_gitadora = false;
    bool attach_nostalgia = false;
    bool attach_bbc = false;
    bool attach_hpm = false;
    bool attach_qma = false;
    bool attach_dea = false;
    bool attach_mfc = false;
    bool attach_ftt = false;
    bool attach_bs = false;
    bool attach_loveplus = false;
    bool attach_scotto = false;
    bool attach_rf3d = false;
    bool attach_drs = false;
    bool attach_we = false;
    bool attach_otoca = false;
    bool attach_silentscope = false;
    bool attach_pcm = false;
    bool attach_onpara = false;
    bool attach_bc = false;

    // misc settings
    size_t user_heap_size = 0;
    unsigned short easrv_port = 0;
    bool easrv_maint = true;
    bool easrv_smart = false;
    bool load_stubs = false;
    bool netfix_disable = false;
    bool lang_disable = false;
    bool realtime = false;
    bool cardio_enabled = false;
    bool peb_print = false;
    bool cfg_run = false;
    bool rich_presence = false;
    bool automap = false;
    bool ssl_disable = false;
    std::vector<std::string> sextet_devices;

    // parse arguments
    LAUNCHER_OPTIONS = launcher::parse_options(argc, argv);

    // detect model used to load option overrides
    auto options_version = launcher::detect_gameversion(
            LAUNCHER_OPTIONS->at(launcher::Options::PathToEa3Config).value
    );
    if (!options_version.model.empty() && options_version.model.size() < 4) {
        if (options_version.dest.size() == 1) {
            avs::game::DEST[0] = options_version.dest[0];
        }
        if (options_version.spec.size() == 1) {
            avs::game::SPEC[0] = options_version.spec[0];
        }
        if (options_version.rev.size() == 1) {
            avs::game::REV[0] = options_version.rev[0];
        }
        if (options_version.ext.size() == 10) {
            strcpy(avs::game::EXT, options_version.ext.c_str());
        }
        strcpy(avs::game::MODEL, options_version.model.c_str());
        eamuse_autodetect_game();
    }

    // grab merged game options
    auto options_ptr = games::get_options(eamuse_get_game());
    if (!options_ptr) {
        options_ptr = LAUNCHER_OPTIONS.get();
    }
    auto &options = *options_ptr;

    // check options
    // TODO: get rid of some booleans here and make use of the options directly
    if (options[launcher::Options::OpenConfigurator].value_bool()) {
        CHECK_DLL_IGNORE_ARCH = true;
        cfg::CONFIGURATOR_TYPE = cfg::ConfigType::Config;
        cfg_run = true;
    }
    if (options[launcher::Options::OpenKFControl].value_bool()) {
        CHECK_DLL_IGNORE_ARCH = true;
        cfg::CONFIGURATOR_TYPE = cfg::ConfigType::KFControl;
        cfg_run = true;
    }
    if (options[launcher::Options::ConfigurationPath].is_active()) {
        CONFIG_PATH_OVERRIDE = options[launcher::Options::ConfigurationPath].value_text();
    }
    if (options[launcher::Options::EAmusementEmulation].value_bool()
    && (options[launcher::Options::ServiceURL].is_active())) {
        log_warning("launcher", "-------------------------------------------------------------------");
        log_warning("launcher", "WARNING - WARNING - WARNING - WARNING - WARNING - WARNING - WARNING");
        log_warning("launcher", "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        log_warning("launcher", "A service URL is set **AND** E-Amusement emulation is enabled.");
        log_warning("launcher", "Either remove the service URL, or disable E-Amusement emulation.");
        log_warning("launcher", "Otherwise you may experience problems logging in.");
        log_warning("launcher", "-------------------------------------------------------------------");
        if (!cfg::CONFIGURATOR_STANDALONE) {
            Sleep(3000);
        }
    }
    if (options[launcher::Options::EAmusementEmulation].value_bool()) {
        avs::ea3::URL_SLASH = 1;
        easrv_port = 8080;
    }
    if (options[launcher::Options::SmartEAmusement].value_bool()) {
        easrv_smart = true;
    }
    if (options[launcher::Options::EAmusementMaintenance].is_active()) {
        easrv_maint = options[launcher::Options::EAmusementMaintenance].value_int() > 0;
    }
    if (options[launcher::Options::WindowedMode].value_bool()) {
        GRAPHICS_WINDOWED = true;
    }
    if (options[launcher::Options::GraphicsForceRefresh].is_active()) {
        GRAPHICS_FORCE_REFRESH = options[launcher::Options::GraphicsForceRefresh].value_int();
    }
    if (options[launcher::Options::GraphicsForceSingleAdapter].value_bool()) {
        GRAPHICS_FORCE_SINGLE_ADAPTER = true;
    }
    if (options[launcher::Options::DisplayAdapter].is_active()) {
        D3D9_ADAPTER = options[launcher::Options::DisplayAdapter].value_int();
    }
    if (options[launcher::Options::CaptureCursor].value_bool()) {
        GRAPHICS_CAPTURE_CURSOR = true;
    }
    if (options[launcher::Options::ShowCursor].value_bool()) {
        GRAPHICS_SHOW_CURSOR = true;
    }
    if (options[launcher::Options::VerboseGraphicsLogging].value_bool()) {
        GRAPHICS_LOG_HRESULT = true;
    }
    if (options[launcher::Options::VerboseAVSLogging].value_bool()) {
        hooks::avs::config::LOG = true;
    }
    if (options[launcher::Options::AdjustOrientation].value_bool()) {
        GRAPHICS_ADJUST_ORIENTATION = true;
    }
    if (options[launcher::Options::LogLevel].is_active()) {
        avs::core::LOG_LEVEL_CUSTOM = options[launcher::Options::LogLevel].value_text();
    }
    for (auto &hook : options[launcher::Options::InjectHook].values_text()) {
        game_hooks.push_back(hook);
    }
    if (options[launcher::Options::LoadStubs].value_bool()) {
        load_stubs = true;
    }
    if (options[launcher::Options::EnableAllIOModules].value_bool()) {
        attach_io = true;
    }
    if (options[launcher::Options::EnableACIOModule].value_bool()) {
        attach_acio = true;
    }
    if (options[launcher::Options::EnableICCAModule].value_bool()) {
        attach_icca = true;
    }
    if (options[launcher::Options::EnableDEVICEModule].value_bool()) {
        attach_device = true;
    }
    if (options[launcher::Options::EnableEXTDEVModule].value_bool()) {
        attach_extdev = true;
    }
    if (options[launcher::Options::EnableSCIUNITModule].value_bool()) {
        attach_sciunit = true;
    }
    if (options[launcher::Options::EnableDevicePassthrough].value_bool()) {
        hooks::device::ENABLE = false;
    }
    if (options[launcher::Options::LoadSoundVoltexModule].value_bool()) {
        attach_sdvx = true;
    }
    if (options[launcher::Options::SDVXDisableCameras].value_bool()) {
        games::sdvx::DISABLECAMS = true;
    }
    if (options[launcher::Options::SDVXNativeTouch].value_bool()) {
        games::sdvx::NATIVETOUCH = true;
    }
    if (options[launcher::Options::LoadIIDXModule].value_bool()) {
        attach_iidx = true;
    }
    if (options[launcher::Options::IIDXCameraOrderFlip].value_bool()) {
        games::iidx::FLIP_CAMS = true;
    }
    if (options[launcher::Options::IIDXDisableCameras].value_bool()) {
        games::iidx::DISABLE_CAMS = true;
    }
    if (options[launcher::Options::IIDXSoundOutputDevice].is_active()) {
        games::iidx::SOUND_OUTPUT_DEVICE = options[launcher::Options::IIDXSoundOutputDevice].value_text();
    }
    if (options[launcher::Options::IIDXAsioDriver].is_active()) {
        games::iidx::ASIO_DRIVER = options[launcher::Options::IIDXAsioDriver].value_text();
    }
    if (options[launcher::Options::IIDXTDJMode].value_bool()) {
        games::iidx::TDJ_MODE = true;
    }
    if (options[launcher::Options::LoadJubeatModule].value_bool()) {
        attach_jb = true;
    }
    if (options[launcher::Options::LoadBeatstreamModule].value_bool()) {
        attach_bs = true;
    }
    if (options[launcher::Options::LoadReflecBeatModule].value_bool()) {
        attach_rb = true;
    }
    if (options[launcher::Options::LoadShogikaiModule].value_bool()) {
        attach_shogikai = true;
    }
    if (options[launcher::Options::LoadPopnMusicModule].value_bool()) {
        attach_popn = true;
    }
    if (options[launcher::Options::PopnMusicForceHDMode].value_bool()) {
        avs::ea3::PCB_TYPE = 1;
    }
    if (options[launcher::Options::PopnMusicForceSDMode].value_bool()) {
        avs::ea3::PCB_TYPE = 0;
    }
    if (options[launcher::Options::LoadMetalGearArcadeModule].value_bool()) {
        attach_mga = true;
    }
    if (options[launcher::Options::LoadGitaDoraModule].value_bool()) {
        attach_gitadora = true;
    }
    if (options[launcher::Options::GitaDoraCabinetType].is_active()) {
        games::gitadora::CAB_TYPE = options[launcher::Options::GitaDoraCabinetType].value_int();
    }
    if (options[launcher::Options::LoadNostalgiaModule].value_bool()) {
        attach_nostalgia = true;
    }
    if (options[launcher::Options::LoadBBCModule].value_bool()) {
        attach_bbc = true;
    }
    if (options[launcher::Options::LoadHelloPopnMusicModule].value_bool()) {
        attach_hpm = true;
    }
    if (options[launcher::Options::LoadQuizMagicAcademyModule].value_bool()) {
        attach_qma = true;
    }
    if (options[launcher::Options::LoadDanceEvolutionModule].value_bool()) {
        attach_dea = true;
    }
    if (options[launcher::Options::LoadLovePlusModule].value_bool()) {
        attach_loveplus = true;
    }
    if (options[launcher::Options::GitaDoraTwoChannelAudio].value_bool()) {
        games::gitadora::TWOCHANNEL = true;
    }
    if (options[launcher::Options::LoadDDRModule].value_bool()) {
        attach_ddr = true;
    }
    if (options[launcher::Options::LoadMahjongFightClubModule].value_bool()) {
        attach_mfc = true;
    }
    if (options[launcher::Options::LoadFutureTomTomModule].value_bool()) {
        attach_ftt = true;
    }
    if (options[launcher::Options::LoadScottoModule].value_bool()) {
        attach_scotto = true;
    }
    if (options[launcher::Options::LoadRoadFighters3DModule].value_bool()) {
        attach_rf3d = true;
    }
    if (options[launcher::Options::LoadDanceRushModule].value_bool()) {
        attach_drs = true;
    }
    if (options[launcher::Options::LoadWinningElevenModule].value_bool()) {
        attach_we = true;
    }
    if (options[launcher::Options::LoadOtocaModule].value_bool()) {
        attach_otoca = true;
    }
    if (options[launcher::Options::LoadChargeMachineModule].value_bool()) {
        attach_pcm = true;
    }
    if (options[launcher::Options::LoadOngakuParadiseModule].value_bool()) {
        attach_onpara = true;
    }
    if (options[launcher::Options::LoadBusouShinkiModule].value_bool()) {
        attach_bc = true;
    }
    if (options[launcher::Options::DDR43Mode].value_bool()) {
        games::ddr::SDMODE = true;
    }
    if (options[launcher::Options::LoadSteelChronicleModule].value_bool()) {
        attach_sc = true;
    }
    if (options[launcher::Options::DisableNetworkFixes].value_bool()) {
        netfix_disable = true;
    }
    if (options[launcher::Options::DisableACPHook].value_bool()) {
        lang_disable = true;
    }
    if (options[launcher::Options::DisableSignalHandling].value_bool()) {
        launcher::signal::DISABLE = true;
    }
    if (options[launcher::Options::DebugCreateFile].value_bool()) {
        DEVICE_CREATEFILE_DEBUG = true;
    }
    if (options[launcher::Options::BlockingLogger].value_bool()) {
        logger::BLOCKING = true;
    }
    if (options[launcher::Options::OutputPEB].value_bool()) {
        peb_print = true;
    }
    if (options[launcher::Options::EnableBemaniTools5API].value_bool()) {
        BT5API_ENABLED = true;
    }
    if (options[launcher::Options::CardIOHIDReaderSupport].value_bool()) {
        cardio_enabled = true;
    }
    if (options[launcher::Options::SDVXPrinterEmulation].value_bool()) {
        attach_cpusbxpkm_printer = true;
    }
    if (options[launcher::Options::SDVXPrinterOutputOverwrite].value_bool()) {
        games::shared::PRINTER_OVERWRITE_FILE = true;
    }
    for (auto &path : options[launcher::Options::SDVXPrinterOutputPath].values_text()) {
        games::shared::PRINTER_PATH.push_back(path);
    }
    for (auto &path : options[launcher::Options::SDVXPrinterOutputFormat].values_text()) {
        games::shared::PRINTER_FORMAT.push_back(path);
    }
    if (options[launcher::Options::SDVXPrinterJPGQuality].is_active()) {
        games::shared::PRINTER_JPG_QUALITY = options[launcher::Options::SDVXPrinterJPGQuality].value_int();
    }
    if (options[launcher::Options::SDVXPrinterOutputClear].value_bool()) {
        games::shared::PRINTER_CLEAR = true;
    }
    if (options[launcher::Options::HTTP11].is_active()) {
        avs::ea3::HTTP11 = options[launcher::Options::HTTP11].value_int();
    }
    if (options[launcher::Options::DisableSSL].value_bool()) {
        ssl_disable = true;
    }
    if (options[launcher::Options::URLSlash].is_active()) {
        avs::ea3::URL_SLASH = options[launcher::Options::URLSlash].value_int();
    }
    if (options[launcher::Options::RealtimeProcessPriority].value_bool()) {
        realtime = true;
    }
    if (options[launcher::Options::HeapSize].is_active()) {
        user_heap_size = options[launcher::Options::HeapSize].value_int();
    }
    if (options[launcher::Options::DisableAvsVfsDriveMountRedirection].is_active()) {
        hooks::avs::config::DISABLE_VFS_DRIVE_REDIRECTION = true;
    }
    if (options[launcher::Options::PathToAppConfig].is_active()) {
        avs::ea3::APP_PATH = options[launcher::Options::PathToAppConfig].value_text();
    }
    if (options[launcher::Options::PathToAvsConfig].is_active()) {
        avs::core::CFG_PATH = options[launcher::Options::PathToAvsConfig].value_text();
    }
    if (options[launcher::Options::PathToEa3Config].is_active()) {
        avs::ea3::CFG_PATH = options[launcher::Options::PathToEa3Config].value_text();
    }
    if (options[launcher::Options::PathToBootstrap].is_active()) {
        avs::ea3::BOOTSTRAP_PATH = options[launcher::Options::PathToBootstrap].value_text();
    }
    if (options[launcher::Options::PathToLog].is_active()) {
        avs::core::LOG_PATH = options[launcher::Options::PathToLog].value_text();
    }
    if (options[launcher::Options::PCBID].is_active()) {
        avs::ea3::PCBID_CUSTOM = options[launcher::Options::PCBID].value_text();
    }
    if (options[launcher::Options::SOFTID].is_active()) {
        avs::ea3::SOFTID_CUSTOM = options[launcher::Options::SOFTID].value_text();
    }
    if (options[launcher::Options::ServiceURL].is_active()) {
        avs::ea3::URL_CUSTOM = options[launcher::Options::ServiceURL].value_text();
    }
    if (options[launcher::Options::PathToModules].is_active()) {
        std::error_code err;

        auto &path = options[launcher::Options::PathToModules].value_text();
        auto module_path = std::filesystem::absolute(path, err);

        if (err) {
            if (cfg::CONFIGURATOR_STANDALONE) {
                log_warning("launcher", "failed to resolve module path '{}': {}", path, err.message());
            } else {
                log_fatal("launcher", "failed to resolve module path '{}': {}", path, err.message());
            }
        }

        MODULE_PATH = std::move(module_path);
        SetDllDirectoryW(MODULE_PATH.c_str());
    }
    if (options[launcher::Options::Player1Card].is_active()) {
        CARD_OVERRIDES[0] = options[launcher::Options::Player1Card].value_text();
    }
    if (options[launcher::Options::Player2Card].is_active()) {
        CARD_OVERRIDES[1] = options[launcher::Options::Player2Card].value_text();
    }
    for (auto &reader : options[launcher::Options::ICCAReaderPort].values_text()) {
        static int reader_id = 0;
        if (reader_id < 2) {
            start_reader_thread(reader, reader_id++);
        } else {
            if (!cfg::CONFIGURATOR_STANDALONE) {
                log_fatal("launcher", "too many readers specified (maximum is 2)");
            }
            break;
        }
    }
    for (auto &sextet : options[launcher::Options::SextetStreamPort].values_text()) {
        sextet_devices.emplace_back(sextet);
    }
    if (options[launcher::Options::HIDSmartCard].value_bool()) {
        WINSCARD_CONFIG.cardinfo_callback = eamuse_scard_callback;
        scard_threadstart();
    }
    if (options[launcher::Options::HIDSmartCardOrderFlip].value_bool()) {
        WINSCARD_CONFIG.flip_order = true;
    }
    if (options[launcher::Options::HIDSmartCardOrderToggle].value_bool()) {
        WINSCARD_CONFIG.toggle_order = true;
    }
    if (options[launcher::Options::CardIOHIDReaderOrderFlip].value_bool()) {
        CARDIO_RUNNER_FLIP = true;
    }
    if (options[launcher::Options::ICCAReaderPortToggle].is_active()) {
        start_reader_thread(options[launcher::Options::ICCAReaderPortToggle].value_text(), -1);
    }
    if (options[launcher::Options::IntelSDEFolder].is_active()) {
        sde_init(options[launcher::Options::IntelSDEFolder].value_text());
    }
    if (options[launcher::Options::AdapterNetwork].is_active()) {
        NETWORK_ADDRESS = options[launcher::Options::AdapterNetwork].value_text();
    }
    if (options[launcher::Options::AdapterSubnet].is_active()) {
        NETWORK_SUBNET = options[launcher::Options::AdapterSubnet].value_text();
    }
    if (options[launcher::Options::APITCPPort].is_active()) {
        api_enable = true;
        api_port = options[launcher::Options::APITCPPort].value_int();
    }
    if (options[launcher::Options::APIPassword].is_active()) {
        api_pass = options[launcher::Options::APIPassword].value_text();
    }
    if (options[launcher::Options::APISerialPort].is_active()) {
        api_serial_port.push_back(options[launcher::Options::APISerialPort].value_text());
    }
    if (options[launcher::Options::APISerialBaud].is_active()) {
        api_serial_baud.push_back(options[launcher::Options::APISerialBaud].value_int());
    }
    if (options[launcher::Options::APIPretty].value_bool()) {
        api_pretty = true;
    }
    if (options[launcher::Options::APIVerboseLogging].value_bool()) {
        api::LOGGING = true;
    }
    if (options[launcher::Options::APIDebugMode].value_bool()) {
        api_debug = true;
    }
    if (options[launcher::Options::DisableDebugHooks].value_bool()) {
        debughook::DEBUGHOOK_LOGGING = false;
    }
    if (options[launcher::Options::ForceWinTouch].value_bool()) {
        rawinput::touch::DISABLED = true;
    }
    if (options[launcher::Options::SDVXForce720p].value_bool()) {
        GRAPHICS_SDVX_FORCE_720 = true;
    }
    if (options[launcher::Options::InvertTouchCoordinates].value_bool()) {
        rawinput::touch::INVERTED = true;
    }
    if (options[launcher::Options::DisableTouchCardInsert].value_bool()) {
        SPICETOUCH_CARD_DISABLE = true;
    }
    if (options[launcher::Options::ForceTouchEmulation].value_bool()) {
        wintouchemu::FORCE = true;
    }
    if (options[launcher::Options::Graphics9On12].value_bool()) {
        GRAPHICS_9_ON_12 = true;
    }
    if (options[launcher::Options::NoLegacy].value_bool()) {
        rawinput::NOLEGACY = true;
    }
    if (options[launcher::Options::RichPresence].value_bool()) {
        rich_presence = true;
    }
    if (options[launcher::Options::DiscordAppID].is_active()) {
        richpresence::discord::APPID_OVERRIDE = options[launcher::Options::DiscordAppID].value_text();
    }
    if (options[launcher::Options::ScreenshotFolder].is_active()) {
        GRAPHICS_SCREENSHOT_DIR = options[launcher::Options::ScreenshotFolder].value_text();
    }
    if (options[launcher::Options::DisableColoredOutput].value_bool()) {
        logger::COLOR = false;
    }
    if (options[launcher::Options::DisableOverlay].value_bool()) {
        overlay::ENABLED = false;
    }
    if (options[launcher::Options::DisableAudioHooks].value_bool()) {
        hooks::audio::ENABLED = false;
    }
    if (options[launcher::Options::AudioBackend].is_active()) {
        auto &name = options[launcher::Options::AudioBackend].value_text();
        auto backend = hooks::audio::name_to_backend(name.c_str());
        if (!backend.has_value() && !cfg::CONFIGURATOR_STANDALONE) {
            log_fatal("launcher", "invalid audio backend: {}", name);
        }

        hooks::audio::BACKEND = backend;
    }
    if (options[launcher::Options::AsioDriverId].is_active()) {
        hooks::audio::ASIO_DRIVER_ID = options[launcher::Options::AsioDriverId].value_int();
    }
    if (options[launcher::Options::AudioDummy].value_bool()) {
        hooks::audio::USE_DUMMY = true;
    }
    if (options[launcher::Options::EAAutomap].value_bool()) {
        easrv_maint = false;
        automap = true;
        avs::automap::ENABLED = true;
        avs::automap::PATCH = true;
        avs::automap::RESTRICT_NETWORK = true;
        avs::automap::DUMP = true;
    }
    if (options[launcher::Options::EANetdump].value_bool()) {
        easrv_maint = false;
        automap = true;
        avs::automap::ENABLED = true;
        avs::automap::PATCH = false;
        avs::automap::RESTRICT_NETWORK = true;
        avs::automap::DUMP = true;
    }
    if (options[launcher::Options::GameExecutable].is_active()) {
        avs::game::DLL_NAME = options[launcher::Options::GameExecutable].value_text();
    }

    // API debugging
    if (api_debug && !cfg::CONFIGURATOR_STANDALONE) {
        API_CONTROLLER = std::make_unique<api::Controller>(api_port, api_pass, api_pretty);
        for (size_t i = 0; i < std::min(api_serial_port.size(), api_serial_baud.size()); i++) {
            API_CONTROLLER->listen_serial(api_serial_port[i], api_serial_baud[i]);
        }
        if (cfg_run) {
            exit(spicecfg_run(sextet_devices));
        } else {
            while (API_CONTROLLER->server_running) {
                Sleep(100);
            }
        }
        log_fatal("launcher", "API server stopped");
    }

    // delay
    if (!cfg::CONFIGURATOR_STANDALONE &&
            (options[launcher::Options::DelayBy5Seconds].value_bool()))
    {
        log_info("launcher", "delay by 5000ms...");
        Sleep(5000);
    }

    // create log file
    if (!cfg::CONFIGURATOR_STANDALONE) {
        avs::core::create_log();
    }

    // log
#ifdef SPICE64
    log_info("launcher", "SpiceTools Bootstrap (x64)");
#else
    log_info("launcher", "SpiceTools Bootstrap (x32)");
#endif
    log_info("launcher", "{}", VERSION_STRING);

    // log command line arguments
    std::ostringstream arguments;
    for (auto &root_option : options) {
        std::vector<Option> options_all { root_option };

        options_all.insert(options_all.end(),
                root_option.alternatives.begin(),
                root_option.alternatives.end());

        for (const auto &option : options_all) {
            if (option.is_active()) {
                auto &definition = option.get_definition();

                arguments << " -" << definition.name;

                if (definition.type != OptionType::Bool) {
                    arguments << " ";

                    if (definition.sensitive) {
                        arguments << std::string(5, '*');
                    } else {
                        arguments << option.value;
                    }
                }
            }
        }
    }
    log_info("launcher", "arguments:{}", arguments.str());

    // disable automatic system/monitor sleep
    if (!SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED)) {
        log_warning("launcher", "could not set thread execution state: {}", GetLastError());
    }

    // set process priority
    if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS)) {
        log_warning("launcher", "could not set process priority to high: {}", GetLastError());
    }
    if (realtime) {
        // while testing, realtime only worked when being set to high before, so this is fine
        if (!SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS)) {
            log_warning("launcher", "could not set process priority to realtime: {}", GetLastError());
        }
    }

    // enable super exit
    superexit::enable();

    // auto detect game if not specified
    if (avs::game::DLL_NAME.empty()) {
        bool module_path_tried = false;
        do {

            // IIDX
            if (check_dll("bm2dx.dll")) {
                avs::game::DLL_NAME = "bm2dx.dll";
                attach_io = true;
                attach_iidx = true;
                break;
            }

            // SDVX
            if (check_dll("soundvoltex.dll")) {
                avs::game::DLL_NAME = "soundvoltex.dll";
                attach_io = true;
                attach_sdvx = true;
#ifdef SPICE64
                debughook::DEBUGHOOK_LOGGING = false;
#endif
                break;
            }

            // JB
            if (check_dll("jubeat.dll")) {
                avs::game::DLL_NAME = "jubeat.dll";
                attach_io = true;
                attach_jb = true;
                break;
            }

            // RB
            if (check_dll("reflecbeat.dll")) {
                avs::game::DLL_NAME = "reflecbeat.dll";
                attach_io = true;
                attach_rb = true;
                break;
            }

            // Shogikai
            if (check_dll("shogi_engine.dll")) {
                avs::game::DLL_NAME = "system.dll";
                attach_io = true;
                attach_shogikai = true;

                // automatically show cursor when no touchscreen is available
                if (!is_touch_available()) {
                    GRAPHICS_SHOW_CURSOR = true;
                }
                break;
            }

            // PCM & MGA
            if (check_dll("launch.dll")) {
                avs::game::DLL_NAME = "launch.dll";
                attach_io = true;

                // MGA uses ESS while PCM does not
                if (check_dll("ess.dll")) {
                    attach_mga = true;
                } else {
                    attach_pcm = true;
                }

                break;
            }

            // DEA
            if (check_dll("arkkdm.dll")) {
                avs::game::DLL_NAME = "arkkdm.dll";
                attach_io = true;
                attach_dea = true;

                // the game is windowed by default unless we set the env
                if (GRAPHICS_WINDOWED) {
                    GRAPHICS_WINDOWED = false;
                } else {
                    SetEnvironmentVariable("DAMAC_VIEWER_FULLSCREEN", "0");
                }

                break;
            }

            // BS
            if (check_dll("beatstream.dll")) {
                avs::game::DLL_NAME = "beatstream.dll";
                attach_io = true;
                attach_bs = true;

                // game crash fix
                easrv_maint = false;

                // automatically show cursor when no touchscreen is available
                if (!is_touch_available()) {
                    GRAPHICS_SHOW_CURSOR = true;
                }

                break;
            }

            // RF3D
            if (check_dll("jgt.dll")) {
                avs::game::DLL_NAME = "jgt.dll";
                attach_io = true;
                attach_rf3d = true;
                break;
            }

            // MUSECA
            if (check_dll("museca.dll")) {
                avs::game::DLL_NAME = "museca.dll";
                attach_io = true;
                break;
            }

            // pop'n Lapistoria/eclale/UsaNeko/peace
            if (check_dll("popn22.dll")) {
                avs::game::DLL_NAME = "popn22.dll";
                attach_io = true;
                attach_popn = true;
                break;
            }

            // pop'n Sunny Park
            if (check_dll("popn21.dll")) {
                avs::game::DLL_NAME = "popn21.dll";
                attach_io = true;
                attach_popn = true;
                break;
            }

            // pop'n Fantasia
            if (check_dll("popn20.dll")) {
                avs::game::DLL_NAME = "popn20.dll";
                attach_io = true;
                attach_popn = true;
                break;
            }

            // pop'n Tune Street
            if (check_dll("popn19.dll")) {
                avs::game::DLL_NAME = "popn19.dll";
                attach_io = true;
                attach_popn = true;
                break;
            }

            // DDR Ace/A20 (bio2)
            if (check_dll("arkmdxbio2.dll")) {
                avs::game::DLL_NAME = "arkmdxbio2.dll";
                attach_io = true;
                attach_ddr = true;
                break;
            }

            // DDR Ace/A20 (p3io)
            if (check_dll("arkmdxp3.dll")) {
                avs::game::DLL_NAME = "arkmdxp3.dll";
                attach_io = true;
                attach_ddr = true;
                break;
            }

            // DDR 2013/2014 (old cabinets)
            if (check_dll("mdxja_945.dll")) {
                avs::game::DLL_NAME = "mdxja_945.dll";
                attach_io = true;
                attach_ddr = true;
                break;
            }

            // DDR 2013/2014 (white cabinet)
            if (check_dll("mdxja_hm65.dll")) {
                avs::game::DLL_NAME = "mdxja_hm65.dll";
                attach_io = true;
                attach_ddr = true;
                break;
            }

            // DDR X2/X3
            if (check_dll("ddr.dll")) {
                avs::game::DLL_NAME = "ddr.dll";
                attach_io = true;
                attach_ddr = true;
                break;
            }

            // GitaDora
            if (check_dll("gdxg.dll")) {
                avs::game::DLL_NAME = "gdxg.dll";
                attach_io = true;
                attach_device = true;
                attach_extdev = true;
                attach_gitadora = true;
                break;
            }

            // Nostalgia
            if (check_dll("nostalgia.dll")) {
                avs::game::DLL_NAME = "nostalgia.dll";
                attach_io = true;
                attach_nostalgia = true;

                // automatically show cursor when no touchscreen is available
                if (!is_touch_available()) {
                    GRAPHICS_SHOW_CURSOR = true;
                }

                break;
            }

            // Bishi Bashi Channel
            if (check_dll("bsch.dll")) {
                avs::game::DLL_NAME = "bsch.dll";
                attach_io = true;
                attach_bbc = true;
                break;
            }

            // HELLO! Pop'n Music
            if (check_dll("popn.dll")) {
                avs::game::DLL_NAME = "popn.dll";
                attach_io = true;
                attach_hpm = true;
                break;
            }

            // Quiz Magic Academy
            if (check_dll("client.dll")) {
                avs::game::DLL_NAME = "client.dll";
                attach_io = true;
                attach_qma = true;
                break;
            }

            // LovePlus
            if (check_dll("arkklp.dll")) {
                avs::game::DLL_NAME = "arkklp.dll";
                attach_io = true;
                attach_loveplus = true;
                attach_cpusbxpkm_printer = true;
                break;
            }

            // Steel Chronicle
            if (check_dll("arkkgg.dll")) {
                avs::game::DLL_NAME = "arkkgg.dll";
                attach_io = true;
                attach_sc = true;
                easrv_maint = false;
                break;
            }

            // Mahjong Fight Club
            if (check_dll("allinone.dll")) {
                avs::game::DLL_NAME = "system.dll";
                attach_io = true;
                attach_mfc = true;
                break;
            }

            // FutureTomTom
            if (check_dll("arkmmd.dll")) {
                avs::game::DLL_NAME = "arkmmd.dll";
                attach_io = true;
                attach_ftt = true;

                // the game is windowed by default unless we set the env
                if (GRAPHICS_WINDOWED) {
                    GRAPHICS_WINDOWED = false;
                } else {
                    SetEnvironmentVariable("DAMAC_VIEWER_FULLSCREEN", "0");
                }

                break;
            }

            // Scotto
            if (check_dll("scotto.dll")) {
                avs::game::DLL_NAME = "scotto.dll";
                attach_io = true;
                attach_scotto = true;
                break;
            }

            // TsumTsum
            if (check_dll("arko26.dll")) {
                avs::game::DLL_NAME = "arko26.dll";
                attach_io = true;
                break;
            }

            // DANCERUSH
            if (check_dll("superstep.dll")) {
                avs::game::DLL_NAME = "superstep.dll";
                attach_io = true;
                attach_drs = true;
                break;
            }

            // Winning Eleven 2012
            if (check_dll("weac12_bootstrap_release.dll")) {
                avs::game::DLL_NAME = "weac12_bootstrap_release.dll";
                attach_io = true;
                attach_we = true;
                break;
            }

            // Winning Eleven 2014
            if (check_dll("arknck.dll")) {
                avs::game::DLL_NAME = "arknck.dll";
                attach_io = true;
                attach_we = true;

                // automatically show cursor when no touchscreen is available
                if (!is_touch_available()) {
                    GRAPHICS_SHOW_CURSOR = true;
                }

                break;
            }

            // Otoca D'or
            if (check_dll("arkkep.dll")) {
                avs::game::DLL_NAME = "arkkep.dll";
                attach_io = true;
                attach_otoca = true;
                break;
            }

            // Silent Scope: Bone Eater
            if (check_dll("arkndd.dll")) {
                avs::game::DLL_NAME = "arkndd.dll";
                attach_io = true;
                attach_silentscope = true;
                break;
            }

            // Ongaku Paradise
            if (check_dll("arkjc9.dll")) {
                avs::game::DLL_NAME = "arkjc9.dll";
                attach_io = true;
                attach_onpara = true;
                break;
            }

            // Busou Shinki: Armored Princess Battle Conductor
            if (check_dll("kamunity.dll")) {
                avs::game::DLL_NAME = "kamunity.dll";
                attach_io = true;
                attach_bc = true;
                break;
            }

            // try module path
            if (!module_path_tried) {
                module_path_tried = true;

                MODULE_PATH /= "modules";
                SetDllDirectoryW(MODULE_PATH.c_str());

                MODULE_PATH /= "";

                continue;
            }

            // usage error
            if (!cfg::CONFIGURATOR_STANDALONE
            && (!CHECK_DLL_IGNORE_ARCH)) {
                log_fatal("launcher", "module auto detection failed.");
            }
            break;

        } while (true);
    }

    // set error mode to show all errors
    SetErrorMode(0);

    // set the AVS heap size to a default value varying by game
    avs::core::set_default_heap_size(avs::game::DLL_NAME);

    // load the games
    std::vector<games::Game *> games;
    if (attach_popn) {
        games.push_back(new games::popn::POPNGame());
    }
    if (attach_bbc) {
        avs::core::set_default_heap_size("bsch.dll");
        games.push_back(new games::bbc::BBCGame());
    }
    if (attach_ddr) {
        avs::core::set_default_heap_size("arkmdxp3.dll");
        games.push_back(new games::ddr::DDRGame());
    }
    if (attach_iidx) {
        avs::core::set_default_heap_size("bm2dx.dll");
        games.push_back(new games::iidx::IIDXGame());
    }
    if (attach_sdvx) {
        avs::core::set_default_heap_size("soundvoltex.dll");
        games.push_back(new games::sdvx::SDVXGame());
    }
    if (attach_jb) {
        avs::core::set_default_heap_size("jubeat.dll");
        games.push_back(new games::jb::JBGame());
    }
    if (attach_nostalgia) {
        avs::core::set_default_heap_size("nostalgia.dll");
        games.push_back(new games::nost::NostGame());
    }
    if (attach_gitadora) {
        games.push_back(new games::gitadora::GitaDoraGame());
    }
    if (attach_hpm) {
        avs::core::set_default_heap_size("popn.dll");
        games.push_back(new games::hpm::HPMGame());
    }
    if (attach_mga) {
        games.push_back(new games::mga::MGAGame());
    }
    if (attach_sc) {
        games.push_back(new games::sc::SCGame());
    }
    if (attach_rb) {
        games.push_back(new games::rb::RBGame());
    }
    if (attach_shogikai) {
        games.push_back(new games::shogikai::ShogikaiGame());
    }
    if (attach_qma) {
        avs::core::set_default_heap_size("client.dll");
        games.push_back(new games::qma::QMAGame());
    }
    if (attach_dea) {
        games.push_back(new games::dea::DEAGame());
    }
    if (attach_mfc) {
        avs::core::set_default_heap_size("system.dll");
        games.push_back(new games::mfc::MFCGame());
    }
    if (attach_ftt) {
        avs::core::set_default_heap_size("arkmmd.dll");
        games.push_back(new games::ftt::FTTGame());
    }
    if (attach_bs) {
        games.push_back(new games::bs::BSGame());
    }
    if (attach_loveplus) {
        games.push_back(new games::loveplus::LovePlusGame());
    }
    if (attach_scotto) {
        avs::core::set_default_heap_size("scotto.dll");
        games.push_back(new games::scotto::ScottoGame());
    }
    if (attach_rf3d) {
        games.push_back(new games::rf3d::RF3DGame());
    }
    if (attach_drs) {
        avs::core::set_default_heap_size("superstep.dll");
        games.push_back(new games::drs::DRSGame());
    }
    if (attach_we) {
        games.push_back(new games::we::WEGame());
    }
    if (attach_otoca) {
        games.push_back(new games::otoca::OtocaGame());
    }
    if (attach_silentscope) {
        games.push_back(new games::silentscope::SilentScopeGame());
    }
    if (attach_pcm) {
        games.push_back(new games::pcm::PCMGame());
    }
    if (attach_onpara) {
        avs::core::set_default_heap_size("arkjc9.dll");
        games.push_back(new games::onpara::OnparaGame());
    }
    if (attach_bc) {
        avs::core::set_default_heap_size("kamunity.dll");
        games.push_back(new games::bc::BCGame());
    }

    // apply user heap size, if defined
    if (user_heap_size > 0) {
        avs::core::HEAP_SIZE = user_heap_size;
    }

    // call pre-attach
    for (auto game : games) {
        game->pre_attach();
    }

    // run configuration utility
    if (cfg_run || cfg::CONFIGURATOR_STANDALONE) {
        if (api_enable || api_debug) {
            API_CONTROLLER = std::make_unique<api::Controller>(api_port, api_pass, api_pretty);
            for (size_t i = 0; i < std::min(api_serial_port.size(), api_serial_baud.size()); i++) {
                API_CONTROLLER->listen_serial(api_serial_port[i], api_serial_baud[i]);
            }
        }
        exit(spicecfg_run(sextet_devices));
    }

    // initialize raw input
    RI_MGR = std::make_unique<rawinput::RawInputManager>();
    for (const auto &device : sextet_devices) {
        RI_MGR->sextet_register(device);
    }

    // print devices
    RI_MGR->devices_print();

    // cardio
    if (cardio_enabled) {
        cardio_runner_start(true);
    }

    // load stubs
    if (load_stubs) {
        for (const auto &stub : STUBS) {
            if (fileutils::verify_header_pe(MODULE_PATH / stub)) {
                libutils::load_library(MODULE_PATH / stub);
            } else {
                log_warning("launcher", "failed to load stubs!");
                load_stubs = false;
            }
        }
    }

    // locale hook
    if (!lang_disable) {
        hooks::lang::early_init();
    }

    // load DLLs
    if (!avs::core::load_dll()) {
        log_fatal("launcher", "avs boot failure");
    }
    avs::ea3::load_dll();

    // net fix
    if (!netfix_disable) {
        networkhook_init();
    }

    // boot AVS
    avs::core::boot();

    // initialize SSL
    if (!ssl_disable) {
        avs::ssl::init();
    }

    // copy defaults to nvram
    avs::core::copy_defaults();

    // load game
    avs::game::load_dll();

    // VR
    if (options[launcher::Options::VREnable].is_active()) {
        vrutil::init();
    }

    // attach games
    for (auto game : games) {
        game->attach();
    }

    // attach stub functions
    if (!load_stubs) {
        stubs::attach();
    }

    // locale hook
    if (!lang_disable) {
        hooks::lang::init();
    }

    // exception/signal hook
    launcher::signal::attach();

    // audio hook
    hooks::audio::init();

    // DirectInput 8 hook
    hooks::input::dinput8::init();

    // D3D9 hook
    graphics_init();

    // debug hook
    debughook::attach();

    // device debug
    if (DEVICE_CREATEFILE_DEBUG) {
        devicehook_init();
    }

    // server
    if (easrv_port != 0u && !easrv_smart) {
        easrv_start(easrv_port, easrv_maint, 4, 8);
    }

    // acio attach
    if (attach_io || attach_acio) {
        acio::attach();
    }

    // acio icca attach
    if (attach_icca) {
        acio::attach_icca();
    }

    // device attach
    if (attach_io || attach_device) {
        spicedevice_attach();
    }

    // ext dev attach
    if (attach_io || attach_extdev) {
        extdev_attach();
    }

    // sci unit attach
    if (attach_io || attach_sciunit) {
        sciunit_attach();
    }

    // SDVX printer attach
    if (attach_cpusbxpkm_printer) {
        games::shared::printer_attach();
    }

    // net fix
    if (!netfix_disable) {
        networkhook_init();
    }

    // layeredfs
    if (fileutils::dir_exists("data_mods") &&
        !fileutils::file_exists("ifs_hook.dll") &&
        !fileutils::file_exists(MODULE_PATH / "ifs_hook.dll"))
    {
        layeredfs::init();
    }

    // load hooks
    for (auto &hook : game_hooks) {
        log_info("launcher", "loading hook DLL {}", hook);
        HMODULE module;
        if (!(module = libutils::try_library(hook))) {
            log_warning("launcher", "failed to load hook {}", hook);
        } else {
            bt5api_hook(module);
        }
    }

    // load AVS-EA3
    avs::ea3::boot(easrv_port, easrv_maint, easrv_smart);

    // apply patches
    {
        overlay::windows::PatchManager patch_manager(nullptr, true);
    }

    // load scripts
    script::manager_scan();
    script::manager_boot();

    // eamuse init
    eamuse_autodetect_game();

    // unis device hook
    unisintrhook_init();

    // BT5API
    if (BT5API_ENABLED) {
        bt5api_init();
    }

    // API
    if (api_enable || std::max(api_serial_port.size(), api_serial_baud.size()) > 0) {
        API_CONTROLLER = std::make_unique<api::Controller>(api_port, api_pass, api_pretty);
    }
    for (size_t i = 0; i < std::max(api_serial_port.size(), api_serial_baud.size()); i++) {
        API_CONTROLLER->listen_serial(api_serial_port[i], api_serial_baud[i]);
    }

    // start coin input thread
    eamuse_coin_start_thread();

    // print PEB
    if (peb_print) {
        peb::peb_print();
    }

    // enable automap
    if (automap) {
        avs::automap::enable();
    }

    // initialize rich presence
    if (rich_presence) {
        richpresence::init();
    }

    // turn off controlled game lights
    auto lights = games::get_lights(eamuse_get_game());
    if (lights) {
        for (auto &light : *lights) {
            GameAPI::Lights::writeLight(RI_MGR, light, 0.f);
        }
        RI_MGR->devices_flush_output();
    }

    // attach games
    for (auto game : games) {
        game->post_attach();
    }

    // game start
    log_info("launcher", "calling game entry");
    avs::game::entry_main();

    // clear presence
    richpresence::shutdown();

    // clean up VR
    vrutil::shutdown();

    // script shutdown
    script::manager_shutdown();

    // detach games
    for (auto game : games) {
        game->detach();
    }

    // sci unit detach
    if (attach_io || attach_sciunit) {
        sciunit_detach();
    }

    // ext dev detach
    if (attach_io || attach_extdev) {
        extdev_detach();
    }

    // device detach
    if (attach_io || attach_device) {
        spicedevice_detach();
    }

    // acio detach
    if (attach_io || attach_acio) {
        acio::detach();
    }

    // delete games
    while (!games.empty()) {
        delete games.back();
        games.pop_back();
    }

    // free api controller
    API_CONTROLLER.reset();

    // stop coin input thread
    eamuse_coin_stop_thread();

    // BT5API
    if (BT5API_ENABLED) {
        bt5api_dispose();
    }

    // stop raw input
    RI_MGR.reset();

    // debug hook
    debughook::detach();

    // scard
    scard_fini();

    // stop reader thread in case it was running
    stop_reader_thread();

    // cardio
    if (cardio_enabled) {
        cardio_runner_stop();
    }

    // server
    if (easrv_port != 0u) {
        easrv_shutdown();
    }

    // AVS EA3 shutdown
    avs::ea3::shutdown();

    // AVS cleanup
    avs::core::shutdown();

    // dispose crypt
    crypt::dispose();

    // disable super exit
    superexit::disable();

    // shutdown
    log_warning("launcher", "end");
    launcher::stop_subsystems();

    return 0;
}

#ifndef SPICETOOLS_SPICECFG_STANDALONE
int main(int argc, char *argv[]) {
    return main_implementation(argc, argv);
}
#endif

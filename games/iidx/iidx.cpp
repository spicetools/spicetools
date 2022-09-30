// set version to Windows 7 to enable Media Foundation functions
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0601

// turn on C-style COM objects
#define CINTERFACE

#include "iidx.h"

#include <mfobjects.h>
#include <mfidl.h>

#include "acioemu/handle.h"
#include "avs/core.h"
#include "avs/game.h"
#include "games/io.h"
#include "hooks/avshook.h"
#include "hooks/cfgmgr32hook.h"
#include "hooks/devicehook.h"
#include "hooks/graphics/graphics.h"
#include "hooks/setupapihook.h"
#include "hooks/sleephook.h"
#include "launcher/options.h"
#include "misc/wintouchemu.h"
#include "misc/eamuse.h"
#include "util/detour.h"
#include "util/fileutils.h"
#include "util/libutils.h"
#include "util/memutils.h"
#include "util/utils.h"

#include "bi2a.h"
#include "bi2x_hook.h"
#include "ezusb.h"
#include "io.h"

static VTBL_TYPE(IMFActivate, GetAllocatedString) GetAllocatedString_orig = nullptr;

static decltype(MFEnumDeviceSources) *MFEnumDeviceSources_orig = nullptr;
static decltype(RegCloseKey) *RegCloseKey_orig = nullptr;
static decltype(RegEnumKeyA) *RegEnumKeyA_orig = nullptr;
static decltype(RegOpenKeyA) *RegOpenKeyA_orig = nullptr;
static decltype(RegOpenKeyExA) *RegOpenKeyExA_orig = nullptr;
static decltype(RegQueryValueExA) *RegQueryValueExA_orig = nullptr;

namespace games::iidx {

    // constants
    const HKEY PARENT_ASIO_REG_HANDLE = reinterpret_cast<HKEY>(0x3001);
    const HKEY DEVICE_ASIO_REG_HANDLE = reinterpret_cast<HKEY>(0x3002);
    const char *ORIGINAL_ASIO_DEVICE_NAME = "XONAR SOUND CARD(64)";

    // settings
    bool FLIP_CAMS = false;
    bool DISABLE_CAMS = false;
    bool TDJ_MODE = false;
    std::optional<std::string> SOUND_OUTPUT_DEVICE = std::nullopt;
    std::optional<std::string> ASIO_DRIVER = std::nullopt;

    // states
    static HKEY real_asio_reg_handle = nullptr;
    static HKEY real_asio_device_reg_handle = nullptr;
    static uint16_t IIDXIO_TT_STATE[2]{};
    static uint16_t IIDXIO_TT_DIRECTION[2]{};
    static bool IIDXIO_TT_PRESSED[2]{};
    char IIDXIO_LED_TICKER[10] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '\x00'};
    bool IIDXIO_LED_TICKER_READONLY = false;
    std::mutex IIDX_LED_TICKER_LOCK;

    static LONG WINAPI RegOpenKeyA_hook(HKEY hKey, LPCSTR lpSubKey, PHKEY phkResult) {
        if (lpSubKey != nullptr && phkResult != nullptr) {
            if (hKey == HKEY_LOCAL_MACHINE &&
                    ASIO_DRIVER.has_value() &&
                    _stricmp(lpSubKey, "software\\asio") == 0)
            {
                *phkResult = PARENT_ASIO_REG_HANDLE;

                return RegOpenKeyA_orig(hKey, lpSubKey, &real_asio_reg_handle);
            }
        }

        return RegOpenKeyA_orig(hKey, lpSubKey, phkResult);
    }

    static LONG WINAPI RegOpenKeyExA_hook(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired,
            PHKEY phkResult)
    {
        if (lpSubKey != nullptr && phkResult != nullptr) {
            if (hKey == PARENT_ASIO_REG_HANDLE &&
                    ASIO_DRIVER.has_value() &&
                    _stricmp(lpSubKey, ORIGINAL_ASIO_DEVICE_NAME) == 0)
            {
                *phkResult = DEVICE_ASIO_REG_HANDLE;

                log_info("iidx::asio", "replacing '{}' with '{}'", lpSubKey, ASIO_DRIVER.value());

                return RegOpenKeyExA_orig(real_asio_reg_handle, ASIO_DRIVER.value().c_str(), ulOptions, samDesired,
                        &real_asio_device_reg_handle);
            }
        }

        return RegOpenKeyExA_orig(hKey, lpSubKey, ulOptions, samDesired, phkResult);
    }

    static LONG WINAPI RegEnumKeyA_hook(HKEY hKey, DWORD dwIndex, LPSTR lpName, DWORD cchName) {
        if (hKey == PARENT_ASIO_REG_HANDLE && ASIO_DRIVER.has_value()) {
            if (dwIndex == 0) {
                auto ret = RegEnumKeyA_orig(real_asio_reg_handle, dwIndex, lpName, cchName);

                if (ret == ERROR_SUCCESS && lpName != nullptr) {
                    log_info("iidx::asio", "stubbing '{}' with '{}'", lpName, ORIGINAL_ASIO_DEVICE_NAME);

                    strncpy(lpName, ORIGINAL_ASIO_DEVICE_NAME, cchName);
                }

                return ret;
            } else {
                return ERROR_NO_MORE_ITEMS;
            }
        }

        return RegEnumKeyA_orig(hKey, dwIndex, lpName, cchName);
    }

    /*
     * Dirty Workaround for IO Device
     * Game gets registry object via SetupDiOpenDevRegKey, then looks up "PortName" via RegQueryValueExA
     * We ignore the first and just cheat on the second.
     */
    static LONG WINAPI RegQueryValueExA_hook(HKEY hKey, LPCTSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType,
            LPBYTE lpData, LPDWORD lpcbData)
    {
        if (lpValueName != nullptr && lpData != nullptr && lpcbData != nullptr) {
            if (hKey == DEVICE_ASIO_REG_HANDLE && ASIO_DRIVER.has_value()) {
                log_info("iidx::asio", "RegQueryValueExA({}, \"{}\")", fmt::ptr((void *) hKey), lpValueName);

                if (_stricmp(lpValueName, "Description") == 0) {
                    memcpy(lpData, ASIO_DRIVER.value().c_str(), ASIO_DRIVER.value().size() + 1);

                    return ERROR_SUCCESS;
                } else {
                    hKey = real_asio_device_reg_handle;
                }
            }

            // check for port name lookup
            if (_stricmp(lpValueName, "PortName") == 0) {
                const char port[] = "COM2";
                memcpy(lpData, port, sizeof(port));

                return ERROR_SUCCESS;
            }
        }

        // fallback
        return RegQueryValueExA_orig(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
    }

    static LONG WINAPI RegCloseKey_hook(HKEY hKey) {
        if (hKey == PARENT_ASIO_REG_HANDLE || hKey == DEVICE_ASIO_REG_HANDLE) {
            return ERROR_SUCCESS;
        }

        return RegCloseKey_orig(hKey);
    }

    /*
     * Camera related stuff
     */
    static std::wstring CAMERA0_ID;
    static std::wstring CAMERA1_ID;

    static HRESULT WINAPI GetAllocatedString_hook(IMFActivate* This, REFGUID guidKey, LPWSTR *ppwszValue,
                                                  UINT32 *pcchLength)
    {
        // call the original
        HRESULT result = GetAllocatedString_orig(This, guidKey, ppwszValue, pcchLength);

        // log the cam name
        log_misc("iidx::cam", "obtained {}", ws2s(*ppwszValue));

        // try first camera
        wchar_t *pwc = nullptr;
        if (CAMERA0_ID.length() == 23) {
            pwc = wcsstr(*ppwszValue, CAMERA0_ID.c_str());
        }

        // try second camera if first wasn't found
        if (!pwc && CAMERA1_ID.length() == 23) {
            pwc = wcsstr(*ppwszValue, CAMERA1_ID.c_str());
        }

        // check if camera could be identified
        if (pwc) {

            // fake the USB IDs
            pwc[4] = L'2';
            pwc[5] = L'8';
            pwc[6] = L'8';
            pwc[7] = L'c';

            pwc[13] = L'0';
            pwc[14] = L'0';
            pwc[15] = L'0';
            pwc[16] = L'2';

            pwc[21] = L'0';
            pwc[22] = L'0';

            log_misc("iidx::cam", "replaced {}", ws2s(*ppwszValue));
        }

        // return original result
        return result;
    }

    static void hook_camera_vtable(IMFActivate *camera) {

        // hook allocated string method for camera identification
        memutils::VProtectGuard camera_guard(camera->lpVtbl);
        camera->lpVtbl->GetAllocatedString = GetAllocatedString_hook;
    }

    static void hook_camera(size_t no, std::wstring camera_id, std::string camera_instance) {

        // logic based on camera no
        if (no == 0) {

            // don't hook if camera 0 is already hooked
            if (CAMERA0_ID.length() > 0) {
                return;
            }

            // save the camera ID
            CAMERA0_ID = camera_id;

            // cfgmgr hook
            CFGMGR32_HOOK_SETTING camera_setting;
            camera_setting.device_instance = 0xDEADBEEF;
            camera_setting.parent_instance = ~camera_setting.device_instance;
            camera_setting.device_id = "USB\\VEN_1022&DEV_7908";
            camera_setting.device_node_id = "USB\\VID_288C&PID_0002&MI_00\\?&????????&?&????";
            if (camera_instance.length() == 17) {
                for (int i = 0; i < 17; i++) {
                    camera_setting.device_node_id[28 + i] = camera_instance[i];
                }
            }
            cfgmgr32hook_add(camera_setting);

            log_info("iidx::cam", "hooked camera 1 @ {}", ws2s(camera_id));
        }
        else if (no == 1) {

            // don't hook if camera 1 is already hooked
            if (CAMERA1_ID.length() > 0) {
                return;
            }

            // save the camera ID
            CAMERA1_ID = camera_id;

            // cfgmgr hook
            CFGMGR32_HOOK_SETTING camera_setting;
            camera_setting.device_instance = 0xBEEFDEAD;
            camera_setting.parent_instance = ~camera_setting.device_instance;
            camera_setting.device_id = "USB\\VEN_1022&DEV_7914";
            camera_setting.device_node_id = "USB\\VID_288C&PID_0002&MI_00\\?&????????&?&????";
            if (camera_instance.length() == 17) {
                for (int i = 0; i < 17; i++) {
                    camera_setting.device_node_id[28 + i] = camera_instance[i];
                }
            }
            cfgmgr32hook_add(camera_setting);

            log_info("iidx::cam", "hooked camera 2 @ {}", ws2s(camera_id));
        }
    }

    static HRESULT WINAPI MFEnumDeviceSources_hook(IMFAttributes *pAttributes, IMFActivate ***pppSourceActivate,
                                                   UINT32 *pcSourceActivate) {

        // call original function
        HRESULT result_orig = MFEnumDeviceSources_orig(pAttributes, pppSourceActivate, pcSourceActivate);

        // check for capture devices
        if (FAILED(result_orig) || !*pcSourceActivate) {
            return result_orig;
        }

        // iterate cameras
        size_t cam_hook_num = 0;
        for (size_t cam_num = 0; cam_num < *pcSourceActivate && cam_hook_num < 2; cam_num++) {

            // flip
            size_t cam_num_flipped = cam_num;
            if (FLIP_CAMS) {
                cam_num_flipped = *pcSourceActivate - cam_num - 1;
            }

            // get camera
            IMFActivate *camera = (*pppSourceActivate)[cam_num_flipped];

            // save original method for later use
            if (GetAllocatedString_orig == nullptr) {
                GetAllocatedString_orig = camera->lpVtbl->GetAllocatedString;
            }

            // hook allocated string method for camera identification
            hook_camera_vtable(camera);

            // get camera link
            LPWSTR camera_link_lpwstr;
            UINT32 camera_link_length;
            if (SUCCEEDED(GetAllocatedString_orig(
                    camera,
                    MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
                    &camera_link_lpwstr,
                    &camera_link_length))) {

                // cut name to make ID
                std::wstring camera_link_ws = std::wstring(camera_link_lpwstr);
                std::wstring camera_id = camera_link_ws.substr(8, 23);

                log_info("iidx::cam", "found video capture device: {}", ws2s(camera_link_ws));

                // only support cameras that start with \\?\usb
                if (!string_begins_with(camera_link_ws, L"\\\\?\\usb")) {
                    continue;
                }

                // get camera instance
                std::string camera_link = ws2s(camera_link_ws);
                std::string camera_instance = camera_link.substr(32, 17);

                // hook the camera
                hook_camera(cam_hook_num, camera_id, camera_instance);

                // increase camera hook number
                cam_hook_num++;

            } else {
                log_warning("iidx::cam", "failed to open camera {}", cam_num_flipped);
            }
        }

        // return result
        return result_orig;
    }

    static bool log_hook(void *user, const std::string &data, logger::Style style, std::string &out) {

        // get rid of the log spam
        if (data.find(" I:graphic: adapter mode ") != std::string::npos) {
            out.clear();
            return true;
        } else {
            return false;
        }
    }

    typedef void* (*aioIob2Bi2x_CreateWriteFirmContext_t)(unsigned int, int);
    static aioIob2Bi2x_CreateWriteFirmContext_t aioIob2Bi2x_CreateWriteFirmContext_orig = nullptr;

    static void* aioIob2Bi2x_CreateWriteFirmContext_hook(unsigned int a1, int a2) {
        if (aioIob2Bi2x_CreateWriteFirmContext_orig) {
            auto options = games::get_options(eamuse_get_game());
            if (options->at(launcher::Options::IIDXBIO2FW).value_bool()) {
                log_info("iidx", "CreateWriteFirmContext({}, {} -> 2)", a1, a2);
                return aioIob2Bi2x_CreateWriteFirmContext_orig(a1, 2);
            } else {
                log_info("iidx", "CreateWriteFirmContext({}, {})", a1, a2);
                return aioIob2Bi2x_CreateWriteFirmContext_orig(a1, a2);
            }
        }
        log_warning("iidx", "CreateWriteFirmContext == nullptr");
        return nullptr;
    }

    IIDXGame::IIDXGame() : Game("Beatmania IIDX") {
        logger::hook_add(log_hook, this);
    }

    IIDXGame::~IIDXGame() {
        logger::hook_remove(log_hook, this);
    }

    void IIDXGame::attach() {
        Game::attach();

        // IO boards
        auto options = games::get_options(eamuse_get_game());
        if (!options->at(launcher::Options::IIDXBIO2FW).value_bool()) {

            // reduce boot wait time
            hooks::sleep::init(1000, 1);

            // add old IO board
            SETUPAPI_SETTINGS settings1 {};
            settings1.class_guid[0] = 0xAE18AA60;
            settings1.class_guid[1] = 0x11D47F6A;
            settings1.class_guid[2] = 0x0100DD97;
            settings1.class_guid[3] = 0x59B92902;
            const char property1[] = "Cypress EZ-USB (2235 - EEPROM missing)";
            const char interface_detail1[] = "\\\\.\\Ezusb-0";
            memcpy(settings1.property_devicedesc, property1, sizeof(property1));
            memcpy(settings1.interface_detail, interface_detail1, sizeof(interface_detail1));
            setupapihook_init(avs::game::DLL_INSTANCE);
            setupapihook_add(settings1);

            // IIDX <25 with EZUSB input device
            devicehook_init();
            devicehook_add(new EZUSBHandle());

            // add new BIO2 I/O board
            SETUPAPI_SETTINGS settings2 {};
            settings2.class_guid[0] = 0x4D36E978;
            settings2.class_guid[1] = 0x11CEE325;
            settings2.class_guid[2] = 0x0008C1BF;
            settings2.class_guid[3] = 0x1803E12B;
            const char property2[] = "BIO2(VIDEO)";
            const char interface_detail2[] = "COM2";
            memcpy(settings2.property_devicedesc, property2, sizeof(property2));
            memcpy(settings2.interface_detail, interface_detail2, sizeof(interface_detail2));
            setupapihook_add(settings2);

            // IIDX 25-27 BIO2 BI2A input device
            devicehook_add(new IIDXFMSerialHandle());
        }

        // check for new I/O DLL
        auto aio = libutils::try_library("libaio.dll");
        if (aio != nullptr) {

            // check TDJ mode
            TDJ_MODE |= fileutils::text_read("D:\\001rom.txt") == "TDJ";

            // force TDJ mode
            if (TDJ_MODE) {

                // ensure game starts in the desired mode
                hooks::avs::set_rom("/d_drv/001rom.txt", "TDJ");

                // need to hook `avs2-core.dll` so AVS win32fs operations go through rom hook
                devicehook_init(avs::core::DLL_INSTANCE);

                // hook touch window
                wintouchemu::FORCE = true;
                wintouchemu::hook_title_ends("beatmania IIDX", "main", avs::game::DLL_INSTANCE);

                // prevent crash on TDJ mode without correct DLL
                if (!GetModuleHandle("nvcuda.dll")) {
                    DISABLE_CAMS = true;
                }
            }

            // insert BI2X hooks
            bi2x_hook_init();

            // add card readers
            devicehook_init(aio);
            devicehook_add(new acioemu::ACIOHandle(L"COM1"));

            // firmware upgrade hook
            if (options->at(launcher::Options::IIDXBIO2FW).value_bool()) {
                aioIob2Bi2x_CreateWriteFirmContext_orig = detour::iat(
                        "aioIob2Bi2x_CreateWriteFirmContext",
                        aioIob2Bi2x_CreateWriteFirmContext_hook);
                //devicehook_add(new MITMHandle(L"#vid_1ccf&pid_8050", "", true));
            } else {

                /*
                // add the BIO2 I/O board (with different firmware)
                SETUPAPI_SETTINGS settings {};
                settings.class_guid[0] = 0x0;
                settings.class_guid[1] = 0x0;
                settings.class_guid[2] = 0x0;
                settings.class_guid[3] = 0x0;
                const char property[] = "1CCF(8050)_000";
                const char property_hardwareid[] = "USB\\VID_1CCF&PID_8050&MI_00\\000";
                const char interface_detail[] = "COM3";
                memcpy(settings.property_devicedesc, property, sizeof(property));
                memcpy(settings.property_hardwareid, property_hardwareid, sizeof(property_hardwareid));
                memcpy(settings.interface_detail, interface_detail, sizeof(interface_detail));
                setupapihook_init(aio);
                setupapihook_add(settings);

                // IIDX 27 BIO2 BI2X input devce
                devicehook_add(new BI2XSerialHandle());
                */
            }
        }

        // ASIO device hook
        RegCloseKey_orig = detour::iat_try(
                "RegCloseKey", RegCloseKey_hook, avs::game::DLL_INSTANCE);
        RegEnumKeyA_orig = detour::iat_try(
                "RegEnumKeyA", RegEnumKeyA_hook, avs::game::DLL_INSTANCE);
        RegOpenKeyA_orig = detour::iat_try(
                "RegOpenKeyA", RegOpenKeyA_hook, avs::game::DLL_INSTANCE);
        RegOpenKeyExA_orig = detour::iat_try(
                "RegOpenKeyExA", RegOpenKeyExA_hook, avs::game::DLL_INSTANCE);

        // IO device workaround
        RegQueryValueExA_orig = detour::iat_try(
                "RegQueryValueExA", RegQueryValueExA_hook, avs::game::DLL_INSTANCE);

        // check if cam hook should be enabled
        if (!DISABLE_CAMS) {

            // camera media framework hook
            MFEnumDeviceSources_orig = detour::iat_try(
                    "MFEnumDeviceSources", MFEnumDeviceSources_hook, avs::game::DLL_INSTANCE);

            // camera settings
            SETUPAPI_SETTINGS settings3 {};
            settings3.class_guid[0] = 0x00000000;
            settings3.class_guid[1] = 0x00000000;
            settings3.class_guid[2] = 0x00000000;
            settings3.class_guid[3] = 0x00000000;
            const char property3[] = "USB Composite Device";
            memcpy(settings3.property_devicedesc, property3, sizeof(property3));
            settings3.property_address[0] = 1;
            settings3.property_address[1] = 7;
            setupapihook_add(settings3);
        }

        // init cfgmgr32 hooks
        cfgmgr32hook_init(avs::game::DLL_INSTANCE);
    }

    void IIDXGame::pre_attach() {
        Game::pre_attach();

        // environment variables must be set before the DLL is loaded as the VC++ runtime copies all
        // environment variables at startup
        if (DISABLE_CAMS) {
            SetEnvironmentVariable("CONNECT_CAMERA", "0");
        }
        if (SOUND_OUTPUT_DEVICE.has_value()) {
            SetEnvironmentVariable("SOUND_OUTPUT_DEVICE", SOUND_OUTPUT_DEVICE.value().c_str());
        }
    }

    void IIDXGame::detach() {
        Game::detach();

        devicehook_dispose();
    }

    uint32_t get_pad() {
        uint32_t pad = 0;

        // get buttons
        auto &buttons = get_buttons();

        // player 1 buttons
        if (GameAPI::Buttons::getState(RI_MGR, buttons.at(Buttons::P1_1)))
            pad |= 1 << 0x08;
        if (GameAPI::Buttons::getState(RI_MGR, buttons.at(Buttons::P1_2)))
            pad |= 1 << 0x09;
        if (GameAPI::Buttons::getState(RI_MGR, buttons.at(Buttons::P1_3)))
            pad |= 1 << 0x0A;
        if (GameAPI::Buttons::getState(RI_MGR, buttons.at(Buttons::P1_4)))
            pad |= 1 << 0x0B;
        if (GameAPI::Buttons::getState(RI_MGR, buttons.at(Buttons::P1_5)))
            pad |= 1 << 0x0C;
        if (GameAPI::Buttons::getState(RI_MGR, buttons.at(Buttons::P1_6)))
            pad |= 1 << 0x0D;
        if (GameAPI::Buttons::getState(RI_MGR, buttons.at(Buttons::P1_7)))
            pad |= 1 << 0x0E;

        // player 2 buttons
        if (GameAPI::Buttons::getState(RI_MGR, buttons.at(Buttons::P2_1)))
            pad |= 1 << 0x0F;
        if (GameAPI::Buttons::getState(RI_MGR, buttons.at(Buttons::P2_2)))
            pad |= 1 << 0x10;
        if (GameAPI::Buttons::getState(RI_MGR, buttons.at(Buttons::P2_3)))
            pad |= 1 << 0x11;
        if (GameAPI::Buttons::getState(RI_MGR, buttons.at(Buttons::P2_4)))
            pad |= 1 << 0x12;
        if (GameAPI::Buttons::getState(RI_MGR, buttons.at(Buttons::P2_5)))
            pad |= 1 << 0x13;
        if (GameAPI::Buttons::getState(RI_MGR, buttons.at(Buttons::P2_6)))
            pad |= 1 << 0x14;
        if (GameAPI::Buttons::getState(RI_MGR, buttons.at(Buttons::P2_7)))
            pad |= 1 << 0x15;

        // player 1 start
        if (GameAPI::Buttons::getState(RI_MGR, buttons.at(Buttons::P1_Start)))
            pad |= 1 << 0x18;

        // player 2 start
        if (GameAPI::Buttons::getState(RI_MGR, buttons.at(Buttons::P2_Start)))
            pad |= 1 << 0x19;

        // VEFX
        if (GameAPI::Buttons::getState(RI_MGR, buttons.at(Buttons::VEFX)))
            pad |= 1 << 0x1A;

        // EFFECT
        if (GameAPI::Buttons::getState(RI_MGR, buttons.at(Buttons::Effect)))
            pad |= 1 << 0x1B;

        // test
        if (GameAPI::Buttons::getState(RI_MGR, buttons.at(Buttons::Test)))
            pad |= 1 << 0x1C;

        // service
        if (GameAPI::Buttons::getState(RI_MGR, buttons.at(Buttons::Service)))
            pad |= 1 << 0x1D;

        return ~(pad & 0xFFFFFF00);
    }

    void write_lamp(uint16_t lamp) {

        // mapping
        static const size_t mapping[] = {
                Lights::P1_1,
                Lights::P1_2,
                Lights::P1_3,
                Lights::P1_4,
                Lights::P1_5,
                Lights::P1_6,
                Lights::P1_7,
                Lights::P2_1,
                Lights::P2_2,
                Lights::P2_3,
                Lights::P2_4,
                Lights::P2_5,
                Lights::P2_6,
                Lights::P2_7,
        };

        // get lights
        auto &lights = get_lights();

        // bit scan
        for (int i = 0; i < 14; i++) {
            float value = (lamp & (1 << i)) ? 1.f : 0.f;
            GameAPI::Lights::writeLight(RI_MGR, lights.at(mapping[i]), value);
        }
    }

    void write_led(uint8_t led) {

        // mapping
        static const size_t mapping[] = {
                Lights::P1_Start,
                Lights::P2_Start,
                Lights::VEFX,
                Lights::Effect,
        };

        // get lights
        auto &lights = get_lights();

        // bit scan
        for (int i = 0; i < 4; i++) {
            auto value = (led & (1 << i)) ? 1.f : 0.f;
            GameAPI::Lights::writeLight(RI_MGR, lights.at(mapping[i]), value);
        }
    }

    void write_top_lamp(uint8_t top_lamp) {

        // mapping
        static const size_t mapping[] = {
                Lights::SpotLight1,
                Lights::SpotLight2,
                Lights::SpotLight3,
                Lights::SpotLight4,
                Lights::SpotLight5,
                Lights::SpotLight6,
                Lights::SpotLight7,
                Lights::SpotLight8,
        };

        // get lights
        auto &lights = get_lights();

        // bit scan
        for (int i = 0; i < 8; i++) {
            auto value = (top_lamp & (1 << i)) ? 1.f : 0.f;
            GameAPI::Lights::writeLight(RI_MGR, lights.at(mapping[i]), value);
        }
    }

    void write_top_neon(uint8_t top_neon) {

        // get lights
        auto &lights = get_lights();

        // write value
        auto value = top_neon > 0 ? 1.f : 0.f;
        GameAPI::Lights::writeLight(RI_MGR, lights.at(Lights::NeonLamp), value);
    }

    unsigned char get_tt(int player, bool slow) {

        // check change value for high/low precision
        uint16_t change = slow ? (uint16_t) 1 : (uint16_t) 4;

        // check player number
        if (player > 1)
            return 0;

        // get buttons
        auto &buttons = get_buttons();
        bool ttp = GameAPI::Buttons::getState(RI_MGR, buttons.at(
                player != 0 ? Buttons::P2_TTPlus : Buttons::P1_TTPlus));
        bool ttm = GameAPI::Buttons::getState(RI_MGR, buttons.at(
                player != 0 ? Buttons::P2_TTMinus : Buttons::P1_TTMinus));
        bool ttpm = GameAPI::Buttons::getState(RI_MGR, buttons.at(
                player != 0 ? Buttons::P2_TTPlusMinus : Buttons::P1_TTPlusMinus));

        // TT+
        if (ttp)
            IIDXIO_TT_STATE[player] += change;

        // TT-
        if (ttm)
            IIDXIO_TT_STATE[player] -= change;

        // TT+/-
        if (ttpm) {
            if (IIDXIO_TT_DIRECTION[player] != 0u) {
                if (!IIDXIO_TT_PRESSED[player])
                    IIDXIO_TT_DIRECTION[player] = 0;
                IIDXIO_TT_STATE[player] += change;
            } else {
                if (!IIDXIO_TT_PRESSED[player])
                    IIDXIO_TT_DIRECTION[player] = 1;
                IIDXIO_TT_STATE[player] -= change;
            }
            IIDXIO_TT_PRESSED[player] = true;
        } else
            IIDXIO_TT_PRESSED[player] = false;

        // raw input
        auto &analogs = get_analogs();
        auto &analog = analogs[player != 0 ? Analogs::TT_P2 : Analogs::TT_P1];
        auto ret_value = IIDXIO_TT_STATE[player];
        if (analog.isSet()) {
            ret_value = IIDXIO_TT_STATE[player];
            ret_value += (uint16_t) (GameAPI::Analogs::getState(RI_MGR, analog) * 1023.999f);
        }

        // return higher 8 bit
        return (uint8_t) (ret_value >> 2);
    }

    unsigned char get_slider(uint8_t slider) {

        // check slide
        if (slider > 4)
            return 0;

        // get analog
        auto &analogs = get_analogs();
        Analog *analog = nullptr;
        switch (slider) {
            case 0:
                analog = &analogs.at(Analogs::VEFX);
                break;
            case 1:
                analog = &analogs.at(Analogs::LowEQ);
                break;
            case 2:
                analog = &analogs.at(Analogs::HiEQ);
                break;
            case 3:
                analog = &analogs.at(Analogs::Filter);
                break;
            case 4:
                analog = &analogs.at(Analogs::PlayVolume);
                break;
            default:
                break;
        }

        // if not set return max value
        if (!analog || !analog->isSet()) {
            return 0xF;
        }

        // return slide
        return (unsigned char) (GameAPI::Analogs::getState(RI_MGR, *analog) * 15.999f);
    }

    const char* get_16seg() {
        return IIDXIO_LED_TICKER;
    }
}

#include "kfcontrol.h"

#include <windows.h>
#include <commdlg.h>

#include "cfg/configurator.h"
#include "misc/eamuse.h"
#include "games/io.h"
#include "games/sdvx/io.h"
#include "games/shared/lcdhandle.h"
#include "launcher/launcher.h"
#include "launcher/shutdown.h"
#include "overlay/imgui/extensions.h"
#include "api/controller.h"
#include "rawinput/rawinput.h"
#include "external/rapidjson/document.h"
#include "external/rapidjson/prettywriter.h"
#include "util/netutils.h"
#include "util/fileutils.h"
#include "util/time.h"
#include "util/utils.h"

using namespace GameAPI;
using namespace rapidjson;

namespace overlay::windows {

    KFControl::KFControl(SpiceOverlay *overlay) : Window(overlay) {
        this->title = "KFControl";
        this->init_size = ImVec2(400, 316);
        this->size_min = ImVec2(100, 200);
        this->init_pos = ImVec2(0, 0);
        this->config_load();
        if (cfg::CONFIGURATOR_STANDALONE && cfg::CONFIGURATOR_TYPE == cfg::ConfigType::KFControl) {
            this->active = true;
            this->flags |= ImGuiWindowFlags_NoResize;
            this->flags |= ImGuiWindowFlags_NoMove;
            this->flags |= ImGuiWindowFlags_NoTitleBar;
            this->flags |= ImGuiWindowFlags_NoCollapse;
            this->flags |= ImGuiWindowFlags_NoDecoration;
            eamuse_set_game("Sound Voltex");
            for (auto &button : *games::get_buttons("Sound Voltex")) {
                button.clearBindings();
            }
            for (auto &analog : *games::get_analogs("Sound Voltex")) {
                analog.clearBindings();
            }
            this->worker_start();
        }
    }

    KFControl::~KFControl() {
        worker_running = false;
        worker->join();
    }

    void KFControl::config_save() {

        // create document
        Document doc;
        doc.SetObject();
        auto &alloc = doc.GetAllocator();

        // save settings
        doc.AddMember("poll_delay", poll_delay, alloc);
        doc.AddMember("vol_deadzone", vol_deadzone, alloc);
        doc.AddMember("vol_timeout", vol_timeout, alloc);
        doc.AddMember("vol_mouse", vol_mouse, alloc);
        doc.AddMember("vol_mouse_sensitivity", vol_mouse_sensitivity, alloc);
        doc.AddMember("start_click", start_click, alloc);
        doc.AddMember("kp_profiles", kp_profiles, alloc);
        doc.AddMember("vol_sound", vol_sound, alloc);
        doc.AddMember("vol_headphone", vol_headphone, alloc);
        doc.AddMember("vol_external", vol_external, alloc);
        doc.AddMember("vol_woofer", vol_woofer, alloc);
        doc.AddMember("vol_mute", vol_mute, alloc);
        doc.AddMember("icca_timeout", icca_timeout, alloc);
        doc.AddMember("icca_file", StringRef(icca_file), alloc);
        doc.AddMember("light_wing_left_up_r", light_wing_left_up.x, alloc);
        doc.AddMember("light_wing_left_up_g", light_wing_left_up.y, alloc);
        doc.AddMember("light_wing_left_up_b", light_wing_left_up.z, alloc);
        doc.AddMember("light_wing_left_low_r", light_wing_left_low.x, alloc);
        doc.AddMember("light_wing_left_low_g", light_wing_left_low.y, alloc);
        doc.AddMember("light_wing_left_low_b", light_wing_left_low.z, alloc);
        doc.AddMember("light_wing_right_up_r", light_wing_right_up.x, alloc);
        doc.AddMember("light_wing_right_up_g", light_wing_right_up.y, alloc);
        doc.AddMember("light_wing_right_up_b", light_wing_right_up.z, alloc);
        doc.AddMember("light_wing_right_low_r", light_wing_right_low.x, alloc);
        doc.AddMember("light_wing_right_low_g", light_wing_right_low.y, alloc);
        doc.AddMember("light_wing_right_low_b", light_wing_right_low.z, alloc);
        doc.AddMember("light_woofer_r", light_woofer.x, alloc);
        doc.AddMember("light_woofer_g", light_woofer.y, alloc);
        doc.AddMember("light_woofer_b", light_woofer.z, alloc);
        doc.AddMember("light_controller_r", light_controller.x, alloc);
        doc.AddMember("light_controller_g", light_controller.y, alloc);
        doc.AddMember("light_controller_b", light_controller.z, alloc);
        doc.AddMember("light_generator_r", light_generator.x, alloc);
        doc.AddMember("light_generator_g", light_generator.y, alloc);
        doc.AddMember("light_generator_b", light_generator.z, alloc);
        doc.AddMember("lcd_bri", games::shared::LCD_BRI, alloc);
        doc.AddMember("lcd_con", games::shared::LCD_CON, alloc);
        doc.AddMember("lcd_bl", games::shared::LCD_BL, alloc);
        doc.AddMember("lcd_red", games::shared::LCD_RED, alloc);
        doc.AddMember("lcd_green", games::shared::LCD_GREEN, alloc);
        doc.AddMember("lcd_blue", games::shared::LCD_BLUE, alloc);
        doc.AddMember("coin_block", eamuse_coin_get_block(), alloc);
        doc.AddMember("light_buttons", light_buttons, alloc);
        doc.AddMember("light_hue_preview", light_hue_preview, alloc);
        doc.AddMember("light_hue_func", (int) light_hue_func, alloc);
        doc.AddMember("light_hue_disable", light_hue_disable, alloc);
        doc.AddMember("light_wing_left_up_hue_amp", light_wing_left_up_hue_amp, alloc);
        doc.AddMember("light_wing_left_up_hue_per", light_wing_left_up_hue_per, alloc);
        doc.AddMember("light_wing_left_low_hue_amp", light_wing_left_low_hue_amp, alloc);
        doc.AddMember("light_wing_left_low_hue_per", light_wing_left_low_hue_per, alloc);
        doc.AddMember("light_wing_right_up_hue_amp", light_wing_right_up_hue_amp, alloc);
        doc.AddMember("light_wing_right_up_hue_per", light_wing_right_up_hue_per, alloc);
        doc.AddMember("light_wing_right_low_hue_amp", light_wing_right_low_hue_amp, alloc);
        doc.AddMember("light_wing_right_low_hue_per", light_wing_right_low_hue_per, alloc);
        doc.AddMember("light_woofer_hue_amp", light_woofer_hue_amp, alloc);
        doc.AddMember("light_woofer_hue_per", light_woofer_hue_per, alloc);
        doc.AddMember("light_controller_hue_amp", light_controller_hue_amp, alloc);
        doc.AddMember("light_controller_hue_per", light_controller_hue_per, alloc);
        doc.AddMember("light_generator_hue_amp", light_generator_hue_amp, alloc);
        doc.AddMember("light_generator_hue_per", light_generator_hue_per, alloc);
        doc.AddMember("scan_service", scan_service, alloc);
        doc.AddMember("scan_test", scan_test, alloc);
        doc.AddMember("scan_coin_mech", scan_coin_mech, alloc);
        doc.AddMember("scan_bt_a", scan_bt_a, alloc);
        doc.AddMember("scan_bt_b", scan_bt_b, alloc);
        doc.AddMember("scan_bt_c", scan_bt_c, alloc);
        doc.AddMember("scan_bt_d", scan_bt_d, alloc);
        doc.AddMember("scan_fx_l", scan_fx_l, alloc);
        doc.AddMember("scan_fx_r", scan_fx_r, alloc);
        doc.AddMember("scan_start", scan_start, alloc);
        doc.AddMember("scan_headphone", scan_headphone, alloc);
        doc.AddMember("scan_vol_l_left", scan_vol_l_left, alloc);
        doc.AddMember("scan_vol_l_right", scan_vol_l_right, alloc);
        doc.AddMember("scan_vol_r_left", scan_vol_r_left, alloc);
        doc.AddMember("scan_vol_r_right", scan_vol_r_right, alloc);
        doc.AddMember("scan_icca", scan_icca, alloc);
        doc.AddMember("scan_coin", scan_coin, alloc);
        doc.AddMember("scan_kp_0", scan_kp_0, alloc);
        doc.AddMember("scan_kp_1", scan_kp_1, alloc);
        doc.AddMember("scan_kp_2", scan_kp_2, alloc);
        doc.AddMember("scan_kp_3", scan_kp_3, alloc);
        doc.AddMember("scan_kp_4", scan_kp_4, alloc);
        doc.AddMember("scan_kp_5", scan_kp_5, alloc);
        doc.AddMember("scan_kp_6", scan_kp_6, alloc);
        doc.AddMember("scan_kp_7", scan_kp_7, alloc);
        doc.AddMember("scan_kp_8", scan_kp_8, alloc);
        doc.AddMember("scan_kp_9", scan_kp_9, alloc);
        doc.AddMember("scan_kp_00", scan_kp_00, alloc);
        doc.AddMember("scan_kp_decimal", scan_kp_decimal, alloc);

        // convert to JSON
        StringBuffer buf;
        PrettyWriter<StringBuffer> writer(buf);
        writer.SetIndent(' ', 2);
        doc.Accept(writer);
        buf.Put('\r');
        buf.Put('\n');

        // write to file
        log_info("kfcontrol", "saving config to: {}", config_path);
        fileutils::text_write(config_path, buf.GetString());
    }

    template<class T>
    static void load_val(T &node, const std::string &name, float &value) {
        auto it = node.FindMember(name.c_str());
        if (it != node.MemberEnd() && it->value.IsFloat()) {
            value = it->value.GetFloat();
        }
    }

    template<class T>
    static void load_val(T &node, const std::string &name, int &value) {
        auto it = node.FindMember(name.c_str());
        if (it != node.MemberEnd() && it->value.IsInt()) {
            value = it->value.GetInt();
        }
    }

    static std::string key_format(int vsc) {
        if (vsc == 0) return "Disabled";
        auto vk = MapVirtualKey(vsc, MAPVK_VSC_TO_VK_EX);
        auto ch = MapVirtualKey(vk, MAPVK_VK_TO_CHAR);
        char keyName[128];
        if (GetKeyNameText((LONG) (vsc << 16), keyName, sizeof(keyName))) {
            return fmt::format("Scan: %d - VK: {} - Key: {}", vk, keyName);
        } else if (ch) {
            return fmt::format("Scan: %d - VK: {} - Char: '{}'", vk, ch);
        } else {
            return fmt::format("Scan: %d - VK: {}", vk);
        }
    }

    void KFControl::config_load() {
        this->config_path = std::string(getenv("APPDATA"));
        this->config_path += fmt::format("\\spicetools_kfcontrol_{}.json", config_profile);

        // check if config missing
        if (!fileutils::file_exists(config_path)) {
            return;
        }

        // read config file
        std::string config = fileutils::text_read(config_path);
        if (config.empty()) {
            return;
        }

        // parse document
        log_info("kfcontrol", "loading config from: {}", config_path);
        Document doc;
        if (doc.Parse(config.c_str()).HasParseError()) {
            log_warning("kfcontrol", "unable to parse config: {}", doc.GetParseError());
        }

        // verify root is a dict
        if (!doc.IsObject()) {
            log_warning("kfcontrol", "config not a dict");
            return;
        }

        // read settings
        load_val(doc, "poll_delay", poll_delay);
        auto vol_deadzone_it = doc.FindMember("vol_deadzone");
        if (vol_deadzone_it != doc.MemberEnd() && vol_deadzone_it->value.IsFloat()) {
            vol_deadzone = vol_deadzone_it->value.GetFloat();
        }
        auto vol_timeout_it = doc.FindMember("vol_timeout");
        if (vol_timeout_it != doc.MemberEnd() && vol_deadzone_it->value.IsUint64()) {
            vol_timeout = vol_timeout_it->value.GetUint64();
        }
        auto vol_mouse_it = doc.FindMember("vol_mouse");
        if (vol_mouse_it != doc.MemberEnd() && vol_mouse_it->value.IsBool()) {
            vol_mouse = vol_mouse_it->value.GetBool();
        }
        load_val(doc, "vol_mouse_sensitivity", vol_mouse_sensitivity);
        auto start_click_it = doc.FindMember("start_click");
        if (start_click_it != doc.MemberEnd() && start_click_it->value.IsBool()) {
            start_click = start_click_it->value.GetBool();
        }
        auto kp_profiles_it = doc.FindMember("kp_profiles");
        if (kp_profiles_it != doc.MemberEnd() && kp_profiles_it->value.IsBool()) {
            kp_profiles = kp_profiles_it->value.GetBool();
        }
        auto vol_sound_it = doc.FindMember("vol_sound");
        if (vol_sound_it != doc.MemberEnd() && vol_sound_it->value.IsFloat()) {
            vol_sound = vol_sound_it->value.GetFloat();
        }
        auto vol_headphone_it = doc.FindMember("vol_headphone");
        if (vol_headphone_it != doc.MemberEnd() && vol_headphone_it->value.IsFloat()) {
            vol_headphone = vol_headphone_it->value.GetFloat();
        }
        auto vol_external_it = doc.FindMember("vol_external");
        if (vol_external_it != doc.MemberEnd() && vol_external_it->value.IsFloat()) {
            vol_external = vol_external_it->value.GetFloat();
        }
        auto vol_woofer_it = doc.FindMember("vol_woofer");
        if (vol_woofer_it != doc.MemberEnd() && vol_woofer_it->value.IsFloat()) {
            vol_woofer = vol_woofer_it->value.GetFloat();
        }
        auto vol_mute_it = doc.FindMember("vol_mute");
        if (vol_mute_it != doc.MemberEnd() && vol_mute_it->value.IsBool()) {
            vol_mute = vol_mute_it->value.GetBool();
        }
        auto icca_timeout_it = doc.FindMember("icca_timeout");
        if (icca_timeout_it != doc.MemberEnd() && icca_timeout_it->value.IsUint64()) {
            icca_timeout = icca_timeout_it->value.GetUint64();
        }
        auto icca_file_it = doc.FindMember("icca_file");
        if (icca_file_it != doc.MemberEnd() && icca_file_it->value.IsString()) {
            if (icca_file_it->value.GetStringLength() < sizeof(icca_file)) {
                strcpy(icca_file, icca_file_it->value.GetString());
            }
        }
        auto coin_timeout_it = doc.FindMember("coin_timeout");
        if (coin_timeout_it != doc.MemberEnd() && coin_timeout_it->value.IsUint64()) {
            coin_timeout = coin_timeout_it->value.GetUint64();
        }
        auto light_wing_left_up_r_it = doc.FindMember("light_wing_left_up_r");
        if (light_wing_left_up_r_it != doc.MemberEnd() && light_wing_left_up_r_it->value.IsFloat()) {
            light_wing_left_up.x = light_wing_left_up_r_it->value.GetFloat();
        }
        auto light_wing_left_up_g_it = doc.FindMember("light_wing_left_up_g");
        if (light_wing_left_up_g_it != doc.MemberEnd() && light_wing_left_up_g_it->value.IsFloat()) {
            light_wing_left_up.y = light_wing_left_up_g_it->value.GetFloat();
        }
        auto light_wing_left_up_b_it = doc.FindMember("light_wing_left_up_b");
        if (light_wing_left_up_b_it != doc.MemberEnd() && light_wing_left_up_b_it->value.IsFloat()) {
            light_wing_left_up.z = light_wing_left_up_b_it->value.GetFloat();
        }
        auto light_wing_left_low_r_it = doc.FindMember("light_wing_left_low_r");
        if (light_wing_left_low_r_it != doc.MemberEnd() && light_wing_left_low_r_it->value.IsFloat()) {
            light_wing_left_low.x = light_wing_left_low_r_it->value.GetFloat();
        }
        auto light_wing_left_low_g_it = doc.FindMember("light_wing_left_low_g");
        if (light_wing_left_low_g_it != doc.MemberEnd() && light_wing_left_low_g_it->value.IsFloat()) {
            light_wing_left_low.y = light_wing_left_low_g_it->value.GetFloat();
        }
        auto light_wing_left_low_b_it = doc.FindMember("light_wing_left_low_b");
        if (light_wing_left_low_b_it != doc.MemberEnd() && light_wing_left_low_b_it->value.IsFloat()) {
            light_wing_left_low.z = light_wing_left_low_b_it->value.GetFloat();
        }
        auto light_wing_right_up_r_it = doc.FindMember("light_wing_right_up_r");
        if (light_wing_right_up_r_it != doc.MemberEnd() && light_wing_right_up_r_it->value.IsFloat()) {
            light_wing_right_up.x = light_wing_right_up_r_it->value.GetFloat();
        }
        auto light_wing_right_up_g_it = doc.FindMember("light_wing_right_up_g");
        if (light_wing_right_up_g_it != doc.MemberEnd() && light_wing_right_up_g_it->value.IsFloat()) {
            light_wing_right_up.y = light_wing_right_up_g_it->value.GetFloat();
        }
        auto light_wing_right_up_b_it = doc.FindMember("light_wing_right_up_b");
        if (light_wing_right_up_b_it != doc.MemberEnd() && light_wing_right_up_b_it->value.IsFloat()) {
            light_wing_right_up.z = light_wing_right_up_b_it->value.GetFloat();
        }
        auto light_wing_right_low_r_it = doc.FindMember("light_wing_right_low_r");
        if (light_wing_right_low_r_it != doc.MemberEnd() && light_wing_right_low_r_it->value.IsFloat()) {
            light_wing_right_low.x = light_wing_right_low_r_it->value.GetFloat();
        }
        auto light_wing_right_low_g_it = doc.FindMember("light_wing_right_low_g");
        if (light_wing_right_low_g_it != doc.MemberEnd() && light_wing_right_low_g_it->value.IsFloat()) {
            light_wing_right_low.y = light_wing_right_low_g_it->value.GetFloat();
        }
        auto light_wing_right_low_b_it = doc.FindMember("light_wing_right_low_b");
        if (light_wing_right_low_b_it != doc.MemberEnd() && light_wing_right_low_b_it->value.IsFloat()) {
            light_wing_right_low.z = light_wing_right_low_b_it->value.GetFloat();
        }
        auto light_woofer_r_it = doc.FindMember("light_woofer_r");
        if (light_woofer_r_it != doc.MemberEnd() && light_woofer_r_it->value.IsFloat()) {
            light_woofer.x = light_woofer_r_it->value.GetFloat();
        }
        auto light_woofer_g_it = doc.FindMember("light_woofer_g");
        if (light_woofer_g_it != doc.MemberEnd() && light_woofer_g_it->value.IsFloat()) {
            light_woofer.y = light_woofer_g_it->value.GetFloat();
        }
        auto light_woofer_b_it = doc.FindMember("light_woofer_b");
        if (light_woofer_b_it != doc.MemberEnd() && light_woofer_b_it->value.IsFloat()) {
            light_woofer.z = light_woofer_b_it->value.GetFloat();
        }
        auto light_controller_r_it = doc.FindMember("light_controller_r");
        if (light_controller_r_it != doc.MemberEnd() && light_controller_r_it->value.IsFloat()) {
            light_controller.x = light_controller_r_it->value.GetFloat();
        }
        auto light_controller_g_it = doc.FindMember("light_controller_g");
        if (light_controller_g_it != doc.MemberEnd() && light_controller_g_it->value.IsFloat()) {
            light_controller.y = light_controller_g_it->value.GetFloat();
        }
        auto light_controller_b_it = doc.FindMember("light_controller_b");
        if (light_controller_b_it != doc.MemberEnd() && light_controller_b_it->value.IsFloat()) {
            light_controller.z = light_controller_b_it->value.GetFloat();
        }
        auto light_generator_r_it = doc.FindMember("light_generator_r");
        if (light_generator_r_it != doc.MemberEnd() && light_generator_r_it->value.IsFloat()) {
            light_generator.x = light_generator_r_it->value.GetFloat();
        }
        auto light_generator_g_it = doc.FindMember("light_generator_g");
        if (light_generator_g_it != doc.MemberEnd() && light_generator_g_it->value.IsFloat()) {
            light_generator.y = light_generator_g_it->value.GetFloat();
        }
        auto light_generator_b_it = doc.FindMember("light_generator_b");
        if (light_generator_b_it != doc.MemberEnd() && light_generator_b_it->value.IsFloat()) {
            light_generator.z = light_generator_b_it->value.GetFloat();
        }
        auto lcd_bri_it = doc.FindMember("lcd_bri");
        if (lcd_bri_it != doc.MemberEnd() && lcd_bri_it->value.IsUint()) {
            games::shared::LCD_BRI = lcd_bri_it->value.GetUint();
        }
        auto lcd_con_it = doc.FindMember("lcd_con");
        if (lcd_con_it != doc.MemberEnd() && lcd_con_it->value.IsUint()) {
            games::shared::LCD_CON = lcd_con_it->value.GetUint();
        }
        auto lcd_bl_it = doc.FindMember("lcd_bl");
        if (lcd_bl_it != doc.MemberEnd() && lcd_bl_it->value.IsUint()) {
            games::shared::LCD_BL = lcd_bl_it->value.GetUint();
        }
        auto lcd_red_it = doc.FindMember("lcd_red");
        if (lcd_red_it != doc.MemberEnd() && lcd_red_it->value.IsUint()) {
            games::shared::LCD_RED = lcd_red_it->value.GetUint();
        }
        auto lcd_green_it = doc.FindMember("lcd_green");
        if (lcd_green_it != doc.MemberEnd() && lcd_green_it->value.IsUint()) {
            games::shared::LCD_GREEN = lcd_green_it->value.GetUint();
        }
        auto lcd_blue_it = doc.FindMember("lcd_blue");
        if (lcd_blue_it != doc.MemberEnd() && lcd_blue_it->value.IsUint()) {
            games::shared::LCD_BLUE = lcd_blue_it->value.GetUint();
        }
        auto coin_block_it = doc.FindMember("coin_block");
        if (coin_block_it != doc.MemberEnd() && coin_block_it->value.IsBool()) {
            eamuse_coin_set_block(coin_block_it->value.GetBool());
        }
        auto light_buttons_it = doc.FindMember("light_buttons");
        if (light_buttons_it != doc.MemberEnd() && light_buttons_it->value.IsBool()) {
            light_buttons = light_buttons_it->value.GetBool();
        }
        auto light_hue_preview_it = doc.FindMember("light_hue_preview");
        if (light_hue_preview_it != doc.MemberEnd() && light_hue_preview_it->value.IsBool()) {
            light_hue_preview = light_hue_preview_it->value.GetBool();
        }
        auto light_hue_disable_it = doc.FindMember("light_hue_disable");
        if (light_hue_disable_it != doc.MemberEnd() && light_hue_disable_it->value.IsBool()) {
            light_hue_disable = light_hue_disable_it->value.GetBool();
        }
        auto light_hue_func_it = doc.FindMember("light_hue_func");
        if (light_hue_func_it != doc.MemberEnd() && light_hue_func_it->value.IsUint()) {
            auto val = light_hue_func_it->value.GetUint();
            if (val < (int) HueFunc::Count) {
                light_hue_func = (HueFunc) val;
            }
        }
        load_val(doc, "light_wing_left_up_hue_amp", light_wing_left_up_hue_amp);
        load_val(doc, "light_wing_left_up_hue_per", light_wing_left_up_hue_per);
        load_val(doc, "light_wing_left_low_hue_amp", light_wing_left_low_hue_amp);
        load_val(doc, "light_wing_left_low_hue_per", light_wing_left_low_hue_per);
        load_val(doc, "light_wing_right_up_hue_amp", light_wing_right_up_hue_amp);
        load_val(doc, "light_wing_right_up_hue_per", light_wing_right_up_hue_per);
        load_val(doc, "light_wing_right_low_hue_amp", light_wing_right_low_hue_amp);
        load_val(doc, "light_wing_right_low_hue_per", light_wing_right_low_hue_per);
        load_val(doc, "light_woofer_hue_amp", light_woofer_hue_amp);
        load_val(doc, "light_woofer_hue_per", light_woofer_hue_per);
        load_val(doc, "light_controller_hue_amp", light_controller_hue_amp);
        load_val(doc, "light_controller_hue_per", light_controller_hue_per);
        load_val(doc, "light_generator_hue_amp", light_generator_hue_amp);
        load_val(doc, "light_generator_hue_per", light_generator_hue_per);
        load_val(doc, "scan_service", scan_service);
        load_val(doc, "scan_test", scan_test);
        load_val(doc, "scan_coin_mech", scan_coin_mech);
        load_val(doc, "scan_bt_a", scan_bt_a);
        load_val(doc, "scan_bt_b", scan_bt_b);
        load_val(doc, "scan_bt_c", scan_bt_c);
        load_val(doc, "scan_bt_d", scan_bt_d);
        load_val(doc, "scan_fx_l", scan_fx_l);
        load_val(doc, "scan_fx_r", scan_fx_r);
        load_val(doc, "scan_start", scan_start);
        load_val(doc, "scan_headphone", scan_headphone);
        load_val(doc, "scan_vol_l_left", scan_vol_l_left);
        load_val(doc, "scan_vol_l_right", scan_vol_l_right);
        load_val(doc, "scan_vol_r_left", scan_vol_r_left);
        load_val(doc, "scan_vol_r_right", scan_vol_r_right);
        load_val(doc, "scan_icca", scan_icca);
        load_val(doc, "scan_coin", scan_coin);
        load_val(doc, "scan_kp_0", scan_kp_0);
        load_val(doc, "scan_kp_1", scan_kp_1);
        load_val(doc, "scan_kp_2", scan_kp_2);
        load_val(doc, "scan_kp_3", scan_kp_3);
        load_val(doc, "scan_kp_4", scan_kp_4);
        load_val(doc, "scan_kp_5", scan_kp_5);
        load_val(doc, "scan_kp_6", scan_kp_6);
        load_val(doc, "scan_kp_7", scan_kp_7);
        load_val(doc, "scan_kp_8", scan_kp_8);
        load_val(doc, "scan_kp_9", scan_kp_9);
        load_val(doc, "scan_kp_00", scan_kp_00);
        load_val(doc, "scan_kp_decimal", scan_kp_decimal);
    }

    ImVec4 KFControl::hsv_transform(ImVec4 col, float hue, float sat, float val) {
        ImVec4 hsv;
        ImGui::ColorConvertRGBtoHSV(col.x, col.y, col.z, hsv.x, hsv.y, hsv.z);
        hsv.x += hue;
        if (hsv.x < 0.f) {
            hsv.x = 1.f + hsv.x;
        }
        if (hsv.x > 1.f) {
            hsv.x = fmodf(hsv.x, 1.f);
        }
        hsv.y *= sat;
        hsv.z *= val;
        ImGui::ColorConvertHSVtoRGB(hsv.x, hsv.y, hsv.z, col.x, col.y, col.z);
        return col;
    }

    ImVec4 KFControl::hue_shift(ImVec4 col, float amp, float per, uint64_t ms) {
        if (light_hue_disable) return col;
        if (fabs(amp) < 0.001f) return col;
        if (per < 0.001f) per = 0.001f;
        uint64_t per_ms = per * 1000;
        auto per_cur = (ms % per_ms) / (float) per_ms;
        auto amp_cur = 0.f;
        switch (light_hue_func) {
            default:
            case HueFunc::Sine:
                amp_cur = sinf(per_cur * (M_PI * 2)) * amp;
                break;
            case HueFunc::Cosine:
                amp_cur = cosf(per_cur * (M_PI * 2)) * amp;
                break;
            case HueFunc::Absolute:
                if (per_cur < 0.5f)
                    amp_cur = per_cur * 2 * amp - amp * 0.5f;
                else
                    amp_cur = (1.f - (per_cur - 0.5f) * 2) * amp - amp * 0.5f;
                break;
            case HueFunc::Linear:
                amp_cur = per_cur * amp - amp * 0.5f;
                break;
        }
        return hsv_transform(col, amp_cur);
    }

    void KFControl::build_content() {
        auto time_cur = get_system_milliseconds();

        // if standalone then fullscreen window
        if (cfg::CONFIGURATOR_STANDALONE) {
            ImGui::SetWindowPos(ImVec2(0, 0));
            ImGui::SetWindowSize(ImGui::GetIO().DisplaySize);
        }

        // get IO pointers
        auto buttons = games::get_buttons("Sound Voltex");
        auto analogs = games::get_analogs("Sound Voltex");
        //auto lights = games::get_lights("Sound Voltex");

        // get button states
        auto service_state = Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::Service));
        auto test_state = Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::Test));
        auto coin_mech_state = Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::CoinMech));
        auto bt_a_state = Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::BT_A));
        auto bt_b_state = Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::BT_B));
        auto bt_c_state = Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::BT_C));
        auto bt_d_state = Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::BT_D));
        auto fx_l_state = Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::FX_L));
        auto fx_r_state = Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::FX_R));
        auto start_state = Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::Start));
        auto headphone_state = Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::Headphone));

        // get analog states
        auto vol_l_state = Analogs::getState(RI_MGR, analogs->at(games::sdvx::Analogs::VOL_L));
        auto vol_r_state = Analogs::getState(RI_MGR, analogs->at(games::sdvx::Analogs::VOL_R));

        // tab bar
        if (!ImGui::BeginTabBar("KFControl")) {
            return;
        }

        // display API state
        if (ImGui::BeginTabItem("API")) {
            if (API_CONTROLLER == nullptr) {
                ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "Warning: No API server running!");
                ImGui::HelpMarker("Please specify \"-api [PORT]\" / \"-apipass [PASS] \" command line arguments.");
                ImGui::Separator();
            } else {
                std::vector<api::ClientState> client_states;
                API_CONTROLLER->obtain_client_states(&client_states);

                // show ip addresses
                auto ip_addresses = netutils::get_local_addresses();
                if (!ip_addresses.empty()) {
                    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                    if (ImGui::TreeNode("Local IP-Addresses")) {
                        for (auto &adr : ip_addresses) {
                            ImGui::BulletText("%s", adr.c_str());
                        }
                        ImGui::TreePop();
                    }
                }

                // client count
                ImGui::Text("Connected clients: %u", (unsigned int) client_states.size());

                // iterate clients
                for (auto &client : client_states) {
                    auto address = API_CONTROLLER->get_ip_address(client.address);
                    if (ImGui::TreeNode(("Client @ " + address).c_str())) {
                        if (client.password.empty()) {
                            ImGui::Text("No password set.");
                        } else {
                            ImGui::Text("Password set.");
                        }
                        if (ImGui::TreeNode("Modules")) {
                            for (auto &module : client.modules) {
                                if (ImGui::TreeNode(module->name.c_str())) {
                                    ImGui::Text("Password force: %i", module->password_force);
                                    ImGui::TreePop();
                                }
                            }
                            ImGui::TreePop();
                        }
                        ImGui::TreePop();
                    }
                }
            }
            ImGui::EndTabItem();
        }

        // display input states
        if (ImGui::BeginTabItem("Input")) {

            // text view
            ImGui::Text("Service:   %s", service_state ? "ON" : "OFF");
            ImGui::Text("Test:      %s", test_state ? "ON" : "OFF");
            ImGui::Text("Coin Mech: %s", coin_mech_state ? "ON" : "OFF");
            ImGui::Text("BT-A:      %s", bt_a_state ? "ON" : "OFF");
            ImGui::Text("BT-B:      %s", bt_b_state ? "ON" : "OFF");
            ImGui::Text("BT-C:      %s", bt_c_state ? "ON" : "OFF");
            ImGui::Text("BT-D:      %s", bt_d_state ? "ON" : "OFF");
            ImGui::Text("FX-L:      %s", fx_l_state ? "ON" : "OFF");
            ImGui::Text("FX-R:      %s", fx_r_state ? "ON" : "OFF");
            ImGui::Text("Start:     %s", start_state ? "ON" : "OFF");
            ImGui::Text("Headphone: %s", headphone_state ? "ON" : "OFF");

            // labeled knobs
            ImGui::Knob(vol_l_state, 39.f);
            ImGui::SameLine();
            ImGui::Knob(vol_r_state, 39.f);
            ImGui::Text("VOL-L  VOL-R");

            // prepare controller view
            auto draw = ImGui::GetWindowDrawList();
            float bt_space = 50;
            ImVec2 bt_a_pos(175 + 0 * bt_space, 140);
            ImVec2 bt_b_pos(175 + 1 * bt_space, 140);
            ImVec2 bt_c_pos(175 + 2 * bt_space, 140);
            ImVec2 bt_d_pos(175 + 3 * bt_space, 140);
            ImVec2 fx_l_pos((bt_a_pos.x + bt_b_pos.x) * 0.5f, bt_a_pos.y + bt_space);
            ImVec2 fx_r_pos((bt_c_pos.x + bt_d_pos.x) * 0.5f, bt_d_pos.y + bt_space);
            ImVec2 start_pos((bt_b_pos.x + bt_c_pos.x) * 0.5f, bt_b_pos.y - bt_space);
            ImVec2 vol_l_pos(bt_a_pos.x - bt_space * 1.00f, bt_a_pos.y - bt_space * 1.25f);
            ImVec2 vol_r_pos(bt_d_pos.x + bt_space * 0.25f, bt_d_pos.y - bt_space * 1.25f);
            ImColor col_off(1.f, 1.f, 1.f, 1.f);
            ImColor col_on(1.f, 0.3f, 0.3f, 1.f);

            // abcd buttons
            draw->AddRectFilled(
                    ImVec2(bt_a_pos.x - 15, bt_a_pos.y - 15),
                    ImVec2(bt_a_pos.x + 15, bt_a_pos.y + 15),
                    bt_a_state ? col_on : col_off);
            draw->AddRectFilled(
                    ImVec2(bt_b_pos.x - 15, bt_b_pos.y - 15),
                    ImVec2(bt_b_pos.x + 15, bt_b_pos.y + 15),
                    bt_b_state ? col_on : col_off);
            draw->AddRectFilled(
                    ImVec2(bt_c_pos.x - 15, bt_c_pos.y - 15),
                    ImVec2(bt_c_pos.x + 15, bt_c_pos.y + 15),
                    bt_c_state ? col_on : col_off);
            draw->AddRectFilled(
                    ImVec2(bt_d_pos.x - 15, bt_d_pos.y - 15),
                    ImVec2(bt_d_pos.x + 15, bt_d_pos.y + 15),
                    bt_d_state ? col_on : col_off);

            // fx buttons
            draw->AddRectFilled(
                    ImVec2(fx_l_pos.x - 20, fx_l_pos.y - 10),
                    ImVec2(fx_l_pos.x + 20, fx_l_pos.y + 10),
                    fx_l_state ? col_on : col_off);
            draw->AddRectFilled(
                    ImVec2(fx_r_pos.x - 20, fx_r_pos.y - 10),
                    ImVec2(fx_r_pos.x + 20, fx_r_pos.y + 10),
                    fx_r_state ? col_on : col_off);

            // start button
            draw->AddRectFilled(
                    ImVec2(start_pos.x - 10, start_pos.y - 10),
                    ImVec2(start_pos.x + 10, start_pos.y + 10),
                    start_state ? col_on : col_off);

            // knobs
            ImGui::Knob(vol_l_state, bt_space * 0.75f, 2.f,
                    vol_l_pos.x, vol_l_pos.y);
            ImGui::Knob(vol_r_state, bt_space * 0.75f, 2.f,
                    vol_r_pos.x, vol_r_pos.y);

            // end tab
            ImGui::EndTabItem();
        }

        // display light states
        if (ImGui::BeginTabItem("Lights")) {

            // set up columns
            ImGui::Columns(3, "LightColumns", false);
            ImGui::SetColumnWidth(0, 60);
            ImGui::SetColumnWidth(1, 60);
            ImGui::TextColored(ImVec4(1.f, 0.7f, 0, 1), "Hue Amp");
            ImGui::NextColumn();
            ImGui::TextColored(ImVec4(1.f, 0.7f, 0, 1), "Hue Per");
            ImGui::NextColumn();
            ImGui::TextColored(ImVec4(1.f, 0.7f, 0, 1), "Light Color");
            ImGui::NextColumn();
            ImGui::Separator();

            // lock data
            worker_m.lock();

            // wing left up light
            ImGui::SetNextItemWidth(50);
            ImGui::DragFloat("###hue00", (float *) &light_wing_left_up_hue_amp, 0.001f, 0.f, 16.f, "%.3f", 2.f);
            ImGui::NextColumn();
            ImGui::SetNextItemWidth(50);
            ImGui::DragFloat("###hue01", (float *) &light_wing_left_up_hue_per, 0.001f, 0.001f, 60.f, "%.3f", 2.f);
            ImGui::NextColumn();
            ImGui::SetNextItemWidth(150);
            if (light_hue_preview) {
                auto shift = hue_shift(light_wing_left_up,
                                       light_wing_left_up_hue_amp,
                                       light_wing_left_up_hue_per,
                                       time_cur);
                ImGui::ColorEdit3("Wing Left Up", (float *) &shift);
            } else {
                ImGui::ColorEdit3("Wing Left Up", (float *) &light_wing_left_up);
            }
            ImGui::NextColumn();

            // wing left low light
            ImGui::SetNextItemWidth(50);
            ImGui::DragFloat("###hue10", (float *) &light_wing_left_low_hue_amp, 0.001f, 0.f, 16.f, "%.3f", 2.f);
            ImGui::NextColumn();
            ImGui::SetNextItemWidth(50);
            ImGui::DragFloat("###hue11", (float *) &light_wing_left_low_hue_per, 0.001f, 0.001f, 60.f, "%.3f", 2.f);
            ImGui::NextColumn();
            ImGui::SetNextItemWidth(150);
            if (light_hue_preview) {
                auto shift = hue_shift(light_wing_left_low,
                                       light_wing_left_low_hue_amp,
                                       light_wing_left_low_hue_per,
                                       time_cur);
                ImGui::ColorEdit3("Wing Left Low", (float *) &shift);
            } else {
                ImGui::ColorEdit3("Wing Left Low", (float *) &light_wing_left_low);
            }
            ImGui::NextColumn();

            // wing right up light
            ImGui::SetNextItemWidth(50);
            ImGui::DragFloat("###hue20", (float *) &light_wing_right_up_hue_amp, 0.001f, 0.f, 16.f, "%.3f", 2.f);
            ImGui::NextColumn();
            ImGui::SetNextItemWidth(50);
            ImGui::DragFloat("###hue21", (float *) &light_wing_right_up_hue_per, 0.001f, 0.001f, 60.f, "%.3f", 2.f);
            ImGui::NextColumn();
            ImGui::SetNextItemWidth(150);
            if (light_hue_preview) {
                auto shift = hue_shift(light_wing_right_up,
                                       light_wing_right_up_hue_amp,
                                       light_wing_right_up_hue_per,
                                       time_cur);
                ImGui::ColorEdit3("Wing Right Up", (float *) &shift);
            } else {
                ImGui::ColorEdit3("Wing Right Up", (float *) &light_wing_right_up);
            }
            ImGui::NextColumn();

            // wing right low light
            ImGui::SetNextItemWidth(50);
            ImGui::DragFloat("###hue30", (float *) &light_wing_right_low_hue_amp, 0.001f, 0.f, 16.f, "%.3f", 2.f);
            ImGui::NextColumn();
            ImGui::SetNextItemWidth(50);
            ImGui::DragFloat("###hue31", (float *) &light_wing_right_low_hue_per, 0.001f, 0.001f, 60.f, "%.3f", 2.f);
            ImGui::NextColumn();
            ImGui::SetNextItemWidth(150);
            if (light_hue_preview) {
                auto shift = hue_shift(light_wing_right_low,
                                       light_wing_right_low_hue_amp,
                                       light_wing_right_low_hue_per,
                                       time_cur);
                ImGui::ColorEdit3("Wing Right Low", (float *) &shift);
            } else {
                ImGui::ColorEdit3("Wing Right Low", (float *) &light_wing_right_low);
            }
            ImGui::NextColumn();

            // woofer light
            ImGui::SetNextItemWidth(50);
            ImGui::DragFloat("###hue40", (float *) &light_woofer_hue_amp, 0.001f, 0.f, 16.f, "%.3f", 2.f);
            ImGui::NextColumn();
            ImGui::SetNextItemWidth(50);
            ImGui::DragFloat("###hue41", (float *) &light_woofer_hue_per, 0.001f, 0.001f, 60.f, "%.3f", 2.f);
            ImGui::NextColumn();
            ImGui::SetNextItemWidth(150);
            if (light_hue_preview) {
                auto shift = hue_shift(light_woofer,
                                       light_woofer_hue_amp,
                                       light_woofer_hue_per,
                                       time_cur);
                ImGui::ColorEdit3("Woofer", (float *) &shift);
            } else {
                ImGui::ColorEdit3("Woofer", (float *) &light_woofer);
            }
            ImGui::NextColumn();

            // controller light
            ImGui::SetNextItemWidth(50);
            ImGui::DragFloat("###hue50", (float *) &light_controller_hue_amp, 0.001f, 0.f, 16.f, "%.3f", 2.f);
            ImGui::NextColumn();
            ImGui::SetNextItemWidth(50);
            ImGui::DragFloat("###hue51", (float *) &light_controller_hue_per, 0.001f, 0.001f, 60.f, "%.3f", 2.f);
            ImGui::NextColumn();
            ImGui::SetNextItemWidth(150);
            if (light_hue_preview) {
                auto shift = hue_shift(light_controller,
                                       light_controller_hue_amp,
                                       light_controller_hue_per,
                                       time_cur);
                ImGui::ColorEdit3("Controller", (float *) &shift);
            } else {
                ImGui::ColorEdit3("Controller", (float *) &light_controller);
            }
            ImGui::NextColumn();

            // generator light
            ImGui::SetNextItemWidth(50);
            ImGui::DragFloat("###hue60", (float *) &light_generator_hue_amp, 0.001f, 0.f, 16.f, "%.3f", 2.f);
            ImGui::NextColumn();
            ImGui::SetNextItemWidth(50);
            ImGui::DragFloat("###hue61", (float *) &light_generator_hue_per, 0.001f, 0.001f, 60.f, "%.3f", 2.f);
            ImGui::NextColumn();
            ImGui::SetNextItemWidth(150);
            if (light_hue_preview) {
                auto shift = hue_shift(light_generator,
                        light_generator_hue_amp,
                        light_generator_hue_per,
                        time_cur);
                ImGui::ColorEdit3("Generator", (float *) &shift);
            } else {
                ImGui::ColorEdit3("Generator", (float *) &light_generator);
            }
            ImGui::NextColumn();

            // clean
            worker_m.unlock();
            ImGui::Columns();

            // settings
            ImGui::Text("Animation Settings");
            const char *HUE_FUNCS[] = {"Sine", "Cosine", "Absolute", "Linear", "Count"};
            if (ImGui::BeginCombo("Hue Function", HUE_FUNCS[(int) light_hue_func])) {
                for (int i = 0; i < (int) HueFunc::Count; i++) {
                    bool selected = i == (int) light_hue_func;
                    if (ImGui::Selectable(HUE_FUNCS[i], selected)) {
                        light_hue_func = (HueFunc) i;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::Checkbox("Hue Preview", &light_hue_preview);
            ImGui::SameLine();
            ImGui::Checkbox("Hue Disable", &light_hue_disable);

            // end tab
            ImGui::EndTabItem();
        }

        // display volume states
        if (ImGui::BeginTabItem("Volumes")) {
            ImGui::Text(
                    "Sound Amplifier\n"
                    "---------------\n");
            ImGui::Text("Sound");
            ImGui::SliderFloat("###Sound", &vol_sound, 0.f, 1.f, "%.2f");
            ImGui::Text("Headphone");
            ImGui::SliderFloat("###Headphone", &vol_headphone, 0.f, 1.f, "%.2f");
            ImGui::Text("External");
            ImGui::SliderFloat("###External", &vol_external, 0.f, 1.f, "%.2f");
            ImGui::Text("Woofer");
            ImGui::SliderFloat("###Woofer", &vol_woofer, 0.f, 1.f, "%.2f");
            ImGui::Dummy(ImVec2(0.f, 5.f));
            ImGui::Separator();
            ImGui::Text("Settings");
            ImGui::Checkbox("Mute", &vol_mute);
            ImGui::EndTabItem();
        }

        // keys
        if (ImGui::BeginTabItem("Keys")) {
            ImGui::Text("Input Configuration");
            if (ImGui::BeginChild("Buttons")) {
                ImGui::SliderInt("Service", &scan_service, 0, 127, key_format(scan_service).c_str());
                ImGui::SliderInt("Test", &scan_test, 0, 127, key_format(scan_test).c_str());
                ImGui::SliderInt("Coin Mech", &scan_coin_mech, 0, 127, key_format(scan_coin_mech).c_str());
                ImGui::SliderInt("BT-A", &scan_bt_a, 0, 127, key_format(scan_bt_a).c_str());
                ImGui::SliderInt("BT-B", &scan_bt_b, 0, 127, key_format(scan_bt_b).c_str());
                ImGui::SliderInt("BT-C", &scan_bt_c, 0, 127, key_format(scan_bt_c).c_str());
                ImGui::SliderInt("BT-D", &scan_bt_d, 0, 127, key_format(scan_bt_d).c_str());
                ImGui::SliderInt("FX-L", &scan_fx_l, 0, 127, key_format(scan_fx_l).c_str());
                ImGui::SliderInt("FX-R", &scan_fx_r, 0, 127, key_format(scan_fx_r).c_str());
                ImGui::SliderInt("Start", &scan_start, 0, 127, key_format(scan_start).c_str());
                ImGui::SliderInt("Headphone", &scan_headphone, 0, 127, key_format(scan_headphone).c_str());
                ImGui::SliderInt("VOL-L Left", &scan_vol_l_left, 0, 127, key_format(scan_vol_l_left).c_str());
                ImGui::SliderInt("VOL-L Right", &scan_vol_l_right, 0, 127, key_format(scan_vol_l_right).c_str());
                ImGui::SliderInt("VOL-R Left", &scan_vol_r_left, 0, 127, key_format(scan_vol_r_left).c_str());
                ImGui::SliderInt("VOL-R Right", &scan_vol_r_right, 0, 127, key_format(scan_vol_r_right).c_str());
                ImGui::SliderInt("Card Swipe", &scan_icca, 0, 127, key_format(scan_icca).c_str());
                ImGui::SliderInt("Coin Insert", &scan_coin, 0, 127, key_format(scan_coin).c_str());
                ImGui::SliderInt("Keypad 0", &scan_kp_0, 0, 127, key_format(scan_kp_0).c_str());
                ImGui::SliderInt("Keypad 1", &scan_kp_1, 0, 127, key_format(scan_kp_1).c_str());
                ImGui::SliderInt("Keypad 2", &scan_kp_2, 0, 127, key_format(scan_kp_2).c_str());
                ImGui::SliderInt("Keypad 3", &scan_kp_3, 0, 127, key_format(scan_kp_3).c_str());
                ImGui::SliderInt("Keypad 4", &scan_kp_4, 0, 127, key_format(scan_kp_4).c_str());
                ImGui::SliderInt("Keypad 5", &scan_kp_5, 0, 127, key_format(scan_kp_5).c_str());
                ImGui::SliderInt("Keypad 6", &scan_kp_6, 0, 127, key_format(scan_kp_6).c_str());
                ImGui::SliderInt("Keypad 7", &scan_kp_7, 0, 127, key_format(scan_kp_7).c_str());
                ImGui::SliderInt("Keypad 8", &scan_kp_8, 0, 127, key_format(scan_kp_8).c_str());
                ImGui::SliderInt("Keypad 9", &scan_kp_9, 0, 127, key_format(scan_kp_9).c_str());
                ImGui::SliderInt("Keypad 00", &scan_kp_00, 0, 127, key_format(scan_kp_00).c_str());
                ImGui::SliderInt("Keypad Dec", &scan_kp_decimal, 0, 127, key_format(scan_kp_decimal).c_str());
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        // opt
        if (ImGui::BeginTabItem("Opt")) {
            ImGui::Text("Tweaking");
            ImGui::SameLine();
            ImGui::HelpMarker("Change these settings on your own risk.\nReset if you messed up.");
            ImGui::DragFloat("Knob Deadzone", &vol_deadzone, 0.0001f, 0.f, 1.f, "%.4f");
            ImGui::DragFloat("Mouse Sensitivity", &vol_mouse_sensitivity, 1.f, 0.f, 0.f, "%.0f");
            ImGui::DragInt("Knob Timeout (ms)", (int*) &vol_timeout, 1, 0, 2000);
            ImGui::DragInt("Card Timeout (ms)", (int*) &icca_timeout, 1, 0, 2000);
            ImGui::DragInt("Coin Timeout (ms)", (int*) &coin_timeout, 1, 0, 2000);
            ImGui::DragInt("Poll Delay (ms)", (int*) &poll_delay, 0.01f, 0, 10);
            ImGui::Separator();
            ImGui::Text("Card Swipe Write ID to File");
            ImGui::InputText("###icca_file", icca_file, sizeof(icca_file));
            ImGui::SameLine();
            if (ImGui::Button("...")) {
                std::thread thread([this] {

                    // open dialog to get path
                    auto ofn_path = std::make_unique<wchar_t[]>(sizeof(icca_file));
                    OPENFILENAMEW ofn {};
                    memset(&ofn, 0, sizeof(ofn));
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = nullptr;
                    ofn.lpstrFilter = L"";
                    ofn.lpstrFile = ofn_path.get();
                    ofn.nMaxFile = sizeof(icca_file);
                    ofn.Flags = OFN_EXPLORER;
                    ofn.lpstrDefExt = L"txt";

                    // check for success
                    if (GetSaveFileNameW(&ofn)) {

                        // update path
                        std::string path = std::filesystem::path(ofn_path.get()).string();
                        if (path.length() < sizeof(icca_file)) {
                            strcpy(icca_file, path.c_str());
                        }

                    } else {
                        auto error = CommDlgExtendedError();
                        if (error) {
                            log_warning("kfcontrol", "failed to get save file name: {}", error);
                        } else {
                            log_warning("kfcontrol", "failed to get save file name");
                        }
                    }
                });
                thread.detach();
            }
            ImGui::Separator();
            ImGui::Checkbox("Button Lights", &light_buttons);
            ImGui::SameLine();
            ImGui::HelpMarker("Lights up buttons when pressed.");
            ImGui::Checkbox("Keypad Profile Switch", &kp_profiles);
            ImGui::SameLine();
            ImGui::HelpMarker("Changes the profile on number press.\nDiscards unsaved settings.");
            ImGui::EndTabItem();
        }

        // misc
        if (ImGui::BeginTabItem("Misc")) {

            /*
             * LCD Settings
             */

            ImGui::Text("LCD Settings");
            int lcd_bri = games::shared::LCD_BRI;
            int lcd_con = games::shared::LCD_CON;
            int lcd_bl = games::shared::LCD_BL;
            int lcd_red = games::shared::LCD_RED;
            int lcd_green = games::shared::LCD_GREEN;
            int lcd_blue = games::shared::LCD_BLUE;
            if (ImGui::SliderInt("Brightness", &lcd_bri, 0, 255)) {
                games::shared::LCD_BRI = lcd_bri;
            }
            if (ImGui::SliderInt("Contrast", &lcd_con, 0, 255)) {
                games::shared::LCD_CON = lcd_con;
            }
            if (ImGui::SliderInt("Black Value", &lcd_bl, 0, 255)) {
                games::shared::LCD_BL = lcd_bl;
            }
            if (ImGui::SliderInt("Red Value", &lcd_red, 0, 255)) {
                games::shared::LCD_RED = lcd_red;
            }
            if (ImGui::SliderInt("Green Value", &lcd_green, 0, 255)) {
                games::shared::LCD_GREEN = lcd_green;
            }
            if (ImGui::SliderInt("Blue Value", &lcd_blue, 0, 255)) {
                games::shared::LCD_BLUE = lcd_blue;
            }

            // settings
            ImGui::Separator();
            ImGui::Text("Settings");
            auto block = eamuse_coin_get_block();
            if (ImGui::Checkbox("Coin Blocker", &block)) {
                eamuse_coin_set_block(block);
            }
            ImGui::SameLine();
            ImGui::Checkbox("Knobs Mouse", &vol_mouse);
            ImGui::SameLine();
            ImGui::Checkbox("Start Click", &start_click);

            // save/load
            ImGui::Separator();
            ImGui::Text("Configuration");
            if (ImGui::Button("Save")) {
                config_save();
            }
            ImGui::SameLine();
            if (ImGui::Button("Load")) {
                config_load();
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset")) {
                fileutils::text_write(config_path, "");
                launcher::restart();
            }
            ImGui::SameLine();
            ImGui::Checkbox("Overlay", &OVERLAY->hotkeys_enable);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            if (ImGui::InputInt("Profile", &config_profile, 1, 0)) {
                if (config_profile < 0) config_profile = 0;
                this->config_load();
            }

            ImGui::EndTabItem();
        }

        // about
        if (ImGui::BeginTabItem("About")) {
            ImGui::Text(
                    "Description\n"
                    "-----------\n");
            ImGui::Text(
                  //"------------------------------------------------------"
                    "This tool allows you to remotely control your\n"
                    "KFChicken powered cabinet.");
            ImGui::Text(
                  //"------------------------------------------------------"
                    "Buttons, knobs, card swipes and coins are mapped to\n"
                    "your keyboard using the SendInput Windows API. Using\n"
                    "this you can play K-Shoot MANIA and USC using your\n"
                    "legit Sound Voltex cabinet.\n");
            ImGui::Text(
                    //"------------------------------------------------------"
                    "You can also use this to receive remote input from\n"
                    "the SpiceCompanion app.\n");
            ImGui::Dummy(ImVec2(0, 5));
            ImGui::Text(
                    "Basic Usage\n"
                    "-----------\n");
            ImGui::Text(
                  //"------------------------------------------------------"
                    "Start KFControl using the '-api [PORT]' and\n"
                    "'-apipass [PASS]' parameters and point your\n"
                    "cabinet running KFChicken to this computer\n"
                    "using the specified settings.");
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    void KFControl::worker_start() {
        worker_running = true;
        this->worker.reset(new std::thread(&KFControl::worker_func, this));
    }

    void KFControl::worker_func() {

        // get IO pointers
        auto buttons = games::get_buttons("Sound Voltex");
        auto analogs = games::get_analogs("Sound Voltex");
        auto lights = games::get_lights("Sound Voltex");

        // button states
        bool service_old = false;
        bool test_old = false;
        bool coin_mech_old = false;
        bool bt_a_old = false;
        bool bt_b_old = false;
        bool bt_c_old = false;
        bool bt_d_old = false;
        bool fx_l_old = false;
        bool fx_r_old = false;
        bool start_old = false;
        bool start_click_old = false;
        bool headphone_old = false;
        bool kp_0_old = false;
        bool kp_1_old = false;
        bool kp_2_old = false;
        bool kp_3_old = false;
        bool kp_4_old = false;
        bool kp_5_old = false;
        bool kp_6_old = false;
        bool kp_7_old = false;
        bool kp_8_old = false;
        bool kp_9_old = false;
        bool kp_00_old = false;
        bool kp_decimal_old = false;

        // volume states
        float vol_l_old = Analogs::getState(RI_MGR, analogs->at(games::sdvx::Analogs::VOL_L));
        float vol_r_old = Analogs::getState(RI_MGR, analogs->at(games::sdvx::Analogs::VOL_R));
        int vol_l_trigger = 0;
        int vol_r_trigger = 0;
        uint64_t vol_l_time = 0;
        uint64_t vol_r_time = 0;

        // icca/coin states
        bool icca_old = false;
        bool coin_old = false;
        uint64_t icca_time = 0;
        uint64_t coin_time = 0;

        // main loop
        while (worker_running) {
            uint64_t time_cur = get_system_milliseconds();

            // update button states
            auto service_state = Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::Service));
            worker_button_check(service_state, &service_old, scan_service);
            auto test_state = Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::Test));
            worker_button_check(test_state, &test_old, scan_test);
            auto coin_mech_state = Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::CoinMech));
            worker_button_check(coin_mech_state, &coin_mech_old, scan_coin_mech);
            auto bt_a_state = Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::BT_A));
            worker_button_check(bt_a_state, &bt_a_old, scan_bt_a);
            auto bt_b_state = Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::BT_B));
            worker_button_check(bt_b_state, &bt_b_old, scan_bt_b);
            auto bt_c_state = Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::BT_C));
            worker_button_check(bt_c_state, &bt_c_old, scan_bt_c);
            auto bt_d_state = Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::BT_D));
            worker_button_check(bt_d_state, &bt_d_old, scan_bt_d);
            auto fx_l_state = Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::FX_L));
            worker_button_check(fx_l_state, &fx_l_old, scan_fx_l);
            auto fx_r_state = Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::FX_R));
            worker_button_check(fx_r_state, &fx_r_old, scan_fx_r);
            auto start_state = Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::Start));
            worker_button_check(start_state, &start_old, scan_start);
            auto headphone_state = Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::Headphone));
            worker_button_check(headphone_state, &headphone_old, scan_headphone);

            // mouse click on start button
            if (start_click) {
                if (start_state) {
                    if (!start_click_old) {
                        start_click_old = true;
                        worker_mouse_click(true);
                    }
                } else {
                    if (start_click_old) {
                        start_click_old = false;
                        worker_mouse_click(false);
                    }
                }
            }

            // get analog states
            auto vol_l_state = Analogs::getState(RI_MGR, analogs->at(games::sdvx::Analogs::VOL_L));
            auto vol_r_state = Analogs::getState(RI_MGR, analogs->at(games::sdvx::Analogs::VOL_R));
            auto vol_l_diff = vol_l_state - vol_l_old;
            auto vol_r_diff = vol_r_state - vol_r_old;
            auto vol_l_delta = time_cur - vol_l_time;
            auto vol_r_delta = time_cur - vol_r_time;

            // wrap around
            if (vol_l_diff > 0.5f) {
                vol_l_diff = -vol_l_old - (1 - vol_l_state);
            }
            if (vol_l_diff < -0.5f) {
                vol_l_diff = (1 - vol_l_old) + vol_l_state;
            }
            if (vol_r_diff > 0.5f) {
                vol_r_diff = -vol_r_old - (1 - vol_r_state);
            }
            if (vol_r_diff < -0.5f) {
                vol_r_diff = (1 - vol_r_old) + vol_r_state;
            }

            // manual volume controls
            if (Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::VOL_L_Left))) {
                vol_l_diff += vol_deadzone * -2;
            }
            if (Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::VOL_L_Right))) {
                vol_l_diff += vol_deadzone * 2;
            }
            if (Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::VOL_R_Left))) {
                vol_r_diff += vol_deadzone * -2;
            }
            if (Buttons::getState(RI_MGR, buttons->at(games::sdvx::Buttons::VOL_R_Right))) {
                vol_r_diff += vol_deadzone * 2;
            }

            // left knob
            if (std::abs(vol_l_diff) > vol_deadzone || vol_l_delta < vol_timeout) {
                if (std::abs(vol_l_diff) > vol_deadzone) {
                    vol_l_time = time_cur;
                }
                vol_l_old = vol_l_state;
                if (vol_l_diff < 0 && vol_l_trigger != scan_vol_l_left) {
                    if (vol_l_trigger != 0) worker_button_set(vol_l_trigger, false);
                    vol_l_trigger = scan_vol_l_left;
                    worker_button_set(vol_l_trigger, true);
                } else if (vol_l_diff > 0 && vol_l_trigger != scan_vol_l_right) {
                    if (vol_l_trigger != 0) worker_button_set(vol_l_trigger, false);
                    vol_l_trigger = scan_vol_l_right;
                    worker_button_set(vol_l_trigger, true);
                }
                if (vol_mouse) {
                    worker_mouse_move(vol_l_diff * vol_mouse_sensitivity, 0);
                }
            } else {
                vol_l_time = 0;
                if (vol_l_trigger != 0) {
                    worker_button_set(vol_l_trigger, false);
                    vol_l_trigger = 0;
                }
            }

            // right knob
            if (std::abs(vol_r_diff) > vol_deadzone || vol_r_delta < vol_timeout) {
                if (std::abs(vol_r_diff) > vol_deadzone) {
                    vol_r_time = time_cur;
                }
                vol_r_old = vol_r_state;
                if (vol_r_diff < 0 && vol_r_trigger != scan_vol_r_left) {
                    if (vol_r_trigger != 0) worker_button_set(vol_r_trigger, false);
                    vol_r_trigger = scan_vol_r_left;
                    worker_button_set(vol_r_trigger, true);
                } else if (vol_r_diff > 0 && vol_r_trigger != scan_vol_r_right) {
                    if (vol_r_trigger != 0) worker_button_set(vol_r_trigger, false);
                    vol_r_trigger = scan_vol_r_right;
                    worker_button_set(vol_r_trigger, true);
                }
                if (vol_mouse) {
                    worker_mouse_move(0, vol_r_diff * vol_mouse_sensitivity);
                }
            } else {
                vol_r_time = 0;
                if (vol_r_trigger != 0) {
                    worker_button_set(vol_r_trigger, false);
                    vol_r_trigger = 0;
                }
            }

            // update icca
            if (eamuse_card_insert_consume(1, 0)) {
                icca_time = time_cur;

                // write card ID to file
                if (strlen(icca_file) > 0) {
                    uint8_t card_data[8];
                    if (eamuse_get_card(1, 0, card_data)) {
                        auto card_hex = bin2hex(card_data, sizeof(card_data));
                        fileutils::text_write(icca_file, card_hex);
                        log_info("kfcontrol", "wrote {} to file", card_hex);
                    }
                }
            }
            if (time_cur - icca_time < icca_timeout) {
                if (!icca_old) {
                    worker_button_set(scan_icca, true);
                    icca_old = true;
                }
            } else if (icca_old) {
                worker_button_set(scan_icca, false);
                icca_old = false;
            }

            // update coin
            if (eamuse_coin_consume_stock()) {
                coin_time = time_cur;
            }
            if (time_cur - coin_time < coin_timeout) {
                if (!coin_old) {
                    worker_button_set(scan_coin, true);
                    coin_old = true;
                }
            } else if (icca_old) {
                worker_button_set(scan_coin, false);
                coin_old = false;
            }

            // light hue shifts
            auto hue_wing_left_up = hue_shift(
                    light_wing_left_up,
                    light_wing_left_up_hue_amp,
                    light_wing_left_up_hue_per,
                    time_cur);
            auto hue_wing_left_low = hue_shift(
                    light_wing_left_low,
                    light_wing_left_low_hue_amp,
                    light_wing_left_low_hue_per,
                    time_cur);
            auto hue_wing_right_up = hue_shift(
                    light_wing_right_up,
                    light_wing_right_up_hue_amp,
                    light_wing_right_up_hue_per,
                    time_cur);
            auto hue_wing_right_low = hue_shift(
                    light_wing_right_low,
                    light_wing_right_low_hue_amp,
                    light_wing_right_low_hue_per,
                    time_cur);
            auto hue_woofer = hue_shift(
                    light_woofer,
                    light_woofer_hue_amp,
                    light_woofer_hue_per,
                    time_cur);
            auto hue_controller = hue_shift(
                    light_controller,
                    light_controller_hue_amp,
                    light_controller_hue_per,
                    time_cur);
            auto hue_generator = hue_shift(
                    light_generator,
                    light_generator_hue_amp,
                    light_generator_hue_per,
                    time_cur);

            // update lights
            worker_m.lock();
            Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::WING_LEFT_UP_R), hue_wing_left_up.x);
            Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::WING_LEFT_UP_G), hue_wing_left_up.y);
            Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::WING_LEFT_UP_B), hue_wing_left_up.z);
            Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::WING_LEFT_LOW_R), hue_wing_left_low.x);
            Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::WING_LEFT_LOW_G), hue_wing_left_low.y);
            Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::WING_LEFT_LOW_B), hue_wing_left_low.z);
            Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::WING_RIGHT_UP_R), hue_wing_right_up.x);
            Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::WING_RIGHT_UP_G), hue_wing_right_up.y);
            Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::WING_RIGHT_UP_B), hue_wing_right_up.z);
            Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::WING_RIGHT_LOW_R), hue_wing_right_low.x);
            Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::WING_RIGHT_LOW_G), hue_wing_right_low.y);
            Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::WING_RIGHT_LOW_B), hue_wing_right_low.z);
            Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::WOOFER_R), hue_woofer.x);
            Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::WOOFER_G), hue_woofer.y);
            Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::WOOFER_B), hue_woofer.z);
            Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::CONTROLLER_R), hue_controller.x);
            Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::CONTROLLER_G), hue_controller.y);
            Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::CONTROLLER_B), hue_controller.z);
            Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::GENERATOR_R), hue_generator.x);
            Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::GENERATOR_G), hue_generator.y);
            Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::GENERATOR_B), hue_generator.z);
            if (vol_mute) {
                Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::VOLUME_SOUND), 0.f);
                Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::VOLUME_HEADPHONE), 0.f);
                Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::VOLUME_EXTERNAL), 0.f);
                Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::VOLUME_WOOFER), 0.f);
            } else {
                Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::VOLUME_SOUND), vol_sound);
                Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::VOLUME_HEADPHONE), vol_headphone);
                Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::VOLUME_EXTERNAL), vol_external);
                Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::VOLUME_WOOFER), vol_woofer);
            }
            worker_m.unlock();

            // button lights
            if (light_buttons) {
                Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::BT_A), bt_a_state ? 1.f : 0.f);
                Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::BT_B), bt_b_state ? 1.f : 0.f);
                Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::BT_C), bt_c_state ? 1.f : 0.f);
                Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::BT_D), bt_d_state ? 1.f : 0.f);
                Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::FX_L), fx_l_state ? 1.f : 0.f);
                Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::FX_R), fx_r_state ? 1.f : 0.f);
                Lights::writeLight(RI_MGR, lights->at(games::sdvx::Lights::START), start_state ? 1.f : 0.f);
            }

            // update devices
            RI_MGR->devices_flush_output();

            // check keypad for input
            auto keypad_state = eamuse_get_keypad_state(0);
            worker_button_check(keypad_state & (1 << EAM_IO_KEYPAD_0), &kp_0_old, scan_kp_0, 0);
            worker_button_check(keypad_state & (1 << EAM_IO_KEYPAD_1), &kp_1_old, scan_kp_1, 1);
            worker_button_check(keypad_state & (1 << EAM_IO_KEYPAD_2), &kp_2_old, scan_kp_2, 2);
            worker_button_check(keypad_state & (1 << EAM_IO_KEYPAD_3), &kp_3_old, scan_kp_3, 3);
            worker_button_check(keypad_state & (1 << EAM_IO_KEYPAD_4), &kp_4_old, scan_kp_4, 4);
            worker_button_check(keypad_state & (1 << EAM_IO_KEYPAD_5), &kp_5_old, scan_kp_5, 5);
            worker_button_check(keypad_state & (1 << EAM_IO_KEYPAD_6), &kp_6_old, scan_kp_6, 6);
            worker_button_check(keypad_state & (1 << EAM_IO_KEYPAD_7), &kp_7_old, scan_kp_7, 7);
            worker_button_check(keypad_state & (1 << EAM_IO_KEYPAD_8), &kp_8_old, scan_kp_8, 8);
            worker_button_check(keypad_state & (1 << EAM_IO_KEYPAD_9), &kp_9_old, scan_kp_9, 9);
            worker_button_check(keypad_state & (1 << EAM_IO_KEYPAD_00), &kp_00_old, scan_kp_00);
            worker_button_check(keypad_state & (1 << EAM_IO_KEYPAD_DECIMAL), &kp_decimal_old, scan_kp_decimal);

            // slow down
            std::this_thread::sleep_for(std::chrono::milliseconds(poll_delay));
        }
    }

    void KFControl::worker_button_check(bool state_new, bool *state_old, int scan, int profile) {

        // check for trigger
        if (state_new != *state_old) {

            // set button state
            if (scan != 0) {
                worker_button_set(scan, state_new);
            }

            // switch profile
            if (kp_profiles && profile >= 0 && state_new) {
                config_profile = profile;
                config_load();
            }
        }

        // update old state
        *state_old = state_new;
    }

    void KFControl::worker_button_set(int scan, bool state) {
        INPUT ip;
        ip.type = INPUT_KEYBOARD;
        ip.ki.wScan = scan;
        ip.ki.time = 0;
        ip.ki.dwExtraInfo = 0;
        ip.ki.wVk = 0;
        ip.ki.dwFlags = KEYEVENTF_SCANCODE | (state ? 0 : KEYEVENTF_KEYUP);
        SendInput(1, &ip, sizeof(INPUT));
    }

    void KFControl::worker_mouse_click(bool state) {
        INPUT ip;
        ip.type = INPUT_MOUSE;
        ip.mi.dwFlags = state ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
        SendInput(1, &ip, sizeof(INPUT));
    }

    void KFControl::worker_mouse_move(int dx, int dy) {
        if (dx == 0 && dy == 0) return;
        INPUT ip;
        ip.type = INPUT_MOUSE;
        ip.mi.dx = dx;
        ip.mi.dy = dy;
        ip.mi.mouseData = 0;
        ip.mi.dwFlags = MOUSEEVENTF_MOVE;
        ip.mi.dwExtraInfo = 0;
        SendInput(1, &ip, sizeof(INPUT));
    }
}

#pragma once

#include "overlay/window.h"

#include <thread>
#include <mutex>
#include <memory>

namespace overlay::windows {

    enum class HueFunc : int {
        Sine, Cosine, Absolute, Linear,
        Count
    };

    class KFControl : public Window {
    public:

        KFControl(SpiceOverlay *overlay);
        ~KFControl() override;

        void config_save();
        void config_load();
        ImVec4 hsv_transform(ImVec4 col,
                float hue = 0.f, float sat = 1.f, float val = 1.f);
        ImVec4 hue_shift(ImVec4 col, float amp, float per, uint64_t ms);

        void build_content() override;

    private:
        std::string config_path;
        int config_profile = 0;

        std::unique_ptr<std::thread> worker;
        bool worker_running = false;
        void worker_start();
        void worker_func();
        std::mutex worker_m;
        void worker_button_check(bool state_new, bool *state_old, int scan,
                int profile_switch = -1);
        void worker_button_set(int scan, bool state);
        void worker_mouse_click(bool state);
        void worker_mouse_move(int dx, int dy);

        int poll_delay = 1;
        float vol_deadzone = 0.003f;
        uint64_t vol_timeout = 32;
        bool vol_mouse = false;
        float vol_mouse_sensitivity = 512;
        bool start_click = false;
        bool kp_profiles = false;

        float vol_sound = 0.f;
        float vol_headphone = 0.f;
        float vol_external = 0.f;
        float vol_woofer = 0.f;
        bool vol_mute = false;

        char icca_file[512] = "";
        uint64_t icca_timeout = 4000;
        uint64_t coin_timeout = 250;

        ImVec4 light_wing_left_up {};
        ImVec4 light_wing_left_low {};
        ImVec4 light_wing_right_up {};
        ImVec4 light_wing_right_low {};
        ImVec4 light_woofer {};
        ImVec4 light_controller {};
        ImVec4 light_generator {};

        bool light_buttons = true;
        bool light_hue_preview = false;
        bool light_hue_disable = false;
        HueFunc light_hue_func = HueFunc::Sine;
        float light_wing_left_up_hue_amp = 0.f;
        float light_wing_left_up_hue_per = 1.f;
        float light_wing_left_low_hue_amp = 0.f;
        float light_wing_left_low_hue_per = 1.f;
        float light_wing_right_up_hue_amp = 0.f;
        float light_wing_right_up_hue_per = 1.f;
        float light_wing_right_low_hue_amp = 0.f;
        float light_wing_right_low_hue_per = 1.f;
        float light_woofer_hue_amp = 0.f;
        float light_woofer_hue_per = 1.f;
        float light_controller_hue_amp = 0.f;
        float light_controller_hue_per = 1.f;
        float light_generator_hue_amp = 0.f;
        float light_generator_hue_per = 1.f;

        /*
         * Keyboard Scancodes
         * Check: http://kbdlayout.info/kbdus/overview+scancodes
         */

        int scan_service = 3;
        int scan_test = 4;
        int scan_coin_mech = 5;
        int scan_bt_a = 32;
        int scan_bt_b = 33;
        int scan_bt_c = 36;
        int scan_bt_d = 37;
        int scan_fx_l = 46;
        int scan_fx_r = 50;
        int scan_start = 2;
        int scan_headphone = 0;
        int scan_vol_l_left = 17;
        int scan_vol_l_right = 18;
        int scan_vol_r_left = 24;
        int scan_vol_r_right = 25;
        int scan_icca = 6;
        int scan_coin = 7;
        int scan_kp_0 = 0;
        int scan_kp_1 = 0;
        int scan_kp_2 = 0;
        int scan_kp_3 = 0;
        int scan_kp_4 = 0;
        int scan_kp_5 = 0;
        int scan_kp_6 = 0;
        int scan_kp_7 = 0;
        int scan_kp_8 = 0;
        int scan_kp_9 = 0;
        int scan_kp_00 = 0;
        int scan_kp_decimal = 0;
    };
}

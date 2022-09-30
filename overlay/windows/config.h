#pragma once

#include "cfg/game.h"
#include "overlay/window.h"
#include "rawinput/device.h"
#include "external/imgui/imgui_filebrowser.h"
#include "patch_manager.h"

namespace overlay::windows {

    class Config : public Window {
    private:

        // game selection
        int games_selected = -1;
        std::string games_selected_name = "";
        std::vector<Game> games_list;
        std::vector<const char *> games_names;

        // buttons tab
        int buttons_page = 0;
        bool buttons_keyboard_state[0xFF];
        bool buttons_bind_active = false;
        bool buttons_many_active = false;
        bool buttons_many_naive = false;
        int buttons_many_delay = 0;
        int buttons_many_index = -1;

        // analogs tab
        std::vector<rawinput::Device *> analogs_devices;
        int analogs_devices_selected = -1;
        int analogs_devices_control_selected = -1;

        // lights tab
        int lights_page = 0;
        std::vector<rawinput::Device *> lights_devices;
        int lights_devices_selected = -1;
        int lights_devices_control_selected = -1;

        // keypads tab
        int keypads_selected[2] {};
        char keypads_card_path[2][1024] {};
        std::thread *keypads_card_select = nullptr;
        bool keypads_card_select_done = false;
        ImGui::FileBrowser keypads_card_select_browser[2];
        char keypads_card_number[2][18] {};

        // patches tab
        std::unique_ptr<PatchManager> patch_manager;

        // options tab
        bool options_show_hidden = false;
        bool options_dirty = false;
        int options_category = 0;

    public:
        Config(SpiceOverlay *overlay);
        ~Config() override;

        void read_card(int player = -1);
        void write_card(int player);
        void build_content() override;
        void build_buttons(const std::string &name, std::vector<Button> *buttons,
                int min = 0, int max = -1);
        void build_analogs(const std::string &name, std::vector<Analog> *analogs);
        void build_lights(const std::string &name, std::vector<Light> *lights);
        void build_cards();
        void build_options(std::vector<Option> *options, const std::string &category);
        void build_about();
        void build_licenses();
        void build_launcher();

        static void build_page_selector(int *page);
        static void vertical_align_text_column();
    };
}

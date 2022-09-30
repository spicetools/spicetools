#include "config.h"

#include <random>
#include <thread>

#include <windows.h>
#include <commdlg.h>

#include "build/defs.h"
#include "build/resource.h"
#include "cfg/config.h"
#include "cfg/configurator.h"
#include "external/imgui/imgui_internal.h"
#include "games/io.h"
#include "avs/core.h"
#include "avs/ea3.h"
#include "avs/game.h"
#include "launcher/launcher.h"
#include "launcher/shutdown.h"
#include "misc/eamuse.h"
#include "overlay/imgui/extensions.h"
#include "rawinput/piuio.h"
#include "rawinput/rawinput.h"
#include "rawinput/touch.h"
#include "util/fileutils.h"
#include "util/logging.h"
#include "util/resutils.h"
#include "util/time.h"
#include "util/utils.h"

#ifdef min
#undef min
#endif

namespace overlay::windows {

    Config::Config(overlay::SpiceOverlay *overlay) : Window(overlay) {
        this->title = "Configuration";
        this->toggle_button = games::OverlayButtons::ToggleConfig;
        this->init_size = ImVec2(800, 600);
        this->size_min = ImVec2(100, 200);
        this->init_pos = ImVec2(0, 0);
        if (cfg::CONFIGURATOR_STANDALONE && cfg::CONFIGURATOR_TYPE == cfg::ConfigType::Config) {
            this->active = true;
            this->flags |= ImGuiWindowFlags_NoResize;
            this->flags |= ImGuiWindowFlags_NoMove;
            this->flags |= ImGuiWindowFlags_NoTitleBar;
            this->flags |= ImGuiWindowFlags_NoCollapse;
            this->flags |= ImGuiWindowFlags_NoDecoration;
        }

        // build game list
        auto &game_names = games::get_games();
        for (auto &game_name : game_names) {
            this->games_names.push_back(game_name.c_str());
            auto &game = this->games_list.emplace_back(game_name);
            auto buttons = games::get_buttons(game_name);
            auto analogs = games::get_analogs(game_name);
            auto lights = games::get_lights(game_name);
            if (buttons) {
                for (auto &item : *buttons) {
                    game.addItems(item);
                }
            }
            if (analogs) {
                for (auto &item : *analogs) {
                    game.addItems(item);
                }
            }
            if (lights) {
                for (auto &item : *lights) {
                    game.addItems(item);
                }
            }

            // default to currently running game
            if (!cfg::CONFIGURATOR_STANDALONE && game_name == eamuse_get_game()) {
                this->games_selected = games_list.size() - 1;
                this->games_selected_name = game_name;
            }

            // standalone configurator should look for file hints
            if (cfg::CONFIGURATOR_STANDALONE) {
                auto file_hints = games::get_game_file_hints(game_name);
                if (file_hints) {
                    for (auto &file_hint : *file_hints) {
                        if (fileutils::file_exists(file_hint) ||
                            fileutils::file_exists(std::filesystem::path("modules") / file_hint) ||
                            fileutils::file_exists(std::filesystem::path("contents") / file_hint) ||
                            fileutils::file_exists(MODULE_PATH / file_hint))
                        {
                            this->games_selected = games_list.size() - 1;
                            this->games_selected_name = game_name;
                            eamuse_set_game(game_name);
                        }
                    }
                }
            }
        }

        // configurator fallback to detected game name
        if (cfg::CONFIGURATOR_STANDALONE && this->games_selected == -1) {
            for (size_t i = 0; i < games_names.size(); i++) {
                if (games_names[i] == eamuse_get_game()) {
                    this->games_selected = i;
                }
            }
        }

        // add games to the config and window
        auto &config = ::Config::getInstance();
        for (auto &game : games_list) {
            config.addGame(game);

            if (!config.getStatus()) {
                log_warning("config", "failure adding game: {}", game.getGameName());
            }
        }

        // read card numbers
        read_card();
    }

    Config::~Config() {
    }

    void Config::read_card(int player) {

        // check if a game is selected
        if (this->games_selected_name.empty()) {
            return;
        }

        // iterate bindings
        auto bindings = ::Config::getInstance().getKeypadBindings(this->games_selected_name);
        for (int p = 0; p < 2; ++p) {
            if (player < 0 || player == p) {

                // get path
                std::filesystem::path path;
                if (!bindings.card_paths[p].empty()) {
                    path = bindings.card_paths[p];
                } else {
                    path = p > 0 ? "card1.txt" : "card0.txt";
                }

                // open file
                std::ifstream f(path);
                if (!f || !f.is_open()) {
                    this->keypads_card_number[p][0] = 0;
                    continue;
                }

                // get file size
                f.seekg(0, f.end);
                auto length = (size_t) f.tellg();
                f.seekg(0, f.beg);

                // read file contents
                f.read(this->keypads_card_number[p], 16);
                this->keypads_card_number[p][length < 16 ? length : 16] = 0;
                f.close();
            }
        }
    }

    void Config::write_card(int player) {

        // get path
        auto bindings = ::Config::getInstance().getKeypadBindings(this->games_selected_name);
        std::filesystem::path path;
        if (!bindings.card_paths[player].empty()) {
            path = bindings.card_paths[player];
        } else {
            path = player > 0 ? "card1.txt" : "card0.txt";
        }

        // write file
        std::ofstream f(path);
        if (f) {
            f.write(this->keypads_card_number[player], strlen(this->keypads_card_number[player]));
            f.close();
        }
    }

    void Config::build_content() {

        // if standalone then fullscreen window
        if (cfg::CONFIGURATOR_STANDALONE) {
            ImGui::SetWindowPos(ImVec2(0, 0));
            ImGui::SetWindowSize(ImGui::GetIO().DisplaySize);
        }

        // game selector
        int previous_games_selected = this->games_selected;
        ImGui::PushItemWidth(MAX(1, 463 - MAX(0, 580 - ImGui::GetWindowSize().x)));
        ImGui::Combo("Game Selection",
                &this->games_selected, games_names.data(),
                (int) games_list.size());
        ImGui::PopItemWidth();

        // remember selected game name
        if (this->games_selected >= 0 && this->games_selected < (int) games_list.size()) {
            this->games_selected_name = games_list.at(games_selected).getGameName();

            // standalone configurator applies selected game
            if (cfg::CONFIGURATOR_STANDALONE) {
                eamuse_set_game(games_selected_name);
            }

        } else {

            // invalid selection
            this->games_selected_name = "";
        }

        // display launcher if no game is selected
        if (this->games_selected_name.empty()) {
            this->build_launcher();
            return;
        }

        // selected game changed
        if (previous_games_selected != this->games_selected) {
            read_card();
        }

        // tab selection
        if (ImGui::BeginTabBar("Config Tabs", ImGuiTabBarFlags_NoCloseWithMiddleMouseButton)) {
            int page_offset = cfg::CONFIGURATOR_STANDALONE ? 92 : 110;
            int page_offset2 = cfg::CONFIGURATOR_STANDALONE ? 69 : 87;
            if (ImGui::BeginTabItem("Buttons")) {
                ImGui::BeginChild("Buttons", ImVec2(
                        0, ImGui::GetWindowContentRegionMax().y - page_offset), false);

                // game buttons
                this->build_buttons("Game", games::get_buttons(this->games_selected_name));

                // keypad buttons
                ImGui::TextUnformatted("");
                auto keypad_buttons = games::get_buttons_keypads(this->games_selected_name);
                auto keypad_count = eamuse_get_game_keypads_name();
                if (keypad_count == 1) {
                    this->build_buttons("Keypad", keypad_buttons,
                                        0, games::KeypadButtons::Size - 1);
                } else if (keypad_count >= 2) {
                    this->build_buttons("Keypad", keypad_buttons);
                }

                // overlay buttons
                ImGui::TextUnformatted("");
                this->build_buttons("Overlay", games::get_buttons_overlay(this->games_selected_name));
                ImGui::EndChild();

                // page selector
                this->build_page_selector(&this->buttons_page);

                // standalone configurator extras
                if (cfg::CONFIGURATOR_STANDALONE) {
                    ImGui::SameLine();
                    ImGui::Checkbox("Enable Overlay in Config", &OVERLAY->hotkeys_enable);
                }

                // bind many
                ImGui::SameLine();
                if (ImGui::Checkbox("Bind Many", &buttons_many_active)) {
                    buttons_many_index = -1;
                    buttons_many_delay = 0;
                }
                ImGui::SameLine();
                ImGui::HelpMarker("Immediately query for the next button after binding one.");

                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Analogs")) {
                ImGui::BeginChild("Analogs", ImVec2(
                    0, ImGui::GetWindowContentRegionMax().y - page_offset2), false);
                this->build_analogs("Game", games::get_analogs(this->games_selected_name));
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Lights")) {
                ImGui::BeginChild("Lights", ImVec2(
                        0, ImGui::GetWindowContentRegionMax().y - page_offset), false);
                this->build_lights("Game", games::get_lights(this->games_selected_name));
                ImGui::EndChild();
                this->build_page_selector(&this->lights_page);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Cards")) {
                ImGui::BeginChild("Cards", ImVec2(
                    0, ImGui::GetWindowContentRegionMax().y - page_offset2), false);
                this->build_cards();
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Patches")) {

                // initialization
                static std::once_flag initialized;
                static bool failure = false;
                std::call_once(initialized, [this] {
                    if (cfg::CONFIGURATOR_STANDALONE) {

                        // verify game is set, otherwise set failure flag
                        if (strlen(avs::game::MODEL) != 3
                        || (strlen(avs::game::DEST) != 1)
                        || (strlen(avs::game::SPEC) != 1)
                        || (strlen(avs::game::REV) != 1)
                        || (strlen(avs::game::EXT) != 10)
                        || (strcmp(avs::game::MODEL, "000") == 0)
                        || (strcmp(avs::game::EXT, "0000000000") == 0)) {
                            failure = true;
                        }
                    }
                });

                // display tab contents
                ImGui::BeginChild("Patches", ImVec2(
                        0, ImGui::GetWindowContentRegionMax().y - page_offset2), false);
                if (failure) {
                    ImGui::TextColored(ImVec4(0.7f, 0.f, 0.f, 1.f),
                            "Unable to detect the game version.\n"
                            "Try to open Patch Manager using the game overlay.");
                } else {

                    // allocate patch manager
                    if (!patch_manager) {
                        patch_manager.reset(new PatchManager(overlay));
                    }

                    // display patch manager
                    this->patch_manager->build_content();
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Options")) {
                ImGui::BeginChild("Options", ImVec2(
                        0, ImGui::GetWindowContentRegionMax().y - page_offset), false);

                // get options and categories
                auto options = games::get_options(this->games_selected_name);
                std::vector<const char *> categories;
                categories.push_back("Everything");
                if (options) {
                    for (auto &option : *options) {
                        bool found = false;
                        for (auto &category : categories) {
                            if (strcmp(option.get_definition().category.c_str(), category) == 0) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            categories.push_back(option.get_definition().category.c_str());
                        }
                    }
                }

                // get current category
                std::string category = "";
                if (this->options_category != -1 && this->options_category < (int) categories.size()) {
                    category = categories[this->options_category];
                    if (category == "Everything") {
                        category = "";
                    }
                }

                // options list
                this->build_options(options, category);
                ImGui::EndChild();

                // hidden options checkbox
                ImGui::Checkbox("Show Hidden Options", &this->options_show_hidden);
                if (!cfg::CONFIGURATOR_STANDALONE && this->options_dirty) {
                    ImGui::SameLine();
                    if (ImGui::Button("Restart Game")) {
                        launcher::restart();
                    }
                    ImGui::SameLine();
                    ImGui::HelpMarker("You need to restart the game to apply the changed settings.");
                }

                // reset configuration button
                ImGui::SameLine();
                if (ImGui::Button("Reset Configuration")) {
                    ImGui::OpenPopup("Reset Config");
                }
                if (ImGui::BeginPopupModal("Reset Config", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1.f),
                            "Do you really want to reset your configuration for all games?\n"
                            "Warning: This can't be reverted!");
                    if (ImGui::Button("Yes")) {
                        ::Config::getInstance().createConfigFile();
                        launcher::restart();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Nope")) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }

                // category selection
                ImGui::SameLine();
                ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.3f);
                ImGui::Combo("Category", &this->options_category, categories.data(), categories.size());
                ImGui::PopItemWidth();

                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("About")) {
                ImGui::BeginChild("About", ImVec2(
                    0, ImGui::GetWindowContentRegionMax().y - page_offset2), false);
                this->build_about();
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Licenses")) {
                ImGui::BeginChild("Licenses", ImVec2(
                    0, ImGui::GetWindowContentRegionMax().y - page_offset2), false);
                this->build_licenses();
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        // disclaimer
        ImGui::TextColored(
                ImVec4(1, 0.5f, 0.5f, 1.f),
                "Do NOT stream or upload game data anywhere public! Support arcades when you can. Thanks.");
    }

    void Config::build_buttons(const std::string &name, std::vector<Button> *buttons, int min, int max) {
        ImGui::Columns(3, "ButtonsColumns", true);
        ImGui::TextColored(ImVec4(1.f, 0.7f, 0, 1), "%s Button", name.c_str());
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(1.f, 0.7f, 0, 1), "Binding");
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(1.f, 0.7f, 0, 1), "Actions");
        ImGui::SameLine();
        ImGui::HelpMarker("Use 'Bind' to bind a button to a device using RawInput.\n"
                          "Use 'Naive' for device independent binding using GetAsyncKeyState.");
        ImGui::NextColumn();
        ImGui::Separator();

        // check if empty
        if (!buttons || buttons->empty()) {
            ImGui::TextUnformatted("-");
            ImGui::NextColumn();
            ImGui::TextUnformatted("-");
            ImGui::NextColumn();
            ImGui::TextUnformatted("-");
            ImGui::NextColumn();
        }

        // check buttons
        if (buttons) {
            int button_it_max = max < 0 ? buttons->size() - 1 : std::min((int) buttons->size() - 1, max);
            for (int button_it = min; button_it <= button_it_max; button_it++) {
                auto &button_in = buttons->at(button_it);

                // get button based on page
                auto button = &button_in;
                auto &button_alternatives = button->getAlternatives();
                if (this->buttons_page > 0) {
                    while ((int) button_alternatives.size() < this->buttons_page) {
                        button_alternatives.emplace_back(button->getName());
                    }
                    button = &button_alternatives.at(this->buttons_page - 1);
                }

                // get button info
                ImGui::PushID(button);
                auto button_name = button->getName();
                auto button_display = button->getDisplayString(RI_MGR.get());
                auto button_state = GameAPI::Buttons::getState(RI_MGR, *button);
                auto button_velocity = GameAPI::Buttons::getVelocity(RI_MGR, *button);

                // list entry
                bool style_color = false;
                if (button_state == GameAPI::Buttons::State::BUTTON_PRESSED) {
                    style_color = true;
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.7f, 0.f, 1.f));
                } else if (button_display.empty()) {
                    style_color = true;
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.f));
                }
                vertical_align_text_column();
                ImGui::Text("%s", button_name.c_str());
                ImGui::NextColumn();
                vertical_align_text_column();
                ImGui::Text("%s", button_display.empty() ? "None" : button_display.c_str());
                ImGui::NextColumn();
                if (style_color) {
                    ImGui::PopStyleColor();
                }

                // normal button binding
                std::string bind_name = "Bind " + button_name;
                if (ImGui::Button("Bind")
                || (buttons_many_active && !buttons_bind_active
                && !buttons_many_naive && buttons_many_index == button_it
                && ++buttons_many_delay > 60)) {
                    ImGui::OpenPopup(bind_name.c_str());
                    buttons_bind_active = true;
                    if (buttons_many_active) {
                        buttons_many_delay = 0;
                        buttons_many_index = button_it;
                        buttons_many_naive = false;
                    }

                    // midi freeze
                    RI_MGR->devices_midi_freeze(true);

                    // reset updated devices
                    RI_MGR->devices_get_updated();

                    // remember start values in bind data
                    for (auto device : RI_MGR->devices_get()) {
                        switch (device.type) {
                            case rawinput::MOUSE: {
                                memcpy(device.mouseInfo->key_states_bind,
                                        device.mouseInfo->key_states,
                                        sizeof(device.mouseInfo->key_states_bind));
                                break;
                            }
                            case rawinput::HID: {
                                for (size_t i = 0; i < device.hidInfo->value_states.size(); i++)
                                    device.hidInfo->bind_value_states[i] = device.hidInfo->value_states[i];
                                break;
                            }
                            case rawinput::MIDI: {
                                for (size_t i = 0; i < device.midiInfo->states.size(); i++)
                                    device.midiInfo->bind_states[i] = device.midiInfo->states[i];
                                for (size_t i = 0; i < device.midiInfo->controls_precision.size(); i++)
                                    device.midiInfo->controls_precision_bind[i] =
                                            device.midiInfo->controls_precision[i];
                                for (size_t i = 0; i < device.midiInfo->controls_single.size(); i++)
                                    device.midiInfo->controls_single_bind[i] = device.midiInfo->controls_single[i];
                                for (size_t i = 0; i < device.midiInfo->controls_onoff.size(); i++)
                                    device.midiInfo->controls_onoff_bind[i] = device.midiInfo->controls_onoff[i];
                                break;
                            }
                            default:
                                break;
                        }
                    }
                }
                if (ImGui::BeginPopupModal(bind_name.c_str(), NULL,
                        ImGuiWindowFlags_AlwaysAutoResize)) {

                    // modal content
                    ImGui::Text("Please press any button.");
                    ImGui::TextColored(ImVec4(1, 0.7f, 0, 1), "Hint: Press ESC to cancel!");
                    if (ImGui::Button("Cancel")) {
                        RI_MGR->devices_midi_freeze(false);
                        buttons_bind_active = false;
                        buttons_many_index = -1;
                        ImGui::CloseCurrentPopup();
                    } else {

                        // iterate updated devices
                        auto updated_devices = RI_MGR->devices_get_updated();
                        for (auto device : updated_devices) {
                            std::lock_guard<std::mutex> lock(*device->mutex);
                            switch (device->type) {
                                case rawinput::MOUSE: {
                                    auto mouse = device->mouseInfo;
                                    for (size_t i = 0; i < sizeof(mouse->key_states_bind); i++) {
                                        if (mouse->key_states[i] && !mouse->key_states_bind[i]
                                        && !ImGui::IsItemHovered()) {

                                            // bind key
                                            button->setDeviceIdentifier(device->name);
                                            button->setVKey(static_cast<unsigned short>(i));
                                            button->setAnalogType(BAT_NONE);
                                            ::Config::getInstance().updateBinding(
                                                    games_list[games_selected], *button, buttons_page - 1);
                                            ImGui::CloseCurrentPopup();
                                            buttons_bind_active = false;
                                            ++buttons_many_index;
                                            RI_MGR->devices_midi_freeze(false);
                                            break;
                                        }
                                    }
                                    break;
                                }
                                case rawinput::KEYBOARD: {
                                    auto kb = device->keyboardInfo;
                                    for (unsigned short vkey = 0; vkey < 1024; vkey++) {

                                        // check if key is down
                                        if (vkey != VK_NUMLOCK && kb->key_states[vkey]) {

                                            // cancel on escape key
                                            if (vkey == VK_ESCAPE) {
                                                ImGui::CloseCurrentPopup();
                                                buttons_bind_active = false;
                                                buttons_many_index = -1;
                                                RI_MGR->devices_midi_freeze(false);
                                                break;
                                            }

                                            // bind key
                                            button->setDeviceIdentifier(device->name);
                                            button->setVKey(vkey);
                                            button->setAnalogType(BAT_NONE);
                                            ::Config::getInstance().updateBinding(
                                                    games_list[games_selected], *button, buttons_page - 1);
                                            ImGui::CloseCurrentPopup();
                                            buttons_bind_active = false;
                                            ++buttons_many_index;
                                            RI_MGR->devices_midi_freeze(false);
                                            break;
                                        }
                                    }
                                    break;
                                }
                                case rawinput::HID: {
                                    auto hid = device->hidInfo;

                                    // ignore touchscreen button inputs
                                    if (!rawinput::touch::is_touchscreen(device)) {

                                        // button caps
                                        auto button_states_list = &hid->button_states;
                                        size_t button_index = 0;
                                        for (auto &button_states : *button_states_list) {
                                            for (size_t i = 0; i < button_states.size(); i++) {

                                                // check if button is down
                                                if (button_states[i]) {

                                                    // bind key
                                                    button->setDeviceIdentifier(device->name);
                                                    button->setVKey(static_cast<unsigned short>(button_index + i));
                                                    button->setAnalogType(BAT_NONE);
                                                    ::Config::getInstance().updateBinding(
                                                            games_list[games_selected], *button,
                                                            buttons_page - 1);
                                                    ImGui::CloseCurrentPopup();
                                                    buttons_bind_active = false;
                                                    ++buttons_many_index;
                                                    RI_MGR->devices_midi_freeze(false);
                                                    break;
                                                }
                                            }
                                            button_index += button_states.size();
                                        }
                                    }

                                    // value caps
                                    auto value_states = &hid->value_states;
                                    auto bind_value_states = &hid->bind_value_states;
                                    auto value_names = &hid->value_caps_names;
                                    for (size_t i = 0; i < value_states->size(); i++) {
                                        auto &state = value_states->at(i);
                                        auto &bind_state = bind_value_states->at(i);
                                        auto &value_name = value_names->at(i);

                                        // check for valid axis names
                                        if (value_name == "X" ||
                                            value_name == "Y" ||
                                            value_name == "Rx" ||
                                            value_name == "Ry" ||
                                            value_name == "Z")
                                        {
                                            // check if axis is in activation area
                                            float normalized = (state - 0.5f) * 2.f;
                                            float diff = std::fabs(state - bind_state);
                                            if (std::fabs(normalized) > 0.9f && diff > 0.1f) {
                                                auto bat = normalized > 0 ? BAT_POSITIVE : BAT_NEGATIVE;

                                                // bind value
                                                button->setDeviceIdentifier(device->name);
                                                button->setVKey(static_cast<unsigned short>(i));
                                                button->setAnalogType(bat);
                                                button->setDebounceUp(0.0);
                                                button->setDebounceDown(0.0);
                                                ::Config::getInstance().updateBinding(
                                                        games_list[games_selected], *button,
                                                        buttons_page - 1);
                                                ImGui::CloseCurrentPopup();
                                                buttons_bind_active = false;
                                                ++buttons_many_index;
                                                RI_MGR->devices_midi_freeze(false);
                                                break;

                                            } else if (diff > 0.3f) {
                                                bind_state = state;
                                            }
                                        }

                                        // hat switch
                                        if (value_name == "Hat switch") {

                                            // get hat switch values
                                            ButtonAnalogType buffer[3], buffer_bind[3];
                                            Button::getHatSwitchValues(state, buffer);
                                            Button::getHatSwitchValues(bind_state, buffer_bind);

                                            // check the first entry only
                                            if (buffer[0] != BAT_NONE && buffer[0] != buffer_bind[0]) {

                                                // bind value
                                                button->setDeviceIdentifier(device->name);
                                                button->setVKey(static_cast<unsigned short>(i));
                                                button->setAnalogType(buffer[0]);
                                                button->setDebounceUp(0.0);
                                                button->setDebounceDown(0.0);
                                                ::Config::getInstance().updateBinding(
                                                        games_list[games_selected], *button,
                                                        buttons_page - 1);
                                                ImGui::CloseCurrentPopup();
                                                buttons_bind_active = false;
                                                ++buttons_many_index;
                                                RI_MGR->devices_midi_freeze(false);
                                                break;
                                            }
                                        }
                                    }
                                    break;
                                }
                                case rawinput::MIDI: {
                                    auto midi = device->midiInfo;

                                    // iterate all 128 notes on 16 channels
                                    for (unsigned short index = 0; index < 16 * 128; index++) {

                                        // check if note is down
                                        if (midi->states[index]) {

                                            // check if it wasn't down before
                                            if (!midi->bind_states[index]) {

                                                // bind key
                                                button->setDeviceIdentifier(device->name);
                                                button->setVKey(index);
                                                button->setAnalogType(BAT_NONE);
                                                button->setDebounceUp(0.0);
                                                button->setDebounceDown(0.0);
                                                ::Config::getInstance().updateBinding(
                                                        games_list[games_selected], *button,
                                                        buttons_page - 1);
                                                ImGui::CloseCurrentPopup();
                                                buttons_bind_active = false;
                                                ++buttons_many_index;
                                                RI_MGR->devices_midi_freeze(false);
                                                break;
                                            }

                                        } else {

                                            // note was on when dialog opened, is now off
                                            midi->bind_states[index] = false;
                                        }
                                    }

                                    // check precision controls
                                    for (unsigned short index = 0; index < 16 * 32; index++) {
                                        if (midi->controls_precision[index] > 0) {
                                            if (midi->controls_precision_bind[index] == 0) {

                                                // bind control
                                                button->setDeviceIdentifier(device->name);
                                                button->setVKey(index);
                                                button->setAnalogType(BAT_MIDI_CTRL_PRECISION);
                                                button->setDebounceUp(0.0);
                                                button->setDebounceDown(0.0);
                                                ::Config::getInstance().updateBinding(
                                                        games_list[games_selected], *button,
                                                        buttons_page - 1);
                                                ImGui::CloseCurrentPopup();
                                                buttons_bind_active = false;
                                                ++buttons_many_index;
                                                RI_MGR->devices_midi_freeze(false);
                                                break;
                                            }
                                        } else {
                                            midi->controls_precision_bind[index] = 0;
                                        }
                                    }

                                    // check single controls
                                    for (unsigned short index = 0; index < 16 * 44; index++) {
                                        if (midi->controls_single[index] > 0) {
                                            if (midi->controls_single_bind[index] == 0) {

                                                // bind control
                                                button->setDeviceIdentifier(device->name);
                                                button->setVKey(index);
                                                button->setAnalogType(BAT_MIDI_CTRL_SINGLE);
                                                button->setDebounceUp(0.0);
                                                button->setDebounceDown(0.0);
                                                ::Config::getInstance().updateBinding(
                                                        games_list[games_selected], *button,
                                                        buttons_page - 1);
                                                ImGui::CloseCurrentPopup();
                                                buttons_bind_active = false;
                                                ++buttons_many_index;
                                                RI_MGR->devices_midi_freeze(false);
                                                break;
                                            }
                                        } else {
                                            midi->controls_single_bind[index] = 0;
                                        }
                                    }

                                    // check on/off controls
                                    for (unsigned short index = 0; index < 16 * 6; index++) {
                                        if (midi->controls_onoff[index]) {
                                            if (!midi->controls_onoff_bind[index]) {

                                                // bind control
                                                button->setDeviceIdentifier(device->name);
                                                button->setVKey(index);
                                                button->setAnalogType(BAT_MIDI_CTRL_ONOFF);
                                                button->setDebounceUp(0.0);
                                                button->setDebounceDown(0.0);
                                                ::Config::getInstance().updateBinding(
                                                        games_list[games_selected], *button,
                                                        buttons_page - 1);
                                                ImGui::CloseCurrentPopup();
                                                buttons_bind_active = false;
                                                ++buttons_many_index;
                                                RI_MGR->devices_midi_freeze(false);
                                                break;
                                            }
                                        } else {
                                            midi->controls_onoff_bind[index] = 0;
                                        }
                                    }

                                    // check pitch bend down
                                    if (midi->pitch_bend < 0x2000) {

                                        // bind control
                                        button->setDeviceIdentifier(device->name);
                                        button->setVKey(0);
                                        button->setAnalogType(BAT_MIDI_PITCH_DOWN);
                                        button->setDebounceUp(0.0);
                                        button->setDebounceDown(0.0);
                                        ::Config::getInstance().updateBinding(
                                                games_list[games_selected], *button,
                                                buttons_page - 1);
                                        ImGui::CloseCurrentPopup();
                                        buttons_bind_active = false;
                                        ++buttons_many_index;
                                        RI_MGR->devices_midi_freeze(false);
                                        break;
                                    }

                                    // check pitch bend up
                                    if (midi->pitch_bend > 0x2000) {

                                        // bind control
                                        button->setDeviceIdentifier(device->name);
                                        button->setVKey(0);
                                        button->setAnalogType(BAT_MIDI_PITCH_UP);
                                        button->setDebounceUp(0.0);
                                        button->setDebounceDown(0.0);
                                        ::Config::getInstance().updateBinding(
                                                games_list[games_selected], *button,
                                                buttons_page - 1);
                                        ImGui::CloseCurrentPopup();
                                        buttons_bind_active = false;
                                        ++buttons_many_index;
                                        RI_MGR->devices_midi_freeze(false);
                                        break;
                                    }

                                    break;
                                }
                                case rawinput::PIUIO_DEVICE: {
                                    auto piuio_dev = device->piuioDev;

                                    // iterate all PIUIO inputs
                                    for (int i = 0; i < rawinput::PIUIO::PIUIO_MAX_NUM_OF_INPUTS; i++) {

                                        // check for down event
                                        if (piuio_dev->IsPressed(i) && !piuio_dev->WasPressed(i)) {

                                            // bind key
                                            button->setDeviceIdentifier(device->name);
                                            button->setVKey(i);
                                            button->setAnalogType(BAT_NONE);
                                            button->setDebounceUp(0.0);
                                            button->setDebounceDown(0.0);
                                            ::Config::getInstance().updateBinding(
                                                    games_list[games_selected], *button,
                                                    buttons_page - 1);
                                            ImGui::CloseCurrentPopup();
                                            buttons_bind_active = false;
                                            ++buttons_many_index;
                                            RI_MGR->devices_midi_freeze(false);
                                            break;
                                        }
                                    }

                                    break;
                                }
                                default:
                                    break;
                            }
                        }
                    }

                    // clean up
                    ImGui::EndPopup();
                }

                // naive binding
                ImGui::SameLine();
                std::string naive_string = "Naive " + button_name;
                if (ImGui::Button("Naive")
                || (buttons_many_active && !buttons_bind_active
                    && buttons_many_naive && buttons_many_index == button_it
                    && ++buttons_many_delay > 60)) {
                    ImGui::OpenPopup(naive_string.c_str());
                    if (buttons_many_active) {
                        buttons_many_index = button_it;
                        buttons_many_naive = true;
                        buttons_many_delay = 0;
                    }

                    // grab current keyboard state
                    for (unsigned short int i = 0x01; i < 0xFF; i++) {
                        buttons_keyboard_state[i] = GetAsyncKeyState(i) != 0;
                    }
                }
                if (ImGui::BeginPopupModal(naive_string.c_str(), NULL,
                        ImGuiWindowFlags_AlwaysAutoResize)) {
                    buttons_bind_active = true;

                    // modal content
                    ImGui::Text("Please press any button.");
                    ImGui::TextColored(ImVec4(1, 0.7f, 0, 1), "Hint: Press ESC to cancel!");
                    if (ImGui::Button("Cancel")) {
                        buttons_bind_active = false;
                        buttons_many_index = -1;
                        ImGui::CloseCurrentPopup();
                    } else {

                        // get new keyboard state
                        bool keyboard_state_new[sizeof(buttons_keyboard_state)];
                        for (size_t i = 0; i < sizeof(keyboard_state_new); i++) {
                            keyboard_state_new[i] = GetAsyncKeyState(i) != 0;
                        }

                        // detect key presses
                        for (unsigned short int vKey = 0x01; vKey < sizeof(buttons_keyboard_state); vKey++) {

                            // ignore num lock escape sequence
                            if (vKey != VK_NUMLOCK) {

                                // check for key press
                                if (keyboard_state_new[vKey] && !buttons_keyboard_state[vKey]) {

                                    // ignore escape
                                    if (vKey != VK_ESCAPE && (vKey != VK_LBUTTON || !ImGui::IsItemHovered())) {

                                        // bind key
                                        button->setDeviceIdentifier("");
                                        button->setVKey(vKey);
                                        button->setDebounceUp(0.0);
                                        button->setDebounceDown(0.0);
                                        ::Config::getInstance().updateBinding(
                                                games_list[games_selected], *button,
                                                buttons_page - 1);
                                        ++buttons_many_index;
                                    } else {
                                        buttons_many_index = -1;
                                    }

                                    buttons_bind_active = false;
                                    ImGui::CloseCurrentPopup();
                                }
                            }
                        }

                        // clean up
                        memcpy(buttons_keyboard_state, keyboard_state_new, sizeof(buttons_keyboard_state));
                    }
                    ImGui::EndPopup();
                }

                // edit button
                ImGui::SameLine();
                std::string edit_name = "Edit " + button->getName();
                if (ImGui::Button("Edit")) {
                    ImGui::OpenPopup(edit_name.c_str());
                }
                if (ImGui::BeginPopupModal(edit_name.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                    bool dirty = false;

                    // binding
                    ImGui::Text("Binding");

                    // device identifier
                    auto device_id = button->getDeviceIdentifier();
                    if (device_id.empty()) device_id = "Empty (Naive)";
                    if (ImGui::BeginCombo("Device Identifier", device_id.c_str())) {
                        bool empty_selected = device_id == "Empty (Naive)";
                        if (ImGui::Selectable("Empty (Naive)", empty_selected)) {
                            button->setDeviceIdentifier("");
                            dirty = true;
                        }
                        if (empty_selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                        for (auto &device : RI_MGR->devices_get()) {
                            bool selected = device_id == device.desc.c_str();
                            if (ImGui::Selectable(device.desc.c_str(), selected)) {
                                button->setDeviceIdentifier(device.desc);
                                dirty = true;
                            }
                            if (selected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }

                    // analog type
                    auto bat = button->getAnalogType();
                    if (ImGui::BeginCombo("Analog Type", ButtonAnalogTypeStr[bat])) {
                        for (int i = 0; i <= 16; i++) {
                            bool selected = (int) bat == i;
                            if (ImGui::Selectable(ButtonAnalogTypeStr[i], selected)) {
                                button->setAnalogType((ButtonAnalogType) i);
                                dirty = true;
                            }
                            if (selected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }

                    // virtual key
                    int vKey = button->getVKey();
                    if (ImGui::InputInt(device_id == "Empty (Naive)" ? "Virtual Key" : "Index", &vKey, 1, 1)) {
                        button->setVKey(vKey);
                        dirty = true;
                    }

                    // preview
                    if (!button_display.empty()) {
                        ImGui::TextUnformatted("\nPreview");
                        ImGui::Separator();
                        ImGui::Text("%s\n\n", button_display.c_str());
                    } else {
                        ImGui::TextUnformatted("");
                    }

                    // options
                    ImGui::Text("Options");

                    // check for debounce
                    auto device = RI_MGR->devices_get(button->getDeviceIdentifier());
                    if (button->getDebounceUp() || button->getDebounceDown()
                    || (device != nullptr && (
                            device->type == rawinput::MOUSE ||
                            device->type == rawinput::KEYBOARD ||
                            (device->type == rawinput::HID && button->getAnalogType() == BAT_NONE)
                    ))) {

                        // debounce up
                        auto debounce_up = button->getDebounceUp() * 1000;
                        if (ImGui::InputDouble("Debounce Up (ms)", &debounce_up, 1, 1, "%.2f")) {
                            debounce_up = std::max(0.0, debounce_up);
                            button->setDebounceUp(debounce_up * 0.001);
                            dirty = true;
                        }
                        ImGui::SameLine();
                        ImGui::HelpMarker("Time a button needs to be up to be detected as up.\n"
                                          "Can solve micro switch issues with long notes for example.");

                        // debounce down
                        auto debounce_down = button->getDebounceDown() * 1000;
                        if (ImGui::InputDouble("Debounce Down (ms)", &debounce_down, 1, 1, "%.2f")) {
                            debounce_down = std::max(0.0, debounce_down);
                            button->setDebounceDown(debounce_down * 0.001);
                            dirty = true;
                        }
                        ImGui::SameLine();
                        ImGui::HelpMarker("Time a button needs to be down to be detected as down.\n"
                                          "This setting will add noticable input lag.");
                    }

                    // invert
                    bool invert = button->getInvert();
                    if (ImGui::Checkbox("Invert", &invert)) {
                        button->setInvert(invert);
                        dirty = true;
                    }

                    // state display
                    ImGui::TextUnformatted("");
                    ImGui::Text("State");
                    ImGui::ProgressBar(button_velocity);

                    // check if dirty
                    if (dirty) {
                        ::Config::getInstance().updateBinding(
                                games_list[games_selected], *button, buttons_page - 1);
                    }

                    // close button
                    ImGui::TextUnformatted("");
                    if (ImGui::Button("Close")) {
                        buttons_many_active = false;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }

                // clear button
                if (button_display.size() > 0) {
                    ImGui::SameLine();
                    if (ImGui::Button("Clear")) {
                        button->setDeviceIdentifier("");
                        button->setVKey(0xFF);
                        button->setAnalogType(BAT_NONE);
                        button->setDebounceUp(0.0);
                        button->setDebounceDown(0.0);
                        button->setInvert(false);
                        button->setLastState(GameAPI::Buttons::BUTTON_NOT_PRESSED);
                        button->setLastVelocity(0);
                        ::Config::getInstance().updateBinding(
                                games_list[games_selected], *button,
                                buttons_page - 1);
                    }
                }

                // clean up
                ImGui::NextColumn();
                ImGui::PopID();
            }
        }
        ImGui::Columns();
    }

    void Config::build_analogs(const std::string &name, std::vector<Analog> *analogs) {
        ImGui::Columns(3, "AnalogsColumns", true);
        ImGui::TextColored(ImVec4(1.f, 0.7f, 0, 1), "%s Analog", name.c_str()); ImGui::NextColumn();
        ImGui::TextColored(ImVec4(1.f, 0.7f, 0, 1), "Binding"); ImGui::NextColumn();
        ImGui::TextColored(ImVec4(1.f, 0.7f, 0, 1), "Actions"); ImGui::NextColumn();
        ImGui::Separator();

        // check if empty
        if (!analogs || analogs->empty()) {
            ImGui::TextUnformatted("-");
            ImGui::NextColumn();
            ImGui::TextUnformatted("-");
            ImGui::NextColumn();
            ImGui::TextUnformatted("-");
            ImGui::NextColumn();
        }

        // check analogs
        if (analogs) {
            for (auto &analog : *analogs) {

                // get analog info
                ImGui::PushID(&analog);
                auto analog_name = analog.getName();
                auto analog_display = analog.getDisplayString(RI_MGR.get());
                auto analog_state = GameAPI::Analogs::getState(RI_MGR, analog);

                // list entry
                ImGui::ProgressBar(analog_state, ImVec2(32.f, 0));
                ImGui::SameLine();
                if (analog_display.empty()) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.f));
                }
                ImGui::Text("%s", analog_name.c_str());
                ImGui::NextColumn();
                vertical_align_text_column();
                ImGui::Text("%s", analog_display.empty() ? "None" : analog_display.c_str());
                ImGui::NextColumn();
                if (analog_display.empty()) {
                    ImGui::PopStyleColor();
                }

                // analog binding
                if (ImGui::Button("Bind")) {
                    ImGui::OpenPopup("Analog Binding");

                    // get devices
                    this->analogs_devices.clear();
                    for (auto &device : RI_MGR->devices_get()) {
                        switch (device.type) {
                            case rawinput::MOUSE:
                                this->analogs_devices.emplace_back(&device);
                                break;
                            case rawinput::HID:
                                if (!device.hidInfo->value_caps_names.empty())
                                    this->analogs_devices.emplace_back(&device);
                                break;
                            case rawinput::MIDI:
                                this->analogs_devices.emplace_back(&device);
                                break;
                            default:
                                continue;
                        }

                        // check if this is the current device
                        if (device.name == analog.getDeviceIdentifier()) {
                            analogs_devices_selected = this->analogs_devices.size() - 1;
                        }
                    }
                }
                if (ImGui::BeginPopupModal("Analog Binding", NULL,
                        ImGuiWindowFlags_AlwaysAutoResize)) {

                    // device selector
                    auto analog_device_changed = ImGui::Combo(
                        "Device",
                        &this->analogs_devices_selected,
                        [](void* data, int i, const char **item) {
                            *item = ((std::vector<rawinput::Device*>*) data)->at(i)->desc.c_str();
                            return true;
                        },
                        &this->analogs_devices, (int) this->analogs_devices.size());

                    // obtain controls
                    std::vector<std::string> control_names;
                    std::vector<int> analogs_midi_indices;
                    if (this->analogs_devices_selected >= 0) {
                        auto device = this->analogs_devices.at(this->analogs_devices_selected);
                        switch (device->type) {
                            case rawinput::MOUSE: {

                                // add X/Y axis and mouse wheel
                                control_names.push_back("X");
                                control_names.push_back("Y");
                                control_names.push_back("Scroll Wheel");
                                break;
                            }
                            case rawinput::HID: {

                                // add value names
                                for (auto &analog_name : device->hidInfo->value_caps_names) {
                                    control_names.push_back(analog_name);
                                }
                                break;
                            }
                            case rawinput::MIDI: {

                                // add pitch bend
                                control_names.push_back("Pitch Bend");
                                analogs_midi_indices.push_back(device->midiInfo->controls_precision.size()
                                                               + device->midiInfo->controls_single.size()
                                                               + device->midiInfo->controls_onoff.size()
                                );

                                // add precision values
                                auto precision = device->midiInfo->controls_precision;
                                for (size_t i = 0; i < precision.size(); i++) {
                                    if (device->midiInfo->controls_precision_set[i]) {
                                        control_names.push_back("PREC 0x" + bin2hex((uint8_t) i));
                                        analogs_midi_indices.push_back(i);
                                    }
                                }

                                // add single values
                                auto single = device->midiInfo->controls_single;
                                for (size_t i = 0; i < single.size(); i++) {
                                    if (device->midiInfo->controls_single_set[i]) {
                                        control_names.push_back("CTRL 0x" + bin2hex((uint8_t) i));
                                        analogs_midi_indices.push_back(i + device->midiInfo->controls_precision.size());
                                    }
                                }

                                // add onoff values
                                auto onoff = device->midiInfo->controls_onoff;
                                for (size_t i = 0; i < onoff.size(); i++) {
                                    if (device->midiInfo->controls_onoff_set[i]) {
                                        control_names.push_back("ONOFF 0x" + bin2hex((uint8_t) i));
                                        analogs_midi_indices.push_back(i
                                                                       + device->midiInfo->controls_precision.size()
                                                                       + device->midiInfo->controls_single.size());
                                    }
                                }
                            }
                            default:
                                break;
                        }

                        // select the previously chosen value
                        auto selected_control = 0;
                        if (!analog_device_changed) {
                            if (analogs_midi_indices.empty()) {
                                selected_control = analog.getIndex();
                            } else {
                                for (size_t i = 0; i < analogs_midi_indices.size(); i++) {
                                    if (analog.getIndex() == analogs_midi_indices.at(i)) {
                                        selected_control = i;
                                        break;
                                    }
                                }
                            }

                            if (0 <= selected_control && selected_control < static_cast<int>(control_names.size())) {
                                this->analogs_devices_control_selected = selected_control;
                            }
                        }
                    }

                    // controls
                    ImGui::Combo("Control",
                                 &this->analogs_devices_control_selected,
                                 [](void* data, int i, const char **item) {
                                     *item = ((std::vector<std::string>*) data)->at(i).c_str();
                                     return true;
                                 },
                                 &control_names, control_names.size());

                    // sensitivity/deadzone
                    if (this->analogs_devices_selected >= 0) {
                        auto device = this->analogs_devices.at(this->analogs_devices_selected);
                        if (device->type == rawinput::MOUSE || device->type == rawinput::HID) {
                            auto sensitivity = sqrtf(analog.getSensitivity());
                            ImGui::SliderFloat("Sensitivity", &sensitivity,
                                    0.f, 2.f, "%.3f", 1.f);
                            if (analog.getSensitivity() != sensitivity) {
                                analog.setSensitivity(sensitivity * sensitivity);
                                ::Config::getInstance().updateBinding(
                                        games_list[games_selected], analog);
                            }
                        }
                        if (device->type == rawinput::HID || device->type == rawinput::MIDI) {
                            auto deadzone = analog.getDeadzone();
                            ImGui::SliderFloat("Deadzone", &deadzone,
                                               -0.999f, 0.999f, "%.3f", 1.f);
                            if (analog.getDeadzone() != deadzone) {
                                analog.setDeadzone(deadzone);
                                ::Config::getInstance().updateBinding(
                                        games_list[games_selected], analog);
                            }
                            ImGui::SameLine();
                            ImGui::HelpMarker("Positive values specify a deadzone around the middle.\n"
                                              "Negative values specify a deadzone from the minimum value.");

                            // deadzone mirror
                            bool deadzone_mirror = analog.getDeadzoneMirror();
                            ImGui::Checkbox("Deadzone Mirror", &deadzone_mirror);
                            ImGui::SameLine();
                            ImGui::HelpMarker("Positive deadzone values cut off at edges instead.\n"
                                              "Negative deadzone values cut off at maximum value instead.");
                            if (deadzone_mirror != analog.getDeadzoneMirror()) {
                                analog.setDeadzoneMirror(deadzone_mirror);
                                ::Config::getInstance().updateBinding(
                                        games_list[games_selected], analog);
                            }
                        }
                    }

                    // invert axis
                    bool invert = analog.getInvert();
                    ImGui::Checkbox("Invert Axis", &invert);
                    if (invert != analog.getInvert()) {
                        analog.setInvert(invert);
                        ::Config::getInstance().updateBinding(
                                games_list[games_selected], analog);
                    }
                    
                    // smoothing
                    if (this->analogs_devices_selected >= 0) {
                        auto device = this->analogs_devices.at(this->analogs_devices_selected);
                        if (device->type == rawinput::HID) {
                            bool smoothing = analog.getSmoothing();
                            ImGui::Checkbox("Smooth Axis (adds latency)", &smoothing);
                            if (smoothing != analog.getSmoothing()) {
                                analog.setSmoothing(smoothing);
                                ::Config::getInstance().updateBinding(
                                        games_list[games_selected], analog);
                            }
                        }
                    }

                    // current state
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(1.f, 0.7f, 0.f, 1.f), "Preview");
                    float value = GameAPI::Analogs::getState(RI_MGR, analog);
                    ImGui::ProgressBar(value);

                    // centered knob preview
                    const float knob_size = 64.f;
                    auto width = ImGui::GetContentRegionAvail().x - knob_size;
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width / 2));
                    ImGui::Knob(value, knob_size);

                    // update analog
                    if (analogs_devices_selected >= 0 && analogs_devices_selected < (int) analogs_devices.size()) {

                        // update identifier on change
                        auto identifier = this->analogs_devices.at(this->analogs_devices_selected)->name;
                        if (identifier != analog.getDeviceIdentifier()) {
                            analog.setDeviceIdentifier(identifier);
                            ::Config::getInstance().updateBinding(
                                    games_list[games_selected], analog);
                        }

                        // update control
                        if (this->analogs_devices_control_selected >= 0) {

                            // MIDI devices have their own dynamic indices
                            auto index = this->analogs_devices_control_selected;
                            if (!analogs_midi_indices.empty()) {
                                if (this->analogs_devices_control_selected < (int) analogs_midi_indices.size()) {
                                    index = analogs_midi_indices[this->analogs_devices_control_selected];
                                }
                            }

                            // update index on change
                            if ((int) analog.getIndex() != index) {
                                analog.setIndex(index);
                                ::Config::getInstance().updateBinding(
                                        games_list[games_selected], analog);
                            }
                        }
                    }

                    // close button
                    ImGui::Separator();
                    if (ImGui::Button("Close")) {
                        ImGui::CloseCurrentPopup();
                    }

                    // clean up
                    ImGui::EndPopup();
                }

                // clear analog
                if (analog_display.size() > 0) {
                    ImGui::SameLine();
                    if (ImGui::Button("Clear")) {
                        analog.clearBindings();
                        analog.setLastState(0.f);
                        ::Config::getInstance().updateBinding(
                                games_list[games_selected], analog);
                    }
                }

                // clean up
                ImGui::NextColumn();
                ImGui::PopID();
            }
        }
        ImGui::Columns();
    }

    void Config::build_lights(const std::string &name, std::vector<Light> *lights) {
        ImGui::Columns(3, "LightsColumns", true);
        ImGui::TextColored(ImVec4(1.f, 0.7f, 0, 1), "Name"); ImGui::NextColumn();
        ImGui::TextColored(ImVec4(1.f, 0.7f, 0, 1), "Binding"); ImGui::NextColumn();
        ImGui::TextColored(ImVec4(1.f, 0.7f, 0, 1), "Actions"); ImGui::SameLine();
        ImGui::HelpMarker("Use 'Bind' to redirect cabinet light outputs to HID-compatible value output devices.");
        ImGui::NextColumn();
        ImGui::Separator();

        // check if empty
        if (!lights || lights->empty()) {
            ImGui::TextUnformatted("-");
            ImGui::NextColumn();
            ImGui::TextUnformatted("-");
            ImGui::NextColumn();
            ImGui::TextUnformatted("-");
            ImGui::NextColumn();
        }

        // check lights
        if (lights) {
            for (auto &light_in : *lights) {

                // get light based on page
                auto light = &light_in;
                auto &light_alternatives = light->getAlternatives();
                if (this->lights_page > 0) {
                    while ((int) light_alternatives.size() < this->lights_page) {
                        light_alternatives.emplace_back(light->getName());
                    }
                    light = &light_alternatives.at(this->lights_page - 1);
                }

                // get light info
                auto light_name = light->getName();
                auto light_display = light->getDisplayString(RI_MGR.get());
                auto light_state = GameAPI::Lights::readLight(RI_MGR, *light);

                // list entry
                if (light_display.empty()) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.f));
                }
                ImGui::PushID(light);
                ImGui::ProgressBar(light_state, ImVec2(32.f, 0));
                ImGui::SameLine();
                ImGui::Text("%s", light_name.c_str());
                ImGui::NextColumn();
                vertical_align_text_column();
                ImGui::Text("%s", light_display.empty() ? "None" : light_display.c_str());
                ImGui::NextColumn();
                if (light_display.empty()) {
                    ImGui::PopStyleColor();
                }

                // light binding
                if (ImGui::Button("Bind")) {
                    ImGui::OpenPopup("Light Binding");
                    light->override_enabled = true;

                    // get devices
                    this->lights_devices.clear();
                    for (auto &device : RI_MGR->devices_get()) {
                        switch (device.type) {
                            case rawinput::HID:
                                if (!device.hidInfo->button_output_caps_list.empty()
                                    || !device.hidInfo->value_output_caps_list.empty())
                                    this->lights_devices.emplace_back(&device);
                                break;
                            case rawinput::SEXTET_OUTPUT:
                            case rawinput::PIUIO_DEVICE:
                                this->lights_devices.emplace_back(&device);
                                break;
                            default:
                                continue;
                        }

                        // check if this is the current device
                        if (device.name == light->getDeviceIdentifier()) {
                            this->lights_devices_selected = (int) this->lights_devices.size() - 1;
                            this->lights_devices_control_selected = light->getIndex();
                        }
                    }
                }
                if (ImGui::BeginPopupModal("Light Binding", NULL,
                        ImGuiWindowFlags_AlwaysAutoResize)) {

                    // device selector
                    bool control_changed = false;
                    if (ImGui::Combo("Device",
                                     &this->lights_devices_selected,
                                     [] (void* data, int i, const char **item) {
                                         *item = ((std::vector<rawinput::Device*>*) data)->at(i)->desc.c_str();
                                         return true;
                                     },
                                     &this->lights_devices, (int) this->lights_devices.size())) {
                        this->lights_devices_control_selected = 0;
                        control_changed = true;
                    }

                    // obtain controls
                    std::vector<std::string> control_names;
                    if (lights_devices_selected >= 0 && lights_devices_selected < (int) lights_devices.size()) {
                        auto device = lights_devices[lights_devices_selected];
                        switch (device->type) {
                            case rawinput::HID: {
                                size_t index = 0;

                                // add button names
                                for (auto &button_name : device->hidInfo->button_output_caps_names) {

                                    // build name
                                    std::string name = button_name;
                                    if (index > 0xFF)
                                        name += " (0x" + bin2hex(&((char*) &index)[1], 1) + bin2hex(&((char*) &index)[0], 1) + ")";
                                    else
                                        name += " (0x" + bin2hex(&((char*) &index)[0], 1) + ")";

                                    // add name
                                    control_names.push_back(name);
                                    index++;
                                }

                                // add value names
                                for (auto &value_name : device->hidInfo->value_output_caps_names) {

                                    // build name
                                    std::string name = value_name;
                                    if (index > 0xFF)
                                        name += " (0x" + bin2hex(&((char*) &index)[1], 1)
                                                + bin2hex(&((char*) &index)[0], 1)
                                                + ", value cap)";
                                    else
                                        name += " (0x" + bin2hex(&((char*) &index)[0], 1) + ", value cap)";

                                    // add name
                                    control_names.push_back(name);
                                    index++;
                                }

                                break;
                            }
                            case rawinput::SEXTET_OUTPUT: {

                                // add all names of sextet device
                                for (int i = 0; i < rawinput::SextetDevice::LIGHT_COUNT; i++) {
                                    std::string name(rawinput::SextetDevice::LIGHT_NAMES[i]);

                                    // add name
                                    control_names.push_back(name);
                                }
                                break;
                            }
                            case rawinput::PIUIO_DEVICE: {

                                // add all names of PIUIO device
                                for (int i = 0; i < rawinput::PIUIO::PIUIO_MAX_NUM_OF_LIGHTS; i++) {
                                    std::string name(rawinput::PIUIO::LIGHT_NAMES[i]);

                                    // add name
                                    control_names.push_back(name);
                                }
                                break;
                            }
                            default:
                                break;
                        }
                    }

                    // controls
                    if (ImGui::Combo("Light Control",
                                 &this->lights_devices_control_selected,
                                 [] (void* data, int i, const char **item) {
                                     *item = ((std::vector<std::string> *) data)->at(i).c_str();
                                     return true;
                                 },
                                 &control_names, control_names.size())) {
                        control_changed = true;
                    }

                    // update light
                    if (lights_devices_selected >= 0 && lights_devices_selected < (int) lights_devices.size()) {
                        auto identifier = this->lights_devices[lights_devices_selected]->name;
                        if (identifier != light->getDeviceIdentifier()) {
                            light->setDeviceIdentifier(identifier);
                            ::Config::getInstance().updateBinding(
                                    games_list[games_selected], *light,
                                    lights_page - 1);
                        }
                        if (this->lights_devices_control_selected >= 0) {
                            if ((int) light->getIndex() != this->lights_devices_control_selected) {
                                light->setIndex(this->lights_devices_control_selected);
                                ::Config::getInstance().updateBinding(
                                        games_list[games_selected], *light,
                                        lights_page - 1);
                            }
                        }
                    }

                    // value preview
                    ImGui::Separator();
                    float value_orig = GameAPI::Lights::readLight(RI_MGR, *light);
                    float value = value_orig;
                    ImGui::SliderFloat("Preview", &value, 0.f, 1.f);

                    // manual button controls
                    if (ImGui::Button("Turn On")) {
                        value = 1.f;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Turn Off")) {
                        value = 0.f;
                    }

                    // manual lock
                    if (!cfg::CONFIGURATOR_STANDALONE) {
                        ImGui::SameLine();
                        ImGui::Checkbox("Lock", &light->override_enabled);
                    }

                    // apply new value
                    if (value != value_orig || control_changed) {
                        if (light->override_enabled) {
                            light->override_state = value;
                        }
                        auto ident = light->getDeviceIdentifier();
                        GameAPI::Lights::writeLight(RI_MGR->devices_get(ident), light->getIndex(), value);
                        RI_MGR->devices_flush_output();
                    }

                    // close button
                    ImGui::Separator();
                    if (ImGui::Button("Close")) {
                        ImGui::CloseCurrentPopup();
                        light->override_enabled = false;
                    }

                    // clean up
                    ImGui::EndPopup();
                }

                // clear light
                if (light_display.size() > 0) {
                    ImGui::SameLine();
                    if (ImGui::Button("Clear")) {
                        light->setDeviceIdentifier("");
                        light->setIndex(0xFF);
                        ::Config::getInstance().updateBinding(
                                games_list[games_selected], *light,
                                lights_page - 1);
                    }
                }

                // clean up
                ImGui::NextColumn();
                ImGui::PopID();
            }
        }
        ImGui::Columns();
    }

    void Config::build_cards() {

        // early quit
        if (this->games_selected < 0 || this->games_selected_name.empty()) {
            ImGui::Text("Please select a game first.");
            return;
        }

        // get bindings and copy paths
        auto game = games_list[games_selected];
        auto bindings = ::Config::getInstance().getKeypadBindings(this->games_selected_name);
        bool bindings_updated = false;
        for (int player = 0; player < 2; player++) {
            strncpy(this->keypads_card_path[player],
                    bindings.card_paths[player].string().c_str(),
                    sizeof(this->keypads_card_path[0]) - 1);
            this->keypads_card_path[player][sizeof(this->keypads_card_path[0]) - 1] = '\0';
        }

        // card settings for each player
        for (int player = 0; player < 2; player++) {

            // custom ID and title
            ImGui::PushID(("KeypadP" + to_string(player)).c_str());
            ImGui::TextColored(ImVec4(1, 0.7f, 0, 1), "Player %i", player + 1);

            // card path
            std::string hint = "card" + to_string(player) + ".txt";
            if (ImGui::InputTextWithHint("Card Path", hint.c_str(),
                    this->keypads_card_path[player], sizeof(this->keypads_card_path[0]) - 1))
            {
                bindings.card_paths[player] = this->keypads_card_path[player];
                bindings_updated = true;
            }

            // help marker
            ImGui::SameLine();
            ImGui::HelpMarker("Leave this empty to use the card file in your game directory.\n"
                              "Hint: You can place 'card0.txt' (P1) / 'card1.txt' (P2) into the root of your USB "
                              "flash drive and it will trigger a card insert when you connect it!");

            // card path file selector
            ImGui::SameLine();
            if (ImGui::Button("...")) {

                // standalone version opens native file browser
                if (cfg::CONFIGURATOR_STANDALONE && !keypads_card_select) {

                    // run in separate thread otherwise we get a crash
                    keypads_card_select_done = false;
                    keypads_card_select = new std::thread([this, bindings, player, game] {

                        // open dialog to get path
                        auto ofn_path = std::make_unique<wchar_t[]>(512);
                        OPENFILENAMEW ofn {};
                        memset(&ofn, 0, sizeof(ofn));
                        ofn.lStructSize = sizeof(ofn);
                        ofn.hwndOwner = nullptr;
                        ofn.lpstrFilter = L"";
                        ofn.lpstrFile = ofn_path.get();
                        ofn.nMaxFile = 512;
                        ofn.Flags = OFN_EXPLORER;
                        ofn.lpstrDefExt = L"txt";

                        // check for success
                        if (GetSaveFileNameW(&ofn)) {

                            // update card path
                            auto new_bindings = bindings;
                            new_bindings.card_paths[player] = std::filesystem::path(ofn_path.get());
                            ::Config::getInstance().updateBinding(game, new_bindings);
                            eamuse_update_keypad_bindings();

                            read_card(player);
                        } else {
                            auto error = CommDlgExtendedError();
                            if (error) {
                                log_warning("cfg", "failed to get save file name: {}", error);
                            } else {
                                log_warning("cfg", "failed to get save file name");
                            }
                        }

                        // clean up
                        keypads_card_select_done = true;
                    });
                }

                // in-game version opens ImGui file browser
                if (!cfg::CONFIGURATOR_STANDALONE && !this->keypads_card_select_browser[player].IsOpened()) {
                    this->keypads_card_select_browser[player].SetTitle("Card Select");
                    this->keypads_card_select_browser[player].SetTypeFilters({".txt", "*"});
                    this->keypads_card_select_browser[player].flags_ |= ImGuiFileBrowserFlags_EnterNewFilename;
                    this->keypads_card_select_browser[player].Open();
                }
            }

            // clear button
            if (!bindings.card_paths[player].empty()) {
                ImGui::SameLine();
                if (ImGui::Button("Clear")) {
                    bindings.card_paths[player] = "";
                    bindings_updated = true;
                }
            }

            // verify card number
            auto card_valid = true;
            if (this->keypads_card_number[player][0] != 0) {
                for (int n = 0; n < 16; n++) {
                    char c = this->keypads_card_number[player][n];
                    bool digit = c >= '0' && c <= '9';
                    bool character_big = c >= 'A' && c <= 'F';
                    bool character_small = c >= 'a' && c <= 'f';
                    if (!digit && !character_big && !character_small) {
                        card_valid = false;
                    }
                }
            }

            // card number box
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  card_valid ? ImVec4(1.f, 1.f, 1.f, 1.f) :
                                  ImVec4(1.f, 0.f, 0.f, 1.f));
            ImGui::InputTextWithHint("Card Number", "E004000000000000",
                    this->keypads_card_number[player], sizeof(this->keypads_card_number[0]) - 1,
                    ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_CharsHexadecimal);
            ImGui::PopStyleColor();

            // write card after edit
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                write_card(player);
                read_card(1 - player);
            }

            // generate button
            ImGui::SameLine();
            if (ImGui::Button("Generate")) {

                // create random
                std::random_device rd;
                std::mt19937 generator(rd());
                std::uniform_int_distribution<> uniform(0, 15);

                // randomize card
                strcpy(this->keypads_card_number[player], "E004");
                char hex[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
                for (int i = 4; i < 16; i++) {
                    this->keypads_card_number[player][i] = hex[uniform(generator)];
                }

                // terminate and flush
                this->keypads_card_number[player][16] = 0;
                write_card(player);
                read_card(1 - player);
            }

            // render card select browser
            this->keypads_card_select_browser[player].Display();
            if (this->keypads_card_select_browser[player].HasSelected()) {
                auto selected = keypads_card_select_browser[player].GetSelected();
                this->keypads_card_select_browser[player].ClearSelected();
                bindings.card_paths[player] = selected;
                bindings_updated = true;
            }

            // clean up thread when needed
            if (keypads_card_select_done) {
                keypads_card_select_done = false;
                keypads_card_select->join();
                delete keypads_card_select;
                keypads_card_select = nullptr;
            }

            // clean up
            ImGui::PopID();
            ImGui::Separator();
        }

        // check for binding update and save
        if (bindings_updated) {
            ::Config::getInstance().updateBinding(game, bindings);
            eamuse_update_keypad_bindings();
            read_card();
        }
    }

    void Config::build_options(std::vector<Option> *options, const std::string &category) {
        ImGui::Columns(3, "OptionsColumns", true);
        ImGui::TextColored(ImVec4(1.f, 0.7f, 0, 1), "Title"); ImGui::NextColumn();
        ImGui::TextColored(ImVec4(1.f, 0.7f, 0, 1), "Parameter");
        ImGui::SameLine();
        ImGui::HelpMarker("These are the command-line parameters you can use in your .bat file to set the options.\n"
                          "Example: spice.exe -w -ea\n"
                          "         spice64.exe -api 1337 -apipass changeme");
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(1.f, 0.7f, 0, 1), "Setting"); ImGui::NextColumn();
        ImGui::Separator();

        // check if empty
        if (!options || options->empty()) {
            ImGui::TextUnformatted("-");
            ImGui::NextColumn();
            ImGui::TextUnformatted("-");
            ImGui::NextColumn();
            ImGui::TextUnformatted("-");
            ImGui::NextColumn();
        }

        // iterate options
        if (options) {
            for (auto &option : *options) {

                // get option definition
                auto &definition = option.get_definition();

                // check category
                if (!category.empty() && definition.category != category) {
                    continue;
                }

                // check hidden option
                if (!this->options_show_hidden && option.value.empty()) {

                    // skip hidden entries
                    if (definition.hidden) {
                        continue;
                    }

                    // check for game exclusivity
                    if (!definition.game_name.empty()) {
                        if (definition.game_name != this->games_selected_name) {
                            continue;
                        }
                    }
                }

                // list entry
                ImGui::PushID(&option);
                vertical_align_text_column();
                ImGui::HelpMarker(definition.desc.c_str());
                ImGui::SameLine();
                if (option.is_active()) {
                    if (option.disabled) {
                        ImGui::TextColored(ImVec4(1.f, 0.4f, 0.f, 1.f), "%s", definition.title.c_str());
                    } else {
                        ImGui::TextColored(ImVec4(1.f, 0.7f, 0.f, 1.f), "%s", definition.title.c_str());
                    }
                } else if (definition.hidden
                || (!definition.game_name.empty() && definition.game_name != this->games_selected_name)) {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "%s", definition.title.c_str());
                } else if (definition.game_name == this->games_selected_name) {
                    ImGui::TextColored(ImVec4(0.8f, 0, 0.8f, 1.f), "%s", definition.title.c_str());
                } else {
                    ImGui::Text("%s", definition.title.c_str());
                }
                ImGui::NextColumn();
                vertical_align_text_column();
                ImGui::Text("-%s", definition.name.c_str());
                ImGui::NextColumn();
                if (option.disabled) {
                    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                }
                switch (definition.type) {
                    case OptionType::Bool: {
                        bool state = !option.value.empty();
                        if (ImGui::Checkbox(state ? "Enabled" : "Disabled", &state)) {
                            this->options_dirty = true;
                            option.value = state ? "/ENABLED" : "";
                            ::Config::getInstance().updateBinding(games_list[games_selected], option);
                        }
                        break;
                    }
                    case OptionType::Integer: {
                        char buffer[512];
                        strncpy(buffer, option.value.c_str(), sizeof(buffer) - 1);
                        buffer[sizeof(buffer) - 1] = '\0';                        
                        auto digits_filter = [](ImGuiInputTextCallbackData* data) {
                            if ('0' <= data->EventChar && data->EventChar <= '9') {
                                return 0;
                            }
                            return 1;
                        };

                        const char *hint = definition.setting_name.empty() ? "Enter number..."
                                : definition.setting_name.c_str();

                        if (ImGui::InputTextWithHint("", hint,
                                buffer, sizeof(buffer) - 1,
                                ImGuiInputTextFlags_CallbackCharFilter, digits_filter)) {
                            this->options_dirty = true;
                            option.value = buffer;
                            ::Config::getInstance().updateBinding(games_list[games_selected], option);
                        }
                        break;
                    }
                    case OptionType::Text: {
                        char buffer[512];
                        strncpy(buffer, option.value.c_str(), sizeof(buffer) - 1);
                        buffer[sizeof(buffer) - 1] = '\0';

                        const char *hint = definition.setting_name.empty() ? "Enter value..."
                                : definition.setting_name.c_str();

                        if (ImGui::InputTextWithHint("", hint, buffer, sizeof(buffer) - 1)) {
                            this->options_dirty = true;
                            option.value = buffer;
                            ::Config::getInstance().updateBinding(games_list[games_selected], option);
                        }
                        break;
                    }
                    case OptionType::Enum: {
                        std::string current_item = option.value_text();
                        for (auto &element : definition.elements) {
                            if (element.first == current_item) {
                                current_item += fmt::format(" ({})", element.second);
                            }
                        }
                        if (current_item.empty()) {
                            current_item = "Default";
                        }
                        if (ImGui::BeginCombo("##combo", current_item.c_str(), 0)) {
                            for (auto &element : definition.elements) {
                                bool selected = current_item == element.first;
                                std::string label = element.first;
                                if (!element.second.empty()) {
                                    label += fmt::format(" ({})", element.second);
                                }
                                if (ImGui::Selectable(label.c_str(), selected)) {
                                    this->options_dirty = true;
                                    option.value = element.first;
                                    ::Config::getInstance().updateBinding(games_list[games_selected], option);
                                }
                                if (selected) {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }
                            ImGui::EndCombo();
                        }
                        break;
                    }
                    default: {
                        ImGui::Text("Unknown option type");
                        break;
                    }
                }

                // clear button
                if (!option.disabled && option.is_active() && option.get_definition().type != OptionType::Bool) {
                    ImGui::SameLine();
                    if (ImGui::Button("Clear")) {
                        this->options_dirty = true;
                        option.value = "";
                        ::Config::getInstance().updateBinding(games_list[games_selected], option);
                    }
                }

                // clean up disabled item flags
                if (option.disabled) {
                    ImGui::PopItemFlag();
                    ImGui::PopStyleVar();
                }

                // disabled help
                if (option.disabled) {
                    ImGui::SameLine();
                    ImGui::HelpMarker("This option can not be edited because it was overriden by command-line "
                                      "options.\n"
                                      "Run spicecfg.exe to configure the options and then run spice(64).exe directly.");
                }

                // next item
                ImGui::PopID();
                ImGui::NextColumn();
            }
        }
    }

    void Config::build_about() {
        auto time = get_system_milliseconds();
        auto value = ((float) cos(time * 0.0005) + 1.f) * 0.5f;
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f - value * 0.3f, 1.f, 1.f, 1.f));
        ImGui::TextUnformatted(std::string(
                                      "SpiceTools\r\n"
                                      "=========================\r\n" +
                                      to_string(VERSION_STRING) + "\r\n\r\n" + to_string(
                                      resutil::load_file_string_crlf(IDR_README)) +
                                      "\r\n" +
                                      "Changelog (Highlights):\r\n" +
                                      resutil::load_file_string_crlf(IDR_CHANGELOG)).c_str());
        ImGui::PopStyleColor();
    }

    void Config::build_licenses() {
        auto time = get_system_milliseconds();
        auto value = ((float) cos(time * 0.0005) + 1.f) * 0.5f;
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 0.9f - value * 0.3f, 1.f));
        ImGui::TextUnformatted(resutil::load_file_string_crlf(IDR_LICENSES).c_str());
        ImGui::PopStyleColor();
    }

    void Config::build_launcher() {
        ImGui::TextUnformatted("Please select a game!");
        ImGui::Separator();
        ImGui::BeginChild("Launcher");
        this->build_about();
        ImGui::EndChild();
    }

    void Config::build_page_selector(int *page) {

        // left page selector
        if (ImGui::Button("<")) {
            *page = *page - 1;
            if (*page < 0) {
                *page = 99;
            }
        }

        // page display and reset
        ImGui::SameLine();
        if (ImGui::Button(("Page " + to_string(*page)).c_str())) {
            *page = 0;
        }

        // right page selector
        ImGui::SameLine();
        if (ImGui::Button(">")) {
            *page = *page + 1;
            if (*page > 99) {
                *page = 0;
            }
        }

        // help
        ImGui::SameLine();
        ImGui::HelpMarker("You can bind buttons/lights to multiple inputs/outputs "
                          "at the same time using pages.");
    }

    void Config::vertical_align_text_column() {
        /*
         * see https://github.com/ocornut/imgui/issues/1284
         * height of most widgets (like Button) is GetFontSize() + GetStyle().FramePadding.y * 2
         * height of unformatted text is GetFontSize()
         * by default, rows are top-aligned
         * so we add FramePadding.y to the top of unformatted text for vertical centering. 
         */
        auto y = ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y;
        ImGui::SetCursorPosY(y);
    }
}

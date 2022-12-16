#include <games/io.h>

#include "screen_resize.h"
#include "cfg/screen_resize.h"
#include "misc/eamuse.h"
#include "util/logging.h"

namespace overlay::windows {

    ScreenResize::ScreenResize(SpiceOverlay *overlay) : Window(overlay) {
        this->title = "Screen Resize";
        this->flags = ImGuiWindowFlags_AlwaysAutoResize;
        this->init_pos = ImVec2(
                ImGui::GetIO().DisplaySize.x / 2 - this->init_size.x / 2,
                ImGui::GetIO().DisplaySize.y / 2 - this->init_size.y / 2);
        this->toggle_button = games::OverlayButtons::ToggleScreenResize;
        this->toggle_screen_resize = games::OverlayButtons::ScreenResize;
    }

    ScreenResize::~ScreenResize() {
    }

    void ScreenResize::build_content() {

        // enable checkbox
        ImGui::Checkbox("Enable Screen Resize", &cfg::SCREENRESIZE->enable_screen_resize);
        if (cfg::SCREENRESIZE->enable_screen_resize) {

            // general settings
        	ImGui::Checkbox("Enable Linear Filter", &cfg::SCREENRESIZE->enable_linear_filter);
        	ImGui::InputInt("X Offset", &cfg::SCREENRESIZE->offset_x);
        	ImGui::InputInt("Y Offset", &cfg::SCREENRESIZE->offset_y);

        	// aspect ratio
        	ImGui::Checkbox("Keep Aspect Ratio", &cfg::SCREENRESIZE->keep_aspect_ratio);
        	if (cfg::SCREENRESIZE->keep_aspect_ratio) {				
        		if (ImGui::SliderFloat("Scale", &cfg::SCREENRESIZE->scale_x, 0.65f, 2.0f)) {
                    cfg::SCREENRESIZE->scale_y = cfg::SCREENRESIZE->scale_x;
                }
        	} else {
        		ImGui::SliderFloat("Width Scale", &cfg::SCREENRESIZE->scale_x, 0.65f, 2.0f);
        		ImGui::SliderFloat("Height Scale", &cfg::SCREENRESIZE->scale_y, 0.65f, 2.0f);
        	}

        	// reset button
        	if (ImGui::Button("Reset")) {
        		cfg::SCREENRESIZE->offset_x = 0;
        		cfg::SCREENRESIZE->offset_y = 0;
        		cfg::SCREENRESIZE->scale_x = 1;
        		cfg::SCREENRESIZE->scale_y = 1;
        	}
        }

        // load button
        ImGui::SameLine();
        if (ImGui::Button("Load")) {
            cfg::SCREENRESIZE->config_load();
        }

        // save button
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            cfg::SCREENRESIZE->config_save();
        }
    }

    void ScreenResize::update() {
        Window::update();
        if (this->toggle_screen_resize != ~0u) {
            auto overlay_buttons = games::get_buttons_overlay(eamuse_get_game());
            bool toggle_screen_resize_new = overlay_buttons
                && this->overlay->hotkeys_triggered()
                && GameAPI::Buttons::getState(RI_MGR, overlay_buttons->at(this->toggle_screen_resize));
            
            if (toggle_screen_resize_new && !this->toggle_screen_resize_state) {
                cfg::SCREENRESIZE->enable_screen_resize = !cfg::SCREENRESIZE->enable_screen_resize;
            }
            this->toggle_screen_resize_state = toggle_screen_resize_new;
        }
    }
}

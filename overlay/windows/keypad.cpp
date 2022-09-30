#include <games/io.h>
#include "keypad.h"

#include "misc/eamuse.h"
#include "util/logging.h"

namespace overlay::windows {

    Keypad::Keypad(SpiceOverlay *overlay, size_t unit) : Window(overlay), unit(unit) {
        this->title = "Keypad P" + to_string(unit + 1);
        this->flags = ImGuiWindowFlags_NoResize
                    | ImGuiWindowFlags_NoCollapse
                    | ImGuiWindowFlags_AlwaysAutoResize;

        switch (this->unit) {
            case 0: {
                this->toggle_button = games::OverlayButtons::ToggleVirtualKeypadP1;
                this->init_pos = ImVec2(
                        26,
                        ImGui::GetIO().DisplaySize.y - 264);
                break;
            }
            case 1: {
                this->toggle_button = games::OverlayButtons::ToggleVirtualKeypadP2;
                this->init_pos = ImVec2(
                        ImGui::GetIO().DisplaySize.x - 220,
                        ImGui::GetIO().DisplaySize.y - 264);
                break;
            }
        }
    }

    Keypad::~Keypad() {

        // reset overrides
        eamuse_set_keypad_overrides_overlay(this->unit, 0);
    }

    void Keypad::build_content() {

        // buttons
        static const struct {
            const char *text;
            int flag;
        } BUTTONS[] = {
            { "7", 1 << EAM_IO_KEYPAD_7 },
            { "8", 1 << EAM_IO_KEYPAD_8 },
            { "9", 1 << EAM_IO_KEYPAD_9 },
            { "4", 1 << EAM_IO_KEYPAD_4 },
            { "5", 1 << EAM_IO_KEYPAD_5 },
            { "6", 1 << EAM_IO_KEYPAD_6 },
            { "1", 1 << EAM_IO_KEYPAD_1 },
            { "2", 1 << EAM_IO_KEYPAD_2 },
            { "3", 1 << EAM_IO_KEYPAD_3 },
            { "0", 1 << EAM_IO_KEYPAD_0 },
            { "00", 1 << EAM_IO_KEYPAD_00 },
            { ".", 1 << EAM_IO_KEYPAD_DECIMAL },
            { "Insert Card", 1 << EAM_IO_INSERT },
        };

        // reset overrides
        eamuse_set_keypad_overrides_overlay(this->unit, 0);

        // build grid
        for (size_t i = 0; i < std::size(BUTTONS); i++) {
            auto &button = BUTTONS[i];

            // push id and alignment
            ImGui::PushID(4096 + i);
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));

            // add selectable (fill last line)
            if (i == std::size(BUTTONS) - 1) {
                ImGui::Selectable(button.text, false, 0, ImVec2(112, 32));
            } else {
                ImGui::Selectable(button.text, false, 0, ImVec2(32, 32));
            }

            // mouse down handler
            if (ImGui::IsItemHovered() && ImGui::IsAnyMouseDown()) {
                eamuse_set_keypad_overrides_overlay(this->unit, button.flag);
            }

            // pop id and alignment
            ImGui::PopStyleVar();
            ImGui::PopID();

            // line join
            if ((i % 3) < 2) {
                ImGui::SameLine();
            }
        }
    }
}

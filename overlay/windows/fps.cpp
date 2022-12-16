#include "fps.h"

namespace overlay::windows {

    FPS::FPS(SpiceOverlay *overlay) : Window(overlay) {
        this->title = "FPS & Frame Time";
        this->flags = ImGuiWindowFlags_NoTitleBar
                      | ImGuiWindowFlags_NoCollapse
                      | ImGuiWindowFlags_NoResize
                      | ImGuiWindowFlags_AlwaysAutoResize
                      | ImGuiWindowFlags_NoDecoration
                      | ImGuiWindowFlags_NoFocusOnAppearing
                      | ImGuiWindowFlags_NoNavFocus
                      | ImGuiWindowFlags_NoNavInputs;
        this->bg_alpha = 0.4f;
    }

    const ImVec2 FPS::initial_pos() {
        return ImVec2(ImGui::GetIO().DisplaySize.x - 100, 10);
    }

    void FPS::build_content() {

        // frame timers
        ImGuiIO &io = ImGui::GetIO();
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::Text("FT: %.2fms", 1000 / io.Framerate);
    }
}

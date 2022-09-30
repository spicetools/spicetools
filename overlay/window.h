#pragma once

#include <string>

#include "external/imgui/imgui.h"
#include "overlay.h"

namespace overlay {

    class Window {
    public:
        virtual ~Window();
        virtual const ImVec2 initial_pos() {
            return this->init_pos;
        }
        virtual void build_content() = 0;
        virtual void update();
        virtual void after_render() {};

        void build();
        void toggle_active();
        void set_active(bool active);
        bool get_active();

    protected:

        // state
        SpiceOverlay *overlay;
        bool active = false;
        std::vector<Window*> children;

        // settings
        bool draws_window = true;
        std::string title = "Title";
        ImGuiWindowFlags flags = 0;
        size_t toggle_button = ~0u;
        bool toggle_button_state = false;

        // init settings
        ImVec2 init_pos = ImVec2(0, 0);
        ImVec2 init_size = ImVec2(0, 0);
        ImVec2 size_min = ImVec2(0, 0);
        ImVec2 size_max = ImVec2(-1, -1);
        float bg_alpha = 0.8f;

        Window(SpiceOverlay *overlay);
    };
}

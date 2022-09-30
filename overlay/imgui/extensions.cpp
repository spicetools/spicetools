#include "extensions.h"
#include <cmath>

#include "external/imgui/imgui.h"


namespace ImGui {

    void HelpMarker(const char* desc) {
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    void Knob(float fraction, float size, float thickness, float pos_x, float pos_y) {

        // get values
        auto radius = size * 0.5f;
        auto pos = ImGui::GetCursorScreenPos();
        if (pos_x >= 0) pos.x = pos_x;
        if (pos_y >= 0) pos.y = pos_y;
        auto center = ImVec2(pos.x + radius, pos.y + radius);
        auto draw_list = ImGui::GetWindowDrawList();

        // dummy for spacing knob with other content
        if (pos_x < 0 && pos_y < 0) {
            ImGui::Dummy(ImVec2(size, size));
        }

        // draw knob
        auto angle = (fraction + 0.25f) * (3.141592f * 2);
        draw_list->AddCircleFilled(center, radius, ImGui::GetColorU32(ImGuiCol_FrameBg), 16);
        draw_list->AddLine(center,
                ImVec2(center.x + cosf(angle) * radius, center.y + sinf(angle) * radius),
                ImGui::GetColorU32(ImGuiCol_PlotHistogram),
                thickness);
    }
}

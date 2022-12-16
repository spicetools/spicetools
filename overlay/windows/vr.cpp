#include "vr.h"
#include "misc/vrutil.h"
#include "util/logging.h"
#include "games/io.h"
#include "games/drs/drs.h"
#include "avs/game.h"
#include "overlay/imgui/extensions.h"

namespace overlay::windows {

    VRWindow::VRWindow(SpiceOverlay *overlay) : Window(overlay) {
        this->title = "VR";
        this->flags = ImGuiWindowFlags_None;
        this->toggle_button = games::OverlayButtons::ToggleVRControl;
        this->init_size = ImVec2(500, 800);
        this->size_min = ImVec2(250, 200);
    }

    VRWindow::~VRWindow() {
    }

    void VRWindow::build_content() {
        ImGui::BeginTabBar("VRTabBar");
        if (ImGui::BeginTabItem("Info")) {
            build_info();
            ImGui::EndTabItem();
        }
        if (avs::game::is_model("REC")) {
            if (ImGui::BeginTabItem("Dancefloor")) {
                build_dancefloor();
            }
        }
    }

    void VRWindow::build_info() {

        // status
        auto status = vrutil::STATUS;
        switch (status) {
            case vrutil::VRStatus::Disabled:
                ImGui::TextColored(
                        ImVec4(0.4f, 0.4f, 0.4f, 1.f),
                        "Disabled");
                if (ImGui::Button("Start")) {
                    vrutil::init();
                    if (avs::game::is_model("REC")) {
                        games::drs::start_vr();
                    }
                }
                break;
            case vrutil::VRStatus::Error:
                ImGui::TextColored(
                        ImVec4(0.8f, 0.1f, 0.1f, 1.f),
                        "Error");
                if (ImGui::Button("Restart")) {
                    vrutil::shutdown();
                    vrutil::init();
                }
                break;
            case vrutil::VRStatus::Running:
                ImGui::TextColored(
                        ImVec4(0.1f, 0.8f, 0.1f, 1.f),
                        "Running");
                if (ImGui::Button("Stop")) {
                    vrutil::shutdown();
                }
                break;
        }

        // rescan
        if (ImGui::Button("Rescan Devices")) {
            vrutil::scan(true);
        }

        // data table header
        ImGui::Columns(2);
        ImGui::Text("Device");
        ImGui::NextColumn();
        ImGui::Text("Position");
        ImGui::NextColumn();
        ImGui::Separator();

        // HMD/Left/Right data
        vr::TrackedDevicePose_t hmd_pose, left_pose, right_pose;
        vr::VRControllerState_t left_state, right_state;
        vrutil::get_hmd_pose(&hmd_pose);
        vrutil::get_con_pose(vrutil::INDEX_LEFT, &left_pose, &left_state);
        vrutil::get_con_pose(vrutil::INDEX_RIGHT, &right_pose, &right_state);
        auto hmd_pos = vrutil::get_translation(hmd_pose.mDeviceToAbsoluteTracking);
        auto left_pos = vrutil::get_translation(left_pose.mDeviceToAbsoluteTracking);
        auto right_pos = vrutil::get_translation(right_pose.mDeviceToAbsoluteTracking);
        ImGui::Text("HMD");
        ImGui::NextColumn();
        ImGui::TextUnformatted(fmt::format(
                "X={:3f} Y={:3f} Z={:3f}",
                hmd_pos.x, hmd_pos.y, hmd_pos.z).c_str());
        ImGui::NextColumn();
        ImGui::Text("Left");
        ImGui::NextColumn();
        ImGui::TextUnformatted(fmt::format(
                "X={:3f} Y={:3f} Z={:3f}",
                left_pos.x, left_pos.y, left_pos.z).c_str());
        ImGui::NextColumn();
        ImGui::Text("Right");
        ImGui::NextColumn();
        ImGui::TextUnformatted(fmt::format(
                "X={:3f} Y={:3f} Z={:3f}",
                right_pos.x, right_pos.y, right_pos.z).c_str());
        ImGui::NextColumn();
    }

    void VRWindow::build_dancefloor() {
        ImGui::Separator();

        // settings
        ImGui::DragFloat3("Scale", &games::drs::VR_SCALE[0], 0.1f);
        ImGui::DragFloat3("Offset", &games::drs::VR_OFFSET[0], 0.1f);
        ImGui::DragFloat("Rotation", &games::drs::VR_ROTATION, 0.5f);
        for (int i = 0; i < (int) std::size(games::drs::VR_FOOTS); ++i) {
            auto &foot = games::drs::VR_FOOTS[i];
            ImGui::Separator();
            ImGui::PushID(&foot);
            ImGui::Text("%s Foot", i == 0 ? "Left" : "Right");
            ImGui::InputInt("Device Index", (int*) &foot.index, 1, 1);
            ImGui::DragFloat("Length", &foot.length,
                             0.005f, 0.001f, 1000.f);
            ImGui::DragFloat("Size Base", &foot.size_base,
                             0.005f, 0.001f, 1000.f);
            ImGui::DragFloat("Size Scale", &foot.size_scale,
                             0.005f, 0.001f, 1000.f);
            ImGui::DragFloat4("Rotation Quat", &foot.rotation.x, 0.001f, -1, 1);
            if (ImGui::Button("Calibrate")) {
                vr::TrackedDevicePose_t pose;
                vr::VRControllerState_t state;
                vrutil::get_con_pose(foot.get_index(), &pose, &state);
                foot.length = foot.height + 0.02f;
                auto pose_rot = vrutil::get_rotation(pose.mDeviceToAbsoluteTracking.m);
                foot.rotation = linalg::qmul(linalg::qinv(pose_rot),
                        vrutil::get_rotation((float) M_PI * -0.5f, 0, 0));
            }
            ImGui::SameLine();
            ImGui::HelpMarker("Place the controller to the lower part of your leg "
                              "and press this button to auto calibrate angle and length");
            ImGui::PopID();
        }

        // prepare view
        auto draw_list = ImGui::GetWindowDrawList();
        auto canvas_pos = ImGui::GetCursorScreenPos();
        auto canvas_size = ImGui::GetContentRegionAvail();
        float offset_x = canvas_size.x * 0.5f;
        float offset_y = canvas_size.y * 0.1f;
        float off_x = offset_x + canvas_pos.x;
        float off_y = offset_y + canvas_pos.y;
        float scale = std::min(canvas_size.x, canvas_size.y) / 60;

        // axis
        draw_list->AddLine(
                ImVec2(canvas_pos.x, off_y),
                ImVec2(canvas_pos.x + canvas_size.x, off_y),
                ImColor(255, 0, 0, 128));
        draw_list->AddLine(
                ImVec2(off_x, canvas_pos.y),
                ImVec2(off_x, canvas_pos.y + canvas_size.y),
                ImColor(0, 255, 0, 128));

        // tiles
        for (int x = 0; x < 38; x++) {
            for (int y = 0; y < 49; y++) {
                auto &led = games::drs::DRS_TAPELED[x + y * 38];
                ImColor color((int) led[0], (int) led[1], (int) led[2]);
                ImVec2 p1((x - 19) * scale + off_x, (y + 0) * scale + off_y);
                ImVec2 p2((x - 18) * scale + off_x, (y + 1) * scale + off_y);
                draw_list->AddRectFilled(p1, p2, color, 0.f);
            }
        }

        // foots
        const float foot_box = 2.f * scale;
        for (auto &foot : games::drs::VR_FOOTS) {
            vr::TrackedDevicePose_t pose;
            vr::VRControllerState_t state;
            vrutil::get_con_pose(foot.get_index(), &pose, &state);
            if (pose.bPoseIsValid) {

                // position
                auto pos = vrutil::get_translation(pose.mDeviceToAbsoluteTracking);
                pos = foot.to_world(pos);
                pos.x -= 19;
                pos *= scale;
                ImColor color(255, 0, 255);
                if (foot.event.type == games::drs::DRS_DOWN
                || (foot.event.type == games::drs::DRS_MOVE)) {
                    auto size_factor = foot.event.width / (foot.size_base + foot.size_scale);
                    color = ImColor((int) (size_factor * 127) + 128, 0, 0);
                }
                ImVec2 p1(pos.x + off_x - foot_box * 0.5f,
                          pos.y + off_y - foot_box * 0.5f);
                ImVec2 p2(pos.x + off_x + foot_box * 0.5f,
                          pos.y + off_y + foot_box * 0.5f);
                draw_list->AddRectFilled(p1, p2, color, 0.f);

                // direction
                auto direction = -linalg::qzdir(linalg::qmul(
                        vrutil::get_rotation(pose.mDeviceToAbsoluteTracking.m),
                        foot.rotation));
                direction = linalg::aliases::float3 {
                        -direction.z, direction.x, direction.y
                };
                auto end = pos + direction * foot.length * scale;
                draw_list->AddLine(
                        ImVec2(pos.x + off_x, pos.y + off_y),
                        ImVec2(end.x + off_x, end.y + off_y),
                        ImColor(0, 255, 0));
            }
        }
    }
}

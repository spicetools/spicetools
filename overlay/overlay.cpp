#include "overlay.h"

#include "avs/game.h"
#include "cfg/configurator.h"
#include "games/io.h"
#include "hooks/graphics/graphics.h"
#include "misc/eamuse.h"
#include "touch/touch.h"
#include "util/logging.h"

#include "imgui/impl_dx9.h"
#include "imgui/impl_spice.h"
#include "imgui/impl_sw.h"
#include "overlay/imgui/impl_dx9.h"
#include "overlay/imgui/impl_spice.h"
#include "overlay/imgui/impl_sw.h"

#include "window.h"
#include "windows/card_manager.h"
#include "windows/screen_resize.h"
#include "windows/config.h"
#include "windows/control.h"
#include "windows/fps.h"
#include "windows/iidx_sub.h"
#include "windows/sdvx_sub.h"
#include "windows/keypad.h"
#include "windows/kfcontrol.h"
#include "windows/log.h"
#include "windows/patch_manager.h"
#include "windows/vr.h"

namespace overlay {

    // settings
    bool ENABLED = true;

    // global
    std::mutex OVERLAY_MUTEX;
    std::unique_ptr<overlay::SpiceOverlay> OVERLAY = nullptr;
}

static void *ImGui_Alloc(size_t sz, void *user_data) {
    void *data = malloc(sz);
    if (!data) {
        return nullptr;
    }

    memset(data, 0, sz);

    return data;
}

static void ImGui_Free(void *data, void *user_data) {
    free(data);
}

void overlay::create_d3d9(HWND hWnd, IDirect3D9 *d3d, IDirect3DDevice9 *device) {
    if (!overlay::ENABLED) {
        return;
    }

    const std::lock_guard<std::mutex> lock(OVERLAY_MUTEX);

    if (!overlay::OVERLAY) {
        overlay::OVERLAY = std::make_unique<overlay::SpiceOverlay>(hWnd, d3d, device);
    }
}

void overlay::create_software(HWND hWnd) {
    if (!overlay::ENABLED) {
        return;
    }

    const std::lock_guard<std::mutex> lock(OVERLAY_MUTEX);

    if (!overlay::OVERLAY) {
        overlay::OVERLAY = std::make_unique<overlay::SpiceOverlay>(hWnd);
    }
}

void overlay::destroy(HWND hWnd) {
    if (!overlay::ENABLED) {
        return;
    }

    const std::lock_guard<std::mutex> lock(OVERLAY_MUTEX);

    if (overlay::OVERLAY && (hWnd == nullptr || overlay::OVERLAY->uses_window(hWnd))) {
        overlay::OVERLAY.reset();
    }
}

overlay::SpiceOverlay::SpiceOverlay(HWND hWnd, IDirect3D9 *d3d, IDirect3DDevice9 *device)
        : renderer(OverlayRenderer::D3D9), hWnd(hWnd), d3d(d3d), device(device) {
    log_info("overlay", "initializing (D3D9)");

    // increment reference counts
    this->d3d->AddRef();
    this->device->AddRef();

    // get creation parameters
    HRESULT ret;
    ret = this->device->GetCreationParameters(&this->creation_parameters);
    if (FAILED(ret)) {
        log_fatal("overlay", "GetCreationParameters failed, hr={}", FMT_HRESULT(ret));
    }

    // get adapter identifier
    ret = this->d3d->GetAdapterIdentifier(
            creation_parameters.AdapterOrdinal,
            0,
            &this->adapter_identifier);
    if (FAILED(ret)) {
        log_fatal("overlay", "GetAdapterIdentifier failed, hr={}", FMT_HRESULT(ret));
    }

    // init
    this->init();
}

overlay::SpiceOverlay::SpiceOverlay(HWND hWnd)
        : renderer(OverlayRenderer::SOFTWARE), hWnd(hWnd) {
    log_info("overlay", "initializing (SOFTWARE)");

    // init
    this->init();
}

void overlay::SpiceOverlay::init() {

    // init imgui
    IMGUI_CHECKVERSION();
    ImGui::SetAllocatorFunctions(ImGui_Alloc, ImGui_Free, nullptr);
    ImGui::CreateContext();
    ImGui::GetIO();

    // set style
    ImGui::StyleColorsDark();
    if (this->renderer == OverlayRenderer::SOFTWARE) {
        imgui_sw::make_style_fast();
        ImVec4* colors = ImGui::GetStyle().Colors;
        colors[ImGuiCol_Border].w = 0;
        colors[ImGuiCol_Separator].w = 0.25f;
    } else {
        auto &style = ImGui::GetStyle();
        style.WindowRounding = 0;
    }

    // red theme
    /*
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Border]                 = ImVec4(0.50f, 0.43f, 0.43f, 0.50f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.48f, 0.16f, 0.16f, 0.54f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.98f, 0.26f, 0.26f, 0.40f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.98f, 0.26f, 0.26f, 0.67f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.48f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.98f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.88f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.98f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.98f, 0.26f, 0.26f, 0.40f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.98f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.98f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.98f, 0.26f, 0.26f, 0.31f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.98f, 0.26f, 0.26f, 0.80f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.98f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_Separator]              = ImVec4(0.50f, 0.43f, 0.43f, 0.50f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.75f, 0.10f, 0.10f, 0.78f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.75f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.98f, 0.26f, 0.26f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.98f, 0.26f, 0.26f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.98f, 0.26f, 0.26f, 0.95f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.58f, 0.18f, 0.18f, 0.86f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.98f, 0.26f, 0.26f, 0.80f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.68f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.15f, 0.07f, 0.07f, 0.97f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.42f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.98f, 0.26f, 0.26f, 0.35f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.98f, 0.26f, 0.26f, 1.00f);
    */

    // configure IO
    auto &io = ImGui::GetIO();
    io.UserData = this;
    io.ConfigFlags = ImGuiConfigFlags_NavEnableKeyboard
                     | ImGuiConfigFlags_NavEnableGamepad
                     | ImGuiConfigFlags_NavEnableSetMousePos
                     | ImGuiConfigFlags_DockingEnable
                     | ImGuiConfigFlags_ViewportsEnable;
    if (is_touch_available()) {
        io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;
    }

    io.MouseDrawCursor = !GRAPHICS_SHOW_CURSOR;

    // disable config
    io.IniFilename = nullptr;

    // allow CTRL+WHEEL scaling
    io.FontAllowUserScaling = true;

    // add default font
    io.Fonts->AddFontDefault();

    // add fallback fonts for missing glyph ranges
    ImFontConfig config {};
    config.MergeMode = true;
    io.Fonts->AddFontFromFileTTF(R"(C:\Windows\Fonts\simsun.ttc)",
            13.0f, &config, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    io.Fonts->AddFontFromFileTTF(R"(C:\Windows\Fonts\arial.ttf)",
            13.0f, &config, io.Fonts->GetGlyphRangesCyrillic());
    io.Fonts->AddFontFromFileTTF(R"(C:\Windows\Fonts\meiryu.ttc)",
            13.0f, &config, io.Fonts->GetGlyphRangesJapanese());
    io.Fonts->AddFontFromFileTTF(R"(C:\Windows\Fonts\meiryo.ttc)",
            13.0f, &config, io.Fonts->GetGlyphRangesJapanese());
    io.Fonts->AddFontFromFileTTF(R"(C:\Windows\Fonts\gulim.ttc)",
            13.0f, &config, io.Fonts->GetGlyphRangesKorean());
    io.Fonts->AddFontFromFileTTF(R"(C:\Windows\Fonts\cordia.ttf)",
            13.0f, &config, io.Fonts->GetGlyphRangesThai());
    io.Fonts->AddFontFromFileTTF(R"(C:\Windows\Fonts\arial.ttf)",
            13.0f, &config, io.Fonts->GetGlyphRangesVietnamese());

    // init implementation
    ImGui_ImplSpice_Init(this->hWnd);
    switch (this->renderer) {
        case OverlayRenderer::D3D9:
            ImGui_ImplDX9_Init(this->device);
            break;
        case OverlayRenderer::SOFTWARE:
            imgui_sw::bind_imgui_painting();
            break;
    }

    // create empty frame
    switch (this->renderer) {
        case OverlayRenderer::D3D9:
            ImGui_ImplDX9_NewFrame();
            break;
        case OverlayRenderer::SOFTWARE:
            break;
    }
    ImGui_ImplSpice_NewFrame();
    ImGui::NewFrame();
    ImGui::EndFrame();

    // fix navigation buttons causing crash on overlay activation
    ImGui::Begin("Temp");
    ImGui::End();

    // referenced windows
    this->window_add(window_fps = new overlay::windows::FPS(this));

    // add default windows
    this->window_add(new overlay::windows::Config(this));
    this->window_add(new overlay::windows::Control(this));
    this->window_add(new overlay::windows::Log(this));
    this->window_add(new overlay::windows::CardManager(this));
    this->window_add(new overlay::windows::ScreenResize(this));
    this->window_add(new overlay::windows::PatchManager(this));
    this->window_add(new overlay::windows::Keypad(this, 0));
    this->window_add(new overlay::windows::KFControl(this));
    this->window_add(new overlay::windows::VRWindow(this));
    if (eamuse_get_game_keypads() > 1) {
        this->window_add(new overlay::windows::Keypad(this, 1));
    }
    if (avs::game::is_model("LDJ")) {
        this->window_add(new overlay::windows::IIDXSubScreen(this));
    }
    if (avs::game::is_model("KFC")) {
        this->window_add(new overlay::windows::SDVXSubScreen(this));
    }
}

overlay::SpiceOverlay::~SpiceOverlay() {

    // imgui shutdown
    ImGui_ImplSpice_Shutdown();
    switch (this->renderer) {
        case OverlayRenderer::D3D9:
            ImGui_ImplDX9_Shutdown();

            // drop references
            this->device->Release();
            this->d3d->Release();

            break;
        case OverlayRenderer::SOFTWARE:
            imgui_sw::unbind_imgui_painting();
            break;
    }
    ImGui::DestroyContext();
}

void overlay::SpiceOverlay::window_add(Window *wnd) {
    this->windows.emplace_back(std::unique_ptr<Window>(wnd));
}

void overlay::SpiceOverlay::new_frame() {

    // update implementation
    ImGui_ImplSpice_NewFrame();
    this->total_elapsed += ImGui::GetIO().DeltaTime;

    // check if inactive
    if (!this->active) {
        return;
    }

    // init frame
    switch (this->renderer) {
        case OverlayRenderer::D3D9:
            ImGui_ImplDX9_NewFrame();
            break;
        case OverlayRenderer::SOFTWARE:
            ImGui_ImplSpice_UpdateDisplaySize();
            break;
    }
    ImGui::NewFrame();

    // animated background
    if (cfg::CONFIGURATOR_STANDALONE) {
        auto flags = ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoBackground |
                ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_NoFocusOnAppearing |
                ImGuiWindowFlags_NoInputs;
        if (ImGui::Begin("Background", nullptr, flags)) {
            ImGui::SetWindowPos(ImVec2(0, 0));
            ImGui::SetWindowSize(ImGui::GetIO().DisplaySize);

            static const int EDGES[] = { 3, 4, 5, 6, 18 };

            auto &io = ImGui::GetIO();
            auto display_x = std::ceil(io.DisplaySize.x);
            auto display_y = std::ceil(io.DisplaySize.y);
            auto spacing = (display_x + display_y) * 0.1f;
            auto max_x = static_cast<int>(display_x / spacing + 1.f);
            auto max_y = static_cast<int>(display_y / spacing + 1.f);
            auto draw = ImGui::GetWindowDrawList();
            for (int i_x = -1; i_x < max_x; i_x++) {
                for (int i_y = -1; i_y < max_y; i_y++) {
                    auto x = static_cast<float>(i_x);
                    auto y = static_cast<float>(i_y);
                    draw->AddCircleFilled(
                            ImVec2(
                                    x * spacing + sinf(total_elapsed * 0.5f + x - y) * spacing * 0.8f,
                                    y * spacing + cosf(total_elapsed * 0.5f - x + y) * spacing * 0.8f
                            ),
                            spacing * 0.3f + sinf(total_elapsed * 0.2f - x - y) * spacing * 0.05f,
                            ImColor(
                                    0.1f,
                                    0.1f,
                                    0.1f,
                                    1.f),
                            EDGES[static_cast<size_t>(i_x + i_y) % std::size(EDGES)]);
                }
            }
            ImGui::End();
        }
    }

    // build windows
    for (auto &window : this->windows) {
        window->build();
    }

    // end frame
    ImGui::EndFrame();
}

void overlay::SpiceOverlay::render() {

    // check if inactive
    if (!this->active) {
        return;
    }

    // imgui render
    ImGui::Render();

    // implementation render
    switch (this->renderer) {
        case OverlayRenderer::D3D9:
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            break;
        case OverlayRenderer::SOFTWARE: {

            // get display metrics
            auto &io = ImGui::GetIO();
            auto width = static_cast<size_t>(std::ceil(io.DisplaySize.x));
            auto height = static_cast<size_t>(std::ceil(io.DisplaySize.y));
            auto pixels = width * height;

            // make sure buffer is big enough
            if (this->pixel_data.size() < pixels) {
                this->pixel_data.resize(pixels, 0);
            }

            // reset buffer
            memset(&this->pixel_data[0], 0, width * height * sizeof(uint32_t));

            // render to pixel data
            imgui_sw::SwOptions options {
                .optimize_text = true,
                .optimize_rectangles = true,
            };
            imgui_sw::paint_imgui(&this->pixel_data[0], width, height, options);
            pixel_data_width = width;
            pixel_data_height = height;

            break;
        }
    }

    for (auto &window : this->windows) {
        window->after_render();
    }
}

void overlay::SpiceOverlay::update() {

    // check overlay toggle
    auto overlay_buttons = games::get_buttons_overlay(eamuse_get_game());
    bool toggle_down_new = overlay_buttons
            && this->hotkeys_triggered()
            && GameAPI::Buttons::getState(RI_MGR, overlay_buttons->at(games::OverlayButtons::ToggleOverlay));
    if (toggle_down_new && !this->toggle_down) {
        toggle_active(true);
    }
    this->toggle_down = toggle_down_new;

    // update windows
    for (auto &window : this->windows) {
        window->update();
    }

    // deactivate if no windows are shown
    bool window_active = false;
    for (auto &window : this->windows) {
        if (window->get_active()) {
            window_active = true;
            break;
        }
    }
    if (!window_active) {
        this->set_active(false);
    }
}

bool overlay::SpiceOverlay::update_cursor() {
    return ImGui_ImplSpice_UpdateMouseCursor();
}

void overlay::SpiceOverlay::toggle_active(bool overlay_key) {

    // invert active state
    this->active = !this->active;

    // show FPS window if toggled with overlay key
    if (overlay_key) {
        this->window_fps->set_active(this->active);
    }
}

void overlay::SpiceOverlay::set_active(bool new_active) {

    // toggle if different
    if (this->active != new_active) {
        this->toggle_active();
    }
}

bool overlay::SpiceOverlay::get_active() {
    return this->active;
}

bool overlay::SpiceOverlay::has_focus() {
    return this->get_active() && ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow);
}

bool overlay::SpiceOverlay::hotkeys_triggered() {

    // check if disabled first
    if (!this->hotkeys_enable) {
        return false;
    }

    // get buttons
    auto buttons = games::get_buttons_overlay(eamuse_get_game());
    if (!buttons) {
        return false;
    }

    auto &hotkey1 = buttons->at(games::OverlayButtons::HotkeyEnable1);
    auto &hotkey2 = buttons->at(games::OverlayButtons::HotkeyEnable2);
    auto &toggle = buttons->at(games::OverlayButtons::HotkeyToggle);

    // check hotkey toggle
    auto toggle_state = GameAPI::Buttons::getState(RI_MGR, toggle);
    if (toggle_state) {
        if (!this->hotkey_toggle_last) {
            this->hotkey_toggle_last = true;
            this->hotkey_toggle = !this->hotkey_toggle;
        }
    } else {
        this->hotkey_toggle_last = false;
    }

    // hotkey toggle overrides hotkey enable button states
    if (hotkey_toggle) {
        return true;
    }

    // check hotkey enable buttons
    bool triggered = true;
    if (hotkey1.isSet() && !GameAPI::Buttons::getState(RI_MGR, hotkey1)) {
        triggered = false;
    }
    if (hotkey2.isSet() && !GameAPI::Buttons::getState(RI_MGR, hotkey2)) {
        triggered = false;
    }
    return triggered;
}

void overlay::SpiceOverlay::reset_invalidate() {
    ImGui_ImplDX9_InvalidateDeviceObjects();
}

void overlay::SpiceOverlay::reset_recreate() {
    ImGui_ImplDX9_CreateDeviceObjects();
}

void overlay::SpiceOverlay::input_char(unsigned int c, bool rawinput) {

    // disable rawinput characters once we got one from somewhere else (such as a WndProc)
    if (!rawinput) {
        this->rawinput_char = false;
    } else if (!this->rawinput_char) {
        return;
    }

    // add character to ImGui
    ImGui::GetIO().AddInputCharacter(c);
}

uint32_t *overlay::SpiceOverlay::sw_get_pixel_data(int *width, int *height) {

    // check if active
    if (!this->active) {
        *width = 0;
        *height = 0;
        return nullptr;
    }

    // ensure buffer has the right size
    const size_t total_size = this->pixel_data_width * this->pixel_data_height;
    if (this->pixel_data.size() < total_size) {
        this->pixel_data.resize(total_size, 0);
    }

    // check for empty surface
    if (this->pixel_data.empty()) {
        *width = 0;
        *height = 0;
        return nullptr;
    }

    // copy and return pointer to data
    *width = this->pixel_data_width;
    *height = this->pixel_data_height;
    return &this->pixel_data[0];
}

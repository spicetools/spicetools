#pragma once

#include <memory>
#include <mutex>
#include <vector>
#include <windows.h>
#include <d3d9.h>

#include "external/imgui/imgui.h"

namespace overlay {
    class Window;

    enum class OverlayRenderer {
        D3D9,
        SOFTWARE,
    };

    // settings
    extern bool ENABLED;

    class SpiceOverlay {
    public:

        D3DDEVICE_CREATION_PARAMETERS creation_parameters {};
        D3DADAPTER_IDENTIFIER9 adapter_identifier {};
        bool hotkeys_enable = true;

        explicit SpiceOverlay(HWND hWnd, IDirect3D9 *d3d, IDirect3DDevice9 *device);
        explicit SpiceOverlay(HWND hWnd);
        ~SpiceOverlay();

        void window_add(Window *wnd);
        void new_frame();
        void render();
        void update();
        void toggle_active(bool overlay_key = false);
        void set_active(bool active);
        bool get_active();
        bool has_focus();
        bool hotkeys_triggered();

        static bool update_cursor();
        static void reset_invalidate();
        static void reset_recreate();
        void input_char(unsigned int c, bool rawinput = false);

        uint32_t *sw_get_pixel_data(int *width, int *height);

        inline bool uses_window(HWND hWnd) {
            return this->hWnd == hWnd;
        }
        inline bool uses_context(IDirect3D9 *other) {
            return this->d3d == other;
        }
        inline bool uses_device(IDirect3DDevice9 *other) {
            return this->device == other;
        }
        inline IDirect3DDevice9 *get_device() {
            return this->device;
        }

        // renderer
        OverlayRenderer renderer;
        float total_elapsed = 0.f;

    private:

        HWND hWnd = nullptr;

        // D3D9
        IDirect3D9 *d3d = nullptr;
        IDirect3DDevice9 *device = nullptr;

        // software
        std::vector<uint32_t> pixel_data;
        size_t pixel_data_width = 0;
        size_t pixel_data_height = 0;

        std::vector<std::unique_ptr<Window>> windows;
        Window *window_fps = nullptr;

        bool active = false;
        bool toggle_down = false;
        bool rawinput_char = true;
        bool hotkey_toggle = false;
        bool hotkey_toggle_last = false;

        void init();
    };

    // global
    extern std::mutex OVERLAY_MUTEX;
    extern std::unique_ptr<overlay::SpiceOverlay> OVERLAY;

    // synchronized helpers
    void create_d3d9(HWND hWnd, IDirect3D9 *d3d, IDirect3DDevice9 *device);
    void create_software(HWND hWnd);
    void destroy(HWND hWnd = nullptr);
}

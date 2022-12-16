#ifndef SPICETOOLS_OVERLAY_WINDOWS_IIDX_SUB_H
#define SPICETOOLS_OVERLAY_WINDOWS_IIDX_SUB_H

#include <optional>

#include <windows.h>
#include <d3d9.h>

#include "overlay/window.h"

namespace overlay::windows {

    class IIDXSubScreen : public Window {
    public:
        IIDXSubScreen(SpiceOverlay *overlay);

        void build_content() override;

    private:
        bool build_texture(IDirect3DSurface9 *surface);
        void draw_texture();

        std::optional<std::string> status_message = std::nullopt;

        IDirect3DDevice9 *device = nullptr;
        IDirect3DTexture9 *texture = nullptr;
        ImVec2 texture_size;
    };
}

#endif // SPICETOOLS_OVERLAY_WINDOWS_IIDX_SUB_H

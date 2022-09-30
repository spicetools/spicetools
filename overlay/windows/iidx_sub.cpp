#undef CINTERFACE

#include "iidx_sub.h"

#include <fmt/format.h>

#include "games/io.h"
#include "hooks/graphics/backends/d3d9/d3d9_backend.h"
#include "hooks/graphics/backends/d3d9/d3d9_device.h"
#include "util/logging.h"

namespace overlay::windows {

    const ImVec4 YELLOW(1.f, 1.f, 0.f, 1.f);
    const ImVec4 WHITE(1.f, 1.f, 1.f, 1.f);

    IIDXSubScreen::IIDXSubScreen(SpiceOverlay *overlay) : Window(overlay), device(overlay->get_device()) {
        this->draws_window = false;
        this->title = "Sub Screen";
        this->toggle_button = games::OverlayButtons::ToggleSubScreen;

        this->texture_size = ImVec2(0, 0);
    }

    void IIDXSubScreen::build_content() {
        this->draw_texture();

        /*
        if (this->status_message.has_value()) {
            ImGui::TextColored(YELLOW, "%s", this->status_message.value().c_str());
        } else if (this->texture) {
            ImGui::TextColored(WHITE, "Successfully acquired surface texture");
        } else {
            ImGui::TextColored(YELLOW, "Failed to acquire surface texture");
        }
        */
    }

    bool IIDXSubScreen::build_texture(IDirect3DSurface9 *surface) {
        HRESULT hr;

        D3DSURFACE_DESC desc {};
        hr = surface->GetDesc(&desc);
        if (FAILED(hr)) {
            this->status_message = fmt::format("Failed to get surface descriptor, hr={}", FMT_HRESULT(hr));
            return false;
        }

        hr = this->device->CreateTexture(desc.Width, desc.Height, 0, desc.Usage, desc.Format,
                desc.Pool, &this->texture, nullptr);
        if (FAILED(hr)) {
            this->status_message = fmt::format("Failed to create render target, hr={}", FMT_HRESULT(hr));
            return false;
        }

        this->texture_size = ImVec2(desc.Width, desc.Height);

        return true;
    }

    void IIDXSubScreen::draw_texture() {
        HRESULT hr;

        auto surface = graphics_d3d9_ldj_get_sub_screen();
        if (surface == nullptr) {
            return;
        }

        if (this->texture == nullptr) {
            if (!this->build_texture(surface)) {
                this->texture = nullptr;
                this->texture_size = ImVec2(0, 0);

                surface->Release();
                return;
            }
        }

        IDirect3DSurface9 *texture_surface = nullptr;
        hr = this->texture->GetSurfaceLevel(0, &texture_surface);
        if (FAILED(hr)) {
            this->status_message = fmt::format("Failed to get texture surface, hr={}", FMT_HRESULT(hr));

            surface->Release();
            return;
        }

        hr = this->device->StretchRect(surface, nullptr, texture_surface, nullptr, D3DTEXF_NONE);
        if (FAILED(hr)) {
            this->status_message = fmt::format("Failed to copy back buffer contents, hr={}", FMT_HRESULT(hr));

            surface->Release();
            texture_surface->Release();
            return;
        }

        surface->Release();
        texture_surface->Release();

        ImGui::GetBackgroundDrawList()->AddImage(
                reinterpret_cast<void *>(this->texture),
                ImVec2(0, 0),
                this->texture_size,
                ImVec2(0, 0),
                ImVec2(1, 1));
    }
}

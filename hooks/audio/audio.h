#pragma once

#include <optional>

#include <windows.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>

namespace hooks::audio {
    enum class Backend {
        Asio,
        WaveOut,
    };

    extern bool ENABLED;
    extern bool USE_DUMMY;
    extern WAVEFORMATEXTENSIBLE FORMAT;
    extern std::optional<Backend> BACKEND;
    extern size_t ASIO_DRIVER_ID;

    void init();
    void stop();

    inline std::optional<Backend> name_to_backend(const char *value) {
        if (_stricmp(value, "asio") == 0) {
            return Backend::Asio;
        } else if (_stricmp(value, "waveout") == 0) {
            return Backend::WaveOut;
        }

        return std::nullopt;
    }
}

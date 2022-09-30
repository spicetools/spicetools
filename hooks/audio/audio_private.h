#pragma once

#include <mutex>

#include <windows.h>
#include <audioclient.h>

constexpr bool AUDIO_LOG_HRESULT = true;

namespace hooks::audio {

    extern IAudioClient *CLIENT;
    extern std::mutex INITIALIZE_LOCK;
}

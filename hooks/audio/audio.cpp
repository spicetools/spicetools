#include "audio.h"

#include <vector>

#include <windows.h>
#include <initguid.h>
#include <audioclient.h>
#include <mmdeviceapi.h>

#include "hooks/audio/backends/mmdevice/device_enumerator.h"
#include "util/detour.h"
#include "util/logging.h"
#include "util/memutils.h"

#include "audio_private.h"

#ifdef _MSC_VER
DEFINE_GUID(CLSID_MMDeviceEnumerator,
    0xBCDE0395, 0xE52F, 0x467C,
    0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E);
#endif

// function pointers
static decltype(CoCreateInstance) *CoCreateInstance_orig = nullptr;

namespace hooks::audio {

    // public globals
    bool ENABLED = true;
    bool USE_DUMMY = false;
    WAVEFORMATEXTENSIBLE FORMAT {};
    std::optional<Backend> BACKEND = std::nullopt;
    size_t ASIO_DRIVER_ID = 0;

    // private globals
    IAudioClient *CLIENT = nullptr;
    std::mutex INITIALIZE_LOCK;
}

static HRESULT STDAPICALLTYPE CoCreateInstance_hook(
    REFCLSID rclsid,
    LPUNKNOWN pUnkOuter,
    DWORD dwClsContext,
    REFIID riid,
    LPVOID *ppv)
{
    // call original
    HRESULT ret = CoCreateInstance_orig(rclsid, pUnkOuter, dwClsContext, riid, ppv);
    if (FAILED(ret)) {
        if (IsEqualCLSID(rclsid, CLSID_MMDeviceEnumerator)) {
            log_warning("audio", "CoCreateInstance failed, hr={}", FMT_HRESULT(ret));
        }
        return ret;
    }

    // check if this is the audio device enumerator
    if (IsEqualCLSID(rclsid, CLSID_MMDeviceEnumerator)) {

        // wrap object
        auto mmde = reinterpret_cast<IMMDeviceEnumerator **>(ppv);
        *mmde = new WrappedIMMDeviceEnumerator(*mmde);
    }

    // return original result
    return ret;
}

namespace hooks::audio {
    void init() {
        if (!ENABLED) {
            return;
        }

        log_info("audio", "initializing");

        // general hooks
        CoCreateInstance_orig = detour::iat_try("CoCreateInstance", CoCreateInstance_hook);
    }

    void stop() {
        if (CLIENT) {
            CLIENT->Stop();
            CLIENT->Release();
            CLIENT = nullptr;
        }
    }
}

#include "device_enumerator.h"

#include "util/logging.h"

#include "device.h"

#ifdef _MSC_VER
DEFINE_GUID(IID_IMMDeviceEnumerator,
        0xa95664d2, 0x9614, 0x4f35,
        0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6);
#endif

HRESULT STDMETHODCALLTYPE WrappedIMMDeviceEnumerator::QueryInterface(REFIID riid, void **ppvObj) {
    if (ppvObj == nullptr) {
        return E_POINTER;
    }

    if (riid == IID_IMMDeviceEnumerator) {
        this->AddRef();
        *ppvObj = this;

        return S_OK;
    }

    return pReal->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE WrappedIMMDeviceEnumerator::AddRef() {
    return pReal->AddRef();
}
ULONG STDMETHODCALLTYPE WrappedIMMDeviceEnumerator::Release() {

    // get reference count of underlying interface
    ULONG refs = pReal != nullptr ? pReal->Release() : 0;

    if (refs == 0) {
        delete this;
    }

    return refs;
}

HRESULT STDMETHODCALLTYPE WrappedIMMDeviceEnumerator::EnumAudioEndpoints(
    EDataFlow dataFlow,
    DWORD dwStateMask,
    IMMDeviceCollection **ppDevices)
{
    return pReal->EnumAudioEndpoints(dataFlow, dwStateMask, ppDevices);
}

HRESULT STDMETHODCALLTYPE WrappedIMMDeviceEnumerator::GetDefaultAudioEndpoint(
    EDataFlow dataFlow,
    ERole role,
    IMMDevice **ppEndpoint)
{
    // call orignal
    HRESULT ret = this->pReal->GetDefaultAudioEndpoint(dataFlow, role, ppEndpoint);

    // check for failure
    if (FAILED(ret)) {
        log_warning("audio", "IMMDeviceEnumerator::GetDefaultAudioEndpoint failed, hr={}", FMT_HRESULT(ret));
        return ret;
    }

    // wrap interface
    *ppEndpoint = new WrappedIMMDevice(*ppEndpoint);

    // return original result
    return ret;
}
HRESULT STDMETHODCALLTYPE WrappedIMMDeviceEnumerator::GetDevice(
    LPCWSTR pwstrId,
    IMMDevice **ppDevice)
{
    return pReal->GetDevice(pwstrId, ppDevice);
}
HRESULT STDMETHODCALLTYPE WrappedIMMDeviceEnumerator::RegisterEndpointNotificationCallback(
    IMMNotificationClient *pClient)
{
    return pReal->RegisterEndpointNotificationCallback(pClient);
}
HRESULT STDMETHODCALLTYPE WrappedIMMDeviceEnumerator::UnregisterEndpointNotificationCallback(
    IMMNotificationClient *pClient)
{
    return pReal->UnregisterEndpointNotificationCallback(pClient);
}

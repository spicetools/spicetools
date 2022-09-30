#include "cpuutils.h"

#include <thread>

#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS

#include <winternl.h>
#include <ntstatus.h>

#include "util/libutils.h"
#include "util/logging.h"
#include "util/utils.h"

/*
#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif
*/

namespace cpuutils {

    typedef struct _SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION {
        LARGE_INTEGER IdleTime;
        LARGE_INTEGER KernelTime;
        LARGE_INTEGER UserTime;
        LARGE_INTEGER DpcTime;
        LARGE_INTEGER InterruptTime;
        ULONG         InterruptCount;
    } SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION, *PSYSTEM_PROCESSOR_PERFORMANCE_INFORMATION;

    typedef NTSTATUS (WINAPI *NtQuerySystemInformation_t)(
            DWORD SystemInformationClass,
            PVOID SystemInformation,
            ULONG SystemInformationLength,
            PULONG ReturnLength
    );
    static NtQuerySystemInformation_t NtQuerySystemInformation = nullptr;
    static size_t PROCESSOR_COUNT = 0;
    static std::vector<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION> PROCESSOR_STATES;

    static void init() {

        // check if done
        static bool done = false;
        if (done) {
            return;
        }

        // get pointers
        if (NtQuerySystemInformation == nullptr) {
            auto ntdll = libutils::try_module("ntdll.dll");

            if (ntdll != nullptr) {
                NtQuerySystemInformation = libutils::try_proc<NtQuerySystemInformation_t>(
                        ntdll, "NtQuerySystemInformation");

                if (NtQuerySystemInformation == nullptr) {
                    log_warning("cpuutils", "NtQuerySystemInformation not found");
                }
            }
        }

        // get system info
        SYSTEM_INFO info {};
        GetSystemInfo(&info);
        PROCESSOR_COUNT = info.dwNumberOfProcessors;
        log_misc("cpuutils", "detected {} processors", PROCESSOR_COUNT);
        done = true;

        // init processor states
        get_load();
    }

    std::vector<float> get_load() {

        // lazy init
        cpuutils::init();
        std::vector<float> cpu_load_values;

        // query system information
        if (NtQuerySystemInformation) {
            auto ppi = std::make_unique<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION[]>(PROCESSOR_COUNT);
            ULONG ret_len = sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * PROCESSOR_COUNT;
            NTSTATUS ret;
            if (NT_SUCCESS(ret = NtQuerySystemInformation(8, ppi.get(), ret_len, &ret_len))) {

                // check cpu core count
                auto count = ret_len / sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION);
                for (size_t i = 0; i < count; i++) {
                    auto &pi = ppi[i];

                    // get old state
                    if (PROCESSOR_STATES.size() <= i) {
                        PROCESSOR_STATES.push_back(pi);
                    }
                    auto &pi_old = PROCESSOR_STATES[i];

                    // get delta state
                    SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION delta;
                    delta.DpcTime.QuadPart = pi.DpcTime.QuadPart - pi_old.DpcTime.QuadPart;
                    delta.InterruptTime.QuadPart = pi.InterruptTime.QuadPart - pi_old.InterruptTime.QuadPart;
                    delta.UserTime.QuadPart = pi.UserTime.QuadPart - pi_old.UserTime.QuadPart;
                    delta.KernelTime.QuadPart = pi.KernelTime.QuadPart - pi_old.KernelTime.QuadPart;
                    delta.IdleTime.QuadPart = pi.IdleTime.QuadPart - pi_old.IdleTime.QuadPart;

                    // calculate total time run
                    LARGE_INTEGER time_run {
                        .QuadPart = delta.DpcTime.QuadPart
                                + delta.InterruptTime.QuadPart
                                + delta.UserTime.QuadPart
                                + delta.KernelTime.QuadPart
                    };
                    if (time_run.QuadPart == 0) {
                        time_run.QuadPart = 1;
                    }

                    // calculate CPU load
                    cpu_load_values.emplace_back(MIN(MAX(1.f - (
                            (float) delta.IdleTime.QuadPart / (float) time_run.QuadPart), 0.f), 1.f) * 100.f);

                    // save state
                    PROCESSOR_STATES[i] = pi;
                }
            } else {
                log_warning("cpuutils", "NtQuerySystemInformation failed: {}", ret);
            }
        }

        // return data
        return cpu_load_values;
    }
}

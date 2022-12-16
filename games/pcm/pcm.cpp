#include "pcm.h"

#include "io.h"
#include "misc/wintouchemu.h"
#include "util/detour.h"
#include "util/utils.h"
#include "util/libutils.h"

namespace games::pcm {
    uint32_t *(__cdecl *GsIo_GetState_orig)(uint32_t *state);

    static int BillKind = 0;

    static int __cdecl BillVali_GetEscrowBillKind() {
        return BillKind;
    }

    static int __cdecl BillVali_GetCurrentStep() {
        return 0;
    }

    static int __cdecl BillVali_GetError() {
        return 1;
    }

    static char __cdecl BillVali_IsEscrowNow() {
        auto &buttons = games::pcm::get_buttons();

        if (GameAPI::Buttons::getState(RI_MGR, buttons[games::pcm::Buttons::Insert1000YenBill])) {
            log_misc("pcm", "1000 yen bill inserted");
            BillKind = 1;
            return true;
        }

        if (GameAPI::Buttons::getState(RI_MGR, buttons[games::pcm::Buttons::Insert2000YenBill])) {
            log_misc("pcm", "2000 yen bill inserted");
            BillKind = 2;
            return true;
        }

        if (GameAPI::Buttons::getState(RI_MGR, buttons[games::pcm::Buttons::Insert5000YenBill])) {
            log_misc("pcm", "5000 yen bill inserted");
            BillKind = 3;
            return true;
        }

        if (GameAPI::Buttons::getState(RI_MGR, buttons[games::pcm::Buttons::Insert10000YenBill])) {
            log_misc("pcm", "10000 yen bill inserted");
            BillKind = 4;
            return true;
        }

        return false;
    }

    static bool __cdecl BillVali_IsIdling() {
        return true;
    }

    static bool __cdecl BillVali_IsReady() {
        return true;
    }

    static bool __cdecl BillVali_IsWaiting() {
        return true;
    }

    static int __cdecl BillVali_GetReceivedBill() {
        static int bills_received = 0;

        return ++bills_received;
    }

    static bool __cdecl BillVali_ReceiveBill(int) {
        return true;
    }

    static uint32_t* __cdecl GsIo_GetState_hook(uint32_t *state) {
        GsIo_GetState_orig(state);

        // "bill validator is happy" flag :]
        *state |= 4;

        return state;
    }

    PCMGame::PCMGame() : Game("Charge Machine") {}

    void PCMGame::attach() {
        Game::attach();

        // system.dll doesn't get loaded until later so load it now for hooking.
        HMODULE system = libutils::try_library("system.dll");

        // apply hooks
        detour::trampoline("system.dll", "?GetState@GsIo@@SA?BVSTATE@1@XZ",
                GsIo_GetState_hook, &GsIo_GetState_orig);

        detour::inline_hook(BillVali_GetCurrentStep, libutils::try_proc(
                system, "?GetCurrentStep@GsBillVali@@SAHXZ"));
        detour::inline_hook(BillVali_GetError, libutils::try_proc(
                system, "?GetError@GsBillVali@@SAHXZ"));
        detour::inline_hook(BillVali_GetReceivedBill, libutils::try_proc(
                system, "?GetReceivedBill@GsBillVali@@SAHXZ"));
        detour::inline_hook(BillVali_IsEscrowNow, libutils::try_proc(
                system, "?IsEscrowNow@GsBillVali@@SA_NXZ"));
        detour::inline_hook(BillVali_IsIdling, libutils::try_proc(
                system, "?IsIdling@GsBillVali@@SA_NXZ"));
        detour::inline_hook(BillVali_IsWaiting, libutils::try_proc(
                system, "?IsWaiting@GsBillVali@@SA_NXZ"));
        detour::inline_hook(BillVali_IsReady, libutils::try_proc(
                system, "?IsReady@GsBillVali@@SA_NXZ"));
        detour::inline_hook(BillVali_ReceiveBill, libutils::try_proc(
                system, "?ReceiveBill@GsBillVali@@SA_NH@Z"));
        detour::inline_hook(BillVali_GetEscrowBillKind, libutils::try_proc(
                system, "?GetEscrowBillKind@GsBillVali@@SAHXZ"));
    }
}

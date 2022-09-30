#include "bc.h"

#include "hooks/setupapihook.h"
#include "util/libutils.h"
#include "acio2emu/handle.h"
#include "acioemu/handle.h"
#include "node.h"

namespace games::bc {
    void BCGame::attach() {
        Game::attach();

        setupapihook_init(libutils::load_library("libaio.dll"));
        setupapihook_add({
            { 0, 0, 0, 0, },
            "",
            "USB\\VID_1CCF&PID_8050\\0000",
            {},
            "COM3",
        });

        auto iob = new acio2emu::IOBHandle(L"COM3");
        iob->register_node(std::make_unique<TBSNode>());

        devicehook_init();
        devicehook_add(iob);
        devicehook_add(new acioemu::ACIOHandle(L"COM1"));
    }

    void BCGame::detach() {
        Game::detach();

        devicehook_dispose();
    }
}
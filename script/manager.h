#pragma once

#include <memory>

namespace script {
    class Instance;

    void manager_scan();
    void manager_add(std::shared_ptr<Instance> instance);
    void manager_boot();
    void manager_config();
    void manager_shutdown();
}

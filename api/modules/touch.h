#pragma once

#include <vector>
#include "api/module.h"
#include "api/request.h"

namespace api::modules {

    class Touch : public Module {
    public:
        Touch();

    private:

        // function definitions
        void read(Request &req, Response &res);
        void write(Request &req, Response &res);
        void write_reset(Request &req, Response &res);
    };
}

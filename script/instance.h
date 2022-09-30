#pragma once

#include <thread>
#include <mutex>
#include <string_view>

namespace script {
    struct LuaData;

    class Instance {
    private:
        LuaData *lua = nullptr;
        std::thread *thread = nullptr;
        std::mutex thread_m;
        bool running = false;
        bool paused = false;
        bool error = false;

    public:

        Instance(const char *str, bool is_path = true, bool sandbox = false);
        ~Instance();

        void join();
        void stop();
        void pause();
        void resume();

        inline bool is_running() {
            return running;
        }

        inline bool is_paused() {
            return paused;
        }
    };

    extern thread_local Instance *INSTANCE;
}

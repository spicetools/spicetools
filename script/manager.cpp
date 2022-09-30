#include "manager.h"
#include <thread>
#include "util/fileutils.h"
#include "script/instance.h"
#include "launcher/launcher.h"
#include "launcher/options.h"
#include "games/io.h"
#include "misc/eamuse.h"
#include "util/logging.h"

namespace script {

    // globals
    static std::string DIRECTORY = "script";
    static std::vector<std::string> BOOT_LIST;
    static std::vector<std::string> CONFIG_LIST;
    static std::vector<std::string> SHUTDOWN_LIST;
    static std::vector<std::shared_ptr<Instance>> INSTANCES;
    static std::mutex INSTANCES_M;
    static bool BOOT = false;
    static bool CONFIG = false;
    static bool SHUTDOWN = false;

    void manager_scan() {

        // clear lists
        BOOT_LIST.clear();

        // scan script directory
        std::vector<std::string> files;
        fileutils::dir_scan(DIRECTORY, files, true);
        for (auto &file : files) {
            if (file.ends_with(".lua")) {
                auto name = file.substr(file.find_last_of("/\\") + 1);
                if (name.starts_with("boot_")) {
                    BOOT_LIST.push_back(file);
                }
                if (name.starts_with("shutdown_")) {
                    SHUTDOWN_LIST.push_back(file);
                }
                if (name.starts_with("config_")) {
                    CONFIG_LIST.push_back(file);
                }
            }
        }
    }

    void manager_add(std::shared_ptr<Instance> instance) {
        std::lock_guard<std::mutex> lock(INSTANCES_M);
        auto it = INSTANCES.begin();
        while (it != INSTANCES.end()) {
            if (!(*it)->is_running()) {
                it = INSTANCES.erase(it);
            } else {
                ++it;
            }
        }
        INSTANCES.emplace_back(instance);
    }

    void manager_boot() {
        if (BOOT) return;
        BOOT = true;

        // game options
        auto &options = *games::get_options(eamuse_get_game());
        for (auto &value : options[launcher::Options::ExecuteScript].values()) {
            if (fileutils::file_exists(value)) {
                manager_add(std::make_shared<Instance>(value.c_str(), true, false));
            } else {
                log_warning("script", "unable to locate {}", value);
            }
        }

        // run boot scripts
        for (auto &file : BOOT_LIST) {
            manager_add(std::make_shared<Instance>(file.c_str(), true, false));
        }
    }

    void manager_config() {
        if (CONFIG) return;
        CONFIG = true;

        // launcher options
        for (auto &value : LAUNCHER_OPTIONS->at(launcher::Options::ExecuteScript).values()) {
            if (fileutils::file_exists(value)) {
                manager_add(std::make_shared<Instance>(value.c_str(), true, false));
            } else {
                log_warning("script", "unable to locate {}", value);
            }
        }

        // run config scripts
        for (auto &file : CONFIG_LIST) {
            manager_add(std::make_shared<Instance>(file.c_str(), true, false));
        }
    }

    void manager_shutdown() {
        if (SHUTDOWN) return;
        SHUTDOWN = true;

        // stop running scripts
        INSTANCES_M.lock();
        INSTANCES.clear();
        INSTANCES_M.unlock();

        // run shutdown scripts
        std::vector<std::shared_ptr<Instance>> join_list;
        for (auto &file : SHUTDOWN_LIST) {
            auto instance = std::make_shared<Instance>(file.c_str(), true, false);
            join_list.emplace_back(instance);
            manager_add(instance);
        }

        // wait for completion
        for (auto &instance : join_list) {
            instance->join();
        }
    }
}

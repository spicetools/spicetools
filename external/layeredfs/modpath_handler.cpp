#include <windows.h>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "modpath_handler.h"

#include "config.h"
#include "utils.h"

using std::nullopt;

namespace layeredfs {

    typedef struct {
        std::string name;
        std::unordered_set<string> contents;
    } mod_contents_t;

    std::vector<mod_contents_t> cached_mods;

    std::unordered_set<string> walk_dir(const string &path, const string &root) {
        std::unordered_set<string> result;

        WIN32_FIND_DATAA ffd;
        auto contents = FindFirstFileA((path + "/*").c_str(), &ffd);

        if (contents != INVALID_HANDLE_VALUE) {
            do {
                if (!strcmp(ffd.cFileName, ".") ||
                    !strcmp(ffd.cFileName, "..")) {
                    continue;
                }
                string result_path;
                if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    result_path = root + ffd.cFileName + "/";
                    logf_verbose("  %s", result_path.c_str());
                    auto subdir_walk = walk_dir(path + "/" + ffd.cFileName, result_path);
                    result.insert(subdir_walk.begin(), subdir_walk.end());
                } else {
                    result_path = root + ffd.cFileName;
                    logf_verbose("  %s", result_path.c_str());
                }
                result.insert(result_path);
            } while (FindNextFileA(contents, &ffd) != 0);

            FindClose(contents);
        }

        return result;
    }

    void cache_mods(void) {
        if (config.developer_mode)
            return;

        // this is a bit hacky
        config.developer_mode = true;
        auto avail_mods = available_mods();
        config.developer_mode = false;

        for (auto &dir : avail_mods) {
            logf_verbose("Walking %s", dir.c_str());
            mod_contents_t mod;
            mod.name = dir;
            mod.contents = walk_dir(dir, "");
            cached_mods.push_back(mod);
        }
    }

    optional<string> normalise_path(const string &path) {
        auto data_pos = path.find("data/");
        auto data2_pos = string::npos;

        if (data_pos == string::npos) {
            data2_pos = path.find("data2/");

            if (data2_pos == string::npos)
                return nullopt;
        }
        auto actual_pos = (data_pos != string::npos) ? data_pos : data2_pos;
        // if data2 was found, use root data2/.../... instead of just .../...
        auto offset = (data2_pos != string::npos) ? 0 : strlen("data/");
        auto data_str = path.substr(actual_pos + offset);
        // nuke backslash
        string_replace(data_str, "\\", "/");
        // nuke double slash
        string_replace(data_str, "//", "/");

        return data_str;
    }

    vector<string> available_mods() {
        vector<string> ret;
        string mod_root = MOD_FOLDER "/";

        if (config.developer_mode) {
            WIN32_FIND_DATAA ffd;
            auto mods = FindFirstFileA(MOD_FOLDER "/*", &ffd);

            if (mods != INVALID_HANDLE_VALUE) {
                do {
                    if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
                        !strcmp(ffd.cFileName, ".") ||
                        !strcmp(ffd.cFileName, "..") ||
                        !strcmp(ffd.cFileName, "_cache")) {
                        continue;
                    }
                    ret.push_back(mod_root + ffd.cFileName);
                } while (FindNextFileA(mods, &ffd) != 0);

                FindClose(mods);
            }
        } else {
            for (auto &dir : cached_mods) {
                ret.push_back(dir.name);
            }
        }
        std::sort(ret.begin(), ret.end());
        return ret;
    }

    bool mkdir_p(string &path) {
        /* Adapted from http://stackoverflow.com/a/2336245/119527 */
        const size_t len = strlen(path.c_str());
        char _path[MAX_PATH + 1];
        char *p;

        errno = 0;

        /* Copy string so its mutable */
        if (len > sizeof(_path) - 1) {
            return false;
        }
        strncpy(_path, path.c_str(), MAX_PATH);
        _path[MAX_PATH] = '\0';

        /* Iterate the string */
        for (p = _path + 1; *p; p++) {
            if (*p == '/') {
                /* Temporarily truncate */
                *p = '\0';

                if (!CreateDirectoryA(_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                    return false;
                }

                *p = '/';
            }
        }

        if (!CreateDirectoryA(_path, NULL)) {
            if (GetLastError() != ERROR_ALREADY_EXISTS) {
                return false;
            }
        }

        return true;
    }

    // same for files and folders when cached
    optional<string> find_first_cached_item(const string &norm_path) {
        for (auto &dir : cached_mods) {
            auto file_search = dir.contents.find(norm_path);
            if (file_search == dir.contents.end()) {
                continue;
            }
            return dir.name + "/" + *file_search;
        }

        return nullopt;
    }

    optional<string> find_first_modfile(const string &norm_path) {
        if (config.developer_mode) {
            for (auto &dir : available_mods()) {
                auto mod_path = dir + "/" + norm_path;
                if (file_exists(mod_path.c_str())) {
                    return mod_path;
                }
            }
        } else {
            return find_first_cached_item(norm_path);
        }
        return nullopt;
    }

    optional<string> find_first_modfolder(const string &norm_path) {
        if (config.developer_mode) {
            for (auto &dir : available_mods()) {
                auto mod_path = dir + "/" + norm_path;
                if (folder_exists(mod_path.c_str())) {
                    return mod_path;
                }
            }
        } else {
            return find_first_cached_item(norm_path + "/");
        }
        return nullopt;
    }

    vector<string> find_all_modfile(const string &norm_path) {
        vector<string> ret;

        if (config.developer_mode) {
            for (auto &dir : available_mods()) {
                auto mod_path = dir + "/" + norm_path;
                if (file_exists(mod_path.c_str())) {
                    ret.push_back(mod_path);
                }
            }
        } else {
            for (auto &dir : cached_mods) {
                auto file_search = dir.contents.find(norm_path);
                if (file_search == dir.contents.end()) {
                    continue;
                }
                ret.push_back(dir.name + "/" + *file_search);
            }
        }

        // needed for consistency when hashing names
        std::sort(ret.begin(), ret.end());
        return ret;
    }

}

#pragma once

#include "overlay/window.h"

namespace overlay::windows {

    enum class PatchType {
        Unknown,
        Memory,
        Signature,
    };

    enum class PatchStatus {
        Error,
        Disabled,
        Enabled,
    };

    struct MemoryPatch {
        std::string dll_name = "";
        std::shared_ptr<uint8_t[]> data_disabled = nullptr;
        size_t data_disabled_len = 0;
        std::shared_ptr<uint8_t[]> data_enabled = nullptr;
        size_t data_enabled_len = 0;
        uint64_t data_offset = 0;
        uint8_t *data_offset_ptr = nullptr;
        bool fatal_error = false;
    };

    struct PatchData;
    struct SignaturePatch {
        std::string dll_name = "";
        std::string signature = "", replacement = "";
        int64_t offset = 0, usage = 0;

        MemoryPatch to_memory(PatchData *patch);
    };

    struct PatchData {
        bool enabled;
        std::string game_code;
        int datecode_min, datecode_max;
        std::string name, description;
        PatchType type;
        bool preset;
        std::vector<MemoryPatch> patches_memory;
        PatchStatus last_status;
        bool saved;
        std::string hash;
        bool unverified = false;
    };

    class PatchManager : public Window {
    public:

        PatchManager(SpiceOverlay *overlay, bool apply_patches = false);
        ~PatchManager() override;

        void build_content() override;
        void reload_patches(bool apply_patches = false);

    private:

        // configuration
        static std::string config_path;
        static bool config_dirty;
        static bool setting_auto_apply;
        static std::vector<std::string> setting_auto_apply_list;
        static std::vector<std::string> setting_patches_enabled;

        // patches
        static std::vector<PatchData> patches;
        static bool patches_initialized;

        void config_load();
        void config_save();

        void append_patches(std::string &patches_json, bool apply_patches = false);
    };

    PatchStatus is_patch_active(PatchData &patch);
    bool apply_patch(PatchData &patch, bool active);
}

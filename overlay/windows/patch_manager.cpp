#include "patch_manager.h"

#include <psapi.h>
#include "external/rapidjson/document.h"
#include "external/rapidjson/writer.h"
#include "external/hash-library/sha256.h"
#include "external/robin_hood.h"
#include "cfg/configurator.h"
#include "util/memutils.h"
#include "games/io.h"
#include "build/resource.h"
#include "util/sigscan.h"
#include "util/resutils.h"
#include "util/fileutils.h"
#include "util/libutils.h"
#include "util/logging.h"
#include "util/utils.h"
#include "overlay/imgui/extensions.h"
#include "avs/game.h"

// std::min
#ifdef min
#undef min
#endif

// std::max
#ifdef max
#undef max
#endif

using namespace rapidjson;


namespace overlay::windows {

    robin_hood::unordered_map<std::string, std::unique_ptr<std::vector<uint8_t>>> DLL_MAP;

    // configuration
    std::string PatchManager::config_path;
    bool PatchManager::config_dirty = false;
    bool PatchManager::setting_auto_apply = false;
    std::vector<std::string> PatchManager::setting_auto_apply_list;
    std::vector<std::string> PatchManager::setting_patches_enabled;

    // patches
    std::vector<PatchData> PatchManager::patches;
    bool PatchManager::patches_initialized = false;

    PatchManager::PatchManager(SpiceOverlay *overlay, bool apply_patches) : Window(overlay) {
        this->title = "Patch Manager";
        this->flags |= ImGuiWindowFlags_AlwaysAutoResize;
        this->toggle_button = games::OverlayButtons::TogglePatchManager;
        this->init_pos = ImVec2(10, 10);
        config_path = std::string(getenv("APPDATA")) + "\\spicetools_patch_manager.json";
        if (!patches_initialized) {
            if (cfg::CONFIGURATOR_STANDALONE) { apply_patches = true; }
            if (apply_patches) {
                if (fileutils::file_exists(config_path)) {
                    this->config_load();
                }
                this->reload_patches(apply_patches);
            }
        }
    }

    PatchManager::~PatchManager() = default;

    void PatchManager::build_content() {

        // check if initialized
        if (!patches_initialized) {
            if (fileutils::file_exists(config_path)) {
                this->config_load();
            }
            this->reload_patches();
        }

        // check for unapplied patch
        bool unapplied_patches = false;
        for (auto &patch : patches) {
            if (patch.enabled && patch.last_status == PatchStatus::Disabled) {
                unapplied_patches = true;
                break;
            }
        }

        // game code info
        ImGui::HelpMarker(
                "Patches kept up to date with mon's bemanipatcher.\n"
                "Credits go to everyone involved!"
        );
        ImGui::SameLine();
        ImGui::Text("%s", avs::game::get_identifier().c_str());

        // auto apply checkbox
        ImGui::HelpMarker(
                "This option is saved per game.\n"
                "When checked, all set patches will be applied on game boot."
        );
        ImGui::SameLine();
        if (ImGui::Checkbox("Auto Apply", &setting_auto_apply)) {
            config_dirty = true;
        }

        // check for dirty state
        if (config_dirty) {
            if (cfg::CONFIGURATOR_STANDALONE) {

                // auto safe for configurator version
                this->config_save();
                config_dirty = false;

            } else {

                // manual safe for live version
                ImGui::SameLine();
                if (ImGui::Button("Save")) {
                    this->config_save();
                }
            }
        }

        // apply button
        if (unapplied_patches) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.f, 0.3f, 1.f));
            if (ImGui::Button("Apply")) {
                for (auto &patch : patches) {
                    if (patch.enabled && patch.last_status == PatchStatus::Disabled) {
                        if (apply_patch(patch, true)) {
                            patch.last_status = PatchStatus::Enabled;
                        }
                    }
                }
            }
            ImGui::PopStyleColor();
        }

        // check for empty list
        ImGui::Separator();
        if (patches.empty()) {
            ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "No patches available.");
        } else {

            // draw patches
            for (auto &patch : patches) {

                // get patch status
                PatchStatus patch_status = is_patch_active(patch);
                patch.last_status = patch_status;

                // get current state
                bool patch_checked = patch_status == PatchStatus::Enabled;

                // push style
                int style_color_pushed = 0;
                switch (patch_status) {
                    case PatchStatus::Error:
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.f, 0.f, 1.f));
                        style_color_pushed++;
                        break;
                    case PatchStatus::Enabled:
                        if (setting_auto_apply && patch.enabled) {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 0.f, 1.f));
                            style_color_pushed++;
                        }
                        break;
                    case PatchStatus::Disabled:
                        if (patch.enabled) {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.f, 0.3f, 1.f));
                            style_color_pushed++;
                        }
                        break;
                    default:
                        break;
                }

                // checkbox
                auto patch_name = patch.name;
                if (patch.enabled) {
                    patch_name += setting_auto_apply ? " (Locked)" : " (Saved)";
                }
                if (patch.unverified) {
                    patch_name += " (Unverified)";
                }
                if (ImGui::Checkbox(patch_name.c_str(), &patch_checked)) {
                    config_dirty = true;
                    switch (patch_status) {
                        case PatchStatus::Enabled:
                        case PatchStatus::Disabled:
                            if (patch_checked) {
                                patch.saved = true;
                                if (cfg::CONFIGURATOR_STANDALONE) {
                                    setting_auto_apply = true;
                                }
                            }
                            patch.enabled = patch_checked;
                            apply_patch(patch, patch_checked);
                            break;
                        case PatchStatus::Error:
                            if (cfg::CONFIGURATOR_STANDALONE) {
                                if (patch_checked) {
                                    patch.saved = true;
                                    if (cfg::CONFIGURATOR_STANDALONE) {
                                        setting_auto_apply = true;
                                    }
                                }
                                patch.enabled = patch_checked;
                            }
                            break;
                        default:
                            break;
                    }

                    // update status
                    patch.last_status = is_patch_active(patch);
                }

                // pop style
                if (style_color_pushed) {
                    ImGui::PopStyleColor(style_color_pushed);
                }

                // help marker
                std::string description = patch.description;
                if (!patch.patches_memory.empty() && patch.patches_memory.size() <= 10) {
                    if (!description.empty()) description += "\n\n";
                    description += "Patch Data:";
                    for (auto &mp : patch.patches_memory) {
                        if (!description.empty()) description += "\n\n";
                        description += fmt::format(
                                "{:#08X}: {} -> {}",
                                mp.data_offset ? mp.data_offset : (uint64_t) mp.data_offset_ptr,
                                bin2hex(mp.data_disabled.get(), mp.data_enabled_len),
                                bin2hex(mp.data_enabled.get(), mp.data_enabled_len));
                        if (description.length() > 512) {
                            description = patch.description;
                            break;
                        }
                    }
                }
                if (!description.empty()) {
                    ImGui::SameLine();
                    ImGui::HelpMarker(description.c_str());
                }
            }
        }
    }

    void PatchManager::config_load() {
        log_info("patchmanager", "loading config");

        // read config file
        std::string config = fileutils::text_read(config_path);
        if (!config.empty()) {

            // parse document
            Document doc;
            doc.Parse(config.c_str());

            // check parse error
            auto error = doc.GetParseError();
            if (error) {
                log_warning("patchmanager", "config parse error: {}", error);
            }

            // verify root is a dict
            if (doc.IsObject()) {

                // read auto apply settings
                auto auto_apply = doc.FindMember("auto_apply");
                if (auto_apply != doc.MemberEnd() && auto_apply->value.IsArray()) {

                    // get game id
                    auto game_id = avs::game::get_identifier();

                    // iterate entries
                    setting_auto_apply = false;
                    setting_auto_apply_list.clear();
                    for (auto &entry : auto_apply->value.GetArray()) {
                        if (entry.IsString()) {

                            // check if this is our game identifier
                            std::string entry_id = entry.GetString();
                            if (game_id == entry_id) {
                                setting_auto_apply = true;
                            }

                            // move to list
                            setting_auto_apply_list.emplace_back(entry_id);
                        }
                    }
                }

                // read enabled patches
                auto patches_enabled = doc.FindMember("patches_enabled");
                if (patches_enabled != doc.MemberEnd() && patches_enabled->value.IsArray()) {
                    setting_patches_enabled.clear();
                    for (const auto &patch : patches_enabled->value.GetArray()) {
                        if (patch.IsString()) {
                            setting_patches_enabled.emplace_back(std::string(patch.GetString()));
                        }
                    }
                }
            }
        }
    }

    static std::string patch_hash(PatchData &patch) {
        SHA256 hash;
        hash.add(patch.game_code.c_str(), patch.game_code.length());
        hash.add(&patch.datecode_min, sizeof(patch.datecode_min));
        hash.add(&patch.datecode_max, sizeof(patch.datecode_max));
        hash.add(patch.name.c_str(), patch.name.length());
        hash.add(patch.description.c_str(), patch.description.length());
        return hash.getHash();
    }

    void PatchManager::config_save() {

        // create document
        Document doc;
        doc.Parse(
                "{"
                "  \"auto_apply\": [],"
                "  \"patches_enabled\": []"
                "}"
        );

        // check parse error
        auto error = doc.GetParseError();
        if (error) {
            log_warning("patchmanager", "template parse error: {}", error);
        }

        // auto apply setting
        auto &auto_apply_list = doc["auto_apply"];
        auto game_id = avs::game::get_identifier();
        bool game_id_added = false;
        for (auto &entry : setting_auto_apply_list) {
            if (entry == game_id) {
                if (!setting_auto_apply) {
                    continue;
                }
                game_id_added = true;
            }
            auto_apply_list.PushBack(StringRef(entry.c_str()), doc.GetAllocator());
        }
        if (setting_auto_apply && !game_id_added) {
            auto_apply_list.PushBack(StringRef(game_id.c_str()), doc.GetAllocator());
        }

        // get enabled patches
        auto &doc_patches_enabled = doc["patches_enabled"];
        for (auto &patch : patches) {

            // hash patch and find entry
            auto hash = patch_hash(patch);
            auto entry = std::find(
                    setting_patches_enabled.begin(),
                    setting_patches_enabled.end(), hash);

            // enable hash if known as enabled, overridden and missing from list
            if ((patch.last_status == PatchStatus::Enabled && patch.enabled)
            || (cfg::CONFIGURATOR_STANDALONE
             && patch.last_status == PatchStatus::Error && patch.enabled)) {
                if (entry == setting_patches_enabled.end()) {
                    setting_patches_enabled.emplace_back(hash);
                }
            }

            // disable hash if patch known as disabled
            if (patch.last_status == PatchStatus::Disabled
            || (cfg::CONFIGURATOR_STANDALONE
             && patch.last_status == PatchStatus::Error && !patch.enabled)) {
                if (entry != setting_patches_enabled.end()) {
                    setting_patches_enabled.erase(entry);
                }
            }
        }

        // add hashes to document
        for (auto &hash : setting_patches_enabled) {
            Value hash_value(hash.c_str(), doc.GetAllocator());
            doc_patches_enabled.PushBack(hash_value, doc.GetAllocator());
        }

        // build JSON
        StringBuffer buffer;
        Writer<StringBuffer> writer(buffer);
        doc.Accept(writer);

        // save to file
        if (fileutils::text_write(config_path, buffer.GetString())) {
            config_dirty = false;
        } else {
            log_warning("patchmanager", "unable to save config file to {}", config_path);
        }
    }

    void PatchManager::reload_patches(bool apply_patches) {

        // announce reload
        if (apply_patches) {
            log_info("patchmanager", "reloading and applying patches");
        } else {
            log_info("patchmanager", "reloading patches");
        }

        // clear old patches
        patches.clear();

        // load embedded patches from resources
        auto patches_json = resutil::load_file_string(IDR_PATCHES);
        this->append_patches(patches_json, apply_patches);

        // automatic patch file detection
        std::filesystem::path autodetect_paths[] {
                "patches.json",
                MODULE_PATH / "patches.json",
                std::filesystem::path("..") / "patches.json",
        };
        for (const auto &path : autodetect_paths) {
            if (fileutils::file_exists(path)) {
                std::string contents = fileutils::text_read(path);
                if (!contents.empty()) {
                    log_info("patchmanager", "appending patches from {}", path.string());
                    this->append_patches(contents, apply_patches);
                    break;
                } else {
                    log_warning("patchmanager", "failed reading contents from {}", path.string());
                }
            }
        }

        // show amount of patches
        log_info("patchmanager", "loaded {} patches", patches.size());
        patches_initialized = true;
    }

    void PatchManager::append_patches(std::string &patches_json, bool apply_patches) {

        // parse document
        Document doc;
        doc.Parse(patches_json.c_str());

        // check parse error
        auto error = doc.GetParseError();
        if (error) {
            log_warning("patchmanager", "config parse error: {}", error);
        }

        // iterate patches
        for (auto &patch : doc.GetArray()) {

            // verfiy patch data
            auto name_it = patch.FindMember("name");
            if (name_it == patch.MemberEnd() || !name_it->value.IsString()) {
                log_warning("patchmanager", "failed to parse patch name");
                continue;
            }
            auto game_code_it = patch.FindMember("gameCode");
            if (game_code_it == patch.MemberEnd() || !game_code_it->value.IsString()) {
                log_warning("patchmanager", "failed to parse game code for {}",
                        name_it->value.GetString());
                continue;
            }
            auto description_it = patch.FindMember("description");
            if (description_it == patch.MemberEnd() || !description_it->value.IsString()) {
                log_warning("patchmanager", "failed to parse description for {}",
                        name_it->value.GetString());
                continue;
            }
            auto type_it = patch.FindMember("type");
            if (type_it == patch.MemberEnd() || !type_it->value.IsString()) {
                log_warning("patchmanager", "failed to parse type for {}",
                        name_it->value.GetString());
                continue;
            }
            auto preset_it = patch.FindMember("preset");
            bool preset = false;
            if (preset_it != patch.MemberEnd() && preset_it->value.IsBool()) {
                preset = preset_it->value.GetBool();
            }

            // build patch data
            PatchData patch_data {
                .enabled = false,
                .game_code = game_code_it->value.GetString(),
                .datecode_min = 0,
                .datecode_max = 0,
                .name = name_it->value.GetString(),
                .description = description_it->value.GetString(),
                .type = PatchType::Unknown,
                .preset = preset,
                .patches_memory = std::vector<MemoryPatch>(),
                .last_status = PatchStatus::Disabled,
                .saved = false,
                .hash = "",
                .unverified = false,
            };

            // determine patch type
            auto type_str = type_it->value.GetString();
            if (!_stricmp(type_str, "memory")) {
                patch_data.type = PatchType::Memory;
            } else if (!_stricmp(type_str, "signature")) {
                patch_data.type = PatchType::Signature;
            }

            // determine date code
            auto date_code_it = patch.FindMember("dateCode");
            if (date_code_it != patch.MemberEnd() && date_code_it->value.IsInt()) {
                patch_data.datecode_min = date_code_it->value.GetInt();
                patch_data.datecode_max = patch_data.datecode_min;
            } else {
                auto date_code_min_it = patch.FindMember("dateCodeMin");
                if (date_code_min_it == patch.MemberEnd()
                || !date_code_min_it->value.IsInt()) {
                    log_warning("patchmanager", "unable to parse datecode for {}",
                            name_it->value.GetString());
                    continue;
                }
                auto date_code_max_it = patch.FindMember("dateCodeMax");
                if (date_code_max_it == patch.MemberEnd()
                || !date_code_max_it->value.IsInt()) {
                    log_warning("patchmanager", "unable to parse datecode for {}",
                            name_it->value.GetString());
                    continue;
                }
                patch_data.datecode_min = date_code_min_it->value.GetInt();
                patch_data.datecode_max = date_code_max_it->value.GetInt();
            }

            // check for skip
            if (!avs::game::is_model(patch_data.game_code.c_str())) {
                continue;
            }
            if (!avs::game::is_ext(patch_data.datecode_min, patch_data.datecode_max)) {
                continue;
            }

            // generate hash
            patch_data.hash = patch_hash(patch_data);

            // check for existing
            bool existing = false;
            for (auto &added_patch : patches) {
                if (added_patch.hash == patch_data.hash) {
                    existing = true;
                    break;
                }
            }
            if (existing) {
                continue;
            }

            // hash check for enabled
            for (auto &enabled_entry : setting_patches_enabled) {
                if (patch_data.hash == enabled_entry) {
                    patch_data.enabled = true;
                    patch_data.saved = true;
                }
            }

            // check patch type
            switch (patch_data.type) {
                case PatchType::Memory: {

                    // iterate memory patches
                    auto patches_it = patch.FindMember("patches");
                    if (patches_it == patch.MemberEnd()
                    || !patches_it->value.IsArray()) {
                        log_warning("patchmanager", "unable to get patches for {}",
                                name_it->value.GetString());
                        continue;
                    }
                    for (auto &memory_patch : patches_it->value.GetArray()) {

                        // validate data
                        auto data_disabled_it = memory_patch.FindMember("dataDisabled");
                        if (data_disabled_it == memory_patch.MemberEnd()
                        || !data_disabled_it->value.IsString()) {
                            log_warning("patchmanager", "unable to get data for {}",
                                    name_it->value.GetString());
                            continue;
                        }
                        auto data_enabled_it = memory_patch.FindMember("dataEnabled");
                        if (data_enabled_it == memory_patch.MemberEnd()
                        || !data_enabled_it->value.IsString()) {
                            log_warning("patchmanager", "unable to get data for {}",
                                    name_it->value.GetString());
                            continue;
                        }

                        // get hex strings
                        auto data_disabled_hex = data_disabled_it->value.GetString();
                        auto data_enabled_hex = data_enabled_it->value.GetString();
                        auto data_disabled_hex_len = strlen(data_disabled_hex);
                        auto data_enabled_hex_len = strlen(data_enabled_hex);
                        if ((data_disabled_hex_len % 2) != 0 || (data_enabled_hex_len % 2) != 0) {
                            log_warning("patchmanager", "patch hex data length has odd length for {}",
                                    name_it->value.GetString());
                            continue;
                        }

                        // convert to binary
                        std::shared_ptr<uint8_t[]> data_disabled(new uint8_t[data_disabled_hex_len / 2]);
                        std::shared_ptr<uint8_t[]> data_enabled(new uint8_t[data_enabled_hex_len / 2]);
                        if (!hex2bin(data_disabled_hex, data_disabled.get())
                        || (!hex2bin(data_enabled_hex, data_enabled.get()))) {
                            log_warning("patchmanager", "failed to parse patch data from hex for {}",
                                    name_it->value.GetString());
                            continue;
                        }

                        // get DLL name
                        auto dll_name_it = memory_patch.FindMember("dllName");
                        if (dll_name_it == memory_patch.MemberEnd()
                        || !dll_name_it->value.IsString()) {
                            log_warning("patchmanager", "unable to get dllName for {}",
                                    name_it->value.GetString());
                            continue;
                        }
                        std::string dll_name = dll_name_it->value.GetString();

                        // IIDX omnimix dll name fix
                        if (dll_name == "bm2dx.dll" && avs::game::is_model("LDJ") && avs::game::REV[0] == 'X') {
                            dll_name = avs::game::DLL_NAME;
                        }

                        // BST 1/2 combined release dll name fix
                        if (dll_name == "beatstream.dll" &&
                             (avs::game::DLL_NAME == "beatstream1.dll"
                           || avs::game::DLL_NAME == "beatstream2.dll"))
                        {
                            dll_name = avs::game::DLL_NAME;
                        }

                        // build memory patch data
                        MemoryPatch memory_patch_data {
                                .dll_name = dll_name,
                                .data_disabled = std::move(data_disabled),
                                .data_disabled_len = data_disabled_hex_len / 2,
                                .data_enabled = std::move(data_enabled),
                                .data_enabled_len = data_enabled_hex_len / 2,
                                .data_offset = 0,
                        };

                        // get data offset
                        auto data_offset_it = memory_patch.FindMember("dataOffset");
                        if (data_offset_it == memory_patch.MemberEnd()) {
                            log_warning("patchmanager", "unable to get dataOffset for {}",
                                        name_it->value.GetString());
                            continue;
                        }
                        if (data_offset_it->value.IsUint64()) {
                            memory_patch_data.data_offset = data_offset_it->value.GetUint64();
                        } else if (data_offset_it->value.IsString()) {
                            std::stringstream ss;
                            ss << data_offset_it->value.GetString();
                            ss >> memory_patch_data.data_offset;
                            if (!ss.good() || !ss.eof()) {
                                log_warning("patchmanager", "invalid dataOffset for {}",
                                            name_it->value.GetString());
                                continue;
                            }
                        } else {
                            log_warning("patchmanager", "unable to get dataOffset for {}",
                                        name_it->value.GetString());
                            continue;
                        }

                        // move to list
                        patch_data.patches_memory.emplace_back(memory_patch_data);
                    }
                    break;
                }
                case PatchType::Signature: {

                    // validate data
                    auto data_signature_it = patch.FindMember("signature");
                    if (data_signature_it == patch.MemberEnd()
                        || !data_signature_it->value.IsString()) {
                        log_warning("patchmanager", "unable to get data for {}",
                                    name_it->value.GetString());
                        continue;
                    }
                    auto data_replacement_it = patch.FindMember("replacement");
                    if (data_replacement_it == patch.MemberEnd()
                        || !data_replacement_it->value.IsString()) {
                        log_warning("patchmanager", "unable to get data for {}",
                                    name_it->value.GetString());
                        continue;
                    }

                    // get DLL name
                    auto dll_name_it = patch.FindMember("dllName");
                    if (dll_name_it == patch.MemberEnd()
                        || !dll_name_it->value.IsString()) {
                        log_warning("patchmanager", "unable to get dllName for {}",
                                    name_it->value.GetString());
                        continue;
                    }
                    std::string dll_name = dll_name_it->value.GetString();

                    // IIDX omnimix dll name fix
                    if (dll_name == "bm2dx.dll" && avs::game::is_model("LDJ") && avs::game::REV[0] == 'X') {
                        dll_name = avs::game::DLL_NAME;
                    }

                    // BST 1/2 combined release dll name fix
                    if (dll_name == "beatstream.dll" &&
                        (avs::game::DLL_NAME == "beatstream1.dll"
                         || avs::game::DLL_NAME == "beatstream2.dll"))
                    {
                        dll_name = avs::game::DLL_NAME;
                    }

                    // get optional offset
                    int offset = 0;
                    auto offset_it = patch.FindMember("offset");
                    if (offset_it != patch.MemberEnd()) {
                        bool invalid = false;
                        if (offset_it->value.IsInt64()) {
                            offset = offset_it->value.GetInt64();
                        } else if (offset_it->value.IsString()) {
                            std::stringstream ss;
                            ss << offset_it->value.GetString();
                            ss >> offset;
                            invalid = !ss.good() || !ss.eof();
                        } else {
                            invalid = true;
                        }
                        if (invalid) {
                            log_warning("patchmanager", "invalid offset for {}",
                                        name_it->value.GetString());
                        }
                    }

                    // get optional usage
                    int usage = 0;
                    auto usage_it = patch.FindMember("usage");
                    if (usage_it != patch.MemberEnd()) {
                        bool invalid = false;
                        if (usage_it->value.IsInt64()) {
                            usage = usage_it->value.GetInt64();
                        } else if (usage_it->value.IsString()) {
                            std::stringstream ss;
                            ss << usage_it->value.GetString();
                            ss >> usage;
                            invalid = !ss.good() || !ss.eof();
                        } else {
                            invalid = true;
                        }
                        if (invalid) {
                            log_warning("patchmanager", "invalid usage for {}",
                                        name_it->value.GetString());
                        }
                    }

                    // build signature patch
                    SignaturePatch signature_data = {
                            .dll_name = dll_name,
                            .signature = data_signature_it->value.GetString(),
                            .replacement = data_replacement_it->value.GetString(),
                            .offset = offset,
                            .usage = usage,
                    };

                    // convert to memory patch
                    patch_data.patches_memory.emplace_back(signature_data.to_memory(&patch_data));
                    patch_data.type = PatchType::Memory;
                    break;
                }
                case PatchType::Unknown:
                default:
                    log_warning("patchmanager", "unknown patch type: {}", patch_data.type);
                    break;
            }

            // auto apply
            if (apply_patches && setting_auto_apply && patch_data.enabled) {
                log_misc("patchmanager", "auto apply: {}", patch_data.name);
                apply_patch(patch_data, true);
            }

            // remember patch
            patches.emplace_back(patch_data);
        }
    }

    PatchStatus is_patch_active(PatchData &patch) {

        // check patch type
        switch (patch.type) {
            case PatchType::Memory: {

                // iterate patches
                bool enabled = false;
                bool disabled = false;
                for (auto &memory_patch : patch.patches_memory) {
                    auto max_size = std::max(memory_patch.data_enabled_len, memory_patch.data_disabled_len);

                    // check for error to not try to get the pointer every frame
                    if (memory_patch.fatal_error) {
                        if (cfg::CONFIGURATOR_STANDALONE) {
                            patch.unverified = true;
                            return patch.enabled ? PatchStatus::Enabled : PatchStatus::Disabled;
                        }
                        return PatchStatus::Error;
                    }

                    // check data pointer
                    if (memory_patch.data_offset_ptr == nullptr) {

                        // check if file exists
                        auto dll_path = MODULE_PATH / memory_patch.dll_name;
                        if (!fileutils::file_exists(dll_path)) {

                            // file does not exist so that's pretty fatal
                            memory_patch.fatal_error = true;
                            return PatchStatus::Error;
                        }

                        // standalone mode
                        if (cfg::CONFIGURATOR_STANDALONE) {

                            // load file into dll map if missing
                            auto it = DLL_MAP.find(memory_patch.dll_name);
                            if (it == DLL_MAP.end()) {
                                DLL_MAP[memory_patch.dll_name] =
                                        std::unique_ptr<std::vector<uint8_t>>(
                                                fileutils::bin_read(dll_path));
                            }

                            // check bounds
                            if (memory_patch.data_offset + max_size >= DLL_MAP[memory_patch.dll_name]->size()) {

                                // offset outside of file bounds
                                memory_patch.fatal_error = true;
                                return PatchStatus::Error;
                            } else {

                                // just remember raw file offset
                                memory_patch.data_offset_ptr = reinterpret_cast<uint8_t *>(memory_patch.data_offset);
                            }

                        } else {

                            // get module
                            auto module = libutils::try_module(dll_path);
                            if (!module) {

                                // no fatal error, might just not be loaded yet
                                return PatchStatus::Error;
                            }

                            // convert offset to RVA
                            auto offset = libutils::offset2rva(dll_path, memory_patch.data_offset);
                            if (offset == -1) {

                                // RVA not found means unrecoverable
                                memory_patch.fatal_error = true;
                                return PatchStatus::Error;
                            }

                            // get module information
                            MODULEINFO module_info {};
                            if (!GetModuleInformation(
                                    GetCurrentProcess(),
                                    module,
                                    &module_info,
                                    sizeof(MODULEINFO))) {

                                // hmm, maybe try again sometime, not fatal
                                return PatchStatus::Error;
                            }

                            // check bounds
                            auto max_offset = static_cast<uintptr_t>(offset) + max_size;
                            auto image_size = static_cast<uintptr_t>(module_info.SizeOfImage);
                            if (max_offset >= image_size) {

                                // outside of bounds, invalid patch, fatal
                                memory_patch.fatal_error = true;
                                return PatchStatus::Error;
                            }

                            // save pointer
                            auto dll_base = reinterpret_cast<uintptr_t>(module_info.lpBaseOfDll);
                            memory_patch.data_offset_ptr = reinterpret_cast<uint8_t *>(dll_base + offset);
                        }
                    }

                    // standalone mode
                    if (cfg::CONFIGURATOR_STANDALONE) {
                        auto &file = DLL_MAP[memory_patch.dll_name];
                        if (!file) {
                            return PatchStatus::Error;
                        }
                        memory_patch.data_offset_ptr = &(*file)[memory_patch.data_offset];
                    }

                    // virtual protect
                    memutils::VProtectGuard guard(memory_patch.data_offset_ptr, max_size);

                    // compare
                    if (!guard.is_bad_address() && !memcmp(
                                memory_patch.data_enabled.get(),
                                memory_patch.data_offset_ptr,
                                memory_patch.data_enabled_len))
                    {
                        enabled = true;
                    } else if (!guard.is_bad_address() && !memcmp(
                                memory_patch.data_disabled.get(),
                                memory_patch.data_offset_ptr,
                                memory_patch.data_disabled_len))
                    {
                        disabled = true;
                    } else {
                        return PatchStatus::Error;
                    }
                }

                // check detection flags
                if (enabled && disabled) {
                    return PatchStatus::Error;
                } else if (enabled) {
                    return PatchStatus::Enabled;
                } else if (disabled) {
                    return PatchStatus::Disabled;
                } else {
                    return PatchStatus::Error;
                }
            }
            case PatchType::Signature: {
                return PatchStatus::Error;
            }
            case PatchType::Unknown:
            default:
                return PatchStatus::Error;
        }
    }

    bool apply_patch(PatchData &patch, bool active) {

        // check patch type
        switch (patch.type) {
            case PatchType::Memory: {

                // iterate memory patches
                for (auto &memory_patch : patch.patches_memory) {

                    /*
                     * we won't use the cached data_offset_ptr here
                     * that makes it more reliable, also only happens on load/toggle
                     */

                    // determine source/target buffer/size
                    uint8_t *src_buf = active
                            ? memory_patch.data_disabled.get()
                            : memory_patch.data_enabled.get();
                    size_t src_len = active
                            ? memory_patch.data_disabled_len
                            : memory_patch.data_enabled_len;
                    uint8_t *target_buf = active
                            ? memory_patch.data_enabled.get()
                            : memory_patch.data_disabled.get();
                    size_t target_len = active
                            ? memory_patch.data_enabled_len
                            : memory_patch.data_disabled_len;

                    // standalone mode
                    if (cfg::CONFIGURATOR_STANDALONE) {

                        // load file into dll map if missing
                        auto it = DLL_MAP.find(memory_patch.dll_name);
                        if (it == DLL_MAP.end()) {
                            DLL_MAP[memory_patch.dll_name] =
                                    std::unique_ptr<std::vector<uint8_t>>(
                                            fileutils::bin_read(MODULE_PATH / memory_patch.dll_name));
                        }

                        // find file
                        auto &dll_file = DLL_MAP[memory_patch.dll_name];
                        if (!dll_file) {
                            return false;
                        }

                        // check bounds
                        auto max_len = std::max(src_len, target_len);
                        if (memory_patch.data_offset + max_len >= dll_file->size()) {
                            return false;
                        }

                        // copy target to memory if src matches
                        auto offset_ptr = &(*dll_file)[memory_patch.data_offset];
                        if (memcmp(offset_ptr, src_buf, src_len) == 0) {
                            memcpy(offset_ptr, target_buf, target_len);
                        }

                    } else {

                        // check pointer
                        auto max_len = std::max(src_len, target_len);
                        uint8_t *offset_ptr = memory_patch.data_offset_ptr;
                        if (offset_ptr == nullptr) {

                            // check if file exists
                            auto dll_path = MODULE_PATH / memory_patch.dll_name;
                            if (!fileutils::file_exists(dll_path)) {
                                return false;
                            }

                            // get module
                            auto module = libutils::try_module(dll_path);
                            if (!module) {
                                return false;
                            }

                            // convert offset to RVA
                            auto offset = libutils::offset2rva(dll_path, (intptr_t) memory_patch.data_offset);
                            if (offset == -1) {
                                return false;
                            }

                            // get module information
                            MODULEINFO module_info {};
                            if (!GetModuleInformation(
                                    GetCurrentProcess(),
                                    module,
                                    &module_info,
                                    sizeof(MODULEINFO))) {
                                return false;
                            }

                            // transmute pointer
                            auto dll_base = reinterpret_cast<uint8_t *>(module_info.lpBaseOfDll);
                            auto dll_image_size = static_cast<uintptr_t>(module_info.SizeOfImage);

                            // check bounds
                            auto max_offset = static_cast<uintptr_t>(offset + max_len);
                            if (max_offset >= dll_image_size) {
                                return false;
                            }

                            // get pointer
                            offset_ptr = &dll_base[offset];
                        }

                        // virtual protect
                        memutils::VProtectGuard guard(offset_ptr, max_len);

                        // copy target to memory if src matches
                        if (memcmp(offset_ptr, src_buf, src_len) == 0) {
                            memcpy(offset_ptr, target_buf, target_len);
                        }
                    }
                }

                // success
                return true;
            }
            case PatchType::Signature: {
                return false;
            }
            default: {

                // unknown patch type - fail
                return false;
            }
        }
    }

    MemoryPatch SignaturePatch::to_memory(PatchData *patch) {

        // check if file exists
        auto dll_path = MODULE_PATH / dll_name;
        if (!fileutils::file_exists(dll_path)) {

            // file does not exist so that's pretty fatal
            return {.fatal_error = true};
        }

        // remove spaces
        signature.erase(std::remove(signature.begin(), signature.end(), ' '), signature.end());
        replacement.erase(std::remove(replacement.begin(), replacement.end(), ' '), replacement.end());

        // build pattern
        std::string pattern_str(signature);
        strreplace(pattern_str, "??", "00");
        strreplace(pattern_str, "XX", "00");
        auto pattern_bin = std::make_unique<uint8_t[]>(signature.length() / 2);
        if (!hex2bin(pattern_str.c_str(), pattern_bin.get())) {
            return {.fatal_error = true};
        }

        // build signature mask
        std::ostringstream signature_mask;
        for (size_t i = 0; i < signature.length(); i += 2) {
            if (signature[i] == '?' || signature[i] == 'X') {
                if (signature[i + 1] == '?' || signature[i + 1] == 'X') {
                    signature_mask << '?';
                } else {
                    return {.fatal_error = true};
                }
            } else {
                signature_mask << 'X';
            }
        }
        std::string signature_mask_str = signature_mask.str();

        // build replace data
        std::string replace_data_str(replacement);
        strreplace(replace_data_str, "??", "00");
        strreplace(replace_data_str, "XX", "00");
        auto replace_data_bin = std::make_unique<uint8_t[]>(replacement.length() / 2);
        if (!hex2bin(replace_data_str.c_str(), replace_data_bin.get())) {
            return {.fatal_error = true};
        }

        // build replace mask
        std::ostringstream replace_mask;
        for (size_t i = 0; i < replacement.length(); i += 2) {
            if (replacement[i] == '?' || replacement[i] == 'X') {
                if (replacement[i + 1] == '?' || replacement[i + 1] == 'X') {
                    replace_mask << '?';
                } else {
                    return {.fatal_error = true};
                }
            } else {
                replace_mask << 'X';
            }
        }
        std::string replace_mask_str = replace_mask.str();

        // find offset
        uint64_t data_offset = 0;
        uint8_t *data_offset_ptr = nullptr;
        uintptr_t data_offset_ptr_base = 0;
        if (cfg::CONFIGURATOR_STANDALONE) {

            // load file into dll map if missing
            auto it = DLL_MAP.find(dll_name);
            if (it == DLL_MAP.end()) {
                DLL_MAP[dll_name] =
                        std::unique_ptr<std::vector<uint8_t>>(
                                fileutils::bin_read(dll_path));
                it = DLL_MAP.find(dll_name);
            }

            // find pattern
            data_offset = find_pattern(*it->second, 0, pattern_bin.get(), signature_mask_str.c_str(), offset, usage);
            data_offset_ptr = reinterpret_cast<uint8_t *>(data_offset);
            data_offset_ptr_base = (uintptr_t) it->second->data();

        } else {

            // get module
            auto module = libutils::try_module(dll_path);
            bool module_free = false;
            if (!module) {
                module = libutils::try_library(dll_path);
                if (module) {
                    module_free = true;
                } else {
                    return {.fatal_error = true};
                }
            }

            // find pattern
            data_offset_ptr = reinterpret_cast<uint8_t *>(
                    find_pattern(module, pattern_bin.get(), signature_mask_str.c_str(), offset, usage));

            // convert back to offset
            data_offset = libutils::rva2offset(dll_path, (intptr_t) (data_offset_ptr - (uint8_t*) module));

            // clean
            if (module_free) {
                FreeLibrary(module);
            }
        }

        // check pointers
        if (data_offset_ptr == nullptr) {
            return {.fatal_error = true};
        }

        // get disabled/enabled data
        size_t data_len = std::max(signature_mask_str.length(), replace_mask_str.length());
        std::shared_ptr<uint8_t[]> data_disabled(new uint8_t[data_len]);
        std::shared_ptr<uint8_t[]> data_enabled(new uint8_t[data_len]);
        memutils::VProtectGuard data_guard(data_offset_ptr + data_offset_ptr_base, data_len);
        for (size_t i = 0; i < data_len; ++i) {
            if (i >= signature_mask_str.length() || signature_mask_str[i] != 'X') {
                data_disabled.get()[i] = (data_offset_ptr + data_offset_ptr_base)[i];
            } else {
                data_disabled.get()[i] = pattern_bin.get()[i];
            }
        }
        for (size_t i = 0; i < data_len; ++i) {
            if (i >= replace_mask_str.length() || replace_mask_str[i] != 'X') {
                data_enabled.get()[i] = (data_offset_ptr + data_offset_ptr_base)[i];
            } else {
                data_enabled.get()[i] = replace_data_bin.get()[i];
            }
        }

        // log edit
        log_misc("patchmanager", "found {}: {:#08X}: {} -> {}",
                 patch->name, data_offset,
                 bin2hex(data_disabled.get(), data_len),
                 bin2hex(data_enabled.get(), data_len));

        // build patch
        return MemoryPatch {
                .dll_name = dll_name,
                .data_disabled = std::move(data_disabled),
                .data_disabled_len = data_len,
                .data_enabled = std::move(data_enabled),
                .data_enabled_len = data_len,
                .data_offset = data_offset,
                .data_offset_ptr = data_offset_ptr,
        };
    }
}

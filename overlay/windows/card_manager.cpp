#include <games/io.h>
#include "card_manager.h"

#include "external/rapidjson/document.h"
#include "external/rapidjson/writer.h"
#include "misc/eamuse.h"
#include "util/utils.h"
#include "util/fileutils.h"

using namespace rapidjson;


namespace overlay::windows {

    CardManager::CardManager(SpiceOverlay *overlay) : Window(overlay) {
        this->title = "Card Manager";
        this->init_size = ImVec2(300, 200);
        this->init_pos = ImVec2(
                ImGui::GetIO().DisplaySize.x / 2 - this->init_size.x / 2,
                ImGui::GetIO().DisplaySize.y / 2 - this->init_size.y / 2);
        this->toggle_button = games::OverlayButtons::ToggleCardManager;
        this->config_path = std::string(getenv("APPDATA")) + "\\spicetools_card_manager.json";
        if (fileutils::file_exists(this->config_path)) {
            this->config_load();
        }
    }

    CardManager::~CardManager() {
    }

    void CardManager::build_content() {

        // get window size
        auto window_size = ImGui::GetWindowSize();

        // name field
        ImGui::InputTextWithHint("Card Name", "Main Card",
                this->name_buffer, std::size(this->name_buffer));

        // card field
        ImGui::InputTextWithHint("Card ID", "E0040123456789AB",
                this->card_buffer,
                std::size(this->card_buffer),
                ImGuiInputTextFlags_CharsHexadecimal
                | ImGuiInputTextFlags_CharsUppercase);

        // add card button
        if (strlen(this->card_buffer) == 16) {
            if (ImGui::Button("Add Card", ImVec2(-1.f, 0.f))) {

                // save entry
                CardEntry entry {
                    .name = this->name_buffer,
                    .id = this->card_buffer
                };
                this->cards.emplace_back(entry);
                this->config_dirty = true;

                // clear input fields
                memset(this->name_buffer, 0, sizeof(this->name_buffer));
                memset(this->card_buffer, 0, sizeof(this->card_buffer));
            }
        } else {
            ImGui::Text("Enter card identifier...");
        }

        // cards area
        ImGui::Separator();
        if (ImGui::BeginChild("cards", ImVec2(0, window_size.y - 128))) {
            for (auto &card : this->cards) {

                // get card name
                std::string card_name = card.name;
                if (card.name.size() > 0) {
                    card_name += " - ";
                }
                card_name += card.id;

                // draw entry
                ImGui::PushID(&card);
                if (ImGui::Selectable(card_name.c_str(), card.selected)) {

                    // unselect other cards
                    for (auto &card_disable : this->cards) {
                        card_disable.selected = false;
                    }

                    // mark this card as the selected one
                    card.selected = true;
                }
                ImGui::PopID();
            }
        }
        ImGui::EndChild();

        // insert P1 button
        if (ImGui::Button("Insert P1")) {
            auto card = this->cards_get_selected();
            uint8_t card_bin[8];
            if (card && card->id.length() == 16 && hex2bin(card->id.c_str(), card_bin)) {
                eamuse_card_insert(0, card_bin);
            }
        }

        // insert P2 button
        if (eamuse_get_game_keypads() > 1) {
            ImGui::SameLine();
            if (ImGui::Button("Insert P2")) {
                auto card = this->cards_get_selected();
                uint8_t card_bin[8];
                if (card && card->id.length() == 16 && hex2bin(card->id.c_str(), card_bin)) {
                    eamuse_card_insert(1, card_bin);
                }
            }
        }

        // save button
        if (this->config_dirty) {
            ImGui::SameLine();
            if (ImGui::Button("Save")) {
                this->config_save();
            }
        }
    }

    CardEntry *CardManager::cards_get_selected() {

        // iterate cards
        for (auto &card : this->cards) {

            // check if selected and return pointer
            if (card.selected) {
                return &card;
            }
        }

        // no card selected
        return nullptr;
    }

    void CardManager::config_load() {
        log_info("cardmanager", "loading config");

        // clear cards
        this->cards.clear();

        // read config file
        std::string config = fileutils::text_read(this->config_path);
        if (!config.empty()) {

            // parse document
            Document doc;
            doc.Parse(config.c_str());

            // check parse error
            auto error = doc.GetParseError();
            if (error) {
                log_warning("cardmanager", "config parse error: {}", error);
            }

            // verify root is a dict
            if (doc.IsObject()) {

                // find pages
                auto pages = doc.FindMember("pages");
                if (pages != doc.MemberEnd() && pages->value.IsArray()) {

                    // iterate pages
                    for (auto &page : pages->value.GetArray()) {
                        if (page.IsObject()) {

                            // get cards
                            auto cards = page.FindMember("cards");
                            if (cards != doc.MemberEnd() && cards->value.IsArray()) {

                                // iterate cards
                                for (auto &card : cards->value.GetArray()) {
                                    if (card.IsObject()) {

                                        // find attributes
                                        auto name = card.FindMember("name");
                                        if (name == doc.MemberEnd() || !name->value.IsString()) {
                                            log_warning("cardmanager", "card name not found");
                                            continue;
                                        }
                                        auto id = card.FindMember("id");
                                        if (id == doc.MemberEnd() || !id->value.IsString()) {
                                            log_warning("cardmanager", "card id not found");
                                            continue;
                                        }

                                        // save entry
                                        CardEntry entry {
                                                .name = name->value.GetString(),
                                                .id = id->value.GetString()
                                        };
                                        this->cards.emplace_back(entry);

                                    } else {
                                        log_warning("cardmanager", "card is not an object");
                                    }
                                }
                            } else {
                                log_warning("cardmanager", "cards not found or not an array");
                            }
                        } else {
                            log_warning("cardmanager", "page is not an object");
                        }
                    }
                } else {
                    log_warning("cardmanager", "pages not found or not an array");
                }
            }
        }
    }

    void CardManager::config_save() {
        log_info("cardmanager", "saving config");

        // create document
        Document doc;
        doc.Parse(
                "{"
                "  \"pages\": ["
                "    {"
                "      \"cards\": ["
                "      ]"
                "    }"
                "  ]"
                "}"
        );

        // check parse error
        auto error = doc.GetParseError();
        if (error) {
            log_warning("cardmanager", "template parse error: {}", error);
        }

        // add cards
        auto &cards = doc["pages"][0]["cards"];
        for (auto &entry : this->cards) {
            Value card(kObjectType);
            card.AddMember("name", StringRef(entry.name.c_str()), doc.GetAllocator());
            card.AddMember("id", StringRef(entry.id.c_str()), doc.GetAllocator());
            cards.PushBack(card, doc.GetAllocator());
        }

        // build JSON
        StringBuffer buffer;
        Writer<StringBuffer> writer(buffer);
        doc.Accept(writer);

        // save to file
        if (fileutils::text_write(this->config_path, buffer.GetString())) {
            this->config_dirty = false;
        } else {
            log_warning("cardmanager", "unable to save config file to {}", this->config_path);
        }
    }
}

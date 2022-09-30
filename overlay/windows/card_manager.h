#pragma once

#include "overlay/window.h"

namespace overlay::windows {

    struct CardEntry {
        std::string name = "unnamed";
        std::string id = "E004000000000000";
        bool selected = false;
    };

    class CardManager : public Window {
    public:

        CardManager(SpiceOverlay *overlay);
        ~CardManager() override;

        void build_content() override;

    private:

        std::string config_path;
        bool config_dirty = false;
        std::vector<CardEntry> cards;
        char name_buffer[65] {};
        char card_buffer[17] {};

        CardEntry *cards_get_selected();
        void config_load();
        void config_save();
    };
}

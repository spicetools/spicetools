#pragma once

#include <string>
#include <utility>
#include <vector>

enum class OptionType {
    Bool,
    Text,
    Integer,
    Enum,
};

struct OptionDefinition {
    std::string title;
    std::string name;
    std::string desc;
    OptionType type;
    bool hidden = false;
    std::string setting_name = "";
    std::string game_name = "";
    std::string category = "Development";
    bool sensitive = false;
    std::vector<std::pair<std::string, std::string>> elements = {};
};

class Option {
private:
    OptionDefinition definition;

public:
    std::string value;
    std::vector<Option> alternatives;
    bool disabled = false;

    explicit Option(OptionDefinition definition, std::string value = "") :
        definition(std::move(definition)), value(std::move(value)) {};

    inline const OptionDefinition &get_definition() const {
        return this->definition;
    }
    inline void set_definition(OptionDefinition definition) {
        this->definition = std::move(definition);
    }

    inline bool is_active() const {
        return !this->value.empty();
    }

    void value_add(std::string new_value);

    bool has_alternatives() const;
    bool value_bool() const;
    const std::string &value_text() const;
    std::vector<std::string> values() const;
    std::vector<std::string> values_text() const;
    int value_int() const;
};

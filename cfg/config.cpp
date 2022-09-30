#include "config.h"
#include "util/logging.h"

/*
 * This code absolutely sucks.
 * If you hate, then replace, it's long overdue.
 * Maybe use RapidJSON instead so we can ditch tinyxml2 completely.
 */

// settings
std::string CONFIG_PATH_OVERRIDE = "";


///////////////////
/// Constructor ///
///////////////////

Config::Config() {
    this->status = false;
    if (CONFIG_PATH_OVERRIDE.length() > 0) {
        this->configLocation = CONFIG_PATH_OVERRIDE;
    } else {
        this->configLocation = std::string(getenv("APPDATA")) + "\\spicetools.xml";
    }

    tinyxml2::XMLError configLoadError, *previousConfigLoadError = nullptr;

    do {
        configLoadError = this->configFile.LoadFile(this->configLocation.c_str());

        if (previousConfigLoadError == nullptr) {
            previousConfigLoadError = new tinyxml2::XMLError(configLoadError);
        } else if (configLoadError == *previousConfigLoadError) {
            break;
        }

        switch (configLoadError) {
            case tinyxml2::XMLError::XML_SUCCESS:
                this->status = true;
                break;
            case tinyxml2::XMLError::XML_ERROR_EMPTY_DOCUMENT:
                this->firstFillConfigFile();
                break;
            case tinyxml2::XMLError::XML_ERROR_FILE_COULD_NOT_BE_OPENED:
                log_fatal("cfg", "could not open config file: {}", this->configLocation);
                break;
            case tinyxml2::XMLError::XML_ERROR_FILE_NOT_FOUND:
                this->createConfigFile();
                break;
            case tinyxml2::XMLError::XML_ERROR_FILE_READ_ERROR:
            case tinyxml2::XMLError::XML_ERROR_PARSING_ELEMENT:
            case tinyxml2::XMLError::XML_ERROR_PARSING_ATTRIBUTE:
            case tinyxml2::XMLError::XML_ERROR_PARSING_TEXT:
            case tinyxml2::XMLError::XML_ERROR_PARSING_CDATA:
            case tinyxml2::XMLError::XML_ERROR_PARSING_COMMENT:
            case tinyxml2::XMLError::XML_ERROR_PARSING_DECLARATION:
            case tinyxml2::XMLError::XML_ERROR_PARSING_UNKNOWN:
            case tinyxml2::XMLError::XML_ERROR_MISMATCHED_ELEMENT:
            case tinyxml2::XMLError::XML_ERROR_PARSING:
                log_warning("cfg", "Couldn't read config file: {}", this->configLocation);
                this->createConfigFile();
                this->firstFillConfigFile();
                break;
            default:
                log_warning("cfg", "Unknown XML error reading config: {}", configLoadError);
                break;
        }
    } while (configLoadError != tinyxml2::XMLError::XML_SUCCESS);

    this->configFile.SetBOM(true);
}

////////////////////////
/// Public Functions ///
////////////////////////

Config &Config::getInstance() {
    static auto instance = new Config;
    return *instance;
}

bool Config::getStatus() {
    return this->status;
}

bool Config::addGame(Game &game) {
    tinyxml2::XMLNode *rootNode = this->configFile.LastChild();
    tinyxml2::XMLElement *gameNodes = rootNode->FirstChildElement("game");

    bool gameExists = false;

    while (gameNodes != nullptr) {
        const char *gameName = gameNodes->Attribute("name");

        if (gameName == nullptr) {
            rootNode->DeleteChild(gameNodes);
        } else if (std::string(gameName) == game.getGameName()) {
            gameExists = true;
            break;
        }

        gameNodes = gameNodes->NextSiblingElement("game");
    }

    if (gameExists) {

        // buttons list
        tinyxml2::XMLElement *gameButtonsNode = gameNodes->FirstChildElement("buttons");
        if (gameButtonsNode == nullptr) {
            gameButtonsNode = this->configFile.NewElement("buttons");
            gameNodes->InsertEndChild(gameButtonsNode);
            gameButtonsNode = gameNodes->FirstChildElement("buttons");
        }

        // iterate buttons in game
        for (auto &it : game.getButtons()) {

            // iterate buttons in config
            size_t button_count = 0;
            tinyxml2::XMLElement *gameButtonNode = gameButtonsNode->FirstChildElement("button");
            while (gameButtonNode != nullptr) {

                // verify button name
                const char *buttonName = gameButtonNode->Attribute("name");

                if (buttonName == nullptr) {
                    gameButtonsNode->DeleteChild(gameButtonNode);
                } else if (std::string(buttonName) == it.getName()) {

                    // alternative binding stuff
                    Button *button;
                    if (button_count == 0) {
                        button = &it;
                        button_count++;
                    } else {
                        auto &alternatives = it.getAlternatives();
                        if (alternatives.size() > button_count - 1) {
                            button = &alternatives.at(button_count - 1);
                        } else {
                            button = &alternatives.emplace_back(it.getName());
                        }
                        button_count++;
                    }

                    // process button
                    int vKey = 255;
                    auto analogType = (int) BAT_NONE;
                    double debounce_up = 0.0;
                    double debounce_down = 0.0;
                    bool invert = false;
                    tinyxml2::XMLError attrError = gameButtonNode->QueryIntAttribute("vkey", &vKey);
                    const char *devid = gameButtonNode->Attribute("devid");
                    gameButtonNode->QueryIntAttribute("analogtype", &analogType);
                    gameButtonNode->QueryDoubleAttribute("debounce_up", &debounce_up);
                    gameButtonNode->QueryDoubleAttribute("debounce_down", &debounce_down);
                    gameButtonNode->QueryBoolAttribute("invert", &invert);
                    if (attrError != tinyxml2::XMLError::XML_SUCCESS) {
                        gameButtonsNode->DeleteChild(gameButtonNode);
                        gameButtonNode = this->configFile.NewElement("button");
                        gameButtonNode->SetAttribute("name", button->getName().c_str());
                        gameButtonNode->SetAttribute("vkey", button->getVKey());
                        gameButtonNode->SetAttribute("analogtype", (int) button->getAnalogType());
                        gameButtonNode->SetAttribute("devid", button->getDeviceIdentifier().c_str());
                        gameButtonNode->SetAttribute("debounce_up", debounce_up);
                        gameButtonNode->SetAttribute("debounce_down", debounce_down);
                        gameButtonNode->SetAttribute("invert", invert);
                        gameButtonsNode->InsertEndChild(gameButtonNode);
                    } else {
                        button->setVKey(static_cast<unsigned short int>(vKey));
                        button->setAnalogType((ButtonAnalogType) analogType);
                        button->setDebounceUp(debounce_up);
                        button->setDebounceDown(debounce_down);
                        button->setInvert(invert);
                        if (devid) {
                            button->setDeviceIdentifier(devid);
                        }
                    }
                }

                gameButtonNode = gameButtonNode->NextSiblingElement("button");
            }

            // create button entry if none was found
            if (button_count == 0) {
                gameButtonNode = this->configFile.NewElement("button");
                gameButtonNode->SetAttribute("name", it.getName().c_str());
                gameButtonNode->SetAttribute("vkey", it.getVKey());
                gameButtonNode->SetAttribute("analogtype", (int) it.getAnalogType());
                gameButtonNode->SetAttribute("debounce_up", it.getDebounceUp());
                gameButtonNode->SetAttribute("debounce_down", it.getDebounceDown());
                gameButtonNode->SetAttribute("invert", it.getInvert());
                gameButtonNode->SetAttribute("devid", it.getDeviceIdentifier().c_str());
                gameButtonsNode->InsertEndChild(gameButtonNode);
            }
        }

        // analogs list
        tinyxml2::XMLElement *gameAnalogsNode = gameNodes->FirstChildElement("analogs");
        if (gameAnalogsNode == nullptr) {
            gameAnalogsNode = this->configFile.NewElement("analogs");
            gameNodes->InsertEndChild(gameAnalogsNode);
            gameAnalogsNode = gameNodes->FirstChildElement("analogs");
        }

        for (auto &it : game.getAnalogs()) {
            bool analogExists = false;

            tinyxml2::XMLElement *gameAnalogNode = gameAnalogsNode->FirstChildElement("analog");
            while (gameAnalogNode != nullptr) {
                const char *analogName = gameAnalogNode->Attribute("name");

                if (analogName == nullptr) {
                    gameAnalogsNode->DeleteChild(gameAnalogNode);
                } else if (std::string(analogName) == it.getName()) {
                    analogExists = true;
                    break;
                }

                gameAnalogNode = gameAnalogNode->NextSiblingElement("analog");
            }

            if (analogExists) {
                int index = 255;
                float sensitivity = 1.f;
                float deadzone = 0.f;
                bool deadzone_mirror = false;
                bool invert = false;
                bool smoothing = false;
                tinyxml2::XMLError err1 = gameAnalogNode->QueryIntAttribute("index", &index);
                gameAnalogNode->QueryFloatAttribute("sensivity", &sensitivity);
                gameAnalogNode->QueryFloatAttribute("deadzone", &deadzone);
                gameAnalogNode->QueryBoolAttribute("deadzone_mirror", &deadzone_mirror);
                gameAnalogNode->QueryBoolAttribute("invert", &invert);
                gameAnalogNode->QueryBoolAttribute("smoothing", &smoothing);
                const char *devid = gameAnalogNode->Attribute("devid");

                if (err1 != tinyxml2::XMLError::XML_SUCCESS || !devid) {
                    gameAnalogsNode->DeleteChild(gameAnalogNode);
                    gameAnalogNode = this->configFile.NewElement("analog");
                    gameAnalogNode->SetAttribute("name", it.getName().c_str());
                    gameAnalogNode->SetAttribute("index", it.getIndex());
                    gameAnalogNode->SetAttribute("sensivity", it.getSensitivity());
                    gameAnalogNode->SetAttribute("deadzone", it.getDeadzone());
                    gameAnalogNode->SetAttribute("devid", it.getDeviceIdentifier().c_str());
                    gameAnalogNode->SetAttribute("deadzone_mirror", it.getDeadzoneMirror());
                    gameAnalogNode->SetAttribute("invert", it.getInvert());
                    gameAnalogNode->SetAttribute("smoothing", it.getSmoothing());
                    gameAnalogsNode->InsertEndChild(gameAnalogNode);
                } else {
                    it.setIndex(static_cast<unsigned short int>(index));
                    it.setSensitivity(sensitivity);
                    it.setDeadzone(deadzone);
                    it.setDeviceIdentifier(devid);
                    it.setDeadzoneMirror(deadzone_mirror);
                    it.setInvert(invert);
                    it.setSmoothing(smoothing);
                }
            } else {
                gameAnalogNode = this->configFile.NewElement("analog");
                gameAnalogNode->SetAttribute("name", it.getName().c_str());
                gameAnalogNode->SetAttribute("index", it.getIndex());
                gameAnalogNode->SetAttribute("sensivity", it.getSensitivity());
                gameAnalogNode->SetAttribute("deadzone", it.getDeadzone());
                gameAnalogNode->SetAttribute("deadzone_mirror", it.getDeadzoneMirror());
                gameAnalogNode->SetAttribute("invert", it.getInvert());
                gameAnalogNode->SetAttribute("smoothing", it.getSmoothing());
                gameAnalogNode->SetAttribute("devid", it.getDeviceIdentifier().c_str());
                gameAnalogsNode->InsertEndChild(gameAnalogNode);
            }
        }

        // lights list
        tinyxml2::XMLElement *gameLightsNode = gameNodes->FirstChildElement("lights");
        if (gameLightsNode == nullptr) {
            gameLightsNode = this->configFile.NewElement("lights");
            gameNodes->InsertEndChild(gameLightsNode);
            gameLightsNode = gameNodes->FirstChildElement("lights");
        }

        // iterate lights in game
        for (auto &it : game.getLights()) {

            // iterate lights in config
            size_t light_count = 0;
            tinyxml2::XMLElement *gameLightNode = gameLightsNode->FirstChildElement("light");
            while (gameLightNode != nullptr) {

                // verify light name
                const char *lightName = gameLightNode->Attribute("name");
                if (lightName == nullptr) {
                    gameLightsNode->DeleteChild(gameLightNode);
                } else if (std::string(lightName) == it.getName()) {

                    // alternative binding stuff
                    Light *light;
                    if (light_count == 0) {
                        light = &it;
                        light_count++;
                    } else {
                        auto &alternatives = it.getAlternatives();
                        if (alternatives.size() > light_count - 1) {
                            light = &alternatives.at(light_count - 1);
                        } else {
                            light = &alternatives.emplace_back(it.getName());
                        }
                        light_count++;
                    }

                    // process light
                    int index = 0;
                    tinyxml2::XMLError attrError = gameLightNode->QueryIntAttribute("index", &index);
                    const char *devid = gameLightNode->Attribute("devid");
                    if (attrError != tinyxml2::XMLError::XML_SUCCESS) {
                        gameLightsNode->DeleteChild(gameLightNode);
                        gameLightNode = this->configFile.NewElement("light");
                        gameLightNode->SetAttribute("name", light->getName().c_str());
                        gameLightNode->SetAttribute("index", light->getIndex());
                        gameLightNode->SetAttribute("devid", light->getDeviceIdentifier().c_str());
                        gameLightsNode->InsertEndChild(gameLightNode);
                    } else {
                        light->setIndex(static_cast<unsigned int>(index));
                        if (devid) {
                            light->setDeviceIdentifier(devid);
                        }
                    }
                }
                gameLightNode = gameLightNode->NextSiblingElement("light");
            }

            // create light entry if none was found
            if (light_count == 0) {
                gameLightNode = this->configFile.NewElement("light");
                gameLightNode->SetAttribute("name", it.getName().c_str());
                gameLightNode->SetAttribute("index", it.getIndex());
                gameLightNode->SetAttribute("devid", it.getDeviceIdentifier().c_str());
                gameLightsNode->InsertEndChild(gameLightNode);
            }
        }

        // options list
        tinyxml2::XMLElement *gameOptionsNode = gameNodes->FirstChildElement("options");
        if (gameOptionsNode == nullptr) {
            gameOptionsNode = this->configFile.NewElement("options");
            gameNodes->InsertEndChild(gameOptionsNode);
            gameOptionsNode = gameNodes->FirstChildElement("options");
        }

        for (auto &it : game.getOptions()) {
            bool optionExist = false;

            tinyxml2::XMLElement *gameOptionNode = gameOptionsNode->FirstChildElement("option");
            while (gameOptionNode != nullptr) {
                const char *optionName = gameOptionNode->Attribute("name");

                if (optionName == nullptr) {
                    gameOptionsNode->DeleteChild(gameOptionNode);
                } else if (std::string(optionName) == it.get_definition().name) {
                    optionExist = true;
                    break;
                }

                gameOptionNode = gameOptionNode->NextSiblingElement("option");
            }

            if (optionExist) {
                const char *name = gameOptionNode->Attribute("name");

                if (name == nullptr) {
                    gameOptionsNode->DeleteChild(gameOptionNode);
                    gameOptionNode = this->configFile.NewElement("options");
                    gameOptionNode->SetAttribute("name", it.get_definition().name.c_str());
                    gameOptionNode->SetAttribute("value", it.value.c_str());
                    gameOptionsNode->InsertEndChild(gameOptionNode);
                } else {
                    it.value = gameOptionNode->Attribute("value");
                }
            } else {
                gameOptionNode = this->configFile.NewElement("option");
                gameOptionNode->SetAttribute("name", it.get_definition().name.c_str());
                gameOptionNode->SetAttribute("value", it.value.c_str());
                gameOptionsNode->InsertEndChild(gameOptionNode);
            }
        }
    } else {

        // game does not exist, create it
        tinyxml2::XMLElement *gameNode = this->configFile.NewElement("game");
        gameNode->SetAttribute("name", game.getGameName().c_str());

        // create buttons
        tinyxml2::XMLElement *gameButtonsNode = this->configFile.NewElement("buttons");
        gameNode->InsertEndChild(gameButtonsNode);
        for (auto &it : game.getButtons()) {
            tinyxml2::XMLElement *gameButtonNode = this->configFile.NewElement("button");
            gameButtonNode->SetAttribute("name", it.getName().c_str());
            gameButtonNode->SetAttribute("vkey", it.getVKey());
            gameButtonNode->SetAttribute("analogtype", it.getAnalogType());
            gameButtonNode->SetAttribute("debounce_up", it.getDebounceUp());
            gameButtonNode->SetAttribute("debounce_down", it.getDebounceDown());
            gameButtonNode->SetAttribute("invert", it.getInvert());
            gameButtonNode->SetAttribute("devid", it.getDeviceIdentifier().c_str());
            gameButtonsNode->InsertEndChild(gameButtonNode);
        }

        // create analogs
        tinyxml2::XMLElement *gameAnalogsNode = this->configFile.NewElement("analogs");
        gameNode->InsertEndChild(gameAnalogsNode);
        for (auto &it : game.getAnalogs()) {
            tinyxml2::XMLElement *gameAnalogNode = this->configFile.NewElement("analog");
            gameAnalogNode->SetAttribute("name", it.getName().c_str());
            gameAnalogNode->SetAttribute("devid", it.getDeviceIdentifier().c_str());
            gameAnalogNode->SetAttribute("sensivity", it.getSensitivity());
            gameAnalogNode->SetAttribute("deadzone", it.getDeadzone());
            gameAnalogNode->SetAttribute("deadzone_mirror", it.getDeadzoneMirror());
            gameAnalogNode->SetAttribute("invert", it.getInvert());
            gameAnalogNode->SetAttribute("smoothing", it.getSmoothing());
            gameAnalogsNode->InsertEndChild(gameAnalogNode);
        }

        // create lights
        tinyxml2::XMLElement *gameLightsNode = this->configFile.NewElement("lights");
        gameNode->InsertEndChild(gameLightsNode);
        for (auto &it : game.getLights()) {
            tinyxml2::XMLElement *gameLightNode = this->configFile.NewElement("light");
            gameLightNode->SetAttribute("name", it.getName().c_str());
            gameLightNode->SetAttribute("index", it.getIndex());
            gameLightNode->SetAttribute("devid", it.getDeviceIdentifier().c_str());
            gameLightsNode->InsertEndChild(gameLightNode);
        }

        // create options
        tinyxml2::XMLElement *gameOptionsNode = this->configFile.NewElement("options");
        gameNode->InsertEndChild(gameOptionsNode);
        for (auto &it : game.getOptions()) {
            tinyxml2::XMLElement *gameOptionNode = this->configFile.NewElement("option");
            gameOptionNode->SetAttribute("name", it.get_definition().name.c_str());
            gameOptionNode->SetAttribute("value", it.value.c_str());
            gameOptionsNode->InsertEndChild(gameOptionNode);
        }

        rootNode->InsertEndChild(gameNode);
    }

    // save config
    this->configFile.SaveFile(this->configLocation.c_str(), false);

    // return success
    return true;
}

bool Config::updateBinding(const Game &game, const Button &button, int alternative) {

    // get root node
    tinyxml2::XMLNode *rootNode = this->configFile.LastChild();

    // find game
    tinyxml2::XMLElement *gameNodes = rootNode->FirstChildElement("game");
    bool gameExists = false;
    while (gameNodes != nullptr) {
        const char *gameNodeName = gameNodes->Attribute("name");

        if (gameNodeName == nullptr) {
            rootNode->DeleteChild(gameNodes);
        } else if (std::string(gameNodeName) == game.getGameName()) {
            gameExists = true;
            break;
        }

        gameNodes = gameNodes->NextSiblingElement("game");
    }

    // if game doesn't exist
    if (!gameExists) {
        return false;
    }

    // get button nodes
    tinyxml2::XMLElement *gameButtonsNode = gameNodes->FirstChildElement("buttons");
    if (gameButtonsNode == nullptr) {
        return false;
    }

    // iterate button nodes
    tinyxml2::XMLElement *gameButtonNode = gameButtonsNode->FirstChildElement("button");
    int button_count = 0;
    while (gameButtonNode != nullptr) {
        const char *buttonNodeName = gameButtonNode->Attribute("name");

        if (buttonNodeName == nullptr) {
            gameButtonsNode->DeleteChild(gameButtonNode);
        } else if (std::string(buttonNodeName) == button.getName()) {
            if (button_count++ == alternative + 1 || alternative < 0) {
                gameButtonNode->SetAttribute("vkey", button.getVKey());
                gameButtonNode->SetAttribute("analogtype", (int) button.getAnalogType());
                gameButtonNode->SetAttribute("debounce_up", button.getDebounceUp());
                gameButtonNode->SetAttribute("debounce_down", button.getDebounceDown());
                gameButtonNode->SetAttribute("invert", button.getInvert());
                gameButtonNode->SetAttribute("devid", button.getDeviceIdentifier().c_str());
                break;
            }
        }

        gameButtonNode = gameButtonNode->NextSiblingElement("button");

        if (gameButtonNode == nullptr) {
            gameButtonNode = this->configFile.NewElement("button");
            gameButtonNode->SetAttribute("name", button.getName().c_str());
            gameButtonNode->SetAttribute("vkey", 0xFF);
            gameButtonNode->SetAttribute("analogtype", 0);
            gameButtonNode->SetAttribute("debounce_up", 0.0);
            gameButtonNode->SetAttribute("debounce_down", 0.0);
            gameButtonNode->SetAttribute("invert", false);
            gameButtonNode->SetAttribute("devid", "");
            gameButtonsNode->InsertEndChild(gameButtonNode);
        }
    }

    // check if button was not found
    if (button_count == 0) {
        return false;
    }

    // save config
    this->configFile.SaveFile(this->configLocation.c_str(), false);

    // return success
    return true;
}

bool Config::updateBinding(const Game &game, const Analog &analog) {
    tinyxml2::XMLNode *rootNode = this->configFile.LastChild();
    tinyxml2::XMLElement *gameNodes = rootNode->FirstChildElement("game");

    bool gameExists = false;

    while (gameNodes != nullptr) {
        const char *gameNodeName = gameNodes->Attribute("name");

        if (gameNodeName == nullptr) {
            rootNode->DeleteChild(gameNodes);
        } else if (std::string(gameNodeName) == game.getGameName()) {
            gameExists = true;
            break;
        }

        gameNodes = gameNodes->NextSiblingElement("game");
    }

    if (!gameExists) {
        return false;
    }

    // update buttons
    tinyxml2::XMLElement *gameAnalogsNode = gameNodes->FirstChildElement("analogs");
    if (gameAnalogsNode == nullptr) {
        return false;
    }

    bool analogExists = false;
    tinyxml2::XMLElement *gameAnalogNode = gameAnalogsNode->FirstChildElement("analog");

    while (gameAnalogNode != nullptr) {
        const char *analogNodeName = gameAnalogNode->Attribute("name");

        if (analogNodeName == nullptr) {
            gameAnalogsNode->DeleteChild(gameAnalogNode);
        } else if (std::string(analogNodeName) == analog.getName()) {
            analogExists = true;
            break;
        }

        gameAnalogNode = gameAnalogNode->NextSiblingElement("analog");
    }

    if (analogExists) {
        gameAnalogNode->SetAttribute("index", analog.getIndex());
        gameAnalogNode->SetAttribute("sensivity", analog.getSensitivity());
        gameAnalogNode->SetAttribute("deadzone", analog.getDeadzone());
        gameAnalogNode->SetAttribute("deadzone_mirror", analog.getDeadzoneMirror());
        gameAnalogNode->SetAttribute("invert", analog.getInvert());
        gameAnalogNode->SetAttribute("smoothing", analog.getSmoothing());
        gameAnalogNode->SetAttribute("devid", analog.getDeviceIdentifier().c_str());
    } else {
        gameAnalogNode = this->configFile.NewElement("analog");
        gameAnalogNode->SetAttribute("index", analog.getIndex());
        gameAnalogNode->SetAttribute("sensivity", analog.getSensitivity());
        gameAnalogNode->SetAttribute("deadzone", analog.getDeadzone());
        gameAnalogNode->SetAttribute("deadzone_mirror", analog.getDeadzoneMirror());
        gameAnalogNode->SetAttribute("invert", analog.getInvert());
        gameAnalogNode->SetAttribute("smoothing", analog.getSmoothing());
        gameAnalogNode->SetAttribute("devid", analog.getDeviceIdentifier().c_str());
        gameAnalogsNode->InsertEndChild(gameAnalogNode);
    }

    this->configFile.SaveFile(this->configLocation.c_str(), false);

    return true;
}

bool Config::updateBinding(const Game &game, ConfigKeypadBindings &keypads) {
    tinyxml2::XMLNode *rootNode = this->configFile.LastChild();
    tinyxml2::XMLElement *gameNodes = rootNode->FirstChildElement("game");

    bool gameExists = false;

    while (gameNodes != nullptr) {
        const char *gameNodeName = gameNodes->Attribute("name");

        if (gameNodeName == nullptr) {
            rootNode->DeleteChild(gameNodes);
        } else if (std::string(gameNodeName) == game.getGameName()) {
            gameExists = true;
            break;
        }

        gameNodes = gameNodes->NextSiblingElement("game");
    }

    if (!gameExists) {
        return false;
    }

    // update keypads
    tinyxml2::XMLElement *gameKeypadNode = gameNodes->FirstChildElement("keypads");
    if (gameKeypadNode == nullptr) {
        gameKeypadNode = this->configFile.NewElement("keypads");
        gameNodes->InsertEndChild(gameKeypadNode);
    }

    // update attributes
    gameKeypadNode->SetAttribute("devid1", keypads.keypads[0].c_str());
    gameKeypadNode->SetAttribute("devid2", keypads.keypads[1].c_str());
    gameKeypadNode->SetAttribute("cardpath1", reinterpret_cast<const char *>(keypads.card_paths[0].u8string().c_str()));
    gameKeypadNode->SetAttribute("cardpath2", reinterpret_cast<const char *>(keypads.card_paths[1].u8string().c_str()));

    this->configFile.SaveFile(this->configLocation.c_str(), false);

    return true;
}

bool Config::updateBinding(const Game &game, const Light &light, int alternative) {

    // get root node and find game
    tinyxml2::XMLNode *rootNode = this->configFile.LastChild();
    tinyxml2::XMLElement *gameNodes = rootNode->FirstChildElement("game");

    bool gameExists = false;

    while (gameNodes != nullptr) {
        const char *gameNodeName = gameNodes->Attribute("name");
        if (gameNodeName == nullptr) {
            rootNode->DeleteChild(gameNodes);
        } else if (std::string(gameNodeName) == game.getGameName()) {
            gameExists = true;
            break;
        }
        gameNodes = gameNodes->NextSiblingElement("game");
    }

    // if game does not exist
    if (!gameExists) {
        return false;
    }

    // get light nodes
    tinyxml2::XMLElement *gameLightsNode = gameNodes->FirstChildElement("lights");
    if (gameLightsNode == nullptr) {
        return false;
    }

    // iterate light nodes
    tinyxml2::XMLElement *gameLightNode = gameLightsNode->FirstChildElement("light");
    int light_count = 0;
    while (gameLightNode != nullptr) {
        const char *lightNodeName = gameLightNode->Attribute("name");

        if (lightNodeName == nullptr) {
            gameLightsNode->DeleteChild(gameLightNode);
        } else if (std::string(lightNodeName) == light.getName()) {
            if (light_count++ == alternative + 1 || alternative < 0) {
                gameLightNode->SetAttribute("index", light.getIndex());
                gameLightNode->SetAttribute("devid", light.getDeviceIdentifier().c_str());
                break;
            }
        }

        gameLightNode = gameLightNode->NextSiblingElement("light");

        if (gameLightNode == nullptr) {
            gameLightNode = this->configFile.NewElement("light");
            gameLightNode->SetAttribute("name", light.getName().c_str());
            gameLightNode->SetAttribute("index", 0);
            gameLightNode->SetAttribute("devid", "");
            gameLightsNode->InsertEndChild(gameLightNode);
        }
    }

    // check if light was not found
    if (light_count == 0) {
        return false;
    }

    // save config
    this->configFile.SaveFile(this->configLocation.c_str(), false);

    // return success
    return true;
}

bool Config::updateBinding(const Game &game, const Option &option) {
    tinyxml2::XMLNode *rootNode = this->configFile.LastChild();
    tinyxml2::XMLElement *gameNodes = rootNode->FirstChildElement("game");

    bool gameExists = false;

    while (gameNodes != nullptr) {
        const char *gameNodeName = gameNodes->Attribute("name");

        if (gameNodeName == nullptr) {
            rootNode->DeleteChild(gameNodes);
        } else if (std::string(gameNodeName) == game.getGameName()) {
            gameExists = true;
            break;
        }

        gameNodes = gameNodes->NextSiblingElement("game");
    }

    if (!gameExists) {
        return false;
    }

    // update options
    tinyxml2::XMLElement *gameOptionsNode = gameNodes->FirstChildElement("options");
    if (gameOptionsNode == nullptr) {
        return false;
    }
    bool optionExists = false;

    tinyxml2::XMLElement *gameOptionNode = gameOptionsNode->FirstChildElement("option");
    while (gameOptionNode != nullptr) {
        const char *optionNodeName = gameOptionNode->Attribute("name");

        if (optionNodeName == nullptr) {
            gameOptionsNode->DeleteChild(gameOptionNode);
        } else if (std::string(optionNodeName) == option.get_definition().name) {
            optionExists = true;
            break;
        }

        gameOptionNode = gameOptionNode->NextSiblingElement("option");
    }

    if (optionExists) {
        gameOptionNode->SetAttribute("value", option.value.c_str());
    } else {
        gameOptionNode = this->configFile.NewElement("option");
        gameOptionNode->SetAttribute("name", option.get_definition().name.c_str());
        gameOptionNode->SetAttribute("value", option.value.c_str());
        gameOptionsNode->InsertEndChild(gameOptionNode);
    }

    this->configFile.SaveFile(this->configLocation.c_str(), false);

    return true;
}

std::vector<Button> Config::getButtons(const std::string &gameName) {
    std::vector<Button> buttons;

    tinyxml2::XMLNode *rootNode = this->configFile.LastChild();
    tinyxml2::XMLElement *gameNodes = rootNode->FirstChildElement("game");

    bool gameExists = false;

    while (gameNodes != nullptr) {
        const char *gameNodeName = gameNodes->Attribute("name");

        if (gameNodeName == nullptr) {
            rootNode->DeleteChild(gameNodes);
        } else if (std::string(gameNodeName) == gameName) {
            gameExists = true;
            break;
        }
        gameNodes = gameNodes->NextSiblingElement("game");
    }
    if (gameExists) {

        // get buttons node
        tinyxml2::XMLElement *gameButtonsNode = gameNodes->FirstChildElement("buttons");
        if (gameButtonsNode == nullptr) {
            return buttons;
        }

        // iterate all buttons
        tinyxml2::XMLElement *gameButtonNode = gameButtonsNode->FirstChildElement("button");
        while (gameButtonNode != nullptr) {
            const char *buttonNodeName = nullptr;
            buttonNodeName = gameButtonNode->Attribute("name");
            if (buttonNodeName == nullptr) {
                gameButtonsNode->DeleteChild(gameButtonNode);
            } else {

                // get attributes
                int vKey = 0xFF;
                auto analogType = (int) BAT_NONE;
                double debounce_up = 0.0;
                double debounce_down = 0.0;
                bool invert = false;
                gameButtonNode->QueryIntAttribute("vkey", &vKey);
                gameButtonNode->QueryIntAttribute("analogtype", &analogType);
                gameButtonNode->QueryDoubleAttribute("debounce_up", &debounce_up);
                gameButtonNode->QueryDoubleAttribute("debounce_down", &debounce_down);
                gameButtonNode->QueryBoolAttribute("invert", &invert);
                const char *devid = gameButtonNode->Attribute("devid");

                // find alternative
                bool alternative_found = false;
                for (auto &button : buttons) {
                    if (button.getName() == std::string(buttonNodeName)) {
                        auto &alternatives = button.getAlternatives();
                        auto &alt = alternatives.emplace_back(buttonNodeName);
                        alt.setVKey((unsigned short) vKey);
                        alt.setAnalogType((ButtonAnalogType) analogType);
                        alt.setDebounceUp(debounce_up);
                        alt.setDebounceDown(debounce_down);
                        alt.setInvert(invert);
                        if (devid) {
                            alt.setDeviceIdentifier(std::string(devid));
                        }
                        alternative_found = true;
                        break;
                    }
                }

                // if no alternative was found
                if (!alternative_found) {

                    // create button and add to list
                    auto &button = buttons.emplace_back(buttonNodeName);
                    button.setVKey((unsigned short) vKey);
                    button.setAnalogType((ButtonAnalogType) analogType);
                    button.setDebounceUp(debounce_up);
                    button.setDebounceDown(debounce_down);
                    button.setInvert(invert);
                    if (devid) {
                        button.setDeviceIdentifier(devid);
                    }
                }
            }

            // get next button
            gameButtonNode = gameButtonNode->NextSiblingElement("button");
        }
    }

    return buttons;
}

std::vector<Button> Config::getButtons(Game *game) {
    return this->getButtons(game->getGameName());
}

std::vector<Light> Config::getLights(const std::string &gameName) {
    std::vector<Light> lights;

    tinyxml2::XMLNode *rootNode = this->configFile.LastChild();
    tinyxml2::XMLElement *gameNodes = rootNode->FirstChildElement("game");

    bool gameExists = false;

    while (gameNodes != nullptr) {
        const char *gameNodeName = gameNodes->Attribute("name");

        if (gameNodeName == nullptr) {
            rootNode->DeleteChild(gameNodes);
        } else if (std::string(gameNodeName) == gameName) {
            gameExists = true;
            break;
        }

        gameNodes = gameNodes->NextSiblingElement("game");
    }

    if (gameExists) {

        // get lights node
        tinyxml2::XMLElement *gameLightsNode = gameNodes->FirstChildElement("lights");
        if (gameLightsNode == nullptr) {
            return lights;
        }

        // iterate all lights
        tinyxml2::XMLElement *gameLightNode = gameLightsNode->FirstChildElement("light");
        while (gameLightNode != nullptr) {
            const char *lightNodeName = nullptr;
            lightNodeName = gameLightNode->Attribute("name");
            if (lightNodeName == nullptr) {
                gameLightsNode->DeleteChild(gameLightNode);
            } else {

                // get attributes
                int index = 0;
                gameLightNode->QueryIntAttribute("index", &index);
                const char* devid = gameLightNode->Attribute("devid");

                // find alternative
                bool alternative_found = false;
                for (auto &light : lights) {
                    if (light.getName() == std::string(lightNodeName)) {
                        auto &alternatives = light.getAlternatives();
                        auto &alt = alternatives.emplace_back(lightNodeName);
                        alt.setIndex((unsigned int) index);
                        if (devid) {
                            alt.setDeviceIdentifier(devid);
                        }
                        alternative_found = true;
                        break;
                    }
                }

                // if no alternative was found
                if (!alternative_found) {

                    // create light and add to lights
                    auto &light = lights.emplace_back(lightNodeName);
                    light.setIndex((unsigned int) index);
                    if (devid) {
                        light.setDeviceIdentifier(devid);
                    }
                }
            }

            // get next light
            gameLightNode = gameLightNode->NextSiblingElement("light");
        }
    }

    return lights;
}

std::vector<Light> Config::getLights(Game *game) {
    return this->getLights(game->getGameName());
}

std::vector<Analog> Config::getAnalogs(const std::string &gameName) {
    std::vector<Analog> analogs;

    tinyxml2::XMLNode *rootNode = this->configFile.LastChild();
    tinyxml2::XMLElement *gameNodes = rootNode->FirstChildElement("game");

    bool gameExists = false;

    while (gameNodes != nullptr) {
        const char *gameNodeName = gameNodes->Attribute("name");

        if (gameNodeName == nullptr) {
            rootNode->DeleteChild(gameNodes);
        } else if (std::string(gameNodeName) == gameName) {
            gameExists = true;
            break;
        }

        gameNodes = gameNodes->NextSiblingElement("game");
    }
    if (gameExists) {
        tinyxml2::XMLElement *gameAnalogsNode = gameNodes->FirstChildElement("analogs");
        if (gameAnalogsNode == nullptr) {
            return analogs;
        }

        tinyxml2::XMLElement *gameAnalogNode = gameAnalogsNode->FirstChildElement("analog");

        while (gameAnalogNode != nullptr) {
            const char *analogNodeName = gameAnalogNode->Attribute("name");

            if (analogNodeName == nullptr) {
                gameAnalogsNode->DeleteChild(gameAnalogNode);
            } else {

                // get attributes
                int index = 0xFF;
                float sensitivity = 1.f;
                float deadzone = 0.f;
                bool deadzone_mirror = false;
                bool invert = false;
                bool smoothing = false;
                gameAnalogNode->QueryIntAttribute("index", &index);
                gameAnalogNode->QueryFloatAttribute("sensivity", &sensitivity);
                gameAnalogNode->QueryFloatAttribute("deadzone", &deadzone);
                gameAnalogNode->QueryBoolAttribute("deadzone_mirror", &deadzone_mirror);
                gameAnalogNode->QueryBoolAttribute("invert", &invert);
                gameAnalogNode->QueryBoolAttribute("smoothing", &smoothing);
                const char *devid = gameAnalogNode->Attribute("devid");

                // create analog and add to list
                auto &analog = analogs.emplace_back(analogNodeName);
                analog.setIndex((unsigned short) index);
                analog.setSensitivity(sensitivity);
                analog.setDeadzone(deadzone);
                analog.setDeadzoneMirror(deadzone_mirror);
                analog.setInvert(invert);
                analog.setSmoothing(smoothing);
                if (devid) {
                    analog.setDeviceIdentifier(devid);
                }
            }

            gameAnalogNode = gameAnalogNode->NextSiblingElement("analog");
        }
    }

    return analogs;
}

std::vector<Analog> Config::getAnalogs(Game *game) {
    return this->getAnalogs(game->getGameName());
}

ConfigKeypadBindings Config::getKeypadBindings(const std::string &gameName) {
    ConfigKeypadBindings bindings {};

    tinyxml2::XMLNode *rootNode = this->configFile.LastChild();
    tinyxml2::XMLElement *gameNodes = rootNode->FirstChildElement("game");

    bool gameExists = false;

    while (gameNodes != nullptr) {
        const char *gameNodeName = gameNodes->Attribute("name");

        if (gameNodeName == nullptr) {
            rootNode->DeleteChild(gameNodes);
        } else if (std::string(gameNodeName) == gameName) {
            gameExists = true;
            break;
        }

        gameNodes = gameNodes->NextSiblingElement("game");
    }

    if (gameExists) {
        tinyxml2::XMLElement *gameKeypadNode = gameNodes->FirstChildElement("keypads");
        if (gameKeypadNode == nullptr) {
            return bindings;
        }

        // set bindings
        const char *tmp;
        tmp = gameKeypadNode->Attribute("devid1");
        if (tmp) {
            bindings.keypads[0] = std::string(tmp);
        }
        tmp = gameKeypadNode->Attribute("devid2");
        if (tmp) {
            bindings.keypads[1] = std::string(tmp);
        }
        tmp = gameKeypadNode->Attribute("cardpath1");
        if (tmp) {
            bindings.card_paths[0] = std::u8string(reinterpret_cast<const char8_t *>(tmp));
        }
        tmp = gameKeypadNode->Attribute("cardpath2");
        if (tmp) {
            bindings.card_paths[1] = std::u8string(reinterpret_cast<const char8_t *>(tmp));
        }
    }

    return bindings;
}

ConfigKeypadBindings Config::getKeypadBindings(Game *game) {
    return this->getKeypadBindings(game->getGameName());
}

std::vector<Option> Config::getOptions(const std::string &gameName) {
    std::vector<Option> options;

    tinyxml2::XMLNode *rootNode = this->configFile.LastChild();
    tinyxml2::XMLElement *gameNodes = rootNode->FirstChildElement("game");

    // find game
    bool gameExists = false;
    while (gameNodes != nullptr) {
        const char *gameNodeName = gameNodes->Attribute("name");

        if (gameNodeName == nullptr) {
            rootNode->DeleteChild(gameNodes);
        } else if (std::string(gameNodeName) == gameName) {
            gameExists = true;
            break;
        }

        gameNodes = gameNodes->NextSiblingElement("game");
    }

    // check if game exists
    if (gameExists) {

        // check if options node exists
        auto gameOptionsNode = gameNodes->FirstChildElement("options");
        if (gameOptionsNode == nullptr) {
            return options;
        }

        // iterate options
        auto gameOptionNode = gameOptionsNode->FirstChildElement("option");
        while (gameOptionNode != nullptr) {
            const char *optionNodeName = gameOptionNode->Attribute("name");

            if (optionNodeName == nullptr) {
                gameOptionNode->DeleteChild(gameOptionNode);
            } else {
                const char *value = gameOptionNode->Attribute("value");

                options.emplace_back(OptionDefinition {
                    .title = optionNodeName,
                    .name = optionNodeName,
                    .desc = "",
                    .type = OptionType::Text,
                }, value);
            }

            gameOptionNode = gameOptionNode->NextSiblingElement("option");
        }
    }

    return options;
}

std::vector<Option> Config::getOptions(Game *game) {
    return this->getOptions(game->getGameName());
}

/////////////////////////
/// Private Functions ///
/////////////////////////

bool Config::createConfigFile() {
    std::ofstream ofsConfig;
    ofsConfig.open(this->configLocation);
    if (!ofsConfig.is_open() || ofsConfig.fail() || ofsConfig.bad()) {
        this->status = false;
        return false;
    }
    ofsConfig.close();
    return this->firstFillConfigFile();
}

bool Config::firstFillConfigFile() {
    this->configFile.LoadFile(this->configLocation.c_str());
    this->configFile.Clear();

    tinyxml2::XMLNode *declarationNode = this->configFile.NewDeclaration();
    this->configFile.InsertFirstChild(declarationNode);

    tinyxml2::XMLNode *rootNode = this->configFile.NewElement("games");
    this->configFile.InsertEndChild(rootNode);

    this->configFile.SaveFile(this->configLocation.c_str(), false);
    return true;
}

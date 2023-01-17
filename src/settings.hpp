#pragma once

#include "plugin.hpp"
#include <string>
#include <fstream>
#include <stdexcept>

// Global module settings
struct UserSettings {
    std::string settingFileName;
    json_t* settingCache = nullptr;

    UserSettings(std::string fn, std::function<json_t*(json_t*)> initFunction=nullptr) {
        settingFileName = fn;

        if (initFunction) {
            std::ifstream file(asset::user(settingFileName));
            if (!file.is_open()) {
                json_t* json = readSettings();
                json = initFunction(json);
                saveSettings(json);
            } else file.close();
        }
    }

    template <typename T>
    T getSetting(std::string setting) {
        throw std::runtime_error("QuestionableModules::UserSettings::getJsonSetting function for type not defined. :(");
    }

    template <typename T>
    void setSetting(std::string setting, T value) {
        throw std::runtime_error("QuestionableModules::UserSettings::setJsonSetting function for type not defined. :(");
    }

    private:

    json_t * readSettings() {
        if (!settingCache) {
            std::string settingsFilename = asset::user(settingFileName);
            FILE *file = fopen(settingsFilename.c_str(), "r");
            if (!file) {
                return json_object();
            }
            
            json_error_t error;
            json_t *rootJ = json_loadf(file, 0, &error);
            
            fclose(file);
            settingCache = rootJ;
            return rootJ;
        } else return settingCache;
    }

    void saveSettings(json_t *rootJ) {
        std::string settingsFilename = asset::user(settingFileName);
        
        FILE *file = fopen(settingsFilename.c_str(), "w");
        
        if (file) {
            json_dumpf(rootJ, file, JSON_INDENT(2) | JSON_REAL_PRECISION(9));
            fclose(file);
        }
    }
    
};

template<>
int UserSettings::getSetting<int>(std::string setting) {
    return json_integer_value(json_object_get(readSettings(), setting.c_str()));
}

template<>
float UserSettings::getSetting<float>(std::string setting) {
    return json_real_value(json_object_get(readSettings(), setting.c_str()));
}

template<>
std::string UserSettings::getSetting<std::string>(std::string setting) {
    return json_string_value(json_object_get(readSettings(), setting.c_str()));
}

template<>
void UserSettings::setSetting<int>(std::string setting, int value) {
    json_t* settings = readSettings();
    json_object_set(settings, setting.c_str(), json_integer(value));
    saveSettings(settings);
}

template<>
void UserSettings::setSetting<float>(std::string setting, float value) {
    json_t* settings = readSettings();
    json_object_set(settings, setting.c_str(), json_real(value));
    saveSettings(settings);
}

template<>
void UserSettings::setSetting<std::string>(std::string setting, std::string value) {
    json_t* settings = readSettings();
    json_object_set(settings, setting.c_str(), json_string(value.c_str()));
    saveSettings(settings);
}
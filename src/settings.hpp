#pragma once

#include <rack.hpp>
using namespace rack;

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

        if constexpr (std::is_same<T, int>::value) return json_integer_value(json_object_get(readSettings(), setting.c_str()));
        if constexpr (std::is_same<T, float>::value) return json_real_value(json_object_get(readSettings(), setting.c_str()));
        if constexpr (std::is_same<T, std::string>::value) return json_string_value(json_object_get(readSettings(), setting.c_str()));

        throw std::runtime_error("QuestionableModules::UserSettings::getJsonSetting function for type not defined. :(");
    }

    template <typename T>
    void setSetting(std::string setting, T value) {
        json_t* v = nullptr;

        if constexpr (std::is_same<T, int>::value) v = json_integer(value);
        if constexpr (std::is_same<T, float>::value) v = json_real(value);
        if constexpr (std::is_same<T, std::string>::value) v = json_string(value.c_str());

        if (v) {
            json_t* settings = readSettings();
            json_object_set(settings, setting.c_str(), v);
            saveSettings(settings);

            return;
        }
        
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
#pragma once

#include <rack.hpp>
using namespace rack;

#include <string>
#include <fstream>
#include <stdexcept>

// Global module settings
struct UserSettings {
    enum Version {
        LATEST // define migrations above
    };
    int settingsVersion = LATEST;

    std::string settingFileName;
    json_t* settingCache = nullptr;

    UserSettings(std::string fn, std::function<json_t*(json_t*)> initFunction, std::vector<std::function<json_t*(json_t*)>> migrations) {
        settingFileName = fn;

        if (initFunction) {
            json_t* json = readSettings();
            UserSettings::json_create_if_not_exists(json, "settingsVersion", json_integer(settingsVersion));
            json = initFunction(json);
            if (migrations.size()) json = runMigrations(json, migrations);
            saveSettings(json);
        }
    }

    static void json_create_if_not_exists(json_t* json, std::string name, json_t* value) {
        if (!json_object_get(json, name.c_str())) json_object_set_new(json, name.c_str(), value);
    }

    json_t* runMigrations(json_t* json, std::vector<std::function<json_t*(json_t*)>> migrations) {
        int saveFileVer = getSetting<int>("settingsVersion", json);
        if (saveFileVer < settingsVersion) {
            // migrate save file to latest
            for (int i = saveFileVer; i < LATEST; i++) {
                json = migrations[i](json);
                json_object_set(json, "settingsVersion", json_integer(i+1));
            }
        }
        return json;
    }

    template <typename T>
    T getSetting(std::string setting, json_t* settings=nullptr) {
        if (!settings) settings = readSettings();

        if constexpr (std::is_same<T, int>::value) return json_integer_value(json_object_get(settings, setting.c_str()));
        if constexpr (std::is_same<T, bool>::value) return json_boolean_value(json_object_get(settings, setting.c_str()));
        if constexpr (std::is_same<T, float>::value) return json_real_value(json_object_get(settings, setting.c_str()));
        if constexpr (std::is_same<T, std::string>::value) return json_string_value(json_object_get(settings, setting.c_str()));

        throw std::runtime_error("QuestionableModules::UserSettings::getJsonSetting function for type not defined. :(");
    }

    template <typename T>
    void setSetting(std::string setting, T value) {
        json_t* v = nullptr;

        if constexpr (std::is_same<T, int>::value) v = json_integer(value);
        if constexpr (std::is_same<T, bool>::value) v = json_boolean(value);
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
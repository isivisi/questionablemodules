#include "plugin.hpp"
#include <string>

// Global module settings
struct UserSettings {
    std::string settingFileName;

    UserSettings(std::string fn) {
        settingFileName = fn;
    }

    json_t * readSettings() {
        std::string settingsFilename = asset::user(settingFileName);
        FILE *file = fopen(settingsFilename.c_str(), "r");
        if (!file) {
            return json_object();
        }
        
        json_error_t error;
        json_t *rootJ = json_loadf(file, 0, &error);
        
        fclose(file);
        return rootJ;
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
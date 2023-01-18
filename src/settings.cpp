#include "settings.hpp"

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
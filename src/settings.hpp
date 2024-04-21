#pragma once

#include <rack.hpp>
using namespace rack;

#include <string>
#include <fstream>
#include <stdexcept>
#include <mutex>
#include <memory>

// https://stackoverflow.com/questions/17032310/how-to-make-a-variadic-is-same
template <class T, class... Ts>
struct is_any : std::disjunction<std::is_same<T, Ts>...> {};

struct QuestionableJsonable {
	virtual json_t* toJson() = 0;
	virtual void fromJson(json_t*) = 0;
};

// Global module settings
struct UserSettings {
	std::mutex lock;
	enum Version {
		LATEST // define migrations above
	};
	int settingsVersion = LATEST;

	std::string settingFileName;
	json_t* settingCache = nullptr;

	std::function<json_t*(json_t*)> initFunction = nullptr;
	const std::function<json_t*(json_t*)>* migrations = nullptr;

	UserSettings(std::string fn, std::function<json_t*(json_t*)> initFunction, const std::function<json_t*(json_t*)>* migrations) {
		settingFileName = fn;

		this->initFunction = initFunction;
		this->migrations = migrations;

		if (initFunction) {
			json_t* json = readSettings();
			UserSettings::json_create_if_not_exists(json, "settingsVersion", json_integer(settingsVersion));
			if (migrations) json = runMigrations(json, migrations);
			json = initFunction(json);
			saveSettings(json);
		}
	}

	static void json_create_if_not_exists(json_t* json, std::string name, json_t* value) {
		if (!json_object_get(json, name.c_str())) json_object_set_new(json, name.c_str(), value);
	}

	json_t* runMigrations(json_t* json, const std::function<json_t*(json_t*)>* migrations) {
		static_assert(std::extent<decltype(migrations)>::value == LATEST, "Migration size must match migration enum size");
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
		std::lock_guard<std::mutex> guard(lock);

		if (!settings) settings = readSettings();

		if constexpr (std::is_base_of<QuestionableJsonable, T>::value) {
			T object;
			object.fromJson(json_object_get(settings, setting.c_str()));
			return object;
		} else return jsonToType<T>(json_object_get(settings, setting.c_str()));

		throw std::runtime_error("QuestionableModules::UserSettings::getSetting function for type not defined. :(");
	}

	template <typename T>
	void setSetting(std::string setting, T value) {
		std::lock_guard<std::mutex> guard(lock);

		json_t* v = nullptr;
		
		if constexpr (std::is_base_of<QuestionableJsonable, T>::value) v = value.toJson();
		else v = typeToJson<T>(value);

		if (v) {
			json_t* settings = readSettings();
			json_object_set(settings, setting.c_str(), v);
			saveSettings(settings);

			return;
		}
		
		throw std::runtime_error("QuestionableModules::UserSettings::setSetting function for type not defined. :(");
	}

	template<typename T>
	std::vector<T> getArraySetting(std::string setting, json_t* settings=nullptr) {
		std::lock_guard<std::mutex> guard(lock);

		if (!settings) settings = readSettings();
		std::vector<T> ret;

		size_t index;
		json_t* value;
		json_t* array = json_object_get(settings, setting.c_str());
		json_array_foreach(array, index, value) {
			if constexpr (std::is_base_of<QuestionableJsonable, T>::value) {
				T object;
				object.fromJson(value);
				ret.push_back(object);
			} else ret.push_back(jsonToType<T>(value));
		}

		return ret;
	}

	template<typename T>
	void setArraySetting(std::string setting, std::vector<T> value) {
		std::lock_guard<std::mutex> guard(lock);
		json_t* settings = readSettings();

		json_t* array = json_array();
		for (size_t i = 0; i < value.size(); i++) {
			if constexpr (std::is_base_of<QuestionableJsonable, T>::value) json_array_append_new(array, value[i].toJson());
			else json_array_append_new(array, typeToJson<T>(value[i]));
		}

		json_object_set(settings, setting.c_str(), array);
		saveSettings(settings);
	}


	private:

	template<typename T> 
	T jsonToType(json_t* json) {
		static_assert(is_any<T, int, bool, float, std::string, json_t*>::value, "no function defined for type");
		if constexpr (std::is_same<T, int>::value) return json_integer_value(json);
		if constexpr (std::is_same<T, bool>::value) return json_boolean_value(json);
		if constexpr (std::is_same<T, float>::value) return json_real_value(json);
		if constexpr (std::is_same<T, std::string>::value) return json_string_value(json);
		if constexpr (std::is_same<T, json_t*>::value) return json;
	}

	template<typename T> 
	json_t* typeToJson(T value) {
		static_assert(is_any<T, int, bool, float, std::string, json_t*>::value, "no function defined for type");
		if constexpr (std::is_same<T, int>::value) return json_integer(value);
		if constexpr (std::is_same<T, bool>::value) return json_boolean(value);
		if constexpr (std::is_same<T, float>::value) return json_real(value);
		if constexpr (std::is_same<T, std::string>::value) return json_string(value.c_str());
		if constexpr (std::is_same<T, json_t*>::value) return value;
	}

	json_t * readSettings() {
		if (!settingCache) {
			try {
				std::string settingsFilename = asset::user(settingFileName);
				FILE *file = fopen(settingsFilename.c_str(), "r");
				if (!file) {
					return json_object();
				}
				
				json_error_t error;
				json_t *rootJ = json_loadf(file, 0, &error);
				
				fclose(file);
				
				if (!rootJ) {
					json_t* newJ = json_object();
					UserSettings::json_create_if_not_exists(newJ, "settingsVersion", json_integer(settingsVersion));
					if (migrations) newJ = runMigrations(newJ, migrations);
					newJ = initFunction(newJ);
					saveSettings(newJ);
					settingCache = newJ;
					return newJ;
				}

				settingCache = rootJ;
				return rootJ;
			} catch (const std::exception& e) {
				json_t* newJ = json_object();

				UserSettings::json_create_if_not_exists(newJ, "settingsVersion", json_integer(settingsVersion));
				if (migrations) newJ = runMigrations(newJ, migrations);
				newJ = initFunction(newJ);
				saveSettings(newJ);

				settingCache = newJ;
				return newJ;
			}
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
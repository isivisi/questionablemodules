#include "plugin.hpp"

Plugin* pluginInstance;

const std::function<json_t*(json_t*)>* migrations = {};

UserSettings userSettings("questionablemodules.json", [](json_t* json) {
	// Runs at program start

	UserSettings::json_create_if_not_exists(json, "theme", json_string(""));
	UserSettings::json_create_if_not_exists(json, "treequencerScreenColor", json_integer(0));
	UserSettings::json_create_if_not_exists(json, "showDescriptors", json_boolean(false));
	UserSettings::json_create_if_not_exists(json, "gitPersonalAccessToken", json_string(""));

	return json;
}, migrations);

void init(Plugin* p) {
	pluginInstance = p;

	// Add modules here
	p->addModel(modelNandomizer);
	p->addModel(modelDiscombobulator);
	p->addModel(modelTreequencer);
	p->addModel(modelQuatOSC);
	p->addModel(modelNightBin);

	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}
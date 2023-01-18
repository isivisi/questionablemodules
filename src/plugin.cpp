#include "plugin.hpp"

Plugin* pluginInstance;
UserSettings userSettings("questionablemodules.json", [](json_t* json) {
	// first time initialization

	json_object_set_new(json, "theme", json_string(""));

	return json;
});

void init(Plugin* p) {
	pluginInstance = p;

	// Add modules here
	p->addModel(modelNandomizer);
	p->addModel(modelDiscombobulator);
	p->addModel(modelTreequencer);
	p->addModel(modelQuatOSC);

	userSettings.setSetting<std::string>("theme", "Dark");

	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}
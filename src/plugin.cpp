#include "plugin.hpp"


Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;

	// Add modules here
	p->addModel(modelNandomizer2);
	p->addModel(modelNandomizer3);
	p->addModel(modelNandomizer4);
	p->addModel(modelNandomizer5);
	p->addModel(modelNandomizer6);
	p->addModel(modelNandomizer7);
	p->addModel(modelNandomizer8);

	p->addModel(modelDiscombobulator);

	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}
#include "plugin.hpp"


Plugin* pluginInstance;

/*
    Param: Read with params[...].getValue()
    Input: Read with inputs[...].getVoltage()
    Output: Write with outputs[...].setVoltage(voltage)
    Light: Write with lights[...].setBrightness(brightness)
*/


void init(Plugin* p) {
	pluginInstance = p;

	// Add modules here
	p->addModel(modelModule);

	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}
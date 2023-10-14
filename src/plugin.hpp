#pragma once
#include <rack.hpp>
using namespace rack;
#include "settings.hpp"

extern UserSettings userSettings;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
extern Model* modelNandomizer;
extern Model* modelDiscombobulator;
extern Model* modelTreequencer;
extern Model* modelQuatOSC;
extern Model* modelNightBin;
extern Model* modelGreenscreen;
extern Model* modelSyncMute;
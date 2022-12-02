#include "plugin.hpp"
#include "nandomizer.cpp"
#include <vector>
#include <list>
#include <random>

struct Nandomizer8 : Nandomizer {
    Nandomizer8() {
        inputsUsed = 8;
    }
};

Model* modelNandomizer8 = createModel<Nandomizer8, NandomizerWidget>("nandomizer8");
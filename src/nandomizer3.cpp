#include "plugin.hpp"
#include "nandomizer.cpp"
#include <vector>
#include <list>
#include <random>

struct Nandomizer3 : Nandomizer {
    Nandomizer3() {
        inputsUsed = 3;
    }
};

Model* modelNandomizer3 = createModel<Nandomizer3, NandomizerWidget>("nandomizer3");
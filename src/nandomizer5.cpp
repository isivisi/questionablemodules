#include "plugin.hpp"
#include "nandomizer.cpp"
#include <vector>
#include <list>
#include <random>

struct Nandomizer5 : Nandomizer {
    Nandomizer5() {
        inputsUsed = 5;
    }
};

Model* modelNandomizer5 = createModel<Nandomizer5, NandomizerWidget>("nandomizer5");
#include "plugin.hpp"
#include "nandomizer.cpp"
#include <vector>
#include <list>
#include <random>

struct Nandomizer2 : Nandomizer {
    Nandomizer2() {
        inputsUsed = 2;
    }
};

Model* modelNandomizer2 = createModel<Nandomizer2, NandomizerWidget>("nandomizer");
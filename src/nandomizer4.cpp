#include "plugin.hpp"
#include "nandomizer.cpp"
#include <vector>
#include <list>
#include <random>

struct Nandomizer4 : Nandomizer {
    Nandomizer4() {
        inputsUsed = 4;
    }
};

Model* modelNandomizer4 = createModel<Nandomizer4, NandomizerWidget>("nandomizer4");
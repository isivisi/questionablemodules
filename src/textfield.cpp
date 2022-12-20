#include "plugin.hpp"

typedef std::function<void(std::string)> textLambda;

struct QTextField : ui::TextField {

    textLambda functionPtr;

    QTextField(textLambda fn) : TextField() {

        functionPtr = fn;

    }

    void onSelectKey(const SelectKeyEvent& e) override {
        TextField::onSelectKey(e);
        functionPtr(text);
        e.consume(this);
    }

};
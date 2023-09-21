#include "plugin.hpp"

typedef std::function<void(std::string)> textLambda;

struct QTextField : ui::TextField {

	textLambda functionPtr;

	QTextField(textLambda fn, int xsize = 100, std::string defaultText = "") : TextField() {

		functionPtr = fn;
		box.size.x = xsize;
		text = defaultText;

	}

	void onSelectKey(const SelectKeyEvent& e) override {
		TextField::onSelectKey(e);
		functionPtr(text);
		e.consume(this);
	}

};
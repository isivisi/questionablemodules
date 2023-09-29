#include "plugin.hpp"

typedef std::function<void(std::string)> textLambda;

struct QuestionableTextField : ui::TextField {

	textLambda functionPtr;

	QuestionableTextField(textLambda fn, int xsize = 100, std::string defaultText = "") : TextField() {

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

typedef std::function<float()> quantityGetFunc;
typedef std::function<void(float)> quantitySetFunc;

struct QuestionableQuantity : Quantity {
	std::function<float()> getValueFunc;
	std::function<void(float)> setValueFunc;

	QuestionableQuantity(quantityGetFunc getFunc, quantitySetFunc setFunc) {
		getValueFunc = getFunc;
		setValueFunc = setFunc;
	}

	void setValue(float value) override {
		setValueFunc(value);
	}
	float getValue() override {
		return getValueFunc();
	}

};

struct QuestionableMenu : Menu {
	std::function<void()> onDestruct = nullptr;

	~QuestionableMenu() override {
		if (onDestruct) onDestruct();
	}

};
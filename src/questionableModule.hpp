#pragma once

#include "plugin.hpp"
#include "colorBG.hpp"
#include <string>
#include <cctype>
#include <iomanip>
#include <sstream>

struct QuestionableModule : Module {
	bool supportsSampleRateOverride = false; 
	float sampleRateOverride = 0;
	bool runHalfRate = false;
	int64_t frame = 0;
	bool showDescriptors = userSettings.getSetting<bool>("showDescriptors");
    std::string theme = userSettings.getSetting<std::string>("theme");

    json_t* dataToJson() override {
		json_t* rootJ = json_object();
        json_object_set_new(rootJ, "theme", json_string(theme.c_str()));
		json_object_set_new(rootJ, "showDescriptors", json_boolean(showDescriptors));
		json_object_set_new(rootJ, "runHalfRate", json_boolean(runHalfRate));
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        if (json_t* s = json_object_get(rootJ, "theme")) theme = json_string_value(s);
		if (json_t* d = json_object_get(rootJ, "showDescriptors")) showDescriptors = json_boolean_value(d);
		if (json_t* hr = json_object_get(rootJ, "runHalfRate")) runHalfRate = json_boolean_value(hr);
    }

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		if (supportsSampleRateOverride && runHalfRate) sampleRateOverride = e.sampleRate / 2;
	}

	void process(const ProcessArgs& args) override {
		if (sampleRateOverride == 0) {
			processUndersampled(args);
			return;
		}
		// only undersample logic for now
		if (args.sampleRate == sampleRateOverride || args.frame % (int)(args.sampleRate/sampleRateOverride) == 0) {
			ProcessArgs newArgs;
			newArgs.sampleTime = 1 / sampleRateOverride;
			newArgs.sampleRate = sampleRateOverride;
			newArgs.frame = frame++;
			processUndersampled(newArgs);
		}
	}

	// if the module needs to undersample it will use this instead and rely on parent to send process calls
	virtual void processUndersampled(const ProcessArgs& args) {};

};

struct QuestionableWidget : ModuleWidget {
    ImagePanel *backdrop;
    ColorBG* color;
	bool lastPreferDark = false;

    QuestionableWidget() {

    }

	void step() override {
		if (settings::preferDarkPanels != lastPreferDark) {
			lastPreferDark = settings::preferDarkPanels;
			if (settings::preferDarkPanels && !module) setWidgetTheme("Dark");
		}
		ModuleWidget::step();
	}

	void backgroundColorLogic(QuestionableModule* module) {
		if (module && module->theme.size()) {
			color->drawBackground = true;
			color->setTheme(BG_THEMES[module->theme]);
		}
		if (module) color->setTextGroupVisibility("descriptor", module->showDescriptors);
	}

	void setWidgetTheme(std::string theme, bool setGlobal=true) {
		QuestionableModule* mod = (QuestionableModule*)module;
		color->drawBackground = theme != "";
		color->setTheme(BG_THEMES[theme]);
		if (mod) mod->theme = theme;
		if (setGlobal) userSettings.setSetting<std::string>("theme", theme);
	}

	void draw(const DrawArgs& args) override {
		ModuleWidget::draw(args);
		if (module && backdrop && module->isBypassed()) {
			backdrop->drawBackground = false;
		} else if (backdrop) backdrop->drawBackground = true;
	}
    
	void appendContextMenu(Menu *menu) override
  	{
		QuestionableModule* mod = (QuestionableModule*)module;

		if (mod->supportsSampleRateOverride) menu->addChild(rack::createSubmenuItem("Sample Rate", "", [=](ui::Menu* menu) {
			menu->addChild(createMenuItem("Full", mod->runHalfRate ? "" : "•",[=]() { 
				mod->sampleRateOverride = 0; 
				mod->runHalfRate = false; 
			}));
			menu->addChild(createMenuItem("Half", mod->runHalfRate ? "•" : "", [=]() { 
				mod->sampleRateOverride = APP->engine->getSampleRate() / 2; 
				mod->runHalfRate = true; 
			}));
		}));

		menu->addChild(rack::createSubmenuItem("Theme", "", [=](ui::Menu* menu) {
			menu->addChild(createMenuItem("Default", mod->theme == "" ? "•" : "",[=]() {
				setWidgetTheme("");
			}));
			menu->addChild(createMenuItem("Boring", mod->theme == "Light" ? "•" : "", [=]() {
				setWidgetTheme("Light");
			}));
			menu->addChild(createMenuItem("Boring but Dark", mod->theme == "Dark" ? "•" : "", [=]() {
				setWidgetTheme("Dark");
			}));
		}));

		menu->addChild(createMenuItem("Toggle Descriptors", "", [=]() {
			mod->showDescriptors = !mod->showDescriptors;
			color->setTextGroupVisibility("descriptor", mod->showDescriptors);
			userSettings.setSetting<bool>("showDescriptors", mod->showDescriptors);
		}));

		menu->addChild(rack::createMenuItem("Report Bug", "", [=]() {
			Model* model = getModel();
			std::string title = model->name + std::string(" Bug Report");
			
			std::string url = "https://github.com/isivisi/questionablemodules/issues/new?title=" + network::encodeUrl(title) + std::string("&body=") + network::encodeUrl(getReportBody(true));
			if (url.size() < 8202) { // can be too large for githubs nginx buffer
				system::openBrowser(url);
			} else system::openBrowser("https://github.com/isivisi/questionablemodules/issues/new?title=" + network::encodeUrl(title) + std::string("&body=") + network::encodeUrl(getReportBody(false)));

		}));

	}

	std::string getReportBody(bool json) {
		char* jsondump = json_dumps(module->toJson(), 0);
		std::string body = std::string("Module: ") + model->name +
		std::string("\nPlugin Version: ") + model->plugin->version + 
		(json ? std::string("\nJSON: `") + std::string(jsondump) : "") +
		std::string("`\n---------- Please describe your problem below: ----------\n\n");
		free(jsondump);
		return body;
	}
};

template <typename T>
struct QuestionableParam : T {
	static_assert(std::is_base_of<ParamWidget, T>::value, "T must inherit from ParamWidget");

	void appendContextMenu(Menu* menu) override {
		if (!this->module) return;
		menu->addChild(createMenuItem("Find in Documentation", "", [=]() {
			Model* model = this->module->getModel();
			if (!model) return;
			ParamQuantity* param = this->getParamQuantity();
			std::string url = "https://isivisi.github.io/questionablemodules/" + string::lowercase(model->name) + "#" + param->name;
			system::openBrowser(url);
		}));
	}
};

template <typename T>
struct QuestionablePort : T {
	static_assert(std::is_base_of<PortWidget, T>::value, "T must inherit from PortWidget");

	void appendContextMenu(Menu* menu) override {
		if (!this->module) return;
		menu->addChild(createMenuItem("Find in Documentation", "", [=]() {
			Model* model = this->module->getModel();
			if (!model) return;
			PortInfo* param = this->getPortInfo();
			std::string url = "https://isivisi.github.io/questionablemodules/" + string::lowercase(model->name) + "#" + param->name;
			system::openBrowser(url);
		}));
	}
};

struct QuestionableLightSwitch : QuestionableParam<SvgSwitch> {
	bool allowDraw = false;

	QuestionableLightSwitch() {
		QuestionableParam();
		fb->removeChild(shadow); // we don't need this
	}

	// force draw on light layer
	void drawLayer(const DrawArgs &args, int layer) override {
		if (layer == 1) {
			allowDraw = true;
			draw(args);
			allowDraw = false;
		}
	}

	void draw(const DrawArgs &args) override {
		if (allowDraw) SvgSwitch::draw(args);
	}

};

// Quick inline draw helper
struct QuestionableDrawWidget : Widget {
	std::function<void(const DrawArgs&)> func;

	QuestionableDrawWidget(math::Vec pos, std::function<void(const DrawArgs&)> drawFunc) {
		box.pos = pos;
		func = drawFunc;
	}

	void draw(const DrawArgs &args) override {
		func(args);
	}
};

//helpers

template <typename T>
static T lerp(T point1, T point2, T t) {
	T diff = point2 - point1;
	return point1 + diff * t;
}

template <typename T>
T clamp(T min, T max, T value) {
	return std::min(max, std::max(min, value));
}

template <typename T>
T randomReal(T min = 0.0, T max = 1.0) {
	std::uniform_real_distribution<T> distribution(0, max);
	std::random_device rd;
	return distribution(rd);
}

template <typename T>
T randomInt(T min, T max) {
	std::random_device rd; // obtain a random number from hardware
	std::mt19937 gen(rd()); // seed the generator
	std::uniform_int_distribution<T> distr(min, max); // define the range
	return distr(gen);
}

// Deg to Rad
template <typename T>
T dtor(T deg) {
	return deg  * (M_PI / 180);
}

template <typename T>
T normalizeRange(T min, T max, T value) {
	return (max-min)/(max-min)*(value-max)+max;
}
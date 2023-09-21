#pragma once

#include "plugin.hpp"
#include "colorBG.hpp"
#include <string>
#include <cctype>
#include <iomanip>
#include <sstream>

struct QuestionableModule : Module {
	bool supportsSampleRateOverride = false; 
	bool supportsThemes = true;
	bool toggleableDescriptors = true;

	float sampleRateOverride = 0;
	bool runHalfRate = false;
	int64_t frame = 0;
	bool showDescriptors = userSettings.getSetting<bool>("showDescriptors");
	std::string theme = userSettings.getSetting<std::string>("theme");

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		if (supportsThemes) json_object_set_new(rootJ, "theme", json_string(theme.c_str()));
		if (toggleableDescriptors) json_object_set_new(rootJ, "showDescriptors", json_boolean(showDescriptors));
		if (supportsSampleRateOverride) json_object_set_new(rootJ, "runHalfRate", json_boolean(runHalfRate));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		if (supportsThemes) if (json_t* s = json_object_get(rootJ, "theme")) theme = json_string_value(s);
		if (toggleableDescriptors) if (json_t* d = json_object_get(rootJ, "showDescriptors")) showDescriptors = json_boolean_value(d);
		if (supportsSampleRateOverride) if (json_t* hr = json_object_get(rootJ, "runHalfRate")) runHalfRate = json_boolean_value(hr);
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

struct QuestionableThemed {
	virtual void onThemeChange(std::string theme) = 0;
};

struct QuestionableWidget : ModuleWidget {
	ImagePanel *backdrop = nullptr;
	ColorBG* color = nullptr;
	bool lastPreferDark = false;

	bool supportsThemes = true;
	bool toggleableDescriptors = true;

	QuestionableWidget() {

	}

	void step() override {
		if (settings::preferDarkPanels != lastPreferDark) {
			lastPreferDark = settings::preferDarkPanels;
			if (!module) setWidgetTheme(settings::preferDarkPanels ? "Dark" : "");
		}
		ModuleWidget::step();
	}

	void backgroundColorLogic(QuestionableModule* module) {
		if (module && module->theme.size()) {
			setWidgetTheme(module->theme, false);
		}
		if (module && color) color->setTextGroupVisibility("descriptor", module->showDescriptors);
	}

	void setWidgetTheme(std::string theme, bool setGlobal=true) {
		QuestionableModule* mod = (QuestionableModule*)module;
		if (!supportsThemes) return;
		color->drawBackground = theme != "";
		color->setTheme(BG_THEMES[theme]);
		if (mod) mod->theme = theme;
		if (setGlobal) userSettings.setSetting<std::string>("theme", theme);
		propigateThemeToChildren(theme);
	}

	void propigateThemeToChildren(std::string theme) {
		for (auto it = children.begin(); it != children.end(); it++) {
			if (QuestionableThemed* themedWidget = dynamic_cast<QuestionableThemed*>(*it)) themedWidget->onThemeChange(theme);
		}
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

		if (supportsThemes) {
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
		}

		if (toggleableDescriptors) {
			menu->addChild(createMenuItem("Toggle Labels", "", [=]() {
				mod->showDescriptors = !mod->showDescriptors;
				if (color) color->setTextGroupVisibility("descriptor", mod->showDescriptors);
				userSettings.setSetting<bool>("showDescriptors", mod->showDescriptors);
			}));
		}

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
			std::string url = "https://isivisi.github.io/questionablemodules/" + rack::string::lowercase(model->name) + "#" + param->name;
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
			std::string url = "https://isivisi.github.io/questionablemodules/" + rack::string::lowercase(model->name) + "#" + param->name;
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

inline static bool isNumber(std::string s)
{
	std::size_t char_pos(0);
	// skip the whilespaces 
	char_pos = s.find_first_not_of(' ');
	if (char_pos == s.size()) return false;
	// check the significand 
	if (s[char_pos] == '+' || s[char_pos] == '-') 
		++char_pos; // skip the sign if exist 
	int n_nm, n_pt;
	for (n_nm = 0, n_pt = 0;
		std::isdigit(s[char_pos]) || s[char_pos] == '.';
		++char_pos) {
		s[char_pos] == '.' ? ++n_pt : ++n_nm;
	}
	if (n_pt>1 || n_nm<1) // no more than one point, at least one digit 
		return false;
	// skip the trailing whitespaces 
	while (s[char_pos] == ' ') {
		++ char_pos;
	}
	return char_pos == s.size(); // must reach the ending 0 of the string 
}

inline static bool isInteger(const std::string& s)
{
   if(s.empty() || ((!isdigit(s[0])) && (s[0] != '-') && (s[0] != '+'))) return false;

   char * p;
   strtol(s.c_str(), &p, 10);

   return (*p == 0);
}
#pragma once

#include "plugin.hpp"
#include "colorBG.hpp"
#include <string>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <chrono>

struct QuestionableModule : Module {
	bool showDescriptors = userSettings.getSetting<bool>("showDescriptors");
    std::string theme = userSettings.getSetting<std::string>("theme");

    json_t* dataToJson() override {
		json_t* rootJ = json_object();
        json_object_set_new(rootJ, "theme", json_string(theme.c_str()));
		json_object_set_new(rootJ, "showDescriptors", json_boolean(showDescriptors));
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        if (json_t* s = json_object_get(rootJ, "theme")) theme = json_string_value(s);
		if (json_t* d = json_object_get(rootJ, "showDescriptors")) showDescriptors = json_boolean_value(d);
    }
};

struct QuestionableModuleWidget : ModuleWidget {
    ImagePanel *backdrop;
    ColorBG* color;

    QuestionableModuleWidget() {
		
    }

	void setWidgetTheme(std::string theme) {
		QuestionableModule* mod = (QuestionableModule*)module;
		color->drawBackground = theme != "";
		color->setTheme(BG_THEMES[theme]);
		if (mod) mod->theme = theme;
		userSettings.setSetting<std::string>("theme", theme);
	}

	void draw(const DrawArgs& args) override {
		ModuleWidget::draw(args);
		if (module && backdrop && module->isBypassed()) {
			backdrop->drawBackground = false;
		} else if (backdrop) backdrop->drawBackground = true;
	}
    
	void appendContextMenu(Menu *menu) override
  	{

		menu->addChild(rack::createSubmenuItem("Theme", "", [=](ui::Menu* menu) {
			menu->addChild(createMenuItem("Default", "",[=]() {
				setWidgetTheme("");
			}));
			menu->addChild(createMenuItem("Boring", "", [=]() {
				setWidgetTheme("Light");
			}));
			menu->addChild(createMenuItem("Boring but Dark", "", [=]() {
				setWidgetTheme("Dark");
			}));
		}));

		menu->addChild(createMenuItem("Toggle Descriptors", "", [=]() {
			QuestionableModule* mod = (QuestionableModule*)module;
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

struct QuestionableWidget : Widget {
	std::chrono::time_point<std::chrono::high_resolution_clock> startTime;

	double fps = 60;
	double deltaTime = 0.016;
	int64_t frame = 0;

	QuestionableWidget() {
		startTime = std::chrono::high_resolution_clock::now();
	}

	virtual void draw(const DrawArgs&, float);

    void draw(const DrawArgs &args) override {
		auto currentTime = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> elapsedTime = currentTime - startTime;
		fps = 1 / elapsedTime.count();
		deltaTime = elapsedTime.count();
		frame += 1;

		startTime = currentTime;

		draw(args, deltaTime);
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
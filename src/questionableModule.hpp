#pragma once

#include "plugin.hpp"
#include "colorBG.hpp"
#include <string>
#include <cctype>
#include <iomanip>
#include <sstream>

struct QuestionableModule : Module {
    std::string theme = userSettings.getSetting<std::string>("theme");

    json_t* dataToJson() override {
		json_t* rootJ = json_object();
        json_object_set_new(rootJ, "theme", json_string(theme.c_str()));
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        if (json_t* s = json_object_get(rootJ, "theme")) theme = json_string_value(s);
    }
};

struct QuestionableWidget : ModuleWidget {
    ImagePanel *backdrop;
    ColorBG* color;

    QuestionableWidget() {
		
    }

	void setWidgetTheme(std::string theme) {
		QuestionableModule* mod = (QuestionableModule*)module;
		color->drawBackground = theme != "Default";
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
				setWidgetTheme("Default");
			}));
			menu->addChild(createMenuItem("Boring", "", [=]() {
				setWidgetTheme("Light");
			}));
			menu->addChild(createMenuItem("Boring but Dark", "", [=]() {
				setWidgetTheme("Dark");
			}));
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
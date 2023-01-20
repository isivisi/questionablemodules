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
    
	void appendContextMenu(Menu *menu) override
  	{
		QuestionableModule* mod = (QuestionableModule*)module;

		menu->addChild(rack::createSubmenuItem("Theme", "", [=](ui::Menu* menu) {
			menu->addChild(createMenuItem("Default", "",[=]() {
				color->drawBackground = false;
				color->setTheme(BG_THEMES["Dark"]); // for text
				mod->theme = "";
				userSettings.setSetting<std::string>("theme", "");
			}));
			menu->addChild(createMenuItem("Boring", "", [=]() {
				color->drawBackground = true;
				color->setTheme(BG_THEMES["Light"]);
				mod->theme = "Light";
				userSettings.setSetting<std::string>("theme", "Light");
			}));
			menu->addChild(createMenuItem("Boring but Dark", "", [=]() {
				color->drawBackground = true;
				color->setTheme(BG_THEMES["Dark"]);
				mod->theme = "Dark";
				userSettings.setSetting<std::string>("theme", "Dark");
			}));
		}));

		menu->addChild(rack::createMenuItem("Report Bug", "", [=]() {
			Model* model = getModel();
			char* jsondump = json_dumps(module->toJson(), 0);
			std::string title = model->name + std::string(" Bug Report");
			std::string body = std::string("Module: ") + model->name +
			std::string("\nPlugin Version: ") + model->plugin->version + 
			std::string("\nJSON: `") + std::string(jsondump) +
			std::string("`\n---------- Please describe your problem below: ----------\n\n");
			system::openBrowser("https://github.com/isivisi/questionablemodules/issues/new?title=" + network::encodeUrl(title) + std::string("&body=") + network::encodeUrl(body));
			free(jsondump);
		}));

	}
};
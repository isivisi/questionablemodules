#include "plugin.hpp"
#include "colorBG.hpp"
#include <string>

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
			std::string title = model->name + std::string(" Bug Report");
			std::string body = std::string("Module: ") + model->name +
			std::string("%0APlugin Version: ") + model->plugin->version + 
			std::string("%0APlease describe your problem below:%0A%0A");
			system::openBrowser("https://github.com/isivisi/questionablemodules/issues/new?title=" + title + std::string("&body=") + body);
		}));

	}
};
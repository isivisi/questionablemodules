#include "plugin.hpp"
#include "library.hpp"
#include "imagepanel.cpp"
#include "colorBG.hpp"
#include "questionableModule.hpp"
#include <vector>
#include <algorithm>

const int MODULE_SIZE = 6;



struct NightBin : QuestionableModule {
	enum ParamId {
		PARAMS_LEN
	};
	enum InputId {
		INPUTS_LEN
	};
	enum OutputId {
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	NightBin() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
	}

	void process(const ProcessArgs& args) override {

	}

};

struct NightBinWidget : QuestionableWidget {

	void setText() {
		NVGcolor c = nvgRGB(255,255,255);
		color->textList.clear();
		color->addText("NIGHTBIN", "OpenSans-ExtraBold.ttf", c, 24, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, 25));
		color->addText("·ISI·", "OpenSans-ExtraBold.ttf", c, 28, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, RACK_GRID_HEIGHT-13));

		color->addText("OUT", "OpenSans-Bold.ttf", c, 7, Vec(22, 353), "descriptor");
	}

	NightBinWidget(NightBin* module) {
		setModule(module);

		backdrop = new ImagePanel();
		backdrop->box.size = Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
		//backdrop->imagePath = asset::plugin(pluginInstance, "res/backdrop.jpg");
		backdrop->scalar = 3.5;
		backdrop->visible = true;

		color = new ColorBG(Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT));
		color->drawBackground = false;
		setText();

		backgroundColorLogic(module);
		
		setPanel(backdrop);
		addChild(color);

		//addChild(new QuestionableDrawWidget(Vec(18, 100), [module](const DrawArgs &args) {
		//}));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

	}

    struct QPluginInfo {
        std::string name;
        std::string slug;
        std::string gitURL;
        rack::string::Version version;

        static QPluginInfo fromJson(json_t* json) {
            QPluginInfo newInfo;
            if (json_t* n = json_object_get(json, "name")) newInfo.name = json_string_value(n);
            if (json_t* s = json_object_get(json, "slug")) newInfo.name = json_string_value(s);
            if (json_t* g = json_object_get(json, "sourceUrl")) newInfo.gitURL = json_string_value(g);
            if (json_t* v = json_object_get(json, "version")) newInfo.name = json_string_value(v);

            return QPluginInfo();
        }

        json_t* toJson() {

        }
    };

    std::vector<QPluginInfo> getPotentialPlugins() {
        std::vector<QPluginInfo> plugins;
        if (library::isLoggedIn()) {
            json_t* request = network::requestJson(network::METHOD_GET, "https://api.vcvrack.com/library/manifests", json_object());
            DEFER({json_decref(request);});

            if (!request) {
                WARN("[QuestionableModules::NightBin] Request for library manifests failed");
                return;
            }
        
            json_t* manifests = json_object_get(request, "manifests");
            DEFER({json_decref(manifests);});

            size_t index;
            json_t *value;
            json_array_foreach(manifests, index, value) {
                    plugins.push_back(QPluginInfo::fromJson(value));
            }
        }
        return plugins;
    }

    void appendContextMenu(Menu *menu) override
  	{
		NightBin* mod = (NightBin*)module;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Update All", "",[=]() {
			
		}));
        menu->addChild(new MenuSeparator);

        

        /*for (size_t i = 0; i < pluginInstance->plugins.size(); i++) {
            Plugin* plugin = pluginInstance->plugins[i];
            menu->addChild(rack::createSubmenuItem(plugin->name, plugin->version, [=](ui::Menu* menu) {
            
		    }));
        }*/

		menu->addChild(rack::createSubmenuItem("ModuleName", "v2.1.10", [=](ui::Menu* menu) {
            
		}));

		QuestionableWidget::appendContextMenu(menu);
	}

};

Model* modelNightBin = createModel<NightBin, NightBinWidget>("nightbin");
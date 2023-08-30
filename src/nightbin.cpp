#include "plugin.hpp"
#include "library.hpp"
#include "imagepanel.cpp"
#include "textfield.cpp"
#include "colorBG.hpp"
#include "questionableModule.hpp"
#include <vector>
#include <algorithm>
#include <regex>
#include <thread>
#include <mutex>
#include <plugin.hpp>

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

	std::string gitPersonalAccessToken = userSettings.getSetting<std::string>("gitPersonalAccessToken");

	NightBin() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
	}

	void process(const ProcessArgs& args) override {

	}

	/*json_t* dataToJson() override {
		json_t* nodeJ = QuestionableModule::dataToJson();

		json_t* qtArray = json_array();
		for (size_t i = 0; i < quantizedVOCT.size(); i++) json_array_append_new(qtArray, json_boolean(quantizedVOCT[i]));
		json_object_set_new(nodeJ, "knownPlugins", qtArray);

		return nodeJ;
	}

	void dataFromJson(json_t* rootJ) override {
		QuestionableModule::dataFromJson(rootJ);
		
		if (json_t* qtArray = json_object_get(rootJ, "knownPlugins")) {
			for (size_t i = 0; i < ; i++) { 
				quantizedVOCT[i] = json_boolean_value(json_array_get(qtArray, i));

			}
		}

	}*/

};

struct NightBinWidget : QuestionableWidget {
	std::vector<Plugin*> plugins;
	std::mutex gathering;
	std::thread gatherThread;

	void setText() {
		NVGcolor c = nvgRGB(255,255,255);
		color->textList.clear();
		color->addText("NIGHTBIN", "OpenSans-ExtraBold.ttf", c, 24, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, 25));
		color->addText("·ISI·", "OpenSans-ExtraBold.ttf", c, 28, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, RACK_GRID_HEIGHT-13));
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

		gatherThread = std::thread(&NightBinWidget::getPotentialPlugins, this);
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

	std::string getRepoAPI(Plugin* plugin) {
		std::regex r(R"(github\.com/(.*\/*.))");
		std::smatch match;
		if (std::regex_search(plugin->sourceUrl, match, r)) {
			if (match.size()<2) return "";
			return "https://api.github.com/repos/" + match[1].str();
		}
		return "";
	}

	std::vector<Plugin*> getSelectedPlugins() {
		std::vector<Plugin*> plugins;
		json_t* array = userSettings.getSetting<json_t*>("nightbinSelectedPlugins");
		
		size_t index;
		json_t *value;
		json_array_foreach(array, index, value) {
			std::string slug = json_string_value(value);
			auto foundPlugin = find_if(rack::plugin::plugins.begin(), rack::plugin::plugins.end(), [slug](Plugin* x){return x->slug == slug;});
			if (foundPlugin != rack::plugin::plugins.end()) plugins.push_back(*foundPlugin);
		}

		return plugins;
	}

	void addPlugin(std::string slug) {
		std::vector<Plugin*> plugins = getSelectedPlugins();
		auto found = find_if(plugins.begin(), plugins.end(), [slug](Plugin* x){return x->slug == slug;});
		if (found == rack::plugin::plugins.end()) {
			json_t* array = userSettings.getSetting<json_t*>("nightbinSelectedPlugins");
			json_array_append_new(array, json_string(slug.c_str()));
			userSettings.setSetting<json_t*>("nightbinSelectedPlugins", array);
		}
	}

    void getPotentialPlugins() {
		std::lock_guard<std::mutex> guard(gathering);

        for (plugin::Plugin* plugin : getSelectedPlugins()) {
			if (!plugin->sourceUrl.size()) continue;

			std::string api = getRepoAPI(plugin);
			WARN("[QuestionableModules::NightBin] checking for builds at: %s", api.c_str());
			if (!api.size()) {
				WARN("[QuestionableModules::NightBin] Failed to get api string for module: %s, sourceURL: %s", plugin->name.c_str(), plugin->sourceUrl.c_str());
				continue;
			}

			network::CookieMap cookies;
			cookies["Authorization"] = userSettings.getSetting<std::string>("gitPersonalAccessToken");

			json_t* request = network::requestJson(network::METHOD_GET, api + "/releases/tags/Nightly", nullptr, cookies);
			DEFER({json_decref(request);});

			if (!request) {
				WARN("[QuestionableModules::NightBin] Request for github release info failed");
				continue;
			}

			if (json_t* msg = json_object_get(request, "message")) {
				std::string message = json_string_value(msg);
				if (message == "Not Found") continue;
				if (message.find("API rate limit exceeded") != std::string::npos) {
					WARN("[QuestionableModules::NightBin] Request for github rate limited, consider setting your gitPersonalAccessToken");
					continue;
				}
			}

			plugins.push_back(plugin);
		}
    }

    void appendContextMenu(Menu *menu) override
  	{
        QuestionableWidget::appendContextMenu(menu);

		NightBin* mod = (NightBin*)module;
		menu->addChild(new MenuSeparator);
		
		if (!userSettings.getSetting<std::string>("gitPersonalAccessToken").size()) {
			menu->addChild(rack::createMenuLabel("Github Access Token:"));

			ui::TextField* param = new QTextField([=](std::string text) { 
				if (text.length()) userSettings.setSetting("gitPersonalAccessToken", text); // https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/managing-your-personal-access-tokens
			});
			param->box.size.x = 100;
			menu->addChild(param);
		} else {
			menu->addChild(createMenuItem("Clear Github Access Token", "", [=]() { 
				userSettings.setSetting<std::string>("gitPersonalAccessToken", "");
			}));
		}

		menu->addChild(new MenuSeparator);

		menu->addChild(createSubmenuItem("Add Modules", "", [=](Menu* menu) {
			 for (plugin::Plugin* plugin : rack::plugin::plugins) {
				if (!plugin->sourceUrl.size()) continue;
				menu->addChild(createMenuItem(plugin->name, "",[=]() {
					addPlugin(plugin->slug);
				}));
			 }
		}));

		menu->addChild(createMenuItem("Update All", "",[=]() {
				
		}));

		if (gathering.try_lock()) {
			for (plugin::Plugin* plugin : plugins) {
				menu->addChild(createMenuItem(plugin->name, plugin->version, [=]() {
					
				}));
			}
			gathering.unlock();
		} else {
			menu->addChild(createMenuItem("Checking for updates..."));
		}

	}

};

Model* modelNightBin = createModel<NightBin, NightBinWidget>("nightbin");
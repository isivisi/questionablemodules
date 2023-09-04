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
#include <app/MenuBar.hpp>

const int MODULE_SIZE = 8;

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

struct NightbinButton : ui::Button {
	std::mutex gathering;
	std::mutex updating;
	std::thread gatherThread;
	std::thread updateThread;
	bool isUpdating;
	float progress;

	NightbinButton() {
		text = "Nightbin";
	}

	void step() override {
		box.size.x = bndLabelWidth(APP->window->vg, -1, text.c_str()) + 1.0;
		Widget::step();
	}

	void draw(const DrawArgs& args) override {
		BNDwidgetState state = BND_DEFAULT;
		if (APP->event->hoveredWidget == this)
			state = BND_HOVER;
		if (APP->event->draggedWidget == this)
			state = BND_ACTIVE;
		bndMenuItem(args.vg, 0.0, 0.0, box.size.x, box.size.y, state, -1, text.c_str());
		Widget::draw(args);
	}

	struct QRemotePluginInfo {
        std::string name;
        std::string slug;
        std::string version;
		std::string dlURL;
		Plugin* pluginRef;

		struct LinkInfo {
			std::string version;
			std::string os;
			std::string arch;
			std::string url;

			static LinkInfo getLinkInfo(std::string link) {
				// example: download/Nightly/questionablemodules-2.1.10-nightly-7ce3a21-lin-x64.vcvplugin
				std::regex r(R"(-([0-9.\-]*.*(lin|win|mac)-(x64|arm64)))");
				std::smatch match;
				if (std::regex_search(link, match, r)) {
					if (match.size() >= 4) return {match[1].str(), match[2].str(), match[3].str(), link};
				}
				return LinkInfo();
			}
		};

        static QRemotePluginInfo fromJson(json_t* json, Plugin* plugin) {
            QRemotePluginInfo newInfo;
			newInfo.pluginRef = plugin;
			newInfo.name = plugin->name;
			newInfo.slug = plugin->slug;
			if (json_t* array = json_object_get(json, "assets")) {
				size_t key;
				json_t* value;
				json_array_foreach(array, key, value) {
					if (json_t* url = json_object_get(value, "browser_download_url")) {
						std::string download = json_string_value(url);
						LinkInfo info = LinkInfo::getLinkInfo(download);
						INFO("[QuestionableModules::NightBin] Found download %s-%s %s", info.os.c_str(), info.arch.c_str(), download.c_str());

						INFO("[QuestionableModules::NightBin] looking for %s-%s", APP_OS.c_str(), APP_CPU.c_str());
						if (info.arch == APP_CPU && info.os == APP_OS) {
							INFO("[QuestionableModules::NightBin] Found correct dl for arch %s", info.version.c_str());
							newInfo.version = info.version;
							newInfo.dlURL = download;
							return newInfo;
						}
					}
				}
			}
            return newInfo;
        }

		bool updatable() {
			return version != "" && pluginRef->version != version;
		}

        bool operator==(const QRemotePluginInfo other) {
			return other.slug == slug;
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
		std::vector<std::string> slugs = userSettings.getArraySetting<std::string>("nightbinSelectedPlugins");
		
		for (size_t i = 0; i < slugs.size(); i++) {
			auto foundPlugin = find_if(rack::plugin::plugins.begin(), rack::plugin::plugins.end(), [slugs, i](Plugin* x){return x->slug == slugs[i];});
			if (foundPlugin != rack::plugin::plugins.end()) plugins.push_back(*foundPlugin);
		}

		return plugins;
	}

	void addPlugin(std::string slug) {
		std::vector<std::string> plugins = userSettings.getArraySetting<std::string>("nightbinSelectedPlugins");
		auto found = find(plugins.begin(), plugins.end(), slug);
		if (found == plugins.end()) {
			plugins.push_back(slug);
			userSettings.setArraySetting<std::string>("nightbinSelectedPlugins", plugins);
			startQueryThread();
		}
	}

	void downloadUpdate(QRemotePluginInfo info) {
		system::setThreadName("Nightbin Update Thread for " + info.name);
		std::lock_guard<std::mutex> guard(updating);
		isUpdating = true;
		DEFER({isUpdating = false;});

		network::CookieMap cookies;
		cookies["Authorization"] = userSettings.getSetting<std::string>("gitPersonalAccessToken");

		std::string filename = info.dlURL.substr(info.dlURL.rfind('/')+1);
		std::string packagePath = system::join(plugin::pluginsPath, filename);

		INFO("Downloading %s to %s", info.name.c_str(), packagePath.c_str());
		if (!network::requestDownload(info.dlURL, packagePath, &progress, cookies)) {
			WARN("Download failed :(");
		} else {
			gatheredInfo.erase(std::find(gatheredInfo.begin(), gatheredInfo.end(), info));
		}
	}

	void startQueryThread() {
		if (gatherThread.joinable()) gatherThread.detach(); // let go of existing thread as it is either done or will finish on its own
		gatherThread = std::thread(&NightbinButton::queryForUpdates, this);
	}

	void startUpdateThread(std::vector<QRemotePluginInfo> updates) {
		if (updateThread.joinable()) updateThread.detach();
		updateThread = std::thread([=]() {
			INFO("Updating plugins");
			for (size_t i = 0; i < updates.size(); i++) downloadUpdate(updates[i]);
		});
	}

	std::vector<QRemotePluginInfo> gatheredInfo;

    void queryForUpdates() {
		system::setThreadName("Nightbin query Thread");
		std::lock_guard<std::mutex> guard(gathering);

		gatheredInfo.clear();

        for (plugin::Plugin* plugin : getSelectedPlugins()) {
			if (!plugin->sourceUrl.size()) continue;

			std::string api = getRepoAPI(plugin);
			INFO("checking for builds at: %s", api.c_str());
			if (!api.size()) {
				WARN("Failed to get api string for module: %s, sourceURL: %s", plugin->name.c_str(), plugin->sourceUrl.c_str());
				continue;
			}

			network::CookieMap cookies;
			cookies["Authorization"] = userSettings.getSetting<std::string>("gitPersonalAccessToken");

			json_t* request = network::requestJson(network::METHOD_GET, api + "/releases/tags/Nightly", nullptr, cookies);
			DEFER({json_decref(request);});

			if (!request) {
				WARN("Request for github release info failed");
				continue;
			}

			if (json_t* msg = json_object_get(request, "message")) {
				std::string message = json_string_value(msg);
				if (message == "Not Found") continue;
				if (message.find("API rate limit exceeded") != std::string::npos) {
					WARN("Request for github rate limited, consider setting your gitPersonalAccessToken");
					continue;
				}
			}

			gatheredInfo.push_back(QRemotePluginInfo::fromJson(request, plugin));
		}
    }

	void onAction(const ActionEvent& e) override {
		ui::Menu* menu = createMenu();
		menu->cornerFlags = BND_CORNER_TOP;
		menu->box.pos = getAbsoluteOffset(math::Vec(0, box.size.y));

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

		if (gatheredInfo.size()) menu->addChild(createMenuItem("Update All", "",[=]() { startUpdateThread(gatheredInfo); }));
		menu->addChild(createMenuItem("Query for Updates", "",[=]() { startQueryThread(); }));

		menu->addChild(new MenuSeparator);
		if (gathering.try_lock()) {
			for (QRemotePluginInfo info : gatheredInfo) {
				if (!info.updatable()) return;
				menu->addChild(createMenuItem(info.name, info.pluginRef->version + " → " + info.version, [=]() {
					startUpdateThread(std::vector<QRemotePluginInfo>{info});
				}));
			}
			gathering.unlock();
		} else {
			menu->addChild(createMenuItem("Checking for updates..."));
		}
	}
};

struct NightBinWidget : QuestionableWidget {
	ColorBGSimple* background = nullptr;

	void setText() {
		NVGcolor c = nvgRGB(255,255,255);
		color->textList.clear();
		color->addText("NIGHT-BIN", "OpenSans-ExtraBold.ttf", c, 24, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, 25));
		color->addText("·ISI·", "OpenSans-ExtraBold.ttf", c, 28, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, RACK_GRID_HEIGHT-13));
	}

	NightBinWidget(NightBin* module) {
		setModule(module);

		background = new ColorBGSimple(Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT), nvgRGB(150, 173, 233));

		color = new ColorBG(Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT));
		color->drawBackground = false;
		setText();

		backgroundColorLogic(module);
		setPanel(background);
		addChild(color);
		
		if (module) setupMenuBar();

		addChild(new QuestionableDrawWidget(Vec((MODULE_SIZE * RACK_GRID_WIDTH)/2, RACK_GRID_HEIGHT/2), [module](const DrawArgs &args) {
			std::string theme = module ? module->theme : "";
			for (int i = -4; i < 4; i++) {
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, ((MODULE_SIZE * RACK_GRID_WIDTH)/8) * i, -150);
				nvgLineTo(args.vg, ((MODULE_SIZE * RACK_GRID_WIDTH)/8) * i, 150);
				nvgStrokeColor(args.vg, (theme == "Dark" || theme == "") ? nvgRGB(250, 250, 250) : nvgRGB(30, 30, 30));
				nvgStrokeWidth(args.vg, 2);
				nvgStroke(args.vg);
			}
			for (int i = -9; i <= 9; i++) {
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, -55, (RACK_GRID_HEIGHT/25) * i);
				nvgLineTo(args.vg, 55, (RACK_GRID_HEIGHT/25) * i);
				nvgStrokeColor(args.vg, (theme == "Dark" || theme == "") ? nvgRGB(250, 250, 250) : nvgRGB(30, 30, 30));
				nvgStrokeWidth(args.vg, 2);
				nvgStroke(args.vg);
			}
		}));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		//startQueryThread();
	}

	// try to add our own menu item to the main rack bar
	void setupMenuBar() {
		auto rackLayout = std::find_if(APP->scene->menuBar->children.begin(), APP->scene->menuBar->children.end(), [=](widget::Widget* widget) {
			return dynamic_cast<ui::SequentialLayout*>(widget) != nullptr;
		});
		if (rackLayout != APP->scene->menuBar->children.end()) {
			auto existing = std::find_if((*rackLayout)->children.begin(), (*rackLayout)->children.end(), [=](widget::Widget* widget) {
				return dynamic_cast<NightbinButton*>(widget) != nullptr;
			});
			if (existing != (*rackLayout)->children.end()) return;
			
			NightbinButton* menuButton = new NightbinButton;
			(*rackLayout)->addChildBelow(menuButton, (*rackLayout)->children.back());
		} else WARN("Unable to add to racks menubar, could not find ui::SequentialLayout reference.");
	}

    void appendContextMenu(Menu *menu) override
  	{
        QuestionableWidget::appendContextMenu(menu);
	}

};

Model* modelNightBin = createModel<NightBin, NightBinWidget>("nightbin");
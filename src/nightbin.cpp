#include "plugin.hpp"
#include "library.hpp"
#include "imagepanel.cpp"
#include "textfield.cpp"
#include "colorBG.hpp"
#include "common.hpp"
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
	bool isUpdating = false;
	bool isGathering = false;
	float progress = 0.f;
	std::vector<std::string> warnings;

	NightbinButton() {
		text = "Night-bin";

		startQueryThread();
	}

	void step() override {
		if (isUpdating) text = "Updating... (%" + std::to_string((int)(progress*100)) + ")";
		else if (isGathering) text = "Night-bin ...";
		else text = "Night-bin";
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

		if (gatheredInfo.size()) {
			nvgBeginPath(args.vg);
			float radius = 4;
			nvgCircle(args.vg, radius, radius, radius);
			nvgFillColor(args.vg, nvgRGBf(1.0, 0.0, 0.0));
			nvgFill(args.vg);
			nvgStrokeColor(args.vg, nvgRGBf(0.5, 0.0, 0.0));
			nvgStroke(args.vg);
		}
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
				std::string lastElement = string::split(link, "/").back();
				std::regex r(R"(-([0-9.\-]*.*)-(lin|win|mac)-(x64|arm64))");
				std::smatch match;
				if (std::regex_search(lastElement, match, r)) {
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

		bool isValid() {
			return pluginRef != nullptr;
		}

		bool updatable() {
			return version != "" && pluginRef->version != version;
		}

		bool operator==(const QRemotePluginInfo other) {
			return other.slug == slug;
		}

		bool operator==(const Plugin* plug) {
			return plug->slug == slug;
		}
	};

	std::string getRepoAPI(Plugin* plugin) {
		std::regex r(R"(github\.com/(.*\/*.))");
		std::smatch match;
		if (std::regex_search(plugin->sourceUrl, match, r)) {
			if (match.size()<2) return "";
			std::string gitInfo = match[1].str();
			if (gitInfo.find_last_of("/") == gitInfo.size()-1) gitInfo.pop_back();
			return "https://api.github.com/repos/" + gitInfo;
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

	void addPlugin(std::string slug, bool check = true) {
		std::vector<std::string> plugins = userSettings.getArraySetting<std::string>("nightbinSelectedPlugins");
		auto found = find(plugins.begin(), plugins.end(), slug);
		if (found == plugins.end()) {
			plugins.push_back(slug);
			userSettings.setArraySetting<std::string>("nightbinSelectedPlugins", plugins);
			if (check) startQueryThread();
		}
	}

	void removePlugin(std::string slug) {
		std::vector<std::string> plugins = userSettings.getArraySetting<std::string>("nightbinSelectedPlugins");
		auto it = std::remove(plugins.begin(), plugins.end(), slug);
		if (it == plugins.end()) return;
		plugins.erase(it);
		userSettings.setArraySetting<std::string>("nightbinSelectedPlugins", plugins);
	}

	void downloadUpdate(QRemotePluginInfo info) {
		system::setThreadName("Nightbin Update Thread for " + info.name);
		std::lock_guard<std::mutex> guard(updating);
		isUpdating = true;
		DEFER({isUpdating = false;});

		network::CookieMap cookies;
		std::vector<std::string> headers = getAuth();

		std::string filename = info.dlURL.substr(info.dlURL.rfind('/')+1);
		std::string packagePath = system::join(plugin::pluginsPath, filename);

		INFO("Downloading %s to %s", info.name.c_str(), packagePath.c_str());
		if (!q::network::requestDownload(info.dlURL, packagePath, &progress, headers, cookies)) {
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

	std::vector<std::string> getAuth() {
		std::vector<std::string> headers;
		std::string token = userSettings.getSetting<std::string>("gitPersonalAccessToken");
		if (token.size()) headers.push_back("Authorization: Bearer " + token);
		return headers;
	}

	std::vector<QRemotePluginInfo> gatheredInfo;
	std::vector<Plugin*> pluginsWithBuilds;

	void queryForUpdates() {
		system::setThreadName("Nightbin query Thread");
		std::lock_guard<std::mutex> guard(gathering);
		isGathering = true; 
		DEFER({isGathering = false;});

		gatheredInfo.clear();

		for (plugin::Plugin* plugin : getSelectedPlugins()) {
			QRemotePluginInfo pluginInfo = getPluginRemoteInfo(plugin);
			if (pluginInfo.updatable()) gatheredInfo.push_back(pluginInfo);
		}
	}

	void queryForUpdatablePlugins() {
		system::setThreadName("Nightbin query Thread");
		std::lock_guard<std::mutex> guard(gathering);
		isGathering = true; 
		DEFER({isGathering = false;});

		if (userSettings.getSetting<std::string>("gitPersonalAccessToken").empty()) return;

		// if git token set check all plugins for Nightly builds
		if (!pluginsWithBuilds.size()) {
			for (plugin::Plugin* plugin : plugin::plugins) {
				QRemotePluginInfo pluginInfo = getPluginRemoteInfo(plugin);
				if (pluginInfo.isValid()) pluginsWithBuilds.push_back(plugin);
			}
		}
	}

	QRemotePluginInfo getPluginRemoteInfo(Plugin* plugin) {
		if (!plugin->sourceUrl.size()) return QRemotePluginInfo();

		std::string api = getRepoAPI(plugin);
		INFO("checking for builds at: %s", api.c_str());
		if (!api.size()) {
			WARN("Failed to get api string for module: %s, sourceURL: %s", plugin->name.c_str(), plugin->sourceUrl.c_str());
			warnings.push_back("Could not parse api string for " + plugin->name);
			return QRemotePluginInfo();
		}

		network::CookieMap cookies;
		std::vector<std::string> headers = getAuth();
		json_t* request = q::network::requestJson(network::METHOD_GET, api + "/releases/tags/Nightly", nullptr, headers, cookies);
		DEFER({json_decref(request);});

		if (!request) {
			WARN("Request for github release info failed");
			warnings.push_back("Failed to get " + plugin->name + ", request failed.");
			return QRemotePluginInfo();
		}

		if (json_t* msg = json_object_get(request, "message")) {
			std::string message = json_string_value(msg);
			if (message == "Not Found") {
				removePlugin(plugin->slug);
				warnings.push_back(plugin->name + " does not have a Nightly tagged release, cannot find updates.");
				return QRemotePluginInfo();
			};
			if (message.find("API rate limit exceeded") != std::string::npos) {
				WARN("Request for github rate limited, consider setting your gitPersonalAccessToken");
				warnings.push_back("Request for github rate limited, consider setting your gitPersonalAccessToken");
				return QRemotePluginInfo();
			}
		}

		return QRemotePluginInfo::fromJson(request, plugin);
	}

	std::vector<Plugin*> getPluginsSorted(std::vector<Plugin*> plugins = rack::plugin::plugins) {
		std::sort(plugins.begin(), plugins.end(), [](const Plugin* a, const Plugin* b) { return a->name < b->name; });
		return plugins;
	}

	// check if plugin has everytinh needed to query updates
	bool isPluginValid(Plugin* p) {
		if (!p) return false;
		if (p->pluginUrl.empty()) return false;
		if (p->pluginUrl.find("github") == std::string::npos) return false; // github only atm
		return true;
	}

	void onAction(const ActionEvent& e) override {
		if (isGathering || isUpdating) return;

		ui::Menu* menu = createMenu();
		menu->cornerFlags = BND_CORNER_TOP;
		menu->box.pos = getAbsoluteOffset(math::Vec(0, box.size.y));

		if (warnings.size()) {
			for (size_t i = 0; i < warnings.size(); i++) menu->addChild(createMenuLabel(warnings[i]));
			warnings.clear();
		}

		if (!userSettings.getSetting<std::string>("gitPersonalAccessToken").size()) {
			menu->addChild(createMenuLabel("Github Access Token:"));

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

		menu->addChild(createSubmenuItem("Add / Remove Modules", "", [=](Menu* menu) {
			std::vector<Plugin*> selected = getSelectedPlugins();
			
			if (!userSettings.getSetting<std::string>("gitPersonalAccessToken").empty()) {
				menu->addChild(createMenuItem("Add All Plugins", "+",[=]() { 
					for (plugin::Plugin* plugin : rack::plugin::plugins) {
						if (isPluginValid(plugin)) addPlugin(plugin->slug, false);
					}
					startQueryThread();
				}));
				menu->addChild(new MenuSeparator);
			}

			if (selected.size()) {
				for (plugin::Plugin* plugin : getPluginsSorted(selected)) {
					menu->addChild(createMenuItem(plugin->name, "-",[=]() { removePlugin(plugin->slug); }));
				}
				menu->addChild(new MenuSeparator);
			}

			for (plugin::Plugin* plugin : getPluginsSorted()) {
				if (!isPluginValid(plugin)) continue;
				if (std::find(selected.begin(), selected.end(), plugin) == selected.end()) menu->addChild(createMenuItem(plugin->name, "+",[=]() { addPlugin(plugin->slug); }));
			}
		}));

		if (gatheredInfo.size()) menu->addChild(createMenuItem("Update All", "",[=]() { startUpdateThread(gatheredInfo); }));
		menu->addChild(createMenuItem("Query for Updates", "",[=]() { startQueryThread(); }));

		menu->addChild(new MenuSeparator);
		if (gathering.try_lock()) {
			gathering.unlock();
			for (QRemotePluginInfo info : gatheredInfo) {
				if (!info.updatable()) return;
				menu->addChild(createMenuItem(info.name, info.pluginRef->version + " → " + info.version, [=]() {
					startUpdateThread(std::vector<QRemotePluginInfo>{info});
				}));
			}
		} else {
			menu->addChild(createMenuItem("Checking for updates..."));
		}
	}
};

struct NightBinWidget : QuestionableWidget {
	NightbinButton* menuButton = nullptr;
	ColorBGSimple* background = nullptr;

	void setText() {
		NVGcolor c = nvgRGB(255,255,255);
		color->textList.clear();
		color->addText("NIGHT-BIN", "OpenSans-ExtraBold.ttf", c, 24, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, 25));
		color->addText("·ISI·", "OpenSans-ExtraBold.ttf", c, 28, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, RACK_GRID_HEIGHT-13));
	}

	NightBinWidget(NightBin* module) {
		setModule(module);

		toggleableDescriptors = false;

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
			NVGcolor color;
			if (theme == "") color = nvgRGB(250, 250, 250);
			else if (theme == "Dark") color = nvgRGB(150, 150, 150);
			else if (theme == "Light") color = nvgRGB(55, 55, 55);
			for (int i = -3; i <= 3; i++) {
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, ((MODULE_SIZE * RACK_GRID_WIDTH)/8) * i, -150);
				nvgLineTo(args.vg, ((MODULE_SIZE * RACK_GRID_WIDTH)/8) * i, 150);
				nvgStrokeColor(args.vg, color);
				nvgStrokeWidth(args.vg, 2);
				nvgStroke(args.vg);
			}
			for (int i = -9; i <= 9; i++) {
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, -55, (RACK_GRID_HEIGHT/25) * i);
				nvgLineTo(args.vg, 55, (RACK_GRID_HEIGHT/25) * i);
				nvgStrokeColor(args.vg, color);
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

	Widget* getRackLayout() {
		if (!APP->scene || !APP->scene->menuBar) return nullptr;
		auto rackLayout = std::find_if(APP->scene->menuBar->children.begin(), APP->scene->menuBar->children.end(), [=](widget::Widget* widget) {
			return dynamic_cast<ui::SequentialLayout*>(widget) != nullptr;
		});
		if (rackLayout != APP->scene->menuBar->children.end()) return *rackLayout;
		return nullptr;
	}

	// try to add our own menu item to the main rack bar
	void setupMenuBar() {
		Widget* rackLayout = getRackLayout();

		if (rackLayout) {
			auto existing = std::find_if(rackLayout->children.begin(), rackLayout->children.end(), [=](widget::Widget* widget) {
				return dynamic_cast<NightbinButton*>(widget) != nullptr;
			});
			if (existing != rackLayout->children.end()) return;
			
			menuButton = new NightbinButton;
			rackLayout->addChildBelow(menuButton, rackLayout->children.back());
		} else WARN("Unable to add to racks menubar, could not find ui::SequentialLayout reference.");
	}

	~NightBinWidget() {
		if (!module) return;
		Widget* rackLayout = getRackLayout();
		if (rackLayout != nullptr && menuButton != nullptr) rackLayout->removeChild(menuButton);
	}

	void appendContextMenu(Menu *menu) override
  	{
		QuestionableWidget::appendContextMenu(menu);
	}

};

Model* modelNightBin = createModel<NightBin, NightBinWidget>("nightbin");
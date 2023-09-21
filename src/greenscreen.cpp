#include "plugin.hpp"
#include "imagepanel.cpp"
#include "textfield.cpp"
#include "colorBG.hpp"
#include "questionableModule.hpp"
#include <vector>
#include <algorithm>
#include <app/RailWidget.hpp>
#include <memory>

const int MODULE_SIZE = 3;

struct Greenscreen : QuestionableModule {
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

	NVGcolor color = nvgRGB(4, 244, 4);
	std::string text = "Green";
	bool showText = true;

	Greenscreen() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		supportsThemes = false;
		toggleableDescriptors = false;
		
	}

	void process(const ProcessArgs& args) override {

	}

	json_t* dataToJson() override {
		json_t* rootJ = QuestionableModule::dataToJson();
		json_object_set_new(rootJ, "colorR", json_real(color.r));
		json_object_set_new(rootJ, "colorG", json_real(color.g));
		json_object_set_new(rootJ, "colorB", json_real(color.b));
		json_object_set_new(rootJ, "text", json_string(text.c_str()));
		json_object_set_new(rootJ, "showText", json_boolean(showText));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		QuestionableModule::dataFromJson(rootJ);
		json_t* r = json_object_get(rootJ, "colorR");
		json_t* g = json_object_get(rootJ, "colorG");
		json_t* b = json_object_get(rootJ, "colorB");
		if (r && g && b) color = nvgRGBf(json_real_value(r), json_real_value(g), json_real_value(b));
		if (json_t* d = json_object_get(rootJ, "showText")) showText = json_boolean_value(d);
		if (json_t* t = json_object_get(rootJ, "text")) text = json_string_value(t);
	}

};

struct BackgroundWidget : Widget {
	NVGcolor color;

	BackgroundWidget() {

	}

	void draw(const DrawArgs& args) override {
		math::Vec min = args.clipBox.getTopLeft();
		math::Vec max = args.clipBox.getBottomRight();
		nvgFillColor(args.vg, color);
		nvgBeginPath(args.vg);
		nvgRect(args.vg, min.x, min.y, max.x, max.y);
		nvgFill(args.vg);
	}
};

/*struct QColorPicker : ui::MenuItem {

	QColorPicker() {
		box.size.x = 50;
		box.size.y = 50;
	}

	void draw(const DrawArgs& args) {


		
	}

};*/

struct GreenscreenWidget : QuestionableWidget {
	ColorBGSimple* background = nullptr;
	BackgroundWidget* newBackground = nullptr;

	std::string logoText = "Green";

	struct Color : QuestionableJsonable {
		std::string name;
		float r;
		float g;
		float b;

		Color() { }

		Color(std::string name, unsigned char r, unsigned char g, unsigned char b) {
			this->name = name;
			this->r = r / 255.f;
			this->g = g / 255.f;
			this->b = b / 255.f;
		}

		Color(std::string name, NVGcolor c) {
			this->name = name;
			r = c.r;
			g = c.g;
			b = c.b;
		}

		// https://stackoverflow.com/a/9733452
		float getBrightness() {
			return (0.2126*r + 0.7152*g + 0.0722*b);
		}

		float getContrast(Color other) {
			return abs(getBrightness() - other.getBrightness());
		}

		float getContrast(NVGcolor other) {
			return abs(getBrightness() - Color("", other).getBrightness());
		}

		NVGcolor getNVGColor() { return nvgRGBf(r, g, b); }

		json_t* toJson() override {
			json_t* rootJ = new json_t();
			json_object_set_new(rootJ, "colorR", json_real(r));
			json_object_set_new(rootJ, "colorG", json_real(g));
			json_object_set_new(rootJ, "colorB", json_real(b));
			json_object_set_new(rootJ, "text", json_string(name.c_str()));
			return rootJ;
		}

		void fromJson(json_t* rootJ) override {
			if (json_t* rj = json_object_get(rootJ, "r")) r = json_real_value(rj);
			if (json_t* gj = json_object_get(rootJ, "g")) g = json_real_value(gj);
			if (json_t* bj = json_object_get(rootJ, "b")) b = json_real_value(bj);
			if (json_t* t = json_object_get(rootJ, "name")) name = json_string_value(t);
		}

		bool operator==(Color other) {
			return name == other.name;
		}
	};

	std::vector<Color> selectableColors = {
		Color{"Green", 4, 244, 4},
		Color{"Black", 0, 0, 0},
		Color{"White", 255, 255, 255},
		Color{"Cyan", 0, 255, 255},
		Color{"Grey", 50, 50, 50},
		Color{"Yellow", 255, 255, 0},
		Color{"Maroon", 128, 0, 0},
		Color{"Dark Green", 0, 128, 0},
		Color{"Purple", 128, 0, 128},
		Color{"Teal", 0, 128, 128}
	};

	void setText() {
		NVGcolor c = nvgRGB(255,255,255);
		color->textList.clear();
		color->addText(rack::string::uppercase(logoText)+"SCREEN", "OpenSans-ExtraBold.ttf", c, 24, Vec(((MODULE_SIZE * RACK_GRID_WIDTH) / 2) - 6, 25), "default", nvgDegToRad(90.f), NVGalign::NVG_ALIGN_LEFT);
		color->addText("·ISI·", "OpenSans-ExtraBold.ttf", c, 28, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, RACK_GRID_HEIGHT-13));
	}

	void changeColor(Color c) {
		//NVGcolor c = selectableColors.count(name) ? selectableColors[name] : c;
		logoText = c.name; 

		setText();
		if (c.getContrast(nvgRGB(0,0,0)) > 0.75) color->setFontColor(nvgRGB(25,25,25));
		color->setTextGroupVisibility("default", ((Greenscreen*)module)->showText);

		background->color = c.getNVGColor();
		background->stroke = c.getNVGColor();
		if (newBackground) newBackground->color = c.getNVGColor();
		((Greenscreen*)module)->color = c.getNVGColor();
		((Greenscreen*)module)->text = c.name;

	}

	GreenscreenWidget(Greenscreen* module) {
		setModule(module);

		supportsThemes = false;
		toggleableDescriptors = false;

		background = new ColorBGSimple(Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT), nvgRGB(4, 244, 4), nvgRGB(4, 244, 4));

		color = new ColorBG(Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT));
		color->drawBackground = false;
		setText();

		backgroundColorLogic(module);
		
		setPanel(background);
		addChild(color);

		//addChild(new QuestionableDrawWidget(Vec(18, 100), [module](const DrawArgs &args) {
		//}));
		
		if (module) {
			// sneak our own background widget after the rail widget.
			auto railWidget = std::find_if(APP->scene->rack->children.begin(), APP->scene->rack->children.end(), [=](widget::Widget* widget) {
				return dynamic_cast<RailWidget*>(widget) != nullptr;
			});
			if (railWidget != APP->scene->rack->children.end()) {
				newBackground = new BackgroundWidget;
				newBackground->color = nvgRGB(0, 175, 26);
				APP->scene->rack->addChildAbove(newBackground, *railWidget);
			} else WARN("Unable to find railWidget");
			
			changeColor(Color(module->text, module->color));
		}

		//addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		//addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		//addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		//addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	}

	~GreenscreenWidget() {
		if (newBackground) APP->scene->rack->removeChild(newBackground);
	}

	Color preview;

	void updateToPreview() {
		changeColor(preview);
	}

	void appendContextMenu(Menu *menu) override
  	{

		Greenscreen* mod = (Greenscreen*)module;

		menu->addChild(createMenuItem("Toggle Text", "",[=]() {
			mod->showText = !mod->showText;
			color->setTextGroupVisibility("default", mod->showText);
		}));

		menu->addChild(createSubmenuItem("Change Color", "",[=](Menu* menu) {
			menu->addChild(createSubmenuItem("Custom", "", [=](Menu* menu) {
				//std::vector<Color> custom = userSettings.getArraySettingFunc<Color>("greenscreenColors" [=](json_t*) { return 0; });

				std::vector<Color> custom = userSettings.getArraySetting<Color>("greenscreenColors");

				menu->addChild(createSubmenuItem("Add Color", "", [=](Menu* menu) {

					menu->addChild(rack::createMenuLabel("Name:"));
					menu->addChild(new QTextField([=](std::string text) { preview.name = text; updateToPreview(); }, 100, ""));

					menu->addChild(rack::createMenuLabel("R:"));
					menu->addChild(new QTextField([=](std::string text) { 
						if (isInteger(text)) {
							preview.r = clamp<float>(0, 1, std::stoi(text)/255.f); 
							updateToPreview();
						}
					}, 100, "0"));

					menu->addChild(rack::createMenuLabel("G:"));
					menu->addChild(new QTextField([=](std::string text) { 
						if (isInteger(text)) {
							preview.g = clamp<float>(0, 1, std::stoi(text)/255.f); 
							updateToPreview();
						}
					}, 100, "0"));

					menu->addChild(rack::createMenuLabel("B:"));
					menu->addChild(new QTextField([=](std::string text) { 
						if (isInteger(text)) {
							preview.b = clamp<float>(0, 1, std::stoi(text)/255.f); 
							updateToPreview();
						}
					}, 100, "0"));

					menu->addChild(new MenuSeparator);

					menu->addChild(createMenuItem("Save", "", [=]() {
						std::vector<Color> custom = userSettings.getArraySetting<Color>("greenscreenColors");
						if (std::find(custom.begin(), custom.end(), preview) != custom.end()) return;

						custom.push_back(preview);
						userSettings.setArraySetting<Color>("greenscreenColors", custom);
					}));

					menu->addChild(createMenuItem("Clear", "", [=]() { 
						changeColor(Color("Green", nvgRGB(4, 244, 4)));
					}));
				}));

				if (!custom.empty()) menu->addChild(new MenuSeparator);

				for (const auto & ccData : custom) {
					menu->addChild(createMenuItem(ccData.name, "", [=]() { 
						changeColor(ccData);
					}));
				}
			}));
			for (const auto & selectableColor : selectableColors) {
				menu->addChild(createMenuItem(selectableColor.name, "", [=]() { 
					changeColor(selectableColor);
				}));
			}
		}));

		/*ui::TextField* param = new QTextField([=](std::string text) {
			if (text.length()) changeColor();
		});
		param->box.size.x = 100;
		menu->addChild(param);*/

		QuestionableWidget::appendContextMenu(menu);
	}

};

Model* modelGreenscreen = createModel<Greenscreen, GreenscreenWidget>("greenscreen");
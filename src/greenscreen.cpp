#include "plugin.hpp"
#include "imagepanel.cpp"
#include "ui.cpp"
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
		INPUT_R,
		INPUT_G,
		INPUT_B,
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
	bool showInputs = false;

	Greenscreen() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configInput(INPUT_R, "red");
		configInput(INPUT_G, "green");
		configInput(INPUT_B, "blue");

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
		json_object_set_new(rootJ, "showInputs", json_boolean(showInputs));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		QuestionableModule::dataFromJson(rootJ);
		json_t* r = json_object_get(rootJ, "colorR");
		json_t* g = json_object_get(rootJ, "colorG");
		json_t* b = json_object_get(rootJ, "colorB");
		if (r && g && b) color = nvgRGBf(json_real_value(r), json_real_value(g), json_real_value(b));
		if (json_t* d = json_object_get(rootJ, "showText")) showText = json_boolean_value(d);
		if (json_t* i = json_object_get(rootJ, "showInputs")) showInputs = json_boolean_value(i);
		if (json_t* t = json_object_get(rootJ, "text")) text = json_string_value(t);
	}

};

struct Color : QuestionableJsonable {
		std::string name;
		float r;
		float g;
		float b;

		Color() {
			r = 1.f;
			g = 1.f;
			b = 1.f;
		}

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

		float getContrastFrom(Color& other) {
			return abs(getBrightness() - other.getBrightness());
		}

		float getContrastFrom(NVGcolor other) {
			return abs(getBrightness() - Color("", other).getBrightness());
		}
		 
		// https://stackoverflow.com/a/1847112
		float getDifferenceFrom(Color other) {
			return sqrt(std::pow((other.r-r), 2) + std::pow((other.g-g), 2) + std::pow((other.b-b), 2));
		}

		NVGcolor getNVGColor() { return nvgRGBf(r, g, b); }

		static Color getClosestTo(std::vector<Color> list, Color other) {
			Color returnColor("Black", nvgRGB(0,0,0));
			for (auto c : list) {
				if (other.getDifferenceFrom(c) < other.getDifferenceFrom(returnColor)) returnColor = c;
			}
			return returnColor;
		}

		json_t* toJson() override {
			json_t* rootJ = json_object();
			json_object_set_new(rootJ, "r", json_real(r));
			json_object_set_new(rootJ, "g", json_real(g));
			json_object_set_new(rootJ, "b", json_real(b));
			json_object_set_new(rootJ, "name", json_string(name.c_str()));
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

struct BackgroundWidget : Widget {
	Greenscreen* module;
	NVGcolor color;

	BackgroundWidget() {

	}

	void draw(const DrawArgs& args) override {
		if (!module) return;
		if (module->isBypassed()) return;

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

struct RGBSliderQuantity : QuestionableQuantity {
	std::string label;
	RGBSliderQuantity(std::string label, quantityGetFunc g, quantitySetFunc s) : QuestionableQuantity(g, s) { 
		this->label = label;
	}

	float getDefaultValue() override {
		return 0.0f;
	}

	float getDisplayValue() override {
		return getValue() * 255;
	}

	void setDisplayValue(float displayValue) override {
		setValue(displayValue / 255);
	}

	std::string getUnit() override {
		return "";
	}

	std::string getLabel() override {
		return label;
	}

	std::string getDisplayValueString() override {
		float v = getDisplayValue();
		if (std::isnan(v))
			return "NaN";
		return std::to_string((int)getDisplayValue());
	}
};

struct RGBSlider : ui::Slider {
	RGBSlider(std::string label, quantityGetFunc valueGet, quantitySetFunc valueSet) {
		quantity = new RGBSliderQuantity(label, valueGet, valueSet);
		box.size.x = 150.0;
	}
	~RGBSlider() {
		delete quantity;
	}
};

struct GreenscreenPort : PJ301MPort {
	void draw(const DrawArgs &args) override {
		if (!module || (module && !((Greenscreen*)module)->showInputs)) return;

		NVGcolor color = nvgRGB(255, 255, 255);
		Color moduleColor = Color("", ((Greenscreen*)module)->color);
		if (moduleColor.getContrastFrom(nvgRGB(0,0,0)) > 0.75) color = nvgRGB(25,25,25);

		nvgFillColor(args.vg, color);
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, box.size.x/2, box.size.y/2, 10.f);
		nvgFill(args.vg);
	}

	void createTooltip() {
		if (module && ((Greenscreen*)module)->showInputs) PJ301MPort::createTooltip();
	}

	void onDragDrop(const DragDropEvent& e) override {
		if (module && ((Greenscreen*)module)->showInputs) PJ301MPort::onDragDrop(e);
	}

	void onDragStart(const DragStartEvent& e) override {
		if (module && ((Greenscreen*)module)->showInputs) PJ301MPort::onDragStart(e);
	}

	void appendContextMenu(ui::Menu* menu) override {
		if (module && ((Greenscreen*)module)->showInputs) PJ301MPort::appendContextMenu(menu); 
	}
};

struct GreenscreenWidget : QuestionableWidget {
	ColorBGSimple* background = nullptr;
	BackgroundWidget* newBackground = nullptr;

	std::string logoText = "Green";

	std::vector<Color> selectableColors = {
		Color{"Green", 4, 244, 4},
		Color{"Black", 0, 0, 0},
		Color{"White", 255, 255, 255},
		Color{"Cyan", 0, 255, 255},
		Color{"Grey", 50, 50, 50},
		Color{"Dark Grey", 20, 20, 20},
		Color{"Yellow", 255, 255, 0},
		Color{"Dark Red", 139,0,0},
		Color{"Maroon", 128, 0, 0},
		Color{"Dark Green", 0, 128, 0},
		Color{"Purple", 128, 0, 128},
		Color{"Teal", 0, 128, 128},
		Color{"Brown", 165,42,42},
		Color{"Olive", 128,128,0},
		Color{"Navy", 0,0,128},
		Color{"Beige", 245,245,220},
		Color{"Red", 255,0,0},
		Color{"Lime", 0,255,0},
		Color{"Blue",  0,0,255},
		Color{"Sienna", 160,82,45},
		Color("Honeydew", 240,255,240),
		Color("Pink", 255,192,203),
		Color("Hot Pink", 255,105,180),
		Color("Sky Blue", 135,206,235)
	};

	void setText() {
		NVGcolor c = nvgRGB(255,255,255);
		color->textList.clear();
		color->addText(rack::string::uppercase(logoText)+"SCREEN", "OpenSans-ExtraBold.ttf", c, 24, Vec(((MODULE_SIZE * RACK_GRID_WIDTH) / 2) - 6, 25), "default", nvgDegToRad(90.f), NVGalign::NVG_ALIGN_LEFT);
		color->addText("·ISI·", "OpenSans-ExtraBold.ttf", c, 28, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, RACK_GRID_HEIGHT-13));
	}

	void changeColor(Color c, bool save = true) {
		//NVGcolor c = selectableColors.count(name) ? selectableColors[name] : c;
		logoText = c.name; 

		setText();
		if (c.getContrastFrom(nvgRGB(0,0,0)) > 0.75) color->setFontColor(nvgRGB(25,25,25));
		color->setTextGroupVisibility("default", ((Greenscreen*)module)->showText);

		background->color = c.getNVGColor();
		background->stroke = c.getNVGColor();
		if (newBackground) newBackground->color = c.getNVGColor();
		if (save) ((Greenscreen*)module)->color = c.getNVGColor();
		if (save) ((Greenscreen*)module)->text = c.name;

	}

	GreenscreenWidget(Greenscreen* module) {
		setModule(module);

		std::sort(selectableColors.begin(), selectableColors.end(), [](Color& a, Color& b) { return a.name < b.name; });

		supportsThemes = false;
		toggleableDescriptors = false;

		background = new ColorBGSimple(Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT), nvgRGB(4, 244, 4), nvgRGB(4, 244, 4));

		color = new ColorBG(Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT));
		color->drawBackground = false;
		setText();

		backgroundColorLogic(module);
		
		setPanel(background);
		addChild(color);
		
		if (module) {
			// sneak our own background widget after the rail widget.
			auto railWidget = std::find_if(APP->scene->rack->children.begin(), APP->scene->rack->children.end(), [=](widget::Widget* widget) {
				return dynamic_cast<RailWidget*>(widget) != nullptr;
			});
			if (railWidget != APP->scene->rack->children.end()) {
				newBackground = new BackgroundWidget;
				newBackground->color = nvgRGB(0, 175, 26);
				newBackground->module = module;
				APP->scene->rack->addChildAbove(newBackground, *railWidget);
			} else WARN("Unable to find railWidget");
			
			changeColor(Color(module->text, module->color));
		}

		addInput(createInputCentered<QuestionablePort<GreenscreenPort>>(mm2px(Vec(7.8f, 90.f)), module, Greenscreen::INPUT_R));
		addInput(createInputCentered<QuestionablePort<GreenscreenPort>>(mm2px(Vec(7.8f, 100.f)), module, Greenscreen::INPUT_G));
		addInput(createInputCentered<QuestionablePort<GreenscreenPort>>(mm2px(Vec(7.8f, 110.f)), module, Greenscreen::INPUT_B));

		//addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		//addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		//addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		//addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	}

	dirtyable<bool> cvConnected = false;
	void step() override {
		QuestionableWidget::step();
		if (!module) return;
		Greenscreen* mod = (Greenscreen*)module;
		
		bool rConnected = module->inputs[Greenscreen::INPUT_R].isConnected();
		bool gConnected = module->inputs[Greenscreen::INPUT_G].isConnected();
		bool bConnected = module->inputs[Greenscreen::INPUT_B].isConnected();

		float rVal = abs(fmod(mod->color.r + (module->inputs[Greenscreen::INPUT_R].getVoltage()/10.f), deltaTime));
		float gVal = abs(fmod(mod->color.g + (module->inputs[Greenscreen::INPUT_G].getVoltage()/10.f), deltaTime));
		float bVal = abs(fmod(mod->color.b + (module->inputs[Greenscreen::INPUT_B].getVoltage()/10.f), deltaTime));
		
		cvConnected = rConnected || gConnected || bConnected;

		if (cvConnected) {
			float r = rConnected ? lerp<float>(newBackground->color.r, rVal, 0.1f) : mod->color.r;
			float g = gConnected ? lerp<float>(newBackground->color.g, gVal, 0.1f) : mod->color.g;
			float b = bConnected ? lerp<float>(newBackground->color.b, bVal, 0.1f) : mod->color.b;
			Color c = Color(logoText, nvgRGBf(r,g,b));
			c.name = std::string("RGB(") + to_string(rVal) + ", " + to_string(gVal) + ", " + to_string(bVal) + ")";
			changeColor(c, false);
		}

		if (cvConnected.isDirty() && cvConnected == false) changeColor(Color(mod->text, mod->color));
	}

	~GreenscreenWidget() {
		if (newBackground) APP->scene->rack->removeChild(newBackground);
	}

	Color preview = Color("", nvgRGB(255, 255, 255));
	bool setPreviewText = true;

	void updateToPreview() {
		changeColor(preview);
		if (setPreviewText) preview.name = Color::getClosestTo(selectableColors, preview).name + std::string("'ish");
	}

	void appendContextMenu(Menu *menu) override {

		Greenscreen* mod = (Greenscreen*)module;

		menu->addChild(createMenuItem("Toggle Text", "",[=]() {
			mod->showText = !mod->showText;
			color->setTextGroupVisibility("default", mod->showText);
		}));

		menu->addChild(createMenuItem("Toggle CV Inputs", "",[=]() {
			mod->showInputs = !mod->showInputs;
		}));

		menu->addChild(createSubmenuItem("Change Color", "",[=](Menu* menu) {
			std::vector<Color> custom = userSettings.getArraySetting<Color>("greenscreenCustomColors");

				menu->addChild(createSubmenuItem("Add Custom Color", "", [=](Menu* menu) {

					menu->addChild(rack::createMenuLabel("Name:"));
					QuestionableTextField* textField = new QuestionableTextField([=](std::string text) { 
						preview.name = text; 
						updateToPreview();
						setPreviewText = false;
					}, 100, "");
					//if (setPreviewText) preview.name = Color::getClosestTo(selectableColors, preview).name + std::string("'ish");
					textField->text = preview.name;
					menu->addChild(textField);

					menu->addChild(new RGBSlider("R",
						[=]() { return preview.r; },
						[=](float value) { 
							preview.r = clamp<float>(0, 1, value); 
							updateToPreview();
							if (setPreviewText) textField->text = preview.name;
						}
					));
					menu->addChild(new RGBSlider("G",
						[=]() { return preview.g; }, 
						[=](float value) { 
							preview.g = clamp<float>(0, 1, value); 
							updateToPreview();
							if (setPreviewText) textField->text = preview.name;
						}
					));
					menu->addChild(new RGBSlider("B",
						[=]() { return preview.b; },
						[=](float value) { 
							preview.b = clamp<float>(0, 1, value); 
							updateToPreview();
							if (setPreviewText) textField->text = preview.name;
						}
					));

					menu->addChild(new MenuSeparator);

					menu->addChild(createMenuItem("Save", "", [=]() {
						std::vector<Color> custom = userSettings.getArraySetting<Color>("greenscreenCustomColors");
						if (std::find(custom.begin(), custom.end(), preview) != custom.end()) return;

						custom.push_back(preview);
						userSettings.setArraySetting<Color>("greenscreenCustomColors", custom);

						preview = Color("", nvgRGB(255, 255, 255));
						setPreviewText = true;
					}));

					menu->addChild(createMenuItem("Clear", "", [=]() {
						changeColor(Color("Green", nvgRGB(4, 244, 4)));
						setPreviewText = true;
					}));

			}));

			if (!custom.empty()) {
				menu->addChild(createSubmenuItem("Remove Custom Color", "", [=](Menu* menu) {
					for (const auto & ccData : custom) {
						menu->addChild(createMenuItem(ccData.name, "-", [=]() { 
							std::vector<Color> customList = userSettings.getArraySetting<Color>("greenscreenCustomColors");
							auto it = std::remove(customList.begin(), customList.end(), ccData);
							if (it == customList.end()) return;
							customList.erase(it);
							userSettings.setArraySetting<Color>("greenscreenCustomColors", customList);
						}));
					}
				}));
			}

			menu->addChild(new MenuSeparator);

			if (!custom.empty()) {
				for (const auto & ccData : custom) {
					menu->addChild(createMenuItem(ccData.name, "", [=]() { 
						changeColor(ccData);
					}));
				}
				menu->addChild(new MenuSeparator);
			}

			for (const auto & selectableColor : selectableColors) {
				menu->addChild(createMenuItem(selectableColor.name, "", [=]() { 
					changeColor(selectableColor);
				}));
			}
		}));

		/*ui::TextField* param = new QuestionableTextField([=](std::string text) {
			if (text.length()) changeColor();
		});
		param->box.size.x = 100;
		menu->addChild(param);*/

		QuestionableWidget::appendContextMenu(menu);
	}

};

Model* modelGreenscreen = createModel<Greenscreen, GreenscreenWidget>("greenscreen");
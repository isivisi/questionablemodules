#include "plugin.hpp"
#include "imagepanel.cpp"
#include "colorBG.hpp"
#include "questionableModule.hpp"
#include <vector>
#include <algorithm>
#include <app/RailWidget.hpp>

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

    std::map<std::string, NVGcolor> selectableColors = {
        {"Green", nvgRGB(4, 244, 4)},
        {"Black", nvgRGB(0, 0, 0)},
        {"White", nvgRGB(255, 255, 255)},
        {"Cyan", nvgRGB(0, 255, 255)},
        {"Grey", nvgRGB(50, 50, 50)},
        {"Yellow", nvgRGB(255, 255, 0)},
        {"Maroon", nvgRGB(128, 0, 0)},
        {"Dark Green", nvgRGB(0, 128, 0)},
        {"Purple", nvgRGB(128, 0, 128)},
        {"Teal", nvgRGB(0, 128, 128)}
    };

	void setText() {
		NVGcolor c = nvgRGB(255,255,255);
		color->textList.clear();
		color->addText(rack::string::uppercase(logoText)+"SCREEN", "OpenSans-ExtraBold.ttf", c, 24, Vec(((MODULE_SIZE * RACK_GRID_WIDTH) / 2) - 6, 25), "default", nvgDegToRad(90.f), NVGalign::NVG_ALIGN_LEFT);
		color->addText("·ISI·", "OpenSans-ExtraBold.ttf", c, 28, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, RACK_GRID_HEIGHT-13));
	}

    void changeColor(std::string name, NVGcolor c) {
        //NVGcolor c = selectableColors.count(name) ? selectableColors[name] : c;
        logoText = name;
        setText();
        background->color = c;
        background->stroke = c;
        if (newBackground) newBackground->color = c;
        ((Greenscreen*)module)->color = c;
        ((Greenscreen*)module)->text = name;
    }

	GreenscreenWidget(Greenscreen* module) {
		setModule(module);

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

            color->setTextGroupVisibility("default", module->showText);
            changeColor(module->text, module->color);
        }

		//addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		//addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		//addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		//addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	}

    ~GreenscreenWidget() {
        if (newBackground) APP->scene->rack->removeChild(newBackground);
    }

     void appendContextMenu(Menu *menu) override
  	{

        Greenscreen* mod = (Greenscreen*)module;

        menu->addChild(createMenuItem("Toggle Text", "",[=]() {
            mod->showText = !mod->showText;
			color->setTextGroupVisibility("default", mod->showText);
		}));

        menu->addChild(createSubmenuItem("Change Color", "",[=](Menu* menu) {
            for (const auto & pair : selectableColors) {
                menu->addChild(createMenuItem(pair.first, "", [&]() { 
                    changeColor(pair.first, pair.second);
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
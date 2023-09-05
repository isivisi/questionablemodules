#include "plugin.hpp"
#include "imagepanel.cpp"
#include "colorBG.hpp"
#include "questionableModule.hpp"
#include <vector>
#include <algorithm>
#include <app/RailWidget.hpp>

const int MODULE_SIZE = 8;

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
    bool showText = true;

	Greenscreen() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		
	}

	void process(const ProcessArgs& args) override {

	}

    json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "colorR", json_integer(color.r));
        json_object_set_new(rootJ, "colorG", json_integer(color.g));
        json_object_set_new(rootJ, "colorB", json_integer(color.b));
		json_object_set_new(rootJ, "showText", json_boolean(showText));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		if (json_t* d = json_object_get(rootJ, "showText")) showText = json_boolean_value(d);
	}

};

struct BackgroundWidget : Widget {
    NVGcolor color;

    BackgroundWidget() {

    }

    void draw(const DrawArgs& args) {
        math::Vec min = args.clipBox.getTopLeft();
	    math::Vec max = args.clipBox.getBottomRight();
        nvgFillColor(args.vg, color);
		nvgBeginPath(args.vg);
		nvgRect(args.vg, min.x, min.y, max.x, max.y);
		nvgFill(args.vg);
    }
};

struct GreenscreenWidget : QuestionableWidget {
    ColorBGSimple* background = nullptr;
    BackgroundWidget* newBackground = nullptr;

	void setText() {
		NVGcolor c = nvgRGB(255,255,255);
		color->textList.clear();
		color->addText("GREENSCREEN", "OpenSans-ExtraBold.ttf", c, 24, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, 25));
		color->addText("·ISI·", "OpenSans-ExtraBold.ttf", c, 28, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, RACK_GRID_HEIGHT-13));
	}

    changeColor(NVGcolor c) {
        background->color = c;
        background->stroke = c;
        if (newBackground) newBackground->color = c;
        ((Greenscreen*)module)->color = c;
    }

	GreenscreenWidget(Greenscreen* module) {
		setModule(module);

        background = new ColorBGSimple(Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT));

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

            color->setTextGroupVisibility("descriptor", module->showText);
            changeColor(module->color);
        }

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	}

     void appendContextMenu(Menu *menu) override
  	{
        QuestionableWidget::appendContextMenu(menu);

        Greenscreen* mod = (Greenscreen*)module;

        menu->addChild(createMenuItem("Toggle Text", "",[=]() {
            mod->showText = !mod->showText;
			color->setTextGroupVisibility("default", mod->showText);
		}));

        /*ui::TextField* param = new QTextField([=](std::string text) {
			if (text.length()) changeColor();
		});
		param->box.size.x = 100;
		menu->addChild(param);*/
	}

};

Model* modelGreenscreen = createModel<Greenscreen, GreenscreenWidget>("greenscreen");
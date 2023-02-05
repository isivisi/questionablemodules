#include "plugin.hpp"
#include "imagepanel.cpp"
#include "colorBG.hpp"
#include "questionableModule.hpp"
#include <vector>
#include <algorithm>

/*
    Param: Read with params[...].getValue()
    Input: Read with inputs[...].getVoltage()
    Output: Write with outputs[...].setVoltage(voltage)
    Light: Write with lights[...].setBrightness(brightness)
*/

const int MODULE_SIZE = 6;

const int MAX_HISTORY = 32;
const int MAX_INPUTS = 8;

struct Nandomizer : QuestionableModule {
	enum ParamId {
		FADE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		VOLTAGE_IN_1,
		VOLTAGE_IN_2,
		VOLTAGE_IN_3,
		VOLTAGE_IN_4,
		VOLTAGE_IN_5,
		VOLTAGE_IN_6,
		VOLTAGE_IN_7,
		VOLTAGE_IN_8,
		TRIGGER,
		FADE_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		SINE_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		VOLTAGE_IN_LIGHT_1,
		VOLTAGE_IN_LIGHT_2,
		VOLTAGE_IN_LIGHT_3,
		VOLTAGE_IN_LIGHT_4,
		VOLTAGE_IN_LIGHT_5,
		VOLTAGE_IN_LIGHT_6,
		VOLTAGE_IN_LIGHT_7,
		VOLTAGE_IN_LIGHT_8,
		BLINK_LIGHT,
		LIGHTS_LEN
	};

	int inputsUsed{8};

	dsp::SchmittTrigger gateTrigger;

	float history[MAX_INPUTS][MAX_HISTORY] {{0.f}};
	int historyIterator[MAX_INPUTS] = {0};
	float inputFades[MAX_INPUTS] = {0.f};

	bool hasLoadedImage{false};

	int activeOutput = 0;

	Nandomizer() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(FADE_PARAM, 0.f, 1.f, 0.f, "Fade Amount");
		configInput(VOLTAGE_IN_1, "1");
		configInput(VOLTAGE_IN_2, "2");
		configInput(VOLTAGE_IN_3, "3");
		configInput(VOLTAGE_IN_4, "4");
		configInput(VOLTAGE_IN_5, "5");
		configInput(VOLTAGE_IN_6, "6");
		configInput(VOLTAGE_IN_7, "7");
		configInput(VOLTAGE_IN_8, "8");
		configInput(FADE_INPUT, "Fade");
		configOutput(SINE_OUTPUT, "");
		configInput(TRIGGER, "Gate");
		
	}

	int randomInteger(int min, int max) {
		std::random_device rd; // obtain a random number from hardware
		std::mt19937 gen(rd()); // seed the generator
		std::uniform_int_distribution<> distr(min, max); // define the range
		return distr(gen);
	}

	float rmsValue(float arr[], int n) {
		int square = 0;
		float mean = 0.0, root = 0.0;

		for (int i = 0; i < n; i++) {
			square += pow(arr[i], 2);
		}

		mean = (square / (float)(n));

		root = sqrt(mean);

		return root;
	}

	float fclamp(float min, float max, float value) {
		return std::min(min, std::max(max, value));
	}

	void process(const ProcessArgs& args) override {

		std::vector<int> usableInputs;
		float fadeAmnt = params[FADE_PARAM].getValue() + inputs[FADE_INPUT].getVoltage();

		bool shouldRandomize = gateTrigger.process(inputs[8].getVoltage(), 0.1f, 2.f);

		for (int i = 0; i < MAX_INPUTS; i++) {
			if (inputs[i].isConnected()) {
				usableInputs.push_back(i);
				lights[i].setBrightness(1.f);
			} else {
				lights[i].setBrightness(0.f);
			}
		}

		if (!usableInputs.size()) return;

		if (shouldRandomize) activeOutput = usableInputs[randomInteger(0, usableInputs.size()-1)];

		float fadingInputs = 0.f;
		for (int i = 0; i < MAX_INPUTS; i++) {
			if (i != activeOutput) {
				inputFades[i] = std::max(0.f, inputFades[i] - ((inputFades[i] * args.sampleTime)));
				fadingInputs += inputs[i].getVoltage() * inputFades[i];
			} else {
				inputFades[i] = 1.f;
			}
		}

		outputs[0].setVoltage(inputs[activeOutput].getVoltage() + (fadingInputs * fadeAmnt));

		if (shouldRandomize) lights[BLINK_LIGHT].setBrightness(1.f);

		else if (lights[BLINK_LIGHT].getBrightness() > 0.0) lights[BLINK_LIGHT].setBrightness(lights[BLINK_LIGHT].getBrightness() - 0.0001f);

	}

};

struct NandomizerWidget : QuestionableWidget {

	void setText() {
		NVGcolor c = nvgRGB(255,255,255);
		color->textList.clear();

		color->addText("INS", "OpenSans-Bold.ttf", c, 7, Vec(45.35, 257), "descriptor");

		color->addText("FADE", "OpenSans-Bold.ttf", c, 7, Vec(24.5, 315), "descriptor");
		color->addText("GATE", "OpenSans-Bold.ttf", c, 7, Vec(24.5, 353), "descriptor");
		color->addText("OUT", "OpenSans-Bold.ttf", c, 7, Vec(66, 353), "descriptor");
	}

	NandomizerWidget(Nandomizer* module) {
		setModule(module);
		//setPanel(createPanel(asset::plugin(pluginInstance, "res/nrandomizer.svg")));

		backdrop = new ImagePanel();
		backdrop->box.size = Vec(6 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
		backdrop->imagePath = asset::plugin(pluginInstance, "res/backdrop.jpg");
		backdrop->scalar = 3.5;
		backdrop->visible = true;

		color = new ColorBG(Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT));
		color->drawBackground = false;
		if (module) setText();

		if (module && module->theme.size()) {
			color->drawBackground = true;
			color->setTheme(BG_THEMES[module->theme]);
		}
		if (module) color->setTextGroupVisibility("descriptor", module->showDescriptors);
		
		setPanel(backdrop);
		addChild(color);

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<QuestionableParam<RoundSmallBlackKnob>>(mm2px(Vec(8.24, 90)), module, Nandomizer::FADE_PARAM));
		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(8.24, 100)), module, Nandomizer::FADE_INPUT));
		
		for (int i = 0; i < MAX_INPUTS; i++) {
			addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(15.24, 10.478  + (10.0*float(i)))), module, i));
			addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(20.24, 15.478 + (10.0*float(i)))), module, i));
		}

		addOutput(createOutputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(22.24, 113)), module, Nandomizer::SINE_OUTPUT));
		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(8.24, 113)), module, Nandomizer::TRIGGER));

		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(15.24, 102.713)), module, Nandomizer::BLINK_LIGHT));
	}

};

Model* modelNandomizer = createModel<Nandomizer, NandomizerWidget>("nandomizer");
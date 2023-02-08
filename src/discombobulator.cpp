#include "plugin.hpp"
#include "imagepanel.cpp"
#include "colorBG.hpp"
#include "questionableModule.hpp"
#include <vector>
#include <list>
#include <random>
#include <algorithm>

/*
    Param: Read with params[...].getValue()
    Input: Read with inputs[...].getVoltage()
    Output: Write with outputs[...].setVoltage(voltage)
    Light: Write with lights[...].setBrightness(brightness)
*/

const int MODULE_SIZE = 9;

const int MAX_HISTORY = 32;
const int MAX_INPUTS = 8;

struct Discombobulator : QuestionableModule {
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
		VOLTAGE_OUT_1,
		VOLTAGE_OUT_2,
		VOLTAGE_OUT_3,
		VOLTAGE_OUT_4,
		VOLTAGE_OUT_5,
		VOLTAGE_OUT_6,
		VOLTAGE_OUT_7,
		VOLTAGE_OUT_8,
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

	int outputSwaps[MAX_INPUTS];
	float fadingInputs[MAX_INPUTS][MAX_INPUTS] = {{0.f}};
	float lastInput[MAX_INPUTS] = {0.f};

	Discombobulator() {
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
		configOutput(VOLTAGE_OUT_1, "1");
		configOutput(VOLTAGE_OUT_2, "2");
		configOutput(VOLTAGE_OUT_3, "3");
		configOutput(VOLTAGE_OUT_4, "4");
		configOutput(VOLTAGE_OUT_5, "5");
		configOutput(VOLTAGE_OUT_6, "6");
		configOutput(VOLTAGE_OUT_7, "7");
		configOutput(VOLTAGE_OUT_8, "8");
		configInput(TRIGGER, "Gate");

		// Initialize default locations
		for (int i = 0; i < MAX_INPUTS; i++) {
			outputSwaps[i] = i;
		}
		
	}

	void process(const ProcessArgs& args) override {

		std::vector<int> usableInputs;
		float fadeAmnt = params[FADE_PARAM].getValue() + inputs[FADE_INPUT].getVoltage();

		bool shouldRandomize = gateTrigger.process(inputs[8].getVoltage(), 0.1f, 2.f);

		for (int i = 0; i < MAX_INPUTS; i++) {
			if (inputs[i].isConnected()) {
				usableInputs.push_back(i);
			}
		}

		if (!usableInputs.size()) return;

		// swap usable inputs
		if (shouldRandomize) {
			std::vector<int> usableInputPool(usableInputs);
			//std::random_shuffle(usableInputPool, usableInputPool.size());
			for (int i = usableInputs.size() -1; i >= 0; i--) {
				int randomInput = randomInt<int>(0, usableInputPool.size()-1);
				fadingInputs[usableInputs[i]][outputSwaps[usableInputs[i]]] = 1.f; // set full fade before swapping
				outputSwaps[usableInputs[i]] = usableInputPool[randomInput];
				usableInputPool.erase(usableInputPool.begin()+randomInput);
			}
		}
		
		float cumulatedFading[MAX_INPUTS] = {0.f};
		for (int i = 0; i < MAX_INPUTS; i++) {
			for (int x = 0; x < MAX_INPUTS; x++) {
				if (x != i) { // skip own audio
					fadingInputs[i][x] = std::max(0.f, fadingInputs[i][x] - (fadingInputs[i][x] * args.sampleTime));
					cumulatedFading[i] += inputs[x].getVoltage() * fadingInputs[i][x];
				}
			}
		}

		for (int i = 0; i < MAX_INPUTS; i++) {
			outputs[i].setVoltage(inputs[outputSwaps[i]].getVoltage() + (cumulatedFading[i] * fadeAmnt));
		}

		if (shouldRandomize) lights[BLINK_LIGHT].setBrightness(1.f);
		else if (lights[BLINK_LIGHT].getBrightness() > 0.0) lights[BLINK_LIGHT].setBrightness(lights[BLINK_LIGHT].getBrightness() - 0.0001f);

	}
};

struct DiscombobulatorWidget : QuestionableModuleWidget {

	void setText() {
		NVGcolor c = nvgRGB(255,255,255);
		color->textList.clear();

		color->addText("INS", "OpenSans-Bold.ttf", c, 7, Vec(30, 257), "descriptor");
		color->addText("OUTS", "OpenSans-Bold.ttf", c, 7, Vec(104.35, 257), "descriptor");

		color->addText("FADE", "OpenSans-Bold.ttf", c, 7, Vec(30, 353), "descriptor");
		color->addText("GATE", "OpenSans-Bold.ttf", c, 7, Vec(104.35, 353), "descriptor");
	}

	DiscombobulatorWidget(Discombobulator* module) {
		setModule(module);
		//setPanel(createPanel(asset::plugin(pluginInstance, "res/nrandomizer.svg")));

		backdrop = new ImagePanel();
		backdrop->box.size = Vec(9 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
		backdrop->imagePath = asset::plugin(pluginInstance, "res/backdrop-dis.jpg");
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

		//addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 46.063)), module, Nrandomizer::PITCH_PARAM));

		addParam(createParamCentered<QuestionableParam<RoundSmallBlackKnob>>(mm2px(Vec(35.24, 103)), module, Discombobulator::FADE_PARAM));
		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(35.24, 113)), module, Discombobulator::FADE_INPUT));
		
		for (int i = 0; i < MAX_INPUTS; i++) {
			addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(10, 10.478  + (10.0*float(i)))), module, i));
			addOutput(createOutputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(35.24, 10.478  + (10.0*float(i)))), module, i));
		}

		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(10, 113)), module, Discombobulator::TRIGGER));

		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(14.24, 106.713)), module, Discombobulator::BLINK_LIGHT));
	}
};

Model* modelDiscombobulator = createModel<Discombobulator, DiscombobulatorWidget>("discombobulator");
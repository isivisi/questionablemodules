#include "plugin.hpp"
#include <vector>
#include <list>
#include <random>

/*
    Param: Read with params[...].getValue()
    Input: Read with inputs[...].getVoltage()
    Output: Write with outputs[...].setVoltage(voltage)
    Light: Write with lights[...].setBrightness(brightness)
*/

const int MAX_HISTORY = 32;
const int MAX_INPUTS = 8;

struct Discombobulator : Module {
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

	int outputSwaps[MAX_INPUTS];
	float lastInput[MAX_INPUTS] = {0.f};

	float lastTriggerValue = 0.0;

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

	int randomInteger(int min, int max) {
		std::random_device rd; // obtain a random number from hardware
		std::mt19937 gen(rd()); // seed the generator
		std::uniform_int_distribution<> distr(min, max); // define the range
		return distr(gen);
	}

	void process(const ProcessArgs& args) override {

		std::vector<int> usableInputs;

		bool shouldRandomize = fabs(lastTriggerValue - inputs[8].getVoltage()) > 0.1f;
		lastTriggerValue = inputs[8].getVoltage();

		for (int i = 0; i < MAX_INPUTS; i++) {
			float voltage = inputs[i].getVoltage();
			if (inputs[i].isConnected()) {
				usableInputs.push_back(i);
			}
		}

		if (!usableInputs.size()) return;

		// swap usable inputs
		//float fadingInputs[MAX_INPUTS][MAX_INPUTS] = {{0.f}};
		if (shouldRandomize) {
			for (int i = usableInputs.size() -1; i >= 0; i--) {
				//fadingInputs[i][outputSwaps[i]] = 1.f; // set full fade before swapping
				outputSwaps[i] = randomInteger(0, usableInputs.size());
				usableInputs.pop_back();
			}
		}

		/*for (int i = 0; i < MAX_INPUTS; i++) {
			if (i != activeOutput) {
				inputFades[i] = std::max(0.f, inputFades[i] - ((inputFades[i] * args.sampleTime)));
				fadingInputs[i] += inputs[i].getVoltage() * inputFades[i];
			} else {
				inputFades[i] = 1.f;
			}
		}*/

		for (int i = 0; i < MAX_INPUTS; i++) {
			outputs[i].setVoltage(inputs[outputSwaps[i]].getVoltage());
		}

		if (shouldRandomize) lights[BLINK_LIGHT].setBrightness(1.f);
		else if (lights[BLINK_LIGHT].getBrightness() > 0.0) lights[BLINK_LIGHT].setBrightness(lights[BLINK_LIGHT].getBrightness() - 0.0001f);

	}
};

struct MSMPanel : TransparentWidget {
  NVGcolor backgroundColor = componentlibrary::SCHEME_LIGHT_GRAY;
  float scalar = 1.0;
  std::string imagePath;
	void draw(const DrawArgs &args) override {
      std::shared_ptr<Image> backgroundImage = APP->window->loadImage(imagePath);
	  nvgBeginPath(args.vg);
	  nvgRect(args.vg, 0.0, 0.0, box.size.x, box.size.y);

	  // Background color
	  if (backgroundColor.a > 0) {
	    nvgFillColor(args.vg, backgroundColor);
	    nvgFill(args.vg);
	  }

	  // Background image
	  if (backgroundImage) {
	    int width, height;
	    nvgImageSize(args.vg, backgroundImage->handle, &width, &height);
	    NVGpaint paint = nvgImagePattern(args.vg, 0.0, 0.0, width/scalar, height/scalar, 0.0, backgroundImage->handle, 1.0);
	    nvgFillPaint(args.vg, paint);
	    nvgFill(args.vg);
	  }

	  // Border
	  NVGcolor borderColor = componentlibrary::SCHEME_LIGHT_GRAY; //nvgRGBAf(0.5, 0.5, 0.5, 0.5);
	  nvgBeginPath(args.vg);
	  nvgRect(args.vg, 0.5, 0.5, box.size.x - 1.0, box.size.y - 1.0);
	  nvgStrokeColor(args.vg, borderColor);
	  nvgStrokeWidth(args.vg, 1.0);
	  nvgStroke(args.vg);

	  Widget::draw(args);
	}
};


struct DiscombobulatorWidget : ModuleWidget {
	MSMPanel *backdrop;

	DiscombobulatorWidget(Discombobulator* module) {
		setModule(module);
		//setPanel(createPanel(asset::plugin(pluginInstance, "res/nrandomizer.svg")));

		backdrop = new MSMPanel();
		backdrop->box.size = Vec(9 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
		backdrop->imagePath = asset::plugin(pluginInstance, "res/backdrop-dis.png");
		backdrop->scalar = 3.5;
		backdrop->visible = true;
		
		setPanel(backdrop);

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		//addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 46.063)), module, Nrandomizer::PITCH_PARAM));

		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(8.24, 90)), module, Discombobulator::FADE_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.24, 100)), module, Discombobulator::FADE_INPUT));
		
		for (int i = 0; i < MAX_INPUTS; i++) {
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10, 10.478  + (10.0*float(i)))), module, i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(35.24, 10.478  + (10.0*float(i)))), module, i));
		}

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(35.24, 113)), module, Discombobulator::SINE_OUTPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10, 113)), module, Discombobulator::TRIGGER));

		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(15.24, 102.713)), module, Discombobulator::BLINK_LIGHT));
	}
};

Model* modelDiscombobulator = createModel<Discombobulator, DiscombobulatorWidget>("discombobulator");
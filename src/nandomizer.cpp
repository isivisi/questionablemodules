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

struct Nandomizer : Module {
	enum ParamId {
		PITCH_PARAM,
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

	float history[MAX_INPUTS][MAX_HISTORY] {{0.0}};
	int historyIterator[MAX_INPUTS] = {0};
	float lastInput[MAX_INPUTS] = {0.f};

	bool hasLoadedImage{false};

	int activeOutput = randomInteger(0, MAX_INPUTS-1);
	float lastTriggerValue = 0.0;

	Nandomizer() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(PITCH_PARAM, 0.f, 1.f, 0.f, "");
		configInput(VOLTAGE_IN_1, "");
		configInput(VOLTAGE_IN_2, "");
		configInput(VOLTAGE_IN_3, "");
		configInput(VOLTAGE_IN_4, "");
		configInput(VOLTAGE_IN_5, "");
		configInput(VOLTAGE_IN_6, "");
		configInput(VOLTAGE_IN_7, "");
		configInput(VOLTAGE_IN_8, "");
		configOutput(SINE_OUTPUT, "");
		configInput(TRIGGER, "");
		
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

	int randomInteger(int min, int max) {
		std::random_device rd; // obtain a random number from hardware
		std::mt19937 gen(rd()); // seed the generator
		std::uniform_int_distribution<> distr(min, max); // define the range
		return distr(gen);
	}

	void process(const ProcessArgs& args) override {

		float inputRMSValues[inputsUsed] = {0};
		std::vector<int> usableInputs;

		bool shouldRandomize = fabs(lastTriggerValue - inputs[8].getVoltage()) > 0.1f;
		lastTriggerValue = inputs[8].getVoltage();

		for (int i = 0; i < MAX_INPUTS; i++) {
			float voltage = inputs[i].getVoltage();
			if (lastInput[i] != voltage) usableInputs.push_back(i);
			lastInput[i] = voltage;
		}

		if (shouldRandomize) activeOutput = usableInputs[randomInteger(0, usableInputs.size()-1)];

		for (int i = 0; i < inputsUsed; i++) {
			float inputVoltage = inputs[i].getVoltage();
			
			history[i][historyIterator[i]] = inputVoltage;
			historyIterator[i] += 1;
			if (historyIterator[i] >= MAX_HISTORY) historyIterator[i] = 0;

			inputRMSValues[i] = rmsValue(history[i], MAX_HISTORY);
		}

		outputs[0].setVoltage(inputs[activeOutput].getVoltage());

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


struct NandomizerWidget : ModuleWidget {
	MSMPanel *backdrop;

	NandomizerWidget(Nandomizer* module) {
		setModule(module);
		//setPanel(createPanel(asset::plugin(pluginInstance, "res/nrandomizer.svg")));

		backdrop = new MSMPanel();
		backdrop->box.size = Vec(6 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
		backdrop->imagePath = asset::plugin(pluginInstance, "res/backdrop.png");
		backdrop->scalar = 3.5;
		backdrop->visible = true;
		
		setPanel(backdrop);

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		//addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 46.063)), module, Nrandomizer::PITCH_PARAM));
		
		if (module) {
			for (int i = 0; i < module->inputsUsed; i++) {
				addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.24, 15.478  + (10.0*float(i)))), module, i));
				addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(20.24, 20.478 + (10.0*float(i)))), module, i));
			}
		}

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.24, 113)), module, Nandomizer::SINE_OUTPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.24, 113)), module, Nandomizer::TRIGGER));

		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(15.24, 102.713)), module, Nandomizer::BLINK_LIGHT));
	}
};

Model* modelNandomizer = createModel<Nandomizer, NandomizerWidget>("nandomizer");
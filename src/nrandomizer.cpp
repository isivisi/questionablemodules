#include "plugin.hpp"
#include <vector>
#include <list>

/*
    Param: Read with params[...].getValue()
    Input: Read with inputs[...].getVoltage()
    Output: Write with outputs[...].setVoltage(voltage)
    Light: Write with lights[...].setBrightness(brightness)
*/

const int MAX_HISTORY = 32;
const int MAX_INPUTS = 2;

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


struct Nrandomizer : Module {
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
		INPUTS_LEN
	};
	enum OutputId {
		SINE_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		BLINK_LIGHT,
		LIGHTS_LEN
	};

	float history[MAX_INPUTS][MAX_HISTORY] {{0.0}};
	int historyIterator[MAX_INPUTS] = {0};

	bool hasLoadedImage{false};

	Nrandomizer() {
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
		
	}

	void process(const ProcessArgs& args) override {

		float inputRMSValues[MAX_INPUTS] = {0};

		for (int i = 0; i < MAX_INPUTS; i++) {
			float inputVoltage = inputs[i].getVoltage();
			
			history[i][historyIterator[i]] = inputVoltage;
			historyIterator[i] += 1;
			if (historyIterator[i] >= MAX_HISTORY) historyIterator[i] = 0;

			inputRMSValues[i] = rmsValue(history[i], MAX_HISTORY);
		}



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
	  NVGcolor borderColor = nvgRGBAf(0.5, 0.5, 0.5, 0.5);
	  nvgBeginPath(args.vg);
	  nvgRect(args.vg, 0.5, 0.5, box.size.x - 1.0, box.size.y - 1.0);
	  nvgStrokeColor(args.vg, borderColor);
	  nvgStrokeWidth(args.vg, 1.0);
	  nvgStroke(args.vg);

	  Widget::draw(args);
	}
};


struct NrandomizerWidget : ModuleWidget {
	MSMPanel *backdrop;

	NrandomizerWidget(Nrandomizer* module) {
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

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.24, 22.478)), module, Nrandomizer::VOLTAGE_IN_1));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.24, 33.478)), module, Nrandomizer::VOLTAGE_IN_2));
		//addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.24, 44.478)), module, Nrandomizer::VOLTAGE_IN_3));
		//addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.24, 55.478)), module, Nrandomizer::VOLTAGE_IN_4));
		//addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.24, 66.478)), module, Nrandomizer::VOLTAGE_IN_5));
		//addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.24, 77.478)), module, Nrandomizer::VOLTAGE_IN_6));
		//addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.24, 88.478)), module, Nrandomizer::VOLTAGE_IN_7));
		//addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.24, 99.478)), module, Nrandomizer::VOLTAGE_IN_8));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15.24, 108.713)), module, Nrandomizer::SINE_OUTPUT));

		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(15.24, 25.81)), module, Nrandomizer::BLINK_LIGHT));
	}
};

Model* modelNrandomizer = createModel<Nrandomizer, NrandomizerWidget>("nrandomizer");
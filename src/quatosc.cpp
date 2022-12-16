#include "plugin.hpp"
#include "imagepanel.cpp"
#include <gmtl/gmtl.h>
#include <gmtl/Vec.h>
#include <gmtl/Quat.h>
#include <vector>
#include <algorithm>

/*
    Param: Read with params[...].getValue()
    Input: Read with inputs[...].getVoltage()
    Output: Write with outputs[...].setVoltage(voltage)
    Light: Write with lights[...].setBrightness(brightness)
*/

// https://ggt.sourceforge.net/html/gmtlfaq.html

int MODULE_SIZE = 12;

struct QuatOSC : Module {
	enum ParamId {
		FADE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		VOLTAGE_IN_1,
		TRIGGER,
		FADE_INPUT,
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

    gmtl::Quatf sphereQuat; // defaults to identity
    gmtl::Vec3f pointOnSphere;
	gmtl::Vec3f pointOnSphereRotated;

	dsp::SchmittTrigger gateTrigger;

	QuatOSC() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(FADE_PARAM, 0.f, 1.f, 0.f, "Fade Amount");
		configInput(VOLTAGE_IN_1, "1");
		configInput(FADE_INPUT, "Fade");
		configOutput(SINE_OUTPUT, "");
		configInput(TRIGGER, "Gate");

		pointOnSphere = gmtl::Vec3f(25.f, 0.f, 0.f);
		
	}

	float fclamp(float min, float max, float value) {
		return std::min(min, std::max(max, value));
	}

	void process(const ProcessArgs& args) override {
		gmtl::Quatf newRot;

		//gmtl::Vec3f angleVec = gmtl::normalize(gmtl::Vec3f(0.f, 1.f, 0.f));

		gmtl::EulerAngleXYZf angle = gmtl::EulerAngleXYZf(0.f, -0.1f, -0.3f);

		gmtl::lerp(newRot, args.sampleTime * 44, sphereQuat, sphereQuat * gmtl::make<gmtl::Quatf>(angle));
		sphereQuat = newRot;

		pointOnSphereRotated = sphereQuat * pointOnSphere;

	}
};

struct QuatDisplay : Widget {
	QuatOSC* module;

	QuatDisplay() {

	}

	void draw(const DrawArgs &args) override {
		//if (module == NULL) return;

		nvgFillColor(args.vg, nvgRGB(15, 15, 15));
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
        nvgFill(args.vg);

	}

	void drawLayer(const DrawArgs &args, int layer) override {
		if (module == NULL) return;

		// nvgScale
		// nvgText
		// nvgFontSize
		// nvgCreateFont
		// nvgText(0, 0 )

		nvgFillColor(args.vg, nvgRGB(17, 17, 17));
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, 25.f, 25.f, 25);
        nvgFill(args.vg);

		nvgFillColor(args.vg, nvgRGB(15, 250, 15));
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, 25.f + module->pointOnSphereRotated[0], 25.f + module->pointOnSphereRotated[1], 5);
        nvgFill(args.vg);

	
	}

};

struct QuatOSCWidget : ModuleWidget {
	ImagePanel *backdrop;
	QuatDisplay *display;

	QuatOSCWidget(QuatOSC* module) {
		setModule(module);
		//setPanel(createPanel(asset::plugin(pluginInstance, "res/nrandomizer.svg")));

		backdrop = new ImagePanel();
		backdrop->box.size = Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
		backdrop->imagePath = asset::plugin(pluginInstance, "res/backdrop.jpg");
		backdrop->scalar = 3.5;
		backdrop->visible = true;

		display = new QuatDisplay();
		display->box.pos = Vec(2, 50);
        display->box.size = Vec(((MODULE_SIZE -1) * RACK_GRID_WIDTH) + 10, 100);
		display->module = module;
		
		setPanel(backdrop);
		addChild(display);

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(8.24, 90)), module, QuatOSC::FADE_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.24, 100)), module, QuatOSC::FADE_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.24, 113)), module, QuatOSC::SINE_OUTPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.24, 113)), module, QuatOSC::TRIGGER));

		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(15.24, 102.713)), module, QuatOSC::BLINK_LIGHT));
	}
};

Model* modelQuatOSC = createModel<QuatOSC, QuatOSCWidget>("quatosc");
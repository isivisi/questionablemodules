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
		X_PARAM,
		Y_PARAM,
		Z_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		VOLTAGE_IN_1,
		VOLTAGE_IN_2,
		VOLTAGE_IN_3,
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
		configParam(X_PARAM, 0.f, 1.f, 0.f, "X Rot");
		configParam(Y_PARAM, 0.f, 1.f, 0.f, "Y Rot");
		configParam(Z_PARAM, 0.f, 1.f, 0.f, "Z Rot");
		configInput(VOLTAGE_IN_1, "Euler X");
		configInput(VOLTAGE_IN_1, "Euler Y");
		configInput(VOLTAGE_IN_1, "Euler Z");
		configInput(FADE_INPUT, "Fade");
		configOutput(SINE_OUTPUT, "");
		configInput(TRIGGER, "Gate");

		pointOnSphere = gmtl::Vec3f(50.f, 0.f, 0.f);
		
	}

	float fclamp(float min, float max, float value) {
		return std::min(min, std::max(max, value));
	}

	void process(const ProcessArgs& args) override {
		gmtl::Quatf newRot;
		gmtl::Quatf visualRot;

		//gmtl::Vec3f angleVec = gmtl::normalize(gmtl::Vec3f(0.f, 1.f, 0.f));

		gmtl::Vec3f angle = gmtl::Vec3f(params[X_PARAM].getValue() + inputs[VOLTAGE_IN_1].getVoltage(), params[Y_PARAM].getValue() + inputs[VOLTAGE_IN_2].getVoltage(), params[Z_PARAM].getValue() + inputs[VOLTAGE_IN_3].getVoltage());
		//gmtl::normalize(angle);

		//angle *= 44000;

		gmtl::Quatf rotOffset = gmtl::makePure(angle);
		gmtl::Quatf newRotTo =  sphereQuat + (rotOffset * sphereQuat);
		gmtl::normalize(newRotTo);

		gmtl::lerp(newRot, args.sampleTime * 44, sphereQuat, newRotTo);
		sphereQuat = newRot;
		gmtl::normalize(sphereQuat);

		pointOnSphereRotated = sphereQuat * pointOnSphere;

		gmtl::Vec3f norm = pointOnSphereRotated;
		gmtl::normalize(norm);

		outputs[SINE_OUTPUT].setVoltage((norm[1]) * 2.f);

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

		nvgFillColor(args.vg, nvgRGB(25, 25, 25));
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, 100.f, 50.f, 50);
        nvgFill(args.vg);

		gmtl::Vec3f yPoint = gmtl::Vec3f(0.f, 50.f, 0.f);
		gmtl::Vec3f zPoint = gmtl::Vec3f(0.f, 0.f, 50.f);

		yPoint = module->sphereQuat * yPoint;
		zPoint = module->sphereQuat * zPoint;

		nvgFillColor(args.vg, nvgRGB(15, 250, 15));
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, 100.f + module->pointOnSphereRotated[0], 50.f + module->pointOnSphereRotated[1], 3);
        nvgFill(args.vg);

		nvgFillColor(args.vg, nvgRGB(250, 250, 15));
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, 100.f + yPoint[0], 50.f + yPoint[1], 3);
        nvgFill(args.vg);

		nvgFillColor(args.vg, nvgRGB(15, 15, 250));
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, 100.f + zPoint[0], 50.f + zPoint[1], 3);
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


		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.24, 113)), module, QuatOSC::SINE_OUTPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10, 90)), module, QuatOSC::VOLTAGE_IN_1));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20, 90)), module, QuatOSC::VOLTAGE_IN_2));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(30, 90)), module, QuatOSC::VOLTAGE_IN_3));
		
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(10, 80)), module, QuatOSC::X_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(20, 80)), module, QuatOSC::Y_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(30, 80)), module, QuatOSC::Z_PARAM));
	}
};

Model* modelQuatOSC = createModel<QuatOSC, QuatOSCWidget>("quatosc");
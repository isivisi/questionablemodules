#include "plugin.hpp"
#include "imagepanel.cpp"
#include <gmtl/gmtl.h>
#include <gmtl/Vec.h>
#include <gmtl/Quat.h>
#include <vector>
#include <cmath>
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
		X_FLO_I_PARAM,
		Y_FLO_I_PARAM,
		Z_FLO_I_PARAM,
		X_FLO_F_PARAM,
		Y_FLO_F_PARAM,
		Z_FLO_F_PARAM,
		X_POS_I_PARAM,
		Y_POS_I_PARAM,
		Z_POS_I_PARAM,
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
    gmtl::Vec3f xPointOnSphere;
	gmtl::Vec3f yPointOnSphere;
	gmtl::Vec3f zPointOnSphere;
	gmtl::Vec3f pointOnSphereRotated;

	dsp::SchmittTrigger gateTrigger;

	float lfo1Phase = 0.f;
	float lfo2Phase = 0.f;
	float lfo3Phase = 0.f;

	QuatOSC() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(X_FLO_I_PARAM, 0.f, 1.f, 0.f, "X LFO Influence");
		configParam(Y_FLO_I_PARAM, 0.f, 1.f, 0.f, "Y LFO Influence");
		configParam(Z_FLO_I_PARAM, 0.f, 1.f, 0.f, "Z LFO Influence");
		configParam(X_FLO_F_PARAM, 0.f, 512.f, 0.f, "X LFO Frequency");
		configParam(Y_FLO_F_PARAM, 0.f, 512.f, 0.f, "Y LFO Frequency");
		configParam(Z_FLO_F_PARAM, 0.f, 512.f, 0.f, "Z LFO Frequency");
		configParam(X_POS_I_PARAM, 0.f, 1.f, 1.f, "X Position Influence");
		configParam(Y_POS_I_PARAM, 0.f, 1.f, 0.f, "Y Position Influence");
		configParam(Z_POS_I_PARAM, 0.f, 1.f, 0.f, "Z Position Influence");
		configInput(VOLTAGE_IN_1, "Euler X");
		configInput(VOLTAGE_IN_1, "Euler Y");
		configInput(VOLTAGE_IN_1, "Euler Z");
		configInput(FADE_INPUT, "Fade");
		configOutput(SINE_OUTPUT, "");
		configInput(TRIGGER, "Gate");

		xPointOnSphere = gmtl::Vec3f(50.f, 0.f, 0.f);
		yPointOnSphere = gmtl::Vec3f(0.f, 50.f, 0.f);
		zPointOnSphere = gmtl::Vec3f(0.f, 0.f, 50.f);
		
	}

	float fclamp(float min, float max, float value) {
		return std::min(min, std::max(max, value));
	}

	void process(const ProcessArgs& args) override {
		gmtl::Quatf newRot;
		gmtl::Quatf visualRot;

		//gmtl::Vec3f angleVec = gmtl::normalize(gmtl::Vec3f(0.f, 1.f, 0.f));

		lfo1Phase += params[X_FLO_F_PARAM].getValue() * args.sampleTime;
		lfo2Phase += params[Y_FLO_F_PARAM].getValue() * args.sampleTime;
		lfo3Phase += params[Z_FLO_F_PARAM].getValue() * args.sampleTime;
		float flo1Val = params[X_FLO_I_PARAM].getValue() * sin(2.f * M_PI * lfo1Phase);
		float flo2Val = params[Y_FLO_I_PARAM].getValue() * sin(2.f * M_PI * lfo2Phase);
		float flo3Val = params[Z_FLO_I_PARAM].getValue() * sin(2.f * M_PI * lfo3Phase);

		gmtl::Vec3f angle = gmtl::Vec3f(params[X_FLO_I_PARAM].getValue() + flo1Val, params[Y_FLO_I_PARAM].getValue() + flo2Val, params[Z_FLO_I_PARAM].getValue() + flo3Val);
		gmtl::normalize(angle);

		gmtl::Quatf rotOffset = gmtl::makePure(angle);
		gmtl::Quatf newRotTo =  sphereQuat + (rotOffset * sphereQuat);
		gmtl::normalize(newRotTo);
		gmtl::lerp(newRot, args.sampleTime * dsp::FREQ_C4 * 3, sphereQuat, newRotTo);

		sphereQuat += rotOffset;
		gmtl::normalize(sphereQuat);

		gmtl::Vec3f xRotated = sphereQuat * xPointOnSphere;
		gmtl::Vec3f yRotated = sphereQuat * xPointOnSphere;
		gmtl::Vec3f zRotated = sphereQuat * xPointOnSphere;

		gmtl::normalize(xRotated);
		gmtl::normalize(yRotated);
		gmtl::normalize(zRotated);

		outputs[SINE_OUTPUT].setVoltage(((xRotated[1] * params[X_POS_I_PARAM].getValue()) + (yRotated[1] * params[Y_POS_I_PARAM].getValue()) + (zRotated[1] * params[Z_POS_I_PARAM].getValue())) * 3.f);

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

		gmtl::Vec3f xPoint = gmtl::Vec3f(50.f, 0.f, 0.f);
		gmtl::Vec3f yPoint = gmtl::Vec3f(0.f, 50.f, 0.f);
		gmtl::Vec3f zPoint = gmtl::Vec3f(0.f, 0.f, 50.f);

		xPoint = module->sphereQuat * xPoint;
		yPoint = module->sphereQuat * yPoint;
		zPoint = module->sphereQuat * zPoint;

		nvgFillColor(args.vg, nvgRGB(15, 250, 15));
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, 100.f + xPoint[0], 50.f + xPoint[1], 3);
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
		
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(10, 70)), module, QuatOSC::X_FLO_F_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(20, 70)), module, QuatOSC::Y_FLO_F_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(30, 70)), module, QuatOSC::Z_FLO_F_PARAM));

		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(10, 80)), module, QuatOSC::X_FLO_I_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(20, 80)), module, QuatOSC::Y_FLO_I_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(30, 80)), module, QuatOSC::Z_FLO_I_PARAM));

		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(10, 60)), module, QuatOSC::X_POS_I_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(20, 60)), module, QuatOSC::Y_POS_I_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(30, 60)), module, QuatOSC::Z_POS_I_PARAM));
	}
};

Model* modelQuatOSC = createModel<QuatOSC, QuatOSCWidget>("quatosc");
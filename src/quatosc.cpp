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
		VOCT1_OCT,
		VOCT2_OCT,
		VOCT3_OCT,
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
		VOCT,
		VOCT2,
		VOCT3,
		VOLTAGE_IN_2,
		VOLTAGE_IN_3,
		TRIGGER,
		FADE_INPUT,
		CLOCK_INPUT,
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
	gmtl::Quatf visualQuat;
    gmtl::Vec3f xPointOnSphere;
	gmtl::Vec3f yPointOnSphere;
	gmtl::Vec3f zPointOnSphere;
	gmtl::Vec3f pointOnSphereRotated;

	dsp::SchmittTrigger gateTrigger;
	dsp::SchmittTrigger clockTrigger;
	dsp::Timer clockTimer;

	float clockFreq = 2.f;

	float lfo1Phase = 0.f;
	float lfo2Phase = 0.f;
	float lfo3Phase = 0.f;

	QuatOSC() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(X_FLO_I_PARAM, 0.f, 1.f, 0.f, "X LFO Influence");
		configParam(Y_FLO_I_PARAM, 0.f, 1.f, 0.f, "Y LFO Influence");
		configParam(Z_FLO_I_PARAM, 0.f, 1.f, 0.f, "Z LFO Influence");
		configParam(X_FLO_F_PARAM, 0.f, 100.f, 0.f, "X LFO Frequency");
		configParam(Y_FLO_F_PARAM, 0.f, 100.f, 0.f, "Y LFO Frequency");
		configParam(Z_FLO_F_PARAM, 0.f, 100.f, 0.f, "Z LFO Frequency");
		configParam(X_POS_I_PARAM, 0.f, 1.f, 1.f, "X Position Influence");
		configParam(Y_POS_I_PARAM, 0.f, 1.f, 0.f, "Y Position Influence");
		configParam(Z_POS_I_PARAM, 0.f, 1.f, 0.f, "Z Position Influence");
		configSwitch(VOCT1_OCT, 0.f, 8.f, 0.f, "VOct 1 Octave", {"1", "2", "3", "4", "5", "6", "7", "8"});
		configSwitch(VOCT2_OCT, 0.f, 8.f, 0.f, "VOct 2 Octave", {"1", "2", "3", "4", "5", "6", "7", "8"});
		configSwitch(VOCT3_OCT, 0.f, 8.f, 0.f, "VOct 3 Octave", {"1", "2", "3", "4", "5", "6", "7", "8"});
		configInput(VOCT, "VOct");
		configInput(VOCT, "VOct 2");
		configInput(VOCT, "VOct 3");
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

	float VecCombine(gmtl::Vec3f vector) {
		return vector[0] + vector[1];// + vector[2];
	}

	float processLFO(float &phase, float frequency, float deltaTime, int voct = -1) {

		float voctFreq = (clockFreq / 2.f) * dsp::approxExp2_taylor5((inputs[voct].getVoltage() + std::round(params[voct].getValue())) + 30.f) / std::pow(2.f, 30.f);

		phase += ((frequency) + (voct != -1 ? voctFreq : 0.f)) * deltaTime;
		phase -= trunc(phase);

		return sin(2.f * M_PI * phase);
	}

	gmtl::Quatf AngleAxis(float angle, const gmtl::Vec3f& axis)
	{
		gmtl::Quatf quat;

		// Normalize the axis of rotation
		gmtl::Vec3f normalized_axis = axis;
		gmtl::normalize(normalized_axis);

		// Convert the angle to half-angle
		float half_angle = angle * 0.5f;

		// Compute the quaternion elements
		quat[0] = std::cos(half_angle);
		quat[1] = normalized_axis[0] * std::sin(half_angle);
		quat[2] = normalized_axis[1] * std::sin(half_angle);
		quat[3] = normalized_axis[2] * std::sin(half_angle);

		return quat;
	}

	gmtl::Quatf rotation;

	gmtl::Vec3f momentum;
	gmtl::Vec3f lfoRotations;

	void process(const ProcessArgs& args) override {
		gmtl::Quatf newRot;

		if (inputs[CLOCK_INPUT].isConnected()) {
			clockTimer.process(args.sampleTime);
			if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 2.f)) {
				float clockFreq = 1.f / clockTimer.getTime();
				clockTimer.reset();
				if (0.001f <= clockFreq && clockFreq <= 1000.f) {
					this->clockFreq = clockFreq;
				}
			}
		} else clockFreq = 2.f;

		float lfo1Val = params[X_FLO_I_PARAM].getValue()  * ((processLFO(lfo1Phase, params[X_FLO_F_PARAM].getValue(), args.sampleTime, VOCT)));
		float lfo2Val = params[Y_FLO_I_PARAM].getValue()  * ((processLFO(lfo2Phase, params[Y_FLO_F_PARAM].getValue(), args.sampleTime, VOCT2)));
		float lfo3Val = params[Z_FLO_I_PARAM].getValue()  * ((processLFO(lfo3Phase, params[Z_FLO_F_PARAM].getValue(), args.sampleTime, VOCT3)));

		gmtl::Vec3f angle = gmtl::Vec3f(lfo1Val, lfo2Val, lfo3Val);
		gmtl::Quatf rotOffset = gmtl::makePure(angle);


		gmtl::Quatf newRotTo = rotOffset;
		gmtl::normalize(newRotTo);

		sphereQuat = newRotTo;

		gmtl::normalize(sphereQuat);

		gmtl::Quatf visualRotTo = visualQuat * sphereQuat;
		gmtl::lerp(visualQuat, args.sampleTime * 50, visualQuat, visualRotTo);
		gmtl::normalize(visualQuat);

		gmtl::Vec3f xRotated = sphereQuat * xPointOnSphere;
		gmtl::Vec3f yRotated = sphereQuat * yPointOnSphere;
		gmtl::Vec3f zRotated = sphereQuat * zPointOnSphere;

		gmtl::normalize(xRotated);
		gmtl::normalize(yRotated);
		gmtl::normalize(zRotated);

		outputs[SINE_OUTPUT].setVoltage((((VecCombine(xRotated) * params[X_POS_I_PARAM].getValue()) + (VecCombine(yRotated) * params[Y_POS_I_PARAM].getValue()) + (VecCombine(zRotated) * params[Z_POS_I_PARAM].getValue()))) * 3.f);

	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "sphereQuatX", json_real(sphereQuat[0]));
		json_object_set_new(rootJ, "sphereQuatY", json_real(sphereQuat[1]));
		json_object_set_new(rootJ, "sphereQuatZ", json_real(sphereQuat[2]));
		json_object_set_new(rootJ, "sphereQuatW", json_real(sphereQuat[3]));

		return rootJ;
	}


	void dataFromJson(json_t* rootJ) override {

		float x,y,z,w;

		if (json_t* jx = json_object_get(rootJ, "sphereQuatX")) x = json_real_value(jx);
		if (json_t* jy = json_object_get(rootJ, "sphereQuatY")) y = json_real_value(jy);
		if (json_t* jz = json_object_get(rootJ, "sphereQuatZ")) z = json_real_value(jz);
		if (json_t* jw = json_object_get(rootJ, "sphereQuatW")) w = json_real_value(jw);

		//sphereQuat = gmtl::Quatf(x,y,z,w);

	}

};

const int MAX_HISTORY = 100;

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

	struct pointHistory {
		gmtl::Vec3f point;
		NVGcolor color;
	};

	pointHistory xhistory[MAX_HISTORY];
	int xhistoryCursor = 0;
	pointHistory yhistory[MAX_HISTORY];
	int yhistoryCursor = 0;
	pointHistory zhistory[MAX_HISTORY];
	int zhistoryCursor = 0;

	void addToHistory(gmtl::Vec3f vec, NVGcolor color, pointHistory* history, int &cursor) {
		history[cursor] = pointHistory{vec, color};
		cursor = (cursor + 1) % MAX_HISTORY;
	}

	void drawLayer(const DrawArgs &args, int layer) override {
		if (module == NULL) return;

		nvgSave(args.vg);
		nvgScissor(args.vg, 0, 0, box.size.x, box.size.y);

		nvgFillColor(args.vg, nvgRGB(25, 25, 25));
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, 100.f, 50.f, 50);
        nvgFill(args.vg);

		gmtl::Vec3f xPoint = gmtl::Vec3f(50.f, 0.f, 0.f);
		gmtl::Vec3f yPoint = gmtl::Vec3f(0.f, 50.f, 0.f);
		gmtl::Vec3f zPoint = gmtl::Vec3f(0.f, 0.f, 50.f);

		xPoint = module->visualQuat * xPoint;
		yPoint = module->visualQuat * yPoint;
		zPoint = module->visualQuat * zPoint;

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

		addToHistory(xPoint, nvgRGB(15, 250, 15), xhistory, xhistoryCursor);
		addToHistory(yPoint, nvgRGB(250, 250, 15), yhistory, yhistoryCursor);
		addToHistory(zPoint, nvgRGB(15, 15, 250), zhistory, zhistoryCursor);

		nvgBeginPath(args.vg);
		for (int i = (xhistoryCursor+1)%MAX_HISTORY; i != xhistoryCursor; i=(i+1)%MAX_HISTORY) {
			//nvgFillColor(args.vg, history[i].color);
			//nvgBeginPath(args.vg);
			//nvgCircle(args.vg, 100.f + history[i].point[0], 50.f + history[i].point[1], 3);
			//nvgFill(args.vg);
			if (i == 0) nvgMoveTo(args.vg, 100.f + xhistory[i].point[0], 50.f + xhistory[i].point[1]);
			else nvgQuadTo(args.vg, 100.f + xhistory[i].point[0], 50.f + xhistory[i].point[1], 100.f + xhistory[i].point[0], 50.f + xhistory[i].point[1]);
			//else nvgLineTo(args.vg, 100.f + history[i].point[0], 50.f + history[i].point[1]);

		}
		nvgStrokeColor(args.vg, nvgRGB(15, 250, 15));
		nvgStrokeWidth(args.vg, 2.f);
		nvgStroke(args.vg);
		nvgClosePath(args.vg);

		nvgBeginPath(args.vg);
		for (int i = (yhistoryCursor+1)%MAX_HISTORY; i != yhistoryCursor; i=(i+1)%MAX_HISTORY) {
			if (i == 0) nvgMoveTo(args.vg, 100.f + yhistory[i].point[0], 50.f + yhistory[i].point[1]);
			else nvgQuadTo(args.vg, 100.f + yhistory[i].point[0], 50.f + yhistory[i].point[1], 100.f + yhistory[i].point[0], 50.f + yhistory[i].point[1]);
		}
		nvgStrokeColor(args.vg, nvgRGB(250, 250, 15));
		nvgStrokeWidth(args.vg, 2.f);
		nvgStroke(args.vg);
		nvgClosePath(args.vg);

		nvgBeginPath(args.vg);
		for (int i = (zhistoryCursor+1)%MAX_HISTORY; i != zhistoryCursor; i=(i+1)%MAX_HISTORY) {
			if (i == 0) nvgMoveTo(args.vg, 100.f + zhistory[i].point[0], 50.f + zhistory[i].point[1]);
			else nvgQuadTo(args.vg, 100.f + zhistory[i].point[0], 50.f + zhistory[i].point[1], 100.f + zhistory[i].point[0], 50.f + zhistory[i].point[1]);
		}
		nvgStrokeColor(args.vg, nvgRGB(15, 15, 250));
		nvgStrokeWidth(args.vg, 2.f);
		nvgStroke(args.vg);
		nvgClosePath(args.vg);

		nvgRestore(args.vg);
	
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
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10, 100)), module, QuatOSC::VOCT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20, 100)), module, QuatOSC::VOCT2));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(30, 100)), module, QuatOSC::VOCT3));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(50, 100)), module, QuatOSC::CLOCK_INPUT));

		//addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(10, 90)), module, Treequencer::SEND_VOCT_X, Treequencer::SEND_VOCT_X_LIGHT));
		//addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(20, 90)), module, Treequencer::SEND_VOCT_Y, Treequencer::SEND_VOCT_Y_LIGHT));
		//addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(20, 90)), module, Treequencer::SEND_VOCT_Z, Treequencer::SEND_VOCT_Z_LIGHT));
		
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(10, 70)), module, QuatOSC::X_FLO_F_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(20, 70)), module, QuatOSC::Y_FLO_F_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(30, 70)), module, QuatOSC::Z_FLO_F_PARAM));

		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(10, 90)), module, QuatOSC::X_FLO_I_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(20, 90)), module, QuatOSC::Y_FLO_I_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(30, 90)), module, QuatOSC::Z_FLO_I_PARAM));

		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(10, 80)), module, QuatOSC::VOCT1_OCT));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(20, 80)), module, QuatOSC::VOCT2_OCT));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(30, 80)), module, QuatOSC::VOCT3_OCT));

		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(10, 60)), module, QuatOSC::X_POS_I_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(20, 60)), module, QuatOSC::Y_POS_I_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(30, 60)), module, QuatOSC::Z_POS_I_PARAM));

	}
};

Model* modelQuatOSC = createModel<QuatOSC, QuatOSCWidget>("quatosc");
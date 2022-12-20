#include "plugin.hpp"
#include "imagepanel.cpp"
#include <gmtl/gmtl.h>
#include <gmtl/Vec.h>
#include <gmtl/Quat.h>
#include <vector>
#include <cmath>
#include <queue>
#include <mutex>
#include <algorithm>

/*
    Param: Read with params[...].getValue()
    Input: Read with inputs[...].getVoltage()
    Output: Write with outputs[...].setVoltage(voltage)
    Light: Write with lights[...].setBrightness(brightness)
*/

// https://ggt.sourceforge.net/html/gmtlfaq.html

int MODULE_SIZE = 12;
const int MAX_HISTORY = 400;
const int SAMPLES_PER_SECOND = MAX_HISTORY*20;

bool reading = false;

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
		TRIGGER,
		CLOCK_INPUT,
		VOCT1_OCT_INPUT,
		VOCT2_OCT_INPUT,
		VOCT3_OCT_INPUT,
		X_FLO_I_INPUT,
		Y_FLO_I_INPUT,
		Z_FLO_I_INPUT,
		X_FLO_F_INPUT,
		Y_FLO_F_INPUT,
		Z_FLO_F_INPUT,
		X_POS_I_INPUT,
		Y_POS_I_INPUT,
		Z_POS_I_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		MONO_OUT,
		//LEFT_OUT,
		//RIGHT_OUT,
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
	float lfo2Phase = 0.24;
	float lfo3Phase = 0.48f;

	float freqHistory1;
	float freqHistory2;
	float freqHistory3;

	std::queue<gmtl::Vec3f> xPointSamples;
	std::queue<gmtl::Vec3f> yPointSamples;
	std::queue<gmtl::Vec3f> zPointSamples;

	QuatOSC() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(X_FLO_I_PARAM, 0.f, 1.f, 0.f, "X LFO Influence");
		configParam(Y_FLO_I_PARAM, 0.f, 1.f, 0.f, "Y LFO Influence");
		configParam(Z_FLO_I_PARAM, 0.f, 1.f, 0.f, "Z LFO Influence");
		configParam(X_FLO_F_PARAM, 0.f, 100.f, 0.f, "X LFO Frequency");
		configParam(Y_FLO_F_PARAM, 0.f, 100.f, 0.f, "Y LFO Frequency");
		configParam(Z_FLO_F_PARAM, 0.f, 100.f, 0.f, "Z LFO Frequency");
		configParam(X_POS_I_PARAM, 0.f, 1.f, 1.f, "X Position Influence");
		configParam(Y_POS_I_PARAM, 0.f, 1.f, 1.f, "Y Position Influence");
		configParam(Z_POS_I_PARAM, 0.f, 1.f, 1.f, "Z Position Influence");
		configSwitch(VOCT1_OCT, 0.f, 8.f, 0.f, "VOct 1 Octave", {"1", "2", "3", "4", "5", "6", "7", "8"});
		configSwitch(VOCT2_OCT, 0.f, 8.f, 0.f, "VOct 2 Octave", {"1", "2", "3", "4", "5", "6", "7", "8"});
		configSwitch(VOCT3_OCT, 0.f, 8.f, 0.f, "VOct 3 Octave", {"1", "2", "3", "4", "5", "6", "7", "8"});
		configInput(VOCT, "VOct");
		configInput(VOCT2, "VOct 2");
		configInput(VOCT3, "VOct 3");
		configInput(VOCT1_OCT_INPUT, "VOct 1 Octave");
		configInput(VOCT2_OCT_INPUT, "VOct 2 Octave");
		configInput(VOCT3_OCT_INPUT, "VOct 3 Octave");
		configInput(X_FLO_I_INPUT, "X LFO Influence");
		configInput(Y_FLO_I_INPUT, "Y LFO Influence");
		configInput(Z_FLO_I_INPUT, "Z LFO Influence");
		configInput(X_FLO_F_INPUT, "X LFO Frequency");
		configInput(Y_FLO_F_INPUT, "Y LFO Frequency");
		configInput(Z_FLO_F_INPUT, "Z LFO Frequency");
		configInput(X_POS_I_INPUT, "X Position Influence");
		configInput(Y_POS_I_INPUT, "Y Position Influence");
		configInput(Z_POS_I_INPUT, "Z Position Influence");
		configInput(CLOCK_INPUT, "Clock");
		//configOutput(LEFT_OUT, "Left");
		//configOutput(RIGHT_OUT, "Right");
		configOutput(MONO_OUT, "Mono");
		configInput(TRIGGER, "Gate");

		xPointOnSphere = gmtl::Vec3f(65.f, 0.f, 0.f);
		yPointOnSphere = gmtl::Vec3f(0.f, 65.f, 0.f);
		zPointOnSphere = gmtl::Vec3f(0.f, 0.f, 65.f);
		
	}

	float fclamp(float min, float max, float value) {
		return std::min(max, std::max(min, value));
	}

	float getValue(int value, bool c=false) {
		if (c) return fclamp(0.f, 1.f, params[value].getValue() + inputs[value + VOCT1_OCT_INPUT].getVoltage());
		else return params[value].getValue() + inputs[value + VOCT1_OCT_INPUT].getVoltage();
	}

	inline float VecCombine(gmtl::Vec3f vector) {
		return vector[0] + vector[1];// + vector[2];
	}

	inline float calcVOctFreq(int input) {
		return (clockFreq / 2.f) * dsp::approxExp2_taylor5((inputs[input].getVoltage() + std::round(getValue(input))) + 30.f) / std::pow(2.f, 30.f);
	}

	float processLFO(float &phase, float frequency, float deltaTime, float &freqHistory, int voct = -1) {

		float voctFreq = calcVOctFreq(voct);

		if (!voctFreq - freqHistory > 0.001) {
			resetPhase(); 
			freqHistory = voctFreq;
		}

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

	void resetPhase() {
		lfo1Phase = 0.f;
		lfo2Phase = 0.24f;;
		lfo3Phase = 0.48f;
		sphereQuat = gmtl::Quatf(0,0,0,1);
	}

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

		float lfo1Val = getValue(X_FLO_I_PARAM, true)  * ((processLFO(lfo1Phase, getValue(X_FLO_F_PARAM), args.sampleTime, freqHistory1, VOCT)));
		float lfo2Val = getValue(Y_FLO_I_PARAM, true)  * ((processLFO(lfo2Phase, getValue(Y_FLO_F_PARAM), args.sampleTime, freqHistory2, VOCT2)));
		float lfo3Val = getValue(Z_FLO_I_PARAM, true)  * ((processLFO(lfo3Phase, getValue(Z_FLO_F_PARAM), args.sampleTime, freqHistory3, VOCT3)));

		gmtl::Vec3f angle = gmtl::Vec3f(lfo1Val, lfo2Val, lfo3Val);
		gmtl::Quatf rotOffset = gmtl::makePure(angle);

		gmtl::normalize(rotOffset);

		sphereQuat = rotOffset;

		gmtl::normalize(sphereQuat);

		gmtl::Vec3f xRotated = sphereQuat * xPointOnSphere;
		gmtl::Vec3f yRotated = sphereQuat * yPointOnSphere;
		gmtl::Vec3f zRotated = sphereQuat * zPointOnSphere;

		gmtl::normalize(xRotated);
		gmtl::normalize(yRotated);
		gmtl::normalize(zRotated);

		if (/*args.frame % (int)(args.sampleRate/SAMPLES_PER_SECOND) == 0 && */!reading) {
			xPointSamples.push(sphereQuat * xPointOnSphere);
			yPointSamples.push(sphereQuat * yPointOnSphere);
			zPointSamples.push(sphereQuat * zPointOnSphere);
		}
		
		/*double left;
		double right;

		double xAxis = VecCombine(xRotated) * params[X_POS_I_PARAM].getValue();
		double yAxis = VecCombine(yRotated) * params[Y_POS_I_PARAM].getValue();
		double zAxis = VecCombine(zRotated) * params[Z_POS_I_PARAM].getValue();

		// seperate into left right channels by their y value
		left += xRotated[0] > 0 ? xAxis * xRotated[0] > 0 : 0.f;
		left += xAxis * (1-xRotated[0]);
		right += xRotated[0] < 0 ? xAxis * xRotated[0] > 0 : 0.f;;
		right += xAxis * (1-xRotated[0]);

		left += yRotated[0] > 0 ? xAxis * yRotated[0] > 0 : 0.f;
		left += xAxis * (1-yRotated[0]);
		right += yRotated[0] < 0 ? xAxis * yRotated[0] > 0 : 0.f;
		right += xAxis * (1-yRotated[0]);

		left += zRotated[0] > 0 ? xAxis * zRotated[0] > 0 : 0.f;
		left += xAxis * (1-zRotated[0]);
		right += zRotated[0] < 0 ? xAxis * zRotated[0] > 0 : 0.f;;
		right += xAxis * (1-zRotated[0]);

		outputs[LEFT_OUT].setVoltage((left));
		outputs[RIGHT_OUT].setVoltage((right));*/

		outputs[MONO_OUT].setVoltage((((VecCombine(xRotated) * getValue(X_POS_I_PARAM, true)) + (VecCombine(yRotated) * getValue(Y_POS_I_PARAM, true)) + (VecCombine(zRotated) * getValue(Z_POS_I_PARAM, true)))) * 3.f);

	}

	void dataFromJson(json_t* rootJ) override {
		resetPhase();
	}

	/*json_t* dataToJson() override {
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

		sphereQuat = gmtl::Quatf(x,y,z,w);

	}*/

};

struct QuatDisplay : Widget {
	QuatOSC* module;

	float rad = 73.f;

	QuatDisplay() {

	}

	void draw(const DrawArgs &args) override {
		//if (module == NULL) return;

		float centerX = box.size.x/2;
		float centerY = box.size.y/2;

		nvgFillColor(args.vg, nvgRGB(15, 15, 15));
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, centerX, centerY, rad);
        nvgFill(args.vg);

	}

	struct vecHistory {
		gmtl::Vec3f history[MAX_HISTORY+1];
		int cursor = 0;
	};

	vecHistory xhistory;
	vecHistory yhistory;
	vecHistory zhistory;

	void addToHistory(gmtl::Vec3f& vec, vecHistory& h) {
		h.history[h.cursor] = vec;
		h.cursor = (h.cursor + 1) % MAX_HISTORY;
	}

	void drawHistory(NVGcontext* vg, std::queue<gmtl::Vec3f> &history, NVGcolor color, vecHistory& localHistory) {
		float centerX = box.size.x/2;
		float centerY = box.size.y/2;
		bool f = true;

		// grab points from audio thread and clear and add it to our own history list
		while (history.size() > 1) {
			addToHistory(history.front(), localHistory);
			history.pop();
		}

		// Iterate from oldest history value to latest
		nvgBeginPath(vg);
		for (int i = (localHistory.cursor+1)%MAX_HISTORY; i != localHistory.cursor; i = (i+1)%MAX_HISTORY) {
			gmtl::Vec3f point = localHistory.history[i];
			if (f) nvgMoveTo(vg, centerX + point[0], centerY + point[1]);
			//else nvgQuadTo(vg, centerX + point[0], centerY + point[1], centerX + point[0], centerY + point[1]);
			else nvgLineTo(vg, centerX + point[0], centerY + point[1]);
			f = false;
		}
		nvgStrokeColor(vg, color);
		nvgStrokeWidth(vg, 2.5f);
		nvgStroke(vg);
		nvgClosePath(vg);

	}

	void drawLayer(const DrawArgs &args, int layer) override {

		nvgSave(args.vg);
		//nvgScissor(args.vg, 0, 0, box.size.x, box.size.y);

		float centerX = box.size.x/2;
		float centerY = box.size.y/2;

		if (module == NULL) return;

		float xInf = module->getValue(QuatOSC::X_POS_I_PARAM, true);
		float yInf = module->getValue(QuatOSC::Y_POS_I_PARAM, true);
		float zInf = module->getValue(QuatOSC::Z_POS_I_PARAM, true);

		//gmtl::Vec3f xPoint = module->xPointSamples.back();
		//gmtl::Vec3f yPoint = module->yPointSamples.back();
		//gmtl::Vec3f zPoint = module->zPointSamples.back();

		nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
		nvgStrokeWidth(args.vg, 1.f);

		/*nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, centerX, centerY);
		nvgLineTo(args.vg, centerX + xPoint[0], centerY + xPoint[1]);
		nvgStroke(args.vg);
		nvgClosePath(args.vg);
		
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, centerX, centerY);
		nvgLineTo(args.vg, centerX + yPoint[0], centerY + yPoint[1]);
		nvgStroke(args.vg);
		nvgClosePath(args.vg);

		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, centerX, centerY);
		nvgLineTo(args.vg, centerX + zPoint[0], centerY + zPoint[1]);
		nvgStroke(args.vg);
		nvgClosePath(args.vg);

		nvgFillColor(args.vg, nvgRGB(15, 250, 15));
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, centerX + xPoint[0], centerY + xPoint[1], 1.5);
		nvgFill(args.vg);

		nvgFillColor(args.vg, nvgRGB(250, 250, 15));
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, centerX + yPoint[0], centerY + yPoint[1], 1.5);
		nvgFill(args.vg);

		nvgFillColor(args.vg, nvgRGB(15, 255, 250));
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, centerX + zPoint[0], centerY + zPoint[1], 1.5);
		nvgFill(args.vg);*/



		if (layer == 1) {
			reading = true;
			drawHistory(args.vg, module->xPointSamples, nvgRGBA(15, 250, 15, xInf*255), xhistory);
			drawHistory(args.vg, module->yPointSamples, nvgRGBA(250, 250, 15, yInf*255), yhistory);
			drawHistory(args.vg, module->zPointSamples, nvgRGBA(15, 250, 250, zInf*255), zhistory);
			reading = false;
		}

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
		backdrop->imagePath = asset::plugin(pluginInstance, "res/quatosc.jpg");
		backdrop->scalar = 3.5;
		backdrop->visible = true;

		display = new QuatDisplay();
		display->box.pos = Vec(2, 40);
        display->box.size = Vec(((MODULE_SIZE -1) * RACK_GRID_WIDTH) + 10, 125);
		display->module = module;
		
		setPanel(backdrop);
		addChild(display);

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		//addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(10, 90)), module, Treequencer::SEND_VOCT_X, Treequencer::SEND_VOCT_X_LIGHT));
		//addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(20, 90)), module, Treequencer::SEND_VOCT_Y, Treequencer::SEND_VOCT_Y_LIGHT));
		//addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(20, 90)), module, Treequencer::SEND_VOCT_Z, Treequencer::SEND_VOCT_Z_LIGHT));

		float hOff = 5.f;
		float start = 8;
		float next = 8.75;

		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(start, 60 + hOff)), module, QuatOSC::X_POS_I_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(start + (next*2), 60+ hOff)), module, QuatOSC::Y_POS_I_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(start + (next*4), 60+ hOff)), module, QuatOSC::Z_POS_I_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(start + next, 60+ hOff)), module, QuatOSC::X_POS_I_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(start + (next*3), 60+ hOff)), module, QuatOSC::Y_POS_I_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(start + (next*5), 60+ hOff)), module, QuatOSC::Z_POS_I_INPUT));

		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(start, 70+ hOff)), module, QuatOSC::X_FLO_F_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(start + (next*2), 70+ hOff)), module, QuatOSC::Y_FLO_F_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(start + (next*4), 70+ hOff)), module, QuatOSC::Z_FLO_F_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(start + next, 70+ hOff)), module, QuatOSC::X_FLO_F_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(start + (next*3), 70+ hOff)), module, QuatOSC::Y_FLO_F_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(start + (next*5), 70+ hOff)), module, QuatOSC::Z_FLO_F_INPUT));

		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(start, 80+ hOff)), module, QuatOSC::VOCT1_OCT));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(start + (next*2), 80+ hOff)), module, QuatOSC::VOCT2_OCT));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(start + (next*4), 80+ hOff)), module, QuatOSC::VOCT3_OCT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(start + next, 80+ hOff)), module, QuatOSC::VOCT1_OCT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(start + (next*3), 80+ hOff)), module, QuatOSC::VOCT2_OCT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(start + (next*5), 80+ hOff)), module, QuatOSC::VOCT3_OCT_INPUT));

		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(start, 90+ hOff)), module, QuatOSC::X_FLO_I_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(start + (next*2), 90+ hOff)), module, QuatOSC::Y_FLO_I_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(start + (next*4), 90+ hOff)), module, QuatOSC::Z_FLO_I_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(start + next, 90+ hOff)), module, QuatOSC::X_FLO_I_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(start + (next*3), 90+ hOff)), module, QuatOSC::Y_FLO_I_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(start + (next*5), 90+ hOff)), module, QuatOSC::Z_FLO_I_INPUT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(start + (next*0.5), 100+ hOff)), module, QuatOSC::VOCT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(start + (next*2.5), 100+ hOff)), module, QuatOSC::VOCT2));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(start + (next*4.5), 100+ hOff)), module, QuatOSC::VOCT3));

		//addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(start + (next*3), 113)), module, QuatOSC::LEFT_OUT));
		//addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(start + (next*4), 113)), module, QuatOSC::RIGHT_OUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(start + (next*5), 115)), module, QuatOSC::MONO_OUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(start, 115)), module, QuatOSC::CLOCK_INPUT));

	}
};

Model* modelQuatOSC = createModel<QuatOSC, QuatOSCWidget>("quatosc");
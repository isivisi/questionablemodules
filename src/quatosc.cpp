#include "plugin.hpp"
#include "imagepanel.cpp"
#include "colorBG.hpp"
#include "questionableModule.hpp"
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
const int SAMPLES_PER_SECOND = 44100;
const float VECLENGTH = 65.f;

const float HALF_SEMITONE = 1.029302;

bool reading = false;

struct QuatOSC : QuestionableModule {
	enum ParamId {
		VOCT1_OCT,
		VOCT2_OCT,
		VOCT3_OCT,
		X_FLO_I_PARAM,
		Y_FLO_I_PARAM,
		Z_FLO_I_PARAM,
		X_FLO_ROT_PARAM,
		Y_FLO_ROT_PARAM,
		Z_FLO_ROT_PARAM,
		X_POS_I_PARAM,
		Y_POS_I_PARAM,
		Z_POS_I_PARAM,
		STEREO,
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
		OUT,
		OUT2,
		OUTPUTS_LEN
	};
	enum LightId {
		BLINK_LIGHT,
		STEREO_LIGHT,
		LIGHTS_LEN
	};

	std::unordered_map<std::string, gmtl::Vec3f> projectionPlanes = {
		{"X", gmtl::Vec3f{0.f, 1.f, 1.f}},
		{"Y", gmtl::Vec3f{1.f, 0.f, 1.f}},
		{"Z", gmtl::Vec3f{1.f, 1.f, 0.f}}
	};

	std::string projection = "Z";

    gmtl::Quatf sphereQuat;
	gmtl::Quatf rotationAccumulation;
    gmtl::Vec3f xPointOnSphere;
	gmtl::Vec3f yPointOnSphere;
	gmtl::Vec3f zPointOnSphere;

	dsp::SchmittTrigger gateTrigger;
	dsp::SchmittTrigger clockTrigger;
	dsp::Timer clockTimer;

	float clockFreq = 2.f;
	
	// when manipulating the lfos they will always be sligtly out of phase.
	// random phase to start for more consistant inconsitency :)
	float lfo1Phase = 0.8364f;
	float lfo2Phase = 0.435f;
	float lfo3Phase = 0.3234f;

	float freqHistory1;
	float freqHistory2;
	float freqHistory3;

	std::queue<gmtl::Vec3f> xPointSamples;
	std::queue<gmtl::Vec3f> yPointSamples;
	std::queue<gmtl::Vec3f> zPointSamples;

	bool oct1Connected = false;
	bool oct2Connected = false;
	bool oct3Connected = false;

	QuatOSC() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(X_FLO_I_PARAM, 0.f, 1.f, 0.f, "X LFO Influence");
		configParam(Y_FLO_I_PARAM, 0.f, 1.f, 1.f, "Y LFO Influence");
		configParam(Z_FLO_I_PARAM, 0.f, 1.f, 1.f, "Z LFO Influence");
		configParam(X_FLO_ROT_PARAM, 0.f, 100.f, 0.f, "X Rotation");
		configParam(Y_FLO_ROT_PARAM, 0.f, 100.f, 0.f, "Y Rotation");
		configParam(Z_FLO_ROT_PARAM, 0.f, 100.f, 0.f, "Z Rotation");
		configParam(X_POS_I_PARAM, 0.f, 1.f, 1.f, "X Position Influence");
		configParam(Y_POS_I_PARAM, 0.f, 1.f, 1.f, "Y Position Influence");
		configParam(Z_POS_I_PARAM, 0.f, 1.f, 1.f, "Z Position Influence");
		configSwitch(VOCT1_OCT, 0.f, 8.f, 0.f, "VOct 1 Octave", {"1", "2", "3", "4", "5", "6", "7", "8"});
		configSwitch(VOCT2_OCT, 0.f, 8.f, 6.f, "VOct 2 Octave", {"1", "2", "3", "4", "5", "6", "7", "8"});
		configSwitch(VOCT3_OCT, 0.f, 8.f, 0.f, "VOct 3 Octave", {"1", "2", "3", "4", "5", "6", "7", "8"});
		configSwitch(STEREO, 0.f, 2.f, 0.f, "Stereo", {"Mono", "Full Stereo", "Sides"});
		configInput(VOCT, "VOct");
		configInput(VOCT2, "VOct 2");
		configInput(VOCT3, "VOct 3");
		configInput(VOCT1_OCT_INPUT, "VOct 1 Octave");
		configInput(VOCT2_OCT_INPUT, "VOct 2 Octave");
		configInput(VOCT3_OCT_INPUT, "VOct 3 Octave");
		configInput(X_FLO_I_INPUT, "X LFO Influence");
		configInput(Y_FLO_I_INPUT, "Y LFO Influence");
		configInput(Z_FLO_I_INPUT, "Z LFO Influence");
		configInput(X_FLO_F_INPUT, "X LFO Rotation");
		configInput(Y_FLO_F_INPUT, "Y LFO Rotation");
		configInput(Z_FLO_F_INPUT, "Z LFO Rotation");
		configInput(X_POS_I_INPUT, "X Position Influence");
		configInput(Y_POS_I_INPUT, "Y Position Influence");
		configInput(Z_POS_I_INPUT, "Z Position Influence");
		configInput(CLOCK_INPUT, "Clock");
		//configOutput(LEFT_OUT, "Left");
		//configOutput(RIGHT_OUT, "Right");
		configOutput(OUT, "");
		configOutput(OUT2, "");
		configInput(TRIGGER, "Gate");

		xPointOnSphere = gmtl::Vec3f(VECLENGTH, 0.f, 0.f);
		yPointOnSphere = gmtl::Vec3f(0.f, VECLENGTH, 0.f);
		zPointOnSphere = gmtl::Vec3f(0.f, 0.f, VECLENGTH);
		
	}

	float fclamp(float min, float max, float value) {
		return std::min(max, std::max(min, value));
	}

	float getValue(int value, bool c=false) {
		if (c) return fclamp(0.f, 1.f, params[value].getValue() + inputs[value + VOCT1_OCT_INPUT].getVoltage());
		else return params[value].getValue() + inputs[value + VOCT1_OCT_INPUT].getVoltage();
	}

	inline float VecCombine(gmtl::Vec3f vector) {
		gmtl::Vec3f proj = projectionPlanes[projection];
		return (vector[0]*proj[0]) + (vector[1]*proj[1]) + (vector[2]*proj[2]);
	}

	inline float calcVOctFreq(int input) {
		return HALF_SEMITONE * (clockFreq / 2.f) * dsp::exp2_taylor5((inputs[input].getVoltage() + std::round(getValue(input))) + 30.f) / std::pow(2.f, 30.f);
	}

	float processLFO(float &phase, float frequency, float deltaTime, float &freqHistory, int voct = -1) {

		float voctFreq = calcVOctFreq(voct);

		if (fabs(voctFreq - freqHistory) > 0.1) {
			resetPhase(); 
			freqHistory = voctFreq;
		}

		phase += ((frequency) + (voct != -1 ? voctFreq : 0.f)) * deltaTime;
		phase -= trunc(phase);

		return sin(2.f * M_PI * phase);
	}

	void resetPhase(bool resetLfo=false) {
		sphereQuat = gmtl::Quatf(0,0,0,1);
		if (resetLfo) {
			lfo1Phase = 0.8364f;
			lfo2Phase = 0.435f;
			lfo3Phase = 0.3234f;
		}
	}

	float smoothDephase(float offset, float phase, float sampleTime) {
		float phaseError = std::asin(phase) - std::asin(offset);
		if (phaseError > M_PI) phaseError -= 2*M_PI;
		else if (phaseError < -M_PI) phaseError += 2*M_PI;
		return lerp<float>(phase, phase - (phaseError), sampleTime);
	}

	void process(const ProcessArgs& args) override {

		if (oct1Connected != inputs[VOCT].isConnected()) {
			oct1Connected = inputs[VOCT].isConnected();
			resetPhase(true);
		}
		if (oct2Connected != inputs[VOCT2].isConnected()) {
			oct2Connected = inputs[VOCT2].isConnected();
			resetPhase(true);
		}
		if (oct3Connected != inputs[VOCT3].isConnected()) {
			oct3Connected = inputs[VOCT3].isConnected();
			resetPhase(true);
		}

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

		gmtl::Quatf rotOffset = gmtl::makePure(gmtl::Vec3f(
			getValue(X_FLO_I_PARAM, true)  * ((processLFO(lfo1Phase, 0.f, args.sampleTime, freqHistory1, VOCT))), 
			getValue(Y_FLO_I_PARAM, true)  * ((processLFO(lfo2Phase, 0.f, args.sampleTime, freqHistory2, VOCT2))), 
			getValue(Z_FLO_I_PARAM, true)  * ((processLFO(lfo3Phase, 0.f, args.sampleTime, freqHistory3, VOCT3)))
		));
		gmtl::normalize(rotOffset);

		gmtl::Quatf rotAddition = gmtl::makePure(gmtl::Vec3f(
			getValue(X_FLO_ROT_PARAM) * args.sampleTime, 
			getValue(Y_FLO_ROT_PARAM)* args.sampleTime, 
			getValue(Z_FLO_ROT_PARAM)* args.sampleTime
		));
		rotationAccumulation += rotAddition * rotationAccumulation;

		sphereQuat = rotationAccumulation * rotOffset;

		gmtl::lerp(rotationAccumulation, args.sampleTime, rotationAccumulation, gmtl::Quatf(0,0,0,1)); // smooth dephase, less clicking :)

		gmtl::normalize(sphereQuat);

		gmtl::Vec3f xRotated = sphereQuat * xPointOnSphere;
		gmtl::Vec3f yRotated = sphereQuat * yPointOnSphere;
		gmtl::Vec3f zRotated = sphereQuat * zPointOnSphere;

		if ((args.sampleRate >= SAMPLES_PER_SECOND && (args.frame % (int)(args.sampleRate/SAMPLES_PER_SECOND) == 0)) && !reading) {
			xPointSamples.push(xRotated);
			yPointSamples.push(yRotated);
			zPointSamples.push(zRotated);
		}

		gmtl::normalize(xRotated);
		gmtl::normalize(yRotated);
		gmtl::normalize(zRotated);

		//dephase
		lfo1Phase = smoothDephase(0, lfo1Phase, args.sampleTime);
		lfo2Phase = smoothDephase(0, lfo2Phase, args.sampleTime);
		lfo3Phase = smoothDephase(0, lfo3Phase, args.sampleTime);

		if (params[STEREO].getValue() >= 1) {

			//outputs[OUT].setChannels(2);
			float stereo[2] = {0.0f, 0.0f};

			// add each sides influence
			stereo[1] += fclamp(0, 1, xRotated[0]) * (VecCombine(xRotated) * getValue(X_POS_I_PARAM, true));
			stereo[1] += fclamp(0, 1, yRotated[0]) * (VecCombine(yRotated) * getValue(Y_POS_I_PARAM, true));
			stereo[1] += fclamp(0, 1, zRotated[0]) * (VecCombine(zRotated) * getValue(Z_POS_I_PARAM, true));

			stereo[0] += fclamp(-1, 0, xRotated[0]) * (VecCombine(xRotated) * getValue(X_POS_I_PARAM, true));
			stereo[0] += fclamp(-1, 0, yRotated[0]) * (VecCombine(yRotated) * getValue(Y_POS_I_PARAM, true));
			stereo[0] += fclamp(-1, 0, zRotated[0]) * (VecCombine(zRotated) * getValue(Z_POS_I_PARAM, true));

			if (params[STEREO].getValue() == 1) {
				// add center influence
				for (size_t i = 0; i < 2; i++) {
					stereo[i] += (1-fclamp(0, 1, abs(xRotated[0]))) * (VecCombine(xRotated) * getValue(X_POS_I_PARAM, true));
					stereo[i] += (1-fclamp(0, 1, abs(yRotated[0]))) * (VecCombine(yRotated) * getValue(Y_POS_I_PARAM, true));
					stereo[i] += (1-fclamp(0, 1, abs(zRotated[0]))) * (VecCombine(zRotated) * getValue(Z_POS_I_PARAM, true));
				}
			}

			outputs[OUT].setVoltage(stereo[0]);
			outputs[OUT2].setVoltage(stereo[1]);

		} else {

			//outputs[OUT].setChannels(1);

			outputs[OUT].setVoltage((
				((VecCombine(xRotated) * getValue(X_POS_I_PARAM, true)) + 
				(VecCombine(yRotated) * getValue(Y_POS_I_PARAM, true)) + 
				(VecCombine(zRotated) * getValue(Z_POS_I_PARAM, true)))
			));
			outputs[OUT2].setVoltage(outputs[OUT].getVoltage());
		
		}


	}

	json_t* dataToJson() override {
		json_t* nodeJ = QuestionableModule::dataToJson();
		json_object_set_new(nodeJ, "projection", json_string(projection.c_str()));
		json_object_set_new(nodeJ, "clockFreq", json_real(clockFreq));
		return nodeJ;
	}

	void dataFromJson(json_t* rootJ) override {
		QuestionableModule::dataFromJson(rootJ);
		
		if (json_t* p = json_object_get(rootJ, "projection")) projection = json_string_value(p);
		if (json_t* cf = json_object_get(rootJ, "clockFreq")) clockFreq = json_real_value(cf);
		
		resetPhase(true);
	}

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

	std::unordered_map<std::string, gmtl::Quatf> projRot = {
		{"X", {0.f, 0.7071067, 0.f, 0.7071069}},
		{"Y", {0.7071067, 0.f, 0.f, 0.7071069}},
		{"Z", {0.f, 0.f, 0.f, 1.f}},
	};

	void addToHistory(gmtl::Vec3f& vec, vecHistory& h) {
		h.history[h.cursor] = vec;
		h.cursor = (h.cursor + 1) % MAX_HISTORY;
	}

	void drawHistory(NVGcontext* vg, std::queue<gmtl::Vec3f> &history, NVGcolor color, vecHistory& localHistory) {
		float centerX = box.size.x/2;
		float centerY = box.size.y/2;
		bool f = true;

		gmtl::Quatf rot = projRot[((QuatOSC*)module)->projection];

		// grab points from audio thread and clear and add it to our own history list
		while (history.size() > 1) {
			addToHistory(history.front(), localHistory);
			history.pop();
		}

		// Iterate from oldest history value to latest
		nvgBeginPath(vg);
		for (int i = (localHistory.cursor+1)%MAX_HISTORY; i != localHistory.cursor; i = (i+1)%MAX_HISTORY) {
			gmtl::Vec3f point = rot * localHistory.history[i];
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

		if (module == NULL) return;

		float xInf = module->getValue(QuatOSC::X_POS_I_PARAM, true);
		float yInf = module->getValue(QuatOSC::Y_POS_I_PARAM, true);
		float zInf = module->getValue(QuatOSC::Z_POS_I_PARAM, true);

		nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
		nvgStrokeWidth(args.vg, 1.f);

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

// our three way switch for mono, stereo, and stereo - middle
struct SLURPStereoSwitch : QuestionableParam<SvgSwitch> {
	bool allowDraw = false;

	SLURPStereoSwitch() {
		QuestionableParam();

		fb->removeChild(shadow); // we don't need this

		momentary = false;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/slurpMono.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/slurpFullStereo.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/slurpSides.svg")));
	}

	// force draw on light layer
	void drawLayer(const DrawArgs &args, int layer) override {
		if (layer == 1) {
			allowDraw = true;
			draw(args);
			allowDraw = false;
		}
	}

	void draw(const DrawArgs &args) override {
		if (allowDraw) SvgSwitch::draw(args);
	}

};

struct QuatOSCWidget : QuestionableWidget {
	ImagePanel *fade;
	QuatDisplay *display;

	void setText() {
		NVGcolor c = nvgRGB(255,255,255);

		color->textList.clear();
		color->addText("SLURP OSC", "OpenSans-ExtraBold.ttf", c, 24, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, 21));
		color->addText("·ISI·", "OpenSans-ExtraBold.ttf", c, 28, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, RACK_GRID_HEIGHT-13));

		color->addText("X VOCT", "OpenSans-Bold.ttf", c, 6, Vec(37, 209.5), "descriptor");
		color->addText("Y VOCT", "OpenSans-Bold.ttf", c, 6, Vec(90, 209.5), "descriptor");
		color->addText("Z VOCT", "OpenSans-Bold.ttf", c, 6, Vec(144, 209.5), "descriptor");

		color->addText("INFLUENCE", "OpenSans-Bold.ttf", c, 6, Vec(37, 238.5), "descriptor");
		color->addText("INFLUENCE", "OpenSans-Bold.ttf", c, 6, Vec(37 + 53, 238.5), "descriptor");
		color->addText("INFLUENCE", "OpenSans-Bold.ttf", c, 6, Vec(37 + 106, 238.5), "descriptor");

		color->addText("ROTATION", "OpenSans-Bold.ttf", c, 6, Vec(37, 268), "descriptor");
		color->addText("ROTATION", "OpenSans-Bold.ttf", c, 6, Vec(37 + 53, 268), "descriptor");
		color->addText("ROTATION", "OpenSans-Bold.ttf", c, 6, Vec(37 + 106, 268), "descriptor");

		color->addText("OCTAVE", "OpenSans-Bold.ttf", c, 6, Vec(37, 297.5), "descriptor");
		color->addText("OCTAVE", "OpenSans-Bold.ttf", c, 6, Vec(37 + 53, 297.5), "descriptor");
		color->addText("OCTAVE", "OpenSans-Bold.ttf", c, 6, Vec(37 + 106, 297.5), "descriptor");

		color->addText("LFO INFLUENCE", "OpenSans-Bold.ttf", c, 6, Vec(37, 327), "descriptor");
		color->addText("LFO INFLUENCE", "OpenSans-Bold.ttf", c, 6, Vec(37 + 53, 327), "descriptor");
		color->addText("LFO INFLUENCE", "OpenSans-Bold.ttf", c, 6, Vec(37 + 106, 327), "descriptor");

		color->addText("CLOCK", "OpenSans-Bold.ttf", c, 6, Vec(24, 358), "descriptor");
		color->addText("OUT", "OpenSans-Bold.ttf", c, 6, Vec(143.3, 358), "descriptor");
	}

	QuatOSCWidget(QuatOSC* module) {
		setModule(module);
		//setPanel(createPanel(asset::plugin(pluginInstance, "res/nrandomizer.svg")));

		backdrop = new ImagePanel();
		backdrop->box.size = Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
		backdrop->imagePath = asset::plugin(pluginInstance, "res/quatosc.jpg");
		backdrop->scalar = 3.49;
		backdrop->visible = true;

		display = new QuatDisplay();
		display->box.pos = Vec(2, 40);
        display->box.size = Vec(((MODULE_SIZE -1) * RACK_GRID_WIDTH) + 10, 125);
		display->module = module;

		fade = new ImagePanel();
		fade->box.pos = Vec(6, 20);
		fade->box.size = Vec(((MODULE_SIZE -1) * RACK_GRID_WIDTH) + 10, ((MODULE_SIZE -1) * RACK_GRID_WIDTH) + 10);
		fade->imagePath = asset::plugin(pluginInstance, "res/circleFade.png");
		fade->borderColor = nvgRGBA(0,0,0,0);
		fade->scalar = 3;
		fade->opacity = 0.1;
		fade->visible = true;

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
		addChild(display);
		addChild(fade);

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// visuals links
		addChild(new QuestionableDrawWidget(Vec(25, 219), [module](const DrawArgs &args) {
			NVGcolor color = (module->theme == "Dark") ? nvgRGB(200, 200, 200) : nvgRGB(30, 30, 30);
			for (size_t i = 0; i < 3; i++) {
				for (size_t x = 0; x < 4; x++) {
					nvgFillColor(args.vg, color);
					nvgBeginPath(args.vg);
					nvgRoundedRect(args.vg, 55 * i, 29.6 * x, 20, 5, 3);
					nvgFill(args.vg);
				}
			}
		}));

		//addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(10, 90)), module, Treequencer::SEND_VOCT_X, Treequencer::SEND_VOCT_X_LIGHT));
		//addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(20, 90)), module, Treequencer::SEND_VOCT_Y, Treequencer::SEND_VOCT_Y_LIGHT));
		//addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(20, 90)), module, Treequencer::SEND_VOCT_Z, Treequencer::SEND_VOCT_Z_LIGHT));

		float hOff = 15.f;
		float start = 8;
		float next = 9;

		addParam(createParamCentered<QuestionableParam<RoundSmallBlackKnob>>(mm2px(Vec(start, 60 + hOff)), module, QuatOSC::X_POS_I_PARAM));
		addParam(createParamCentered<QuestionableParam<RoundSmallBlackKnob>>(mm2px(Vec(start + (next*2), 60+ hOff)), module, QuatOSC::Y_POS_I_PARAM));
		addParam(createParamCentered<QuestionableParam<RoundSmallBlackKnob>>(mm2px(Vec(start + (next*4), 60+ hOff)), module, QuatOSC::Z_POS_I_PARAM));

		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(start + next, 60+ hOff)), module, QuatOSC::X_POS_I_INPUT));
		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(start + (next*3), 60+ hOff)), module, QuatOSC::Y_POS_I_INPUT));
		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(start + (next*5), 60+ hOff)), module, QuatOSC::Z_POS_I_INPUT));

		addParam(createParamCentered<QuestionableParam<RoundSmallBlackKnob>>(mm2px(Vec(start, 70+ hOff)), module, QuatOSC::X_FLO_ROT_PARAM));
		addParam(createParamCentered<QuestionableParam<RoundSmallBlackKnob>>(mm2px(Vec(start + (next*2), 70+ hOff)), module, QuatOSC::Y_FLO_ROT_PARAM));
		addParam(createParamCentered<QuestionableParam<RoundSmallBlackKnob>>(mm2px(Vec(start + (next*4), 70+ hOff)), module, QuatOSC::Z_FLO_ROT_PARAM));

		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(start + next, 70+ hOff)), module, QuatOSC::X_FLO_F_INPUT));
		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(start + (next*3), 70+ hOff)), module, QuatOSC::Y_FLO_F_INPUT));
		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(start + (next*5), 70+ hOff)), module, QuatOSC::Z_FLO_F_INPUT));

		addParam(createParamCentered<QuestionableParam<RoundSmallBlackKnob>>(mm2px(Vec(start, 80+ hOff)), module, QuatOSC::VOCT1_OCT));
		addParam(createParamCentered<QuestionableParam<RoundSmallBlackKnob>>(mm2px(Vec(start + (next*2), 80+ hOff)), module, QuatOSC::VOCT2_OCT));
		addParam(createParamCentered<QuestionableParam<RoundSmallBlackKnob>>(mm2px(Vec(start + (next*4), 80+ hOff)), module, QuatOSC::VOCT3_OCT));

		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(start + next, 80+ hOff)), module, QuatOSC::VOCT1_OCT_INPUT));
		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(start + (next*3), 80+ hOff)), module, QuatOSC::VOCT2_OCT_INPUT));
		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(start + (next*5), 80+ hOff)), module, QuatOSC::VOCT3_OCT_INPUT));

		addParam(createParamCentered<QuestionableParam<RoundSmallBlackKnob>>(mm2px(Vec(start, 90+ hOff)), module, QuatOSC::X_FLO_I_PARAM));
		addParam(createParamCentered<QuestionableParam<RoundSmallBlackKnob>>(mm2px(Vec(start + (next*2), 90+ hOff)), module, QuatOSC::Y_FLO_I_PARAM));
		addParam(createParamCentered<QuestionableParam<RoundSmallBlackKnob>>(mm2px(Vec(start + (next*4), 90+ hOff)), module, QuatOSC::Z_FLO_I_PARAM));

		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(start + next, 90+ hOff)), module, QuatOSC::X_FLO_I_INPUT));
		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(start + (next*3), 90+ hOff)), module, QuatOSC::Y_FLO_I_INPUT));
		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(start + (next*5), 90+ hOff)), module, QuatOSC::Z_FLO_I_INPUT));

		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(start + (next*0.5), 65)), module, QuatOSC::VOCT));
		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(start + (next*2.5), 65)), module, QuatOSC::VOCT2));
		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(start + (next*4.5), 65)), module, QuatOSC::VOCT3));

		addOutput(createOutputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(start + (next*4), 115)), module, QuatOSC::OUT));
		addOutput(createOutputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(start + (next*5), 115)), module, QuatOSC::OUT2));

		addParam(createParamCentered<SLURPStereoSwitch>(mm2px(Vec(start + (next*3), 115)), module, QuatOSC::STEREO));

		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(start, 115)), module, QuatOSC::CLOCK_INPUT));

	}

	void appendContextMenu(Menu *menu) override
  	{
		QuatOSC* mod = (QuatOSC*)module;
		menu->addChild(new MenuSeparator);
		menu->addChild(rack::createSubmenuItem("Projection Axis", "", [=](ui::Menu* menu) {
			menu->addChild(createMenuItem("X", "",[=]() { mod->projection = "X"; }));
			menu->addChild(createMenuItem("Y", "", [=]() { mod->projection = "Y"; }));
			menu->addChild(createMenuItem("Z", "", [=]() { mod->projection = "Z"; }));
		}));

		//menu->addChild(createMenuItem(mod->stereo ? "Disable Stereo" : "Enable Stereo", "",[=]() { mod->stereo = !mod->stereo; }));

		QuestionableWidget::appendContextMenu(menu);
	}
};

Model* modelQuatOSC = createModel<QuatOSC, QuatOSCWidget>("quatosc");
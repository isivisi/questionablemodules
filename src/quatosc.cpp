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
const int MAX_SPREAD = 16;
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
		SPREAD,
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
	
	enum Stereo {
		OFF,
		FULL,
		SIDES
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

	// logically linked to VOCT{N}_OCT param
	std::vector<bool> quantizedVOCT {true,true,true};
	bool normalizeSpreadVolume = true;

	float clockFreq = 2.f;
	
	// when manipulating the lfos they will always be sligtly out of phase.
	// random phase to start for more consistant inconsitency :)
	float lfo1Phase = 0.8364f;
	float lfo2Phase = 0.435f;
	float lfo3Phase = 0.3234f;

	// statically allocated queue
	// the less allocation going in the audio thread the better
	template <typename T, const size_t allocation_size=SAMPLES_PER_SECOND*2> // around 1mb for 16 voices
	struct SLURPQueue {
		T* values;
		size_t cursor_front = 0;
		size_t cursor_back = 0;
		size_t s = 0;
		SLURPQueue() { values = new T[allocation_size]; } // not limited by stack
		~SLURPQueue() { delete [] values; }
		T pop() {
			if (s > 0) {
				size_t grab = cursor_back;
				cursor_back = (cursor_back+1) % allocation_size;
				s -= 1;
				return values[grab];
			}
			return T();
		}
		void push(T obj) {
			if (s >= allocation_size) return; // not important for us
			cursor_front = (cursor_front+1) % allocation_size;
			s += 1;
			values[cursor_front] = obj;
		}
		size_t size() { return s; }
	};
	
	struct pointSampleGroup {
		SLURPQueue<gmtl::Vec3f> x;
		SLURPQueue<gmtl::Vec3f> y;
		SLURPQueue<gmtl::Vec3f> z;
	};
	pointSampleGroup pointSamples[MAX_SPREAD];

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
		configSwitch(SPREAD, 1.f, 16.f, 1.f, "Spread", {"Off", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16"});
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

		supportsSampleRateOverride = true;

		xPointOnSphere = gmtl::Vec3f(VECLENGTH, 0.f, 0.f);
		yPointOnSphere = gmtl::Vec3f(0.f, VECLENGTH, 0.f);
		zPointOnSphere = gmtl::Vec3f(0.f, 0.f, VECLENGTH);
		
	}

	inline float fclamp(float min, float max, float value) {
		return std::min(max, std::max(min, value));
	}

	inline float getValue(int value, bool c=false) {
		if (c) return fclamp(0.f, 1.f, params[value].getValue() + inputs[value + VOCT1_OCT_INPUT].getVoltage());
		else return params[value].getValue() + inputs[value + VOCT1_OCT_INPUT].getVoltage();
	}

	inline int getSpread() {
		return (int)params[SPREAD].getValue();
	}

	// Covert a 3d point in space to a 1d high value on a plane.
	inline float VecCombine(gmtl::Vec3f vector) {
		gmtl::Vec3f proj = projectionPlanes[projection];
		return (vector[0]*proj[0]) + (vector[1]*proj[1]) + (vector[2]*proj[2]);
	}

	inline float calcVOctFreq(int input) {
		float voctOffset = quantizedVOCT[input] ? std::round(getValue(input)) : getValue(input);
		return HALF_SEMITONE * (clockFreq / 2.f) * dsp::approxExp2_taylor5((inputs[input].getVoltage() + voctOffset) + 30.f) / std::pow(2.f, 30.f);
	}

	inline float processLFO(float &phase, float frequency, float deltaTime, int voct = -1) {

		float voctFreq = calcVOctFreq(voct);

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

	inline float smoothDephase(float offset, float phase, float sampleTime) {
		float phaseError = std::asin(phase) - std::asin(offset);
		if (phaseError > M_PI) phaseError -= 2*M_PI;
		else if (phaseError < -M_PI) phaseError += 2*M_PI;
		return lerp<float>(phase, phase - (phaseError), sampleTime);
	}

	// Convert a 3 point set of 3d positions to stereo data
	// TODO: can be optimized?
	inline std::vector<float> pointToStereo(gmtl::Vec3f* points) {
		std::vector<float> stereo = {0.0f, 0.0f};

		for (size_t i = 0; i < 3; i++) {
			float vecProjected = VecCombine(points[i]);
			// add each sides influence
			// TODO: double check to see if this changes when projected differently. it shoud.
			stereo[0] += fclamp(-1, 0, points[i][0]) * (vecProjected * getValue(X_POS_I_PARAM+i, true));
			stereo[1] += fclamp(0, 1, points[i][0]) * (vecProjected * getValue(X_POS_I_PARAM+i, true));

			if (params[STEREO].getValue() == Stereo::FULL) {
				// add center influence
				for (size_t x = 0; x < 2; x++) {
					stereo[x] += (1-fclamp(0, 1, abs(points[i][0]))) * (vecProjected * getValue(X_POS_I_PARAM+i, true));
				}
			}
		}

		return stereo;
	}

	void processUndersampled(const ProcessArgs& args) override {

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

		// clock stuff from lfo
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

		// quat rotation from lfos
		gmtl::Quatf rotOffset = gmtl::makePure(gmtl::Vec3f(
			getValue(X_FLO_I_PARAM, true)  * ((processLFO(lfo1Phase, 0.f, args.sampleTime, VOCT))), 
			getValue(Y_FLO_I_PARAM, true)  * ((processLFO(lfo2Phase, 0.f, args.sampleTime, VOCT2))), 
			getValue(Z_FLO_I_PARAM, true)  * ((processLFO(lfo3Phase, 0.f, args.sampleTime, VOCT3)))
		));
		gmtl::normalize(rotOffset);

		// quat constant rotation addition
		gmtl::Quatf rotAddition = gmtl::makePure(gmtl::Vec3f(
			getValue(X_FLO_ROT_PARAM) * args.sampleTime, 
			getValue(Y_FLO_ROT_PARAM) * args.sampleTime, 
			getValue(Z_FLO_ROT_PARAM) * args.sampleTime
		));
		rotationAccumulation += rotAddition * rotationAccumulation;

		sphereQuat = rotationAccumulation * rotOffset; // add our lfo rotation to our accumulated rotation

		gmtl::lerp(rotationAccumulation, args.sampleTime, rotationAccumulation, gmtl::Quatf(0,0,0,1)); // smooth dephase, less clicking :)

		gmtl::normalize(sphereQuat);

		//dephase
		lfo1Phase = smoothDephase(0, lfo1Phase, args.sampleTime);
		lfo2Phase = smoothDephase(0, lfo2Phase, args.sampleTime);
		lfo3Phase = smoothDephase(0, lfo3Phase, args.sampleTime);

		// spread polyphonic logic
		int spread = getSpread();
		outputs[OUT].setChannels(spread);
		outputs[OUT2].setChannels(spread);
		float volDec = normalizeSpreadVolume ? (spread>1 ? 2*math::log2(spread) : 1) : 1;
		for (int i = 0; i < spread; i++) {
			gmtl::Quatf offsetRot = gmtl::Quatf();

			if (spread%2) gmtl::set(offsetRot, gmtl::EulerAngleXYZf((i-spread/2)*(M_PI/spread), (i-spread/2)*(M_PI/spread), (i-spread/2)*(M_PI/spread)));
			else gmtl::set(offsetRot, gmtl::EulerAngleXYZf((i-spread/M_PI)*(M_PI/spread), (i-spread/M_PI)*(M_PI/spread), (i-spread/M_PI)*(M_PI/spread)));

			offsetRot = sphereQuat * offsetRot;
			gmtl::normalize(offsetRot);
				
			gmtl::Vec3f newX = offsetRot * xPointOnSphere;
			gmtl::Vec3f newY = offsetRot * yPointOnSphere;
			gmtl::Vec3f newZ = offsetRot * zPointOnSphere;
			if ((args.sampleRate >= SAMPLES_PER_SECOND && (args.frame % (int)(args.sampleRate/SAMPLES_PER_SECOND) == 0)) && !reading) {
				pointSamples[i].x.push(newX);
				pointSamples[i].y.push(newY);
				pointSamples[i].z.push(newZ);
			}
			gmtl::normalize(newX); gmtl::normalize(newY); gmtl::normalize(newZ);
			gmtl::Vec3f points[3] = {newX, newY, newZ};
			if (params[STEREO].getValue() != Stereo::OFF) {
				std::vector<float> sStereo = pointToStereo(points);
				outputs[OUT].setVoltage(sStereo[0] / volDec, i);
				outputs[OUT2].setVoltage(sStereo[1] / volDec, i);
			} else {
				outputs[OUT].setVoltage((
					((VecCombine(newX) * getValue(X_POS_I_PARAM, true)) + 
					(VecCombine(newY) * getValue(Y_POS_I_PARAM, true)) + 
					(VecCombine(newZ) * getValue(Z_POS_I_PARAM, true))) / volDec
				), i);
				outputs[OUT2].setVoltage(outputs[OUT].getVoltage(i), i);
			}
		}
	}

	json_t* dataToJson() override {
		json_t* nodeJ = QuestionableModule::dataToJson();
		json_object_set_new(nodeJ, "projection", json_string(projection.c_str()));
		json_object_set_new(nodeJ, "clockFreq", json_real(clockFreq));
		json_object_set_new(nodeJ, "normalizeSpreadVolume", json_boolean(normalizeSpreadVolume));

		json_t* qtArray = json_array();
		for (size_t i = 0; i < quantizedVOCT.size(); i++) json_array_append_new(qtArray, json_boolean(quantizedVOCT[i]));
		json_object_set_new(nodeJ, "quantizedVOCT", qtArray);

		return nodeJ;
	}

	void dataFromJson(json_t* rootJ) override {
		QuestionableModule::dataFromJson(rootJ);
		
		if (json_t* p = json_object_get(rootJ, "projection")) projection = json_string_value(p);
		if (json_t* cf = json_object_get(rootJ, "clockFreq")) clockFreq = json_real_value(cf);
		if (json_t* nsv = json_object_get(rootJ, "normalizeSpreadVolume")) normalizeSpreadVolume = nsv;
		if (json_t* qtArray = json_object_get(rootJ, "quantizedVOCT")) {
			for (size_t i = 0; i < quantizedVOCT.size(); i++) { quantizedVOCT[i] = json_boolean_value(json_array_get(qtArray, i)); }
		}

	}

	void fromJson(json_t* rootJ) override {
		// reset
		projection = "Z";
		normalizeSpreadVolume = true;
		quantizedVOCT = {true,true,true};
		QuestionableModule::fromJson(rootJ);
		// reset phase on preset load even if data attribute not found
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

	struct channelHistory {
		vecHistory x;
		vecHistory y;
		vecHistory z;
	};
	channelHistory history[MAX_SPREAD];

	std::unordered_map<std::string, gmtl::Quatf> projRot = {
		{"X", {0.f, 0.7071067, 0.f, 0.7071069}},
		{"Y", {0.7071067, 0.f, 0.f, 0.7071069}},
		{"Z", {0.f, 0.f, 0.f, 1.f}},
	};

	void addToHistory(gmtl::Vec3f vec, vecHistory& h) {
		h.history[h.cursor] = vec;
		h.cursor = (h.cursor + 1) % MAX_HISTORY;
	}

	void drawHistory(NVGcontext* vg, QuatOSC::SLURPQueue<gmtl::Vec3f> &history, NVGcolor color, vecHistory& localHistory) {
		float centerX = box.size.x/2;
		float centerY = box.size.y/2;
		bool f = true;

		gmtl::Quatf rot = (module) ? projRot[((QuatOSC*)module)->projection] : projRot["Z"];

		// grab points from audio thread and clear and add it to our own history list
		while (history.size() > 1) {
			addToHistory(history.pop(), localHistory);
		}

		// Iterate from oldest history value to latest
		nvgBeginPath(vg);
		for (int i = (localHistory.cursor+1)%MAX_HISTORY; i != localHistory.cursor; i = (i+1)%MAX_HISTORY) {
			gmtl::Vec3f point = rot * localHistory.history[i];

			/*if (module && module->params[QuatOSC::STEREO].getValue() != QuatOSC::Stereo::OFF) {
				color.r = clamp<float>(0, 1, color.r + (point[0] / (VECLENGTH)) * 0.003);
				color.g = clamp<float>(0, 1, color.r + (point[0] / (VECLENGTH)) * 0.003);
				color.b = clamp<float>(0, 1, color.r + (point[0] / (VECLENGTH)) * 0.003);
			}
			nvgStrokeColor(vg, color);*/
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

		nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
		nvgStrokeWidth(args.vg, 1.f);

		if (module == NULL) {
			// draw example visual
			QuatOSC::SLURPQueue<gmtl::Vec3f> fakeHistory;
			fakeHistory.push(gmtl::Vec3f(0, VECLENGTH, 0));
			fakeHistory.push(gmtl::Vec3f(0, -VECLENGTH, 0));
			fakeHistory.push(gmtl::Vec3f(0, -VECLENGTH, 0));
			fakeHistory.push(gmtl::Vec3f(0, VECLENGTH, 0));
			drawHistory(args.vg, fakeHistory, nvgRGBA(15, 250, 250, 255), history[0].z);
			nvgRestore(args.vg);
			return;
		};

		float xInf = module->getValue(QuatOSC::X_POS_I_PARAM, true);
		float yInf = module->getValue(QuatOSC::Y_POS_I_PARAM, true);
		float zInf = module->getValue(QuatOSC::Z_POS_I_PARAM, true);

		if (layer == 1) {
			reading = true;
			for (size_t i = 0; i < module->getSpread(); i++) {
				drawHistory(args.vg, module->pointSamples[i].x, nvgRGBA(15, 250, 15, xInf*255), history[i].x);
				drawHistory(args.vg, module->pointSamples[i].y, nvgRGBA(250, 250, 15, yInf*255), history[i].y);
				drawHistory(args.vg, module->pointSamples[i].z, nvgRGBA(15, 250, 250, zInf*255), history[i].z);
			}
			reading = false;
		}

		nvgRestore(args.vg);
	}

};

// our three way switch for mono, stereo, and stereo - middle
struct SLURPStereoSwitch : QuestionableLightSwitch {
	SLURPStereoSwitch() {
		QuestionableLightSwitch();
		momentary = false;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/slurpMono.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/slurpFullStereo.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/slurpSides.svg")));
	}
};

struct SLURPSpreadSwitch  : QuestionableLightSwitch {
	SLURPSpreadSwitch() {
		QuestionableLightSwitch();
		momentary = false;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/slurpSpreadOff.svg")));
		for (size_t i = 0; i < 5; i++) addFrame(Svg::load(asset::plugin(pluginInstance, "res/slurpSpreadOn1.svg")));
		for (size_t i = 0; i < 5; i++) addFrame(Svg::load(asset::plugin(pluginInstance, "res/slurpSpreadOn2.svg")));
		for (size_t i = 0; i < 5; i++) addFrame(Svg::load(asset::plugin(pluginInstance, "res/slurpSpreadOn.svg")));
	}
};

// Adds option to disable quantization for the voct offset knob
template <typename T>
struct SLURPOCTParamWidget : QuestionableParam<T> {
	void appendContextMenu(Menu* menu) override {
		if (!this->module) return;
		QuatOSC* quatMod = (QuatOSC*)this->module;
		menu->addChild(createMenuItem(quatMod->quantizedVOCT[this->paramId] ? "Disable Quantization" : "Enable Quantization", "", [=]() {
			quatMod->quantizedVOCT[this->paramId] = !quatMod->quantizedVOCT[this->paramId];
		}));
		QuestionableParam<T>::appendContextMenu(menu);
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
		color->addText("OUTS", "OpenSans-Bold.ttf", c, 6, Vec(143.3, 358), "descriptor");
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
		setText();

		backgroundColorLogic(module);

		setPanel(backdrop);
		addChild(color);
		addChild(display);
		addChild(fade);

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// visuals links
		addChild(new QuestionableDrawWidget(Vec(25, 220), [module](const DrawArgs &args) {
			std::string theme = (module) ? module->theme : "";
			NVGcolor color = (theme == "Dark" || theme == "") ? nvgRGBA(250, 250, 250, 200) : nvgRGBA(30, 30, 30, 200);
			for (size_t i = 0; i < 3; i++) {
				for (size_t x = 0; x < 4; x++) {
					nvgFillColor(args.vg, color);
					nvgBeginPath(args.vg);
					nvgRoundedRect(args.vg, 55 * i, 29.6 * x, 20, 3, 3);
					nvgFill(args.vg);
				}
			}

			if (!module || (module && module->params[QuatOSC::STEREO].getValue() < 1)) {
					nvgFillColor(args.vg, color);
					nvgBeginPath(args.vg);
					nvgRoundedRect(args.vg, 55 * 2, 29.6 * 4, 20, 3, 3);
					nvgFill(args.vg);
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

		addParam(createParamCentered<SLURPOCTParamWidget<RoundSmallBlackKnob>>(mm2px(Vec(start, 80+ hOff)), module, QuatOSC::VOCT1_OCT));
		addParam(createParamCentered<SLURPOCTParamWidget<RoundSmallBlackKnob>>(mm2px(Vec(start + (next*2), 80+ hOff)), module, QuatOSC::VOCT2_OCT));
		addParam(createParamCentered<SLURPOCTParamWidget<RoundSmallBlackKnob>>(mm2px(Vec(start + (next*4), 80+ hOff)), module, QuatOSC::VOCT3_OCT));

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
		addParam(createParamCentered<SLURPSpreadSwitch>(mm2px(Vec(start + (next*2), 115)), module, QuatOSC::SPREAD));

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
		menu->addChild(createMenuItem(mod->normalizeSpreadVolume ? "Disable Spread Volume Normalization" : "Enable Spread Volume Normalization", "",[=]() { mod->normalizeSpreadVolume = !mod->normalizeSpreadVolume; }));

		QuestionableWidget::appendContextMenu(menu);
	}
};

Model* modelQuatOSC = createModel<QuatOSC, QuatOSCWidget>("quatosc");
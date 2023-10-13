#include "plugin.hpp"
#include "imagepanel.cpp"
#include "colorBG.hpp"
#include "questionableModule.hpp"
#include <vector>
#include <algorithm>

const int MODULE_SIZE = 8;

struct SyncMute : QuestionableModule {
	enum ParamId {
		MUTE,
		MUTE1,
		MUTE2,
		MUTE3,
		MUTE4,
		MUTE5,
		MUTE6,
		MUTE7,
		MUTE8,
		TIME_SIG,
		TIME_SIG2,
		TIME_SIG3,
		TIME_SIG4,
		TIME_SIG5,
		TIME_SIG6,
		TIME_SIG7,
		TIME_SIG8,
		PARAMS_LEN
	};
	enum InputId {
		IN,
		IN2,
		IN3,
		IN4,
		IN5,
		IN6,
		IN7,
		IN8,
		CLOCK,
		RESET,
		INPUTS_LEN
	};
	enum OutputId {
		OUT,
		OUT2,
		OUT3,
		OUT4,
		OUT5,
		OUT6,
		OUT7,
		OUT8,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	bool shouldSwap[8] = {false};
	bool muteState[8] = {false};

	dsp::SchmittTrigger resetTrigger;
	dsp::SchmittTrigger clockTrigger;
	dsp::Timer clockTimer;
	float clockTime = 0.5f; // in seconds

	float accumulatedTime[8] = {0.f};

	std::vector<std::string> sigsStrings = {"/32", "/31", "/30", "/29", "/28", "/27", "/26", "/25", "/24", "/23", "/22", "/21", "/20", "/19", "/18", "/17", "/16", "/15", "/14", "/13", "/12", "/11", "/10", "/9", "/8", "/7", "/6", "/5", "/4", "/3", "/2", "Immediate", "X2", "X3", "X4", "X5", "X6", "X7", "X8", "X9", "X10", "X11", "X12", "X13", "X14", "X15", "X16", "X17", "X18", "X19", "X20", "X21", "X22", "X23", "X24", "X25", "X26", "X27", "X28", "X29", "X30", "X31", "X32"};

	SyncMute() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		//configSwitch(RANGE_PARAM, 1.f, 8.f, 1.f, "Range", {"1", "2", "3", "4", "5", "6", "7", "8"});
		configSwitch(TIME_SIG, -31.f, 31.f, 0.f, "Time Signature 1", sigsStrings);
		configSwitch(TIME_SIG2, -31.f, 31.f, 0.f, "Time Signature 2", sigsStrings);
		configSwitch(TIME_SIG3, -31.f, 31.f, 0.f, "Time Signature 3", sigsStrings);
		configSwitch(TIME_SIG4, -31.f, 31.f, 0.f, "Time Signature 4", sigsStrings);
		configSwitch(TIME_SIG5, -31.f, 31.f, 0.f, "Time Signature 5", sigsStrings);
		configSwitch(TIME_SIG6, -31.f, 31.f, 0.f, "Time Signature 6", sigsStrings);
		configSwitch(TIME_SIG7, -31.f, 31.f, 0.f, "Time Signature 7", sigsStrings);
		configSwitch(TIME_SIG8, -31.f, 31.f, 0.f, "Time Signature 8", sigsStrings);
		configInput(IN, "1");
		configInput(IN2, "2");
		configInput(IN3, "3");
		configInput(IN4, "4");
		configInput(IN5, "5");
		configInput(IN6, "6");
		configInput(IN7, "7");
		configInput(IN8, "8");
		configOutput(OUT, "1");
		configOutput(OUT2, "2");
		configOutput(OUT3, "3");
		configOutput(OUT4, "4");
		configOutput(OUT5, "5");
		configOutput(OUT6, "6");
		configOutput(OUT7, "7");
		configOutput(OUT8, "8");
		configInput(CLOCK, "clock");
		configInput(RESET, "reset");
	}

	float timeSigs[8] = {0.f};
	dirtyable<bool> buttons[8] = {false};

	uint64_t clockTicksSinceReset = 0;
	float subClockTime = 0.f;

	void process(const ProcessArgs& args) override {
		bool resetClocks = resetTrigger.process(inputs[RESET].getVoltage(), 0.1f, 2.f);

		if (resetClocks) {
			clockTicksSinceReset = 0;
			subClockTime = 0.f;
		}

		for (size_t i = 0; i < 8; i++) {
			timeSigs[i] = params[TIME_SIG+i].getValue();
		}

		// clock stuff from lfo
		if (inputs[CLOCK].isConnected()) {
			clockTimer.process(args.sampleTime);
			if (clockTimer.getTime() > clockTime) clockTime = clockTimer.getTime();
			if (clockTrigger.process(inputs[CLOCK].getVoltage(), 0.1f, 2.f)) {
				clockTime = clockTimer.getTime();
				clockTimer.reset();
				clockTicksSinceReset += 1;
				subClockTime = 0;
			}
		} else { // 0.5f clock
			clockTime = 0.5f;
			if (subClockTime >= 0.5f) {
				clockTicksSinceReset += 1;
				subClockTime = 0.f;
			}
		}
		subClockTime += args.sampleTime;

		// clock resets and swap checks
		for (size_t i = 0; i < 8; i++) {
			bool clockHit = false;

			// on clock
			if (timeSigs[i] < 0.f) {
				float currentTime = clockTicksSinceReset % (int)abs(timeSigs[i]-1);
				if (currentTime < accumulatedTime[i]) clockHit = true;
				accumulatedTime[i] = currentTime;
			}
			if (timeSigs[i] > 0.f) {
				float currentTime = fmod(subClockTime / (clockTime/timeSigs[i]+1), 2);
				if (currentTime < accumulatedTime[i]) clockHit = true;
				accumulatedTime[i] = currentTime;
			}

			// edge cases
			if (resetClocks) clockHit = true; // on reset
			if (shouldSwap[i] && timeSigs[i] == 0.f) clockHit = true; // on immediate

			if (clockHit) accumulatedTime[i] = 0.f;

			if (shouldSwap[i] && clockHit) {
				muteState[i] = !muteState[i];
				shouldSwap[i] = false;
			}
		}

		// button checks
		for (size_t i = 0; i < 8; i++) {
			buttons[i] = params[MUTE+i].getValue();
			if (buttons[i].isDirty() && buttons[i] == true) shouldSwap[i] = !shouldSwap[i];
		}
		
		// outputs
		for (size_t i = 0; i < 8; i++) {
			outputs[OUT+i].setVoltage(muteState[i] ? 0.f : inputs[IN+i].getVoltage());
		}

	}

	json_t* dataToJson() override { 
		json_t* nodeJ = QuestionableModule::dataToJson();
		json_object_set_new(nodeJ, "clockTime", json_real(clockTime));

		json_t* muteArray = json_array();
		for (size_t i = 0; i < 8; i++) json_array_append_new(muteArray, json_boolean(muteState[i]));
		json_object_set_new(nodeJ, "muteState", muteArray);

		return nodeJ;
	}

	void dataFromJson(json_t* rootJ) override {
		QuestionableModule::dataFromJson(rootJ);
		if (json_t* ct = json_object_get(rootJ, "clockTime")) clockTime = json_real_value(ct);

		if (json_t* array = json_object_get(rootJ, "muteState")) { // assumes all 8 set
			for (size_t i = 0; i < 8; i++) { 
				muteState[i] = json_boolean_value(json_array_get(array, i)); 
			}
		}

	}

};

struct ClockKnob : RoundLargeBlackKnob {

	ClockKnob() {

	}

	void draw(const DrawArgs &args) override {
		SyncMute* mod = (SyncMute*)module;

		float anglePerTick = 31 / 1.65;

		float sig = mod ? mod->timeSigs[paramId - SyncMute::TIME_SIG] : 0.f;

		RoundLargeBlackKnob::draw(args);

		nvgSave(args.vg);

		nvgTranslate(args.vg, box.size.x/2, box.size.y/2);

		for (int i = -15; i < 16; i++) {
			nvgSave(args.vg);
			nvgRotate(args.vg, nvgDegToRad(i*(90.f/ (15 / 1.65))));
			nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, 0, 0);
			nvgLineTo(args.vg, 0, 4.5-box.size.y/2);
			nvgStrokeWidth(args.vg, 0.5);
			nvgStroke(args.vg);
			nvgRestore(args.vg);
		} // clockTime
		
		if (mod && sig < 0.f) nvgRotate(args.vg, nvgDegToRad(mod->clockTicksSinceReset %((int)abs(sig-1))*(-90.f/anglePerTick)));
		if (mod && sig > 0.f) nvgRotate(args.vg, nvgDegToRad(fmod((mod->subClockTime / (mod->clockTime/sig+1))*(sig+1), sig+1)*(90.f/anglePerTick)));
		
		nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 0, 0);
		nvgLineTo(args.vg, 0, 4.5-box.size.y/2);
		nvgStrokeWidth(args.vg, 3);
		nvgStroke(args.vg);

		nvgRestore(args.vg);
	
	}

};

struct MuteButton : Resizable<CKD6> {
	
	MuteButton() : Resizable(0.85, true) { }

	void drawLayer(const DrawArgs &args, int layer) override {
		if (!module) return;
		if (layer != 1) return;

		SyncMute* mod = (SyncMute*)module;
		int sig = mod->timeSigs[paramId];
		
		if (mod->muteState[paramId]) {
			nvgFillColor(args.vg, nvgRGB(255, 0, 25));
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, box.size.x/2, box.size.y/2, 10.f);
			nvgFill(args.vg);
		}

		if (mod->clockTime/32 < 0.05 && sig > 0.f) return; // no super fast flashing lights

		if (mod->shouldSwap[paramId] && (sig < 0.f ? mod->clockTicksSinceReset%2 : fmod((mod->subClockTime / (mod->clockTime/32)), 2)) < 0.5f) {
			nvgFillColor(args.vg, nvgRGB(0, 255, 25));
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, box.size.x/2, box.size.y/2, 10.f);
			nvgFill(args.vg);
		}
	}

};

struct SyncMuteWidget : QuestionableWidget {
	ColorBGSimple* bgSimple;
	void setText() {
		NVGcolor c = nvgRGB(255,255,255);
		color->textList.clear();
		color->addText("SMUTE", "OpenSans-ExtraBold.ttf", c, 24, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, 25));
		color->addText("·ISI·", "OpenSans-ExtraBold.ttf", c, 28, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, RACK_GRID_HEIGHT-13));

		color->addText("OUT", "OpenSans-Bold.ttf", c, 7, Vec(22, 353), "descriptor");
	}

	SyncMuteWidget(SyncMute* module) {
		setModule(module);

		bgSimple = new ColorBGSimple(Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT), nvgRGB(20,20,20));
		bgSimple->visible = true;

		color = new ColorBG(Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT));
		color->drawBackground = false;
		setText();

		backgroundColorLogic(module);
		
		setPanel(bgSimple);
		addChild(color);

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


		for (size_t i = 0; i < 8; i++) {
			addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(7.8, 16 + (13.2* i))), module, SyncMute::IN + i));
			addParam(createParamCentered<QuestionableParam<ClockKnob>>(mm2px(Vec(20.2, 16 + (13.2* i))), module, SyncMute::TIME_SIG + i));
			addParam(createParamCentered<QuestionableParam<MuteButton>>(mm2px(Vec(20.2, 16 + (13.2* i))), module, SyncMute::MUTE + i));
			addOutput(createOutputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(32.8, 16 + (13.2 * i))), module, SyncMute::OUT + i));
		}

		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(7.8, 119)), module, SyncMute::CLOCK));
		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(32.8, 119)), module, SyncMute::RESET));
	}

};

Model* modelSyncMute = createModel<SyncMute, SyncMuteWidget>("smute");
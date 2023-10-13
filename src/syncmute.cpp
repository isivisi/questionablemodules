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

	std::vector<std::string> sigsStrings = {"/16", "/15", "/14", "/13", "/12", "/11", "/10", "/9", "/8", "/7", "/6", "/5", "/4", "/3", "/2", "/1", "Immediate", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16"};

	SyncMute() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		//configSwitch(RANGE_PARAM, 1.f, 8.f, 1.f, "Range", {"1", "2", "3", "4", "5", "6", "7", "8"});
		configSwitch(TIME_SIG, -16.f, 16.f, 0.f, "Time Signature 1", sigsStrings);
		configSwitch(TIME_SIG2, -16.f, 16.f, 0.f, "Time Signature 2", sigsStrings);
		configSwitch(TIME_SIG3, -16.f, 16.f, 0.f, "Time Signature 3", sigsStrings);
		configSwitch(TIME_SIG4, -16.f, 16.f, 0.f, "Time Signature 4", sigsStrings);
		configSwitch(TIME_SIG5, -16.f, 16.f, 0.f, "Time Signature 5", sigsStrings);
		configSwitch(TIME_SIG6, -16.f, 16.f, 0.f, "Time Signature 6", sigsStrings);
		configSwitch(TIME_SIG7, -16.f, 16.f, 0.f, "Time Signature 7", sigsStrings);
		configSwitch(TIME_SIG8, -16.f, 16.f, 0.f, "Time Signature 8", sigsStrings);
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
		bool clockedThisTick = false;
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
			if (clockTrigger.process(inputs[CLOCK].getVoltage(), 0.1f, 2.f)) {
				clockTime = clockTimer.getTime();
				clockTimer.reset();
				clockTicksSinceReset += 1;
				subClockTime = 0;
			}
		} else clockTime = 0.5f;
		subClockTime += args.sampleTime;

		// clock resets and swap checks
		for (size_t i = 0; i < 8; i++) {
			bool clockHit = false;

			// on clock
			if (timeSigs[i] < 0.f) {
				float currentTime = clockTicksSinceReset % (int)abs(timeSigs[i]);
				if (currentTime < accumulatedTime[i]) clockHit = true;
				accumulatedTime[i] = currentTime;
			}
			if (timeSigs[i] > 0.f) {
				float currentTime = fmod(subClockTime, 1/timeSigs[i]);
				if (currentTime < accumulatedTime[i]) clockHit = true;
				accumulatedTime[i] = currentTime;
			}
			
			// on clock
			//if (timeSigs[i] < 0.f && accumulatedTime[i] >= clockTime*fabs(timeSigs[i])) clockHit = true; // clock divide
			//else if (timeSigs[i] > 0.f && accumulatedTime[i] >= clockTime/timeSigs[i]) clockHit = true; // clock multiply

			// edge cases
			if (resetClocks) clockHit = true; // on reset
			if (shouldSwap[i] && timeSigs[i] == 0.f) clockHit = true; // on immediate

			if (clockHit) accumulatedTime[i] = 0.f;

			if (shouldSwap[i] && clockHit) {
				muteState[i] = !muteState[i];
				shouldSwap[i] = false;
			}
		}

		if (resetClocks) {
			for (size_t i = 0; i < 8; i++) {
				if (resetClocks) accumulatedTime[i] = 0.f;
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

	json_t* dataToJson() { 
		json_t* nodeJ = QuestionableModule::dataToJson();
		json_object_set_new(nodeJ, "clockTime", json_real(clockTime));
		return nodeJ;
	}

	void dataFromJson(json_t* rootJ) {
		QuestionableModule::dataFromJson(rootJ);
		if (json_t* ct = json_object_get(rootJ, "clockTime")) clockTime = json_real_value(ct);
	}

};

struct MuteButton : Resizable<CKD6> {
	
	MuteButton() : Resizable(0.85, true) { }

	void drawLayer(const DrawArgs &args, int layer) override {
		if (!module) return;
		SyncMute* mod = (SyncMute*)module;
		
		if (mod->muteState[paramId]) {
			nvgFillColor(args.vg, nvgRGB(255, 0, 0));
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, box.size.x/2, box.size.y/2, 10.f);
			nvgFill(args.vg);
		}

		if (mod->shouldSwap[paramId] && mod->inputs[SyncMute::CLOCK].getVoltage() > 0.f) {
			nvgFillColor(args.vg, nvgRGB(0, 255, 0));
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, box.size.x/2, box.size.y/2, 10.f);
			nvgFill(args.vg);
		}
	}
};

struct SyncMuteWidget : QuestionableWidget {

	void setText() {
		NVGcolor c = nvgRGB(255,255,255);
		color->textList.clear();
		color->addText("MUTE", "OpenSans-ExtraBold.ttf", c, 24, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, 25));
		color->addText("·ISI·", "OpenSans-ExtraBold.ttf", c, 28, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, RACK_GRID_HEIGHT-13));

		color->addText("OUT", "OpenSans-Bold.ttf", c, 7, Vec(22, 353), "descriptor");
	}

	SyncMuteWidget(SyncMute* module) {
		setModule(module);

		backdrop = new ImagePanel();
		backdrop->box.size = Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
		//backdrop->imagePath = asset::plugin(pluginInstance, "res/backdrop.jpg");
		backdrop->scalar = 3.5;
		backdrop->visible = true;

		color = new ColorBG(Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT));
		color->drawBackground = false;
		setText();

		backgroundColorLogic(module);
		
		setPanel(backdrop);
		addChild(color);

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


		for (size_t i = 0; i < 8; i++) {
			addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(7.8, 16 + (13.2* i))), module, SyncMute::IN + i));
			addParam(createParamCentered<QuestionableParam<RoundLargeBlackKnob>>(mm2px(Vec(20.2, 16 + (13.2* i))), module, SyncMute::TIME_SIG + i));
			addParam(createParamCentered<QuestionableParam<MuteButton>>(mm2px(Vec(20.2, 16 + (13.2* i))), module, SyncMute::MUTE + i));
			addOutput(createOutputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(32.8, 16 + (13.2 * i))), module, SyncMute::OUT + i));
		}

		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(7.8, 119)), module, SyncMute::CLOCK));
		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(32.8, 119)), module, SyncMute::RESET));
	}

};

Model* modelSyncMute = createModel<SyncMute, SyncMuteWidget>("syncmute");
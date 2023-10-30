#include "plugin.hpp"
#include "imagepanel.cpp"
#include "colorBG.hpp"
#include "questionableModule.hpp"
#include "ui.cpp"
#include <vector>
#include <algorithm>

const int MODULE_SIZE = 8;

struct SyncMute : QuestionableModule {
	enum ParamId {
		MUTE,
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

	bool expanderRight = false;
	bool expanderLeft = false;

	float lightOpacity = 1.f;

	dsp::SchmittTrigger resetTrigger;
	dsp::SchmittTrigger clockTrigger;
	dsp::Timer clockTimer;
	float clockTime = 0.5f; // in seconds

	float accumulatedTime[8] = {0.f};

	std::vector<std::string> sigsStrings = {"/32", "/31", "/30", "/29", "/28", "/27", "/26", "/25", "/24", "/23", "/22", "/21", "/20", "/19", "/18", "/17", "/16", "/15", "/14", "/13", "/12", "/11", "/10", "/9", "/8", "/7", "/6", "/5", "/4", "/3", "/2", "/1", "Immediate", "X1", "X2", "X3", "X4", "X5", "X6", "X7", "X8", "X9", "X10", "X11", "X12", "X13", "X14", "X15", "X16", "X17", "X18", "X19", "X20", "X21", "X22", "X23", "X24", "X25", "X26", "X27", "X28", "X29", "X30", "X31", "X32"};

	SyncMute() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configSwitch(MUTE, 0.f, 1.f, 0.f, "Mute Toggle");
		configSwitch(MUTE2, 0.f, 1.f, 0.f, "Mute Toggle");
		configSwitch(MUTE3, 0.f, 1.f, 0.f, "Mute Toggle");
		configSwitch(MUTE4, 0.f, 1.f, 0.f, "Mute Toggle");
		configSwitch(MUTE5, 0.f, 1.f, 0.f, "Mute Toggle");
		configSwitch(MUTE6, 0.f, 1.f, 0.f, "Mute Toggle");
		configSwitch(MUTE7, 0.f, 1.f, 0.f, "Mute Toggle");
		configSwitch(MUTE8, 0.f, 1.f, 0.f, "Mute Toggle");
		configSwitch(TIME_SIG, -32.f, 32.f, 0.f,  "Ratio", sigsStrings);
		configSwitch(TIME_SIG2, -32.f, 32.f, 0.f, "Ratio", sigsStrings);
		configSwitch(TIME_SIG3, -32.f, 32.f, 0.f, "Ratio", sigsStrings);
		configSwitch(TIME_SIG4, -32.f, 32.f, 0.f, "Ratio", sigsStrings);
		configSwitch(TIME_SIG5, -32.f, 32.f, 0.f, "Ratio", sigsStrings);
		configSwitch(TIME_SIG6, -32.f, 32.f, 0.f, "Ratio", sigsStrings);
		configSwitch(TIME_SIG7, -32.f, 32.f, 0.f, "Ratio", sigsStrings);
		configSwitch(TIME_SIG8, -32.f, 32.f, 0.f, "Ratio", sigsStrings);
		configInput(IN,  "1");
		configInput(IN2, "2");
		configInput(IN3, "3");
		configInput(IN4, "4");
		configInput(IN5, "5");
		configInput(IN6, "6");
		configInput(IN7, "7");
		configInput(IN8, "8");
		configOutput(OUT,  "1");
		configOutput(OUT2, "2");
		configOutput(OUT3, "3");
		configOutput(OUT4, "4");
		configOutput(OUT5, "5");
		configOutput(OUT6, "6");
		configOutput(OUT7, "7");
		configOutput(OUT8, "8");
		configInput(CLOCK, "Clock");
		configInput(RESET, "Reset");

		for (size_t i = 0; i < 8; i++) {
			mutes[i].module = this;
			mutes[i].paramId = i;
		}

		onReset();
	}

	struct Mute {
		int paramId = -1;
		SyncMute* module = nullptr;
		float timeSignature = 0.f;
		bool muteState = false;
		bool shouldSwap = false;
		dirtyable<bool> button = false;

		bool autoPress = false;
		float lightOpacity = 1.f;

		float accumulatedTime = 0.f;

		float volume = 1.f;

		void step(float deltaTime) {
			timeSignature = module->params[TIME_SIG+paramId].getValue();
			button = module->params[MUTE+paramId].getValue();
			if (button.isDirty() && button == true) shouldSwap = !shouldSwap;

			bool clockHit = false;

			// on clock
			if (timeSignature < 0.f) {
				float currentTime = fmod(module->clockTicksSinceReset + ((timeSignature == -1) ? module->subClockTime : 0), abs(timeSignature));
				if (currentTime < accumulatedTime) clockHit = true;
				accumulatedTime = currentTime;
			}
			if (timeSignature > 0.f) {
				float currentTime = fmod((module->subClockTime / (module->clockTime/(timeSignature))), 1);
				if (currentTime < accumulatedTime) clockHit = true;
				accumulatedTime = currentTime;
			}

			// edge cases
			if (module->resetClocksThisTick) clockHit = true; // on reset
			if (shouldSwap && timeSignature == 0.f) clockHit = true; // on immediate

			if (clockHit) accumulatedTime = 0.f;

			if (shouldSwap && clockHit) {
				muteState = !muteState;
				shouldSwap = false;
			}

			if (timeSignature != 0 && autoPress && clockHit) shouldSwap = true; // auto press on clock option

			float timeMultiply = 25;
			if (timeSignature > 0.f) timeMultiply = 25 * timeSignature; // speed up volume mute for faster intervals
			volume = math::clamp(volume + (muteState ? -(deltaTime*timeMultiply) : deltaTime*timeMultiply));
		}

		json_t* toJson() {
			json_t* rootJ = json_object();
			json_object_set_new(rootJ, "muteState", json_boolean(muteState));
			json_object_set_new(rootJ, "autoPress", json_boolean(autoPress));
			json_object_set_new(rootJ, "lightOpacity", json_real(lightOpacity));
			return rootJ;
		}

		void fromJson(json_t* json) {
			if (json_t* v = json_object_get(json, "muteState")) muteState = json_boolean_value(v);
			if (json_t* v = json_object_get(json, "autoPress")) autoPress = json_boolean_value(v);
			if (json_t* l = json_object_get(json, "lightOpacity")) lightOpacity = json_real_value(l);
		}
	};

	Mute mutes[8];

	uint64_t clockTicksSinceReset = 0;
	float subClockTime = 0.f;

	bool resetClocksThisTick = false;
	dirtyable<bool> isClockInputConnected = true;

	void onReset() override {
		clockTicksSinceReset = 0;
		subClockTime = 0.f;
		//clockTimer.reset();
		clockTrigger.reset();
	}

	void process(const ProcessArgs& args) override {
		resetClocksThisTick = resetTrigger.process(inputs[RESET].getVoltage(), 0.1f, 2.f);
		isClockInputConnected = inputs[CLOCK].isConnected();

		// clock stuff from lfo
		if (isClockInputConnected) {
			if (isClockInputConnected.isDirty()) onReset(); // on first entry of true
			clockTimer.process(args.sampleTime);
			if (clockTimer.getTime() > clockTime) clockTime = clockTimer.getTime();
			if (clockTrigger.process(inputs[CLOCK].getVoltage(), 0.1f, 2.f)) {
				clockTime = clockTimer.getTime();
				clockTimer.reset();
				clockTicksSinceReset += 1;
				subClockTime = 0;
			}
		} else { // 0.5f clock
			if (isClockInputConnected.isDirty()) clockTime = 0.5f;
			if (subClockTime >= clockTime) {
				clockTicksSinceReset += 1;
				subClockTime = 0.f;
			}
		}

		if (resetClocksThisTick) onReset();

		for (size_t i = 0; i < 8; i++) mutes[i].step(args.sampleTime);

		subClockTime += args.sampleTime; // this must be after step to fix clock never getting hit with multiply ratio
		
		// expander logic
		if (expanderRight) {
			Module* rightModule = getRightExpander().module;
			if (rightModule && rightModule->model == this->model) {
				SyncMute* other = (SyncMute*)rightModule;
				if (!other->expanderLeft) {
					if (!other->inputs[RESET].isConnected()) other->inputs[RESET].setVoltage(inputs[RESET].getVoltage());
					if (!other->inputs[CLOCK].isConnected()) {
						other->clockTime = clockTime;
						other->clockTicksSinceReset = clockTicksSinceReset;
						other->subClockTime = subClockTime;
					}
					for (size_t i = 0; i < 8; i++) {
						other->mutes[i].autoPress = mutes[i].autoPress;
						if (params[MUTE+i].getValue() != other->params[MUTE+i].getValue()) other->params[MUTE+i].setValue(params[MUTE+i].getValue());
					}
				}
			}
		}

		if (expanderLeft) {
			Module* leftModule = getLeftExpander().module;
			if (leftModule && leftModule->model == this->model) {
				SyncMute* other = (SyncMute*)leftModule;
				if (!other->expanderRight) {
					if (!other->inputs[RESET].isConnected()) other->inputs[RESET].setVoltage(inputs[RESET].getVoltage());
					if (!other->inputs[CLOCK].isConnected()) {
						other->clockTime = clockTime;
						other->clockTicksSinceReset = clockTicksSinceReset;
						other->subClockTime = subClockTime;
					}
					for (size_t i = 0; i < 8; i++) {
						other->mutes[i].autoPress = mutes[i].autoPress;
						if (params[MUTE+i].getValue() != other->params[MUTE+i].getValue()) other->params[MUTE+i].setValue(params[MUTE+i].getValue());
					}
				}
			}
		}
		
		// outputs
		for (size_t i = 0; i < 8; i++) {
			PolyphonicValue input(inputs[IN+i]);
			input *= mutes[i].volume;
			input.setOutput(outputs[OUT+i]);
		}

	}

	json_t* dataToJson() override { 
		json_t* nodeJ = QuestionableModule::dataToJson();
		json_object_set_new(nodeJ, "clockTime", json_real(clockTime));
		json_object_set_new(nodeJ, "expanderRight", json_boolean(expanderRight)); 
		json_object_set_new(nodeJ, "expanderLeft", json_boolean(expanderLeft)); 
		json_object_set_new(nodeJ, "lightOpacity", json_real(lightOpacity));

		json_t* array = json_array();
		for (size_t i = 0; i < 8; i++) json_array_append_new(array, mutes[i].toJson());
		json_object_set_new(nodeJ, "mutes", array);

		return nodeJ;
	}

	void dataFromJson(json_t* rootJ) override {
		QuestionableModule::dataFromJson(rootJ);
		if (json_t* ct = json_object_get(rootJ, "clockTime")) clockTime = json_real_value(ct);
		if (json_t* er = json_object_get(rootJ, "expanderRight")) expanderRight = json_boolean_value(er);
		if (json_t* el = json_object_get(rootJ, "expanderLeft")) expanderLeft = json_boolean_value(el);
		if (json_t* l = json_object_get(rootJ, "lightOpacity")) lightOpacity = json_real_value(l);

		if (json_t* array = json_object_get(rootJ, "mutes")) { // assumes all 8 set
			for (size_t i = 0; i < 8; i++) { 
				mutes[i].fromJson(json_array_get(array, i)); 
			}
		}

	}

};

struct ClockKnob : Resizable<QuestionableLargeKnob> {

	ClockKnob() : Resizable(1.085, true) {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/BlackKnobFG.svg")));
	}

	void draw(const DrawArgs &args) override {
		SyncMute* mod = (SyncMute*)module;

		float anglePerTick = 31 / 1.65;

		float sig = mod ? mod->mutes[paramId - SyncMute::TIME_SIG].timeSignature : 0.f;
		
		Resizable<QuestionableLargeKnob>::draw(args);

		nvgSave(args.vg);

		nvgTranslate(args.vg, box.size.x/2, box.size.y/2);

		for (int i = -15; i < 16; i++) {
			nvgSave(args.vg);
			nvgRotate(args.vg, nvgDegToRad(i*(90.f/ (15 / 1.65))));
			nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, 0, 0);
			nvgLineTo(args.vg, 0, 6-box.size.y/2);
			nvgStrokeWidth(args.vg, 0.5);
			nvgStroke(args.vg);
			nvgRestore(args.vg);
		}
		
		if (mod && sig < 0.f) nvgRotate(args.vg, nvgDegToRad(mod->clockTicksSinceReset %((int)abs(sig))*(-90.f/anglePerTick)));
		if (mod && sig > 0.f) nvgRotate(args.vg, nvgDegToRad(fmod((mod->subClockTime / (mod->clockTime/sig)) * sig, sig)*(90.f/anglePerTick)));
		
		nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 0, 0);
		nvgLineTo(args.vg, 0, 4.5-box.size.y/2);
		nvgStrokeWidth(args.vg, 3);
		nvgStroke(args.vg);

		nvgRestore(args.vg);
	
	}

};

struct OpacityQuantity : QuestionableQuantity {
	std::function<float()> getValueFunc;
	std::function<void(float)> setValueFunc;

	OpacityQuantity(quantityGetFunc getFunc, quantitySetFunc setFunc) : QuestionableQuantity(getFunc, setFunc) {

	}

	float getDisplayValue() override {
		return getValue() * 100;
	}

	void setDisplayValue(float displayValue) override {
		setValue(displayValue / 100.f);
	}

	std::string getLabel() override {
		return "Light Opacity";
	}

	std::string getUnit() override {
		return "%";
	}

};

struct GlobalOpacityQuantity : OpacityQuantity {
	std::function<float()> getValueFunc;
	std::function<void(float)> setValueFunc;

	GlobalOpacityQuantity(quantityGetFunc getFunc, quantitySetFunc setFunc) : OpacityQuantity(getFunc, setFunc) {
		
	}

	std::string getLabel() override {
		return "Global Light Opacity";
	}

};

struct MuteButton : Resizable<QuestionableTimed<QuestionableParam<CKD6>>> {
	dirtyable<bool> lightState = false;
	float lightAlpha = 0.f;
	
	MuteButton() : Resizable(0.85, true) { }

	void drawLayer(const DrawArgs &args, int layer) override {
		if (!module) return;
		if (layer != 1) return;

		nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);

		SyncMute* mod = (SyncMute*)module;
		int sig = mod->mutes[paramId].timeSignature;
		float opacity = mod->mutes[this->paramId].lightOpacity;
		
		if (mod->mutes[paramId].muteState) {
			nvgFillColor(args.vg, nvgRGBA(255, 0, 25, opacity * mod->lightOpacity*255));
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, box.size.x/2, box.size.y/2, 10.f);
			nvgFill(args.vg);
		}

		if (mod->clockTime/32 < 0.05 && sig > 0.f) return; // no super fast flashing lights

		lightState = mod->mutes[paramId].shouldSwap && (sig < 0.f ? mod->clockTicksSinceReset%2 : fmod((mod->subClockTime / (mod->clockTime/32)), 2)) < 0.5f;

		if (lightState.isDirty()) lightAlpha = opacity * mod->lightOpacity;

		nvgFillColor(args.vg, nvgRGBA(0, 255, 25, lightAlpha*255));
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, box.size.x/2, box.size.y/2, 10.f);
		nvgFill(args.vg);

		lightAlpha = std::max<float>(0.f, lightAlpha-(deltaTime*5));
	}

	void appendContextMenu(ui::Menu* menu) override {
		if (!this->module) return;
		SyncMute* mod = (SyncMute*)this->module;
		menu->addChild(createMenuItem("Automatically Press", mod->mutes[this->paramId].autoPress ? "On" : "Off", [=]() {
			mod->mutes[this->paramId].autoPress = !mod->mutes[this->paramId].autoPress;
		}));

		menu->addChild(new QuestionableSlider<OpacityQuantity>(
			[=]() { return mod->mutes[this->paramId].lightOpacity; }, 
			[=](float value) { mod->mutes[this->paramId].lightOpacity = math::clamp(value); }
		));

		Resizable<QuestionableTimed<QuestionableParam<CKD6>>>::appendContextMenu(menu);
	}

};

struct SyncMuteWidget : QuestionableWidget {
	//ColorBGSimple* bgSimple;
	void setText() {
		NVGcolor c = nvgRGB(255,255,255);
		color->textList.clear();
		color->addText("SMUTE", "OpenSans-ExtraBold.ttf", c, 24, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, 21));
		color->addText("·ISI·", "OpenSans-ExtraBold.ttf", c, 28, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, RACK_GRID_HEIGHT-13));

		color->addText("INS", "OpenSans-Bold.ttf", c, 7, Vec(23, 333), "descriptor");
		color->addText("MUTES", "OpenSans-Bold.ttf", c, 7, Vec(60, 342), "descriptor");
		color->addText("OUTS", "OpenSans-Bold.ttf", c, 7, Vec(97.3, 333), "descriptor");

		color->addText("CLOCK", "OpenSans-Bold.ttf", c, 7, Vec(23, 363), "descriptor");
		color->addText("RESET", "OpenSans-Bold.ttf", c, 7, Vec(97.3, 363), "descriptor");
	}

	SyncMuteWidget(SyncMute* module) {
		setModule(module);

		//bgSimple = new ColorBGSimple(Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT), nvgRGB(20,20,20));
		//bgSimple->visible = true;

		backdrop = new ImagePanel();
		backdrop->box.size = Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
		backdrop->imagePath = asset::plugin(pluginInstance, "res/smute/smute.jpg"); 
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

		float initalOffset = 14.2;
		float yoffset = 13.2;
		for (size_t i = 0; i < 8; i++) {
			addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(7.8, initalOffset + (yoffset* i))), module, SyncMute::IN + i));
			addParam(createParamCentered<QuestionableParam<ClockKnob>>(mm2px(Vec(20.2, initalOffset + (yoffset* i))), module, SyncMute::TIME_SIG + i));
			addParam(createParamCentered<MuteButton>(mm2px(Vec(20.2, initalOffset + (13.2* i))), module, SyncMute::MUTE + i));
			addOutput(createOutputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(32.8, initalOffset + (yoffset * i))), module, SyncMute::OUT + i));
		}

		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(7.8, 117)), module, SyncMute::CLOCK));
		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(32.8, 117)), module, SyncMute::RESET));
	}

	void appendContextMenu(Menu *menu) override
  	{
		if (!module) return;

		SyncMute* mod = (SyncMute*)module;

		menu->addChild(createMenuItem("Toggle Left Expander", mod->expanderLeft ? "On" : "Off",[=]() {
			mod->expanderLeft = !mod->expanderLeft;
		}));

		menu->addChild(createMenuItem("Toggle Right Expander", mod->expanderRight ? "On" : "Off",[=]() {
			mod->expanderRight = !mod->expanderRight;
		}));

		menu->addChild(new QuestionableSlider<GlobalOpacityQuantity>(
			[=]() { return mod->lightOpacity; }, 
			[=](float value) { mod->lightOpacity = math::clamp(value); }
		));

		QuestionableWidget::appendContextMenu(menu);

	}

};

Model* modelSyncMute = createModel<SyncMute, SyncMuteWidget>("smute");
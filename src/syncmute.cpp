#include "plugin.hpp"
#include "imagepanel.cpp"
#include "colorBG.hpp"
#include "questionableModule.hpp"
#include "ui.cpp"
#include <vector>
#include <algorithm>

const int MODULE_SIZE = 8;

const float clockIgnoreTime = 0.001;

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

	float lightOpacity = 1.f;

	dsp::SchmittTrigger resetTrigger;
	dsp::SchmittTrigger clockTrigger;
	bool ignoreClockTrigger = true;
	dsp::Timer clockTimer;
	float clockTime = 0.5f; // in seconds

	float accumulatedTime[8] = {0.f};

	enum MessageType {
		ONBUTTON,
		ONBUTTONAUTO,
		ONRESET,
		ONCLOCK,
	};

	struct ExpanderMessage {
		SyncMute* sender = nullptr; // original sender
		MessageType type;
		int buttonId;
		bool autoPress;
		float time;
		uint64_t clockTicksSinceReset;

		static ExpanderMessage resetMessage(SyncMute* sender) {
			return ExpanderMessage({sender, MessageType::ONRESET});
		}

		static ExpanderMessage buttonMessage(SyncMute* sender, int buttonId) {
			return ExpanderMessage({sender, MessageType::ONBUTTON, buttonId});
		}

		static ExpanderMessage clockMessage(SyncMute* sender, float clockTime, uint64_t clockTicksSinceReset = std::numeric_limits<uint64_t>::max()) {
			return ExpanderMessage({sender, MessageType::ONCLOCK, 0, 0, clockTime, clockTicksSinceReset});
		}

		static ExpanderMessage buttonAutoPress(SyncMute* sender, int buttonId, bool autoPress) {
			return ExpanderMessage({sender, MessageType::ONBUTTONAUTO, buttonId, autoPress});
		}

	};

	enum SendDirection {
		BOTH,
		LEFT,
		RIGHT
	};

	ThreadQueue<ExpanderMessage> expanderMessages;
	bool expanderRight = false;
	bool expanderLeft = false;

	// Send message to any controlled smutes
	void sendExpanderMessage(ExpanderMessage msg, SendDirection sendDirection = SendDirection::BOTH) {
		bool leftConnected = expanderLeft && expanderConnected(true);
		bool rightConnected = expanderRight && expanderConnected(false);
		
		if (leftConnected && sendDirection != SendDirection::RIGHT) {
			SyncMute* mod = (SyncMute*)getLeftExpander().module;
			mod->recieveExpanderMessage(SendDirection::LEFT, msg);
		}
		if (rightConnected && sendDirection != SendDirection::LEFT) {
			SyncMute* mod = (SyncMute*)getRightExpander().module;
			mod->recieveExpanderMessage(SendDirection::RIGHT, msg);
		}
	}
	
	void recieveExpanderMessage(SendDirection fromDirection, ExpanderMessage msg) {
		if (inputs[CLOCK].isConnected() && msg.type == MessageType::ONCLOCK) return; // we dont accept clock control when our own clock is connected
		if (fromDirection == SendDirection::LEFT && expanderRight) return; // dont try to control each other
		if (fromDirection == SendDirection::RIGHT && expanderLeft) return; // dont try to control each other

		expanderMessages.push(msg);

		// pass along if we're also controlling a smute
		if (expanderLeft) sendExpanderMessage(msg, SendDirection::LEFT);
		if (expanderRight) sendExpanderMessage(msg, SendDirection::RIGHT);
	}

	void processMessages() {
		while (!expanderMessages.empty()) {
			ExpanderMessage msg = expanderMessages.front();
			if (msg.type == MessageType::ONRESET) onReset();
			if (msg.type == MessageType::ONBUTTON) mutes[msg.buttonId].shouldSwap = !mutes[msg.buttonId].shouldSwap;
			if (msg.type == MessageType::ONBUTTONAUTO) mutes[msg.buttonId].autoPress = msg.autoPress;
			if (msg.type == MessageType::ONCLOCK) {
				if (inputs[CLOCK].isConnected()) continue; // we take control
				clockTime = msg.time;
				if (msg.clockTicksSinceReset != std::numeric_limits<uint64_t>::max()) {
					clockTicksSinceReset = msg.clockTicksSinceReset;
					subClockTime = 0.f;
				}
			}
			expanderMessages.pop();
		}
	}

	// Are you being controlled by another smute?
	bool isControlled() {
		return isControlledLeft() || isControlledRight();
	}

	bool isControlledLeft() {
		SyncMute* left = getExpander(true);
		if (expanderLeft) return false;
		if (left && left->expanderRight) return true;
		return false;
	}

	bool isControlledRight() {
		SyncMute* right = getExpander(false);
		if (expanderRight) return false;
		if (right && right->expanderLeft) return true;
		return false;
	}

	bool expanderConnected(bool left) {
		Module* expander = left ? getLeftExpander().module : getRightExpander().module;
		return expander && expander->model == this->model;
	}

	SyncMute* getExpander(bool left) {
		Module* expander = left ? getLeftExpander().module : getRightExpander().module;
		if (expander != nullptr && expander->model == this->model) return reinterpret_cast<SyncMute*>(expander);
		return nullptr;
	}

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
		int signatureOffset = 0.f;
		bool muteState = false;
		bool shouldSwap = false;
		dirtyable<bool> button = false;

		dirtyable<bool> autoPress = false;
		float lightOpacity = 1.f;
		bool softTransition = true;
		float ratioRangeLeft = 0;
		float ratioRangeRight = 0;

		float accumulatedTime = 0.f;

		float volume = 1.f;

		float getRawSignatureValue() {
			return module->params[TIME_SIG+paramId].getValue();
		}

		void step(float deltaTime) {
			float rawSig = getRawSignatureValue();
			timeSignature = rawSig + signatureOffset;
			button = module->params[MUTE+paramId].getValue();
			if (button.isDirty() && button == true) {
				shouldSwap = !shouldSwap;
				module->sendExpanderMessage(ExpanderMessage::buttonMessage(module, paramId));
			}

			if (autoPress.isDirty()) module->sendExpanderMessage(ExpanderMessage::buttonAutoPress(module, paramId, autoPress));

			bool clockHit = false;

			// keep ratio ranges within limits
			ratioRangeLeft = math::clamp(ratioRangeLeft, 0.f, 32.f - (rawSig < 0 ? abs(rawSig) : 0));
			ratioRangeRight = math::clamp(ratioRangeRight, 0.f, 32.f - (rawSig > 0 ? abs(rawSig) : 0));

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
				// if range specified, randomly offset ratio
				if (ratioRangeLeft || ratioRangeRight) signatureOffset = randomInt(-(int)ratioRangeLeft, (int)ratioRangeRight);
				else signatureOffset = 0;
			}

			if (timeSignature != 0 && autoPress && clockHit) shouldSwap = true; // auto press on clock option

			if (!softTransition) {
				// immediate swap
				volume = muteState ? 0.f : 1.f;
			} else {
				// soft swap
				float timeMultiply = 25;
				if (timeSignature > 0.f) timeMultiply = 25 * timeSignature; // speed up volume mute for faster intervals
				volume = math::clamp(volume + (muteState ? -(deltaTime*timeMultiply) : deltaTime*timeMultiply));
			}
		}

		json_t* toJson() {
			json_t* rootJ = json_object();
			json_object_set_new(rootJ, "muteState", json_boolean(muteState));
			json_object_set_new(rootJ, "autoPress", json_boolean(autoPress));
			json_object_set_new(rootJ, "lightOpacity", json_real(lightOpacity));
			json_object_set_new(rootJ, "softTransition", json_boolean(softTransition));
			json_object_set_new(rootJ, "ratioRangeLeft", json_integer(ratioRangeLeft));
			json_object_set_new(rootJ, "ratioRangeRight", json_integer(ratioRangeRight));
			return rootJ;
		}

		void fromJson(json_t* json) {
			if (json_t* v = json_object_get(json, "muteState")) muteState = json_boolean_value(v);
			if (json_t* v = json_object_get(json, "autoPress")) autoPress = json_boolean_value(v);
			if (json_t* l = json_object_get(json, "lightOpacity")) lightOpacity = json_real_value(l);
			if (json_t* s = json_object_get(json, "softTransition")) softTransition = json_boolean_value(s);
			if (json_t* l = json_object_get(json, "ratioRangeLeft")) ratioRangeLeft = json_integer_value(l);
			if (json_t* r = json_object_get(json, "ratioRangeRight")) ratioRangeRight = json_integer_value(r);
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
		ignoreClockTrigger = true; // ignore for 0.001 seconds
		sendExpanderMessage(ExpanderMessage::resetMessage(this));
	}

	void process(const ProcessArgs& args) override {
		processMessages();
		resetClocksThisTick = resetTrigger.process(inputs[RESET].getVoltage(), 0.1f, 2.f);
		isClockInputConnected = inputs[CLOCK].isConnected();

		// wait a set amount of time after reset before accepting clock input
		if (ignoreClockTrigger) if (subClockTime >= clockIgnoreTime) ignoreClockTrigger = false;

		// clock stuff from lfo
		if (!ignoreClockTrigger) {
			if (isClockInputConnected) {
				if (isClockInputConnected.isDirty()) onReset(); // on first entry of true
				clockTimer.process(args.sampleTime);
				if (clockTimer.getTime() > clockTime) {
					clockTime = clockTimer.getTime();
					sendExpanderMessage(ExpanderMessage::clockMessage(this, clockTime));
				}
				if (clockTrigger.process(inputs[CLOCK].getVoltage(), 0.1f, 2.f)) {
					clockTime = clockTimer.getTime();
					clockTimer.reset();
					clockTicksSinceReset += 1;
					subClockTime = 0;
					sendExpanderMessage(ExpanderMessage::clockMessage(this, clockTime, clockTicksSinceReset));
				}
			} else if (!isControlled()) { // 0.5f clock
				if (isClockInputConnected.isDirty()) {
					clockTime = 0.5f;
					sendExpanderMessage(ExpanderMessage::clockMessage(this, clockTime));
				}
				if (subClockTime >= clockTime) {
					clockTicksSinceReset += 1;
					subClockTime = 0.f;
					sendExpanderMessage(ExpanderMessage::clockMessage(this, clockTime, clockTicksSinceReset));
				}
			}
		}

		if (resetClocksThisTick) onReset();

		for (size_t i = 0; i < 8; i++) mutes[i].step(args.sampleTime);

		subClockTime += args.sampleTime; // this must be after step to fix clock never getting hit with multiply ratio
		
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

	void step() override {
		Resizable<QuestionableLargeKnob>::step();

		if (!module) return;
		SyncMute* mod = (SyncMute*)module;
		ParamQuantity* pq = getParamQuantity();
		int rangeL = (int)mod->mutes[paramId - SyncMute::TIME_SIG].ratioRangeLeft;
		int rangeR = (int)mod->mutes[paramId - SyncMute::TIME_SIG].ratioRangeRight;
		if (pq && (rangeL || rangeR)) pq->description = "offset: " + std::to_string(mod->mutes[paramId - SyncMute::TIME_SIG].signatureOffset);
	}

	void draw(const DrawArgs &args) override {
		SyncMute* mod = (SyncMute*)module;

		float anglePerTick = 32 / 1.65;

		float sig = mod ? mod->mutes[paramId - SyncMute::TIME_SIG].timeSignature : 0.f;
		int ratioRangeL = mod ? mod->mutes[paramId - SyncMute::TIME_SIG].ratioRangeLeft : 0.f;
		int ratioRangeR = mod ? mod->mutes[paramId - SyncMute::TIME_SIG].ratioRangeRight : 0.f;
		float offset = mod ? mod->mutes[paramId - SyncMute::TIME_SIG].signatureOffset : 0.f;
		
		Resizable<QuestionableLargeKnob>::draw(args);

		nvgSave(args.vg);

		nvgTranslate(args.vg, box.size.x/2, box.size.y/2);

		if (ratioRangeL != 0 || ratioRangeR != 0) {
			float anglePerRatio = 8 / 1.65;
			float baseRatio = mod ? mod->mutes[paramId - SyncMute::TIME_SIG].getRawSignatureValue() : 0.f;

			nvgSave(args.vg);
			nvgRotate(args.vg, nvgDegToRad((baseRatio)*(90.f/anglePerTick)));

			nvgStrokeColor(args.vg, nvgRGB(0, 200, 255));
			nvgBeginPath(args.vg);
			nvgArc(args.vg, 0, 0, 13.f, nvgDegToRad(-90 - (anglePerRatio*ratioRangeL)), nvgDegToRad(-90), NVG_CW);
			nvgStrokeWidth(args.vg, 2);
			nvgStroke(args.vg);

			nvgStrokeColor(args.vg, nvgRGB(0, 200, 255));
			nvgBeginPath(args.vg);
			nvgArc(args.vg, 0, 0, 13.f, nvgDegToRad(-90), nvgDegToRad(-90 + (anglePerRatio*ratioRangeR)), NVG_CW);
			nvgStrokeWidth(args.vg, 2);
			nvgStroke(args.vg);

			nvgRotate(args.vg, nvgDegToRad((offset)*(90.f/anglePerTick)));
			nvgFillColor(args.vg, nvgRGB(0, 200, 255));
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, 0, 2.75-box.size.y/2, 1.45);
			nvgFill(args.vg);
			nvgRestore(args.vg);
		}

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

struct OffsetQuantity : QuestionableQuantity {

	OffsetQuantity(quantityGetFunc getFunc, quantitySetFunc setFunc) : QuestionableQuantity(getFunc, setFunc) {

	}

	float getMaxValue() {
		return 32.f;
	}

	std::string getUnit() override {
		return "";
	}

	std::string getDisplayValueString() override {
		float v = getDisplayValue();
		if (std::isnan(v))
			return "NaN";
		return std::to_string((int)getDisplayValue());
	}

};

struct OffsetQuantityLeft : OffsetQuantity {
	OffsetQuantityLeft(quantityGetFunc getFunc, quantitySetFunc setFunc) : OffsetQuantity(getFunc, setFunc) {

	}
	std::string getLabel() override { return "Range Left"; }
};

struct OffsetQuantityRight : OffsetQuantity {
	OffsetQuantityRight(quantityGetFunc getFunc, quantitySetFunc setFunc) : OffsetQuantity(getFunc, setFunc) {

	}
	std::string getLabel() override { return "Range Right"; }
};

struct OpacityQuantity : QuestionableQuantity {

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

		menu->addChild(createMenuItem("Soft Transition", mod->mutes[this->paramId].softTransition ? "On" : "Off", [=]() {
			mod->mutes[this->paramId].softTransition = !mod->mutes[this->paramId].softTransition;
		}));


		menu->addChild(createSubmenuItem("Random Ratio Ranges", "", [=](ui::Menu* menu) {
			float rawSig = mod->mutes[this->paramId].getRawSignatureValue();
			menu->addChild(new QuestionableSlider<OffsetQuantityLeft>(
				[=]() { return mod->mutes[this->paramId].ratioRangeLeft; }, 
				[=](float value) { mod->mutes[this->paramId].ratioRangeLeft = math::clamp(value, 0.f, 32.f - (rawSig < 0 ? abs(rawSig) : 0)); }
			));

			menu->addChild(new QuestionableSlider<OffsetQuantityRight>(
				[=]() { return mod->mutes[this->paramId].ratioRangeRight; }, 
				[=](float value) { mod->mutes[this->paramId].ratioRangeRight = math::clamp(value, 0.f, 32.f - (rawSig > 0 ? abs(rawSig) : 0)); }
			));
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

		SyncMute* leftExpander = mod->getExpander(true);
		SyncMute* rightExpander = mod->getExpander(false);

		// check if already controlled, or neighbor is already controlled
		bool cantControlLeft = mod->isControlledLeft() ? true : leftExpander && leftExpander->isControlledLeft();
		bool cantControlRight = mod->isControlledRight() ? true : rightExpander && rightExpander->isControlledRight();

		menu->addChild(createMenuItem("Toggle Left Expander", mod->expanderLeft ? "On" : "Off",[=]() {
			mod->expanderLeft = !mod->expanderLeft;
		}, cantControlLeft));

		menu->addChild(createMenuItem("Toggle Right Expander", mod->expanderRight ? "On" : "Off",[=]() {
			mod->expanderRight = !mod->expanderRight;
		}, cantControlRight));

		menu->addChild(new QuestionableSlider<GlobalOpacityQuantity>(
			[=]() { return mod->lightOpacity; }, 
			[=](float value) { mod->lightOpacity = math::clamp(value); }
		));

		QuestionableWidget::appendContextMenu(menu);

	}

};

Model* modelSyncMute = createModel<SyncMute, SyncMuteWidget>("smute");
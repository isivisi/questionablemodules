#include "plugin.hpp"
#include "imagepanel.cpp"
#include "ui.cpp"
#include "colorBG.hpp"
#include "questionableModule.hpp"
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <random>
#include <mutex>
#include <memory>
#include <queue>
#include <algorithm>
#include <utility>

const int MAX_OUTPUTS = 8;
const int MODULE_SIZE = 22;

// make sure module thread and widget threads cooperate :)
//std::recursive_mutex treeMutex;

struct Scale {
	std::string name;
	std::vector<int> notes;

	// probabilities[note] = {anotherNote: probability, ...}
	std::unordered_map<int, std::unordered_map<int, float>> probabilities;

	Scale() { }
	
	Scale(std::string name, std::vector<int> notes) {
		this->name = name;
		this->notes = notes;
	}

	std::unordered_map<int, std::unordered_map<int, float>> getProbabilities() {
		if (probabilities.empty()) {
			probabilities.reserve(notes.size());
			for (size_t i = 0; i < notes.size(); i++) {
				for (size_t x = 0; x < notes.size(); x++) {
					probabilities[i][x] = randomReal<float>();
				}
			}
		}
		return probabilities;
	}

	std::vector<int> generateRandom(int size) {
		std::vector<int> sequence;

		for (int i = 0; i < size; i++) {
			sequence.push_back(getNextInSequence(sequence, size));
		}

		return sequence;
	}

	int getNextInSequence(std::vector<int> sequence, int maxSize=1) {
		int offset = 0;

		/*auto probs = getProbabilities();

		std::vector<float> cumulativeProbs;
		cumulativeProbs.resize(notes.size());
		float maxVal = 0.f;
		for (size_t i = 0; i < sequence.size(); i++) {
			for (auto& notePobPair : probs[sequence[i]%12]) cumulativeProbs[notePobPair.first] += notePobPair.second;
			maxVal = cumulativeProbs[i] > maxVal ? cumulativeProbs[i] : maxVal;
		}

		float rand = randFloat(maxVal);

		int note = 0;
		for(size_t i = 0; i < cumulativeProbs.size(); i++) {
			if (rand <= cumulativeProbs[i]) note = notes[i];
		}
		return note;*/

		//int lookback = randomInt<int>(8, 16);
		//int front = sequence.size() >= lookback ? sequence[(sequence.size())-lookback] :  noteToOffset(sequence.front());

		// convert back to offset values
		int front = noteToOffset(sequence.front());
		int back = noteToOffset(sequence.back());

		if (!sequence.size()) offset = randomInt<int>(0, maxSize);
		else {
			int randomType = randomInt<int>(0, 3);
			switch (randomType) {
				case 0: // move anywere
					offset = back + randomInt<int>(-notes.size(), notes.size());
					break;
				case 1: // move a 3rd
					offset = (randomInt<int>(0,1) ? back+3 : back-3);// * std::max(1, (int)(back / 12));
					break;
				case 2: // move a 5th
					offset = (randomInt<int>(0,1) ? back+5 : back-5);// * std::max(1, (int)(back / 12));
					break;
				case 3: // move a 7th
					offset = (randomInt<int>(0,1) ? back+7 : back-7);// * std::max(1, (int)(back / 12));
					break;
			}
		}

		// keep root octave or not
		if (randomReal<float>(0,1) < 0.5) {
			int relativeOctDiff = notes.size() * (relativeOctave(front) - relativeOctave(offset));
			offset += relativeOctDiff;
		}

		return offsetToNote(offset);
	}

	static std::string getNoteString(int note, bool includeOctaveOffset=false) {
		std::string noteStrings[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
		if (includeOctaveOffset) return noteStrings[abs((note % 12 + 12) % 12)] + std::to_string((int)std::floor(((float)note/12.f)+4));
		else return noteStrings[abs((note % 12 + 12) % 12)];
	}

	// get absolute notes octave
	static int noteOctave(int note) { 
		return (int)std::floor(((float)note/12.f));
	}

	int relativeOctave(int offset) {
		return (int)std::floor((float)offset/(float)notes.size());
	}
	
	// convert absolute note to scale position offset
	int noteToOffset(int note) {
		int relativeOctave = (int)notes.size() * noteOctave(note);
		//relativeOctave = relativeOctave < 0 ? relativeOctave-1 : relativeOctave > 0 ? relativeOctave+1 : relativeOctave;
		for (size_t i = 0; i < notes.size(); i++) {
			if (notes[i] == abs(note%12)) return i + relativeOctave;
		}
		return 0;
	}

	// convert scale offset to absolute note
	int offsetToNote(int offset) {
		int absOctave = 12 * relativeOctave(offset);
		//absOctave = absOctave < 0 ? absOctave-1 : absOctave > 0 ? absOctave+1 : absOctave;
		return notes[(offset%notes.size() + notes.size()) % notes.size()] + absOctave;
	}

	Scale getTransposedBy(int note) {
		Scale newScale;
		newScale.name = Scale::getNoteString(note) + std::string(" ") + name;
		for (size_t i = 0; i < notes.size(); i++) {
			newScale.notes.push_back(notes[i] + note);
		}
		return newScale;
	}

	bool operator==(const std::string other) {
		return name == other;
	}
};

// All starting at C
std::vector<Scale> scales = {
	Scale{"Chromatic", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}}, // C(0), C#(1), D(2), D#(3), E(4), F(5), F#(6), G(7), G#(8), A(9), A#(10), B(11)
	Scale{"Major", {0, 2, 4, 5, 7, 9, 11}}, // C, D, E, F, G, A, B
	Scale{"Minor", {0, 2, 3, 5, 7, 8, 10}}, // C, D, D#, F, G, G#, A#
	Scale{"Pentatonic", {0, 2, 5, 7, 10}}, // C, D, F, G, A#
	Scale{"Blues", {0, 3, 5, 6, 7, 10}}, // C, D#, F, F#, G, A#
	Scale{"Dorian", {0, 2, 3, 5, 7, 9, 10}}, // C, D, D#, F, G, A, A#
	Scale{"Mixolydian", {0, 2, 4, 5, 7, 9, 10}} // C, D, E, F, G, A, A#
};

static Scale getScale(std::string scaleName) {
	auto found = std::find(scales.begin(), scales.end(), scaleName);
	if (found != scales.end()) return *found;
	return Scale{"None", {}};
}

// A node in the tree
struct Node {
  	int output = 0;
	bool enabled = false;
	float chance = 0.5;
	Node* parent;
	int depth = 0;
  	std::vector<Node*> children;

	Rect box;

	Node(Node* p = nullptr, int out = randomInt<int>(-1, 7), float c = randomReal<float>(0, 0.9f)) {
		output = out;
		chance = c;
		parent = p;
		if (parent) depth = parent->depth + 1;
	}

	Node(json_t* json, Node* p=nullptr) {
		parent = p;
		if (parent) depth = parent->depth + 1;
		fromJson(json);
	}

	~Node() {
		for (size_t i = 0; i < children.size(); i++) delete children[i];
	}

	void setOutput(int out) {
		
		output = out;
	}

	void setChance(float ch) {
		
		chance = std::min(0.9f, std::max(0.1f, ch)); 
	}

	void setenabled(bool en) {
		
		enabled = en;
	}

	int getOutput() { 
		
		return output; 
	}

	float getChance() {
		
		return chance; 
	}

	bool getEnabled() {
		
		return enabled; 
	}

	std::vector<Node*> getChildren() {
		
		return children;
	}

	std::vector<int> getHistory() {
		if (!parent) return {output};
		std::vector history = parent->getHistory();
		history.push_back(output);
		return history;
	}

	// Fill each Node with 2 other nodes until depth is met
	// assumes EMPTY
  	void fillToDepth(int desiredDepth) {
		if (desiredDepth <= 0) return;

		Node* child1 = new Node(this);
		Node* child2 = new Node(this);
		child1->fillToDepth(desiredDepth-1);
		child2->fillToDepth(desiredDepth-1);

		children.push_back(child1);
		children.push_back(child2);
  	}

	Node* addChild(int out = randomInt<int>(-1, 7), float c = randomReal<float>(0, 0.9f)) {
		if (children.size() > 1) return nullptr;

		Node* child = new Node(this);
		child->parent = this;
		child->chance = c;
		child->output = out;
		children.push_back(child);

		return child;

	}

	void remove() {
		if (!parent) return;
		parent->children.erase(std::find(parent->children.begin(), parent->children.end(), this));
		delete this;
	}

	int maxDepth() {
		if (children.size() == 0) return 0;

		std::vector<int> sizes;
		for (size_t i = 0; i < children.size(); i++) {
			sizes.push_back(children[i]->maxDepth() + 1);
		}
		return *std::max_element(sizes.begin(), sizes.end());
	}

	void generateSequencesToDepth(Scale s, int d, std::vector<int> history=std::vector<int>()) {
		if (history.empty()) history = getHistory();
		if (d <= 0) return;
		if (depth >= 21) return;
		

		if (randomReal<float>() > 0.9) return;

		if (Node* child1 = addChild()) {
			std::vector<int> c1History = history;
			child1->output = s.getNextInSequence(history, 48);
			c1History.push_back(child1->output);
			child1->generateSequencesToDepth(s, d-1, c1History);
		}

		if (randomReal<float>() > 0.9) return;
		
		if (Node* child2 = addChild()) {
			std::vector<int> c2History = history;
			child2->output = s.getNextInSequence(history, 48);
			c2History.push_back(child2->output);
			child2->generateSequencesToDepth(s, d-1, c2History);
		}
	}

	json_t* toJson() {
		json_t* nodeJ = json_object();

		json_object_set_new(nodeJ, "output", json_integer(output));
		json_object_set_new(nodeJ, "chance", json_real(chance));
		
		json_t *childrenArray = json_array();
		json_object_set_new(nodeJ, "children", childrenArray);

		for (size_t i = 0; i < children.size(); i++) {
			json_array_append_new(childrenArray, children[i]->toJson());
		}

		return nodeJ;
	}

	void fromJson(json_t* json) {

		if (json_t* out = json_object_get(json, "output")) output = json_integer_value(out);
		if (json_t* c = json_object_get(json, "chance")) chance = json_real_value(c);

		if (json_t* arr = json_object_get(json, "children")) {

			for (size_t i = 0; i < json_array_size(arr); i++) {
				children.push_back(new Node(json_array_get(arr, i), this));
			}

		}

	}

};

/*for (const auto& child : node->children) {
	
}*/

struct Treequencer : QuestionableModule {
	enum ParamId {
		FADE_PARAM,
		TRIGGER_TYPE,
		BOUNCE,
		CHANCE_MOD,
		HOLD,
		PARAMS_LEN
	};
	enum InputId {
		GATE_IN_1,
		CLOCK,
		RESET,
		CHANCE_MOD_INPUT,
		HOLD_INPUT,
		TTYPE_GATE,
		BOUNCE_GATE,
		INPUTS_LEN
	};
	enum OutputId {
		SEQ_OUT_1,
		SEQ_OUT_2,
		SEQ_OUT_3,
		SEQ_OUT_4,
		SEQ_OUT_5,
		SEQ_OUT_6,
		SEQ_OUT_7,
		SEQ_OUT_8,
		ALL_OUT,
		SEQUENCE_COMPLETE,
		OUTPUTS_LEN
	};
	enum LightId {
		VOLTAGE_IN_LIGHT_1,
		BLINK_LIGHT,
		TRIGGER_LIGHT,
		BOUNCE_LIGHT,
		HOLD_LIGHT,
		LIGHTS_LEN
	};

	std::queue<std::function<void()>> audioThreadQueue;

	// json data for screen
	float startScreenScale = 12.9f;
	float startOffsetX = 12.5f;
	float startOffsetY = -11.f;
	int colorMode = userSettings.getSetting<int>("treequencerScreenColor");
	int noteRepresentation = 2;
	bool followNodes = false;
	std::string defaultScale = "Pentatonic"; // scale for new node gen
	bool clockInPhasorMode = false;

	bool isDirty = true;
	bool bouncing = false;

	dsp::SchmittTrigger gateTrigger;
	dsp::SchmittTrigger clockTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::SchmittTrigger holdTrigger;
	dsp::SchmittTrigger typeTrigger;
	dsp::SchmittTrigger bounceTrigger;
	dsp::PulseGenerator pulse;
	dsp::PulseGenerator sequencePulse;

	Node rootNode;
	Node* activeNode;
	std::vector<Node*> activeSequence;
	size_t historyPos = 0;
	std::vector<json_t*> history;
	bool historyDirty = true;

	void onAudioThread(std::function<void()> func) {
		audioThreadQueue.push(func);
	}

	void processOffThreadQueue() {
		while (!audioThreadQueue.empty()) {
			audioThreadQueue.front()();
			audioThreadQueue.pop();
		}
	}

	void pushHistory(json_t* state = nullptr) {
		if (historyPos != history.size()) history.erase(history.begin() + historyPos-1, history.end());
		history.push_back(state ? state : rootNode.toJson());
		historyPos = history.size();
		historyDirty = true; // assume new data not saved after this function is called
	}

	void historyGoBack() {
		if (history.empty()) return;
		if (historyPos == history.size() && historyDirty) { // save latest changes before going back
			pushHistory();
			historyDirty = false;
		}
		historyPos = clamp(historyPos-1, 1, history.size());
		setRootNodeFromJson(history[historyPos-1]);
	}

	void historyGoForward() {
		if (historyPos >= history.size()) return;
		historyPos = clamp(historyPos+1, 1, history.size());
		setRootNodeFromJson(history[historyPos-1]);
	}

	// resets the root and loads a tree from json
	void setRootNodeFromJson(json_t* json) {
		rootNode.children.clear();
		activeNode = &rootNode;
		rootNode.fromJson(json);
		isDirty = true;
	}

	enum TrigType {
		STEP,
		SEQUENCE,
	};

	Treequencer() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configInput(GATE_IN_1, "Gate");
		configInput(CLOCK, "Clock/Phasor");
		configInput(RESET, "Reset");
		configInput(CHANCE_MOD_INPUT, "Chance Mod VC");
		configInput(BOUNCE_GATE, "Bounce Gate");
		configInput(HOLD_INPUT, "Hold Gate");
		configInput(TTYPE_GATE, "Trigger Type Gate");
		configSwitch(TRIGGER_TYPE, TrigType::STEP, TrigType::SEQUENCE, TrigType::STEP, "Trigger Type", {"Step", "Sequence"});
		configSwitch(BOUNCE, 0.f, 1.f, 0.f, "Bounce", {"Off", "On"});
		configParam(CHANCE_MOD, -1.f, 1.f, 0.0f, "Chance Mod");
		configSwitch(HOLD, 0.f, 1.f, 0.f, "Hold", {"Off", "On"});
		configOutput(SEQ_OUT_1, "Sequence 1");
		configOutput(SEQ_OUT_2, "Sequence 2");
		configOutput(SEQ_OUT_3, "Sequence 3");
		configOutput(SEQ_OUT_4, "Sequence 4");
		configOutput(SEQ_OUT_5, "Sequence 5");
		configOutput(SEQ_OUT_6, "Sequence 6");
		configOutput(SEQ_OUT_7, "Sequence 7");
		configOutput(SEQ_OUT_8, "Sequence 8");
		configOutput(SEQUENCE_COMPLETE, "Sequence Complete");

		configOutput(ALL_OUT, "VOct");
		
		rootNode.fillToDepth(1);
		
		rootNode.enabled = true;
		activeNode = &rootNode;
		
	}

	float fclamp(float min, float max, float value) {
		return std::min(min, std::max(max, value));
	}

	void resetActiveNode() {
		activeNode->enabled = false;
		activeNode = &rootNode;
		activeNode->enabled = true;
	}

	std::vector<Node*> getWholeSequence(Node* node, std::vector<Node*> sequence = std::vector<Node*>()) {
		sequence.push_back(node);
		if (node->children.size() > 1) {
			float r = randomReal<float>();
			float chance = std::min(1.f, std::max(0.0f, node->chance - getChanceMod()));
			Node* foundNode = node->children[r < chance ? 0 : 1];
			return getWholeSequence(foundNode, sequence);
		} else if(node->children.size()) {
			Node* foundNode = node->children[0];
			return getWholeSequence(foundNode, sequence);
		} else {
			return sequence;
		}
	}

	float getChanceMod() {
		return params[CHANCE_MOD].getValue() + inputs[CHANCE_MOD_INPUT].getVoltage();
	}

	void processGateStep() {
		if (!bouncing) {
			if (!activeNode->children.size()) {
				if (params[BOUNCE].getValue()) {
					bouncing = true;
					if (activeNode->parent) activeNode = activeNode->parent;
				}
				else activeNode = &rootNode;
				sequencePulse.trigger(1e-3f); // signal sequence completed
			}
			else {
				if (activeNode->children.size() > 1) {
					float r = randomReal<float>();
					float chance = std::min(1.f, std::max(0.0f, activeNode->chance - getChanceMod()));
					activeNode = activeNode->children[r < chance ? 0 : 1];
				} else {
					activeNode = activeNode->children[0];
				}
			}
		} else {
			if (activeNode->parent) activeNode = activeNode->parent;
			else {
				bouncing = false;
				if (activeNode->children.size()) processGateStep();
			}
		}
	}

	int sequencePos = 0; // keep this signed, the current logic assumes it.
	void processSequence(bool newSequence = false) {
		bool lastBounce = bouncing;
		if (newSequence) {
			activeSequence = getWholeSequence(&rootNode);
			activeNode = &rootNode;
			sequencePos = 0;
		} else {
			if (!activeSequence.size()) processSequence(true);
			activeNode->enabled = false;
			sequencePos += bouncing ? -1 : 1;
			if (sequencePos <= 0) {
				if (bouncing) sequencePulse.trigger(1e-3f); // signal sequence completed
				bouncing = false;
				sequencePos = 0;
			} else if (sequencePos >= (int)activeSequence.size()) {
				if (params[BOUNCE].getValue()) {
					bouncing = true;
					sequencePos--;
				} else sequencePos = 0;
				if (!bouncing) sequencePulse.trigger(1e-3f); // signal sequence completed
			}
			activeNode = activeSequence[sequencePos];
			activeNode->enabled = true;
		}

		if (!lastBounce && (lastBounce != bouncing)) processSequence();
	}

	void process(const ProcessArgs& args) override {

		processOffThreadQueue();

		bool reset = resetTrigger.process(inputs[RESET].getVoltage(), 0.1f, 2.f);
		bool isGateTriggered = !params[HOLD].getValue() && gateTrigger.process(inputs[GATE_IN_1].getVoltage(), 0.1f, 2.f);
		bool isClockTriggered = !params[HOLD].getValue() && !clockInPhasorMode && clockTrigger.process(inputs[CLOCK].getVoltage(), 0.1f, 2.f);
		bool holdSwap = holdTrigger.process(inputs[HOLD_INPUT].getVoltage(), 0.1, 2.f);
		bool ttypeSwap = typeTrigger.process(inputs[TTYPE_GATE].getVoltage(), 0.1, 2.f);
		bool bounceSwap = bounceTrigger.process(inputs[BOUNCE_GATE].getVoltage(), 0.1, 2.f);

		if (clockInPhasorMode && params[TRIGGER_TYPE].getValue() == TrigType::STEP) params[TRIGGER_TYPE].setValue(TrigType::SEQUENCE);

		lights[TRIGGER_LIGHT].setBrightness(params[TRIGGER_TYPE].getValue());
		lights[BOUNCE_LIGHT].setBrightness(params[BOUNCE].getValue());
		lights[HOLD_LIGHT].setBrightness(params[HOLD].getValue());

		if (holdSwap) params[HOLD].setValue(!params[HOLD].getValue());
		if (ttypeSwap) params[TRIGGER_TYPE].setValue(!params[TRIGGER_TYPE].getValue());
		if (bounceSwap) params[BOUNCE].setValue(!params[BOUNCE].getValue());
		if (!params[BOUNCE].getValue()) bouncing = false;

		bool seqTrigger = params[TRIGGER_TYPE].getValue();

		if (reset) {
			resetActiveNode();
			sequencePos = 0;
			isGateTriggered = false;
		}

		if (!seqTrigger && activeSequence.size()) activeSequence.clear();
		else if (seqTrigger && activeSequence.empty()) activeSequence = getWholeSequence(&rootNode);

		if (!seqTrigger) isGateTriggered = isGateTriggered || isClockTriggered;

		if (isGateTriggered && activeNode) {
			activeNode->enabled = false;

			if (params[TRIGGER_TYPE].getValue()) processSequence(true);
			else processGateStep();

			activeNode->enabled = true;
			pulse.trigger(1e-3f);
		}
		if (isClockTriggered && seqTrigger) {
			processSequence();
			activeNode->enabled = true;
			pulse.trigger(1e-3f);
		}

		// Phasor mode
		if (clockInPhasorMode) {
			if (activeNode) activeNode->enabled = false;
			size_t position = clamp<size_t>(0, activeSequence.size()-1, (inputs[CLOCK].getVoltage() / 10.f) * (float)activeSequence.size());
			activeNode = activeSequence[position];
			activeNode->enabled = true;
		}

		bool activeP = pulse.process(args.sampleTime);
		bool sequenceP = sequencePulse.process(args.sampleTime);

		outputs[SEQUENCE_COMPLETE].setVoltage(sequenceP ? 10.f : 0.0f);


		if (activeNode->output < 8 && activeNode->output >= 0) outputs[activeNode->output].setVoltage(activeP ? 10.f : 0.0f);
		outputs[ALL_OUT].setVoltage((float)activeNode->output/12);

	}

	json_t* dataToJson() override {
		json_t* rootJ = QuestionableModule::dataToJson();
		json_object_set_new(rootJ, "startScreenScale", json_real(startScreenScale));
		json_object_set_new(rootJ, "startOffsetX", json_real(startOffsetX));
		json_object_set_new(rootJ, "startOffsetY", json_real(startOffsetY));
		json_object_set_new(rootJ, "colorMode", json_integer(colorMode));
		json_object_set_new(rootJ, "noteRepresentation", json_integer(noteRepresentation));
		json_object_set_new(rootJ, "followNodes", json_boolean(followNodes));
		json_object_set_new(rootJ, "defaultScale", json_string(defaultScale.c_str()));
		json_object_set_new(rootJ, "clockInPhasorMode", json_boolean(clockInPhasorMode));
		json_object_set_new(rootJ, "rootNode", rootNode.toJson());

		return rootJ;
	}


	void dataFromJson(json_t* rootJ) override {
		QuestionableModule::dataFromJson(rootJ);

		if (json_t* sss = json_object_get(rootJ, "startScreenScale")) startScreenScale = json_real_value(sss);
		if (json_t* sx = json_object_get(rootJ, "startOffsetX")) startOffsetX = json_real_value(sx);
		if (json_t* sy = json_object_get(rootJ, "startOffsetY")) startOffsetY = json_real_value(sy);
		if (json_t* cbm = json_object_get(rootJ, "colorMode")) colorMode = json_integer_value(cbm);
		if (json_t* fn = json_object_get(rootJ, "followNodes")) followNodes = json_boolean_value(fn);
		if (json_t* ds = json_object_get(rootJ, "defaultScale")) defaultScale = json_string_value(ds);
		if (json_t* fm = json_object_get(rootJ, "clockInPhasorMode")) clockInPhasorMode = json_boolean_value(fm);

		if (json_t* nr = json_object_get(rootJ, "noteRepresentation")) noteRepresentation = json_integer_value(nr);
		else noteRepresentation = 0; // preserve previous users visuals

		if (json_t* s = json_object_get(rootJ, "theme")) theme = json_string_value(s);

		if (json_t* rn = json_object_get(rootJ, "rootNode")) setRootNodeFromJson(rn);

	}


};

struct NodeChanceQuantity : QuestionableQuantity {

	NodeChanceQuantity(quantityGetFunc g, quantitySetFunc s) : QuestionableQuantity(g, s) { }

	float getDefaultValue() override {
		return 0.0f;
	}

	float getDisplayValue() override {
		return getValue() * 100;
	}

	void setDisplayValue(float displayValue) override {
		setValue(displayValue / 100);
	}

	std::string getUnit() override {
		return "%";
	}

	std::string getLabel() override {
		return "Chance";
	}

	int getDisplayPrecision() override {
		return 3;
	}

};

struct NodeChanceSlider : ui::Slider {
	NodeChanceSlider(quantityGetFunc valueGet, quantitySetFunc valueSet) {
		quantity = new NodeChanceQuantity(valueGet, valueSet);
		box.size.x = 150.0;
	}
	~NodeChanceSlider() {
		delete quantity;
	}
};

struct TreequencerButton : SvgButton {
	Treequencer* module;
	std::string icon = "";

	SvgWidget* background = nullptr;
	SvgWidget* iconWidget = nullptr;

	bool disabled = false;
	bool latch = false;

	bool latchState = false;

	TreequencerButton(std::string icon, Vec pos, Treequencer* module) {
		this->module = module;
		box.pos = pos;
		this->icon = icon;
		shadow->opacity = 0;

		addFrame(Svg::load(asset::plugin(pluginInstance, "res/treequencer/button-bg.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/treequencer/button-bg-clicked.svg")));

		iconWidget = new SvgWidget();
		iconWidget->setSvg(Svg::load(asset::plugin(pluginInstance, icon)));
		addChild(iconWidget);

	}

	void draw(const DrawArgs &args) override {
		nvgSave(args.vg);
		if (disabled) nvgTint(args.vg, nvgRGB(180,180,180));
		SvgButton::draw(args);
		iconWidget->draw(args);
		nvgRestore(args.vg);
	}

	void onDragStart(const DragStartEvent& e) override {
		if (latch) {
			setLatchState(!latchState);
		} else SvgButton::onDragStart(e);
	}

	void onDragEnd(const DragEndEvent& e) override {
		if (!latch) SvgButton::onDragEnd(e);
	}

	void setLatchState(bool state) {
		latchState = state;
		sw->setSvg(frames[state]);
		fb->setDirty();
	}

};

struct TreequencerHistoryButton : TreequencerButton {
	bool isBack = false;

	TreequencerHistoryButton(bool isBack, Vec pos, Treequencer* module) : TreequencerButton(isBack ? "res/treequencer/back.svg" : "res/treequencer/forward.svg", pos, module) {
		this->isBack = isBack;
	}

	void step() override {
		if (!module) return;
		disabled = isBack ? module->history.size() == 0 || module->historyPos < 1 : module->historyPos >= module->history.size();
	}
	
	void onButton(const ButtonEvent& e) override {
		OpaqueWidget::onButton(e);
		if (!module) return;

		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			if (isBack) module->historyGoBack();
			else module->historyGoForward();
		}
	}

};

struct TreequencerFollowButton : TreequencerButton {

	TreequencerFollowButton(Vec pos, Treequencer* module) : TreequencerButton("res/treequencer/follow.svg", pos, module) {
		latch = true;
		setLatchState(module ? module->followNodes : false);
	}

	void step() override {
		if (!module) return;
		disabled = !module->followNodes;
	}
	
	void onButton(const ButtonEvent& e) override {
		OpaqueWidget::onButton(e);
		if (!module) return;

		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			module->followNodes = !module->followNodes;
		}
	}

};

template <typename T>
struct TreequencerClockPhasorComboPort : QuestionablePort<T> {
	ColorBG* background = nullptr;
	static_assert(std::is_base_of<PortWidget, T>::value, "T must inherit from PortWidget");

	void appendContextMenu(Menu* menu) override {
		if (!this->module || !background) return;
		Treequencer* mod = (Treequencer*)this->module;
		menu->addChild(createMenuItem(mod->clockInPhasorMode ? "Switch to Clock" : "Switch to Phaser", "", [=]() {
			mod->clockInPhasorMode = !mod->clockInPhasorMode;
			background->updateText(3, mod->clockInPhasorMode ? "PHASOR" : "CLOCK");
		}));
		QuestionablePort<T>::appendContextMenu(menu);
	}
};

struct NodeDisplay : Widget {
	Treequencer* module;

	enum NoteRep {
		SQUARES,
		LETTERS,
		NUMBERS,
	};

	const float NODE_SIZE = 25;

	float xOffset = 25;
	float yOffset = 0;

	float dragX = 0;
	float dragY = 0;

	float screenScale = 3.5f;

	bool dirtyRender = true;

	Node* lastActive = nullptr;

	NodeDisplay() {

	}

	void resetScreenPosition() {
		xOffset = 25;
		yOffset = 0;
		dragX = 0;
		dragY = 0;
		screenScale = 3.5f;
	}

	void renderStateDirty() {
		dirtyRender = true;
	}

	void renderStateClean() {
		dirtyRender = false;
		module->isDirty = false;
	}

	bool isRenderStateDirty() {
		return dirtyRender || module->isDirty;
	}

	// AABB
	bool isInsideBox(Vec pos, Rect box){
		// Check if the point is inside the x-bounds of the box
		if (pos.x < box.pos.x || pos.x > box.pos.x + box.size.x)
		{
			return false;
		}

		// Check if the point is inside the y-bounds of the box
		if (pos.y < box.pos.y || pos.y > box.pos.y + box.size.y)
		{
			return false;
		}

		// If the point is inside both the x and y bounds of the box, it is inside the box
		return true;
	}

	void createContextMenuForNode(Node* node) {
		if (!node) return;

		Treequencer* mod = module;

		auto menu = rack::createMenu<QuestionableMenu>();

		int oldNodeOutput = node->output;
		float oldNodeChance = node->chance;
		json_t* prevState = mod->rootNode.toJson();
		menu->onDestruct = [=](){
			if (oldNodeOutput != node->output) mod->pushHistory(prevState);
			if (oldNodeChance != node->chance) mod->pushHistory(prevState);
		};

		menu->addChild(rack::createMenuLabel("Node Output:"));

		ui::TextField* outparam = new QuestionableTextField([=](std::string text) {
			if (text.length() < 4 && isInteger(text)) mod->onAudioThread([=](){ node->setOutput(std::stoi(text)-1); });
		});
		outparam->box.size.x = 100;
		outparam->text = std::to_string(node->output + 1);
		menu->addChild(outparam);

		menu->addChild(rack::createMenuLabel("Node Chance:"));

		NodeChanceSlider* param = new NodeChanceSlider(
			[=]() { return node->getChance(); }, 
			[=](float value) { mod->onAudioThread([=](){ node->setChance(value);}); }
		);
		menu->addChild(param);

		menu->addChild(createMenuItem("Preview", "", [=]() { 
			mod->onAudioThread([=](){
				node->enabled = true;
				mod->activeNode->enabled = false;
				mod->activeNode = node;
				mod->params[Treequencer::HOLD].setValue(1.f);
			});
		}));

		if (node->children.size() < 2 && node->depth < 21) menu->addChild(createMenuItem("Add Child", "", [=]() { 
			mod->onAudioThread([=](){
				mod->pushHistory();
				node->addChild(getScale(mod->defaultScale).getNextInSequence(node->getHistory())); 
				renderStateDirty();
			});
		}));

		if (node != &module->rootNode) menu->addChild(createMenuItem("Remove", "", [=]() {
			mod->onAudioThread([=](){
				mod->pushHistory();
				mod->resetActiveNode();
				node->remove();
				renderStateDirty();
			});
		}));

		menu->addChild(rack::createSubmenuItem("Generate Sequence", "", [=](ui::Menu* menu) {
			for (size_t i = 0; i < scales.size(); i++) {
				menu->addChild(createMenuItem(scales[i].name, "",[=]() {
					mod->onAudioThread([=]() { 
						mod->pushHistory();
						node->generateSequencesToDepth(scales[i], 8);
						renderStateDirty();
					});
				}));
			}
		}));
		
	}

	Node* findNodeClicked(Vec mp, Node* node) {
		if (!node) return nullptr;

		if (isInsideBox(mp, node->box)) return node;

		for (size_t i = 0; i < node->children.size(); i++)
		{
			Node* found = findNodeClicked(mp, node->children[i]);
			if (found) return found;
		}

		return nullptr;
	}

	void onButton(const event::Button &e) override {
		if (e.action == GLFW_PRESS) {
			e.consume(this);
			Vec mousePos = e.pos / screenScale;

			if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
				Node* foundNode = findNodeClicked(mousePos, &module->rootNode);

				if (foundNode) {
					createContextMenuForNode(foundNode);
				}
			}
			
		}
	}

	void onDragStart(const event::DragStart &e) override {
		dragX = APP->scene->rack->getMousePos().x;
		dragY = APP->scene->rack->getMousePos().y;
	}

	void onDragMove(const event::DragMove &e) override {
		float newDragX = APP->scene->rack->getMousePos().x;
		float newDragY = APP->scene->rack->getMousePos().y;

		xOffset += (newDragX - dragX) / screenScale;
		yOffset += (newDragY - dragY) / screenScale;
		
		module->startOffsetX = xOffset;
		module->startOffsetY = yOffset;

		dragX = newDragX;
		dragY = newDragY;

		if (module) module->followNodes = false;
	}

	void onHoverScroll(const HoverScrollEvent& e) override {
		e.consume(this);
		float posX = e.pos.x;
		float posY = e.pos.y;
		float oldScreenScale = screenScale;

		screenScale += (e.scrollDelta.y * screenScale) / 256.f;
		module->startScreenScale = screenScale;

		xOffset = xOffset - ((posX / oldScreenScale) - (posX / screenScale));
		yOffset = yOffset - ((posY / oldScreenScale) - (posY / screenScale));
	}

	// https://personal.sron.nl/~pault/
	const NVGcolor octColors[4][5] = {
		{ // Tol Light
			nvgRGB(238,136,102), // orange
			nvgRGB(153,221,255), // cyan
			nvgRGB(119,170,221), // blue
			nvgRGB(255,170,187), // pink
			nvgRGB(170,170,0) // olive
		},
		{ // Tol Vibrant
			nvgRGB(238,119,51), // orange
			nvgRGB(51,187,82), // cyan
			nvgRGB(0,119,187), // blue
			nvgRGB(238,51,119), // magenta
			nvgRGB(187,187,187) // grey
		},
		{ // Tol Muted
			nvgRGB(204,102,119), // rose
			nvgRGB(136,204,238), // cyan
			nvgRGB(170,68,153), // purple
			nvgRGB(136,34,85), // wine
			nvgRGB(221,204,119) // sand
		},
		{ // White
			nvgRGB(200,200,200),
			nvgRGB(200,200,200),
			nvgRGB(200,200,200),
			nvgRGB(200,200,200),
			nvgRGB(200,200,200),
		}
	};

	const NVGcolor activeColor[4] = {
		nvgRGB(68,187,153),
		nvgRGB(0,153,136),
		nvgRGB(68,170,153),
		nvgRGB(68,187,153)
	};

	void drawNode(NVGcontext* vg, Node* node, float x, float y,  float scale) {
		
		float xVal = x + xOffset;
		float yVal = y + yOffset;
		float xSize = NODE_SIZE * scale;
		float ySize = NODE_SIZE * scale;

		int octOffset = (node->output+1) / 12;

		bool inSequence = std::find(module->activeSequence.begin(), module->activeSequence.end(), node) != module->activeSequence.end();

		if (module->params[Treequencer::TRIGGER_TYPE].getValue() && !inSequence) {
			nvgGlobalAlpha(vg, 0.1);
		}

		// node bg
		nvgFillColor(vg, node->enabled ? activeColor[module->colorMode] : octColors[module->colorMode][abs(octOffset%5)]);
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, NODE_SIZE, NODE_SIZE);
		nvgFill(vg);

		// update pos for buttonclicking
		node->box.pos = Vec(xVal, yVal);
		node->box.size = Vec(xSize, ySize);

		// grid
		if (module->noteRepresentation == NoteRep::SQUARES) {
			for (int i = 0; i < abs((node->output+1) % 12); i++) {

				float boxX = 3 + (3.57 * (i%3));
				float boxY = 3 + (3.57 * floor(i/3));

				nvgFillColor(vg, nvgRGB(44,44,44));
				nvgBeginPath(vg);
				nvgRect(vg, boxX, boxY, NODE_SIZE/8, NODE_SIZE/8);
				nvgFill(vg);

			}
		} 
		if (module->noteRepresentation == NoteRep::LETTERS || module->noteRepresentation == NoteRep::NUMBERS) {
			std::shared_ptr<window::Font> font = APP->window->loadFont(asset::plugin(pluginInstance, std::string("res/fonts/OpenSans-Bold.ttf")));
			nvgSave(vg);
			nvgTranslate(vg, 2, 10);
			nvgScale(vg, 0.25, 0.25);
			nvgFontFaceId(vg, font->handle);
			nvgFontSize(vg, 50);
			nvgFillColor(vg, nvgRGB(44,44,44));
			nvgTextAlign(vg, NVGalign::NVG_ALIGN_LEFT);
			if (module->noteRepresentation == NoteRep::LETTERS) nvgText(vg, 3, 3, Scale::getNoteString(node->output, true).c_str(), NULL);
			else nvgText(vg, 0, 0, ((std::signbit(node->output+1) ? std::string("-") : (node->output+1 > 9 ? std::string("") : std::string("0"))) + std::to_string(abs(node->output+1))).c_str(), NULL);
			nvgRestore(vg);
		}
		
		if (node->children.size() > 1) {
			float chance = std::min(1.f, std::max(0.f, node->chance - module->getChanceMod()));

			nvgFillColor(vg, nvgRGB(240,240,240));
			nvgBeginPath(vg);
			nvgRect(vg, ((NODE_SIZE/8) * 7), 0, NODE_SIZE/8, NODE_SIZE);
			nvgFill(vg);
			nvgFillColor(vg, nvgRGB(44,44,44));
			nvgBeginPath(vg);
			nvgRect(vg, ((NODE_SIZE/8) * 7), 0, NODE_SIZE/8, NODE_SIZE * chance);
			nvgFill(vg);
		}

		nvgGlobalAlpha(vg, 1.0);

	}

	inline float calcNodeScale(int nodeCount) { return (NODE_SIZE/nodeCount) / NODE_SIZE; }

	inline float calcNodeYHeight(float scale, int nodePos, int nodeCount) { return NODE_SIZE+(((NODE_SIZE*scale)*nodePos)-(((NODE_SIZE*scale)*nodeCount)/2)); }

	void drawNodes(NVGcontext* vg) {

		for (size_t i = 0; i < nodeCache.size(); i++) {
			if (nodeCache[i].scale * screenScale < 0.01) continue; // cut when too small
			//if ((nodeCache[i].pos.x + xOffset) * screenScale > box.size.x || nodeCache[i].pos.y * screenScale > box.size.y) continue;
			//if (nodeCache[i].pos.x + xOffset + (NODE_SIZE*nodeCache[i].scale*screenScale) < 0 || nodeCache[i].pos.y + (NODE_SIZE*nodeCache[i].scale*screenScale) < 0) continue;

			nvgSave(vg);
			nvgTranslate(vg, nodeCache[i].pos.x + xOffset, nodeCache[i].pos.y + yOffset);
			nvgScale(vg, nodeCache[i].scale, nodeCache[i].scale);
			drawNode(vg, nodeCache[i].node, nodeCache[i].pos.x, nodeCache[i].pos.y, nodeCache[i].scale);
			nvgRestore(vg);
		}
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

		nvgSave(args.vg);
		nvgScissor(args.vg, 0, 0, box.size.x, box.size.y);

		//screenScale = 1 - (xOffset / NODE_SIZE);

		nvgScale(args.vg, screenScale, screenScale);

		if (layer == 1) {

			if (isRenderStateDirty()) {
				int depth = module->rootNode.maxDepth();
				
				// Initialize bins
				nodeBins.clear();
				int amnt = 1;
				for (int i = 0; i < depth+1; i++) {
					nodeBins.push_back(std::vector<Node*>());

					// allocate a full array with nullptrs to help with visuals later
					amnt *= 2;
					nodeBins[i].resize(amnt);
				}

				cacheNodePositions();
				renderStateClean();
			}
			drawNodes(args.vg);

			if (module && module->followNodes && (!lastActive || lastActive != module->activeNode)) {
				NodePosCache activeNode = getActiveCached();
				xOffset = -(activeNode.pos.x - (0.5*activeNode.scale));
				yOffset = -(activeNode.pos.y - (0.5*activeNode.scale));
				screenScale = 7.65/activeNode.scale;
				lastActive = module->activeNode;
			}

		}

		nvgRestore(args.vg);

	}

	std::vector<std::vector<Node*>> nodeBins;
	struct NodePosCache {
		Vec pos;
		float scale;
		Node* node;

		bool operator==(const Node* other) {
			return node == other;
		}
	};
	
	std::vector<NodePosCache> nodeCache;

	void cacheNodePositions() {
		
		gatherNodesForBins(&module->rootNode);
		nodeCache.clear();
		
		float cumulativeX = -25.f;
		for (size_t d = 0; d < nodeBins.size(); d++) {
			int binLen = nodeBins[d].size();
			float scale = calcNodeScale(binLen);
			float prevScale = calcNodeScale(nodeBins[std::max(0, (int)d-1)].size());
			cumulativeX += ((NODE_SIZE+1)*prevScale);
			for(int i = 0; i < binLen; i++) {
				Node* node = nodeBins[d][i];
				float y = calcNodeYHeight(scale, i, binLen);

				if (node) {
					nodeCache.push_back(NodePosCache{Vec(cumulativeX, y), calcNodeScale(binLen), node});
				}
				
			}
		}

	}

	// find the current active node cache data
	NodePosCache getActiveCached() {
		return *std::find(nodeCache.begin(), nodeCache.end(), module->activeNode);
	}

	void gatherNodesForBins(Node* node, int position = 0, int depth = 0) {
		nodeBins[depth][position] = node;

		//nodeBins[depth].push_back(&node);

		for (size_t i = 0; i < node->children.size(); i++) {
			gatherNodesForBins(node->children[i], (position*2)+i, depth+1);
		}

	}

};

struct TreequencerWidget : QuestionableWidget {
	NodeDisplay *display;
	ImagePanel *dirt;

	enum ScreenMode {
		LIGHT,
		VIBRANT,
		MUTED,
		GREYSCALE
	};

	void setText() {
		NVGcolor c = nvgRGB(255,255,255);
		Treequencer* mod = module ? ((Treequencer*)module) : nullptr;
		color->textList.clear();
		color->addText("TREEQUENCER", "OpenSans-ExtraBold.ttf", c, 14, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, 20));
		color->addText("·ISI·", "OpenSans-ExtraBold.ttf", c, 28, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, RACK_GRID_HEIGHT-13));

		color->addText("GATE", "OpenSans-Bold.ttf", c, 7, Vec(30.5, 48), "descriptor"); // 38
		color->addText(mod ? mod->clockInPhasorMode ? "PHASOR" : "CLOCK" : "CLOCK", "OpenSans-Bold.ttf", c, 7, Vec(69, 48), "descriptor");
		color->addText("RESET", "OpenSans-Bold.ttf", c, 7, Vec(108, 48), "descriptor");

		color->addText("SEQ COM", "OpenSans-Bold.ttf", c, 7, Vec(261.5, 48), "descriptor");
		color->addText("VOCT OUT", "OpenSans-Bold.ttf", c, 7, Vec(299.35, 48), "descriptor");

		color->addText("CHANCE MOD", "OpenSans-Bold.ttf", c, 7, Vec(31, 314), "descriptor");

		color->addText("HOLD", "OpenSans-Bold.ttf", c, 7, Vec(223, 314), "descriptor");
		color->addText("BOUNCE", "OpenSans-Bold.ttf", c, 7, Vec(261.35, 314), "descriptor");
		color->addText("TRIG TYPE", "OpenSans-Bold.ttf", c, 7, Vec(299.35, 314), "descriptor");

		color->addText("FOLLOW", "OpenSans-Bold.ttf", c, 7, Vec(135, 285), "descriptor");
		color->addText("UNDO", "OpenSans-Bold.ttf", c, 7, Vec(165, 285), "descriptor");
		color->addText("REDO", "OpenSans-Bold.ttf", c, 7, Vec(185, 285), "descriptor");
		
		bool isNumber = mod ? mod->noteRepresentation != NodeDisplay::NoteRep::LETTERS : true;
		color->addText(isNumber ? "1" : Scale::getNoteString(0, true), "OpenSans-Bold.ttf", c, 7, Vec(30.5, 353), "descriptor");
		color->addText(isNumber ? "2" : Scale::getNoteString(1, true), "OpenSans-Bold.ttf", c, 7, Vec(69, 353), "descriptor");
		color->addText(isNumber ? "3" : Scale::getNoteString(2, true), "OpenSans-Bold.ttf", c, 7, Vec(108, 353), "descriptor");
		color->addText(isNumber ? "4" : Scale::getNoteString(3, true), "OpenSans-Bold.ttf", c, 7, Vec(146, 353), "descriptor");
		color->addText(isNumber ? "5" : Scale::getNoteString(4, true), "OpenSans-Bold.ttf", c, 7, Vec(184.5, 353), "descriptor");
		color->addText(isNumber ? "6" : Scale::getNoteString(5, true), "OpenSans-Bold.ttf", c, 7, Vec(223, 353), "descriptor");
		color->addText(isNumber ? "7" : Scale::getNoteString(6, true), "OpenSans-Bold.ttf", c, 7, Vec(261.35, 353), "descriptor");
		color->addText(isNumber ? "8" : Scale::getNoteString(7, true), "OpenSans-Bold.ttf", c, 7, Vec(299.35, 353), "descriptor");
	}

	TreequencerWidget(Treequencer* module) {
		setModule(module);
		//setPanel(createPanel(asset::plugin(pluginInstance, "res/nrandomizer.svg")));

		backdrop = new ImagePanel();
		backdrop->box.size = Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
		backdrop->imagePath = asset::plugin(pluginInstance, "res/treequencer/treequencer.jpg");
		backdrop->scalar = 3.49;
		backdrop->visible = true;

		display = new NodeDisplay();
		display->box.pos = Vec(2, 50);
		display->box.size = Vec(((MODULE_SIZE -1) * RACK_GRID_WIDTH) + 10, 200);
		display->module = module;
		if (module) {
			display->screenScale = module->startScreenScale;
			display->xOffset = module->startOffsetX;
			display->yOffset = module->startOffsetY;
		}

		dirt = new ImagePanel();
		dirt->box.pos = Vec(2, 50);
		dirt->box.size = Vec(((MODULE_SIZE -1) * RACK_GRID_WIDTH) + 11, 200);
		dirt->imagePath = asset::plugin(pluginInstance, "res/dirt.png");
		dirt->scalar = 3.5;
		dirt->visible = true;

		color = new ColorBG(Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT));
		color->drawBackground = false;
		setText();

		backgroundColorLogic(module);
		
		setPanel(backdrop);
		addChild(color);
		addChild(display);
		addChild(dirt);

		addChild(createQuestionableWidgetCentered(new TreequencerFollowButton(Vec(135, 266), module)));
		addChild(createQuestionableWidgetCentered(new TreequencerHistoryButton(true, Vec(165, 266), module)));
		addChild(createQuestionableWidgetCentered(new TreequencerHistoryButton(false, Vec(185, 266), module)));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(10.319f, 10)), module, Treequencer::GATE_IN_1));
		addInput(createInputCentered<TreequencerClockPhasorComboPort<PJ301MPort>>(mm2px(Vec(23.336f, 10)), module, Treequencer::CLOCK));
		((TreequencerClockPhasorComboPort<PJ301MPort>*)getInput(Treequencer::CLOCK))->background = color;
		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(36.354f, 10)), module, Treequencer::RESET));

		addParam(createLightParamCentered<QuestionableParam<VCVLightLatch<MediumSimpleLight<WhiteLight>>>>(mm2px(Vec(101.441f, 100.f)), module, Treequencer::TRIGGER_TYPE, Treequencer::TRIGGER_LIGHT));
		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(101.441f, 90.f)), module, Treequencer::TTYPE_GATE));

		addParam(createLightParamCentered<QuestionableParam<VCVLightLatch<MediumSimpleLight<WhiteLight>>>>(mm2px(Vec(88.424f, 100.f)), module, Treequencer::BOUNCE, Treequencer::BOUNCE_LIGHT));
		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(88.424f, 90.f)), module, Treequencer::BOUNCE_GATE));

		addParam(createLightParamCentered<QuestionableParam<VCVLightLatch<MediumSimpleLight<WhiteLight>>>>(mm2px(Vec(75.406f, 100.f)), module, Treequencer::HOLD, Treequencer::HOLD_LIGHT));
		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(75.406f, 90.f)), module, Treequencer::HOLD_INPUT));

		addOutput(createOutputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(88.424f, 10.f)), module, Treequencer::SEQUENCE_COMPLETE));
		addOutput(createOutputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(101.441f, 10.f)), module, Treequencer::ALL_OUT));

		for (int i = 0; i < MAX_OUTPUTS; i++) {
			addOutput(createOutputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(10.319f + ((13.0)*float(i)), 113.f)), module, i));
		}

		addParam(createParamCentered<QuestionableParam<RoundSmallBlackKnob>>(mm2px(Vec(10.319f, 100.f)), module, Treequencer::CHANCE_MOD));
		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(10.319f, 90.f)), module, Treequencer::CHANCE_MOD_INPUT));

		//addParam(createParamCentered<QuestionableParam<RoundSmallBlackKnob>>(mm2px(Vec(35.24, 103)), module, Treequencer::FADE_PARAM));
		//addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(35.24, 113)), module, Treequencer::FADE_INPUT));
		
		//addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(10, 10.478  + (10.0*float(i)))), module, i));
		//addOutput(createOutputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(35.24, 10.478  + (10.0*float(i)))), module, i));

		//addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(10, 113)), module, Treequencer::TRIGGER));

		//addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(14.24, 106.713)), module, Treequencer::BLINK_LIGHT));
	}

	void appendContextMenu(Menu *menu) override
  	{
		Treequencer* mod = (Treequencer*)module;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Reset Screen Position", "",[=]() {
			display->resetScreenPosition();
		}));
		
		menu->addChild(createMenuItem("Toggle Follow Nodes", mod->followNodes ? "On" : "Off", [=]() {
			mod->followNodes = !mod->followNodes;
		}));

		menu->addChild(rack::createSubmenuItem("Default Scale", "", [=](ui::Menu* menu) {
			for (size_t i = 0; i < scales.size(); i++) {
				menu->addChild(createMenuItem(scales[i].name, scales[i].name == mod->defaultScale ? "•" : "",[=]() {
						mod->defaultScale = scales[i].name;
				}));
			}
		}));

		menu->addChild(rack::createSubmenuItem("Screen Color Mode", "", [=](ui::Menu* menu) {
			menu->addChild(createMenuItem("Light", mod->colorMode == ScreenMode::LIGHT ? "•" : "",[=]() {
				mod->onAudioThread([=]() { mod->colorMode = ScreenMode::LIGHT; });
				userSettings.setSetting<int>("treequencerScreenColor", ScreenMode::LIGHT);
			}));
			menu->addChild(createMenuItem("Vibrant", mod->colorMode == ScreenMode::VIBRANT ? "•" : "", [=]() {
				mod->onAudioThread([=]() { mod->colorMode = ScreenMode::VIBRANT; });
				userSettings.setSetting<int>("treequencerScreenColor", ScreenMode::VIBRANT);
			}));
			menu->addChild(createMenuItem("Muted", mod->colorMode == ScreenMode::MUTED ? "•" : "", [=]() {
				mod->onAudioThread([=]() { mod->colorMode = ScreenMode::MUTED; });
				userSettings.setSetting<int>("treequencerScreenColor", ScreenMode::MUTED);
			}));
			menu->addChild(createMenuItem("Greyscale", mod->colorMode == ScreenMode::GREYSCALE ? "•" : "", [=]() {
				mod->onAudioThread([=]() { mod->colorMode = ScreenMode::GREYSCALE; });
				userSettings.setSetting<int>("treequencerScreenColor", ScreenMode::GREYSCALE);
			}));
		}));

		menu->addChild(rack::createSubmenuItem("Note Representation", "", [=](ui::Menu* menu) {
			menu->addChild(createMenuItem("Squares", mod->noteRepresentation == NodeDisplay::SQUARES ? "•" : "", [=]() {
				mod->onAudioThread([=]() { 
					mod->noteRepresentation = NodeDisplay::NoteRep::SQUARES; 
					setText();
					setWidgetTheme(mod->theme, false); // fix text color
				});
			}));
			menu->addChild(createMenuItem("Letters", mod->noteRepresentation == NodeDisplay::LETTERS ? "•" : "",[=]() {
				mod->onAudioThread([=]() { 
					mod->noteRepresentation = NodeDisplay::NoteRep::LETTERS; 
					setText();
					setWidgetTheme(mod->theme, false); // fix text color
				});
			}));
			menu->addChild(createMenuItem("Numbers", mod->noteRepresentation == NodeDisplay::NUMBERS ? "•" : "",[=]() {
				mod->onAudioThread([=]() { 
					mod->noteRepresentation = NodeDisplay::NoteRep::NUMBERS; 
					setText();
					setWidgetTheme(mod->theme, false); // fix text color
				});
			}));
		}));

		QuestionableWidget::appendContextMenu(menu);
	}
};

Model* modelTreequencer = createModel<Treequencer, TreequencerWidget>("treequencer");
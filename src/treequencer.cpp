#include "plugin.hpp"
#include "imagepanel.cpp"
#include "textfield.cpp"
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

const int MAX_OUTPUTS = 8;
const int MODULE_SIZE = 22;

const int DEFAULT_NODE_DEPTH = 3;

// make sure module thread and widget threads cooperate :)
//std::recursive_mutex treeMutex;

Vec lerp(Vec& point1, Vec& point2, float t) {
	Vec diff = point2 - point1;
	return point1 + diff * t;
}

float randFloat(float max = 1.f) {
	std::uniform_real_distribution<float> distribution(0, max);
	std::random_device rd;
	return distribution(rd);
}

int randomInteger(int min, int max) {
	std::random_device rd; // obtain a random number from hardware
	std::mt19937 gen(rd()); // seed the generator
	std::uniform_int_distribution<> distr(min, max); // define the range
	return distr(gen);
}

inline bool isInteger(const std::string& s)
{
   if(s.empty() || ((!isdigit(s[0])) && (s[0] != '-') && (s[0] != '+'))) return false;

   char * p;
   strtol(s.c_str(), &p, 10);

   return (*p == 0);
}

bool isNumber(std::string s)
{
	std::size_t char_pos(0);
	// skip the whilespaces 
	char_pos = s.find_first_not_of(' ');
	if (char_pos == s.size()) return false;
	// check the significand 
	if (s[char_pos] == '+' || s[char_pos] == '-') 
		++char_pos; // skip the sign if exist 
	int n_nm, n_pt;
	for (n_nm = 0, n_pt = 0;
		std::isdigit(s[char_pos]) || s[char_pos] == '.';
		++char_pos) {
		s[char_pos] == '.' ? ++n_pt : ++n_nm;
	}
	if (n_pt>1 || n_nm<1) // no more than one point, at least one digit 
		return false;
	// skip the trailing whitespaces 
	while (s[char_pos] == ' ') {
		++ char_pos;
	}
	return char_pos == s.size(); // must reach the ending 0 of the string 
}

struct Scale {
	std::string name;
	std::vector<int> notes;

	// probabilities[note] = {anotherNote: probability, ...}
	//std::map<int, std::map<int, float>> probabilities;

	std::vector<int> generateRandom(int size) {
		std::vector<int> sequence = sequence;

		for (int i = 0; i < size; i++) {
			sequence.push_back(getNextInSequence(sequence, size));
		}

		return sequence;
	}

	int getNextInSequence(std::vector<int> sequence, int maxSize) {
		int offset = 0;

		// convert back to offset values
		int front = toOffset(sequence.front());
		int back = toOffset(sequence.back());

		if (!sequence.size()) offset = randomInteger(0, maxSize);
		else {
			int randomType = randomInteger(0, 3);
			switch (randomType) {
				case 0: // random number nearby
					offset = randomInteger(back-9, back+9);
					break;
				case 1: // move a 3rd
					offset = (randomInteger(0,1) ? front+3 : front-3) * std::max(1, (int)(back / 12));
					break;
				case 2: // move a 5th
					offset = (randomInteger(0,1) ? front+5 : front-5) * std::max(1, (int)(back / 12));
					break;
				case 3: // move a 7th
					offset = (randomInteger(0,1) ? front+7 : front-7) * std::max(1, (int)(back / 12));
					break;
			}
		}
		return notes[offset%notes.size()] * std::max(1, (int)(offset/(int)notes.size()));
	}

	static std::string getNoteString(int note, bool includeOctaveOffset=false) {
		std::string noteStrings[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
		if (includeOctaveOffset) return noteStrings[abs(note%12)] + std::to_string((int)((note/12)+1));
		else return noteStrings[abs(note%12)];
	}
	
	// convert note to position offset
	int toOffset(int note) {
		for (size_t i = 0; i < notes.size(); i++) {
			if (notes[i] == abs(note % (int)notes.size())) return i * (note / (int)notes.size());
		}
		return 0;
	}

	Scale getTransposedBy(int note) {
		Scale newScale;
		newScale.name = Scale::getNoteString(note) + std::string(" ") + name;
		for (size_t i = 0; i < notes.size(); i++) {
			newScale.notes.push_back(notes[i] + note);
		}
		return newScale;
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

// A node in the tree
struct Node {
  	int output = 0;
	bool enabled = false;
	float chance = 0.5;
	Node* parent;
	int depth = 0;
  	std::vector<Node*> children;

	Rect box;

	Node(Node* p = nullptr, int out = randomInteger(-1, 7), float c = randFloat(0.9f)) {
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

	Node* addChild() {
		if (children.size() > 1) return nullptr;

		Node* child = new Node(this);
		child->parent = this;
		child->chance = randFloat(0.9f);
		child->output = randomInteger(-1, 7);
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
		if (d <= 0) return;
		if (depth >= 21) return;

		if (Node* child1 = addChild()) {
			std::vector<int> c1History = history;
			c1History.push_back(output);
			child1->output = s.getNextInSequence(c1History, 48);
			child1->generateSequencesToDepth(s, d-1, c1History);
		}
		
		if (Node* child2 = addChild()) {
			std::vector<int> c2History = history;
			c2History.push_back(output);
			child2->output = s.getNextInSequence(c2History, 48);
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

	void onAudioThread(std::function<void()> func) {
		audioThreadQueue.push(func);
	}

	void processOffThreadQueue() {
		while (!audioThreadQueue.empty()) {
			audioThreadQueue.front()();
			audioThreadQueue.pop();
		}
	}

	Treequencer() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configInput(GATE_IN_1, "Gate");
		configInput(CLOCK, "Clock");
		configInput(RESET, "Reset");
		configInput(CHANCE_MOD_INPUT, "Chance Mod VC");
		configInput(BOUNCE_GATE, "Bounce Gate");
		configInput(HOLD_INPUT, "Hold Gate");
		configInput(TTYPE_GATE, "Trigger Type Gate");
		configSwitch(TRIGGER_TYPE, 0.f, 1.f, 0.f, "Trigger Type", {"Step", "Sequence"});
		configSwitch(BOUNCE, 0.f, 1.f, 0.f, "Bounce", {"Off", "On"});
		configSwitch(CHANCE_MOD, -1.f, 1.f, 0.0f, "Chance Mod");
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
			float r = randFloat();
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
					float r = randFloat();
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

	size_t sequencePos = 0;
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
			} else if (sequencePos >= activeSequence.size()) {
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
		bool isClockTriggered = !params[HOLD].getValue() && clockTrigger.process(inputs[CLOCK].getVoltage(), 0.1f, 2.f);
		bool holdSwap = holdTrigger.process(inputs[HOLD_INPUT].getVoltage(), 0.1, 2.f);
		bool ttypeSwap = typeTrigger.process(inputs[TTYPE_GATE].getVoltage(), 0.1, 2.f);
		bool bounceSwap = bounceTrigger.process(inputs[BOUNCE_GATE].getVoltage(), 0.1, 2.f);

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
		json_object_set_new(rootJ, "rootNode", rootNode.toJson());

		return rootJ;
	}


	void dataFromJson(json_t* rootJ) override {
		QuestionableModule::dataFromJson(rootJ);

		if (json_t* sss = json_object_get(rootJ, "startScreenScale")) startScreenScale = json_real_value(sss);
		if (json_t* sx = json_object_get(rootJ, "startOffsetX")) startOffsetX = json_real_value(sx);
		if (json_t* sy = json_object_get(rootJ, "startOffsetY")) startOffsetY = json_real_value(sy);
		if (json_t* cbm = json_object_get(rootJ, "colorMode")) colorMode = json_integer_value(cbm);

		if (json_t* s = json_object_get(rootJ, "theme")) theme = json_string_value(s);

		if (json_t* rn = json_object_get(rootJ, "rootNode")) {

			rootNode.children.clear();
			activeNode = &rootNode;
			rootNode.fromJson(rn);

			isDirty = true;
		}

	}


};

struct NodeDisplay : Widget {
	Treequencer* module;

	const float NODE_SIZE = 25;

	float xOffset = 25;
	float yOffset = 0;

	float dragX = 0;
	float dragY = 0;

	float screenScale = 3.5f;
	bool followNodes = false;

	bool dirtyRender = true;

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

		auto menu = rack::createMenu();

		menu->addChild(rack::createMenuLabel("Node Output:"));

		ui::TextField* outparam = new QTextField([=](std::string text) {
			if (text.length() < 4 && isInteger(text)) mod->onAudioThread([=](){ node->setOutput(std::stoi(text)-1); });
		});
		outparam->box.size.x = 100;
		outparam->text = std::to_string(node->output + 1);
		menu->addChild(outparam);

		menu->addChild(rack::createMenuLabel("Node Chance:"));

		ui::TextField* param = new QTextField([=](std::string text) { 
			if (text.length() < 4 && isNumber(text)) mod->onAudioThread([=](){ node->setChance((float)::atof(text.c_str())); });
		});
		param->box.size.x = 100;
		param->text = std::to_string(node->chance);
		menu->addChild(param);

		menu->addChild(createMenuItem("Preview", "", [=]() { 
			mod->onAudioThread([=](){
				node->enabled = true;
				mod->activeNode->enabled = false;
				mod->activeNode = node;
				mod->params[Treequencer::HOLD].setValue(1.f);
			});
		}));

		menu->addChild(new MenuSeparator);

		if (node->children.size() < 2 && node->depth < 21) menu->addChild(createMenuItem("Add Child", "", [=]() { 
			mod->onAudioThread([=](){
				node->addChild(); 
				renderStateDirty();
			});
		}));

		if (node != &module->rootNode) menu->addChild(createMenuItem("Remove", "", [=]() {
			mod->onAudioThread([=](){
				mod->resetActiveNode();
				node->remove();
				renderStateDirty();
			});
		}));

		menu->addChild(rack::createSubmenuItem("Generate Sequence", "", [=](ui::Menu* menu) {
			for (size_t i = 0; i < scales.size(); i++) {
				menu->addChild(createMenuItem(scales[i].name, "",[=]() {
					mod->onAudioThread([=]() { 
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

    }

	void onHoverScroll(const HoverScrollEvent& e) override {
		e.consume(this);
		//float posX = APP->scene->rack->getMousePos().x;
        //float posY = APP->scene->rack->getMousePos().y;
		//float oldScreenScale = screenScale;

		screenScale += (e.scrollDelta.y * screenScale) / 256.f;
		module->startScreenScale = screenScale;

		//xOffset = xOffset - ((posX / oldScreenScale) - (posX / screenScale));
		//yOffset = yOffset - ((posY / oldScreenScale) - (posY / screenScale));


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
        nvgRect(vg, xVal, yVal, xSize, ySize);
        nvgFill(vg);

		// update pos for buttonclicking
		node->box.pos = Vec(xVal, yVal);
		node->box.size = Vec(xSize, ySize);

		// grid
		float gridStartX = xVal + (xSize/8);
		float gridStartY = yVal + (xSize/8);

		for (int i = 0; i < abs((node->output+1) % 12); i++) {

			float boxX = gridStartX + (((NODE_SIZE/7)*scale) * (i%3));
			float boxY = gridStartY + (((NODE_SIZE/7)*scale) * floor(i/3));

			nvgFillColor(vg, nvgRGB(44,44,44));
			nvgBeginPath(vg);
			nvgRect(vg, boxX, boxY, (NODE_SIZE/8) * scale, (NODE_SIZE/8) * scale);
			nvgFill(vg);

		}

		/*std::shared_ptr<window::Font> font = APP->window->loadFont(asset::plugin(pluginInstance, std::string("res/fonts/OpenSans-Regular.ttf")));
		nvgSave(vg);
		float textScale = scale/6;
		nvgScale(vg, textScale, textScale);
        nvgFontFaceId(vg, font->handle);
        nvgFontSize(vg, 50);
        nvgFillColor(vg, nvgRGB(44,44,44));
    	//nvgTextAlign(vg, NVGalign::NVG_ALIGN_CENTER);
        nvgText(vg, (xVal/textScale) + ((xSize/textScale)/4), (yVal/textScale) + ((ySize/textScale)), Scale::getNoteString(node->output).c_str(), NULL);
		nvgRestore(vg);*/

		if (node->children.size() > 1) {
			float chance = std::min(1.f, std::max(0.f, node->chance - module->getChanceMod()));

			nvgFillColor(vg, nvgRGB(240,240,240));
			nvgBeginPath(vg);
			nvgRect(vg, xVal + ((xSize/8) * 7), yVal, xSize/8, ySize);
			nvgFill(vg);
			nvgFillColor(vg, nvgRGB(44,44,44));
			nvgBeginPath(vg);
			nvgRect(vg, xVal + ((xSize/8) * 7), yVal, xSize/8, ySize * chance);
			nvgFill(vg);
		}

		nvgGlobalAlpha(vg, 1.0);

	}

	inline float calcNodeScale(int nodeCount) { return (NODE_SIZE/nodeCount) / NODE_SIZE; }

	inline float calcNodeYHeight(float scale, int nodePos, int nodeCount) { return NODE_SIZE+(((NODE_SIZE*scale)*nodePos)-(((NODE_SIZE*scale)*nodeCount)/2)); }

	void drawNodes(NVGcontext* vg) {

		for (size_t i = 0; i < nodeCache.size(); i++) {
			drawNode(vg, nodeCache[i].node, nodeCache[i].pos.x, nodeCache[i].pos.y, nodeCache[i].scale);
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

		// nvgScale
		// nvgText
		// nvgFontSize
		// nvgCreateFont
		// nvgText(0, 0 )

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

		}

		nvgRestore(args.vg);

	}

	std::vector<std::vector<Node*>> nodeBins;
	struct NodePosCache {
		Vec pos;
		float scale;
		Node* node;
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

					if (followNodes && node == module->activeNode) {
						xOffset = -cumulativeX;
						yOffset = -y;
						screenScale = ((1-(scale*2))*25);
					}
				}
				
			}
		}

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

	void setText(NVGcolor c) {
		color->textList.clear();
		color->addText("TREEQUENCER", "OpenSans-ExtraBold.ttf", c, 14, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, 20));
		color->addText("·ISI·", "OpenSans-ExtraBold.ttf", c, 28, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, RACK_GRID_HEIGHT-13));
	}

	TreequencerWidget(Treequencer* module) {
		setModule(module);
		//setPanel(createPanel(asset::plugin(pluginInstance, "res/nrandomizer.svg")));

		backdrop = new ImagePanel();
		backdrop->box.size = Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
		backdrop->imagePath = asset::plugin(pluginInstance, "res/treequencer.jpg");
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
		setText(nvgRGB(255,255,255));

		if (module && module->theme.size()) {
			color->drawBackground = true;
			color->setTheme(BG_THEMES[module->theme]);
		}
		if (module) color->setTextGroupVisibility("descriptor", module->showDescriptors);
		
		setPanel(backdrop);
		addChild(color);
		addChild(display);
		addChild(dirt);

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(10.319f, 10)), module, Treequencer::GATE_IN_1));
		addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(23.336f, 10)), module, Treequencer::CLOCK));
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
		menu->addChild(rack::createSubmenuItem("Screen Color Mode", "", [=](ui::Menu* menu) {
			menu->addChild(createMenuItem("Light", "",[=]() {
				mod->onAudioThread([=]() { mod->colorMode = 0; });
				userSettings.setSetting<int>("treequencerScreenColor", 0);
			}));
			menu->addChild(createMenuItem("Vibrant", "", [=]() {
				mod->onAudioThread([=]() { mod->colorMode = 1; });
				userSettings.setSetting<int>("treequencerScreenColor", 1);
			}));
			menu->addChild(createMenuItem("Muted", "", [=]() {
				mod->onAudioThread([=]() { mod->colorMode = 2; });
				userSettings.setSetting<int>("treequencerScreenColor", 2);
			}));
			menu->addChild(createMenuItem("Greyscale", "", [=]() {
				mod->onAudioThread([=]() { mod->colorMode = 3; });
				userSettings.setSetting<int>("treequencerScreenColor", 3);
			}));
		}));

		QuestionableWidget::appendContextMenu(menu);
	}
};

Model* modelTreequencer = createModel<Treequencer, TreequencerWidget>("treequencer");
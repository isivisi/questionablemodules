#include "plugin.hpp"
#include "imagepanel.cpp"
#include "textfield.cpp"
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
const int MODULE_SIZE = 18;

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

// A node in the tree
struct Node {
  	int output = 0;
	bool enabled = false;
	float chance = 0.5;
	Node* parent;
  	std::vector<Node*> children;

	Rect box;

	Node(Node* p = nullptr, int out = randomInteger(-1, 7), float c = randFloat(0.9f)) {
		output = out;
		chance = c;
		parent = p;
	}

	Node(json_t* json, Node* p=nullptr) {
		parent = p;
		fromJson(json);
	}

	~Node() {
		
		for (int i = 0; i < children.size(); i++) delete children[i];
	}

	void setOutput(int out) {
		
		output = std::max(-1, out);
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

	void addChild() {

		Node* child = new Node(this);
		child->parent = this;
		child->chance = randFloat(0.9f);
		child->output = randomInteger(-1, 7);
		children.push_back(child);

	}

	void removeTopChild() {
		delete children[0];
		children.erase(children.begin()); 
	}

	void removeBottomChild() {
		delete children[1];
		children.pop_back(); 
	}

	int maxDepth() {
		if (children.size() == 0) return 0;

		std::vector<int> sizes;
		for (int i = 0; i < children.size(); i++) {
			sizes.push_back(children[i]->maxDepth() + 1);
		}
		return *std::max_element(sizes.begin(), sizes.end());
	}

	json_t* const toJson() {
		json_t* nodeJ = json_object();

		json_object_set_new(nodeJ, "output", json_integer(output));
		json_object_set_new(nodeJ, "chance", json_real(chance));
		
		json_t *childrenArray = json_array();
		json_object_set_new(nodeJ, "children", childrenArray);

		for (int i = 0; i < children.size(); i++) {
			json_array_append_new(childrenArray, children[i]->toJson());
		}

		return nodeJ;
	}

	void fromJson(json_t* json) {

		if (json_t* out = json_object_get(json, "output")) output = json_integer_value(out);
		if (json_t* c = json_object_get(json, "chance")) chance = json_real_value(c);

		if (json_t* arr = json_object_get(json, "children")) {

			for (int i = 0; i < json_array_size(arr); i++) {
				children.push_back(new Node(json_array_get(arr, i), this));
			}

		}

	}

};

/*for (const auto& child : node->children) {
    
}*/

struct Treequencer : Module {
	enum ParamId {
		FADE_PARAM,
		TRIGGER_TYPE,
		BOUNCE,
		CHANCE_INVERSION,
		PARAMS_LEN
	};
	enum InputId {
		GATE_IN_1,
		RESET,
		CHANCE_INVERSION_INPUT,
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
		LIGHTS_LEN
	};

	std::queue<std::function<void()>> audioThreadQueue;

	// json data for screen
	float startScreenScale = 3.5f;
	float startOffsetX = 25.f;
	float startOffsetY = 0.f;

	bool isDirty = true;
	bool bouncing = false;

	dsp::SchmittTrigger gateTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::PulseGenerator pulse;
	dsp::PulseGenerator sequencePulse;

	Node rootNode;
	Node* activeNode;

	void onAudioThread(std::function<void()> func) {
		audioThreadQueue.push(func);
	}

	void processOffThreadQueue() {
		while (!audioThreadQueue.empty()) {
			std::function<void()> func = audioThreadQueue.front();
			func();
			audioThreadQueue.pop();
		}
	}

	Treequencer() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configInput(GATE_IN_1, "Clock");
		configInput(RESET, "Reset");
		configInput(CHANCE_INVERSION_INPUT, "Chance Inversion VC");
		configSwitch(TRIGGER_TYPE, 0.f, 1.f, 0.f, "Trigger Type", {"Step", "Sequence"});
		configSwitch(BOUNCE, 0.f, 1.f, 0.f, "Bounce", {"Off", "On"});
		configSwitch(CHANCE_INVERSION, -1.f, 1.f, 0.0f, "Chance Inversion");
		configOutput(SEQ_OUT_1, "Sequence 1");
		configOutput(SEQ_OUT_2, "Sequence 2");
		configOutput(SEQ_OUT_3, "Sequence 3");
		configOutput(SEQ_OUT_4, "Sequence 4");
		configOutput(SEQ_OUT_5, "Sequence 5");
		configOutput(SEQ_OUT_6, "Sequence 6");
		configOutput(SEQ_OUT_7, "Sequence 7");
		configOutput(SEQ_OUT_8, "Sequence 8");

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
	}

	void process(const ProcessArgs& args) override {

		processOffThreadQueue();

		bool reset = resetTrigger.process(inputs[RESET].getVoltage(), 0.1f, 2.f);
		bool shouldIterate = gateTrigger.process(inputs[GATE_IN_1].getVoltage(), 0.1f, 2.f);

		lights[TRIGGER_LIGHT].setBrightness(params[TRIGGER_TYPE].getValue());
		lights[BOUNCE_LIGHT].setBrightness(params[BOUNCE].getValue());

		if (shouldIterate && activeNode) {
			activeNode->enabled = false;

			if (!bouncing && !activeNode->children.size()) {
				if (params[BOUNCE].getValue()) bouncing = true;
				else activeNode = &rootNode;
				sequencePulse.trigger(1e-3f); // signal sequence completed
			}
			else {
				if (activeNode->children.size() > 1) {
					float r = randFloat();
					activeNode = activeNode->children[r < activeNode->chance ? 0 : 1];
				} else {
					activeNode = activeNode->children[0];
				}
			}

			if (bouncing) {
				if (activeNode->parent) activeNode = activeNode->parent;
				else bouncing = false;
			}

			activeNode->enabled = true;
			pulse.trigger(1e-3f);
		}

		bool activeP = pulse.process(args.sampleTime);
		bool sequenceP = sequencePulse.process(args.sampleTime);

		outputs[SEQUENCE_COMPLETE].setVoltage(sequenceP ? 10.f : 0.0f);

		if (activeNode->output < 0) {
			outputs[ALL_OUT].setVoltage(0.f);
		} else {
			if (activeNode->output < 8) outputs[activeNode->output].setVoltage(activeP ? 10.f : 0.0f); 
			outputs[ALL_OUT].setVoltage((float)activeNode->output/12);
		}

	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "startScreenScale", json_real(startScreenScale));
		json_object_set_new(rootJ, "startOffsetX", json_real(startOffsetX));
		json_object_set_new(rootJ, "startOffsetY", json_real(startOffsetY));
		json_object_set_new(rootJ, "rootNode", rootNode.toJson());

		return rootJ;
	}


	void dataFromJson(json_t* rootJ) override {

		if (json_t* sss = json_object_get(rootJ, "startScreenScale")) startScreenScale = json_real_value(sss);
		if (json_t* sx = json_object_get(rootJ, "startOffsetX")) startOffsetX = json_real_value(sx);
		if (json_t* sy = json_object_get(rootJ, "startOffsetY")) startOffsetY = json_real_value(sy);

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
			if (isInteger(text)) mod->onAudioThread([=](){ node->setOutput(std::stoi(text)-1); });
		});
		outparam->box.size.x = 100;
		outparam->text = std::to_string(node->output + 1);
		menu->addChild(outparam);

		menu->addChild(rack::createMenuLabel("Node Chance:"));

		ui::TextField* param = new QTextField([=](std::string text) { 
			if (isNumber(text)) mod->onAudioThread([=](){ node->setChance((float)::atof(text.c_str())); });
		});
		param->box.size.x = 100;
		param->text = std::to_string(node->chance);
		menu->addChild(param);

		menu->addChild(rack::createMenuLabel(""));

		// TODO: depth 22 MAX
		if (node->children.size() < 2) menu->addChild(createMenuItem("Add Child", "", [=]() { 
			mod->onAudioThread([=](){
				node->addChild(); 
				renderStateDirty();
			});
		}));

		if (node->children.size() > 0) menu->addChild(createMenuItem("Remove Top Child", "", [=]() {
			mod->onAudioThread([=](){
				mod->resetActiveNode();
				node->removeTopChild();
				renderStateDirty();
			});
		}));

		if (node->children.size() > 1) menu->addChild(createMenuItem("Remove Bottom Child", "", [=]() { 	
			mod->onAudioThread([=](){
				mod->resetActiveNode();
				node->removeBottomChild();
				renderStateDirty();
			});
		}));
		
	}

	Node* findNodeClicked(Vec mp, Node* node) {
		if (!node) return nullptr;

		if (isInsideBox(mp, node->box)) return node;

		for (int i = 0; i < node->children.size(); i++)
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

	void onHoverScroll(const HoverScrollEvent& e) {
		e.consume(this);
		float posX = APP->scene->rack->getMousePos().x;
        float posY = APP->scene->rack->getMousePos().y;
		float oldScreenScale = screenScale;

		screenScale += (e.scrollDelta.y * screenScale) / 256.f;
		module->startScreenScale = screenScale;

		//xOffset = xOffset - ((posX / oldScreenScale) - (posX / screenScale));
		//yOffset = yOffset - ((posY / oldScreenScale) - (posY / screenScale));


	}

	const NVGcolor octColors[5] = {
		nvgRGB(255,127,80),
		nvgRGB(80,208,255),
		nvgRGB(127,80,255),
		nvgRGB(121,255,80),
		nvgRGB(255,215,80)
	};

	void drawNode(NVGcontext* vg, Node* node, float x, float y,  float scale) {
		
		float xVal = x + xOffset;
		float yVal = y + yOffset;
		float xSize = NODE_SIZE * scale;
		float ySize = NODE_SIZE * scale;

		int octOffset = (node->output+1) / 12;

		// node bg
		nvgFillColor(vg, node->enabled ? nvgRGB(124,252,0) : octColors[octOffset%5]);
        nvgBeginPath(vg);
        nvgRect(vg, xVal, yVal, xSize, ySize);
        nvgFill(vg);

		// update pos for buttonclicking
		node->box.pos = Vec(xVal, yVal);
		node->box.size = Vec(xSize, ySize);

		// grid
		float gridStartX = xVal + (xSize/8);
		float gridStartY = yVal + (xSize/8);

		for (int i = 0; i < (node->output+1) % 12; i++) {

			float boxX = gridStartX + (((NODE_SIZE/7)*scale) * (i%3));
			float boxY = gridStartY + (((NODE_SIZE/7)*scale) * floor(i/3));

			nvgFillColor(vg, nvgRGB(44,44,44));
			nvgBeginPath(vg);
			nvgRect(vg, boxX, boxY, (NODE_SIZE/8) * scale, (NODE_SIZE/8) * scale);
			nvgFill(vg);

		}

		if (node->children.size() > 1) {
			// chance
			nvgFillColor(vg, nvgRGB(240,240,240));
			nvgBeginPath(vg);
			nvgRect(vg, xVal + ((xSize/8) * 7), yVal, xSize/8, ySize);
			nvgFill(vg);
			nvgFillColor(vg, nvgRGB(44,44,44));
			nvgBeginPath(vg);
			nvgRect(vg, xVal + ((xSize/8) * 7), yVal, xSize/8, ySize * node->chance);
			nvgFill(vg);
		}

	}

	inline float calcNodeScale(int nodeCount) { return (NODE_SIZE/nodeCount) / NODE_SIZE; }

	inline float calcNodeYHeight(float scale, int nodePos, int nodeCount) { return NODE_SIZE+(((NODE_SIZE*scale)*nodePos)-(((NODE_SIZE*scale)*nodeCount)/2)); }

	void drawNodes(NVGcontext* vg) {

		for (int i = 0; i < nodeCache.size(); i++) {
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
				if (!depth) return;

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

		int depth = module->rootNode.maxDepth() + 2;
		
		float cumulativeX = -25.f;
		for (int d = 0; d < nodeBins.size(); d++) {
			int binLen = nodeBins[d].size();
			float scale = calcNodeScale(binLen);
			float prevScale = calcNodeScale(nodeBins[std::max(0, d-1)].size()); //(1 - ((float)(d-1)/depth));
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

		for (int i = 0; i < node->children.size(); i++) {
			gatherNodesForBins(node->children[i], (position*2)+i, depth+1);
		}

	}

};

struct TreequencerWidget : ModuleWidget {
	ImagePanel *backdrop;
	NodeDisplay *display;
	ImagePanel *dirt;

	TreequencerWidget(Treequencer* module) {
		setModule(module);
		//setPanel(createPanel(asset::plugin(pluginInstance, "res/nrandomizer.svg")));

		backdrop = new ImagePanel();
		backdrop->box.size = Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
		backdrop->imagePath = asset::plugin(pluginInstance, "res/treequencer.png");
		backdrop->scalar = 3.5;
		backdrop->visible = true;

		display = new NodeDisplay();
		display->box.pos = Vec(15, 50);
        display->box.size = Vec(((MODULE_SIZE -1) * RACK_GRID_WIDTH) - 15, 200);
		display->module = module;
		if (module) {
			display->screenScale = module->startScreenScale;
			display->xOffset = module->startOffsetX;
			display->yOffset = module->startOffsetY;
		}

		dirt = new ImagePanel();
		dirt->box.pos = Vec(15, 50);
        dirt->box.size = Vec(((MODULE_SIZE -1) * RACK_GRID_WIDTH) - 15, 200);
		dirt->imagePath = asset::plugin(pluginInstance, "res/dirt.png");
		dirt->scalar = 3.5;
		dirt->visible = true;
		
		setPanel(backdrop);
		addChild(display);
		addChild(dirt);

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.f, 10)), module, Treequencer::GATE_IN_1));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(80.f, 90.f)), module, Treequencer::TRIGGER_TYPE, Treequencer::TRIGGER_LIGHT));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(72.f, 90.f)), module, Treequencer::BOUNCE, Treequencer::BOUNCE_LIGHT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(80.f, 10.f)), module, Treequencer::ALL_OUT));

		for (int i = 0; i < MAX_OUTPUTS; i++) {
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10.f + (10.0*float(i)), 113.f)), module, i));
		}

		//addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 46.063)), module, Treequencer::PITCH_PARAM));

		//addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(35.24, 103)), module, Treequencer::FADE_PARAM));
		//addInput(createInputCentered<PJ301MPort>(mm2px(Vec(35.24, 113)), module, Treequencer::FADE_INPUT));
		
		//addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10, 10.478  + (10.0*float(i)))), module, i));
		//addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(35.24, 10.478  + (10.0*float(i)))), module, i));

		//addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10, 113)), module, Treequencer::TRIGGER));

		//addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(14.24, 106.713)), module, Treequencer::BLINK_LIGHT));
	}
};

Model* modelTreequencer = createModel<Treequencer, TreequencerWidget>("treequencer");
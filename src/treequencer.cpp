#include "plugin.hpp"
#include "imagepanel.cpp"
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <random>
#include <algorithm>

const int MAX_OUTPUTS = 8;
const int MODULE_SIZE = 18;

const int DEFAULT_NODE_DEPTH = 3;

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

// A node in the tree
struct Node {
  	int output = 0;
	bool enabled = false;
	float chance = 0.5;
	Node* parent;
  	std::vector<Node> children;

	Rect box;

	Node(Node* p = nullptr, int out = randomInteger(-1, 7), float c = randFloat(0.9f)) {

		output = out;
		chance = c;
		parent = p;

	}

	Node(json_t* json) {
		fromJson(json);
	}

	// Fill each Node with 2 other nodes until depth is met
	// assumes EMPTY
  	void fillToDepth(int desiredDepth) {
		if (desiredDepth <= 0) return;
		Node child1 = Node(this);
		Node child2 = Node(this);
		child1.fillToDepth(desiredDepth-1);
		child2.fillToDepth(desiredDepth-1);
		children.push_back(child1);
		children.push_back(child2);
  	}

	void addChild() {

		Node child = Node(this);
		child.parent = this;
		child.chance = randFloat(0.9f);
		child.output = randomInteger(-1, 7);
		children.push_back(child);

	}

	int maxDepth() {
		if (children.size() == 0) return 0;

		std::vector<int> sizes;
		for (int i = 0; i < children.size(); i++) {
			sizes.push_back(children[i].maxDepth() + 1);
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
			json_array_append_new(childrenArray, children[i].toJson());
		}

		return nodeJ;
	}

	void fromJson(json_t* json) {

		if (json_t* out = json_object_get(json, "output")) output = json_integer_value(out);
		if (json_t* c = json_object_get(json, "chance")) chance = json_real_value(c);

		if (json_t* arr = json_object_get(json, "children")) {

			for (int i = 0; i < json_array_size(arr); i++) {
				children.push_back(Node(json_array_get(arr, i)));
			}

		}

	}

};

/*for (const auto& child : node->children) {
    
}*/

struct Treequencer : Module {
	enum ParamId {
		FADE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		GATE_IN_1,
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
		OUTPUTS_LEN
	};
	enum LightId {
		VOLTAGE_IN_LIGHT_1,
		BLINK_LIGHT,
		LIGHTS_LEN
	};

	dsp::SchmittTrigger gateTrigger;
	dsp::SchmittTrigger resetTrigger;

	Node rootNode;

	Node* activeNode;

	float lastTriggerValue = 0.f;

	Treequencer() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		//configParam(FADE_PARAM, 0.f, 1.f, 0.f, "Fade Amount");
		configInput(GATE_IN_1, "Clock");
		//configOutput(SINE_OUTPUT, "");
		//configInput(TRIGGER, "Gate");

		configOutput(SEQ_OUT_1, "Sequence 1");
		configOutput(SEQ_OUT_1, "Sequence 2");
		configOutput(SEQ_OUT_1, "Sequence 3");
		configOutput(SEQ_OUT_1, "Sequence 4");
		configOutput(SEQ_OUT_1, "Sequence 5");
		configOutput(SEQ_OUT_1, "Sequence 6");
		configOutput(SEQ_OUT_1, "Sequence 7");
		configOutput(SEQ_OUT_1, "Sequence 8");

		configOutput(ALL_OUT, "Pitch");
		

		rootNode.fillToDepth(3);
		
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

		bool shouldIterate = gateTrigger.process(inputs[GATE_IN_1].getVoltage(), 0.1f, 2.f);

		if (shouldIterate && activeNode) {
			activeNode->enabled = false;
			outputs[activeNode->output].setVoltage(0.f); 
			if (!activeNode->children.size()) activeNode = &rootNode;
			else {
				if (activeNode->children.size() > 1) {
					float r = randFloat();
					activeNode = &activeNode->children[r < activeNode->chance ? 0 : 1];
				} else {
					activeNode = &activeNode->children[0];
				}
			}
			activeNode->enabled = true;
		}

		if (activeNode->output < 0) {
			outputs[activeNode->output].setVoltage(0.f); 
			outputs[ALL_OUT].setVoltage(0.f);
		} else {
			outputs[activeNode->output].setVoltage(10.f); 
			outputs[ALL_OUT].setVoltage((float)activeNode->output/8);
		}

	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		json_object_set_new(rootJ, "rootNode", rootNode.toJson());

		return rootJ;
	}


	void dataFromJson(json_t* rootJ) override {

		if (json_t* rn = json_object_get(rootJ, "rootNode")) {
			rootNode.children.clear();
			activeNode = &rootNode;
			rootNode.fromJson(rn);
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

	NodeDisplay() {

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
		menu->addChild(rack::createMenuLabel("Node Chance:"));

		ui::TextField* param = new ui::TextField();
		param->box.size.x = 100;
		param->text = std::to_string(node->chance);
		menu->addChild(param);

		if (node->children.size() < 2) menu->addChild(createMenuItem("Add Child", "", [=]() { node->addChild(); }));

		if (node->children.size() > 0) menu->addChild(createMenuItem("Remove Top Child", "", [=]() {
			mod->resetActiveNode();
			node->children.erase(node->children.begin()); 
		}));

		if (node->children.size() > 1) menu->addChild(createMenuItem("Remove Bottom Child", "", [=]() { 
			mod->resetActiveNode();
			node->children.pop_back(); 
		}));
		
	}

	Node* findNodeClicked(Vec mp, Node* node) {
		if (!node) return nullptr;

		if (isInsideBox(mp, node->box)) return node;

		for (int i = 0; i < node->children.size(); i++)
    	{
			Node* found = findNodeClicked(mp, &node->children[i]);
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

		dragX = newDragX;
		dragY = newDragY;

    }

	void onHoverScroll(const HoverScrollEvent& e) {
		e.consume(this);
		float posX = APP->scene->rack->getMousePos().x;
        float posY = APP->scene->rack->getMousePos().y;
		float oldScreenScale = screenScale;

		screenScale += (e.scrollDelta.y * screenScale) / 256.f;

		//xOffset = xOffset - ((posX / oldScreenScale) - (posX / screenScale));
		//yOffset = yOffset - ((posY / oldScreenScale) - (posY / screenScale));


	}

	void drawNode(NVGcontext* vg, Node* node, float x, float y,  float scale) {
		
		float xVal = x + xOffset;
		float yVal = y + yOffset;
		float xSize = NODE_SIZE * scale;
		float ySize = NODE_SIZE * scale;

		// node bg
		nvgFillColor(vg, node->enabled ? nvgRGB(124,252,0) : nvgRGB(255,127,80));
        nvgBeginPath(vg);
        nvgRect(vg, xVal, yVal, xSize, ySize);
        nvgFill(vg);

		// update pos for buttonclicking
		node->box.pos = Vec(xVal, yVal);
		node->box.size = Vec(xSize, ySize);

		// grid
		float gridStartX = xVal + (xSize/8);
		float gridStartY = yVal + (xSize/8);

		for (int i = 0; i < node->output+1; i++) {

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

		int depth = module->rootNode.maxDepth() + 2;
		
		float cumulativeX = -25.f;
		for (int d = 0; d < nodeBins.size(); d++) {
			int binLen = nodeBins[d].size();
			float scale = calcNodeScale(binLen);
			float prevScale = calcNodeScale(nodeBins[std::max(0, d-1)].size()); //(1 - ((float)(d-1)/depth));
			cumulativeX += ((NODE_SIZE+1)*prevScale);
			for(int i = 0; i < binLen; i++) {
				Node* node = nodeBins[d][i];

				if (node) {
					float y = calcNodeYHeight(scale, i, binLen);
					drawNode(vg, node, cumulativeX, y, scale);
				}

				/*if (node == module->activeNode) {
					xOffset = -cumulativeX;
					yOffset = -y;
					scale = 6 * ((1-scale)*100);
				}*/
				
			}
		}
	}

	void draw(const DrawArgs &args) override {
		//if (module == NULL) return;

		nvgFillColor(args.vg, nvgRGB(15, 15, 15));
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
        nvgFill(args.vg);

	}

	std::vector<std::vector<Node*>> nodeBins;

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

			gatherNodesForBins(module->rootNode);
			drawNodes(args.vg);

		}

		nvgRestore(args.vg);

	}

	void gatherNodesForBins(Node& node, int position = 0, int depth = 0) {

		nodeBins[depth][position] = &node;

		//nodeBins[depth].push_back(&node);

		for (int i = 0; i < node.children.size(); i++) {
			gatherNodesForBins(node.children[i], (position*2)+i, depth+1);
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
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

// A node in the tree
struct Node {
  	int output = 0;
	bool enabled = false;
	float chance = 0.5;
	Node* parent;
  	std::vector<Node> children;


	// Fill each Node with 2 other nodes until depth is met
	// assumes EMPTY
  	void fillToDepth(int desiredDepth) {
		if (desiredDepth <= 0) return;
		Node child1 = Node();
		Node child2 = Node();
		child1.parent = this;
		child2.parent = this;
		child1.fillToDepth(desiredDepth-1);
		child2.fillToDepth(desiredDepth-1);
		children.push_back(child1);
		children.push_back(child2);
  	}

	int maxDepth() {
		if (children.size() == 0) return 0;

		std::vector<int> sizes;
		for (int i = 0; i < children.size(); i++) {
			sizes.push_back(children[i].maxDepth() + 1);
		}
		return *std::max_element(sizes.begin(), sizes.end());
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
		OUTPUTS_LEN
	};
	enum LightId {
		VOLTAGE_IN_LIGHT_1,
		BLINK_LIGHT,
		LIGHTS_LEN
	};

	Node rootNode;

	Node* activeNode;

	float lastTriggerValue = 0.f;

	Treequencer() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		//configParam(FADE_PARAM, 0.f, 1.f, 0.f, "Fade Amount");
		configInput(GATE_IN_1, "Clock");
		//configOutput(SINE_OUTPUT, "");
		//configInput(TRIGGER, "Gate");

		configInput(SEQ_OUT_1, "Sequence 1");
		configInput(SEQ_OUT_1, "Sequence 2");
		configInput(SEQ_OUT_1, "Sequence 3");
		configInput(SEQ_OUT_1, "Sequence 4");
		configInput(SEQ_OUT_1, "Sequence 5");
		configInput(SEQ_OUT_1, "Sequence 6");
		configInput(SEQ_OUT_1, "Sequence 7");
		configInput(SEQ_OUT_1, "Sequence 8");

		rootNode.fillToDepth(3);
		
		rootNode.enabled = true;
		activeNode = &rootNode;
		
	}

	float fclamp(float min, float max, float value) {
		return std::min(min, std::max(max, value));
	}

	void process(const ProcessArgs& args) override {

		bool shouldIterate = fabs(lastTriggerValue - inputs[GATE_IN_1].getVoltage()) > 0.1f;
		lastTriggerValue = inputs[GATE_IN_1].getVoltage();

		if (shouldIterate) {
			activeNode->enabled = false;
			outputs[activeNode->output].setVoltage(0.f); 
			if (!activeNode->children.size()) activeNode = &rootNode;
			else {
				std::uniform_real_distribution<float> distribution(0, 1);
				std::random_device rd;
				float r = distribution(rd);
				activeNode = &activeNode->children[r < 0.5 ? 0 : 1];
			}
			activeNode->enabled = true;
		}

		outputs[activeNode->output].setVoltage(1.f); 

	}
};

struct NodeDisplay : Widget {
	Treequencer* module;

	const float NODE_SIZE = 25;

	float xOffset = 0;
	float yOffset = 0;

	float dragX = 0;
	float dragY = 0;

	float screenScale = 1.f;

	NodeDisplay() {

	}

	void onButton(const event::Button &e) override {
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
            e.consume(this);
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
		screenScale += e.scrollDelta.y / 256;
	}

	void drawNode(NVGcontext* vg, Node* node, float x, float y,  float scale) {
		
		float xSize = NODE_SIZE * scale;
		float ySize = NODE_SIZE * scale;

		nvgFillColor(vg, node->enabled ? nvgRGB(124,252,0) : nvgRGB(255,127,80));
        nvgBeginPath(vg);
        nvgRect(vg, x + xOffset, y + yOffset, xSize, ySize);
        nvgFill(vg);

	}

	void drawNodes(NVGcontext* vg) {

		int depth = module->rootNode.maxDepth() + 2;
		
		float cumulativeX = 50.f;
		for (int d = 0; d < nodeBins.size(); d++) {
			float scale = (1 - ((float)d/depth));
			float prevScale = (1 - ((float)(d-1)/depth));
			cumulativeX += ((NODE_SIZE)*prevScale) + 1;
			for(int i = 0; i < nodeBins[d].size(); i++) {
				Node* node = nodeBins[d][i];
				float y = ((((NODE_SIZE + 1) *scale) * i) - (((NODE_SIZE*scale) * nodeBins[d].size()) / 2)) + 15;

				drawNode(vg, node, cumulativeX, y, scale);
			}
		}

		/*for (int i = 0; i < node.children.size(); i++) {

			int newX = x + 45;
			int newY = (y + (dx/2)) + (i - node.children.size() / 2) * dx;

			// draw path to child
			Vec startPos = Vec(x + xOffset + 25, y + yOffset + 7.5);
			Vec endPos = Vec(newX + xOffset, newY +yOffset + 7.5);

			Vec QuadMod1 = Vec(startPos.x + 7.5, startPos.y);
			Vec End1 = lerp(startPos, endPos, 0.50);
			Vec QuadMod2 = Vec(endPos.x - 7.5, endPos.y);

			nvgBeginPath(vg);
			nvgMoveTo(vg, startPos.x, startPos.y);
			nvgQuadTo(vg, QuadMod1.x, QuadMod1.y, End1.x, End1.y);
			nvgQuadTo(vg, QuadMod2.x, QuadMod2.y, endPos.x, endPos.y);
			nvgStrokeColor(vg, nvgRGB(255,127,80));
			nvgStrokeWidth(vg, 2);
			nvgStroke(vg);
			nvgClosePath(vg);

			// recursive draw child node
			drawNodes(vg, node.children[i], newX, newY, depth+1);
		}*/

		//nvgRestore(vg);

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
			for (int i = 0; i < depth+1; i++) {
				nodeBins.push_back(std::vector<Node*>());
			}

			gatherNodesForBins(module->rootNode);
			drawNodes(args.vg);

		}

		nvgRestore(args.vg);

	}

	void gatherNodesForBins(Node& node, int depth = 0) {

		nodeBins[depth].push_back(&node);

		for (int i = 0; i < node.children.size(); i++) {
			gatherNodesForBins(node.children[i], depth+1);
		}

	}

};

struct TreequencerWidget : ModuleWidget {
	ImagePanel *backdrop;
	NodeDisplay *display;

	TreequencerWidget(Treequencer* module) {
		setModule(module);
		//setPanel(createPanel(asset::plugin(pluginInstance, "res/nrandomizer.svg")));

		backdrop = new ImagePanel();
		backdrop->box.size = Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
		backdrop->imagePath = asset::plugin(pluginInstance, "res/backdrop-dis.png");
		backdrop->scalar = 3.5;
		backdrop->visible = true;

		display = new NodeDisplay();
		display->box.pos = Vec(15, 50);
        display->box.size = Vec(((MODULE_SIZE -1) * RACK_GRID_WIDTH) - 15, 200);
		display->module = module;
		
		setPanel(backdrop);
		addChild(display);

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.f, 10)), module, Treequencer::GATE_IN_1));

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
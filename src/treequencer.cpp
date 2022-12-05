#include "plugin.hpp"
#include "imagepanel.cpp"
#include <iostream>
#include <map>
#include <string>
#include <vector>
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
  	int output;
  	std::vector<Node> children;

	// Fill each Node with 2 other nodes until depth is met
	// assumes EMPTY
  	void fillToDepth(int depth) {
		if (depth <= 0) return;
		Node child1 = Node();
		Node child2 = Node();
		child1.fillToDepth(depth-1);
		child2.fillToDepth(depth-1);
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
		VOLTAGE_IN_1,
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

	Treequencer() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		//configParam(FADE_PARAM, 0.f, 1.f, 0.f, "Fade Amount");
		//configInput(VOLTAGE_IN_1, "1");
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
		
	}

	float fclamp(float min, float max, float value) {
		return std::min(min, std::max(max, value));
	}

	void process(const ProcessArgs& args) override {

	}
};

struct NodeDisplay : Widget {
	Treequencer* module;

	int xOffset = 0;
	int yOffset = 0;

	int dragX = 0;
	int dragY = 0;

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

		xOffset += newDragX - dragX;
		yOffset += newDragY - dragY;

		dragX = newDragX;
		dragY = newDragY;

    }

	void drawNode(NVGcontext* vg, int x, int y) {

		nvgFillColor(vg, nvgRGB(255,127,80));
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x + xOffset, y + yOffset, 25, 15, 3);
        nvgFill(vg);

	}

	void drawNodes(NVGcontext* vg, Node& node, int x = 0, int y = 0, int dx = 100, int initDx = 100) {

		drawNode(vg, x, y);

		for (int i = 0; i < node.children.size(); i++) {
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
			drawNodes(vg, node.children[i], newX, newY, dx / node.children.size(), initDx);
		}

	}

	void draw(const DrawArgs &args) override {
		if (module == NULL) return;

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

		nvgText(0, 0 )

		nvgSave(args.vg);
		nvgScissor(args.vg, 0, 0, box.size.x, box.size.y);

		if (layer == 1) {

			int depth = module->rootNode.maxDepth();
			if (!depth) return;
			drawNodes(args.vg, module->rootNode, 25, 25, depth * 100, depth * 100);

		}

		nvgRestore(args.vg);

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
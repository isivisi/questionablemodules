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
		TIME_SIG1,
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

	SyncMute() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		//configSwitch(RANGE_PARAM, 1.f, 8.f, 1.f, "Range", {"1", "2", "3", "4", "5", "6", "7", "8"});
		//configParam(OFFSET_PARAM, 0.f, 8.f, 0.f, "Offset");
		configInput(IN, "");
		configOutput(OUT, "");
		
	}

	void process(const ProcessArgs& args) override {

		//outputs[OUTPUT].setVoltage(0);

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
			addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(7.8, 15 + (14* i))), module, SyncMute::IN + i));
			addParam(createParamCentered<QuestionableParam<RoundSmallBlackKnob>>(Vec(18, 15 + (14* i)), module, SyncMute::TIME_SIG + i));
			addParam(createParamCentered<QuestionableParam<CKD6>>(mm2px(Vec(20.2, 15 + (14* i))), module, SyncMute::MUTE + i));
			addOutput(createOutputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(32.8, 15 + (14 * i))), module, SyncMute::OUT + i));
		}

	}

};

Model* modelSyncMute = createModel<SyncMute, SyncMuteWidget>("syncmute");
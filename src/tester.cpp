#include "plugin.hpp"
#include "imagepanel.cpp"
#include "colorBG.hpp"
#include "questionableModule.hpp"
#include <vector>

const int MODULE_SIZE = 6;

struct Watcher : QuestionableModule {
	enum ParamId {
		PARAMS_LEN
	};
	enum InputId {
		VOLTAGE_IN_1,
		VOLTAGE_IN_2,
		VOLTAGE_IN_3,
		VOLTAGE_IN_4,
		VOLTAGE_IN_5,
		VOLTAGE_IN_6,
		VOLTAGE_IN_7,
		VOLTAGE_IN_8,
		INPUTS_LEN
	};
	enum OutputId {
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};
	
	template <typename T>
	static T lerp(T point1, T point2, T t) {
		float diff = point2 - point1;
		return point1 + diff * t;
	}

	struct sampleSet {
		float sampleRate;
		std::vector<float> samples;

		double playbackSamplePos = 0.0;

		// for playback feature
		float sampleAtRate(float sampleRate) {
			if (!samples.size() > 0) return 0.f;
			float s = 0.f;

			// check if value is whole number, if not interpolate between values
			if (fabs(playbackSamplePos - round(playbackSamplePos)) < 0.00001) s = samples[floor(playbackSamplePos)];
			else s = lerp<double>(samples[floor(playbackSamplePos)], samples[ceil(playbackSamplePos)], fabs(playbackSamplePos - floor(playbackSamplePos)));

			float sampleRateRatio = (float)sampleRate / (float)sRate;
			
			playbackSamplePos += sampleRateRatio;
			playbackSamplePos = std::max(0.0, playbackSamplePos);

			if (playbackSamplePos >= samples.size()-1) playbackSamplePos = 0.0;

			return s;
		}
	};

	std::vector<sampleSet> sampleData[8];

	Watcher() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configInput(VOLTAGE_IN_1, "1");
		configInput(VOLTAGE_IN_2, "2");
		configInput(VOLTAGE_IN_3, "3");
		configInput(VOLTAGE_IN_4, "4");
		configInput(VOLTAGE_IN_5, "5");
		configInput(VOLTAGE_IN_6, "6");
		configInput(VOLTAGE_IN_7, "7");
		configInput(VOLTAGE_IN_8, "8");
		
	}

	void process(const ProcessArgs& args) override {

		

	}

};

struct WatcherWidget : QuestionableWidget {

	void setText() {
		NVGcolor c = nvgRGB(255,255,255);
		color->textList.clear();

		//color->addText("", "OpenSans-Bold.ttf", c, 7, Vec(45.35, 257), "descriptor");
	}

	WatcherWidget(Watcher* module) {
		setModule(module);
		//setPanel(createPanel(asset::plugin(pluginInstance, "res/nrandomizer.svg")));

		backdrop = new ImagePanel();
		backdrop->box.size = Vec(6 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
		backdrop->imagePath = asset::plugin(pluginInstance, "res/backdrop.jpg");
		backdrop->scalar = 3.5;
		backdrop->visible = true;

		color = new ColorBG(Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT));
		color->drawBackground = false;
		if (module) setText();

		if (module && module->theme.size()) {
			color->drawBackground = true;
			color->setTheme(BG_THEMES[module->theme]);
		}
		if (module) color->setTextGroupVisibility("descriptor", module->showDescriptors);
		
		setPanel(backdrop);
		addChild(color);

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		for (int i = 0; i < 8; i++) {
			addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(15.24, 10.478  + (10.0*float(i)))), module, i));
			//addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(20.24, 15.478 + (10.0*float(i)))), module, i));
		}

	}

};

Model* modelWatcher = createModel<Watcher, WatcherWidget>("watcher");
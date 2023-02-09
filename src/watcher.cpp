#include "plugin.hpp"
#include "imagepanel.cpp"
#include "colorBG.hpp"
#include "questionableModule.hpp"
#include <gmtl/gmtl.h>
#include <gmtl/Vec.h>
#include <vector>
#include <algorithm>

const int MODULE_SIZE = 12;

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
		OUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	struct sampleSet {
		float sampleRate;
		std::vector<float> samples;

		double playbackSamplePos = 0.0;

		// for playback feature
		float sampleAtRate(float incomingSampleRate) {
			if (!samples.size()) return 0.f;
			float s = 0.f;

			// check if value is whole number, if not interpolate between values
			if (fabs(playbackSamplePos - round(playbackSamplePos)) < 0.00001) s = samples[floor(playbackSamplePos)];
			else s = lerp<double>(samples[floor(playbackSamplePos)], samples[ceil(playbackSamplePos)], fabs(playbackSamplePos - floor(playbackSamplePos)));

			float sampleRateRatio = (float)sampleRate / (float)incomingSampleRate;
			
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

		/*if (!APP->engine->getMasterModule()) {
			APP->engine->setMasterModule(this);
		}*/

	}

};

inline gmtl::Vec2f VecToVec2f(const Vec data){
	return gmtl::Vec2f(data.x, data.y);
}

struct Tentacle : QuestionableWidget {
	float wiggle = 20;
	float stretch = 0.f;

	float offset = randomReal<float>(0, 15);

	float rotAngle = 90.f;

	Tentacle() {

	}

	void draw(const DrawArgs &args) override {
		//if (module == NULL) return;
		QuestionableWidget::draw(args);

		nvgSave(args.vg);

		nvgRotate(args.vg, (M_PI / 180) * rotAngle);

		wiggle = sin(offset+time * 0.2 * M_PI) * 20.f;
		stretch = sin(offset+time * 0.1 * M_PI) * 15.f;

		nvgStrokeColor(args.vg, nvgRGB(0,0,0));
		nvgStrokeWidth(args.vg, 10.f);
		nvgLineCap(args.vg, NVG_ROUND);
		nvgLineJoin(args.vg, NVG_BEVEL);
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 0, 0);
		nvgQuadTo(args.vg, 10+stretch, -wiggle, 20+stretch, 0);
		nvgQuadTo(args.vg, 30+stretch, wiggle, 40+stretch, wiggle);
		nvgQuadTo(args.vg, 60+stretch, -wiggle, 80+stretch, wiggle/2);
		nvgStroke(args.vg);

		nvgRestore(args.vg);

	}
};

struct WatchersEye : QuestionableWidget {

	float eyeLength = 35;
	float eyeLidPos = 15.0;
	Vec eyePos = Vec(0,0);
	int64_t eyeTarget = 0;
	float eyeRotation = 0.0;

	gmtl::Vec2f eyeDirection;
	bool closeEye = false;

	NVGcolor eyeColor = nvgRGB(0,0,0);
	NVGcolor pupilColor = nvgRGB(255,255,255);
	NVGcolor eyeLidColor = nvgRGB(0,0,0);

	WatchersEye() {

	}

	void findNewTarget() {
		std::vector<int64_t> modIds = APP->engine->getModuleIds();
		eyeTarget = randomInt<int>(0, modIds.size());
		if (eyeTarget == 0) return;
		eyeTarget = modIds[eyeTarget-1];
	}

	void draw(const DrawArgs &args) override {
		//if (module == NULL) return;
		QuestionableWidget::draw(args);

		if ((frame % (int)60*30) == 0 && randomReal<float>() > 0.5) findNewTarget();

		Vec lookPos;
		if (eyeTarget) {
			if (ModuleWidget* mod = APP->scene->rack->getModule(eyeTarget)) 
				lookPos = mod->getAbsoluteOffset(Vec()) + mod->box.size.x/2 + mod->box.size.y/2;
		} else lookPos = APP->scene->getMousePos();
		Vec epos = getAbsoluteOffset(Vec());

		// calculate pupil position
		eyeDirection = VecToVec2f(lookPos - epos);
		float magnitude = clamp<float>(0.0, 8.0, gmtl::length(eyeDirection)/100);
		gmtl::normalize(eyeDirection);
		eyePos = lerp<Vec>(eyePos, Vec(eyeDirection[0] * magnitude, eyeDirection[1] * magnitude), deltaTime*5);

		// deterime when to blink
		if (closeEye) {
			eyeLidPos = lerp<float>(eyeLidPos, 9.f, deltaTime*7.f);
			if (eyeLidPos < 15.0) closeEye = false;
		} else {
			eyeLidPos = lerp<float>(eyeLidPos, 50.f, deltaTime*3.f);
			if ((frame % 60) == 0 && randomReal<float>() > 0.95) closeEye = true;
		}

		// eye background
		nvgStrokeColor(args.vg, pupilColor);
		nvgStrokeWidth(args.vg, 20.f);
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, eyeLength, 0);
		nvgQuadTo(args.vg, 0, eyeLidPos/4, -eyeLength, 0);
		nvgStroke(args.vg);

		nvgBeginPath(args.vg);
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, eyeLength, 0);
		nvgQuadTo(args.vg, 0, -eyeLidPos/4, -eyeLength, 0);
		nvgStroke(args.vg);

		// pupil
		if (eyeLidPos > 20.0) {
			nvgFillColor(args.vg, eyeColor);
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, eyePos.x, eyePos.y, 10);
			nvgFill(args.vg);

			nvgFillColor(args.vg, pupilColor);
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, eyePos.x * 1.2, eyePos.y * 1.2, 3);
			nvgFill(args.vg);

			nvgSave(args.vg);
			eyeRotation = std::fmod(eyeRotation + deltaTime, 360);
			nvgTranslate(args.vg, eyePos.x * 1.2, eyePos.y * 1.2);
			nvgRotate(args.vg, eyeRotation);
			for (int i = 0; i < 8; i++) {
				nvgFillColor(args.vg, pupilColor);
				nvgBeginPath(args.vg);
				nvgCircle(args.vg,sin(i *((M_PI / 180) * (360/8)))*5, cos(i *((M_PI / 180) * (360/8)))*5, 0.75);
				nvgFill(args.vg);
			}
			nvgRestore(args.vg);
		}

		// eyelids
		nvgStrokeColor(args.vg, eyeLidColor);
		nvgStrokeWidth(args.vg, 20.f);
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, eyeLength, 0);
		nvgQuadTo(args.vg, 0, eyeLidPos, -eyeLength, 0);
		nvgStroke(args.vg);

		nvgBeginPath(args.vg);
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, eyeLength, 0);
		nvgQuadTo(args.vg, 0, -eyeLidPos, -eyeLength, 0);
		nvgStroke(args.vg);

	}
};

struct WatcherWidget : QuestionableModuleWidget {
	WatchersEye* eye;
	std::vector<Tentacle*> ts;

	void setText() {
		NVGcolor c = nvgRGB(255,255,255);
		color->textList.clear();
		color->addText("WATCHER", "OpenSans-ExtraBold.ttf", c, 24, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, 21));
		color->addText("·ISI·", "OpenSans-ExtraBold.ttf", c, 28, Vec((MODULE_SIZE * RACK_GRID_WIDTH) / 2, RACK_GRID_HEIGHT-13));

		//color->addText("", "OpenSans-Bold.ttf", c, 7, Vec(45.35, 257), "descriptor");
	}

	WatcherWidget(Watcher* module) {
		setModule(module);
		//setPanel(createPanel(asset::plugin(pluginInstance, "res/nrandomizer.svg")));

		backdrop = new ImagePanel();
		backdrop->box.size = Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
		backdrop->imagePath = asset::plugin(pluginInstance, "res/watcher.jpg");
		backdrop->scalar = 3.5;
		backdrop->visible = true;

		eye = new WatchersEye();
		eye->box.pos = Vec(130, 235);

		color = new ColorBG(Vec(MODULE_SIZE * RACK_GRID_WIDTH, RACK_GRID_HEIGHT));
		color->drawBackground = false;
		if (module) setText();

		if (module && module->theme.size()) {
			color->drawBackground = true;
			color->setTheme(BG_THEMES[module->theme]);
		}
		if (module) color->setTextGroupVisibility("descriptor", module->showDescriptors);
		
		setPanel(backdrop);

		for (int i = 0; i < 15; i++) {
			Tentacle* tentacle = new Tentacle();
			tentacle->rotAngle = i * 24;
			tentacle->box.pos = Vec(130, 235);
			ts.push_back(tentacle);
			addChild(tentacle);
		}

		addChild(color);
		addChild(eye);

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		for (int i = 0; i < 8; i++) {
			addInput(createInputCentered<QuestionablePort<PJ301MPort>>(mm2px(Vec(12.24, 20.478  + (12.0*float(i)))), module, i));
			//addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(20.24, 15.478 + (10.0*float(i)))), module, i));
		}

	}

};

Model* modelWatcher = createModel<Watcher, WatcherWidget>("watcher");
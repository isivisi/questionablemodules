#pragma once

#include "plugin.hpp"
#include <string>
#include <unordered_map>

struct ColorBGTheme {
	std::string name;
	NVGcolor color;
	NVGcolor stroke;
	NVGcolor fontColor;
};

static std::unordered_map<std::string, ColorBGTheme> BG_THEMES = {
	{"", ColorBGTheme{"", nvgRGB(35, 35, 35), nvgRGB(215, 215, 215), nvgRGB(255,255,255)}}, // just for light text
	{"Light", ColorBGTheme{"Light", nvgRGB(225, 225, 225), nvgRGB(195, 195, 195), nvgRGB(15,15,15)}},
	{"Dark", ColorBGTheme{"Dark", nvgRGB(35, 35, 35), nvgRGB(215, 215, 215), nvgRGB(255,255,255)}},
};

struct ColorBGSimple : Widget {
	NVGcolor color;
	NVGcolor stroke;
	bool drawBackground = true;

	ColorBGSimple(Vec s, NVGcolor c = nvgRGB(225, 225, 225), NVGcolor st = nvgRGB(215, 215, 215)) {
		box.size = s;
		color = c;
		stroke = st;
	}

	void draw(const DrawArgs &args) override {
		if (!drawBackground) return;
		nvgFillColor(args.vg, color);
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0,0, box.size.x, box.size.y);
		nvgStrokeColor(args.vg, stroke);
		nvgStrokeWidth(args.vg, 2.0);
		nvgFill(args.vg);
		nvgStroke(args.vg);
	}
};

struct ColorBG : Widget {
	NVGcolor color = nvgRGB(225, 225, 225);
	NVGcolor stroke = nvgRGB(215, 215, 215);
	Vec size;

	bool drawBackground = true;
	bool drawText = true;

	std::string currTheme = "";

	struct drawableText {
		std::string text = "";
		std::string font = "";
		std::string group = "default";
		bool enabled = true;
		NVGcolor color = nvgRGB(255,255,255);
		float size = 1;
		Vec pos;
		float rotation;
		NVGalign align;
	};
	std::vector<drawableText> textList;

	std::function<void(const DrawArgs&, std::string)> onDraw;

	ColorBG(Vec s) {
		size = s;
	}

	void setFontColor(NVGcolor c) {
		for (size_t i = 0; i < textList.size(); i++) {
			textList[i].color = c;
		}
	}

	void setTheme(ColorBGTheme theme) {
		currTheme = theme.name;
		color = theme.color;
		stroke = theme.stroke;

		setFontColor(theme.fontColor);

		setTextGroupVisibility("nondefault", theme.name != "");
	}

	size_t addText(std::string text, std::string font, NVGcolor color, float size, Vec pos, std::string group="default", float rotation = 0, NVGalign align = NVGalign::NVG_ALIGN_CENTER) {
		textList.push_back(drawableText{text,font,group,true,color,size,pos, rotation, align});
		if (group == "nondefault") textList.back().enabled = currTheme != "";
		return textList.size()-1;
	}

	void updateText(size_t pos, std::string text) {
		textList[pos].text = text;
	}

	void setTextGroupVisibility(std::string group, bool visibility) {
		for (size_t i = 0; i < textList.size(); i++) {
			if (textList[i].group == group) textList[i].enabled = visibility;
		}
	}

	void draw(const DrawArgs &args) override {

		if (drawBackground) {
			nvgFillColor(args.vg, color);
			nvgBeginPath(args.vg);
			nvgRect(args.vg, 0,0, size.x, size.y);
			nvgStrokeColor(args.vg, stroke);
			nvgStrokeWidth(args.vg, 2.0);
			nvgFill(args.vg);
			nvgStroke(args.vg);
		}

		if (onDraw) onDraw(args, currTheme);

		if (drawText) {
			
			for (size_t i = 0; i < textList.size(); i++) {
				drawableText textDef = textList[i];
				if (textDef.enabled) {
					std::shared_ptr<window::Font> font = APP->window->loadFont(asset::plugin(pluginInstance, std::string("res/fonts/") + textDef.font));
					if (!font) return;
					nvgSave(args.vg);
					nvgTranslate(args.vg, textDef.pos.x, textDef.pos.y);
					nvgRotate(args.vg, textDef.rotation);
					nvgFontFaceId(args.vg, font->handle);
					nvgTextLetterSpacing(args.vg, 0.0);
					nvgFontSize(args.vg, textDef.size);
					nvgFillColor(args.vg, textDef.color);
					nvgTextAlign(args.vg, textDef.align);
					nvgText(args.vg, 0, 0, textDef.text.c_str(), NULL);
					nvgRestore(args.vg);
				}
			}
		}
	}
};
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
    {"Light", ColorBGTheme{"Light", nvgRGB(225, 225, 225), nvgRGB(195, 195, 195), nvgRGB(15,15,15)}},
    {"Dark", ColorBGTheme{"Dark", nvgRGB(35, 35, 35), nvgRGB(215, 215, 215), nvgRGB(255,255,255)}},
};

struct ColorBG : Widget {
	NVGcolor color = nvgRGB(225, 225, 225);
    NVGcolor stroke = nvgRGB(215, 215, 215);
    Vec size;

    bool drawBackground = true;
    bool drawText = true;

    std::string currTheme = "";

    struct drawableText {
        std::string text;
        std::string font;
        NVGcolor color;
        float size;
        Vec pos;
    };
    std::vector<drawableText> textList;

    std::function<void(const DrawArgs&, std::string)> onDraw;

    ColorBG(Vec s) {
        size = s;
    }

    void setTheme(ColorBGTheme theme) {
        currTheme = theme.name;
        color = theme.color;
        stroke = theme.stroke;

        for (int i = 0; i < textList.size(); i++) {
            textList[i].color = theme.fontColor;
        }
    }

    void addText(std::string text, std::string font, NVGcolor color, float size, Vec pos) {
        textList.push_back(drawableText{text,font,color,size,pos});
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
            
            for (int i = 0; i < textList.size(); i++) {
                drawableText textDef = textList[i];
                std::shared_ptr<window::Font> font = APP->window->loadFont(asset::plugin(pluginInstance, std::string("res/fonts/") + textDef.font));
                if (!font) return;
                nvgFontFaceId(args.vg, font->handle);
                nvgTextLetterSpacing(args.vg, 0.0);
                nvgFontSize(args.vg, textDef.size);
                nvgFillColor(args.vg, textDef.color);
                nvgTextAlign(args.vg, NVGalign::NVG_ALIGN_CENTER);
                nvgText(args.vg, textDef.pos.x, textDef.pos.y, textDef.text.c_str(), NULL);
            }
        }
	}
};
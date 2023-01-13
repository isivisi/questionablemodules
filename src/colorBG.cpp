#include "plugin.hpp"

struct ColorBG : Widget {
	NVGcolor color = nvgRGB(225, 225, 225);
    Vec size;

    bool drawBackground = true;
    bool drawText = true;

    struct drawableText {
        std::string text;
        std::string font;
        NVGcolor color;
        float size;
        Vec pos;
    };
    std::vector<drawableText> textList;

    ColorBG(Vec s) {
        size = s;
    }

    void addText(std::string text, std::string font, NVGcolor color, float size, Vec pos) {
        textList.push_back(drawableText{text,font,color,size,pos});
    }

	void draw(const DrawArgs &args) override {

        if (drawBackground) {
            nvgFillColor(args.vg, color);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0,0, size.x, size.y);
            nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
            nvgStrokeWidth(args.vg, 3.0);
            nvgFill(args.vg);
        }

        if (drawText) {
            
            for (int i = 0; i < textList.size(); i++) {
                drawableText textDef = textList[i];
                std::shared_ptr<window::Font> font = APP->window->loadFont(asset::plugin(pluginInstance, asset::plugin(pluginInstance, std::string("res/fonts/") + textDef.font)));
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
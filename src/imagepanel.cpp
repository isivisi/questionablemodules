#include "plugin.hpp"

struct ImagePanel : TransparentWidget {
  float scalar = 1.0;
  bool glows = false;
  std::string imagePath;
	void draw(const DrawArgs &args) override {
	  nvgBeginPath(args.vg);
	  nvgRect(args.vg, 0.0, 0.0, box.size.x, box.size.y);

	  // Background image
	  if (!glows) {
	    drawBackground(args.vg);
	  }

	  // Border
	  NVGcolor borderColor = componentlibrary::SCHEME_LIGHT_GRAY; //nvgRGBAf(0.5, 0.5, 0.5, 0.5);
	  nvgBeginPath(args.vg);
	  nvgRect(args.vg, 0.5, 0.5, box.size.x - 1.0, box.size.y - 1.0);
	  nvgStrokeColor(args.vg, borderColor);
	  nvgStrokeWidth(args.vg, 1.0);
	  nvgStroke(args.vg);

	  Widget::draw(args);
	}

	void drawLayer(const DrawArgs &args, int layer) override {
		if (layer == 1) {
			// Background image
			if (glows) {
				drawBackground(args.vg);
			}
		}
	}

	void drawBackground(NVGcontext* vg) {
		int width, height;
		std::shared_ptr<Image> backgroundImage = APP->window->loadImage(imagePath);
		if (backgroundImage) {
			nvgImageSize(vg, backgroundImage->handle, &width, &height);
			NVGpaint paint = nvgImagePattern(vg, 0.0, 0.0, width/scalar, height/scalar, 0.0, backgroundImage->handle, 1.0);
			nvgFillPaint(vg, paint);
			nvgFill(vg);
		}
	}
};
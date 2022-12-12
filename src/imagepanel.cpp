#include "plugin.hpp"

struct ImagePanel : TransparentWidget {
  float scalar = 1.0;
  bool glows = false;
  std::string imagePath;
	void draw(const DrawArgs &args) override {
	  std::shared_ptr<Image> backgroundImage = APP->window->loadImage(imagePath);
	  nvgBeginPath(args.vg);
	  nvgRect(args.vg, 0.0, 0.0, box.size.x, box.size.y);

	  // Background image
	  	if (backgroundImage) {
			int width, height;
			nvgImageSize(args.vg, backgroundImage->handle, &width, &height);
			NVGpaint paint = nvgImagePattern(args.vg, 0.0, 0.0, width/scalar, height/scalar, 0.0, backgroundImage->handle, 1.0);
			nvgFillPaint(args.vg, paint);
			nvgFill(args.vg);
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
};
#include "Adafruit_GFX.h"

template<class Base>
class GfxWrapper : public Adafruit_GFX
{
  public:
	Base &base;
	GfxWrapper(Base &cvbs, const int xres, const int yres)
		:
		Adafruit_GFX(xres, yres),
		base(cvbs)
	{
	}

	virtual void drawPixel(int16_t x, int16_t y, uint16_t color)
	{
		base.monodot(x, y, color);
	}
};
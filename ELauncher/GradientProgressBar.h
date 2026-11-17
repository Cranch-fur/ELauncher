#pragma once
#include "Utilities.h"






class GradientProgressBar
{
public:
	Gdiplus::Rect bounds;

	Gdiplus::Color startColor = Gdiplus::Color::LimeGreen; // The starting color of the gradient.
	Gdiplus::Color endColor = Gdiplus::Color::IndianRed;   // The ending color of the gradient.

	int maximum = 100;
	int value = 0;

	void Draw(Gdiplus::Graphics& graphics) const;
};

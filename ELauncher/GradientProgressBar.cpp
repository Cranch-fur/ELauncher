#include "GradientProgressBar.h"






void GradientProgressBar::Draw(Gdiplus::Graphics& graphics) const
{
	/* Fill the background. */
	Gdiplus::SolidBrush backgroundBrush(Utilities::Drawing::GetControlColor());
	graphics.FillRectangle(&backgroundBrush, bounds);


	/* Draw the gradient progress. */
	if (value > 0 && maximum > 0)
	{
		double progressFraction = (double)value / maximum;
		int progressWidth = (int)(bounds.Width * progressFraction);

		if (progressWidth > 0 && bounds.Height > 0)
		{
			Gdiplus::Rect progressRect(bounds.X, bounds.Y, progressWidth, bounds.Height);
			Gdiplus::LinearGradientBrush gradientBrush(progressRect, startColor, endColor, Gdiplus::LinearGradientModeHorizontal);

			graphics.FillRectangle(&gradientBrush, progressRect);
		}
	}
}

#pragma once
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "Utilities.h"






/*
	Still or animated image, loaded from a file or an embedded resource.
	Anything GDI+ reads works (PNG, JPG, BMP, GIF...), animated GIF and animated PNG (APNG) are played.

	There's no destructor on purpose: the images live as long as the launcher,
	and GDI+ is already shut down by the time static objects would be destroyed.
*/
class AnimatedImage
{
private:
	/* One APNG frame, as described by its fcTL chunk. */
	struct ApngFrame
	{
		Gdiplus::Bitmap* bitmap = nullptr;
		int x = 0;
		int y = 0;
		UINT delay = 0;     // Milliseconds.
		BYTE disposeOp = 0; // What happens to the frame's area before the next one: 0 = nothing, 1 = cleared, 2 = restored.
		BYTE blendOp = 0;   // 0 = frame replaces its area, 1 = frame is drawn over it.
	};


	/* Still images and GIF: GDI+ keeps all frames itself. */
	Gdiplus::Image* image = nullptr;
	IStream* imageStream = nullptr;
	std::vector<UINT> gifDelays;

	/* APNG: frames are drawn one after another onto the canvas, like a browser does. */
	std::vector<ApngFrame> apngFrames;
	Gdiplus::Bitmap* canvas = nullptr;
	Gdiplus::Bitmap* previousCanvas = nullptr;

	/* Current frame resized for drawing, kept until the frame or the size changes. */
	Gdiplus::Bitmap* scaledFrame = nullptr;
	int scaledWidth = 0;
	int scaledHeight = 0;
	UINT scaledFrameVersion = 0;
	UINT frameVersion = 1; // Goes up every time a different frame is shown.

	/* Playback. */
	UINT frameCount = 0;
	UINT loopCount = 0; // 0 = loop forever.
	UINT loopsPlayed = 0;
	UINT currentFrame = 0;
	ULONGLONG frameStartTime = 0;
	bool isFinished = false;


	bool Load(const std::vector<BYTE>& data);
	bool LoadApng(const std::vector<BYTE>& data);
	bool LoadWithGdiplus(const std::vector<BYTE>& data);

	UINT GetFrameDelay(UINT frame) const;
	void ShowFrame(UINT frame);
	void ComposeApngFrame(UINT frame);

	void Clear();
	void Swap(AnimatedImage& other);


public:
	AnimatedImage() = default;
	AnimatedImage(const AnimatedImage&) = delete;
	AnimatedImage& operator=(const AnimatedImage&) = delete;

	bool LoadFromFile(const std::wstring& path);
	bool LoadFromResource(int resourceId);

	Gdiplus::Image* GetFrame() const;
	Gdiplus::Image* GetScaledFrame(int width, int height);
	bool IsAnimated() const;

	bool Update(ULONGLONG currentTime);
};

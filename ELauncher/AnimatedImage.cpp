#include "AnimatedImage.h"






/* GDI+ frame dimension of animated images (FrameDimensionTime). */
static const GUID frameDimensionTime = { 0x6aedbd6d, 0x3fb5, 0x418a, { 0x83, 0xa6, 0x7f, 0x45, 0x22, 0x9d, 0xc8, 0x72 } };

/* Every PNG file starts with these 8 bytes. */
static const BYTE pngSignature[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };




/* PNG stores numbers big-endian. */
static UINT ReadUInt32(const BYTE* data)
{
	return ((UINT)data[0] << 24) | ((UINT)data[1] << 16) | ((UINT)data[2] << 8) | (UINT)data[3];
}

static USHORT ReadUInt16(const BYTE* data)
{
	return (USHORT)((data[0] << 8) | data[1]);
}

static void WriteUInt32(BYTE* data, UINT value)
{
	data[0] = (BYTE)(value >> 24);
	data[1] = (BYTE)(value >> 16);
	data[2] = (BYTE)(value >> 8);
	data[3] = (BYTE)value;
}

/* CRC-32 that every PNG chunk ends with. */
static UINT Crc32(const BYTE* data, size_t size)
{
	static UINT table[256] = {};
	if (table[1] == 0)
	{
		for (UINT i = 0; i < 256; i++)
		{
			UINT value = i;
			for (int bit = 0; bit < 8; bit++)
				value = (value & 1) ? 0xEDB88320 ^ (value >> 1) : value >> 1;

			table[i] = value;
		}
	}

	UINT crc = 0xFFFFFFFF;
	for (size_t i = 0; i < size; i++)
		crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);

	return crc ^ 0xFFFFFFFF;
}

/* Adds a chunk (length, type, data, CRC) to the end of a PNG file. */
static void AppendChunk(std::vector<BYTE>& png, const char* type, const BYTE* data, size_t size)
{
	BYTE length[4];
	WriteUInt32(length, (UINT)size);
	png.insert(png.end(), length, length + 4);

	size_t typeOffset = png.size();
	png.insert(png.end(), type, type + 4);
	if (size > 0)
		png.insert(png.end(), data, data + size);

	BYTE crc[4];
	WriteUInt32(crc, Crc32(png.data() + typeOffset, size + 4));
	png.insert(png.end(), crc, crc + 4);
}

/* Decodes a complete PNG file with GDI+ into a 32-bit bitmap that no longer needs the file data. */
static Gdiplus::Bitmap* DecodePng(const std::vector<BYTE>& png)
{
	IStream* stream = SHCreateMemStream(png.data(), (UINT)png.size());
	if (stream == nullptr)
		return nullptr;

	Gdiplus::Bitmap* decoded = Gdiplus::Bitmap::FromStream(stream);
	Gdiplus::Bitmap* bitmap = nullptr;

	if (decoded != nullptr && decoded->GetLastStatus() == Gdiplus::Ok)
	{
		INT width = (INT)decoded->GetWidth();
		INT height = (INT)decoded->GetHeight();

		bitmap = new Gdiplus::Bitmap(width, height, PixelFormat32bppARGB);
		Gdiplus::Graphics graphics(bitmap);
		graphics.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
		Utilities::Drawing::DrawImageUnscaled(graphics, decoded, 0, 0);
	}

	delete decoded;
	stream->Release();

	return bitmap;
}




bool AnimatedImage::LoadFromFile(const std::wstring& path)
{
	return Load(Utilities::File::ReadAllBytes(path));
}

bool AnimatedImage::LoadFromResource(int resourceId)
{
	return Load(Utilities::Resource::ReadAllBytes(resourceId));
}

/* The current image is only replaced if the new one loads, otherwise it's kept as it is. */
bool AnimatedImage::Load(const std::vector<BYTE>& data)
{
	if (data.empty())
		return false;

	AnimatedImage loaded;
	if (loaded.LoadApng(data) == false && loaded.LoadWithGdiplus(data) == false)
		return false;

	Clear();
	Swap(loaded);

	return true;
}

/*
	An APNG is a normal PNG with extra chunks:
	acTL (frame count and loops), fcTL (one per frame: size, position, delay, dispose and blend) and fdAT (image data of frames after the first).

	Every frame is rebuilt into a small standalone PNG (header with the frame's size + the shared chunks + its image data),
	which GDI+ then decodes like any other PNG, so no extra decompression library is needed.
*/
bool AnimatedImage::LoadApng(const std::vector<BYTE>& data)
{
	if (data.size() < 8 || memcmp(data.data(), pngSignature, 8) != 0)
		return false;


	struct ParsedFrame
	{
		std::vector<BYTE> control;   // fcTL contents.
		std::vector<BYTE> imageData; // IDAT / fdAT contents.
	};

	std::vector<BYTE> header;       // IHDR contents.
	std::vector<BYTE> sharedChunks; // Chunks every frame needs (palette, transparency, color space...), copied as they are.
	std::vector<ParsedFrame> parsedFrames;
	bool hasAnimationControl = false;
	bool hasImageData = false;

	size_t offset = 8;
	while (offset + 12 <= data.size())
	{
		UINT length = ReadUInt32(&data[offset]);
		if (length > data.size() - offset - 12)
			break; // Damaged file, use whatever was read so far.

		std::string type((const char*)&data[offset + 4], 4);
		const BYTE* chunkData = &data[offset + 8];

		if (type == "IHDR")
			header.assign(chunkData, chunkData + length);
		else if (type == "acTL" && length >= 8)
		{
			hasAnimationControl = true;
			loopCount = ReadUInt32(chunkData + 4);
		}
		else if (type == "fcTL" && length >= 26)
			parsedFrames.push_back({ std::vector<BYTE>(chunkData, chunkData + length), {} });
		else if (type == "IDAT")
		{
			hasImageData = true;

			/* The regular image is the first frame only if an fcTL came before it, otherwise it's just a still fallback. */
			if (parsedFrames.size() == 1)
				parsedFrames[0].imageData.insert(parsedFrames[0].imageData.end(), chunkData, chunkData + length);
		}
		else if (type == "fdAT" && length > 4)
		{
			if (parsedFrames.empty() == false)
				parsedFrames.back().imageData.insert(parsedFrames.back().imageData.end(), chunkData + 4, chunkData + length); // First 4 bytes are a sequence number.
		}
		else if (type == "IEND")
			break;
		else if (hasImageData == false)
			sharedChunks.insert(sharedChunks.end(), data.begin() + offset, data.begin() + offset + 12 + length);

		offset += 12 + length;
	}

	if (hasAnimationControl == false || header.size() != 13 || parsedFrames.empty())
		return false;


	UINT width = ReadUInt32(&header[0]);
	UINT height = ReadUInt32(&header[4]);
	canvas = new Gdiplus::Bitmap(width, height, PixelFormat32bppARGB);
	previousCanvas = new Gdiplus::Bitmap(width, height, PixelFormat32bppARGB);

	for (ParsedFrame& parsed : parsedFrames)
	{
		if (parsed.imageData.empty())
			continue;

		const BYTE* control = parsed.control.data();

		/* Standalone PNG of this frame. */
		std::vector<BYTE> frameHeader = header;
		WriteUInt32(&frameHeader[0], ReadUInt32(control + 4)); // Frame width.
		WriteUInt32(&frameHeader[4], ReadUInt32(control + 8)); // Frame height.

		std::vector<BYTE> png(pngSignature, pngSignature + 8);
		AppendChunk(png, "IHDR", frameHeader.data(), frameHeader.size());
		png.insert(png.end(), sharedChunks.begin(), sharedChunks.end());
		AppendChunk(png, "IDAT", parsed.imageData.data(), parsed.imageData.size());
		AppendChunk(png, "IEND", nullptr, 0);

		Gdiplus::Bitmap* bitmap = DecodePng(png);
		if (bitmap == nullptr)
			continue;


		ApngFrame frame;
		frame.bitmap = bitmap;
		frame.x = (int)ReadUInt32(control + 12);
		frame.y = (int)ReadUInt32(control + 16);
		frame.disposeOp = control[24];
		frame.blendOp = control[25];

		/* The delay is a fraction of a second, a denominator of 0 means 1/100. */
		USHORT delayNumerator = ReadUInt16(control + 20);
		USHORT delayDenominator = ReadUInt16(control + 22);
		frame.delay = (delayNumerator * 1000) / (delayDenominator == 0 ? 100 : delayDenominator);

		apngFrames.push_back(frame);
	}

	if (apngFrames.empty())
	{
		Clear();
		return false;
	}


	/* The first frame has no earlier state to restore, so "restore" means "clear" for it (APNG specification). */
	if (apngFrames[0].disposeOp == 2)
		apngFrames[0].disposeOp = 1;

	frameCount = (UINT)apngFrames.size();
	ComposeApngFrame(0);

	return true;
}

/* Still images in any format GDI+ supports, and GIF (animated or not). */
bool AnimatedImage::LoadWithGdiplus(const std::vector<BYTE>& data)
{
	/* GDI+ reads from the stream for as long as the image exists, so it's kept until Clear(). */
	imageStream = SHCreateMemStream(data.data(), (UINT)data.size());
	if (imageStream == nullptr)
		return false;

	image = Gdiplus::Image::FromStream(imageStream);
	if (image == nullptr || image->GetLastStatus() != Gdiplus::Ok)
	{
		Clear();
		return false;
	}

	frameCount = image->GetFrameCount(&frameDimensionTime);
	if (frameCount <= 1)
	{
		frameCount = 1;
		return true;
	}


	/* Frame delays, stored in 1/100 of a second. */
	UINT delaysSize = image->GetPropertyItemSize(PropertyTagFrameDelay);
	if (delaysSize > 0)
	{
		std::vector<BYTE> buffer(delaysSize);
		Gdiplus::PropertyItem* delays = (Gdiplus::PropertyItem*)buffer.data();

		if (image->GetPropertyItem(PropertyTagFrameDelay, delaysSize, delays) == Gdiplus::Ok)
		{
			const LONG* values = (const LONG*)delays->value;
			UINT valueCount = delays->length / sizeof(LONG);

			for (UINT frame = 0; frame < frameCount; frame++)
				gifDelays.push_back(frame < valueCount ? (UINT)values[frame] * 10 : 100);
		}
	}

	/* Loop count, a GIF without one plays once. */
	loopCount = 1;
	UINT loopSize = image->GetPropertyItemSize(PropertyTagLoopCount);
	if (loopSize > 0)
	{
		std::vector<BYTE> buffer(loopSize);
		Gdiplus::PropertyItem* loops = (Gdiplus::PropertyItem*)buffer.data();

		if (image->GetPropertyItem(PropertyTagLoopCount, loopSize, loops) == Gdiplus::Ok)
			loopCount = *(const USHORT*)loops->value;
	}

	return true;
}




/* Browsers show frames shorter than 10 ms for 100 ms, as old GIFs relied on it, so we do the same. */
UINT AnimatedImage::GetFrameDelay(UINT frame) const
{
	UINT delay = 100;
	if (apngFrames.empty() == false)
		delay = apngFrames[frame].delay;
	else if (frame < gifDelays.size())
		delay = gifDelays[frame];

	if (delay <= 10)
		delay = 100;

	return delay;
}

void AnimatedImage::ShowFrame(UINT frame)
{
	if (apngFrames.empty() == false)
		ComposeApngFrame(frame);
	else
		image->SelectActiveFrame(&frameDimensionTime, frame);

	currentFrame = frame;
	frameVersion++;
}

/*
	Draws an APNG frame onto the canvas:
	1. The previous frame's area is cleaned up as it asked (left as it is, cleared, or restored to how it was before it).
	2. If this frame asks to be restored later, the canvas is backed up first.
	3. The frame is drawn at its position, either replacing its area or over it.
*/
void AnimatedImage::ComposeApngFrame(UINT frame)
{
	{
		Gdiplus::Graphics graphics(canvas);
		graphics.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);

		if (frame == 0)
			graphics.Clear(Gdiplus::Color(0, 0, 0, 0)); // Every loop starts from an empty canvas.
		else
		{
			const ApngFrame& previous = apngFrames[frame - 1];
			if (previous.disposeOp == 1)
			{
				Gdiplus::SolidBrush transparentBrush(Gdiplus::Color(0, 0, 0, 0));
				graphics.FillRectangle(&transparentBrush, previous.x, previous.y, (INT)previous.bitmap->GetWidth(), (INT)previous.bitmap->GetHeight());
			}
			else if (previous.disposeOp == 2)
				Utilities::Drawing::DrawImageUnscaled(graphics, previousCanvas, 0, 0);
		}
	}

	const ApngFrame& current = apngFrames[frame];
	if (current.disposeOp == 2)
	{
		Gdiplus::Graphics backup(previousCanvas);
		backup.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
		Utilities::Drawing::DrawImageUnscaled(backup, canvas, 0, 0);
	}

	{
		Gdiplus::Graphics graphics(canvas);
		graphics.SetCompositingMode(current.blendOp == 1 ? Gdiplus::CompositingModeSourceOver : Gdiplus::CompositingModeSourceCopy);
		Utilities::Drawing::DrawImageUnscaled(graphics, current.bitmap, current.x, current.y);
	}
}




/* Frame to draw right now. */
Gdiplus::Image* AnimatedImage::GetFrame() const
{
	if (canvas != nullptr)
		return canvas;

	return image;
}

/*
	Current frame resized to the given size with proper filtering (see Utilities::Drawing::ResizeImage).
	The result is kept until the frame or the size changes, so repainting doesn't resize again.
*/
Gdiplus::Image* AnimatedImage::GetScaledFrame(int width, int height)
{
	Gdiplus::Image* frame = GetFrame();
	if (frame == nullptr)
		return nullptr;

	if ((int)frame->GetWidth() == width && (int)frame->GetHeight() == height)
		return frame;

	if (scaledFrame == nullptr || scaledFrameVersion != frameVersion || scaledWidth != width || scaledHeight != height)
	{
		delete scaledFrame;
		scaledFrame = Utilities::Drawing::ResizeImage(frame, width, height);
		scaledFrameVersion = frameVersion;
		scaledWidth = width;
		scaledHeight = height;
	}


	if (scaledFrame == nullptr)
		return frame; // Couldn't resize (out of memory), GDI+ scales it while drawing instead.

	return scaledFrame;
}

bool AnimatedImage::IsAnimated() const
{
	return frameCount > 1;
}

/* Moves the animation forward to the given time (GetTickCount64), returns true if a different frame should be drawn now. */
bool AnimatedImage::Update(ULONGLONG currentTime)
{
	if (IsAnimated() == false || isFinished)
		return false;

	if (frameStartTime == 0)
	{
		frameStartTime = currentTime;
		return false;
	}

	/* After a long stall, carry on from now instead of rushing through the missed frames. */
	if (currentTime - frameStartTime > 1000 + GetFrameDelay(currentFrame))
		frameStartTime = currentTime - GetFrameDelay(currentFrame);


	bool hasChanged = false;
	while (isFinished == false && currentTime - frameStartTime >= GetFrameDelay(currentFrame))
	{
		frameStartTime += GetFrameDelay(currentFrame);

		UINT nextFrame = currentFrame + 1;
		if (nextFrame >= frameCount)
		{
			loopsPlayed++;
			if (loopCount != 0 && loopsPlayed >= loopCount)
			{
				isFinished = true; // Stays on the last frame.
				break;
			}

			nextFrame = 0;
		}

		ShowFrame(nextFrame);
		hasChanged = true;
	}


	return hasChanged;
}




void AnimatedImage::Clear()
{
	delete image;
	image = nullptr;

	if (imageStream != nullptr)
		imageStream->Release();
	imageStream = nullptr;

	for (ApngFrame& frame : apngFrames)
		delete frame.bitmap;
	apngFrames.clear();

	delete canvas;
	canvas = nullptr;
	delete previousCanvas;
	previousCanvas = nullptr;

	delete scaledFrame;
	scaledFrame = nullptr;
	scaledWidth = 0;
	scaledHeight = 0;
	scaledFrameVersion = 0;
	frameVersion = 1;

	gifDelays.clear();
	frameCount = 0;
	loopCount = 0;
	loopsPlayed = 0;
	currentFrame = 0;
	frameStartTime = 0;
	isFinished = false;
}

void AnimatedImage::Swap(AnimatedImage& other)
{
	std::swap(image, other.image);
	std::swap(imageStream, other.imageStream);
	std::swap(gifDelays, other.gifDelays);
	std::swap(apngFrames, other.apngFrames);
	std::swap(canvas, other.canvas);
	std::swap(previousCanvas, other.previousCanvas);
	std::swap(scaledFrame, other.scaledFrame);
	std::swap(scaledWidth, other.scaledWidth);
	std::swap(scaledHeight, other.scaledHeight);
	std::swap(scaledFrameVersion, other.scaledFrameVersion);
	std::swap(frameVersion, other.frameVersion);
	std::swap(frameCount, other.frameCount);
	std::swap(loopCount, other.loopCount);
	std::swap(loopsPlayed, other.loopsPlayed);
	std::swap(currentFrame, other.currentFrame);
	std::swap(frameStartTime, other.frameStartTime);
	std::swap(isFinished, other.isFinished);
}

#include "Utilities.h"






bool Utilities::File::Exists(const std::wstring& path)
{
	if (path.empty())
		return false;

	DWORD attributes = GetFileAttributesW(path.c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

/* Returns an empty vector if the file can't be read. */
std::vector<BYTE> Utilities::File::ReadAllBytes(const std::wstring& path)
{
	std::vector<BYTE> bytes;

	HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		return bytes;

	BYTE buffer[4096];
	DWORD bytesRead = 0;
	while (ReadFile(file, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0)
		bytes.insert(bytes.end(), buffer, buffer + bytesRead);

	CloseHandle(file);
	return bytes;
}

/*
	Reads the whole file as UTF-8, or as UTF-16 if it starts with a UTF-16 byte order mark.
	A UTF-8 byte order mark is skipped.
*/
std::wstring Utilities::File::ReadAllText(const std::wstring& path)
{
	std::vector<BYTE> bytes = ReadAllBytes(path);


	/* UTF-16 LE. */
	if (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE)
		return std::wstring((const wchar_t*)(bytes.data() + 2), (bytes.size() - 2) / sizeof(wchar_t));

	/* UTF-8, with or without byte order mark. */
	size_t start = 0;
	if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
		start = 3;

	const char* text = (const char*)bytes.data() + start;
	int textSize = (int)(bytes.size() - start);

	int length = MultiByteToWideChar(CP_UTF8, 0, text, textSize, nullptr, 0);
	std::wstring result(length, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, text, textSize, result.data(), length);

	return result;
}

/* Splits the file into lines, a line break at the very end doesn't add an empty line. */
std::vector<std::wstring> Utilities::File::ReadAllLines(const std::wstring& path)
{
	std::vector<std::wstring> lines;
	std::wstring text = ReadAllText(path);
	std::wstring line;

	for (size_t i = 0; i < text.size(); i++)
	{
		if (text[i] == L'\r' || text[i] == L'\n')
		{
			lines.push_back(line);
			line.clear();

			if (text[i] == L'\r' && i + 1 < text.size() && text[i + 1] == L'\n')
				i++; // Skip the "\n" of a "\r\n" line break.
		}
		else
			line += text[i];
	}

	if (line.empty() == false)
		lines.push_back(line);

	return lines;
}




/* Data embedded with ELauncher.rc as RCDATA, an empty vector if it's missing. */
std::vector<BYTE> Utilities::Resource::ReadAllBytes(int resourceId)
{
	HMODULE module = GetModuleHandleW(nullptr);
	HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
	if (resource == nullptr)
		return std::vector<BYTE>();

	const BYTE* data = (const BYTE*)LockResource(LoadResource(module, resource));
	DWORD size = SizeofResource(module, resource);
	if (data == nullptr)
		return std::vector<BYTE>();

	return std::vector<BYTE>(data, data + size);
}




bool Utilities::Directory::Exists(const std::wstring& path)
{
	if (path.empty())
		return false;

	DWORD attributes = GetFileAttributesW(path.c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}




/* Folder the launcher executable is in, with a trailing backslash. */
std::wstring Utilities::Path::GetLauncherDirectory()
{
	wchar_t executablePath[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, executablePath, MAX_PATH);

	std::wstring directory = executablePath;
	return directory.substr(0, directory.find_last_of(L"\\/") + 1);
}

std::wstring Utilities::Path::GetDirectoryName(const std::wstring& path)
{
	size_t separator = path.find_last_of(L"\\/");
	if (separator == std::wstring::npos)
		return std::wstring();

	/* Keep the backslash of a drive root, "C:\game.exe" -> "C:\". */
	if (separator == 2 && path[1] == L':')
		return path.substr(0, 3);

	return path.substr(0, separator);
}

/* Same as Path.Combine: an absolute path is returned as it is. */
std::wstring Utilities::Path::Combine(const std::wstring& directory, const std::wstring& path)
{
	bool isRooted = (path.empty() == false && (path[0] == L'\\' || path[0] == L'/')) || (path.size() >= 2 && path[1] == L':');
	if (isRooted || directory.empty())
		return path;

	if (directory.back() == L'\\' || directory.back() == L'/')
		return directory + path;

	return directory + L"\\" + path;
}




/* Removes line breaks left at the end by text editors. */
std::wstring Utilities::String::TrimLineBreaks(std::wstring string)
{
	while (string.empty() == false && (string.back() == L'\r' || string.back() == L'\n'))
		string.pop_back();

	return string;
}




/* Returns an empty string if the variable isn't set. */
std::wstring Utilities::Environment::GetVariable(const std::wstring& name)
{
	DWORD length = GetEnvironmentVariableW(name.c_str(), nullptr, 0);
	if (length == 0)
		return std::wstring();

	std::wstring value(length, L'\0');
	length = GetEnvironmentVariableW(name.c_str(), value.data(), length);
	value.resize(length);

	return value;
}

/* Processes started afterwards inherit the variable. */
void Utilities::Environment::SetVariable(const std::wstring& name, const std::wstring& value)
{
	SetEnvironmentVariableW(name.c_str(), value.c_str());
}




/* Value under HKEY_CURRENT_USER, returns false if it doesn't exist or isn't a DWORD. */
bool Utilities::Registry::TryGetUint32(const std::wstring& path, const std::wstring& keyName, uint32_t& value)
{
	if (path.empty() || keyName.empty())
		return false;

	DWORD data = 0;
	DWORD dataSize = sizeof(DWORD);
	LSTATUS status = RegGetValueW(HKEY_CURRENT_USER, path.c_str(), keyName.c_str(), RRF_RT_REG_DWORD, nullptr, &data, &dataSize);

	if (status != ERROR_SUCCESS)
		return false;

	value = (uint32_t)data;
	return true;
}

/* Value under HKEY_CURRENT_USER, defaultValue if it doesn't exist. */
uint32_t Utilities::Registry::GetUint32(const std::wstring& path, const std::wstring& keyName, uint32_t defaultValue)
{
	uint32_t value = defaultValue;
	TryGetUint32(path, keyName, value);

	return value;
}

/* Value under HKEY_CURRENT_USER, the key is created if it doesn't exist yet. */
bool Utilities::Registry::SetUint32(const std::wstring& path, const std::wstring& keyName, uint32_t value)
{
	if (path.empty() || keyName.empty())
		return false;

	HKEY key = nullptr;
	LSTATUS status = RegCreateKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr);
	if (status != ERROR_SUCCESS)
		return false;

	DWORD data = (DWORD)value;
	status = RegSetValueExW(key, keyName.c_str(), 0, REG_DWORD, (const BYTE*)&data, sizeof(DWORD));
	RegCloseKey(key);

	return status == ERROR_SUCCESS;
}




/* Wine's ntdll.dll exports wine_get_version, the Windows one doesn't. */
bool Utilities::Wine::IsRunning()
{
	HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
	return ntdll != nullptr && GetProcAddress(ntdll, "wine_get_version") != nullptr;
}




/* Fits the image inside the bounds keeping its aspect ratio, centered (WinForms "Zoom" layout). */
Gdiplus::Rect Utilities::Drawing::GetZoomRectangle(Gdiplus::Image* image, const Gdiplus::Rect& bounds)
{
	float imageWidth = (float)image->GetWidth();
	float imageHeight = (float)image->GetHeight();

	float ratio = (float)bounds.Width / imageWidth;
	if ((float)bounds.Height / imageHeight < ratio)
		ratio = (float)bounds.Height / imageHeight;

	int width = (int)(imageWidth * ratio + 0.5f);
	int height = (int)(imageHeight * ratio + 0.5f);

	return Gdiplus::Rect(bounds.X + (bounds.Width - width) / 2, bounds.Y + (bounds.Height - height) / 2, width, height);
}

/* Bicubic filter (a = -0.5), the same curve Pillow and most image editors use. */
static double BicubicFilter(double x)
{
	const double a = -0.5;

	if (x < 0.0)
		x = -x;

	if (x < 1.0)
		return ((a + 2.0) * x - (a + 3.0)) * x * x + 1.0;

	if (x < 2.0)
		return (((x - 5.0) * x + 8.0) * x - 4.0) * a;

	return 0.0;
}

/* Which source pixels make up one target pixel, and how much each of them counts. */
struct ResampleWeights
{
	int first = 0;
	std::vector<float> weights;
};

/*
	When shrinking, the filter is stretched by the shrink factor, so every source pixel counts towards the result
	(e.g. 1600 -> 800 blends about 8 pixels per row instead of 2) - that's what keeps it smooth instead of pixelated.
*/
static std::vector<ResampleWeights> GetResampleWeights(int sourceSize, int targetSize)
{
	std::vector<ResampleWeights> result(targetSize);

	double scale = (double)sourceSize / targetSize;
	double filterScale = scale > 1.0 ? scale : 1.0;
	double support = 2.0 * filterScale;

	for (int i = 0; i < targetSize; i++)
	{
		double center = (i + 0.5) * scale;

		int first = (int)(center - support + 0.5);
		int last = (int)(center + support + 0.5);
		if (first < 0)
			first = 0;
		if (last > sourceSize)
			last = sourceSize;

		double total = 0.0;
		for (int j = first; j < last; j++)
		{
			float weight = (float)BicubicFilter((j + 0.5 - center) / filterScale);
			result[i].weights.push_back(weight);
			total += weight;
		}

		/* Weights add up to 1, so brightness is kept and edges don't darken. */
		if (total != 0.0)
		{
			for (float& weight : result[i].weights)
				weight = (float)(weight / total);
		}

		result[i].first = first;
	}

	return result;
}

/*
	Resizes an image with a bicubic filter, done in our own code instead of GDI+:
	GDI+'s default (bilinear) only looks at 4 pixels, so shrinking skips most of the image and looks pixelated,
	and Wine's GDI+ doesn't implement the high quality modes at all.

	Works on premultiplied pixels, so transparent edges don't get dark fringes.
	Returns nullptr if the bitmap can't be created.
*/
Gdiplus::Bitmap* Utilities::Drawing::ResizeImage(Gdiplus::Image* source, int width, int height)
{
	int sourceWidth = (int)source->GetWidth();
	int sourceHeight = (int)source->GetHeight();
	if (width <= 0 || height <= 0 || sourceWidth <= 0 || sourceHeight <= 0)
		return nullptr;


	/* Source pixels as 32-bit premultiplied, whatever format the image had. */
	Gdiplus::Bitmap sourceCopy(sourceWidth, sourceHeight, PixelFormat32bppPARGB);
	{
		Gdiplus::Graphics graphics(&sourceCopy);
		graphics.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
		DrawImageUnscaled(graphics, source, 0, 0);
	}

	Gdiplus::Rect sourceRect(0, 0, sourceWidth, sourceHeight);
	Gdiplus::BitmapData sourceData = {};
	if (sourceCopy.LockBits(&sourceRect, Gdiplus::ImageLockModeRead, PixelFormat32bppPARGB, &sourceData) != Gdiplus::Ok)
		return nullptr;


	/* Horizontal pass: every source row is resized to the target width. */
	std::vector<ResampleWeights> columns = GetResampleWeights(sourceWidth, width);
	std::vector<float> horizontal((size_t)width * sourceHeight * 4);

	for (int y = 0; y < sourceHeight; y++)
	{
		const BYTE* sourceRow = (const BYTE*)sourceData.Scan0 + (INT_PTR)y * sourceData.Stride;
		float* targetRow = &horizontal[(size_t)y * width * 4];

		for (int x = 0; x < width; x++)
		{
			const ResampleWeights& column = columns[x];
			float sum[4] = {};

			for (size_t k = 0; k < column.weights.size(); k++)
			{
				const BYTE* pixel = sourceRow + (size_t)(column.first + k) * 4;
				for (int channel = 0; channel < 4; channel++)
					sum[channel] += pixel[channel] * column.weights[k];
			}

			for (int channel = 0; channel < 4; channel++)
				targetRow[x * 4 + channel] = sum[channel];
		}
	}

	sourceCopy.UnlockBits(&sourceData);


	/* Vertical pass: the columns are resized to the target height, straight into the result. */
	Gdiplus::Bitmap* target = new Gdiplus::Bitmap(width, height, PixelFormat32bppPARGB);
	Gdiplus::Rect targetRect(0, 0, width, height);
	Gdiplus::BitmapData targetData = {};
	if (target->GetLastStatus() != Gdiplus::Ok || target->LockBits(&targetRect, Gdiplus::ImageLockModeWrite, PixelFormat32bppPARGB, &targetData) != Gdiplus::Ok)
	{
		delete target;
		return nullptr;
	}

	std::vector<ResampleWeights> rows = GetResampleWeights(sourceHeight, height);
	for (int y = 0; y < height; y++)
	{
		const ResampleWeights& row = rows[y];
		BYTE* targetRow = (BYTE*)targetData.Scan0 + (INT_PTR)y * targetData.Stride;

		for (int x = 0; x < width; x++)
		{
			float sum[4] = {};
			for (size_t k = 0; k < row.weights.size(); k++)
			{
				const float* pixel = &horizontal[(((size_t)row.first + k) * width + x) * 4];
				for (int channel = 0; channel < 4; channel++)
					sum[channel] += pixel[channel] * row.weights[k];
			}

			/* Bicubic can overshoot slightly, keep every channel in range and colors within alpha (premultiplied). */
			BYTE* pixel = targetRow + (size_t)x * 4;
			float alpha = sum[3] < 0.0f ? 0.0f : (sum[3] > 255.0f ? 255.0f : sum[3]);
			pixel[3] = (BYTE)(alpha + 0.5f);

			for (int channel = 0; channel < 3; channel++)
			{
				float value = sum[channel] < 0.0f ? 0.0f : sum[channel];
				if (value > pixel[3])
					value = pixel[3];

				pixel[channel] = (BYTE)(value + 0.5f);
			}
		}
	}

	target->UnlockBits(&targetData);


	return target;
}




/*
	Draws the image pixel for pixel at the given position.
	A target size is always given, otherwise GDI+ resizes by the DPI saved in the file (a 72 DPI image would come out a third bigger).
*/
void Utilities::Drawing::DrawImageUnscaled(Gdiplus::Graphics& graphics, Gdiplus::Image* image, int x, int y)
{
	INT width = (INT)image->GetWidth();
	INT height = (INT)image->GetHeight();
	graphics.DrawImage(image, Gdiplus::Rect(x, y, width, height), 0, 0, width, height, Gdiplus::UnitPixel);
}

/* GDI+ has no rounded rectangle, so it's built from four corner arcs. */
void Utilities::Drawing::FillRoundedRectangle(Gdiplus::Graphics& graphics, const Gdiplus::Brush* brush, const Gdiplus::RectF& bounds, float radius)
{
	float diameter = radius * 2.0f;

	Gdiplus::GraphicsPath path;
	path.AddArc(bounds.X, bounds.Y, diameter, diameter, 180.0f, 90.0f);
	path.AddArc(bounds.X + bounds.Width - diameter, bounds.Y, diameter, diameter, 270.0f, 90.0f);
	path.AddArc(bounds.X + bounds.Width - diameter, bounds.Y + bounds.Height - diameter, diameter, diameter, 0.0f, 90.0f);
	path.AddArc(bounds.X, bounds.Y + bounds.Height - diameter, diameter, diameter, 90.0f, 90.0f);
	path.CloseFigure();

	graphics.FillPath(brush, &path);
}

/* SystemColors.Control: the default background color of WinForms windows. */
Gdiplus::Color Utilities::Drawing::GetControlColor()
{
	COLORREF color = GetSysColor(COLOR_BTNFACE);
	return Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color));
}

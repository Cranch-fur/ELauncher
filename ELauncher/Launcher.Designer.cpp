#include "Launcher.h"
#include "resource.h"






void Launcher::InitializeComponent()
{
	/* startupLogo. */
	startupLogoImage.LoadFromResource(IDR_IMG_LOGO);

	/* startupPlaytime: mirrors startupLogo in the bottom-right corner, uses the embedded font so it looks the same everywhere (Wine included). */
	labelFontData = Utilities::Resource::ReadAllBytes(IDR_FONT_LABEL);
	labelFonts = new Gdiplus::PrivateFontCollection();
	if (labelFontData.empty() == false && labelFonts->AddMemoryFont(labelFontData.data(), (INT)labelFontData.size()) == Gdiplus::Ok)
	{
		INT familiesFound = 0;
		labelFontFamily = new Gdiplus::FontFamily();
		labelFonts->GetFamilies(1, labelFontFamily, &familiesFound);
		if (familiesFound == 0)
		{
			delete labelFontFamily;
			labelFontFamily = nullptr;
		}
	}

	/* startupProgressBar. */
	startupProgressBar.maximum = 1000;
	startupProgressBar.startColor = Gdiplus::Color::DodgerBlue;
	startupProgressBar.endColor = Gdiplus::Color::PaleTurquoise;

	/* Launcher. */
	backgroundImage.LoadFromResource(IDR_IMG_SPLASHSCREEN);
	clientWidth = designWidth;
	clientHeight = designHeight;
	UpdateLayout();
}

/* DPI of a monitor (96 = 100%). Windows 8.1+ has a value per monitor, older systems and Wine fall back to the system one. */
UINT Launcher::GetDpi(HMONITOR monitor)
{
	typedef HRESULT(WINAPI* GetDpiForMonitorFunction)(HMONITOR, int, UINT*, UINT*);

	static GetDpiForMonitorFunction getDpiForMonitor = []() -> GetDpiForMonitorFunction
	{
		HMODULE shcore = LoadLibraryW(L"shcore.dll");
		return shcore != nullptr ? (GetDpiForMonitorFunction)GetProcAddress(shcore, "GetDpiForMonitor") : nullptr;
	}();

	UINT dpiX = 0;
	UINT dpiY = 0;
	if (getDpiForMonitor != nullptr && SUCCEEDED(getDpiForMonitor(monitor, 0, &dpiX, &dpiY)) && dpiX > 0) // 0 = MDT_EFFECTIVE_DPI.
		return dpiX;

	HDC screen = GetDC(nullptr);
	int systemDpi = GetDeviceCaps(screen, LOGPIXELSX);
	ReleaseDC(nullptr, screen);

	return systemDpi > 0 ? (UINT)systemDpi : 96;
}

/* Turns a rectangle from the 800x450 design into the current window size. */
Gdiplus::Rect Launcher::ScaleDesignRect(const Gdiplus::Rect& designBounds)
{
	int left = (int)std::lround((double)designBounds.X * clientWidth / designWidth);
	int top = (int)std::lround((double)designBounds.Y * clientHeight / designHeight);
	int right = (int)std::lround((double)(designBounds.X + designBounds.Width) * clientWidth / designWidth);
	int bottom = (int)std::lround((double)(designBounds.Y + designBounds.Height) * clientHeight / designHeight);

	return Gdiplus::Rect(left, top, right - left, bottom - top);
}

/* Places everything for the current window size. */
void Launcher::UpdateLayout()
{
	float scaleX = (float)clientWidth / designWidth;
	float scaleY = (float)clientHeight / designHeight;
	layoutScale = scaleX < scaleY ? scaleX : scaleY;

	startupLogoBounds = ScaleDesignRect(startupLogoDesignBounds);
	startupPlaytimeBounds = ScaleDesignRect(startupPlaytimeDesignBounds);
	startupProgressBar.bounds = ScaleDesignRect(startupProgressBarDesignBounds);
}

bool Launcher::CreateHandle(HINSTANCE hInstance)
{
	WNDCLASSEXW windowClass = {};
	windowClass.cbSize = sizeof(WNDCLASSEXW);
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = hInstance;
	windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	windowClass.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_ICON));
	windowClass.hIconSm = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_ICON), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	windowClass.lpszClassName = L"ELauncher";

	if (RegisterClassExW(&windowClass) == 0)
		return false;


	/* Center the window on the screen the mouse cursor is on (FormStartPosition.CenterScreen). */
	POINT cursorPosition = {};
	GetCursorPos(&cursorPosition);

	HMONITOR monitor = MonitorFromPoint(cursorPosition, MONITOR_DEFAULTTONEAREST);
	MONITORINFO monitorInfo = {};
	monitorInfo.cbSize = sizeof(MONITORINFO);
	GetMonitorInfoW(monitor, &monitorInfo);

	/* Sized for the screen's scaling (150% = 1200x675), so it's not tiny on high DPI screens. */
	UINT dpi = GetDpi(monitor);
	clientWidth = MulDiv(designWidth, dpi, 96);
	clientHeight = MulDiv(designHeight, dpi, 96);
	UpdateLayout();

	const RECT& workArea = monitorInfo.rcWork;
	int x = workArea.left + (workArea.right - workArea.left - clientWidth) / 2;
	int y = workArea.top + (workArea.bottom - workArea.top - clientHeight) / 2;
	if (x < workArea.left)
		x = workArea.left;
	if (y < workArea.top)
		y = workArea.top;


	/*
		Borderless (FormBorderStyle.None) and always on top.
		WS_EX_LAYERED keeps the window fully transparent until the fade-in starts.
	*/
	window = CreateWindowExW
	(
		WS_EX_TOPMOST | WS_EX_LAYERED,
		L"ELauncher",
		L"Eefernal Fog",
		WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX, // WS_SYSMENU and WS_MINIMIZEBOX let the taskbar button minimize and close the window.
		x,
		y,
		clientWidth,
		clientHeight,
		nullptr,
		nullptr,
		hInstance,
		nullptr
	);

	if (window == nullptr)
		return false;

	/* The window manager may have given it a different size than asked (Wine, tiling window managers). */
	RECT clientRect = {};
	if (GetClientRect(window, &clientRect) && clientRect.right > 0 && clientRect.bottom > 0)
	{
		clientWidth = clientRect.right;
		clientHeight = clientRect.bottom;
		UpdateLayout();
	}

	isLayered = true;
	SetOpacity(0.0);

	return true;
}




/* Same as Form.Opacity: a value between 0.0 (invisible) and 1.0 (fully visible). */
void Launcher::SetOpacity(double opacity)
{
	if (opacity < 0.0)
		opacity = 0.0;
	if (opacity > 1.0)
		opacity = 1.0;

	BYTE alpha = (BYTE)(opacity * 255.0);
	if (alpha < 255)
	{
		SetLayeredWindowAttributes(window, 0, alpha, LWA_ALPHA);
		return;
	}

	/* At 100% the window stops being layered, the same way WinForms handles it. */
	if (isLayered)
	{
		SetWindowLongPtrW(window, GWL_EXSTYLE, GetWindowLongPtrW(window, GWL_EXSTYLE) & ~WS_EX_LAYERED);
		RedrawWindow(window, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
		isLayered = false;
	}
}

/* Same as Form.TopMost. */
void Launcher::SetTopMost(bool topMost)
{
	SetWindowPos(window, topMost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}




/* Everything is drawn into an off-screen bitmap first, then copied to the window at once, so nothing flickers. */
void Launcher::Launcher_Paint(HDC hdc)
{
	HDC bufferDC = CreateCompatibleDC(hdc);
	HBITMAP bufferBitmap = CreateCompatibleBitmap(hdc, clientWidth, clientHeight);
	HGDIOBJ previousBitmap = SelectObject(bufferDC, bufferBitmap);

	DrawComponents(bufferDC);
	BitBlt(hdc, 0, 0, clientWidth, clientHeight, bufferDC, 0, 0, SRCCOPY);

	SelectObject(bufferDC, previousBitmap);
	DeleteObject(bufferBitmap);
	DeleteDC(bufferDC);
}

/* Draws the whole image into the bounds. */
void Launcher::DrawImage(Gdiplus::Graphics& graphics, Gdiplus::Image* image, const Gdiplus::Rect& bounds)
{
	if (image != nullptr)
		graphics.DrawImage(image, bounds, 0, 0, (INT)image->GetWidth(), (INT)image->GetHeight(), Gdiplus::UnitPixel);
}

/* Time played: white text on a rounded, semi-transparent dark background, in the bottom-right corner of its area. */
void Launcher::DrawPlaytime(Gdiplus::Graphics& graphics)
{
	if (startupPlaytimeText.empty())
		return;

	const Gdiplus::FontFamily* fontFamily = labelFontFamily != nullptr ? labelFontFamily : Gdiplus::FontFamily::GenericSansSerif();
	Gdiplus::Font font(fontFamily, 15.0f * layoutScale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

	Gdiplus::RectF textBounds;
	graphics.MeasureString(startupPlaytimeText.c_str(), -1, &font, Gdiplus::PointF(0.0f, 0.0f), Gdiplus::StringFormat::GenericTypographic(), &textBounds);


	const float paddingX = 12.0f * layoutScale;
	const float height = 28.0f * layoutScale;
	float width = std::ceil(textBounds.Width) + paddingX * 2.0f;
	Gdiplus::RectF labelBounds
	(
		(float)(startupPlaytimeBounds.X + startupPlaytimeBounds.Width) - width,
		(float)(startupPlaytimeBounds.Y + startupPlaytimeBounds.Height) - height,
		width,
		height
	);

	Gdiplus::GraphicsState stateBeforeLabel = graphics.Save();
	graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);

	Gdiplus::SolidBrush backgroundBrush(Gdiplus::Color(150, 0, 0, 0));
	Utilities::Drawing::FillRoundedRectangle(graphics, &backgroundBrush, labelBounds, 8.0f * layoutScale);

	Gdiplus::StringFormat format(Gdiplus::StringFormat::GenericTypographic());
	format.SetAlignment(Gdiplus::StringAlignmentCenter);
	format.SetLineAlignment(Gdiplus::StringAlignmentCenter);

	Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 255, 255, 255));
	graphics.DrawString(startupPlaytimeText.c_str(), -1, &font, labelBounds, &format, &textBrush);

	graphics.Restore(stateBeforeLabel);
}

void Launcher::DrawComponents(HDC hdc)
{
	Gdiplus::Graphics graphics(hdc);

	/* Window background color, visible wherever the background image doesn't cover the window. */
	Gdiplus::SolidBrush backgroundBrush(Utilities::Drawing::GetControlColor());
	graphics.FillRectangle(&backgroundBrush, 0, 0, clientWidth, clientHeight);


	/*
		Images are resized beforehand by AnimatedImage::GetScaledFrame (smooth both when shrinking and enlarging),
		so here they're only copied 1:1 and GDI+ mustn't filter them again.
	*/
	Gdiplus::GraphicsState stateBeforeImages = graphics.Save();
	graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
	graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);


	/* Background image (BackgroundImageLayout.Zoom), current frame if it's animated. */
	if (Gdiplus::Image* backgroundFrame = backgroundImage.GetFrame())
	{
		Gdiplus::Rect backgroundBounds = Utilities::Drawing::GetZoomRectangle(backgroundFrame, Gdiplus::Rect(0, 0, clientWidth, clientHeight));
		DrawImage(graphics, backgroundImage.GetScaledFrame(backgroundBounds.Width, backgroundBounds.Height), backgroundBounds);
	}


	/* startupLogo (PictureBoxSizeMode.Zoom), current frame if it's animated. */
	if (Gdiplus::Image* startupLogoFrame = startupLogoImage.GetFrame())
	{
		Gdiplus::Rect startupLogoFrameBounds = Utilities::Drawing::GetZoomRectangle(startupLogoFrame, startupLogoBounds);
		DrawImage(graphics, startupLogoImage.GetScaledFrame(startupLogoFrameBounds.Width, startupLogoFrameBounds.Height), startupLogoFrameBounds);
	}


	graphics.Restore(stateBeforeImages);


	/* startupPlaytime. */
	DrawPlaytime(graphics);


	/* startupProgressBar. */
	startupProgressBar.Draw(graphics);
}




LRESULT CALLBACK Launcher::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_ERASEBKGND:
			return 1; // Background is drawn together with everything else in WM_PAINT.

		case WM_PAINT:
		{
			PAINTSTRUCT paintStruct = {};
			HDC hdc = BeginPaint(hWnd, &paintStruct);
			Launcher_Paint(hdc);
			EndPaint(hWnd, &paintStruct);
			return 0;
		}

		case WM_SIZE:
			if (LOWORD(lParam) > 0 && HIWORD(lParam) > 0)
			{
				clientWidth = LOWORD(lParam);
				clientHeight = HIWORD(lParam);
				UpdateLayout();
				InvalidateRect(hWnd, nullptr, FALSE);
			}
			return 0;

		case WM_DPICHANGED: // Moved to a screen with different scaling, Windows suggests the new size.
		{
			const RECT* suggested = (const RECT*)lParam;
			SetWindowPos(hWnd, nullptr, suggested->left, suggested->top, suggested->right - suggested->left, suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
			return 0;
		}

		case WM_TIMER:
			if (wParam == GameStartAnimation::timerId)
			{
				if (GameStartAnimation::WindowFadeIn::complete == false)
					Launcher_AnimationTick();

				if (GameStartAnimation::ProgressBar::complete == false)
					startupProgressBar_AnimationTick();

				Images_AnimationTick();
			}
			return 0;

		case WM_LAUNCHER_SHOWN:
			Launcher_Shown();
			return 0;

		case WM_LAUNCHER_HIDE:
			KillTimer(hWnd, GameStartAnimation::timerId);
			ShowWindow(hWnd, SW_HIDE);
			return 0;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
	}


	return DefWindowProcW(hWnd, message, wParam, lParam);
}




int Launcher::Run(HINSTANCE hInstance)
{
	InitializeComponent();

	if (CreateHandle(hInstance) == false)
	{
		MessageBoxW(nullptr, L"Failed to create launcher window!", L"Eefernal Launcher", MB_OK | MB_ICONERROR);
		return 1;
	}

	Launcher_Load();


	/* Show the window, "Shown" is handled once the window is on screen. */
	ShowWindow(window, SW_SHOW);
	PostMessageW(window, WM_LAUNCHER_SHOWN, 0, 0);

	MSG message = {};
	while (GetMessageW(&message, nullptr, 0, 0) > 0)
	{
		TranslateMessage(&message);
		DispatchMessageW(&message);
	}


	return (int)message.wParam;
}

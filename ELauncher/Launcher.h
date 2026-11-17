#pragma once
#include <atomic>
#include <cmath>
#include <string>

#include "Utilities.h"
#include "AnimatedImage.h"
#include "PlaytimeTracker.h"
#include "Localization.h"
#include "GradientProgressBar.h"






class Launcher
{
private:
	static inline const std::wstring launcherDirectory = Utilities::Path::GetLauncherDirectory();

	/* Folder next to the launcher with everything it reads: images, startup settings and playtime registry path. */
	static inline const std::wstring launcherDataDirectory = Utilities::Path::Combine(launcherDirectory, L"ELauncher\\");

	class GameStartData
	{
	public:
		static inline const std::wstring splashScreenFilePath = Utilities::Path::Combine(launcherDataDirectory, L"SplashScreen.mfx");
		static inline const std::wstring logoFilePath = Utilities::Path::Combine(launcherDataDirectory, L"Logo.mfx");

		static inline const std::wstring startupTargetFilePath = Utilities::Path::Combine(launcherDataDirectory, L"StartupTarget.txt");
		static inline const std::wstring startupArgumentsFilePath = Utilities::Path::Combine(launcherDataDirectory, L"StartupArguments.txt");
		static inline const std::wstring startupDependenciesPath = Utilities::Path::Combine(launcherDataDirectory, L"StartupDependencies.txt");

		static inline const std::wstring startupDllOverridesFilePath = Utilities::Path::Combine(launcherDataDirectory, L"StartupDllOverrides.txt");
		static inline const std::wstring defaultDllOverrides = L"dsound=n,b"; // ASI Loader's dsound.dll from the game folder first, Wine's own as fallback.

		static inline const std::wstring registryPathFilePath = Utilities::Path::Combine(launcherDataDirectory, L"RegistryPath.txt");
	};

	/* The started game, kept for playtime tracking. */
	class GameProcess
	{
	public:
		static inline HANDLE job = nullptr;     // Job object holding the game and everything it starts.
		static inline HANDLE process = nullptr; // The process the launcher started.
		static inline ULONGLONG startTime = 0;  // GetTickCount64 at the moment it was started.
	};

	class GameStartAnimation
	{
	public:
		static constexpr UINT_PTR timerId = 1;
		static constexpr UINT timerInterval = 1; // Windows rounds it up to its timer resolution (~15.6 ms), same as the WinForms timer.

		class WindowFadeIn
		{
		public:
			static inline const double stepping = 0.020;
			static inline const int totalFrames = (int)std::ceil(1.0 / stepping); // HARDCODED VALUE! 1.0 represents maximum window opacity value (100%).
			static inline int currentFrame = 0;
			static inline std::atomic<bool> complete = false;
		};

		class ProgressBar
		{
		public:
			static inline const double stepping = 10.0;
			static inline const int totalFrames = (int)std::ceil(1000.0 / stepping); // HARDCODED VALUE! 1000.0 represents progress bar maximum value.
			static inline int currentFrame = 0;
			static inline std::atomic<bool> complete = false;
		};
	};


	/* Window and its components, set up in Launcher.Designer.cpp. */
	static constexpr UINT WM_LAUNCHER_SHOWN = WM_APP + 1; // Posted once the window is on screen, acts as the "Shown" event.
	static constexpr UINT WM_LAUNCHER_HIDE = WM_APP + 2;  // Posted by the termination worker when the launcher stays in the background for playtime tracking.

	/* Layout is designed for 800x450, everything is scaled from it to the real window size. */
	static constexpr int designWidth = 800;
	static constexpr int designHeight = 450;
	static inline const Gdiplus::Rect startupLogoDesignBounds = Gdiplus::Rect(12, 363, 200, 65);
	static inline const Gdiplus::Rect startupPlaytimeDesignBounds = Gdiplus::Rect(588, 363, 200, 65);
	static inline const Gdiplus::Rect startupProgressBarDesignBounds = Gdiplus::Rect(0, 440, 800, 10);

	static inline HWND window = nullptr;
	static inline int clientWidth = 0;
	static inline int clientHeight = 0;
	static inline float layoutScale = 1.0f; // Real size / design size, for things that keep their proportions (text).
	static inline bool isLayered = false;

	static inline AnimatedImage backgroundImage;
	static inline AnimatedImage startupLogoImage;
	static inline Gdiplus::Rect startupLogoBounds;

	static inline std::wstring startupPlaytimeText;                    // Empty = not shown.
	static inline Gdiplus::Rect startupPlaytimeBounds;                 // The label sits in the bottom-right corner of this area.
	static inline std::vector<BYTE> labelFontData;                     // GDI+ reads the font from here for as long as it's used.
	static inline Gdiplus::PrivateFontCollection* labelFonts = nullptr;
	static inline Gdiplus::FontFamily* labelFontFamily = nullptr;
	static inline GradientProgressBar startupProgressBar;


	/* Implemented in Launcher.Designer.cpp. */
	static void InitializeComponent();
	static bool CreateHandle(HINSTANCE hInstance);

	static UINT GetDpi(HMONITOR monitor);
	static Gdiplus::Rect ScaleDesignRect(const Gdiplus::Rect& designBounds);
	static void UpdateLayout();

	static void SetOpacity(double opacity);
	static void SetTopMost(bool topMost);

	static void Launcher_Paint(HDC hdc);
	static void DrawImage(Gdiplus::Graphics& graphics, Gdiplus::Image* image, const Gdiplus::Rect& bounds);
	static void DrawPlaytime(Gdiplus::Graphics& graphics);
	static void DrawComponents(HDC hdc);

	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);


	/* Implemented in Launcher.cpp. */
	static void Exit();
	static void ExitWithMessage(const std::wstring& message, UINT messageType);

	static void ApplyWineDllOverrides();
	static void StartProcess(const std::wstring& startupTarget, const std::wstring& startupArguments);

	static void Launcher_AnimationTick();
	static void startupProgressBar_AnimationTick();
	static void Images_AnimationTick();
	static void Launcher_StartAnimation();

	static void Launcher_Load();
	static void Launcher_Shown();

	static DWORD WINAPI launcherTerminationWorker_DoWork(LPVOID lpParam);


public:
	static int Run(HINSTANCE hInstance);
};

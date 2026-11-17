#include "Launcher.h"






void Launcher::Exit()
{
	ExitProcess(0);
}

void Launcher::ExitWithMessage(const std::wstring& message, UINT messageType)
{
	if (message.empty() == false)
	{
		KillTimer(window, GameStartAnimation::timerId);
		MessageBoxW(window, message.c_str(), L"ELauncher", MB_OK | messageType);
	}

	Exit();
}




/*
	On Linux the game runs through Wine / Proton, which prefers its own built-in DLLs, so ASI Loader's dsound.dll would be ignored.
	The game inherits our environment, so this has the same effect as WINEDLLOVERRIDES="dsound=n,b" %command% in Steam.
	Overrides already set (e.g. from Steam launch options) are kept and come last, so they take priority.
*/
void Launcher::ApplyWineDllOverrides()
{
	if (Utilities::Wine::IsRunning() == false)
		return;

	std::wstring dllOverrides = GameStartData::defaultDllOverrides;
	if (Utilities::File::Exists(GameStartData::startupDllOverridesFilePath))
		dllOverrides = Utilities::String::TrimLineBreaks(Utilities::File::ReadAllText(GameStartData::startupDllOverridesFilePath));

	if (dllOverrides.empty())
		return;

	std::wstring existingDllOverrides = Utilities::Environment::GetVariable(L"WINEDLLOVERRIDES");
	if (existingDllOverrides.empty() == false)
		dllOverrides += L";" + existingDllOverrides;

	Utilities::Environment::SetVariable(L"WINEDLLOVERRIDES", dllOverrides);
}

void Launcher::StartProcess(const std::wstring& startupTarget, const std::wstring& startupArguments)
{
	if (Utilities::File::Exists(startupTarget) == false)
		ExitWithMessage(L"Failed to start process! Startup target doesn't exist!", MB_ICONERROR);


	/*
		Modern applications often rely on relative paths (paths calculated from {workingDirectory} + "..\..").
		In order to achive best compatibility and avoid potential issues, we need to resolve working directory
		from path to target executable, this path will later be specified when creating a process.
	*/
	std::wstring workingDirectory = Utilities::Path::GetDirectoryName(startupTarget);
	if (Utilities::Directory::Exists(workingDirectory) == false)
	{
		wchar_t currentDirectory[MAX_PATH] = {};
		GetCurrentDirectoryW(MAX_PATH, currentDirectory);
		workingDirectory = currentDirectory;
	}


	/*
		When constructing a command line, we need to account for scenario where startup arguments wasn't specified.
		Interacting with low-level libraries always involve proper understanding and accuracy of actions taken, no extra spaces must be left.
	*/
	std::wstring commandLine = startupArguments.empty()
															? L"\"" + startupTarget + L"\""
															: L"\"" + startupTarget + L"\" " + startupArguments;


	STARTUPINFOW startupInfo = {};
	startupInfo.cb = sizeof(STARTUPINFOW);
	startupInfo.dwFlags = STARTF_FORCEONFEEDBACK; // Indicates that the cursor is in feedback mode for two seconds after CreateProcess is called.

	PROCESS_INFORMATION processInfo = {};


	/*
		With playtime tracking, the game is put into a job object, so everything it starts can be followed.
		It's started suspended, so it can't start anything before it's in the job.
	*/
	DWORD creationFlags = 0;
	if (PlaytimeTracker::IsEnabled())
	{
		GameProcess::job = CreateJobObjectW(nullptr, nullptr);
		if (GameProcess::job != nullptr)
			creationFlags = CREATE_SUSPENDED;
	}


	BOOL createProcessResult = CreateProcessW
	(
		nullptr,
		commandLine.data(), // CreateProcessW can modify the contents of this string.
		nullptr,
		nullptr,
		FALSE,
		creationFlags,
		nullptr,
		workingDirectory.c_str(),
		&startupInfo,
		&processInfo
	);

	if (createProcessResult == FALSE)
		ExitWithMessage(L"Failed to start process! WIN32 ERROR: " + std::to_wstring(GetLastError()) + L".", MB_ICONERROR);


	if (GameProcess::job != nullptr)
	{
		if (AssignProcessToJobObject(GameProcess::job, processInfo.hProcess) == FALSE)
		{
			CloseHandle(GameProcess::job); // Only the started process itself will be followed.
			GameProcess::job = nullptr;
		}

		ResumeThread(processInfo.hThread);
	}

	GameProcess::startTime = GetTickCount64();
	CloseHandle(processInfo.hThread);

	if (PlaytimeTracker::IsEnabled())
		GameProcess::process = processInfo.hProcess;
	else
		CloseHandle(processInfo.hProcess);
}




void Launcher::Launcher_AnimationTick()
{
	GameStartAnimation::WindowFadeIn::currentFrame++;
	if (GameStartAnimation::WindowFadeIn::currentFrame > GameStartAnimation::WindowFadeIn::totalFrames)
	{
		SetOpacity(1.0);
		GameStartAnimation::WindowFadeIn::complete = true;
	}
	else
	{
		/* Calculate normalized progress of the animation as a value between 0.0 (start) and 1.0 (end). */
		double normalizedProgress = (double)GameStartAnimation::WindowFadeIn::currentFrame / (double)GameStartAnimation::WindowFadeIn::totalFrames;

		/* Quadratic ease-in - slow at start, accelerating over time. */
		SetOpacity(normalizedProgress * normalizedProgress);
	}
}

void Launcher::startupProgressBar_AnimationTick()
{
	GameStartAnimation::ProgressBar::currentFrame++;
	if (GameStartAnimation::ProgressBar::currentFrame > GameStartAnimation::ProgressBar::totalFrames)
	{
		startupProgressBar.value = startupProgressBar.maximum;
		GameStartAnimation::ProgressBar::complete = true;
	}
	else
	{
		/* Calculate normalized progress of the animation as a value between 0.0 (start) and 1.0 (end). */
		double normalizedProgress = (double)GameStartAnimation::ProgressBar::currentFrame / (double)GameStartAnimation::ProgressBar::totalFrames;

		/* Quadratic ease-in - slow at start, accelerating over time. */
		double quadraticEase = normalizedProgress * normalizedProgress;

		/* Scale quadratic ease-in curve to the progress bar maximum value. */
		startupProgressBar.value = (int)std::nearbyint(quadraticEase * startupProgressBar.maximum);
	}


	/* Redraw only the progress bar. */
	RECT progressBarArea =
	{
		startupProgressBar.bounds.X,
		startupProgressBar.bounds.Y,
		startupProgressBar.bounds.X + startupProgressBar.bounds.Width,
		startupProgressBar.bounds.Y + startupProgressBar.bounds.Height
	};
	InvalidateRect(window, &progressBarArea, FALSE);
}

/* Plays animated splash screen and logo (GIF / APNG), redrawing only what changed. */
void Launcher::Images_AnimationTick()
{
	ULONGLONG currentTime = GetTickCount64();
	bool hasBackgroundChanged = backgroundImage.Update(currentTime);
	bool hasStartupLogoChanged = startupLogoImage.Update(currentTime);

	if (hasBackgroundChanged)
		InvalidateRect(window, nullptr, FALSE);
	else if (hasStartupLogoChanged)
	{
		RECT startupLogoArea =
		{
			startupLogoBounds.X,
			startupLogoBounds.Y,
			startupLogoBounds.X + startupLogoBounds.Width,
			startupLogoBounds.Y + startupLogoBounds.Height
		};
		InvalidateRect(window, &startupLogoArea, FALSE);
	}
}

void Launcher::Launcher_StartAnimation()
{
	/* Fade-in, progress bar and animated images all run from the same timer, see WM_TIMER in Launcher.Designer.cpp. */
	SetTimer(window, GameStartAnimation::timerId, GameStartAnimation::timerInterval, nullptr);
}




void Launcher::Launcher_Load()
{
	SetTopMost(true);


	/* Custom splash screen and logo (still or animated), the embedded images are kept if a file is missing or can't be loaded. */
	if (Utilities::File::Exists(GameStartData::splashScreenFilePath))
		backgroundImage.LoadFromFile(GameStartData::splashScreenFilePath);

	if (Utilities::File::Exists(GameStartData::logoFilePath))
		startupLogoImage.LoadFromFile(GameStartData::logoFilePath);


	/* Playtime tracking is on when ELauncher\RegistryPath.txt exists, it holds the registry path under HKEY_CURRENT_USER. */
	if (Utilities::File::Exists(GameStartData::registryPathFilePath))
		PlaytimeTracker::SetRegistryPath(Utilities::String::TrimLineBreaks(Utilities::File::ReadAllText(GameStartData::registryPathFilePath)));

	/* Time played so far, shown in the bottom-right corner if it could be read. */
	uint32_t playtimeInSeconds = 0;
	if (PlaytimeTracker::IsEnabled() && PlaytimeTracker::TryGetPlaytime(playtimeInSeconds))
		startupPlaytimeText = Localization::GetPlaytimeText(playtimeInSeconds);


	if (HANDLE hThread = CreateThread(nullptr, 0, launcherTerminationWorker_DoWork, nullptr, 0, nullptr))
		CloseHandle(hThread);
}

void Launcher::Launcher_Shown()
{
	/* The moment launcher was drawn on screen, start animation sequence. */
	Launcher_StartAnimation();


	/*
		In order to make launcher as flexible and as compatible with any game given...
		instead of hardcoding path to game executable, we're reading it from within a text file.

		Thefore attempting to start a process, it's important to verify that file exists in first place and that it isn't empty.
	*/
	if (Utilities::File::Exists(GameStartData::startupTargetFilePath) == false)
		ExitWithMessage(L"Failed to initialize! File with startup target is missing!\n\n\"" + GameStartData::startupTargetFilePath + L"\"", MB_ICONERROR);

	std::wstring startupTarget = Utilities::String::TrimLineBreaks(Utilities::File::ReadAllText(GameStartData::startupTargetFilePath));
	if (startupTarget.empty())
		ExitWithMessage(L"Failed to initialize! File with startup target is empty!\n\n\"" + GameStartData::startupTargetFilePath + L"\"", MB_ICONERROR);

	if (Utilities::File::Exists(startupTarget) == false)
	{
		std::wstring startupTargetFullPath = Utilities::Path::Combine(launcherDirectory, startupTarget);
		if (Utilities::File::Exists(startupTargetFullPath))
			startupTarget = startupTargetFullPath;
		else
			ExitWithMessage(L"Failed to initialize! Couldn't locate startup target!\n\n\"" + startupTarget + L"\".\n\nEnsure that game files are in place, they may have been downloaded or unpacked incorrectly.", MB_ICONERROR);
	}


	/*
		In order to make launcher as flexible and as compatible with any game given...
		instead of hardcoding command line arguments, we're reading them from within a text file.
	*/
	std::wstring startupArguments;
	if (Utilities::File::Exists(GameStartData::startupArgumentsFilePath))
		startupArguments = Utilities::String::TrimLineBreaks(Utilities::File::ReadAllText(GameStartData::startupArgumentsFilePath));


	/*
		In order to make launcher as flexible and as compatible with any game given...
		launcher can account for dependencies game wouldn't function without of.

		Dependencies in file must be listed in different lines, one dependancy per line.
	*/
	if (Utilities::File::Exists(GameStartData::startupDependenciesPath))
	{
		std::vector<std::wstring> startupDependencies = Utilities::File::ReadAllLines(GameStartData::startupDependenciesPath);
		for (const std::wstring& dependency : startupDependencies)
		{
			if (Utilities::File::Exists(dependency) == false)
			{
				std::wstring dependencyFullPath = Utilities::Path::Combine(launcherDirectory, dependency);
				if (Utilities::File::Exists(dependencyFullPath) == false)
					ExitWithMessage(L"Failed to initialize! Couldn't locate required dependency!\n\n\"" + dependency + L"\".\n\nEnsure that dependency is in place, it may have been removed by your local anti-virus.", MB_ICONERROR);
			}
		}
	}


	/* Starting a new process may lead to an security pop up to appear, we want to ensure user will be able to see & interact with it. */
	SetTopMost(false);


	/* Under Wine / Proton, let the game load ASI Loader (see ApplyWineDllOverrides). */
	ApplyWineDllOverrides();


	/* Attempt starting a new process using data we've obtained. */
	StartProcess(startupTarget, startupArguments);
}




/*
	Closes the launcher 1.5 seconds after both animations have finished.
	With playtime tracking, the launcher hides instead and keeps counting in the background until the game has closed.
*/
DWORD WINAPI Launcher::launcherTerminationWorker_DoWork(LPVOID lpParam)
{
	while (GameStartAnimation::WindowFadeIn::complete == false || GameStartAnimation::ProgressBar::complete == false)
	{
		Sleep(10);
	}

	Sleep(1500);

	if (PlaytimeTracker::IsEnabled() && GameProcess::process != nullptr)
	{
		PostMessageW(window, WM_LAUNCHER_HIDE, 0, 0);
		PlaytimeTracker::TrackUntilGameCloses(GameProcess::job, GameProcess::process, GameProcess::startTime, launcherDirectory);
	}

	Exit();


	return 0;
}

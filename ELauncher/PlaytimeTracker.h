#pragma once
#include <cstdint>
#include <string>

#include "Utilities.h"






/*
	Counts how long the game runs, the same way DBDPlaytimeTracker.asi does (HKEY_CURRENT_USER\{path}, "Time Played", seconds),
	but from the launcher, so no ASI is needed and it works for every game started through it.

	Turned on per game by ELauncher\RegistryPath.txt, which holds the registry path, e.g. SOFTWARE\DBDPlaytimeTracker.
*/
class PlaytimeTracker
{
private:
	static inline const std::wstring playtimeRegistryKeyName = L"Time Played";

	static inline const ULONGLONG updateDelayInMilliseconds = 10000; // Playtime is saved this often, so a crash loses at most this much.
	static inline const DWORD checkDelayInMilliseconds = 1000;       // How often the launcher checks if the game is still running.
	static inline const ULONGLONG maxCheckGapInMilliseconds = 10000; // A longer gap between checks means the PC was asleep, that time isn't counted.

	static inline std::wstring playtimeRegistryPath;

	static void AddPlaytime(uint32_t seconds);
	static bool IsGameRunning(HANDLE gameJob, HANDLE gameProcess, const std::wstring& gameDirectory);


public:
	static void SetRegistryPath(const std::wstring& registryPath);
	static bool IsEnabled();
	static bool TryGetPlaytime(uint32_t& playtimeInSeconds);

	static void TrackUntilGameCloses(HANDLE gameJob, HANDLE gameProcess, ULONGLONG gameStartTime, const std::wstring& gameDirectory);
};

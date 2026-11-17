#include "PlaytimeTracker.h"






void PlaytimeTracker::SetRegistryPath(const std::wstring& registryPath)
{
	playtimeRegistryPath = registryPath;
}

bool PlaytimeTracker::IsEnabled()
{
	return playtimeRegistryPath.empty() == false;
}

/* Playtime saved so far, returns false if there's none yet (or tracking is off). */
bool PlaytimeTracker::TryGetPlaytime(uint32_t& playtimeInSeconds)
{
	return Utilities::Registry::TryGetUint32(playtimeRegistryPath, playtimeRegistryKeyName, playtimeInSeconds);
}




/* Reads the current value right before adding to it, so nothing else writing the same value gets overwritten. */
void PlaytimeTracker::AddPlaytime(uint32_t seconds)
{
	uint32_t playtimeInSeconds = Utilities::Registry::GetUint32(playtimeRegistryPath, playtimeRegistryKeyName, 0);

	if (playtimeInSeconds > UINT32_MAX - seconds)
		playtimeInSeconds = UINT32_MAX;
	else
		playtimeInSeconds += seconds;

	Utilities::Registry::SetUint32(playtimeRegistryPath, playtimeRegistryKeyName, playtimeInSeconds);
}

/*
	The game counts as running while any process in its job object was started from the game's folder.
	That follows launchers handing over to the real game (DeadByDaylight.exe -> DeadByDaylight-Win64-Shipping.exe),
	but doesn't count programs the game happens to start from elsewhere, such as Steam.

	Without a job object (couldn't be created), only the started process itself is followed.
*/
bool PlaytimeTracker::IsGameRunning(HANDLE gameJob, HANDLE gameProcess, const std::wstring& gameDirectory)
{
	if (gameJob != nullptr)
	{
		/* Room for 64 process ids, a game doesn't start more than a handful. */
		struct
		{
			JOBOBJECT_BASIC_PROCESS_ID_LIST list;
			ULONG_PTR moreProcessIds[63];
		} processes = {};
		processes.list.NumberOfAssignedProcesses = 64;

		BOOL queryResult = QueryInformationJobObject(gameJob, JobObjectBasicProcessIdList, &processes, sizeof(processes), nullptr);
		if (queryResult || GetLastError() == ERROR_MORE_DATA)
		{
			for (DWORD i = 0; i < processes.list.NumberOfProcessIdsInList; i++)
			{
				HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)processes.list.ProcessIdList[i]);
				if (process == nullptr)
					continue;

				wchar_t imagePath[MAX_PATH * 2] = {};
				DWORD imagePathLength = MAX_PATH * 2;
				BOOL hasImagePath = QueryFullProcessImageNameW(process, 0, imagePath, &imagePathLength);
				CloseHandle(process);

				/* Case-insensitive, as Windows paths are. */
				if (hasImagePath && imagePathLength >= gameDirectory.size() && CompareStringOrdinal(imagePath, (int)gameDirectory.size(), gameDirectory.c_str(), (int)gameDirectory.size(), TRUE) == CSTR_EQUAL)
					return true;
			}

			return false;
		}
	}


	return WaitForSingleObject(gameProcess, 0) == WAIT_TIMEOUT;
}




/*
	Runs on the launcher's background thread until the game has closed.
	Playtime counts from the moment the game was started, and is saved every 10 seconds and once more when the game closes.

	Time is added check by check rather than as "now - start", so a PC that was asleep with the game open doesn't count the sleep
	(on Windows the tick count keeps running during sleep, and on a Steam Deck suspend is common).
*/
void PlaytimeTracker::TrackUntilGameCloses(HANDLE gameJob, HANDLE gameProcess, ULONGLONG gameStartTime, const std::wstring& gameDirectory)
{
	ULONGLONG lastCheckTime = gameStartTime;
	ULONGLONG uncountedMilliseconds = 0;
	bool isGameRunning = true;

	while (isGameRunning)
	{
		Sleep(checkDelayInMilliseconds);
		isGameRunning = IsGameRunning(gameJob, gameProcess, gameDirectory);

		ULONGLONG currentTime = GetTickCount64();
		ULONGLONG elapsedMilliseconds = currentTime - lastCheckTime;
		lastCheckTime = currentTime;

		if (elapsedMilliseconds > maxCheckGapInMilliseconds)
			elapsedMilliseconds = checkDelayInMilliseconds; // The PC was asleep.

		uncountedMilliseconds += elapsedMilliseconds;
		if (uncountedMilliseconds >= updateDelayInMilliseconds || isGameRunning == false)
		{
			uint32_t seconds = (uint32_t)(uncountedMilliseconds / 1000);
			if (seconds > 0)
			{
				AddPlaytime(seconds);
				uncountedMilliseconds -= (ULONGLONG)seconds * 1000; // Leftover milliseconds are carried over to the next save.
			}
		}
	}
}

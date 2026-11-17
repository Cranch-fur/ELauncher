#pragma once
#include <cstdint>
#include <string>

#include "Utilities.h"






/*
	Texts shown on the splash screen, in the system's display language (English if it isn't supported).
	To add a language, add a line to playtimeTexts in Localization.cpp.
*/
class Localization
{
private:
	/* How a language picks the word form for a number. */
	enum class PluralRule
	{
		None,     // Same form for every number (Chinese, Japanese, Korean, Turkish).
		OneOnly,  // "one" for exactly 1 (English, German, Spanish, Italian).
		BelowTwo, // "one" for anything below 2, 1,5 included (French, Portuguese).
		Slavic,   // Whole numbers: 1, 21, 31... "one", 2-4, 22-24... "few", the rest "many" (Russian, Ukrainian).
		Polish    // Whole numbers: 1 "one", 2-4, 22-24... "few", the rest "many" (21 is "many").
	};

	/* {0} is replaced with the number. Forms left as nullptr use "other". */
	struct WordForms
	{
		const wchar_t* other; // Decimals (127,3), and every number in languages without plurals.
		const wchar_t* one = nullptr;
		const wchar_t* few = nullptr;
		const wchar_t* many = nullptr;
	};

	struct PlaytimeTexts
	{
		WORD language;    // PRIMARYLANGID, e.g. LANG_RUSSIAN.
		WORD subLanguage; // SUBLANGID, 0 = any.
		PluralRule rule;
		WordForms minutes;
		WordForms hours;
	};

	static const PlaytimeTexts playtimeTexts[];

	static const PlaytimeTexts& GetPlaytimeTexts();
	static const wchar_t* GetWordForm(PluralRule rule, const WordForms& forms, uint32_t tenths);


public:
	static std::wstring GetPlaytimeText(uint32_t playtimeInSeconds);
};

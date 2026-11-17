#include "Localization.h"






/*
	Time played texts, one line per language. English is the first line and the fallback.
	Forms: { other (decimals), one, few, many }.
*/
const Localization::PlaytimeTexts Localization::playtimeTexts[] =
{
	{
		LANG_ENGLISH, 0, PluralRule::OneOnly,
		{ L"{0} minutes played", L"{0} minute played" },
		{ L"{0} hours played", L"{0} hour played" }
	},
	{
		LANG_RUSSIAN, 0, PluralRule::Slavic,
		{ L"В игре: {0} минуты", L"В игре: {0} минута", L"В игре: {0} минуты", L"В игре: {0} минут" },
		{ L"В игре: {0} часа", L"В игре: {0} час", L"В игре: {0} часа", L"В игре: {0} часов" }
	},
	{
		LANG_UKRAINIAN, 0, PluralRule::Slavic,
		{ L"У грі: {0} хвилини", L"У грі: {0} хвилина", L"У грі: {0} хвилини", L"У грі: {0} хвилин" },
		{ L"У грі: {0} години", L"У грі: {0} година", L"У грі: {0} години", L"У грі: {0} годин" }
	},
	{
		LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL, PluralRule::None,
		{ L"已遊玩 {0} 分鐘" },
		{ L"已遊玩 {0} 小時" }
	},
	{
		LANG_CHINESE, SUBLANG_CHINESE_HONGKONG, PluralRule::None,
		{ L"已遊玩 {0} 分鐘" },
		{ L"已遊玩 {0} 小時" }
	},
	{
		LANG_CHINESE, SUBLANG_CHINESE_MACAU, PluralRule::None,
		{ L"已遊玩 {0} 分鐘" },
		{ L"已遊玩 {0} 小時" }
	},
	{
		LANG_CHINESE, 0, PluralRule::None,
		{ L"已游玩 {0} 分钟" },
		{ L"已游玩 {0} 小时" }
	},
	{
		LANG_JAPANESE, 0, PluralRule::None,
		{ L"プレイ時間 {0} 分" },
		{ L"プレイ時間 {0} 時間" }
	},
	{
		LANG_KOREAN, 0, PluralRule::None,
		{ L"플레이 시간 {0}분" },
		{ L"플레이 시간 {0}시간" }
	},
	{
		LANG_GERMAN, 0, PluralRule::OneOnly,
		{ L"{0} Minuten gespielt", L"{0} Minute gespielt" },
		{ L"{0} Stunden gespielt", L"{0} Stunde gespielt" }
	},
	{
		LANG_FRENCH, 0, PluralRule::BelowTwo,
		{ L"{0} minutes jouées", L"{0} minute jouée" },
		{ L"{0} heures jouées", L"{0} heure jouée" }
	},
	{
		LANG_SPANISH, 0, PluralRule::OneOnly,
		{ L"{0} minutos jugados", L"{0} minuto jugado" },
		{ L"{0} horas jugadas", L"{0} hora jugada" }
	},
	{
		LANG_PORTUGUESE, 0, PluralRule::BelowTwo,
		{ L"{0} minutos jogados", L"{0} minuto jogado" },
		{ L"{0} horas jogadas", L"{0} hora jogada" }
	},
	{
		LANG_POLISH, 0, PluralRule::Polish,
		{ L"Czas gry: {0} minuty", L"Czas gry: {0} minuta", L"Czas gry: {0} minuty", L"Czas gry: {0} minut" },
		{ L"Czas gry: {0} godziny", L"Czas gry: {0} godzina", L"Czas gry: {0} godziny", L"Czas gry: {0} godzin" }
	},
	{
		LANG_TURKISH, 0, PluralRule::None,
		{ L"{0} dakika oynandı" },
		{ L"{0} saat oynandı" }
	},
	{
		LANG_ITALIAN, 0, PluralRule::OneOnly,
		{ L"{0} minuti giocati", L"{0} minuto giocato" },
		{ L"{0} ore giocate", L"{0} ora giocata" }
	},
};




/* Texts for the display language: an exact match first (e.g. Traditional Chinese), then any variant of the language, then English. */
const Localization::PlaytimeTexts& Localization::GetPlaytimeTexts()
{
	LANGID displayLanguage = GetUserDefaultUILanguage();
	WORD language = PRIMARYLANGID(displayLanguage);
	WORD subLanguage = SUBLANGID(displayLanguage);

	for (const PlaytimeTexts& texts : playtimeTexts)
	{
		if (texts.language == language && texts.subLanguage == subLanguage)
			return texts;
	}

	for (const PlaytimeTexts& texts : playtimeTexts)
	{
		if (texts.language == language && texts.subLanguage == 0)
			return texts;
	}


	return playtimeTexts[0];
}

/* Picks the word form for a number given in tenths (1273 = 127,3). */
const wchar_t* Localization::GetWordForm(PluralRule rule, const WordForms& forms, uint32_t tenths)
{
	bool isWhole = tenths % 10 == 0;
	uint32_t number = tenths / 10;
	uint32_t lastDigit = number % 10;
	uint32_t lastTwoDigits = number % 100;
	bool isFew = lastDigit >= 2 && lastDigit <= 4 && (lastTwoDigits < 12 || lastTwoDigits > 14);

	const wchar_t* form = nullptr;
	switch (rule)
	{
		case PluralRule::OneOnly:
			if (isWhole && number == 1)
				form = forms.one;
			break;

		case PluralRule::BelowTwo:
			if (tenths < 20)
				form = forms.one;
			break;

		case PluralRule::Slavic:
			if (isWhole)
				form = (lastDigit == 1 && lastTwoDigits != 11) ? forms.one : (isFew ? forms.few : forms.many);
			break;

		case PluralRule::Polish:
			if (isWhole)
				form = number == 1 ? forms.one : (isFew ? forms.few : forms.many);
			break;

		case PluralRule::None:
			break;
	}


	return form != nullptr ? form : forms.other;
}

/*
	"42,2 minutes played" below an hour, "127,3 hours played" from an hour on.
	Always rounded down to one decimal (so 59 min 57 s is 59,9 minutes, never 60), whole numbers are shown without it ("35 hours").
	The decimal separator comes from the user's regional settings.
*/
std::wstring Localization::GetPlaytimeText(uint32_t playtimeInSeconds)
{
	bool isInHours = playtimeInSeconds >= 3600;
	uint32_t tenths = isInHours ? playtimeInSeconds / 360 : playtimeInSeconds / 6;

	std::wstring number = std::to_wstring(tenths / 10);
	if (tenths % 10 != 0)
	{
		wchar_t decimalSeparator[8] = L".";
		if (GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SDECIMAL, decimalSeparator, 8) == 0)
			wcscpy_s(decimalSeparator, L".");

		number += decimalSeparator + std::to_wstring(tenths % 10);
	}

	const PlaytimeTexts& texts = GetPlaytimeTexts();
	std::wstring text = GetWordForm(texts.rule, isInHours ? texts.hours : texts.minutes, tenths);

	size_t placeholder = text.find(L"{0}");
	if (placeholder != std::wstring::npos)
		text.replace(placeholder, 3, number);

	return text;
}

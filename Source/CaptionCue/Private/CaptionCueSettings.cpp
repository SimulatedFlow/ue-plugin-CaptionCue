// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "CaptionCueSettings.h"

UCaptionCueSettings::UCaptionCueSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("CaptionCue");
}

FName UCaptionCueSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

FName UCaptionCueSettings::GetSectionName() const
{
	return TEXT("CaptionCue");
}

const UCaptionCueSettings& UCaptionCueSettings::Get()
{
	const UCaptionCueSettings* Settings = GetDefault<UCaptionCueSettings>();
	check(Settings);
	return *Settings;
}

UCaptionCueSettings* UCaptionCueSettings::GetMutable()
{
	UCaptionCueSettings* Settings = GetMutableDefault<UCaptionCueSettings>();
	check(Settings);
	return Settings;
}

// -------------------------------------------------------------------------------------------------------
// Runtime setters
//
// They write straight onto the settings object, which is also what the draw path reads. That is deliberate:
// one value, one owner, and a menu slider that moves the captions while the player is still holding it.
// -------------------------------------------------------------------------------------------------------

void UCaptionCueSettings::SetCaptionsEnabled(bool bEnabled)
{
	bCaptionsEnabled = bEnabled;
}

void UCaptionCueSettings::SetTextScale(float InTextScale)
{
	TextScale = FMath::Clamp(InTextScale, 0.25f, 4.0f);
}

void UCaptionCueSettings::SetBackgroundBox(bool bEnabled)
{
	bBackgroundBox = bEnabled;
}

void UCaptionCueSettings::SetBackgroundOpacity(float InOpacity)
{
	BackgroundOpacity = FMath::Clamp(InOpacity, 0.0f, 1.0f);
}

void UCaptionCueSettings::SetShowSpeakerNames(bool bEnabled)
{
	bShowSpeakerNames = bEnabled;
}

void UCaptionCueSettings::SetShowSoundCaptions(bool bEnabled)
{
	bShowSoundCaptions = bEnabled;
}

void UCaptionCueSettings::SetMaxLineWidthPercent(float InPercent)
{
	MaxLineWidthPercent = FMath::Clamp(InPercent, 0.2f, 1.0f);
}

void UCaptionCueSettings::SetSafeAreaMarginPercent(float InPercent)
{
	SafeAreaMarginPercent = FMath::Clamp(InPercent, 0.0f, 0.25f);
}

void UCaptionCueSettings::SetCaptionPosition(ECaptionCuePosition InPosition)
{
	Position = InPosition;
}

void UCaptionCueSettings::SetMaxVisibleLines(int32 InMaxVisibleLines)
{
	MaxVisibleLines = FMath::Clamp(InMaxVisibleLines, 1, 8);
}

void UCaptionCueSettings::SetMinSecondsPerCharacter(float InSecondsPerCharacter)
{
	MinSecondsPerCharacter = FMath::Clamp(InSecondsPerCharacter, 0.0f, 0.5f);
}

void UCaptionCueSettings::SetMinDisplaySeconds(float InSeconds)
{
	MinDisplaySeconds = FMath::Clamp(InSeconds, 0.0f, 10.0f);
}

void UCaptionCueSettings::SetDirectionIndicators(bool bEnabled)
{
	bDirectionIndicators = bEnabled;
}

void UCaptionCueSettings::SaveCaptionOptions()
{
#if WITH_EDITOR
	TryUpdateDefaultConfigFile();
#else
	SaveConfig();
#endif
}

// -------------------------------------------------------------------------------------------------------
// Derived helpers
// -------------------------------------------------------------------------------------------------------

bool UCaptionCueSettings::FindSpeaker(FName SpeakerId, FCaptionCueSpeaker& OutSpeaker) const
{
	if (SpeakerId.IsNone())
	{
		return false;
	}

	for (const FCaptionCueSpeaker& Speaker : Speakers)
	{
		if (Speaker.SpeakerId == SpeakerId)
		{
			OutSpeaker = Speaker;
			return true;
		}
	}

	return false;
}

float UCaptionCueSettings::ResolveDisplaySeconds(float RequestedDuration, int32 CharacterCount) const
{
	// The reading time is a floor, not a replacement. A ten-second line of voice acting stays up for ten
	// seconds; a half-second grunt subtitled with a full sentence stays up long enough to read the sentence.
	const float ReadingTime = FMath::Max(0.0f, CharacterCount) * FMath::Max(0.0f, MinSecondsPerCharacter);

	float Seconds = FMath::Max3(FMath::Max(RequestedDuration, 0.0f), ReadingTime, MinDisplaySeconds);
	Seconds = FMath::Min(Seconds, FMath::Max(MaxDisplaySeconds, 0.1f));

	return FMath::Max(Seconds, 0.1f);
}

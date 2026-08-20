// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "CaptionCueStyle.h"

#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "CaptionCue"

UCaptionCueStyle::UCaptionCueStyle()
{
	// Square brackets around non-speech captions are the one piece of captioning grammar players already
	// know from television, so they are the default rather than something an author has to remember.
	Prefix = TEXT("[");
	Suffix = TEXT("]");
}

FPrimaryAssetId UCaptionCueStyle::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("CaptionCueStyle"), GetFName());
}

FSlateFontInfo UCaptionCueStyle::GetFontInfo(float SizeScale, const FLinearColor& InOutlineColor, const UObject* FallbackFont) const
{
	const float FinalSize = FMath::Max(FontSize * SizeScale, 1.0f);

	FSlateFontInfo Result;

	if (Font.HasValidFont())
	{
		Result = Font;
		Result.Size = FinalSize;
	}
	else if (FallbackFont)
	{
		// The engine's own subtitle font, which every install has and nobody has to license. Also the
		// setting a project may already have pointed somewhere else — if a game has chosen a subtitle font,
		// CaptionCue should use it rather than argue.
		Result = FSlateFontInfo(FallbackFont, FinalSize);
	}
	else
	{
		// Last resort. Kept because "no font at all" must not mean "crash", but note that this path draws
		// nothing on some canvas targets; if captions are invisible, this is the line that ran.
		Result = FCoreStyle::GetDefaultFontStyle("Regular", FinalSize);
	}

	if (OutlineSize > 0)
	{
		// Scaled with the text: a fixed two-pixel outline behind text at 200 % reads as a hairline and the
		// caption stops being legible exactly for the player who turned the size up.
		Result.OutlineSettings.OutlineSize = FMath::Max(1, FMath::RoundToInt(OutlineSize * SizeScale));
		Result.OutlineSettings.OutlineColor = InOutlineColor;
	}
	else
	{
		Result.OutlineSettings.OutlineSize = 0;
	}

	return Result;
}

float UCaptionCueStyle::EvaluateOpacity(float Age, float Duration) const
{
	float Opacity = 1.0f;

	if (FadeInSeconds > 0.0f && Age < FadeInSeconds)
	{
		Opacity = FMath::Clamp(Age / FadeInSeconds, 0.0f, 1.0f);
	}

	const float Remaining = Duration - Age;
	if (FadeOutSeconds > 0.0f && Remaining < FadeOutSeconds)
	{
		Opacity = FMath::Min(Opacity, FMath::Clamp(Remaining / FadeOutSeconds, 0.0f, 1.0f));
	}

	return Opacity;
}

FText UCaptionCueStyle::FormatSpeakerPrefix(const FText& InSpeakerName) const
{
	if (InSpeakerName.IsEmpty() || SpeakerFormat == ECaptionCueSpeakerFormat::None)
	{
		return FText::GetEmpty();
	}

	// Format patterns rather than string concatenation, so a translator can move the punctuation. French
	// captioning, for instance, puts a space before the colon; that is a translation decision, not a code one.
	switch (SpeakerFormat)
	{
	case ECaptionCueSpeakerFormat::Brackets:
		return FText::Format(LOCTEXT("SpeakerPrefixBrackets", "[{0}] "), InSpeakerName);

	case ECaptionCueSpeakerFormat::Dash:
		return FText::Format(LOCTEXT("SpeakerPrefixDash", "{0} - "), InSpeakerName);

	case ECaptionCueSpeakerFormat::OwnLine:
		return FText::Format(LOCTEXT("SpeakerPrefixOwnLine", "{0}\n"), InSpeakerName);

	case ECaptionCueSpeakerFormat::Colon:
	default:
		return FText::Format(LOCTEXT("SpeakerPrefixColon", "{0}: "), InSpeakerName);
	}
}

FText UCaptionCueStyle::FormatWithSpeaker(const FText& InText, const FText& InSpeakerName) const
{
	const FText SpeakerPrefix = FormatSpeakerPrefix(InSpeakerName);
	if (SpeakerPrefix.IsEmpty())
	{
		return InText;
	}

	return FText::Format(LOCTEXT("SpeakerAndLine", "{0}{1}"), SpeakerPrefix, InText);
}

FText UCaptionCueStyle::FormatNonSpeech(const FText& InText) const
{
	if (InText.IsEmpty() || (Prefix.IsEmpty() && Suffix.IsEmpty()))
	{
		return InText;
	}

	return FText::Format(
		LOCTEXT("NonSpeechLine", "{0}{1}{2}"),
		FText::FromString(Prefix),
		InText,
		FText::FromString(Suffix));
}

#undef LOCTEXT_NAMESPACE

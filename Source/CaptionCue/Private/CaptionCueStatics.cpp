// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "CaptionCueStatics.h"

#include "CaptionCueSettings.h"
#include "CaptionCueSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace CaptionCueStaticsLocal
{
	static UCaptionCueSubsystem* FindSubsystem(const UObject* WorldContextObject)
	{
		if (!GEngine)
		{
			return nullptr;
		}

		const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
		return World ? World->GetSubsystem<UCaptionCueSubsystem>() : nullptr;
	}
}

// -------------------------------------------------------------------------------------------------------
// Showing captions
// -------------------------------------------------------------------------------------------------------

int32 UCaptionCueStatics::ShowSubtitle(const UObject* WorldContextObject, const FText& Text, FName Speaker, float Duration, int32 Priority)
{
	UCaptionCueSubsystem* Captions = CaptionCueStaticsLocal::FindSubsystem(WorldContextObject);
	return Captions ? Captions->QueueSpeech(Text, Speaker, Duration, Priority) : 0;
}

int32 UCaptionCueStatics::ShowSoundCaption(const UObject* WorldContextObject, const FText& Text, FVector WorldLocation, bool bHasWorldLocation, float Duration, int32 Priority)
{
	UCaptionCueSubsystem* Captions = CaptionCueStaticsLocal::FindSubsystem(WorldContextObject);
	return Captions ? Captions->QueueSoundCaption(Text, WorldLocation, bHasWorldLocation, Duration, Priority) : 0;
}

int32 UCaptionCueStatics::ShowCaptionFromRequest(const UObject* WorldContextObject, const FCaptionCueRequest& Request)
{
	UCaptionCueSubsystem* Captions = CaptionCueStaticsLocal::FindSubsystem(WorldContextObject);
	return Captions ? Captions->QueueCaption(Request) : 0;
}

int32 UCaptionCueStatics::ShowCaptionNow(const UObject* WorldContextObject, const FCaptionCueRequest& Request)
{
	UCaptionCueSubsystem* Captions = CaptionCueStaticsLocal::FindSubsystem(WorldContextObject);
	return Captions ? Captions->ShowCaption(Request) : 0;
}

void UCaptionCueStatics::ClearCaptions(const UObject* WorldContextObject)
{
	if (UCaptionCueSubsystem* Captions = CaptionCueStaticsLocal::FindSubsystem(WorldContextObject))
	{
		Captions->ClearCaptions();
	}
}

void UCaptionCueStatics::RegisterSpeaker(const UObject* WorldContextObject, FName SpeakerId, const FText& DisplayName, FLinearColor Color)
{
	if (UCaptionCueSubsystem* Captions = CaptionCueStaticsLocal::FindSubsystem(WorldContextObject))
	{
		Captions->RegisterSpeaker(SpeakerId, DisplayName, Color);
	}
}

void UCaptionCueStatics::PlaySoundWithCaptionAtLocation(const UObject* WorldContextObject, USoundBase* Sound, FVector Location, const FText& Caption, ECaptionCueKind Kind, FName Speaker, int32 Priority)
{
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(WorldContextObject, Sound, Location);
	}

	if (Caption.IsEmpty())
	{
		return;
	}

	UCaptionCueSubsystem* Captions = CaptionCueStaticsLocal::FindSubsystem(WorldContextObject);
	if (!Captions)
	{
		return;
	}

	// The sound's own length is a starting point, not the answer: the reading clock in the subsystem raises
	// it to whatever the line actually takes to read.
	const float Duration = Sound ? Sound->GetDuration() : 0.0f;

	FCaptionCueRequest Request;
	Request.Text = Caption;
	Request.Speaker = Speaker;
	Request.Kind = Kind;
	Request.Priority = Priority;
	Request.Duration = FMath::IsFinite(Duration) ? Duration : 0.0f;
	Request.bHasWorldLocation = true;
	Request.WorldLocation = Location;

	Captions->QueueCaption(Request);
}

// -------------------------------------------------------------------------------------------------------
// The options menu
// -------------------------------------------------------------------------------------------------------

UCaptionCueSettings* UCaptionCueStatics::GetCaptionSettings()
{
	return UCaptionCueSettings::GetMutable();
}

void UCaptionCueStatics::SetCaptionsEnabled(bool bEnabled)
{
	UCaptionCueSettings::GetMutable()->SetCaptionsEnabled(bEnabled);
}

void UCaptionCueStatics::SetTextScale(float TextScale)
{
	UCaptionCueSettings::GetMutable()->SetTextScale(TextScale);
}

float UCaptionCueStatics::GetTextScale()
{
	return UCaptionCueSettings::Get().TextScale;
}

void UCaptionCueStatics::SetBackgroundBox(bool bEnabled)
{
	UCaptionCueSettings::GetMutable()->SetBackgroundBox(bEnabled);
}

bool UCaptionCueStatics::IsBackgroundBoxEnabled()
{
	return UCaptionCueSettings::Get().bBackgroundBox;
}

void UCaptionCueStatics::SetBackgroundOpacity(float Opacity)
{
	UCaptionCueSettings::GetMutable()->SetBackgroundOpacity(Opacity);
}

void UCaptionCueStatics::SetShowSpeakerNames(bool bEnabled)
{
	UCaptionCueSettings::GetMutable()->SetShowSpeakerNames(bEnabled);
}

bool UCaptionCueStatics::AreSpeakerNamesShown()
{
	return UCaptionCueSettings::Get().bShowSpeakerNames;
}

void UCaptionCueStatics::SetShowSoundCaptions(bool bEnabled)
{
	UCaptionCueSettings::GetMutable()->SetShowSoundCaptions(bEnabled);
}

bool UCaptionCueStatics::AreSoundCaptionsShown()
{
	return UCaptionCueSettings::Get().bShowSoundCaptions;
}

void UCaptionCueStatics::SetMaxLineWidthPercent(float Percent)
{
	UCaptionCueSettings::GetMutable()->SetMaxLineWidthPercent(Percent);
}

void UCaptionCueStatics::SetSafeAreaMarginPercent(float Percent)
{
	UCaptionCueSettings::GetMutable()->SetSafeAreaMarginPercent(Percent);
}

void UCaptionCueStatics::SetCaptionPosition(ECaptionCuePosition Position)
{
	UCaptionCueSettings::GetMutable()->SetCaptionPosition(Position);
}

void UCaptionCueStatics::SetMaxVisibleLines(int32 MaxVisibleLines)
{
	UCaptionCueSettings::GetMutable()->SetMaxVisibleLines(MaxVisibleLines);
}

void UCaptionCueStatics::SetDirectionIndicators(bool bEnabled)
{
	UCaptionCueSettings::GetMutable()->SetDirectionIndicators(bEnabled);
}

void UCaptionCueStatics::SetMinSecondsPerCharacter(float SecondsPerCharacter)
{
	UCaptionCueSettings::GetMutable()->SetMinSecondsPerCharacter(SecondsPerCharacter);
}

// -------------------------------------------------------------------------------------------------------
// Access
// -------------------------------------------------------------------------------------------------------

UCaptionCueSubsystem* UCaptionCueStatics::GetCaptionSubsystem(const UObject* WorldContextObject)
{
	return CaptionCueStaticsLocal::FindSubsystem(WorldContextObject);
}

FCaptionCueStats UCaptionCueStatics::GetCaptionStats(const UObject* WorldContextObject)
{
	const UCaptionCueSubsystem* Captions = CaptionCueStaticsLocal::FindSubsystem(WorldContextObject);
	return Captions ? Captions->GetStats() : FCaptionCueStats();
}

bool UCaptionCueStatics::IsEngineSubtitleBridgeAttached(const UObject* WorldContextObject)
{
	const UCaptionCueSubsystem* Captions = CaptionCueStaticsLocal::FindSubsystem(WorldContextObject);
	return Captions ? Captions->IsEngineBridgeAttached() : false;
}

// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "CaptionCueAudioComponent.h"

#include "CaptionCueSubsystem.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"

UCaptionCueAudioComponent::UCaptionCueAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCaptionCueAudioComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bCaptionsEnabled && !OnQueueSubtitles.IsBound())
	{
		// The interception point the engine itself offers: with this bound, FSubtitleManager never sees this
		// component's cues, so nothing is drawn twice and there is nothing to switch off in the project.
		OnQueueSubtitles.BindDynamic(this, &UCaptionCueAudioComponent::HandleQueueSubtitles);
	}

	if (bCaptionOnPlay && !CaptionOverride.IsEmpty() && IsPlaying())
	{
		// bAutoActivate started the sound before BeginPlay ran, so the caption would otherwise be missed.
		QueueOverrideCaption();
	}
}

void UCaptionCueAudioComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnQueueSubtitles.Unbind();

	Super::EndPlay(EndPlayReason);
}

void UCaptionCueAudioComponent::PlayWithCaption(float StartTime)
{
	if (bCaptionOnPlay && !CaptionOverride.IsEmpty())
	{
		QueueOverrideCaption();
	}

	Play(StartTime);
}

int32 UCaptionCueAudioComponent::QueueOverrideCaption()
{
	if (CaptionOverride.IsEmpty())
	{
		return 0;
	}

	const UWorld* World = GetWorld();
	UCaptionCueSubsystem* Captions = World ? World->GetSubsystem<UCaptionCueSubsystem>() : nullptr;
	if (!Captions)
	{
		return 0;
	}

	// The sound's own length, when it has one. Zero is fine: the reading clock in the subsystem works the
	// display time out from the text instead, which is what a caption wants anyway.
	const float Duration = Sound ? Sound->GetDuration() : 0.0f;

	return Captions->QueueCaption(MakeRequest(CaptionOverride, FMath::IsFinite(Duration) ? Duration : 0.0f));
}

void UCaptionCueAudioComponent::HandleQueueSubtitles(const TArray<FSubtitleCue>& Subtitles, float CueDuration)
{
	if (!bCaptionsEnabled)
	{
		return;
	}

	const UWorld* World = GetWorld();
	UCaptionCueSubsystem* Captions = World ? World->GetSubsystem<UCaptionCueSubsystem>() : nullptr;
	if (!Captions)
	{
		return;
	}

	for (int32 Index = 0; Index < Subtitles.Num(); ++Index)
	{
		const FSubtitleCue& Cue = Subtitles[Index];
		if (Cue.Text.IsEmpty())
		{
			// The engine appends an empty cue at the end of every set to clear the display. It has no text,
			// so there is nothing to caption; the reading clock decides when the last line leaves.
			continue;
		}

		// Each cue lasts until the next one starts, and the last one until the sound is over. The cues are
		// all queued at once and the queue plays them out in order, which is exactly the behaviour that
		// stops a fast line of dialogue overwriting the one before it.
		const float NextTime = Subtitles.IsValidIndex(Index + 1) ? Subtitles[Index + 1].Time : CueDuration;
		const float Duration = FMath::Max(NextTime - Cue.Time, 0.0f);

		Captions->QueueCaption(MakeRequest(Cue.Text, Duration));
	}
}

FCaptionCueRequest UCaptionCueAudioComponent::MakeRequest(const FText& Text, float Duration) const
{
	FCaptionCueRequest Request;
	Request.Text = Text;
	Request.Speaker = CaptionSpeaker;
	Request.Kind = CaptionKind;
	Request.Duration = Duration;
	Request.Priority = CaptionPriority;

	if (bUseComponentLocation)
	{
		Request.bHasWorldLocation = true;
		Request.WorldLocation = GetComponentLocation();
	}

	return Request;
}

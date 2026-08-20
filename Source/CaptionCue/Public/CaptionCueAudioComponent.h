// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CaptionCueTypes.h"
#include "Components/AudioComponent.h"
#include "CaptionCueAudioComponent.generated.h"

struct FSubtitleCue;

/**
 * An audio component that brings its caption with it.
 *
 * This is the richer of the two ways CaptionCue connects to sound, and the one to use for anything you own.
 * UAudioComponent has a documented interception point, OnQueueSubtitles, that fires when a wave's subtitle
 * cues are on their way to FSubtitleManager. Binding it hands us the cues *and their timings* and the
 * sound's own duration, which the process-wide bridge (see FCaptionCueSubtitleBridge) cannot carry — and
 * because this component knows where in the world it is, its captions can have a direction arrow.
 *
 * Three things it gives you that a plain audio component cannot:
 *
 *   - **A speaker.** Set Caption Speaker once on the component and every line this sound plays is labelled
 *     and coloured as that character. Engine subtitles have never had a speaker field.
 *   - **A position.** With Use Component Location on, an off-screen sound draws an edge arrow pointing at it.
 *   - **A caption for a sound with no cues.** Most sound effects have no Subtitles array at all. Fill in
 *     Caption Override and the component queues that line when the sound starts, which is how you caption a
 *     door, a reload or a distant explosion without editing every wave in the project.
 *
 * Note that binding OnQueueSubtitles means this component's cues no longer reach the engine subtitle
 * manager. That is intended: it is what stops the same line being drawn twice.
 */
UCLASS(ClassGroup = (CaptionCue), meta = (BlueprintSpawnableComponent, DisplayName = "CaptionCue Audio Component"))
class CAPTIONCUE_API UCaptionCueAudioComponent : public UAudioComponent
{
	GENERATED_BODY()

public:
	UCaptionCueAudioComponent();

	// UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Route this sound's subtitle cues into CaptionCue instead of the engine's subtitle manager. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptionCue")
	bool bCaptionsEnabled = true;

	/** Who is speaking. Leave at None for a sound nobody is making. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptionCue")
	FName CaptionSpeaker;

	/** Speech, sound, music or system. Decides which style draws the line and whether it gets an arrow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptionCue")
	ECaptionCueKind CaptionKind = ECaptionCueKind::Speech;

	/** Priority the captions from this component are queued with. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptionCue")
	int32 CaptionPriority = 0;

	/**
	 * Give the caption this component's world position, so an off-screen sound gets a direction arrow.
	 * Switch it off for a voice-over that is not coming from anywhere in particular.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptionCue")
	bool bUseComponentLocation = true;

	/**
	 * The line to show when the sound has no subtitle cues of its own — which is true of nearly every sound
	 * effect ever recorded. Left empty, only waves that carry cues produce a caption.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptionCue", meta = (MultiLine = "true"))
	FText CaptionOverride;

	/** Queue the Caption Override as soon as the sound starts, rather than waiting to be asked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptionCue")
	bool bCaptionOnPlay = true;

	/**
	 * Start the sound and queue its Caption Override in the same call. The ordinary Play node works too;
	 * this one exists so that a caption and its sound cannot get out of step in a Blueprint.
	 */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue")
	void PlayWithCaption(float StartTime = 0.0f);

	/** Queue this component's Caption Override now, without touching the sound. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue")
	int32 QueueOverrideCaption();

private:
	/** Bound to UAudioComponent::OnQueueSubtitles; receives the wave's cues before the engine sees them. */
	UFUNCTION()
	void HandleQueueSubtitles(const TArray<FSubtitleCue>& Subtitles, float CueDuration);

	/** Build a request with this component's speaker, kind, priority and position already filled in. */
	FCaptionCueRequest MakeRequest(const FText& Text, float Duration) const;
};

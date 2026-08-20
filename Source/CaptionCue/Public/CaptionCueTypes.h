// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CaptionCueTypes.generated.h"

/**
 * What kind of thing produced this line.
 *
 * The distinction is not cosmetic. "Subtitles" are spoken words; "closed captions" also describe the sound
 * a hearing player would have heard. A player who can hear may want the first and not the second, which is
 * why Show Sound Captions is a separate option, and why the kind travels with every entry.
 */
UENUM(BlueprintType)
enum class ECaptionCueKind : uint8
{
	/** Spoken dialogue. Carries a speaker. */
	Speech UMETA(DisplayName = "Speech"),

	/** A non-speech sound: a door, a footstep, a reload. Usually bracketed, usually carries a world location. */
	Sound UMETA(DisplayName = "Sound Effect"),

	/** Music and its cues: a track starting, a stinger, lyrics. */
	Music UMETA(DisplayName = "Music"),

	/** The game talking to the player: objectives, warnings, tutorial lines. */
	System UMETA(DisplayName = "System")
};

/**
 * Horizontal alignment of the wrapped lines inside a caption block.
 *
 * CaptionCue defines its own rather than reusing Slate's ETextJustify: that enum's reflection data lives in
 * the Slate module, and a runtime module that has to survive a cooked Shipping build has no business
 * pulling in the whole widget framework to describe "centre this line".
 */
UENUM(BlueprintType)
enum class ECaptionCueJustify : uint8
{
	Left UMETA(DisplayName = "Left"),
	Center UMETA(DisplayName = "Center"),
	Right UMETA(DisplayName = "Right")
};

/** Where the caption block sits on screen. */
UENUM(BlueprintType)
enum class ECaptionCuePosition : uint8
{
	/** Under the action, the default everywhere. */
	Bottom UMETA(DisplayName = "Bottom"),

	/** Above it, for games whose bottom edge is already full of interface. */
	Top UMETA(DisplayName = "Top")
};

/**
 * One caption on its way to the screen, as a Blueprint sees it.
 *
 * This is the input side. Everything is optional except the text: leave Duration at zero and the reading
 * time is worked out from the length of the line, leave Speaker at None and no name is drawn, leave
 * b Has World Location off and no direction arrow is drawn.
 */
USTRUCT(BlueprintType)
struct CAPTIONCUE_API FCaptionCueRequest
{
	GENERATED_BODY()

	/** The line itself. FText, never FString: this is the one string in the plugin a player actually reads. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptionCue")
	FText Text;

	/**
	 * Who said it, as a stable id. Register the id once with a display name and a colour
	 * (Caption Cue Settings > Speakers, or Register Speaker at runtime) and every line from that character
	 * is labelled and coloured the same way.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptionCue")
	FName Speaker;

	/**
	 * Optional display name that overrides the registered one. Use it when a character has to stay
	 * "???" until the player has met them.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptionCue")
	FText SpeakerDisplayName;

	/** Speech, sound, music or system. Decides which style draws the line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptionCue")
	ECaptionCueKind Kind = ECaptionCueKind::Speech;

	/**
	 * Seconds the line should stay up, usually the length of the sound. Zero means "work it out from the
	 * text". Either way the minimum reading time still applies: a two-second line of forty characters does
	 * not vanish in two seconds.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptionCue", meta = (ClampMin = "0.0", UIMax = "30.0"))
	float Duration = 0.0f;

	/**
	 * Higher wins. A line with a higher priority is promoted out of the queue first, and Show Caption uses
	 * it to decide whether it may push a line that is already on screen aside.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptionCue")
	int32 Priority = 0;

	/** Turn on to make World Location meaningful. Off means "this sound has no place in the world". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptionCue")
	bool bHasWorldLocation = false;

	/**
	 * Where the sound came from. When it is off screen, a small arrow points at it from the edge — the
	 * difference between a subtitle and a closed caption: "[door creaks]" is not much use without a
	 * direction.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptionCue", meta = (EditCondition = "bHasWorldLocation"))
	FVector WorldLocation = FVector::ZeroVector;

	/**
	 * Draw with the style registered under this name instead of the one the Kind selects.
	 * Leave at None for the normal path.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptionCue", meta = (AdvancedDisplay))
	FName StyleOverride;
};

/**
 * One live caption inside the subsystem.
 *
 * Deliberately not a USTRUCT. World Location is a TOptional, which is the honest type for "this line may or
 * may not have come from somewhere" and which Blueprint has no representation for — FCaptionCueRequest is
 * the reflected shape that crosses that boundary. Nothing here is garbage-collected, so the entries can
 * live in a plain array.
 */
struct FCaptionCueEntry
{
	/** The line as it will be read. */
	FText Text;

	/** Speaker id, or None. */
	FName Speaker;

	/** Resolved display name for the speaker; empty means "fall back to the registry, then to the id". */
	FText SpeakerDisplayName;

	/** Speech, sound, music, system. */
	ECaptionCueKind Kind = ECaptionCueKind::Speech;

	/** Style name override, or None. */
	FName StyleOverride;

	/** Seconds this line stays up once it is visible, minimum reading time already folded in. */
	float Duration = 0.0f;

	/** Seconds it has been visible. Only counts while the line is actually on screen, never while queued. */
	float Age = 0.0f;

	/** Seconds it has been waiting in the queue. Used to break priority ties in favour of the older line. */
	float QueuedAge = 0.0f;

	/** Higher wins. */
	int32 Priority = 0;

	/** Where the sound was, if it was anywhere. */
	TOptional<FVector> WorldLocation;

	/** Handed out by the subsystem, unique per world, never reused. Zero is "no entry". */
	uint32 Id = 0;

	/** Monotonic counter, so two lines queued in the same frame keep the order they were queued in. */
	uint32 Sequence = 0;

	/** True for lines that came in through the engine's own subtitle stream rather than the plugin API. */
	bool bFromEngineBridge = false;

	/** Normalised progress through the display time, clamped to [0,1]. Drives the fade. */
	float GetAlpha() const
	{
		return Duration > 0.0f ? FMath::Clamp(Age / Duration, 0.0f, 1.0f) : 1.0f;
	}
};

/** What the caption layer did, for the on-screen statistics box and for anyone wiring up a test. */
USTRUCT(BlueprintType)
struct CAPTIONCUE_API FCaptionCueStats
{
	GENERATED_BODY()

	/** Lines on screen right now. */
	UPROPERTY(BlueprintReadOnly, Category = "CaptionCue")
	int32 Visible = 0;

	/** Lines waiting for a free slot. */
	UPROPERTY(BlueprintReadOnly, Category = "CaptionCue")
	int32 Pending = 0;

	/** Lines accepted since the world came up. */
	UPROPERTY(BlueprintReadOnly, Category = "CaptionCue")
	int32 TotalQueued = 0;

	/** Of those, how many arrived through the engine subtitle bridge. */
	UPROPERTY(BlueprintReadOnly, Category = "CaptionCue")
	int32 TotalFromEngineBridge = 0;

	/** Lines that a higher-priority Show Caption pushed off screen and back into the queue. */
	UPROPERTY(BlueprintReadOnly, Category = "CaptionCue")
	int32 TotalDisplaced = 0;

	/** Lines dropped because the queue was at its limit. */
	UPROPERTY(BlueprintReadOnly, Category = "CaptionCue")
	int32 TotalDropped = 0;

	/** Direction arrows drawn in the last canvas pass. */
	UPROPERTY(BlueprintReadOnly, Category = "CaptionCue")
	int32 ArrowsDrawn = 0;

	/** Milliseconds the last canvas pass took, wrapping and measuring included. */
	UPROPERTY(BlueprintReadOnly, Category = "CaptionCue")
	float DrawMs = 0.0f;
};

/** A character the caption layer knows how to label. */
USTRUCT(BlueprintType)
struct CAPTIONCUE_API FCaptionCueSpeaker
{
	GENERATED_BODY()

	/** The id gameplay code passes around. Never shown to the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptionCue")
	FName SpeakerId;

	/**
	 * The name the player reads. FText with its own key, so a translator can localise "Guard" without
	 * anyone touching the id the Blueprints use.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptionCue")
	FText DisplayName;

	/**
	 * The colour the name is drawn in. Distinct colours per speaker are the cheapest way to make a
	 * three-way conversation followable, and they are on every certification checklist.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptionCue")
	FLinearColor Color = FLinearColor(0.85f, 0.85f, 0.9f, 1.0f);
};

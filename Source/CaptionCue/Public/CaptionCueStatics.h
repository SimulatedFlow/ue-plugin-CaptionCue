// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CaptionCueTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CaptionCueStatics.generated.h"

class UCaptionCueSettings;
class UCaptionCueSubsystem;
class USoundBase;

/**
 * Everything CaptionCue does, from Blueprint, without a reference to anything.
 *
 * Two halves. The first shows captions: Show Subtitle, Show Sound Caption, Clear Captions. The second is
 * the options menu: Set Text Scale, Set Background Box, Set Show Speaker Names and the rest. A customer
 * building the accessibility page of their options screen should never have to open C++, and should never
 * have to find a subsystem first — that is what this library is for.
 */
UCLASS(meta = (ScriptName = "CaptionCueStatics"))
class CAPTIONCUE_API UCaptionCueStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ----------------------------------------------------------------------------------------------------
	// Showing captions
	// ----------------------------------------------------------------------------------------------------

	/**
	 * Show a spoken line. It appears as soon as there is a free line and never in place of a line the
	 * player has not had time to read.
	 *
	 * @param Speaker  Registered speaker id, or None for an unattributed line.
	 * @param Duration Length of the sound in seconds, or 0 to let the reading speed decide.
	 * @return The caption's id, or 0 when it was refused.
	 */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "3"))
	static int32 ShowSubtitle(const UObject* WorldContextObject, const FText& Text, FName Speaker, float Duration = 0.0f, int32 Priority = 0);

	/**
	 * Show a caption for something the player heard rather than something somebody said: a door, a reload,
	 * a scream two rooms away. Give it a world position and an off-screen source draws an edge arrow.
	 */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "3"))
	static int32 ShowSoundCaption(const UObject* WorldContextObject, const FText& Text, FVector WorldLocation, bool bHasWorldLocation = true, float Duration = 0.0f, int32 Priority = 0);

	/** Show a caption with every field spelled out: kind, style override, position and all. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue", meta = (WorldContext = "WorldContextObject"))
	static int32 ShowCaptionFromRequest(const UObject* WorldContextObject, const FCaptionCueRequest& Request);

	/**
	 * Put a caption on screen at once, pushing aside a visible line of strictly lower priority. The line
	 * that gets pushed aside goes back to the front of the queue, not into the bin.
	 */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue", meta = (WorldContext = "WorldContextObject"))
	static int32 ShowCaptionNow(const UObject* WorldContextObject, const FCaptionCueRequest& Request);

	/** Everything on screen and everything waiting, gone. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue", meta = (WorldContext = "WorldContextObject"))
	static void ClearCaptions(const UObject* WorldContextObject);

	/** Teach this world a speaker: the id gameplay code uses, the name the player reads, the colour it is drawn in. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue", meta = (WorldContext = "WorldContextObject"))
	static void RegisterSpeaker(const UObject* WorldContextObject, FName SpeakerId, const FText& DisplayName, FLinearColor Color);

	/**
	 * Play a sound at a world position and caption it in the same call, with the sound's own length used as
	 * the display time. For sounds that carry no subtitle cues of their own, which is nearly all of them.
	 */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "4"))
	static void PlaySoundWithCaptionAtLocation(const UObject* WorldContextObject, USoundBase* Sound, FVector Location, const FText& Caption, ECaptionCueKind Kind = ECaptionCueKind::Sound, FName Speaker = NAME_None, int32 Priority = 0);

	// ----------------------------------------------------------------------------------------------------
	// The options menu
	// ----------------------------------------------------------------------------------------------------

	/**
	 * The settings object every option below writes to. Bind sliders and check boxes straight to it if you
	 * prefer, or call the wrappers here; both reach the same values.
	 */
	UFUNCTION(BlueprintPure, Category = "CaptionCue|Options")
	static UCaptionCueSettings* GetCaptionSettings();

	/** Captions on or off entirely. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	static void SetCaptionsEnabled(bool bEnabled);

	/** Text size multiplier. 0.75, 1.0, 1.5 and 2.0 are the four presets worth offering. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	static void SetTextScale(float TextScale);

	/** Current text size multiplier. */
	UFUNCTION(BlueprintPure, Category = "CaptionCue|Options")
	static float GetTextScale();

	/** Background box behind the text on or off. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	static void SetBackgroundBox(bool bEnabled);

	/** True while the background box is on. */
	UFUNCTION(BlueprintPure, Category = "CaptionCue|Options")
	static bool IsBackgroundBoxEnabled();

	/** How solid the background box is, in [0,1]. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	static void SetBackgroundOpacity(float Opacity);

	/** Speaker names in front of spoken lines on or off. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	static void SetShowSpeakerNames(bool bEnabled);

	/** True while speaker names are being written. */
	UFUNCTION(BlueprintPure, Category = "CaptionCue|Options")
	static bool AreSpeakerNamesShown();

	/** Captions for non-speech sound on or off — the switch between subtitles and closed captions. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	static void SetShowSoundCaptions(bool bEnabled);

	/** True while non-speech sound is being captioned. */
	UFUNCTION(BlueprintPure, Category = "CaptionCue|Options")
	static bool AreSoundCaptionsShown();

	/** Widest a caption may get, as a fraction of the viewport width. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	static void SetMaxLineWidthPercent(float Percent);

	/** Margin kept free at the edges of the screen, as a fraction of the viewport. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	static void SetSafeAreaMarginPercent(float Percent);

	/** Captions at the bottom of the screen or at the top. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	static void SetCaptionPosition(ECaptionCuePosition Position);

	/** How many captions may share the screen. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	static void SetMaxVisibleLines(int32 MaxVisibleLines);

	/** Edge arrows for off-screen sounds on or off. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	static void SetDirectionIndicators(bool bEnabled);

	/** Reading time granted per character of the line. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	static void SetMinSecondsPerCharacter(float SecondsPerCharacter);

	// ----------------------------------------------------------------------------------------------------
	// Access
	// ----------------------------------------------------------------------------------------------------

	/** This world's caption layer, for anything the library does not wrap. May be null outside a world. */
	UFUNCTION(BlueprintPure, Category = "CaptionCue", meta = (WorldContext = "WorldContextObject"))
	static UCaptionCueSubsystem* GetCaptionSubsystem(const UObject* WorldContextObject);

	/** What the caption layer is doing right now. */
	UFUNCTION(BlueprintPure, Category = "CaptionCue", meta = (WorldContext = "WorldContextObject"))
	static FCaptionCueStats GetCaptionStats(const UObject* WorldContextObject);

	/** True while SoundWave and DialogueWave subtitles are being routed into CaptionCue. */
	UFUNCTION(BlueprintPure, Category = "CaptionCue", meta = (WorldContext = "WorldContextObject"))
	static bool IsEngineSubtitleBridgeAttached(const UObject* WorldContextObject);
};

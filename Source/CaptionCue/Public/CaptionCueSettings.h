// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CaptionCueTypes.h"
#include "Engine/DeveloperSettings.h"
#include "CaptionCueSettings.generated.h"

class UCaptionCueStyle;

/**
 * Everything a player is allowed to change about captions, plus the project-wide defaults behind it.
 *
 * These are not "settings" in the usual sense of values you tune once during development and forget. Text
 * size, background box, speaker names, sound captions, line width, safe area and position are the entries a
 * console certification checklist asks to find in the game's own options menu — so every one of them has a
 * Blueprint-callable setter that takes effect on the very next frame, with no restart and no reload. Wire
 * the setters to your own menu widget and you have built the accessibility page.
 *
 * The values live on the settings object itself rather than in a separate runtime copy, so there is exactly
 * one source of truth: what the menu writes is what the next canvas pass reads. Project Settings >
 * Plugins > CaptionCue edits the shipped defaults, which are stored in DefaultGame.ini.
 *
 * On persistence: the setters change the live values only. In a packaged game the Default*.ini files are
 * read-only, so save the player's choices wherever your game already saves its options (usually a
 * UGameUserSettings subclass or a save game) and call the setters again on load. Save Caption Options is
 * provided for development, where writing the ini is exactly what you want.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "CaptionCue"))
class CAPTIONCUE_API UCaptionCueSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UCaptionCueSettings();

	// UDeveloperSettings interface
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;

	/** Read-only accessor for the draw path. Never returns null. */
	static const UCaptionCueSettings& Get();

	/** Writable accessor for the option setters. Never returns null. */
	static UCaptionCueSettings* GetMutable();

	// ----------------------------------------------------------------------------------------------------
	// Player options
	// ----------------------------------------------------------------------------------------------------

	/** Master switch. Off draws nothing at all, queue included. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Player Options")
	bool bCaptionsEnabled = true;

	/**
	 * Size multiplier on every style's font size. 1.0 is the size the designer authored.
	 * The four presets worth putting in a menu are 0.75, 1.0, 1.5 and 2.0, but any value in range works.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Player Options", meta = (ClampMin = "0.25", ClampMax = "4.0"))
	float TextScale = 1.0f;

	/**
	 * Draw a box behind the text. This is the single most requested subtitle option there is, because
	 * without it a white caption over a snowfield is unreadable no matter how big it gets.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Player Options")
	bool bBackgroundBox = true;

	/** How solid that box is. Multiplied into each style's own background colour. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Player Options", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BackgroundOpacity = 0.65f;

	/** Write the speaker's name in front of spoken lines. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Player Options")
	bool bShowSpeakerNames = true;

	/**
	 * Show captions for non-speech sound as well as for speech. This is the switch between "subtitles" and
	 * "closed captions", and it belongs to the player: someone who can hear the door usually does not want
	 * to read about it.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Player Options")
	bool bShowSoundCaptions = true;

	/**
	 * Widest a caption may get, as a fraction of the viewport width. Longer lines wrap.
	 * Around 0.6 is where readability peaks: a line that runs the full width of a television makes the eye
	 * travel too far to find the start of the next one.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Player Options", meta = (ClampMin = "0.2", ClampMax = "1.0"))
	float MaxLineWidthPercent = 0.6f;

	/**
	 * Margin kept free at the edges of the screen, as a fraction of the viewport.
	 * Television overscan is why this exists; a caption in the outer five percent of the picture can be
	 * cut off on a set nobody in the studio owns.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Player Options", meta = (ClampMin = "0.0", ClampMax = "0.25"))
	float SafeAreaMarginPercent = 0.05f;

	/** Captions at the bottom of the screen or at the top. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Player Options")
	ECaptionCuePosition Position = ECaptionCuePosition::Bottom;

	/**
	 * How many captions may be on screen at once. Anything beyond this waits its turn rather than pushing
	 * an unread line off — that is the whole point of the queue.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Player Options", meta = (ClampMin = "1", ClampMax = "8"))
	int32 MaxVisibleLines = 3;

	/**
	 * Draw an arrow at the edge of the screen pointing at an off-screen sound.
	 * "[door creaks]" is only half the information; the other half is which door.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Player Options")
	bool bDirectionIndicators = true;

	// ----------------------------------------------------------------------------------------------------
	// Reading time
	// ----------------------------------------------------------------------------------------------------

	/**
	 * Seconds of display time granted per character of the line.
	 * 0.06 is roughly 200 words a minute, a comfortable reading speed rather than a fast one. A caption
	 * that disappears with the sound is a caption nobody finished reading, and it is the failure that
	 * shows up in accessibility reviews more than any other.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Reading Time", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float MinSecondsPerCharacter = 0.06f;

	/** Floor on display time, however short the line and however short the sound. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Reading Time", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float MinDisplaySeconds = 1.2f;

	/** Ceiling on display time, so a mis-tagged sound cannot leave a caption on screen forever. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Reading Time", meta = (ClampMin = "1.0", ClampMax = "120.0"))
	float MaxDisplaySeconds = 12.0f;

	/**
	 * Most captions the queue will hold before it starts refusing new ones. A game that queues more than
	 * this has a scripting problem, and silently growing the queue would hide it.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Reading Time", meta = (ClampMin = "1", ClampMax = "256"))
	int32 MaxQueuedCaptions = 32;

	// ----------------------------------------------------------------------------------------------------
	// Styles and speakers
	// ----------------------------------------------------------------------------------------------------

	/**
	 * The style assets this project uses. A style is picked by name when a caption asks for one, otherwise
	 * by matching Kind. Any kind with no style gets a built-in fallback at runtime, so CaptionCue draws
	 * something sensible in a project where nobody has authored an asset yet.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Styles", meta = (AllowedClasses = "/Script/CaptionCue.CaptionCueStyle"))
	TArray<TSoftObjectPtr<UCaptionCueStyle>> Styles;

	/**
	 * Speakers known at project level: id, the name the player reads, and the colour it is drawn in.
	 * Characters that only exist in one level are usually better registered at runtime with Register
	 * Speaker instead.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Speakers")
	TArray<FCaptionCueSpeaker> Speakers;

	/**
	 * Give a speaker nobody registered a stable colour derived from their id, instead of the style's
	 * default. Two characters still read as two characters in a game that never got round to registering
	 * them, and the colour does not change between runs.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Speakers")
	bool bAutoColorUnknownSpeakers = true;

	// ----------------------------------------------------------------------------------------------------
	// Engine bridge
	// ----------------------------------------------------------------------------------------------------

	/**
	 * Take over the engine's own subtitle stream.
	 *
	 * With this on, subtitles that a USoundWave or a UDialogueWave already carries stop being drawn by
	 * FSubtitleManager as one unstyled line and arrive in CaptionCue's queue instead — with no change to
	 * the project that owns those sounds. See Docs/DOCUMENTATION.md for exactly which engine hook this
	 * uses and what it can and cannot carry across.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Bridge")
	bool bBridgeEngineSubtitles = true;

	/** Which kind bridged engine subtitles are filed under. Speech is almost always right. */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Bridge", meta = (EditCondition = "bBridgeEngineSubtitles"))
	ECaptionCueKind EngineBridgeKind = ECaptionCueKind::Speech;

	/** Priority bridged engine subtitles are queued with. */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Bridge", meta = (EditCondition = "bBridgeEngineSubtitles"))
	int32 EngineBridgePriority = 0;

	// ----------------------------------------------------------------------------------------------------
	// Editor and debug
	// ----------------------------------------------------------------------------------------------------

	/**
	 * Draw captions in an editor viewport with nothing playing.
	 * This is how the store images are made, and it is also how a designer checks a caption's line breaks
	 * without entering play mode.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Editor")
	bool bEnableEditorPreview = true;

	/** Show the statistics box as soon as a world comes up. Same as CaptionCue.Stats 1. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bShowStatsByDefault = false;

	// ----------------------------------------------------------------------------------------------------
	// Runtime setters - this is the options menu
	// ----------------------------------------------------------------------------------------------------

	/** Turn captions on or off entirely. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	void SetCaptionsEnabled(bool bEnabled);

	/** Text size multiplier. Clamped to [0.25, 4.0]. Takes effect on the next frame. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	void SetTextScale(float InTextScale);

	/** Background box on or off. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	void SetBackgroundBox(bool bEnabled);

	/** How solid the background box is, in [0,1]. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	void SetBackgroundOpacity(float InOpacity);

	/** Speaker names on or off. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	void SetShowSpeakerNames(bool bEnabled);

	/** Captions for non-speech sound on or off. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	void SetShowSoundCaptions(bool bEnabled);

	/** Widest a caption may get, as a fraction of the viewport width. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	void SetMaxLineWidthPercent(float InPercent);

	/** Margin kept free at the edges, as a fraction of the viewport. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	void SetSafeAreaMarginPercent(float InPercent);

	/** Captions at the bottom or at the top. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	void SetCaptionPosition(ECaptionCuePosition InPosition);

	/** How many captions may share the screen. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	void SetMaxVisibleLines(int32 InMaxVisibleLines);

	/** Reading time granted per character. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	void SetMinSecondsPerCharacter(float InSecondsPerCharacter);

	/** Floor on display time, in seconds. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	void SetMinDisplaySeconds(float InSeconds);

	/** Edge arrows for off-screen sounds on or off. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	void SetDirectionIndicators(bool bEnabled);

	/**
	 * Write the current options into DefaultGame.ini.
	 *
	 * Useful while developing; not the right way to remember a player's choice in a shipped game, where
	 * the Default*.ini files are packaged read-only. Persist through your own settings object instead and
	 * call the setters above when it loads.
	 */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Options")
	void SaveCaptionOptions();

	// ----------------------------------------------------------------------------------------------------
	// Derived helpers
	// ----------------------------------------------------------------------------------------------------

	/** Display name and colour for a speaker id from the project-level list, or false when it is unknown. */
	bool FindSpeaker(FName SpeakerId, FCaptionCueSpeaker& OutSpeaker) const;

	/**
	 * Seconds a line of this length has to stay up: never less than the floor, never less than the reading
	 * speed asks for, never more than the ceiling, and never less than the sound itself lasts.
	 */
	float ResolveDisplaySeconds(float RequestedDuration, int32 CharacterCount) const;
};

// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CaptionCueTypes.h"
#include "Fonts/SlateFontInfo.h"
#include "Subsystems/WorldSubsystem.h"
#include "CaptionCueSubsystem.generated.h"

class UCanvas;
class UCaptionCueStyle;
class UFont;
class APlayerController;

/**
 * The caption layer for one world: the queue, the reading clock, and the renderer.
 *
 * CaptionCue does not decide what is said. A dialogue system, a Sequencer track, a sound wave with cues, or
 * one Blueprint node decides that. This subsystem decides how it reads — and that turns out to be where
 * every home-made subtitle system quietly falls over:
 *
 *   1. **A queue, not a variable.** The commonest bug in a hand-rolled subtitle widget is a single "current
 *      line" that the next line overwrites. Two events land in the same frame and the player reads neither.
 *      Here a line goes into a queue and is promoted when there is room; up to Max Visible Lines share the
 *      screen and the rest wait. A line only loses its place to something with a higher priority, and even
 *      then it goes back into the queue rather than into the bin.
 *
 *   2. **A reading clock, not a sound length.** Captions timed to the audio disappear while the player is
 *      still reading them. Every line here gets at least Min Display Seconds and at least Min Seconds Per
 *      Character of screen time, however short the sound was.
 *
 *   3. **Options that move now.** Text scale, background box, speaker names, sound captions, line width,
 *      safe area and position are read on the frame they are drawn, so a menu slider moves the captions
 *      while the player is still holding it. No restart, no re-created widget.
 *
 *   4. **Direction.** A caption with a world position whose sound is off screen draws an arrow at the edge
 *      pointing at it. That is the difference between a subtitle and a closed caption.
 *
 * Drawing happens on UCanvas through AHUD (see ACaptionCueHUD and UCaptionCueHUDComponent), because that is
 * the path that exists in a cooked Shipping build. The editor-viewport preview registered below is a second
 * way in, not the main one.
 */
UCLASS()
class CAPTIONCUE_API UCaptionCueSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override;

	// ----------------------------------------------------------------------------------------------------
	// Putting captions on screen
	// ----------------------------------------------------------------------------------------------------

	/**
	 * Put a caption in the queue. It appears as soon as there is a free line, and never in place of a line
	 * that has not had its reading time yet.
	 *
	 * @return The caption's id, or 0 when it was refused (captions off, empty text, sound captions off, or
	 *         the queue at its limit).
	 */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue")
	int32 QueueCaption(const FCaptionCueRequest& Request);

	/** Queue a spoken line without building a request struct first. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue", meta = (AdvancedDisplay = "2"))
	int32 QueueSpeech(const FText& Text, FName Speaker, float Duration = 0.0f, int32 Priority = 0);

	/**
	 * Queue a caption for something the player heard but nobody said: a door, a reload, a distant scream.
	 * Give it a world position and an off-screen source gets an arrow.
	 */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue", meta = (AdvancedDisplay = "2"))
	int32 QueueSoundCaption(const FText& Text, FVector WorldLocation, bool bHasWorldLocation = true, float Duration = 0.0f, int32 Priority = 0);

	/**
	 * Put a caption on screen immediately.
	 *
	 * If every line is taken, the visible line with the lowest priority is pushed aside — but only if its
	 * priority is genuinely lower, and it goes back to the front of the queue rather than being thrown
	 * away. A warning that has to be read now can interrupt small talk; small talk cannot interrupt a warning.
	 *
	 * @return The caption's id, or 0 when it was refused.
	 */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue")
	int32 ShowCaption(const FCaptionCueRequest& Request);

	/** Everything on screen and everything waiting, gone. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue")
	void ClearCaptions();

	/** Remove one caption by the id Queue Caption returned, whether it is visible or still waiting. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue")
	bool RemoveCaption(int32 CaptionId);

	// ----------------------------------------------------------------------------------------------------
	// Speakers
	// ----------------------------------------------------------------------------------------------------

	/**
	 * Teach this world a speaker: the id gameplay code uses, the name the player reads, and the colour it
	 * is written in. Registrations made here win over the project-level list in the settings, which is what
	 * lets one level rename "Guard" to "Captain Elsa" without touching the project.
	 */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Speakers")
	void RegisterSpeaker(FName SpeakerId, const FText& DisplayName, FLinearColor Color);

	/** Forget a runtime speaker registration; the project-level entry, if there is one, applies again. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Speakers")
	void UnregisterSpeaker(FName SpeakerId);

	/** The name and colour a speaker id resolves to right now, runtime registrations included. */
	UFUNCTION(BlueprintPure, Category = "CaptionCue|Speakers")
	FCaptionCueSpeaker ResolveSpeaker(FName SpeakerId) const;

	// ----------------------------------------------------------------------------------------------------
	// Drawing
	// ----------------------------------------------------------------------------------------------------

	/**
	 * Draw every visible caption onto the canvas. This is the whole renderer.
	 *
	 * Call it from your own HUD class: Captions->DrawCaptions(Canvas); — one line, in DrawHUD.
	 * Or use ACaptionCueHUD / UCaptionCueHUDComponent, which do exactly that for you.
	 */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue")
	void DrawCaptions(UCanvas* Canvas);

	// ----------------------------------------------------------------------------------------------------
	// State
	// ----------------------------------------------------------------------------------------------------

	/** What the caption layer is doing right now. */
	UFUNCTION(BlueprintPure, Category = "CaptionCue")
	const FCaptionCueStats& GetStats() const { return Stats; }

	/** Captions on screen. */
	UFUNCTION(BlueprintPure, Category = "CaptionCue")
	int32 GetVisibleCount() const { return VisibleCaptions.Num(); }

	/** Captions waiting for a free line. */
	UFUNCTION(BlueprintPure, Category = "CaptionCue")
	int32 GetPendingCount() const { return PendingCaptions.Num(); }

	/** The text of a visible caption, top line first. Empty when the index is out of range. */
	UFUNCTION(BlueprintPure, Category = "CaptionCue")
	FText GetVisibleCaptionText(int32 Index) const;

	/** Show or hide the on-screen statistics box. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Debug")
	void SetShowStats(bool bInShowStats) { bShowStats = bInShowStats; }

	/** True while the statistics box is being drawn. */
	UFUNCTION(BlueprintPure, Category = "CaptionCue|Debug")
	bool IsShowingStats() const { return bShowStats; }

	/**
	 * True while the engine's subtitle stream is routed into CaptionCue — that is, while SoundWave and
	 * DialogueWave subtitles land here instead of on the engine's own unstyled canvas line.
	 */
	UFUNCTION(BlueprintPure, Category = "CaptionCue")
	bool IsEngineBridgeAttached() const;

	/** Style registered under this name, or null. */
	UFUNCTION(BlueprintPure, Category = "CaptionCue|Styles")
	UCaptionCueStyle* GetStyleByName(FName StyleName) const;

	/** The style a caption of this kind is drawn with, built-in fallbacks included. Never null. */
	UFUNCTION(BlueprintPure, Category = "CaptionCue|Styles")
	UCaptionCueStyle* GetStyleForKind(ECaptionCueKind Kind) const;

	/** Every style name this world knows about. */
	UFUNCTION(BlueprintPure, Category = "CaptionCue|Styles")
	TArray<FName> GetStyleNames() const;

	/** Re-read the style list from the project settings. Useful after editing a style asset. */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Styles")
	void RefreshStyles();

	/**
	 * Play a short scripted scene: two speakers, a bracketed sound caption with a position behind the
	 * camera, and two lines fired in the same frame so the queue can be seen doing its job.
	 * The same thing CaptionCue.Test does, and the tool the store images are made with.
	 */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue|Debug")
	void PlayTestScene();

	/**
	 * Called by the engine subtitle bridge. Not part of the everyday API: use Queue Caption.
	 * Bridged lines carry text and nothing else — see FCaptionCueSubtitleBridge for why.
	 */
	void QueueBridgedSubtitle(const FText& SubtitleText);

private:
	/** One wrapped line of one caption, already measured. */
	struct FCaptionLine
	{
		FString Text;
		float Width = 0.0f;

		/** Characters at the front of this line that belong to the speaker label and draw in its colour. */
		int32 SpeakerPrefixLength = 0;
	};

	/** One caption, wrapped and measured, ready to be placed. */
	struct FLaidOutCaption
	{
		/** The wrapped lines, top first. */
		TArray<FCaptionLine> Lines;

		/** The style that produced this layout. Never null in a laid-out caption. */
		const UCaptionCueStyle* Style = nullptr;

		/** The font the lines were measured with, so drawing cannot disagree with wrapping. */
		FSlateFontInfo FontInfo;

		/** Colour of the line text, fade already applied. */
		FLinearColor TextColor = FLinearColor::White;

		/** Colour of the speaker label, fade already applied. */
		FLinearColor SpeakerColor = FLinearColor::White;

		/** Colour of the background box, player opacity and fade already applied. */
		FLinearColor BackgroundColor = FLinearColor::Transparent;

		/** Fade value on its own, for the direction arrow. */
		float Opacity = 1.0f;

		/** Distance from one baseline to the next. */
		float LineHeight = 0.0f;

		/** Widest wrapped line. */
		float TextWidth = 0.0f;

		/** Air between the text and the edge of its box. */
		FVector2D Padding = FVector2D::ZeroVector;

		/** Text width plus padding on both sides. */
		float BlockWidth = 0.0f;

		/** All lines plus padding above and below. */
		float BlockHeight = 0.0f;

		/** Player text scale times any style scaling, for the arrow and the outline. */
		float FontScale = 1.0f;

		/** Index into VisibleCaptions. */
		int32 EntryIndex = INDEX_NONE;

		/** True when this caption asked for a box and the player left boxes switched on. */
		bool bDrawBackground = false;
	};

	/** Pull the style list out of the project settings and fill in a fallback for every kind. */
	void ResolveStyles();

	/** Build a transient style so captions draw before anyone has authored an asset. */
	UCaptionCueStyle* CreateBuiltinStyle(ECaptionCueKind Kind);

	/** Turn a request into an entry, reading time and all. Returns false when the request is refused. */
	bool BuildEntry(const FCaptionCueRequest& Request, bool bFromBridge, FCaptionCueEntry& OutEntry);

	/** Move waiting captions onto the screen while there is room, best priority first. */
	void PromotePending();

	/** Age the visible captions and retire the ones whose reading time is up. */
	void AgeCaptions(float DeltaTime);

	/** The draw pass itself, shared by the HUD path and the editor preview path. */
	void DrawPass(UCanvas* Canvas);

	/** Wrap and measure one entry. Returns false when there is nothing to draw. */
	bool LayOutCaption(UCanvas* Canvas, int32 EntryIndex, float MaxLineWidth, FLaidOutCaption& Out) const;

	/** Break a string into lines no wider than MaxWidth, respecting the newlines already in it. */
	void WrapLine(const FString& Source, int32 SpeakerPrefixLength, const FSlateFontInfo& FontInfo, float MaxWidth, TArray<FCaptionLine>& OutLines) const;

	/** Draw one laid-out caption with its top-left corner at Origin. */
	void DrawCaptionBlock(UCanvas* Canvas, const FLaidOutCaption& Laid, const FVector2D& Origin) const;

	/** Edge arrow for an off-screen sound. Returns true when one was drawn. */
	bool DrawDirectionArrow(UCanvas* Canvas, const FCaptionCueEntry& Entry, const FLaidOutCaption& Laid, const FBox2D& SafeArea) const;

	/** The statistics box. */
	void DrawStatsBox(UCanvas* Canvas) const;

	/** Best available camera position and orientation for this canvas. */
	bool ResolveView(UCanvas* Canvas, FVector& OutLocation, FVector& OutForward) const;

	/** Attach to, or detach from, the engine subtitle stream according to the current settings. */
	void UpdateEngineBridgeRegistration();

#if WITH_EDITOR
	/**
	 * Editor-viewport draw callback. Compiled WITH_EDITOR only, and second in line behind the HUD path.
	 */
	void OnEditorPreviewDraw(UCanvas* Canvas, APlayerController* PlayerController);
#endif

	/** Captions on screen, oldest first. */
	TArray<FCaptionCueEntry> VisibleCaptions;

	/** Captions waiting for a free line. */
	TArray<FCaptionCueEntry> PendingCaptions;

	/** Speakers registered at runtime; these win over the project-level list. */
	TMap<FName, FCaptionCueSpeaker> RuntimeSpeakers;

	/** Resolved styles, project assets first, built-in fallbacks after. */
	UPROPERTY()
	TArray<TObjectPtr<UCaptionCueStyle>> Styles;

	/** Style name to index into Styles. */
	TMap<FName, int32> StyleByName;

	/** Index into Styles for each ECaptionCueKind. Always valid once ResolveStyles has run. */
	int32 StyleForKind[4] = { INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE };

	/**
	 * What styles with no font of their own draw with: the project's subtitle font, or the engine's Roboto.
	 * Held as a property so the garbage collector cannot take it out from under the draw pass.
	 */
	UPROPERTY()
	TObjectPtr<UFont> FallbackFont;

	/** Ids handed out to captions. Never reused inside one world. */
	uint32 NextCaptionId = 1;

	/** Order captions were accepted in, so ties in the queue break in favour of the older line. */
	uint32 NextSequence = 1;

	/** On-screen statistics box. */
	bool bShowStats = false;

	/** True while this subsystem is registered with the process-wide engine subtitle bridge. */
	bool bRegisteredWithBridge = false;

	/** Frame the last real HUD pass drew on, so the editor preview never draws on top of the game's pass. */
	uint64 LastGameDrawFrame = 0;

	/** Last camera the draw pass saw. The test scene puts a sound behind it, so it needs to know where it is. */
	FVector CachedViewLocation = FVector::ZeroVector;
	FVector CachedViewForward = FVector::ForwardVector;
	bool bHasCachedView = false;

	/** Handle for the editor-only preview draw. */
	FDelegateHandle EditorPreviewHandle;

	/** Published through GetStats(). */
	FCaptionCueStats Stats;
};

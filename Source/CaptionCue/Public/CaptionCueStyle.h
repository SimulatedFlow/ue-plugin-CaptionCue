// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CaptionCueTypes.h"
#include "Engine/DataAsset.h"
#include "Fonts/SlateFontInfo.h"
#include "CaptionCueStyle.generated.h"

/** How the speaker's name is written in front of the line. */
UENUM(BlueprintType)
enum class ECaptionCueSpeakerFormat : uint8
{
	/** Guard: Halt, who goes there? — the convention most games use. */
	Colon UMETA(DisplayName = "Name: Text"),

	/** [Guard] Halt, who goes there? — reads better when the name is coloured. */
	Brackets UMETA(DisplayName = "[Name] Text"),

	/** Guard - Halt, who goes there? */
	Dash UMETA(DisplayName = "Name - Text"),

	/** Name on its own line above the text. Costs a line, gains a lot at small text sizes. */
	OwnLine UMETA(DisplayName = "Name on its own line"),

	/** Never write the name, even when one is known. */
	None UMETA(DisplayName = "No name")
};

/**
 * What one kind of caption looks like, as an asset rather than as code.
 *
 * Four styles cover a whole game — Speech, Sound Effect, Music, System — and each is a data asset, so a
 * designer can change how every line of dialogue in the game reads without a recompile and without opening
 * a Blueprint.
 *
 * A style holds the *look*. What the player is allowed to change about it — text scale, background box on
 * or off, position, line width — lives in UCaptionCueSettings, because those are options that belong in the
 * customer's own menu, not values a designer locks down. Where the two meet, the player wins: a style that
 * asks for a background box does not get one if the player has switched boxes off.
 *
 * No font ships with CaptionCue, for licensing reasons. Leave Font empty and the engine's own subtitle font
 * is used, which is present in every install.
 */
UCLASS(BlueprintType, meta = (DisplayName = "CaptionCue Style"))
class CAPTIONCUE_API UCaptionCueStyle : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UCaptionCueStyle();

	// UPrimaryDataAsset interface
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// ----------------------------------------------------------------------------------------------------
	// Identity
	// ----------------------------------------------------------------------------------------------------

	/**
	 * The name a caption can ask for by hand (Style Override). Leave it at None and the asset's own name
	 * is used. Names, not asset references, so gameplay code never hard-references a UI asset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName StyleName;

	/** Which kind of caption this style is picked for when nothing asked for it by name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	ECaptionCueKind Kind = ECaptionCueKind::Speech;

	// ----------------------------------------------------------------------------------------------------
	// Text
	// ----------------------------------------------------------------------------------------------------

	/** Font. Empty means the engine's subtitle font — CaptionCue ships no font of its own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateFontInfo Font;

	/**
	 * Point size before the player's text scale is applied. This is the "100 %" size, so pick something a
	 * player with good eyes finds comfortable and let the scale option do the rest.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text", meta = (ClampMin = "4.0", UIMax = "96.0"))
	float FontSize = 26.0f;

	/** Extra pixels between wrapped lines of the same caption, on top of the font's own height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text", meta = (UIMin = "-10.0", UIMax = "40.0"))
	float LineSpacing = 2.0f;

	/** Left, centre or right inside the caption block. Centre is the usual choice for subtitles. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	ECaptionCueJustify Justification = ECaptionCueJustify::Center;

	/** Colour of the spoken or described text itself. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FLinearColor TextColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	/**
	 * Outline thickness in pixels, scaled with the text. An outline is what keeps a caption readable over a
	 * bright sky when the player has switched the background box off — which many do.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text", meta = (ClampMin = "0", ClampMax = "12"))
	int32 OutlineSize = 2;

	/** Outline colour. Its alpha fades with the line. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FLinearColor OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);

	/** Drop a shadow as well as, or instead of, the outline. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	bool bDrawShadow = false;

	/** Shadow offset in pixels, scaled with the text. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text", meta = (EditCondition = "bDrawShadow"))
	FVector2D ShadowOffset = FVector2D(2.0f, 2.0f);

	/** Shadow colour. Its alpha fades with the line. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text", meta = (EditCondition = "bDrawShadow"))
	FLinearColor ShadowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.65f);

	// ----------------------------------------------------------------------------------------------------
	// Speaker
	// ----------------------------------------------------------------------------------------------------

	/** How the name is written in front of the line. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speaker")
	ECaptionCueSpeakerFormat SpeakerFormat = ECaptionCueSpeakerFormat::Colon;

	/** Colour for a speaker nobody registered a colour for. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speaker")
	FLinearColor DefaultSpeakerColor = FLinearColor(0.62f, 0.82f, 1.0f, 1.0f);

	/**
	 * Tint the whole line in the speaker's colour instead of only the name.
	 * Reads well with two speakers, badly with six.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speaker")
	bool bTintLineWithSpeakerColor = false;

	// ----------------------------------------------------------------------------------------------------
	// Non-speech
	// ----------------------------------------------------------------------------------------------------

	/**
	 * Written in front of a non-speech caption. Square brackets are the convention broadcast captioning
	 * settled on decades ago, and players read them without being told what they mean.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Non-Speech")
	FString Prefix;

	/** Written after a non-speech caption. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Non-Speech")
	FString Suffix;

	// ----------------------------------------------------------------------------------------------------
	// Background box
	// ----------------------------------------------------------------------------------------------------

	/**
	 * Ask for a box behind the text. The player's own Background Box option can still take it away — where
	 * a designer preference and an accessibility option disagree, the accessibility option wins.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Background")
	bool bBackgroundBox = true;

	/** Box colour. Its alpha is multiplied by the player's Background Opacity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Background")
	FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);

	/** Pixels of air between the text and the edge of its box, scaled with the text. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Background")
	FVector2D BackgroundPadding = FVector2D(14.0f, 6.0f);

	/**
	 * One box around the whole caption instead of one box per wrapped line.
	 * Per line looks tighter on ragged text; one block looks calmer on a long line.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Background")
	bool bBoxPerLine = false;

	// ----------------------------------------------------------------------------------------------------
	// Timing
	// ----------------------------------------------------------------------------------------------------

	/** Seconds the line takes to fade in. Zero snaps, which is what most subtitle systems do. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0", UIMax = "1.0"))
	float FadeInSeconds = 0.08f;

	/** Seconds the line takes to fade out at the end of its display time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0", UIMax = "2.0"))
	float FadeOutSeconds = 0.25f;

	// ----------------------------------------------------------------------------------------------------
	// Direction indicator
	// ----------------------------------------------------------------------------------------------------

	/**
	 * Let captions in this style draw an edge arrow when their sound is off screen. Speech rarely wants
	 * one; a door creaking behind the player always does.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Direction")
	bool bAllowDirectionIndicator = true;

	/** Arrow colour. Its alpha fades with the line. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Direction")
	FLinearColor DirectionIndicatorColor = FLinearColor(1.0f, 0.85f, 0.35f, 0.95f);

	/** Arrow length in pixels at 100 % text scale. Scales with the player's text scale along with everything else. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Direction", meta = (ClampMin = "4.0", UIMax = "120.0"))
	float DirectionIndicatorSize = 26.0f;

	// ----------------------------------------------------------------------------------------------------
	// Evaluation
	// ----------------------------------------------------------------------------------------------------

	/**
	 * The font to draw with at the given final size, outline folded in.
	 *
	 * @param FallbackFont What to use when this style has no font of its own. The subsystem resolves it once
	 *                     to the project's subtitle font (or the engine's Roboto) and keeps a reference, so
	 *                     the draw path never loads and the garbage collector never takes it away.
	 */
	FSlateFontInfo GetFontInfo(float SizeScale, const FLinearColor& InOutlineColor, const UObject* FallbackFont) const;

	/** Opacity of the whole line at the given age, fade in and fade out included. */
	float EvaluateOpacity(float Age, float Duration) const;

	/**
	 * The speaker label that goes in front of the line: "Guard: ", "[Guard] ", "Guard\n"…
	 * Returned on its own rather than glued to the text, because the label is drawn in the speaker's colour
	 * and the line is not.
	 */
	FText FormatSpeakerPrefix(const FText& InSpeakerName) const;

	/** Prefix and line together, for callers that do not care about colouring the name separately. */
	FText FormatWithSpeaker(const FText& InText, const FText& InSpeakerName) const;

	/** Wrap a non-speech line in this style's prefix and suffix: "door creaks" becomes "[door creaks]". */
	FText FormatNonSpeech(const FText& InText) const;
};

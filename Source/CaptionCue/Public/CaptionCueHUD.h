// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CaptionCueHUD.generated.h"

class UCaptionCueHUDComponent;

/**
 * The ready-made HUD class: set it as your game mode's HUD Class and captions draw, with nothing else to
 * wire up. That is the five-minute install.
 *
 * This is the main draw path and it is deliberately AHUD-based. Captions are an accessibility feature, so
 * they have to exist in the build the player actually receives: DrawHUD runs in a cooked Shipping build,
 * and the editor-viewport preview in the subsystem does not.
 *
 * Already have your own HUD class? Do not replace it — add UCaptionCueHUDComponent to it and call
 * DrawCaptions(Canvas) from your own DrawHUD instead. Both paths end in the same subsystem call.
 */
UCLASS(meta = (DisplayName = "CaptionCue HUD"))
class CAPTIONCUE_API ACaptionCueHUD : public AHUD
{
	GENERATED_BODY()

public:
	ACaptionCueHUD();

	// AHUD interface
	virtual void DrawHUD() override;

	/** Draw captions when this HUD renders. Off makes the class behave like a plain AHUD. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptionCue")
	bool bDrawCaptions = true;

protected:
	/** Present so that a Blueprint child of this class can reach the same call the C++ path uses. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CaptionCue")
	TObjectPtr<UCaptionCueHUDComponent> CaptionComponent;
};

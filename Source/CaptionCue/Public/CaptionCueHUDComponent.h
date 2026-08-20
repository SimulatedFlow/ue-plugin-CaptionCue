// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CaptionCueHUDComponent.generated.h"

class UCanvas;

/**
 * The way into an existing HUD class.
 *
 * Most projects that need captions already have a HUD, and replacing it with someone else's is not a
 * reasonable thing for a plugin to ask. Add this component to your own AHUD subclass instead and call
 * Draw Captions (Canvas) from your DrawHUD — one node in Blueprint, one line in C++ — and you have the
 * same draw path ACaptionCueHUD uses, with your HUD still yours.
 */
UCLASS(ClassGroup = (CaptionCue), meta = (BlueprintSpawnableComponent, DisplayName = "CaptionCue HUD Component"))
class CAPTIONCUE_API UCaptionCueHUDComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCaptionCueHUDComponent();

	/**
	 * Draw this world's captions onto the canvas. Call it from AHUD::DrawHUD, or from the Receive Draw HUD
	 * event in a Blueprint HUD.
	 */
	UFUNCTION(BlueprintCallable, Category = "CaptionCue")
	void DrawCaptions(UCanvas* Canvas);
};

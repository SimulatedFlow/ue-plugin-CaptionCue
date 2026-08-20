// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "CaptionCueHUDComponent.h"

#include "CaptionCueSubsystem.h"
#include "Engine/World.h"

UCaptionCueHUDComponent::UCaptionCueHUDComponent()
{
	// Nothing to tick: the subsystem owns the reading clock, this component only forwards a canvas.
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = false;
}

void UCaptionCueHUDComponent::DrawCaptions(UCanvas* Canvas)
{
	if (!Canvas)
	{
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		if (UCaptionCueSubsystem* Captions = World->GetSubsystem<UCaptionCueSubsystem>())
		{
			Captions->DrawCaptions(Canvas);
		}
	}
}

// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "CaptionCueHUD.h"

#include "CaptionCueHUDComponent.h"

ACaptionCueHUD::ACaptionCueHUD()
{
	CaptionComponent = CreateDefaultSubobject<UCaptionCueHUDComponent>(TEXT("CaptionCueComponent"));
}

void ACaptionCueHUD::DrawHUD()
{
	Super::DrawHUD();

	if (bDrawCaptions && CaptionComponent)
	{
		// Last, so the captions sit on top of whatever a Blueprint child of this class drew in Receive Draw
		// HUD. A caption that ends up behind a health bar is a caption nobody can read.
		CaptionComponent->DrawCaptions(Canvas);
	}
}

// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * The one runtime module.
 *
 * It owns nothing but the process-wide engine subtitle bridge, which has to be released before the module
 * unloads: FSubtitleManager outlives this module, and a delegate still pointing here after the DLL has gone
 * is a crash in whatever the editor loads next.
 */
class FCaptionCueModule : public IModuleInterface
{
public:
	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

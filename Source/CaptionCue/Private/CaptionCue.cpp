// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "CaptionCue.h"

#include "CaptionCueLog.h"
#include "CaptionCueSubtitleBridge.h"

DEFINE_LOG_CATEGORY(LogCaptionCue);

#define LOCTEXT_NAMESPACE "FCaptionCueModule"

void FCaptionCueModule::StartupModule()
{
	// Nothing is hooked here on purpose. The engine subtitle bridge is attached by the first world's caption
	// subsystem, because FSubtitleManager wants a running engine and this module loads at PreDefault, before
	// there is one.
	UE_LOG(LogCaptionCue, Log, TEXT("CaptionCue started."));
}

void FCaptionCueModule::ShutdownModule()
{
	// FSubtitleManager is a process-wide singleton that outlives this module. Leaving a raw delegate bound
	// into code that is about to be unloaded is the one way this plugin could take an editor down with it.
	FCaptionCueSubtitleBridge::Get().Shutdown();

	UE_LOG(LogCaptionCue, Log, TEXT("CaptionCue shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCaptionCueModule, CaptionCue)

// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "CaptionCueSubtitleBridge.h"

#include "CaptionCueLog.h"
#include "CaptionCueSettings.h"
#include "CaptionCueSubsystem.h"
#include "Engine/Engine.h"
#include "SubtitleManager.h"

FCaptionCueSubtitleBridge& FCaptionCueSubtitleBridge::Get()
{
	static FCaptionCueSubtitleBridge Bridge;
	return Bridge;
}

void FCaptionCueSubtitleBridge::RegisterSubsystem(UCaptionCueSubsystem* Subsystem)
{
	if (!Subsystem)
	{
		return;
	}

	Listeners.RemoveAll([](const TWeakObjectPtr<UCaptionCueSubsystem>& Weak) { return !Weak.IsValid(); });
	Listeners.AddUnique(Subsystem);

	EnsureHooked();
}

void FCaptionCueSubtitleBridge::UnregisterSubsystem(UCaptionCueSubsystem* Subsystem)
{
	Listeners.RemoveAll([Subsystem](const TWeakObjectPtr<UCaptionCueSubsystem>& Weak)
	{
		return !Weak.IsValid() || Weak.Get() == Subsystem;
	});

	if (Listeners.Num() == 0)
	{
		// Nobody left to hear it, so give subtitle drawing back to the engine rather than swallowing it.
		// A plugin that stays hooked after its last world has gone is a plugin that breaks the next project
		// the editor loads.
		ReleaseHook();
	}
}

void FCaptionCueSubtitleBridge::Shutdown()
{
	ReleaseHook();
	Listeners.Reset();
	LastForwardedText.Reset();
}

void FCaptionCueSubtitleBridge::EnsureHooked()
{
	if (SubtitleTextHandle.IsValid())
	{
		return;
	}

	FSubtitleManager* Manager = FSubtitleManager::GetSubtitleManager();
	if (!Manager)
	{
		return;
	}

	SubtitleTextHandle = Manager->OnSetSubtitleText().AddRaw(this, &FCaptionCueSubtitleBridge::HandleSubtitleText);
	LastForwardedText.Reset();

	UE_LOG(LogCaptionCue, Log,
		TEXT("Engine subtitle bridge attached: FSubtitleManager::OnSetSubtitleText is now routed into CaptionCue, ")
		TEXT("and the engine's own canvas subtitle line is suppressed."));
}

void FCaptionCueSubtitleBridge::ReleaseHook()
{
	if (!SubtitleTextHandle.IsValid())
	{
		return;
	}

	if (FSubtitleManager* Manager = FSubtitleManager::GetSubtitleManager())
	{
		Manager->OnSetSubtitleText().Remove(SubtitleTextHandle);
	}

	SubtitleTextHandle.Reset();
	LastForwardedText.Reset();

	UE_LOG(LogCaptionCue, Log, TEXT("Engine subtitle bridge detached; the engine draws its own subtitles again."));
}

void FCaptionCueSubtitleBridge::HandleSubtitleText(const FText& SubtitleText)
{
	const UCaptionCueSettings& Settings = UCaptionCueSettings::Get();

	// The engine checks these two flags before it draws a subtitle on the canvas, and skips that check on
	// the delegate path — so the bridge has to make the check itself, or a project that has switched
	// subtitles off would find them switched back on by installing this plugin.
	if (GEngine && (GEngine->bSubtitlesForcedOff || !GEngine->bSubtitlesEnabled))
	{
		LastForwardedText.Reset();
		return;
	}

	const FString Incoming = SubtitleText.ToString();

	if (Incoming.IsEmpty())
	{
		// The line is over as far as the engine is concerned. The caption is *not* cut short here: it keeps
		// the reading time it was given. Forgetting the text is only what lets the same line be spoken again
		// later and be queued again.
		LastForwardedText.Reset();
		return;
	}

	// The engine re-broadcasts the active subtitle on every game viewport draw, so without this the queue
	// would receive the same line sixty times a second.
	if (Incoming.Equals(LastForwardedText, ESearchCase::CaseSensitive))
	{
		return;
	}

	LastForwardedText = Incoming;

	bool bAnyLive = false;

	for (int32 Index = Listeners.Num() - 1; Index >= 0; --Index)
	{
		UCaptionCueSubsystem* Subsystem = Listeners[Index].Get();
		if (!Subsystem)
		{
			Listeners.RemoveAtSwap(Index, EAllowShrinking::No);
			continue;
		}

		bAnyLive = true;
		Subsystem->QueueBridgedSubtitle(SubtitleText);
	}

	if (!bAnyLive)
	{
		ReleaseHook();
	}
}

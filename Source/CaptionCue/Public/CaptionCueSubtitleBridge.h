// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/IDelegateInstance.h"
#include "UObject/WeakObjectPtr.h"

class UCaptionCueSubsystem;

/**
 * The connection to the engine's own subtitle stream.
 *
 * This is the piece that makes CaptionCue worth installing in a project that already has voice acting.
 * Every USoundWave carries a Subtitles array and every UDialogueWave carries spoken lines; the engine
 * collects them in FSubtitleManager and draws them as a single unstyled line, with no speaker, no box, no
 * size option and nothing at all for non-speech sound. That pipe is fine. It is the far end that is missing.
 *
 * ------------------------------------------------------------------------------------------------------
 * Which hook this uses, precisely, so nobody has to guess
 * ------------------------------------------------------------------------------------------------------
 *
 * FSubtitleManager exposes OnSetSubtitleText(), a multicast delegate the engine describes in its own header
 * as a way for a display to "hijack subtitle text ... and get around the Canvas display". Binding to it has
 * two documented effects inside FSubtitleManager::DisplaySubtitles and ::DisplaySubtitle:
 *
 *   1. The engine stops drawing subtitles on the canvas itself. There is no double display and nothing to
 *      switch off in the project.
 *   2. The currently active, highest-priority subtitle text is broadcast to us instead, once per game
 *      viewport draw, as an FText. An empty FText means "nothing is speaking right now".
 *
 * That is a real, supported hook, not a patch and not a copied engine file. What it carries is exactly one
 * thing: the text. What it does not carry, and what CaptionCue therefore fills in on its own:
 *
 *   - No duration. The reading time is worked out from the length of the line (see Min Seconds Per
 *     Character), which is what a subtitle wants anyway.
 *   - No speaker. Engine subtitles have never had one. Bridged lines are queued without a name; use
 *     UCaptionCueAudioComponent, or the direct API, when a name matters.
 *   - No world position, so bridged lines get no direction arrow.
 *   - No world. The delegate is process-wide and says nothing about which world is speaking, so in a
 *     multi-world session (two PIE clients) every registered world hears the same line. Only one game
 *     viewport draws subtitles per frame, so in practice this is a Play-as-Client curiosity rather than a
 *     problem, but it is a limit and it is written down here rather than discovered later.
 *
 * Because the broadcast repeats every frame for as long as the line is active, the bridge only forwards a
 * line when the text has actually changed. An empty broadcast ends the run without cutting the caption
 * short: a caption that has been on screen for half a second still gets its full reading time, which is
 * the entire point of the minimum display time.
 *
 * The richer path, for sounds you own, is UCaptionCueAudioComponent: that one intercepts the cues
 * themselves, with their timings and the sound's own duration, and knows where in the world it is playing.
 */
class CAPTIONCUE_API FCaptionCueSubtitleBridge
{
public:
	/** The one bridge. There is one FSubtitleManager per process, so there is one bridge per process. */
	static FCaptionCueSubtitleBridge& Get();

	/**
	 * A world's caption subsystem starts listening. The first registration binds the engine delegate, which
	 * is also the moment the engine stops drawing subtitles on the canvas itself.
	 */
	void RegisterSubsystem(UCaptionCueSubsystem* Subsystem);

	/** A world's caption subsystem stops listening. The last one out releases the engine delegate. */
	void UnregisterSubsystem(UCaptionCueSubsystem* Subsystem);

	/** True while the engine delegate is bound and the engine is no longer drawing subtitles itself. */
	bool IsHooked() const { return SubtitleTextHandle.IsValid(); }

	/** Number of worlds currently listening. */
	int32 GetListenerCount() const { return Listeners.Num(); }

	/** Drop the engine delegate and forget every listener. Called when the module shuts down. */
	void Shutdown();

private:
	/** Bind the engine delegate if it is not bound already. */
	void EnsureHooked();

	/** Release the engine delegate and hand subtitle drawing back to the engine. */
	void ReleaseHook();

	/** The engine's broadcast. Runs on the game thread, once per game viewport draw. */
	void HandleSubtitleText(const FText& SubtitleText);

	/** Worlds listening for bridged lines. Weak, because a world can go away without telling anyone. */
	TArray<TWeakObjectPtr<UCaptionCueSubsystem>> Listeners;

	/** Handle on FSubtitleManager::OnSetSubtitleText. */
	FDelegateHandle SubtitleTextHandle;

	/** The last line forwarded, so a broadcast that repeats every frame is queued exactly once. */
	FString LastForwardedText;
};

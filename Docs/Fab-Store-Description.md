# CaptionCue — Subtitles & Closed Captions

**The subtitle layer Unreal never finished.**

Your dialogue system decides what is said. Nothing in the engine decides how it *reads*. `FSubtitleManager`
puts one unstyled line at the bottom of the screen: no speaker, no background box, no size option, nothing
at all for the sounds a player cannot hear. That is where every project starts writing its own subtitle
widget, and where most of them ship the same four bugs.

CaptionCue is the missing half. Drop it in, set one HUD class, and you have captions you can actually
ship — with the options a console checklist expects to find in your options menu.

---

## What you get

**Speaker names, in colour.** Register an id once with a display name and a colour, and every line that
character speaks is labelled and coloured the same way. The name is drawn separately from the line, which is
what makes a three-way conversation followable. Characters nobody registered still get a stable colour
derived from their name.

**Closed captions, not just subtitles.** Four kinds — Speech, Sound Effect, Music, System — each with its
own style. Non-speech captions are bracketed the way broadcast captioning has done it for decades, and the
player can switch them off separately from dialogue, because someone who can hear the door does not want to
read about it.

**Direction arrows.** A sound caption that carries a world position draws a small arrow at the edge of the
screen when its source is off screen, and nothing when it is visible. "[door creaks]" is not much use
without knowing which door. This is the part that makes them *closed captions*.

**A queue, not a variable.** The commonest bug in a hand-rolled subtitle widget is one "current line" that
the next line overwrites — two events in the same frame and the player reads neither. Here lines queue.
Up to three share the screen and the rest wait. A line only loses its place to something of strictly higher
priority, and even then it goes back into the queue rather than into the bin.

**A reading clock.** Captions timed to the audio vanish while the player is still reading. Every line gets
at least a minimum display time and at least a per-character reading time, however short the sound was. A
half-second grunt subtitled with a full sentence stays up long enough to read the sentence.

**It picks up the subtitles you already have.** CaptionCue attaches to the engine's own subtitle stream
through `FSubtitleManager::OnSetSubtitleText` — a supported engine hook, not a patch. Subtitles already
carried by your `SoundWave` and `DialogueWave` assets stop being drawn as one unstyled line and arrive in
CaptionCue's queue instead, **with no change to your project**. For sounds you own, `CaptionCue Audio
Component` goes further: it intercepts the cues with their timings, and adds the speaker and the world
position the engine's stream cannot carry. Exactly what each path does and does not carry is written out in
the documentation — nothing is claimed that does not run.

**The whole options page, as Blueprint nodes.** Text scale (0.75 / 1.0 / 1.5 / 2.0 or anything between),
background box and opacity, speaker names on/off, sound captions on/off, line width, safe-area margin,
bottom or top, lines on screen, reading speed, direction arrows. Every one takes effect on the next frame —
no restart, no re-created widget. Build your accessibility page without opening C++.

**Styles are assets.** Font, size, colour, outline, shadow, box, padding, justification, speaker format,
brackets, fades and arrow colour — one data asset per kind. Change how every line of dialogue in the game
reads without a recompile. Four styles ship with the demo content, and four built-in fallbacks mean captions
draw before you have authored anything at all.

**Built for the build the player gets.** Drawn on `UCanvas` from `AHUD`: no `UUserWidget` per line, no Slate
layout pass per caption, no UMG dependency, no editor module. Verified compiling for Editor Development,
Game Development and Game **Shipping**. Captions also draw in an editor viewport with nothing playing, which
is how the screenshots on this page were made.

**Localised properly.** `FText` everywhere on the display path — never `FString`. Speaker names have their
own translation keys. Even the punctuation between a name and a line is a format pattern, so a French
translation can move the colon. String Tables work exactly as you would expect. Wrapping is measured on the
translated glyphs, not the English ones.

---

## Why this exists

Console certification asks for subtitle options — size, background, speaker identification. The European
Accessibility Act has applied since June 2025. If you are shipping on console, you need this; the only
question is whether you build it yourself.

Fab has five well-reviewed dialogue systems and no established subtitle layer. Those two things are not
competitors. A dialogue system decides *what* is said. CaptionCue decides how it reads on the screen —
and every one of those buyers still needs the second half.

---

## Install

1. Copy into `Plugins/`, restart, enable.
2. Set your Game Mode's HUD Class to **CaptionCue HUD** — or add **CaptionCue HUD Component** to the HUD you
   already have and call `Draw Captions (Canvas)` from `DrawHUD`.
3. Call **Show Subtitle**. Or type `CaptionCue.Test`.

Nothing to author first. No widget to build. No font to import.

---

## Honest limits — please read before buying

* **This is not a dialogue system.** No dialogue trees, no authoring interface, no branching, no voice-over
  management. CaptionCue displays what another system triggers, and is designed as a companion to Narrative
  Tales, SUDS and the rest — not a replacement.
* **No automatic transcription.** No speech-to-text. Someone has to write the line.
* **No SRT file playback** for linear video. If that is your problem, "Subtitles File Reader" is the tool.
* **No translation.** Localisation support, yes. Translating the words, no — that is not something a plugin
  can do.
* **No guarantee of certification.** The documentation maps common checklist requirements to the settings
  that address them. Whether a submission passes is decided by the platform holder, who is also looking at
  your defaults, your menu, your fonts and your translations.
* **No font is shipped**, for licensing reasons. The engine's own subtitle font is used, which every install
  has.
* **The engine subtitle bridge carries text only.** Duration, speaker and world position are not in the
  engine's delegate — use `CaptionCue Audio Component` or the direct API for those. Bridged lines also do
  not appear in an editor viewport without play mode, because the engine only broadcasts them while a game
  viewport is drawing.
* **Captions draw on `UCanvas`, not inside UMG.** Deliberate — it is what makes them cheap and identical in
  the editor and in a cooked build. If you need captions inside a UMG widget hierarchy, this is the wrong
  tool.

---

**Engine:** 5.8 · **Platforms:** Win64, Mac, Linux · **Modules:** one runtime C++ module, full source ·
**Dependencies:** none beyond the engine · **Network Replicated:** No (captions are a client-side display)

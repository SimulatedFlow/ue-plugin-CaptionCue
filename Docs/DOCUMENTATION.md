# CaptionCue — Subtitles & Closed Captions

**Unreal Engine 5.8 · one runtime C++ module · Win64 / Mac / Linux**

CaptionCue is the presentation half of subtitles. It does not decide **what** is said — a dialogue system,
a Sequencer track, a sound wave with cues or one Blueprint node decides that. CaptionCue decides how it
**reads**: speaker names in colour, closed captions for non-speech sound with direction arrows, a queue that
never overwrites an unread line, a minimum reading time, and the player options a certification checklist
expects to find in your game's own menu.

---

## 1. Five-minute install

1. Copy `CaptionCue/` into your project's `Plugins/` folder and restart the editor. Enable it under
   **Edit ▸ Plugins ▸ UI** if it is not on already.
2. Give the game a HUD that draws captions. Pick one:
   * **New project, no HUD yet** — set your Game Mode's *HUD Class* to **CaptionCue HUD**. Done.
   * **You already have a HUD class** — do not replace it. Add a **CaptionCue HUD Component** to it and call
     `Draw Captions (Canvas)` from `DrawHUD` (C++) or from the **Receive Draw HUD** event (Blueprint).
   * **C++, no component wanted** — one line in your `DrawHUD`:
     ```cpp
     if (UCaptionCueSubsystem* Captions = GetWorld()->GetSubsystem<UCaptionCueSubsystem>())
     {
         Captions->DrawCaptions(Canvas);
     }
     ```
3. Show something. In any Blueprint: **Show Subtitle** (`Text`, `Speaker`). Or open the console and type
   `CaptionCue.Test`.

There is no widget to create, no font to import and no asset to author before captions appear: every kind
of caption has a built-in fallback style, so an install with no configuration still draws something
sensible.

---

## 2. Wiring up the options menu

This is the part a shipping game actually needs. Every option is a Blueprint node in
**CaptionCue ▸ Options**, and every one of them takes effect on the next frame — no restart, no reloaded
widget, no re-created HUD.

| Node | What the player is choosing |
|---|---|
| `Set Captions Enabled` | Captions on / off |
| `Set Text Scale` | 0.75 / 1.0 / 1.5 / 2.0 are the four presets worth offering; any value in `[0.25, 4.0]` works |
| `Set Background Box` | The box behind the text |
| `Set Background Opacity` | How solid that box is |
| `Set Show Speaker Names` | Names in front of spoken lines |
| `Set Show Sound Captions` | Non-speech captions — the switch between *subtitles* and *closed captions* |
| `Set Max Line Width Percent` | How wide a line may get before it wraps |
| `Set Safe Area Margin Percent` | Margin kept free at the screen edges |
| `Set Caption Position` | Bottom or Top |
| `Set Max Visible Lines` | How many captions may share the screen |
| `Set Direction Indicators` | Edge arrows for off-screen sounds |
| `Set Min Seconds Per Character` | Reading speed |

`Get Caption Settings` hands you the settings object itself if you would rather bind sliders and check boxes
straight to its properties. It is the same object the renderer reads, so there is exactly one source of
truth and nothing to synchronise.

**Persistence.** The setters change the live values only. In a packaged game the `Default*.ini` files are
read-only, so save the player's choices wherever your game already saves its options — a `UGameUserSettings`
subclass or a save game — and call the setters again when it loads. `Save Caption Options` writes
`DefaultGame.ini` and is meant for development, not for a shipped title.

---

## 3. Showing captions

### From Blueprint

| Node | Use |
|---|---|
| `Show Subtitle` | A spoken line. Pass a speaker id and a duration (0 = work it out from the text). |
| `Show Sound Caption` | Something the player heard. Pass a world position and an off-screen source gets an arrow. |
| `Show Caption From Request` | Every field spelled out: kind, style override, priority, position. |
| `Show Caption Now` | Straight onto the screen, pushing aside a visible line of *strictly lower* priority. |
| `Clear Captions` | Everything on screen and everything waiting, gone. |
| `Register Speaker` | Teach this world an id, a display name and a colour. |
| `Play Sound With Caption At Location` | Play a sound and caption it in one call, using the sound's own length. |

### From C++

```cpp
UCaptionCueSubsystem* Captions = GetWorld()->GetSubsystem<UCaptionCueSubsystem>();

Captions->RegisterSpeaker(TEXT("Guard"), LOCTEXT("Guard", "Guard"), FLinearColor(0.45f, 0.85f, 1.0f));
Captions->QueueSpeech(LOCTEXT("Halt", "Halt. Who goes there?"), TEXT("Guard"));
Captions->QueueSoundCaption(LOCTEXT("Creak", "door creaks"), DoorActor->GetActorLocation());
```

### The queue, and why it is a queue

The commonest bug in a hand-rolled subtitle widget is a single "current line" variable that the next line
overwrites. Two events land in the same frame and the player reads neither.

Here a caption goes into a **queue**. Up to *Max Visible Lines* (default 3) share the screen; anything
beyond that waits. A line only loses its place to something of a **strictly higher** priority, and even
then it goes back to the front of the queue rather than into the bin — it was interrupted, not cancelled.

Promotion order is *highest priority first, and among equals the one that has waited longest*, so two
captions queued in the same frame always come out in the order they went in.

### The reading clock

Captions timed to the audio disappear while the player is still reading them. Every line gets:

```
display time = clamp( max( sound length, characters × MinSecondsPerCharacter, MinDisplaySeconds ),
                      0.1, MaxDisplaySeconds )
```

Defaults: `MinSecondsPerCharacter = 0.06` (roughly 200 words a minute — comfortable, not fast),
`MinDisplaySeconds = 1.2`, `MaxDisplaySeconds = 12`. A half-second grunt subtitled with a full sentence
still stays up long enough to read the sentence.

---

## 4. The engine subtitle bridge — what it does, exactly

Every `USoundWave` carries a `Subtitles` array and every `UDialogueWave` carries spoken lines. The engine
collects them in `FSubtitleManager` and draws them as **one unstyled line**: no speaker, no box, no size
option, nothing for non-speech sound. The pipe is there; the far end is missing. CaptionCue is the far end.

**Which hook, precisely.** `FSubtitleManager` exposes `OnSetSubtitleText()`, a multicast delegate the engine
describes in its own header as a way for a display to *"hijack subtitle text … and get around the Canvas
display"*. CaptionCue binds it (once per process, from the first world's caption subsystem). Two documented
effects follow, both inside `FSubtitleManager::DisplaySubtitles`:

1. The engine stops drawing subtitles on the canvas itself — nothing is drawn twice, and there is nothing to
   switch off in your project.
2. The currently active, highest-priority subtitle text is broadcast to CaptionCue instead, once per game
   viewport draw, as an `FText`. An empty `FText` means "nothing is speaking".

**This is a real, supported hook.** No engine file is modified and no header is copied. It is switched on by
default (*Project Settings ▸ Plugins ▸ CaptionCue ▸ Engine Bridge*) and `Is Engine Subtitle Bridge Attached`
reports whether it is live. The statistics box (`CaptionCue.Stats 1`) shows the same thing plus a count of
the lines that have come through it.

**What the bridge cannot carry, and why.** The delegate passes exactly one thing: the text.

| Missing | What CaptionCue does instead |
|---|---|
| Duration | The reading clock works the display time out from the length of the line. |
| Speaker | Bridged lines are queued without a name. Engine subtitles have never had a speaker field. |
| World position | Bridged lines get no direction arrow. |
| Which world | The delegate is process-wide. In a multi-world session (Play as Client with two windows) every registered world hears the same line. Only one game viewport draws subtitles per frame, so in practice this is a curiosity rather than a problem — but it is a limit, and it is written down here rather than discovered later. |

Because the broadcast repeats every frame while a line is active, the bridge forwards a line only when the
text has actually **changed**. An empty broadcast ends the run without cutting the caption short: a line
that has been up for half a second still gets its full reading time.

**Where the bridge draws its captions.** `FSubtitleManager::DisplaySubtitles` is called from
`UGameViewportClient::Draw`, so bridged lines arrive during play (PIE or packaged) — not in an editor
viewport with nothing playing. Captions queued through the plugin's own API *do* appear there; see §8.

### The richer path: CaptionCue Audio Component

For sounds you own, use **CaptionCue Audio Component** (a `UAudioComponent` subclass) instead. It binds
`UAudioComponent::OnQueueSubtitles`, the engine's per-component interception point, which hands over the
cues **with their timings** and the sound's own duration — and the component knows where in the world it is.
That gives you three things the process-wide bridge cannot:

* **A speaker.** Set *Caption Speaker* once on the component; every line it plays is labelled and coloured.
* **A position.** With *Use Component Location* on, an off-screen sound draws an edge arrow.
* **A caption for a sound with no cues.** Most sound effects have no `Subtitles` array at all. Fill in
  *Caption Override* and the component queues that line when the sound starts — which is how you caption a
  door, a reload or a distant explosion without editing every wave in the project.

Binding `OnQueueSubtitles` means this component's cues no longer reach the engine subtitle manager. That is
intended: it is what stops the same line being drawn twice.

---

## 5. Styles

A **CaptionCue Style** is a data asset (*Miscellaneous ▸ Data Asset ▸ CaptionCue Style*) holding the look of
one kind of caption: font, size, colour, outline, shadow, background box, padding, justification, speaker
format, non-speech brackets, fade times and direction-arrow colour. Register your assets in
*Project Settings ▸ Plugins ▸ CaptionCue ▸ Styles*.

A style is selected by, in order:

1. the caption's own `Style Override` name, if it names a registered style;
2. the first registered style whose `Kind` matches the caption's kind;
3. a built-in fallback for that kind.

The four kinds — **Speech**, **Sound Effect**, **Music**, **System** — each have a built-in fallback, so
CaptionCue always draws something even before any asset exists. The demo content ships one authored style
per kind under `Content/CaptionCue/Styles/`.

**Where a style and a player option disagree, the player wins.** A style that asks for a background box does
not get one if the player has switched boxes off. Accessibility options are not designer preferences.

**Fonts.** No font ships with CaptionCue, for licensing reasons. Leave a style's `Font` empty and it uses
your project's subtitle font (`Engine ▸ Fonts ▸ Subtitle Font`), falling back to the engine's own Roboto.
Both are present in every install.

### Speaker formats

| `Speaker Format` | Result |
|---|---|
| `Colon` | `Guard: Halt, who goes there?` |
| `Brackets` | `[Guard] Halt, who goes there?` |
| `Dash` | `Guard - Halt, who goes there?` |
| `Own Line` | name on its own line above the text |
| `None` | never write the name |

The label is drawn in the **speaker's** colour and the line in the style's, in two draw calls on the same
line. That is what makes a three-way conversation followable. A speaker nobody registered gets a stable
colour derived from their id (`Auto Color Unknown Speakers`), so an unfinished project still reads as two
characters rather than one.

---

## 6. Direction indicators

A caption that carries a world position and whose sound is **outside the safe area** draws a small triangle
at the edge of the screen pointing at it. Inside the safe area it draws nothing — pointing at what the
player can already see is clutter.

This is the part that turns subtitles into *closed captions*: "[door creaks]" is not much use without
knowing which door.

* Per player: `Set Direction Indicators`.
* Per style: `Allow Direction Indicator` — the built-in Speech style has it **off**, because an arrow on
  every line of dialogue is noise; Sound Effect has it on.
* The arrow is drawn from the engine's white texture, so no image asset ships with the plugin and there is
  nothing that can go missing in your project.
* The arrow ignores camera roll, as the caption text does. In a game with a rolling camera the arrow points
  at the sound in unrolled screen space, which is the space the player is reading in.

---

## 7. Localisation

Everything the player reads is an `FText`. There is no `FString` anywhere on the display path.

* **Caption text** — pass `FText`. From C++ use `LOCTEXT`/`NSLOCTEXT`; from Blueprint, text pins are already
  `FText` and are gathered by the localisation dashboard.
* **Speaker names** — `FCaptionCueSpeaker::DisplayName` is an `FText` with its own key, so a translator can
  localise "Guard" without anyone touching the `FName` id the Blueprints pass around.
* **Punctuation** — the speaker separators (`{0}: `, `[{0}] `) are format patterns in the `CaptionCue`
  namespace, not hard-coded concatenation, so a translation can move the punctuation. French captioning, for
  instance, puts a space before the colon; that is a translation decision, not a code one.

### String Tables

The usual pattern, and the one the demo uses:

1. **Content Browser ▸ Miscellaneous ▸ String Table**, e.g. `ST_Dialogue`, with keys like `Guard_Halt`.
2. In Blueprint, drag off the caption's `Text` pin and choose **Make Literal Text ▸ String Table Entry**, or
   set the text pin to *Referenced Text* and pick the table and key.
3. In C++: `FText::FromStringTable(TEXT("/Game/Dialogue/ST_Dialogue.ST_Dialogue"), TEXT("Guard_Halt"))`.
4. Gather with **Window ▸ Localization Dashboard** as usual. Nothing about CaptionCue is special here — that
   is the point of using `FText` throughout.

Word wrapping is measured with the same Slate font-measuring service Slate itself uses, so a translated line
wraps to the width of the *translated* glyphs, not the English ones.

---

## 8. Editor preview

With *Editor Preview* on (default), captions also draw in an editor viewport with nothing playing, through
`UDebugDrawService`. Type `CaptionCue.Test` with the level open and the scene plays in the viewport.

This is a **second** attachment point, not the main one. `UDebugDrawService` is compiled out of a Shipping
build entirely; `AHUD::DrawHUD` is not, which is why that is the path the plugin is built around. The
preview exists because typography is checked by looking at it — line breaks at 150 %, a speaker name running
into the text, a box crowding the safe area — and because the store images are made that way.

---

## 9. Console commands

| Command | Effect |
|---|---|
| `CaptionCue.Test` | Queue a short scripted scene: two speakers, a bracketed sound caption behind the camera, a music cue and a high-priority system line. All fired at once, so the queue can be seen doing its job. |
| `CaptionCue.Scale <f>` | Text size multiplier. No argument prints the current value. |
| `CaptionCue.Box 0\|1` | Background box on or off. |
| `CaptionCue.Clear` | Drop everything on screen and everything waiting. |
| `CaptionCue.Stats 0\|1` | The statistics box: visible, queued, dropped, displaced, bridge state, current options, draw time. |

The commands reach every world that has a caption layer, so they behave the same during play and in the
editor with nothing playing.

---

## 10. What certification checklists ask for

Console holders and the European Accessibility Act ask for subtitle **options**. Below is a plain mapping
from the sort of requirement that appears on those lists to the setting that covers it.

**This is a mapping, not a promise.** CaptionCue provides the options; whether a submission passes is
decided by the platform holder, who also looks at your default values, your menu, your fonts, your
translations and everything else on the screen. No logos, no quotations from certification documents, and no
guarantee of a pass — see §12.

| The sort of thing that is asked for | Where it is |
|---|---|
| Subtitles can be switched on and off | `Set Captions Enabled` |
| Subtitle size is adjustable | `Set Text Scale` — offer 0.75 / 1.0 / 1.5 / 2.0 |
| A background or box behind subtitle text | `Set Background Box`, `Set Background Opacity` |
| The speaker is identified when several characters speak | `Set Show Speaker Names` + registered speaker colours |
| Important non-speech sound is conveyed | `Sound` / `Music` captions, `Set Show Sound Captions` |
| The direction of an important sound is conveyed | `Set Direction Indicators` |
| Text stays on screen long enough to be read | `MinSecondsPerCharacter`, `MinDisplaySeconds` |
| Text is not cut off by overscan / stays in the title-safe area | `Set Safe Area Margin Percent` |
| Line length is limited for readability | `Set Max Line Width Percent` |
| Subtitles do not obscure essential interface | `Set Caption Position` (Bottom / Top) |
| Settings apply immediately and survive a session | Every setter takes effect next frame; persist through your own settings object (§2) |
| Subtitles are localisable | `FText` throughout, String Table friendly (§7) |

Two habits worth keeping regardless of any checklist: leave `Show Sound Captions` **on** by default in an
accessibility preset and off in the standard one, and never ship with `MinSecondsPerCharacter` at 0.

---

## 11. Reference

### Classes

| Class | Role |
|---|---|
| `UCaptionCueSubsystem` | Per-world queue, reading clock and renderer. `UTickableWorldSubsystem`. |
| `ACaptionCueHUD` | Ready-made `AHUD`. Set it as your Game Mode's HUD Class. |
| `UCaptionCueHUDComponent` | Add to your own HUD class; call `Draw Captions (Canvas)` from `DrawHUD`. |
| `UCaptionCueAudioComponent` | `UAudioComponent` that brings its caption, speaker and position with it. |
| `UCaptionCueStyle` | `UPrimaryDataAsset`. The look of one kind of caption. |
| `UCaptionCueSettings` | `UDeveloperSettings`. Project defaults plus every player option, with runtime setters. |
| `UCaptionCueStatics` | Blueprint function library: showing captions, and the options menu. |
| `FCaptionCueSubtitleBridge` | The connection to `FSubtitleManager::OnSetSubtitleText`. Not a `UObject`. |

### Types

`ECaptionCueKind` — `Speech`, `Sound`, `Music`, `System`
`ECaptionCuePosition` — `Bottom`, `Top`
`ECaptionCueJustify` — `Left`, `Center`, `Right`
`ECaptionCueSpeakerFormat` — `Colon`, `Brackets`, `Dash`, `OwnLine`, `None`
`FCaptionCueRequest` — the input struct: text, speaker, kind, duration, priority, world location, style override
`FCaptionCueSpeaker` — id, display name, colour
`FCaptionCueStats` — visible, pending, totals, arrows drawn, draw milliseconds

### Module

One runtime module, `LoadingPhase: PreDefault`, `PlatformAllowList: Win64, Mac, Linux`.
Dependencies: `Core`, `CoreUObject`, `Engine`, `DeveloperSettings`, `SlateCore`, `RenderCore`.
**No `UMG` dependency, no `UnrealEd`, no third-party libraries.** Everything on the draw path exists in a
cooked Shipping build; the plugin is built and verified for Editor Development, Game Development and Game
Shipping.

---

## 12. Limits — what CaptionCue is not

* **Not a dialogue system.** No dialogue trees, no authoring interface, no branching, no voice-over
  management. It shows what another system triggers, and it is meant as a companion to Narrative Tales,
  SUDS, and the rest — not a replacement for one.
* **No automatic transcription.** No speech-to-text. Someone has to write the line.
* **No SRT playback** for linear video. Different problem, different tool.
* **No translation.** Localisation, yes — `FText`, String Tables, per-language wrapping. Translating the
  words is not something a plugin can do for you.
* **No guarantee of certification.** §10 is a mapping from common requirements to settings, nothing more.
* **No font shipped**, for licensing reasons — the engine's subtitle font is used instead.
* **The engine bridge carries text only** (§4). Duration, speaker and position are not in the delegate; use
  `UCaptionCueAudioComponent` or the direct API when you need them.
* **Bridged engine subtitles do not appear in an editor viewport without play mode**, because the engine
  only broadcasts them from the game viewport's draw. Captions queued through the plugin's own API do.
* **Draws on `UCanvas`, not in UMG.** That is a deliberate choice — no `UUserWidget` per line, no Slate
  layout pass per caption, and one draw path that behaves identically in the editor and in a cooked build.
  If you need captions inside a UMG widget hierarchy (for a 3D world-space screen, say), CaptionCue is not
  the tool.

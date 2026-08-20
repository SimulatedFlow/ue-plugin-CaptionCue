# CaptionCue — Subtitles & Closed Captions

Unreal Engine 5.8 · one runtime C++ module · Win64 / Mac / Linux

The presentation half of subtitles: speaker names in colour, closed captions for non-speech sound with
direction arrows, a queue that never overwrites an unread line, a minimum reading time, and the player
options a certification checklist expects to find in your own menu.

**CaptionCue does not decide what is said.** It shows what your game — or the engine, or a third-party
dialogue system — already triggers.

## Install in five minutes

1. Copy `CaptionCue/` into your project's `Plugins/` folder and restart the editor.
2. Set your Game Mode's **HUD Class** to **CaptionCue HUD** — or add a **CaptionCue HUD Component** to the
   HUD class you already have and call `Draw Captions (Canvas)` from `DrawHUD`.
3. Call **Show Subtitle** from any Blueprint, or type `CaptionCue.Test` in the console.

No widget to build, no font to import, no asset to author first.

## Where things are

| | |
|---|---|
| Full documentation | [`Docs/DOCUMENTATION.md`](Docs/DOCUMENTATION.md) |
| Store description and honest limits | [`Docs/Fab-Store-Description.md`](Docs/Fab-Store-Description.md) |
| Project settings | Project Settings ▸ Plugins ▸ CaptionCue |
| Demo content | `Content/CaptionCue/` |
| Console commands | `CaptionCue.Test`, `.Scale`, `.Box`, `.Clear`, `.Stats` |

## Limits

Not a dialogue system. No speech-to-text, no translation, no SRT playback for linear video, no guarantee of
certification, and no font shipped (the engine's subtitle font is used). Captions draw on `UCanvas` from
`AHUD`, not inside a UMG widget hierarchy. See `Docs/DOCUMENTATION.md` §12 for the full list.

<!-- SF-STORE-BLOCK:BEGIN -->
## 🛒 Source-available — see before you buy

This repository contains the **full source** of a commercial Unreal Engine plugin. It is **source-available, not open source**: read it, evaluate it, then buy a license to use it. See **the Fab Content License Agreement / Unreal Engine EULA (purchase required)**.

**Get it / Buy:**
- Fab store — all our UE5 plugins: https://www.fab.com/sellers/Silvan%20Teufel

_This plugin does not have its own Fab listing yet — the store link above is where everything we currently sell lives._

### 📬 **Free UE5 Snippet-Pack**

10 ready-to-use C++/Blueprint building blocks (subsystems, versioned saves, async nodes, editor tooling) — MIT licensed. Get it by joining the newsletter — plus a heads-up when something new ships. Double opt-in, unsubscribe in one click, no address sharing.

👉 **[Get the free pack](https://silvan.teufel-engineering.com/newsletter/plugins/?q=gh)**

_© 2026 Silvan Teufel. All rights reserved._
<!-- SF-STORE-BLOCK:END -->

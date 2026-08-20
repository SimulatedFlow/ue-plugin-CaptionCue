// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "CaptionCueSubsystem.h"

#include "Application/SlateApplicationBase.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "CaptionCueLog.h"
#include "CaptionCueSettings.h"
#include "CaptionCueStyle.h"
#include "CaptionCueSubtitleBridge.h"
#include "Camera/PlayerCameraManager.h"
#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "GameFramework/PlayerController.h"
#include "GlobalRenderResources.h"
#include "HAL/IConsoleManager.h"
#include "Misc/StringBuilder.h"
#include "Rendering/SlateRenderer.h"
#include "SceneView.h"
#include "Stats/Stats.h"

#define LOCTEXT_NAMESPACE "CaptionCue"

DECLARE_STATS_GROUP(TEXT("CaptionCue"), STATGROUP_CaptionCue, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("CaptionCue Draw"), STAT_CaptionCueDraw, STATGROUP_CaptionCue);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Captions Visible"), STAT_CaptionCueVisible, STATGROUP_CaptionCue);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Captions Queued"), STAT_CaptionCuePending, STATGROUP_CaptionCue);

namespace CaptionCueDraw
{
	/** Gap between two captions that share the screen, before the player's text scale. */
	static constexpr float CaptionGap = 6.0f;

	/**
	 * Width and height of a piece of text in the given font.
	 *
	 * The measure service is the cache Slate itself uses, so wrapping a caption costs hash lookups rather
	 * than glyph work. The fallback only matters in a build with no Slate application at all, where nothing
	 * is drawn anyway; it exists so that a dedicated server cannot divide by zero here.
	 */
	static FVector2D MeasureText(FStringView Text, const FSlateFontInfo& FontInfo)
	{
		if (FSlateApplicationBase::IsInitialized())
		{
			if (FSlateRenderer* Renderer = FSlateApplicationBase::Get().GetRenderer())
			{
				const UE::Slate::FDeprecateVector2DResult Measured =
					Renderer->GetFontMeasureService()->Measure(Text, FontInfo, 1.0f);
				return FVector2D(Measured.X, Measured.Y);
			}
		}

		return FVector2D(Text.Len() * FontInfo.Size * 0.55f, FontInfo.Size * 1.2f);
	}

	/**
	 * A stable colour for a speaker nobody registered.
	 *
	 * Two characters in an unfinished project still read as two characters, and the colour a character gets
	 * does not change between runs or between machines, because it comes from the name and nothing else.
	 */
	static FLinearColor ColorFromName(FName Name)
	{
		uint32 Hash = GetTypeHash(Name);
		Hash ^= Hash >> 15;
		Hash *= 0x2c1b3c6du;
		Hash ^= Hash >> 12;

		// Saturation kept well below full: a fully saturated caption colour is hard to read at small sizes.
		return FLinearColor::MakeFromHSV8(static_cast<uint8>(Hash & 0xFFu), 96, 255);
	}

	/** Horizontal offset of a line of the given width inside a block of the given width. */
	static float JustifyOffset(ECaptionCueJustify Justification, float BlockWidth, float LineWidth)
	{
		switch (Justification)
		{
		case ECaptionCueJustify::Right:
			return BlockWidth - LineWidth;
		case ECaptionCueJustify::Center:
			return (BlockWidth - LineWidth) * 0.5f;
		case ECaptionCueJustify::Left:
		default:
			return 0.0f;
		}
	}
}

namespace CaptionCueConsole
{
	/**
	 * Console commands reach every world that has a caption layer, not only the one the console happens to
	 * be bound to. CaptionCue.Test then does the same thing whether it is typed during play or in the editor
	 * with nothing playing — which is the case the store images are made in.
	 */
	static void ForEachSubsystem(UWorld* World, TFunctionRef<void(UCaptionCueSubsystem&)> Func)
	{
		TArray<UCaptionCueSubsystem*, TInlineAllocator<4>> Found;

		if (World)
		{
			if (UCaptionCueSubsystem* Subsystem = World->GetSubsystem<UCaptionCueSubsystem>())
			{
				Found.Add(Subsystem);
			}
		}

		if (Found.Num() == 0 && GEngine)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.World())
				{
					if (UCaptionCueSubsystem* Subsystem = Context.World()->GetSubsystem<UCaptionCueSubsystem>())
					{
						Found.AddUnique(Subsystem);
					}
				}
			}
		}

		for (UCaptionCueSubsystem* Subsystem : Found)
		{
			Func(*Subsystem);
		}
	}

	static bool ParseBool(const TArray<FString>& Args, bool bDefault)
	{
		if (Args.Num() == 0)
		{
			return bDefault;
		}
		return Args[0].ToBool() || Args[0] == TEXT("1");
	}

	static FAutoConsoleCommandWithWorldAndArgs GTest(
		TEXT("CaptionCue.Test"),
		TEXT("CaptionCue.Test - queue a short scripted scene: two speakers, a bracketed sound caption behind the camera, and two lines fired at once."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			ForEachSubsystem(World, [](UCaptionCueSubsystem& Subsystem) { Subsystem.PlayTestScene(); });
		}));

	static FAutoConsoleCommandWithWorldAndArgs GScale(
		TEXT("CaptionCue.Scale"),
		TEXT("CaptionCue.Scale <f> - caption text size multiplier. 0.75, 1.0, 1.5 and 2.0 are the presets worth putting in a menu."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			UCaptionCueSettings* Settings = UCaptionCueSettings::GetMutable();
			if (Args.Num() == 0)
			{
				UE_LOG(LogCaptionCue, Display, TEXT("CaptionCue.Scale = %.2f"), Settings->TextScale);
				return;
			}
			Settings->SetTextScale(FCString::Atof(*Args[0]));
			UE_LOG(LogCaptionCue, Display, TEXT("CaptionCue.Scale = %.2f"), Settings->TextScale);
		}));

	static FAutoConsoleCommandWithWorldAndArgs GBox(
		TEXT("CaptionCue.Box"),
		TEXT("CaptionCue.Box 0|1 - background box behind the caption text."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			UCaptionCueSettings::GetMutable()->SetBackgroundBox(ParseBool(Args, true));
		}));

	static FAutoConsoleCommandWithWorldAndArgs GClear(
		TEXT("CaptionCue.Clear"),
		TEXT("CaptionCue.Clear - drop every caption on screen and everything waiting behind them."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			ForEachSubsystem(World, [](UCaptionCueSubsystem& Subsystem) { Subsystem.ClearCaptions(); });
		}));

	static FAutoConsoleCommandWithWorldAndArgs GStats(
		TEXT("CaptionCue.Stats"),
		TEXT("CaptionCue.Stats 0|1 - the on-screen caption statistics box: visible, queued, dropped, and whether the engine bridge is attached."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			const bool bShow = ParseBool(Args, true);
			ForEachSubsystem(World, [bShow](UCaptionCueSubsystem& Subsystem) { Subsystem.SetShowStats(bShow); });
		}));
}

// -------------------------------------------------------------------------------------------------------
// Lifetime
// -------------------------------------------------------------------------------------------------------

void UCaptionCueSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UCaptionCueSettings& Settings = UCaptionCueSettings::Get();
	bShowStats = Settings.bShowStatsByDefault;

	// Resolved once and held as a property. Loading a font on the draw path would be a hitch, and a font
	// nobody holds a reference to is a font the garbage collector may take between two frames.
	FallbackFont = GEngine ? GEngine->GetSubtitleFont() : nullptr;
	if (!FallbackFont)
	{
		FallbackFont = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
	}

	ResolveStyles();
	UpdateEngineBridgeRegistration();

#if WITH_EDITOR
	// Second attachment point, editor only, and deliberately not the main one.
	//
	// The main path is AHUD::DrawHUD (see ACaptionCueHUD and UCaptionCueHUDComponent), because that is the
	// path that survives into a cooked Shipping build: UDebugDrawService is compiled out there entirely.
	//
	// This registration still earns its place. A caption is a piece of typography, and typography is checked
	// by looking at it: line breaks at 150 %, a speaker name that runs into the text, a box that crowds the
	// safe area. Being able to see all of that in a viewport without entering play mode is worth the twenty
	// lines, and it is also how the store images for this plugin are made.
	if (Settings.bEnableEditorPreview)
	{
		EditorPreviewHandle = UDebugDrawService::Register(
			TEXT("Game"),
			FDebugDrawDelegate::CreateUObject(this, &UCaptionCueSubsystem::OnEditorPreviewDraw));
	}
#endif
}

void UCaptionCueSubsystem::Deinitialize()
{
#if WITH_EDITOR
	if (EditorPreviewHandle.IsValid())
	{
		UDebugDrawService::Unregister(EditorPreviewHandle);
		EditorPreviewHandle.Reset();
	}
#endif

	if (bRegisteredWithBridge)
	{
		FCaptionCueSubtitleBridge::Get().UnregisterSubsystem(this);
		bRegisteredWithBridge = false;
	}

	VisibleCaptions.Reset();
	PendingCaptions.Reset();
	RuntimeSpeakers.Reset();
	Styles.Reset();
	StyleByName.Reset();
	FallbackFont = nullptr;

	Super::Deinitialize();
}

bool UCaptionCueSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Editor is in the list on purpose: captions have to draw in the viewport without play mode.
	return WorldType == EWorldType::Game
		|| WorldType == EWorldType::PIE
		|| WorldType == EWorldType::Editor;
}

bool UCaptionCueSubsystem::IsTickableInEditor() const
{
	return UCaptionCueSettings::Get().bEnableEditorPreview;
}

TStatId UCaptionCueSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCaptionCueSubsystem, STATGROUP_Tickables);
}

void UCaptionCueSubsystem::UpdateEngineBridgeRegistration()
{
	const bool bWanted = UCaptionCueSettings::Get().bBridgeEngineSubtitles;

	if (bWanted && !bRegisteredWithBridge)
	{
		FCaptionCueSubtitleBridge::Get().RegisterSubsystem(this);
		bRegisteredWithBridge = true;
	}
	else if (!bWanted && bRegisteredWithBridge)
	{
		FCaptionCueSubtitleBridge::Get().UnregisterSubsystem(this);
		bRegisteredWithBridge = false;
	}
}

bool UCaptionCueSubsystem::IsEngineBridgeAttached() const
{
	return bRegisteredWithBridge && FCaptionCueSubtitleBridge::Get().IsHooked();
}

// -------------------------------------------------------------------------------------------------------
// Styles
// -------------------------------------------------------------------------------------------------------

void UCaptionCueSubsystem::ResolveStyles()
{
	Styles.Reset();
	StyleByName.Reset();
	for (int32& Index : StyleForKind)
	{
		Index = INDEX_NONE;
	}

	const UCaptionCueSettings& Settings = UCaptionCueSettings::Get();

	for (const TSoftObjectPtr<UCaptionCueStyle>& SoftStyle : Settings.Styles)
	{
		if (SoftStyle.IsNull())
		{
			continue;
		}

		UCaptionCueStyle* Style = SoftStyle.LoadSynchronous();
		if (!Style)
		{
			UE_LOG(LogCaptionCue, Warning, TEXT("Style '%s' could not be loaded and is ignored."), *SoftStyle.ToString());
			continue;
		}

		const FName Name = Style->StyleName.IsNone() ? Style->GetFName() : Style->StyleName;
		if (StyleByName.Contains(Name))
		{
			UE_LOG(LogCaptionCue, Warning, TEXT("Two styles claim the name '%s'; the second one is ignored."), *Name.ToString());
			continue;
		}

		const int32 Index = Styles.Add(Style);
		StyleByName.Add(Name, Index);

		// First style claiming a kind wins it, so the order in the settings list is the priority order.
		const int32 KindIndex = static_cast<int32>(Style->Kind);
		if (StyleForKind[KindIndex] == INDEX_NONE)
		{
			StyleForKind[KindIndex] = Index;
		}
	}

	// Built-in fallbacks. A plugin that shows nothing on a project where nobody has authored an asset reads
	// as a broken plugin, so every kind ends up with a style whether the project provided one or not.
	for (int32 KindIndex = 0; KindIndex < UE_ARRAY_COUNT(StyleForKind); ++KindIndex)
	{
		if (StyleForKind[KindIndex] != INDEX_NONE)
		{
			continue;
		}

		if (UCaptionCueStyle* Builtin = CreateBuiltinStyle(static_cast<ECaptionCueKind>(KindIndex)))
		{
			const int32 Index = Styles.Add(Builtin);
			StyleForKind[KindIndex] = Index;
			StyleByName.FindOrAdd(Builtin->StyleName, Index);
		}
	}
}

UCaptionCueStyle* UCaptionCueSubsystem::CreateBuiltinStyle(ECaptionCueKind Kind)
{
	UCaptionCueStyle* Style = NewObject<UCaptionCueStyle>(this, NAME_None, RF_Transient);
	if (!Style)
	{
		return nullptr;
	}

	Style->Kind = Kind;

	switch (Kind)
	{
	case ECaptionCueKind::Speech:
		Style->StyleName = TEXT("Speech");
		Style->FontSize = 26.0f;
		Style->TextColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
		Style->SpeakerFormat = ECaptionCueSpeakerFormat::Colon;
		Style->Prefix.Reset();
		Style->Suffix.Reset();
		// Speech almost never wants an edge arrow: the speaker is usually the thing the player is looking at,
		// and an arrow on every line of dialogue is noise.
		Style->bAllowDirectionIndicator = false;
		break;

	case ECaptionCueKind::Sound:
		Style->StyleName = TEXT("SoundEffect");
		Style->FontSize = 24.0f;
		Style->TextColor = FLinearColor(0.85f, 0.92f, 1.0f, 1.0f);
		Style->SpeakerFormat = ECaptionCueSpeakerFormat::None;
		Style->bAllowDirectionIndicator = true;
		break;

	case ECaptionCueKind::Music:
		Style->StyleName = TEXT("Music");
		Style->FontSize = 24.0f;
		Style->TextColor = FLinearColor(0.86f, 0.78f, 1.0f, 1.0f);
		Style->SpeakerFormat = ECaptionCueSpeakerFormat::None;
		Style->Prefix = TEXT("♪ ");
		Style->Suffix = TEXT(" ♪");
		Style->bAllowDirectionIndicator = false;
		break;

	case ECaptionCueKind::System:
	default:
		Style->StyleName = TEXT("System");
		Style->FontSize = 24.0f;
		Style->TextColor = FLinearColor(1.0f, 0.92f, 0.66f, 1.0f);
		Style->SpeakerFormat = ECaptionCueSpeakerFormat::None;
		Style->Prefix.Reset();
		Style->Suffix.Reset();
		Style->bAllowDirectionIndicator = false;
		break;
	}

	return Style;
}

void UCaptionCueSubsystem::RefreshStyles()
{
	ResolveStyles();
}

UCaptionCueStyle* UCaptionCueSubsystem::GetStyleByName(FName StyleName) const
{
	if (const int32* Found = StyleByName.Find(StyleName))
	{
		return Styles.IsValidIndex(*Found) ? Styles[*Found].Get() : nullptr;
	}
	return nullptr;
}

UCaptionCueStyle* UCaptionCueSubsystem::GetStyleForKind(ECaptionCueKind Kind) const
{
	const int32 KindIndex = static_cast<int32>(Kind);
	if (KindIndex >= 0 && KindIndex < UE_ARRAY_COUNT(StyleForKind))
	{
		const int32 Index = StyleForKind[KindIndex];
		if (Styles.IsValidIndex(Index))
		{
			return Styles[Index].Get();
		}
	}

	return Styles.Num() > 0 ? Styles[0].Get() : nullptr;
}

TArray<FName> UCaptionCueSubsystem::GetStyleNames() const
{
	TArray<FName> Result;
	Result.Reserve(Styles.Num());
	for (const TObjectPtr<UCaptionCueStyle>& Style : Styles)
	{
		if (Style)
		{
			Result.Add(Style->StyleName.IsNone() ? Style->GetFName() : Style->StyleName);
		}
	}
	return Result;
}

// -------------------------------------------------------------------------------------------------------
// Speakers
// -------------------------------------------------------------------------------------------------------

void UCaptionCueSubsystem::RegisterSpeaker(FName SpeakerId, const FText& DisplayName, FLinearColor Color)
{
	if (SpeakerId.IsNone())
	{
		return;
	}

	FCaptionCueSpeaker& Speaker = RuntimeSpeakers.FindOrAdd(SpeakerId);
	Speaker.SpeakerId = SpeakerId;
	Speaker.DisplayName = DisplayName;
	Speaker.Color = Color;
}

void UCaptionCueSubsystem::UnregisterSpeaker(FName SpeakerId)
{
	RuntimeSpeakers.Remove(SpeakerId);
}

FCaptionCueSpeaker UCaptionCueSubsystem::ResolveSpeaker(FName SpeakerId) const
{
	FCaptionCueSpeaker Result;
	Result.SpeakerId = SpeakerId;

	if (SpeakerId.IsNone())
	{
		return Result;
	}

	// Runtime registrations win: a level that renames "Guard" to "Captain Elsa" should not have to edit the
	// project settings to do it.
	if (const FCaptionCueSpeaker* Runtime = RuntimeSpeakers.Find(SpeakerId))
	{
		return *Runtime;
	}

	const UCaptionCueSettings& Settings = UCaptionCueSettings::Get();
	if (Settings.FindSpeaker(SpeakerId, Result))
	{
		return Result;
	}

	Result.DisplayName = FText::FromName(SpeakerId);
	Result.Color = Settings.bAutoColorUnknownSpeakers
		? CaptionCueDraw::ColorFromName(SpeakerId)
		: GetStyleForKind(ECaptionCueKind::Speech)->DefaultSpeakerColor;

	return Result;
}

// -------------------------------------------------------------------------------------------------------
// Queueing
// -------------------------------------------------------------------------------------------------------

bool UCaptionCueSubsystem::BuildEntry(const FCaptionCueRequest& Request, bool bFromBridge, FCaptionCueEntry& OutEntry)
{
	const UCaptionCueSettings& Settings = UCaptionCueSettings::Get();

	if (!Settings.bCaptionsEnabled || Request.Text.IsEmpty())
	{
		return false;
	}

	// The player asked not to be told about sounds. Refusing here rather than at draw time means the line
	// never takes a slot away from a line the player does want to read.
	if (Request.Kind != ECaptionCueKind::Speech && !Settings.bShowSoundCaptions)
	{
		return false;
	}

	OutEntry = FCaptionCueEntry();
	OutEntry.Text = Request.Text;
	OutEntry.Speaker = Request.Speaker;
	OutEntry.SpeakerDisplayName = Request.SpeakerDisplayName;
	OutEntry.Kind = Request.Kind;
	OutEntry.StyleOverride = Request.StyleOverride;
	OutEntry.Priority = Request.Priority;
	OutEntry.bFromEngineBridge = bFromBridge;
	OutEntry.Id = NextCaptionId++;
	OutEntry.Sequence = NextSequence++;

	if (Request.bHasWorldLocation)
	{
		OutEntry.WorldLocation = Request.WorldLocation;
	}

	// The reading clock. Counting the raw characters of the line is close enough: the speaker label and the
	// brackets are decoration, and giving the player a fraction of a second extra for them is the right way
	// to be wrong.
	OutEntry.Duration = Settings.ResolveDisplaySeconds(Request.Duration, Request.Text.ToString().Len());

	return true;
}

int32 UCaptionCueSubsystem::QueueCaption(const FCaptionCueRequest& Request)
{
	FCaptionCueEntry Entry;
	if (!BuildEntry(Request, /*bFromBridge=*/false, Entry))
	{
		return 0;
	}

	const UCaptionCueSettings& Settings = UCaptionCueSettings::Get();

	if (PendingCaptions.Num() >= FMath::Max(Settings.MaxQueuedCaptions, 1))
	{
		// Refusing loudly rather than growing without limit. A game that has thirty-two captions waiting has
		// a scripting problem, and a queue that silently absorbs them hides it until a play test.
		++Stats.TotalDropped;
		UE_LOG(LogCaptionCue, Warning,
			TEXT("Caption queue is full (%d waiting); dropping \"%s\". Raise Max Queued Captions or slow the source down."),
			PendingCaptions.Num(), *Request.Text.ToString());
		return 0;
	}

	++Stats.TotalQueued;
	if (Entry.bFromEngineBridge)
	{
		++Stats.TotalFromEngineBridge;
	}

	const int32 Id = static_cast<int32>(Entry.Id);
	PendingCaptions.Add(MoveTemp(Entry));

	// Straight onto the screen when there is room, so a caption fired on a frame the world is not ticking
	// (an editor viewport with the game paused) still appears.
	PromotePending();

	return Id;
}

int32 UCaptionCueSubsystem::QueueSpeech(const FText& Text, FName Speaker, float Duration, int32 Priority)
{
	FCaptionCueRequest Request;
	Request.Text = Text;
	Request.Speaker = Speaker;
	Request.Kind = ECaptionCueKind::Speech;
	Request.Duration = Duration;
	Request.Priority = Priority;

	return QueueCaption(Request);
}

int32 UCaptionCueSubsystem::QueueSoundCaption(const FText& Text, FVector WorldLocation, bool bHasWorldLocation, float Duration, int32 Priority)
{
	FCaptionCueRequest Request;
	Request.Text = Text;
	Request.Kind = ECaptionCueKind::Sound;
	Request.Duration = Duration;
	Request.Priority = Priority;
	Request.bHasWorldLocation = bHasWorldLocation;
	Request.WorldLocation = WorldLocation;

	return QueueCaption(Request);
}

int32 UCaptionCueSubsystem::ShowCaption(const FCaptionCueRequest& Request)
{
	FCaptionCueEntry Entry;
	if (!BuildEntry(Request, /*bFromBridge=*/false, Entry))
	{
		return 0;
	}

	++Stats.TotalQueued;

	const int32 Id = static_cast<int32>(Entry.Id);
	const int32 MaxVisible = FMath::Max(UCaptionCueSettings::Get().MaxVisibleLines, 1);

	if (VisibleCaptions.Num() >= MaxVisible)
	{
		// Find the weakest line on screen. Only a genuinely lower priority may be pushed aside — equal
		// priority does not win, or two chatty systems would take turns interrupting each other forever.
		int32 WeakestIndex = INDEX_NONE;
		for (int32 Index = 0; Index < VisibleCaptions.Num(); ++Index)
		{
			if (WeakestIndex == INDEX_NONE || VisibleCaptions[Index].Priority < VisibleCaptions[WeakestIndex].Priority)
			{
				WeakestIndex = Index;
			}
		}

		if (WeakestIndex == INDEX_NONE || VisibleCaptions[WeakestIndex].Priority >= Entry.Priority)
		{
			// Nothing may be displaced, so this line waits its turn like any other. It does not overwrite.
			if (PendingCaptions.Num() >= FMath::Max(UCaptionCueSettings::Get().MaxQueuedCaptions, 1))
			{
				++Stats.TotalDropped;
				return 0;
			}

			PendingCaptions.Insert(MoveTemp(Entry), 0);
			return Id;
		}

		// The displaced line goes back to the front of the queue with its clock reset, not into the bin: it
		// was interrupted, not cancelled, and the player still has not read it.
		FCaptionCueEntry Displaced = MoveTemp(VisibleCaptions[WeakestIndex]);
		VisibleCaptions.RemoveAt(WeakestIndex);
		Displaced.Age = 0.0f;
		PendingCaptions.Insert(MoveTemp(Displaced), 0);
		++Stats.TotalDisplaced;
	}

	VisibleCaptions.Add(MoveTemp(Entry));
	return Id;
}

void UCaptionCueSubsystem::QueueBridgedSubtitle(const FText& SubtitleText)
{
	const UCaptionCueSettings& Settings = UCaptionCueSettings::Get();

	FCaptionCueRequest Request;
	Request.Text = SubtitleText;
	Request.Kind = Settings.EngineBridgeKind;
	Request.Priority = Settings.EngineBridgePriority;

	FCaptionCueEntry Entry;
	if (!BuildEntry(Request, /*bFromBridge=*/true, Entry))
	{
		return;
	}

	if (PendingCaptions.Num() >= FMath::Max(Settings.MaxQueuedCaptions, 1))
	{
		++Stats.TotalDropped;
		return;
	}

	++Stats.TotalQueued;
	++Stats.TotalFromEngineBridge;

	PendingCaptions.Add(MoveTemp(Entry));
	PromotePending();
}

void UCaptionCueSubsystem::ClearCaptions()
{
	VisibleCaptions.Reset();
	PendingCaptions.Reset();

	Stats.Visible = 0;
	Stats.Pending = 0;
}

bool UCaptionCueSubsystem::RemoveCaption(int32 CaptionId)
{
	const uint32 Id = static_cast<uint32>(CaptionId);

	const int32 VisibleIndex = VisibleCaptions.IndexOfByPredicate([Id](const FCaptionCueEntry& Entry) { return Entry.Id == Id; });
	if (VisibleIndex != INDEX_NONE)
	{
		VisibleCaptions.RemoveAt(VisibleIndex);
		return true;
	}

	const int32 PendingIndex = PendingCaptions.IndexOfByPredicate([Id](const FCaptionCueEntry& Entry) { return Entry.Id == Id; });
	if (PendingIndex != INDEX_NONE)
	{
		PendingCaptions.RemoveAt(PendingIndex);
		return true;
	}

	return false;
}

FText UCaptionCueSubsystem::GetVisibleCaptionText(int32 Index) const
{
	return VisibleCaptions.IsValidIndex(Index) ? VisibleCaptions[Index].Text : FText::GetEmpty();
}

// -------------------------------------------------------------------------------------------------------
// Tick
// -------------------------------------------------------------------------------------------------------

void UCaptionCueSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Cheap enough to do every frame, and it means switching the bridge off in Project Settings takes effect
	// without restarting the editor.
	UpdateEngineBridgeRegistration();

	AgeCaptions(DeltaTime);

	for (FCaptionCueEntry& Entry : PendingCaptions)
	{
		Entry.QueuedAge += DeltaTime;
	}

	PromotePending();

	Stats.Visible = VisibleCaptions.Num();
	Stats.Pending = PendingCaptions.Num();

	SET_DWORD_STAT(STAT_CaptionCueVisible, Stats.Visible);
	SET_DWORD_STAT(STAT_CaptionCuePending, Stats.Pending);
}

void UCaptionCueSubsystem::AgeCaptions(float DeltaTime)
{
	for (int32 Index = VisibleCaptions.Num() - 1; Index >= 0; --Index)
	{
		FCaptionCueEntry& Entry = VisibleCaptions[Index];
		Entry.Age += DeltaTime;

		if (Entry.Age >= Entry.Duration)
		{
			VisibleCaptions.RemoveAt(Index);
		}
	}
}

void UCaptionCueSubsystem::PromotePending()
{
	const int32 MaxVisible = FMath::Max(UCaptionCueSettings::Get().MaxVisibleLines, 1);

	// Anything above the limit — the player just turned Max Visible Lines down — goes back to the front of
	// the queue rather than being thrown away.
	while (VisibleCaptions.Num() > MaxVisible)
	{
		FCaptionCueEntry Demoted = MoveTemp(VisibleCaptions.Last());
		VisibleCaptions.Pop();
		Demoted.Age = 0.0f;
		PendingCaptions.Insert(MoveTemp(Demoted), 0);
	}

	while (VisibleCaptions.Num() < MaxVisible && PendingCaptions.Num() > 0)
	{
		// Highest priority first, and among equals the one that has been waiting longest. Deterministic, so
		// two captions queued in the same frame always come out in the order they went in.
		int32 BestIndex = 0;
		for (int32 Index = 1; Index < PendingCaptions.Num(); ++Index)
		{
			const FCaptionCueEntry& Candidate = PendingCaptions[Index];
			const FCaptionCueEntry& Best = PendingCaptions[BestIndex];

			if (Candidate.Priority > Best.Priority ||
				(Candidate.Priority == Best.Priority && Candidate.Sequence < Best.Sequence))
			{
				BestIndex = Index;
			}
		}

		FCaptionCueEntry Promoted = MoveTemp(PendingCaptions[BestIndex]);
		PendingCaptions.RemoveAt(BestIndex);
		Promoted.Age = 0.0f;
		VisibleCaptions.Add(MoveTemp(Promoted));
	}
}

// -------------------------------------------------------------------------------------------------------
// Test scene
// -------------------------------------------------------------------------------------------------------

void UCaptionCueSubsystem::PlayTestScene()
{
	// Deliberately fired in one go. The queue is what spaces them out, which is the point being demonstrated:
	// six lines arrive at once, and the player still gets to read all six, in order, one reading time apart.
	RegisterSpeaker(TEXT("CaptionCueTestA"), LOCTEXT("TestSpeakerA", "Mara"), FLinearColor(0.45f, 0.85f, 1.0f, 1.0f));
	RegisterSpeaker(TEXT("CaptionCueTestB"), LOCTEXT("TestSpeakerB", "Detrick"), FLinearColor(1.0f, 0.63f, 0.42f, 1.0f));

	QueueSpeech(LOCTEXT("TestLine1", "You hear that? Something moved behind the shutters."), TEXT("CaptionCueTestA"));
	QueueSpeech(LOCTEXT("TestLine2", "That building has been empty for nine years. Nothing moves in there."), TEXT("CaptionCueTestB"));

	// A sound behind the camera, so the edge arrow has something to point at. If nothing has drawn yet there
	// is no camera to be behind, and the caption simply arrives without an arrow.
	const FVector Behind = bHasCachedView
		? CachedViewLocation - CachedViewForward * 600.0f + FVector::CrossProduct(CachedViewForward, FVector::UpVector) * 500.0f
		: FVector::ZeroVector;

	QueueSoundCaption(LOCTEXT("TestLine3", "metal shutter rattles"), Behind, bHasCachedView);

	QueueSpeech(LOCTEXT("TestLine4", "Nine years is a long time for nothing to move."), TEXT("CaptionCueTestA"));

	FCaptionCueRequest Music;
	Music.Text = LOCTEXT("TestLine5", "low strings swell");
	Music.Kind = ECaptionCueKind::Music;
	QueueCaption(Music);

	FCaptionCueRequest Warning;
	Warning.Text = LOCTEXT("TestLine6", "Objective updated: investigate the shutters");
	Warning.Kind = ECaptionCueKind::System;
	Warning.Priority = 10;
	QueueCaption(Warning);
}

// -------------------------------------------------------------------------------------------------------
// Drawing
// -------------------------------------------------------------------------------------------------------

void UCaptionCueSubsystem::DrawCaptions(UCanvas* Canvas)
{
	if (!Canvas)
	{
		return;
	}

	// Remembering the frame lets the editor preview stand aside whenever a real HUD has already drawn.
	LastGameDrawFrame = GFrameCounter;

	DrawPass(Canvas);
}

bool UCaptionCueSubsystem::ResolveView(UCanvas* Canvas, FVector& OutLocation, FVector& OutForward) const
{
	if (Canvas && Canvas->SceneView)
	{
		OutLocation = Canvas->SceneView->ViewMatrices.GetViewOrigin();
		OutForward = Canvas->SceneView->GetViewDirection();
		return true;
	}

	if (const UWorld* World = GetWorld())
	{
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			if (const APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
			{
				OutLocation = CameraManager->GetCameraLocation();
				OutForward = CameraManager->GetCameraRotation().Vector();
				return true;
			}
		}
	}

	return false;
}

void UCaptionCueSubsystem::DrawPass(UCanvas* Canvas)
{
	SCOPE_CYCLE_COUNTER(STAT_CaptionCueDraw);

	const double StartSeconds = FPlatformTime::Seconds();

	Stats.ArrowsDrawn = 0;

	const UCaptionCueSettings& Settings = UCaptionCueSettings::Get();

	{
		FVector ViewLocation;
		FVector ViewForward;
		if (ResolveView(Canvas, ViewLocation, ViewForward))
		{
			CachedViewLocation = ViewLocation;
			CachedViewForward = ViewForward;
			bHasCachedView = true;
		}
	}

	const float ViewportX = static_cast<float>(Canvas->SizeX);
	const float ViewportY = static_cast<float>(Canvas->SizeY);

	if (Settings.bCaptionsEnabled && VisibleCaptions.Num() > 0 && ViewportX > 0.0f && ViewportY > 0.0f)
	{
		// The safe area. Overscan on a television can eat the outer few percent of the picture, and a caption
		// is exactly the thing that must not be the part that gets eaten.
		const float MarginX = ViewportX * FMath::Clamp(Settings.SafeAreaMarginPercent, 0.0f, 0.4f);
		const float MarginY = ViewportY * FMath::Clamp(Settings.SafeAreaMarginPercent, 0.0f, 0.4f);
		const FBox2D SafeArea(FVector2D(MarginX, MarginY), FVector2D(ViewportX - MarginX, ViewportY - MarginY));

		const float SafeWidth = FMath::Max(SafeArea.Max.X - SafeArea.Min.X, 1.0f);
		const float MaxLineWidth = FMath::Min(ViewportX * FMath::Clamp(Settings.MaxLineWidthPercent, 0.1f, 1.0f), SafeWidth);

		TArray<FLaidOutCaption, TInlineAllocator<8>> LaidOut;
		float TotalHeight = 0.0f;

		for (int32 Index = 0; Index < VisibleCaptions.Num(); ++Index)
		{
			FLaidOutCaption Laid;
			if (LayOutCaption(Canvas, Index, MaxLineWidth, Laid))
			{
				TotalHeight += Laid.BlockHeight;
				LaidOut.Add(MoveTemp(Laid));
			}
		}

		if (LaidOut.Num() > 0)
		{
			const float Gap = CaptionCueDraw::CaptionGap * FMath::Max(Settings.TextScale, 0.25f);
			TotalHeight += Gap * (LaidOut.Num() - 1);

			// Bottom: the block sits on the bottom of the safe area and grows upwards as more lines arrive,
			// which keeps the newest line — the one being spoken — in the same place on screen.
			float CursorY = Settings.Position == ECaptionCuePosition::Bottom
				? SafeArea.Max.Y - TotalHeight
				: SafeArea.Min.Y;

			for (const FLaidOutCaption& Laid : LaidOut)
			{
				const float OriginX = SafeArea.Min.X +
					CaptionCueDraw::JustifyOffset(Laid.Style->Justification, SafeWidth, Laid.BlockWidth);

				DrawCaptionBlock(Canvas, Laid, FVector2D(OriginX, CursorY));

				if (Settings.bDirectionIndicators && VisibleCaptions.IsValidIndex(Laid.EntryIndex))
				{
					if (DrawDirectionArrow(Canvas, VisibleCaptions[Laid.EntryIndex], Laid, SafeArea))
					{
						++Stats.ArrowsDrawn;
					}
				}

				CursorY += Laid.BlockHeight + Gap;
			}
		}
	}

	if (bShowStats)
	{
		DrawStatsBox(Canvas);
	}

	Stats.DrawMs = static_cast<float>((FPlatformTime::Seconds() - StartSeconds) * 1000.0);
}

bool UCaptionCueSubsystem::LayOutCaption(UCanvas* Canvas, int32 EntryIndex, float MaxLineWidth, FLaidOutCaption& Out) const
{
	if (!VisibleCaptions.IsValidIndex(EntryIndex))
	{
		return false;
	}

	const FCaptionCueEntry& Entry = VisibleCaptions[EntryIndex];
	const UCaptionCueSettings& Settings = UCaptionCueSettings::Get();

	const UCaptionCueStyle* Style = Entry.StyleOverride.IsNone() ? nullptr : GetStyleByName(Entry.StyleOverride);
	if (!Style)
	{
		Style = GetStyleForKind(Entry.Kind);
	}
	if (!Style)
	{
		return false;
	}

	const float Scale = FMath::Max(Settings.TextScale, 0.05f);
	const float Opacity = Style->EvaluateOpacity(Entry.Age, Entry.Duration);

	FLinearColor OutlineColor = Style->OutlineColor;
	OutlineColor.A *= Opacity;

	Out.Style = Style;
	Out.FontInfo = Style->GetFontInfo(Scale, OutlineColor, FallbackFont);
	Out.FontScale = Scale;
	Out.Opacity = Opacity;
	Out.EntryIndex = EntryIndex;

	// What the player reads: the speaker label, drawn in the speaker's colour, then the line itself.
	FText SpeakerPrefix = FText::GetEmpty();
	FLinearColor SpeakerColor = Style->DefaultSpeakerColor;

	if (Settings.bShowSpeakerNames && !Entry.Speaker.IsNone())
	{
		const FCaptionCueSpeaker Speaker = ResolveSpeaker(Entry.Speaker);
		const FText DisplayName = Entry.SpeakerDisplayName.IsEmpty() ? Speaker.DisplayName : Entry.SpeakerDisplayName;

		SpeakerPrefix = Style->FormatSpeakerPrefix(DisplayName);
		SpeakerColor = Speaker.Color;
	}

	const FText Body = Entry.Kind == ECaptionCueKind::Speech ? Entry.Text : Style->FormatNonSpeech(Entry.Text);

	FString Source = SpeakerPrefix.ToString();
	const int32 SpeakerPrefixLength = Source.Len();
	Source += Body.ToString();

	if (Source.IsEmpty())
	{
		return false;
	}

	Out.TextColor = Style->bTintLineWithSpeakerColor && !Entry.Speaker.IsNone() ? SpeakerColor : Style->TextColor;
	Out.TextColor.A *= Opacity;

	Out.SpeakerColor = SpeakerColor;
	Out.SpeakerColor.A *= Opacity;

	Out.Padding = Style->BackgroundPadding * Scale;

	Out.bDrawBackground = Style->bBackgroundBox && Settings.bBackgroundBox;
	Out.BackgroundColor = Style->BackgroundColor;
	Out.BackgroundColor.A *= FMath::Clamp(Settings.BackgroundOpacity, 0.0f, 1.0f) * Opacity;

	const float TextArea = FMath::Max(MaxLineWidth - Out.Padding.X * 2.0f, 1.0f);
	WrapLine(Source, SpeakerPrefixLength, Out.FontInfo, TextArea, Out.Lines);

	if (Out.Lines.Num() == 0)
	{
		return false;
	}

	// One measurement of the font itself, so an empty line is still a line and the block does not collapse.
	const float FontHeight = CaptionCueDraw::MeasureText(TEXT("Ag"), Out.FontInfo).Y;
	Out.LineHeight = FontHeight + Style->LineSpacing * Scale;

	Out.TextWidth = 0.0f;
	for (const FCaptionLine& Line : Out.Lines)
	{
		Out.TextWidth = FMath::Max(Out.TextWidth, Line.Width);
	}

	Out.BlockWidth = Out.TextWidth + Out.Padding.X * 2.0f;
	Out.BlockHeight = Out.LineHeight * Out.Lines.Num() + Out.Padding.Y * 2.0f;

	return true;
}

void UCaptionCueSubsystem::WrapLine(const FString& Source, int32 SpeakerPrefixLength, const FSlateFontInfo& FontInfo, float MaxWidth, TArray<FCaptionLine>& OutLines) const
{
	OutLines.Reset();

	const int32 SourceLength = Source.Len();
	if (SourceLength == 0)
	{
		return;
	}

	const TCHAR* Data = *Source;

	int32 SegmentStart = 0;

	// Newlines already in the text are hard breaks — a speaker label on its own line puts one there, and an
	// author who wrote one meant it. Everything between them is wrapped on word boundaries.
	while (SegmentStart <= SourceLength)
	{
		int32 SegmentEnd = SegmentStart;
		while (SegmentEnd < SourceLength && Data[SegmentEnd] != TEXT('\n'))
		{
			++SegmentEnd;
		}

		int32 LineStart = SegmentStart;

		do
		{
			// Skip the spaces a previous break left at the front of this line.
			while (LineStart < SegmentEnd && Data[LineStart] == TEXT(' '))
			{
				++LineStart;
			}

			int32 Accepted = INDEX_NONE;
			float AcceptedWidth = 0.0f;
			int32 Probe = LineStart;

			while (Probe < SegmentEnd)
			{
				// Grow by one word: the run of spaces in front of it plus the word itself.
				int32 WordEnd = Probe;
				while (WordEnd < SegmentEnd && Data[WordEnd] == TEXT(' '))
				{
					++WordEnd;
				}
				while (WordEnd < SegmentEnd && Data[WordEnd] != TEXT(' '))
				{
					++WordEnd;
				}

				if (WordEnd == Probe)
				{
					break;
				}

				const FStringView Candidate(Data + LineStart, WordEnd - LineStart);
				const float Width = static_cast<float>(CaptionCueDraw::MeasureText(Candidate, FontInfo).X);

				if (Width <= MaxWidth || Accepted == INDEX_NONE)
				{
					// The second half of that test is the single-long-word case: a word wider than the whole
					// line still has to be drawn, so it overflows rather than disappearing.
					Accepted = WordEnd;
					AcceptedWidth = Width;
					Probe = WordEnd;

					if (Width > MaxWidth)
					{
						break;
					}
				}
				else
				{
					break;
				}
			}

			if (Accepted == INDEX_NONE)
			{
				// An empty segment: two newlines in a row. Emit nothing and move on.
				break;
			}

			FCaptionLine& Line = OutLines.AddDefaulted_GetRef();
			Line.Text = FString::ConstructFromPtrSize(Data + LineStart, Accepted - LineStart);
			Line.Text.TrimEndInline();
			Line.Width = AcceptedWidth;

			// The speaker label always sits at the very front of the source, so how much of it landed on this
			// line is a straight index comparison. Nothing here can drift out of step with the wrapping.
			Line.SpeakerPrefixLength = FMath::Clamp(SpeakerPrefixLength - LineStart, 0, Line.Text.Len());

			if (Line.Text.Len() != Accepted - LineStart)
			{
				// Trailing spaces were trimmed, so re-measure rather than reporting a width that includes them.
				Line.Width = static_cast<float>(CaptionCueDraw::MeasureText(Line.Text, FontInfo).X);
			}

			LineStart = Accepted;
		}
		while (LineStart < SegmentEnd);

		SegmentStart = SegmentEnd + 1;
	}
}

void UCaptionCueSubsystem::DrawCaptionBlock(UCanvas* Canvas, const FLaidOutCaption& Laid, const FVector2D& Origin) const
{
	const FVector2D TextOrigin = Origin + Laid.Padding;

	// Boxes first, all of them, then the text. Two passes rather than one so that a per-line box can never
	// end up drawn over the line above it.
	if (Laid.bDrawBackground && Laid.BackgroundColor.A > 0.0f)
	{
		if (Laid.Style->bBoxPerLine)
		{
			float LineY = TextOrigin.Y;
			for (const FCaptionLine& Line : Laid.Lines)
			{
				const float LineX = TextOrigin.X +
					CaptionCueDraw::JustifyOffset(Laid.Style->Justification, Laid.TextWidth, Line.Width);

				FCanvasTileItem Box(
					FVector2D(LineX - Laid.Padding.X, LineY - Laid.Padding.Y * 0.5f),
					GWhiteTexture,
					FVector2D(Line.Width + Laid.Padding.X * 2.0f, Laid.LineHeight + Laid.Padding.Y),
					Laid.BackgroundColor);
				Box.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(Box);

				LineY += Laid.LineHeight;
			}
		}
		else
		{
			FCanvasTileItem Box(
				Origin,
				GWhiteTexture,
				FVector2D(Laid.BlockWidth, Laid.BlockHeight),
				Laid.BackgroundColor);
			Box.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Box);
		}
	}

	float LineY = TextOrigin.Y;

	for (const FCaptionLine& Line : Laid.Lines)
	{
		const float LineX = TextOrigin.X +
			CaptionCueDraw::JustifyOffset(Laid.Style->Justification, Laid.TextWidth, Line.Width);

		if (Line.SpeakerPrefixLength > 0 && Line.SpeakerPrefixLength < Line.Text.Len())
		{
			// Two draws, one line: the name in the speaker's colour, the words in the style's. This is what
			// makes a three-way conversation followable, and it is why the label is kept separate from the
			// text all the way through the wrapper rather than being glued on at the start.
			const FStringView PrefixView(*Line.Text, Line.SpeakerPrefixLength);
			const FStringView BodyView(*Line.Text + Line.SpeakerPrefixLength, Line.Text.Len() - Line.SpeakerPrefixLength);

			FCanvasTextStringViewItem PrefixItem(FVector2D(LineX, LineY), PrefixView, Laid.FontInfo, Laid.SpeakerColor);
			if (Laid.Style->bDrawShadow)
			{
				FLinearColor ShadowColor = Laid.Style->ShadowColor;
				ShadowColor.A *= Laid.Opacity;
				PrefixItem.EnableShadow(ShadowColor, Laid.Style->ShadowOffset * Laid.FontScale);
			}
			Canvas->DrawItem(PrefixItem);

			const float PrefixWidth = static_cast<float>(CaptionCueDraw::MeasureText(PrefixView, Laid.FontInfo).X);

			FCanvasTextStringViewItem BodyItem(FVector2D(LineX + PrefixWidth, LineY), BodyView, Laid.FontInfo, Laid.TextColor);
			if (Laid.Style->bDrawShadow)
			{
				FLinearColor ShadowColor = Laid.Style->ShadowColor;
				ShadowColor.A *= Laid.Opacity;
				BodyItem.EnableShadow(ShadowColor, Laid.Style->ShadowOffset * Laid.FontScale);
			}
			Canvas->DrawItem(BodyItem);
		}
		else
		{
			const FLinearColor Color = Line.SpeakerPrefixLength > 0 ? Laid.SpeakerColor : Laid.TextColor;

			FCanvasTextStringViewItem Item(FVector2D(LineX, LineY), Line.Text, Laid.FontInfo, Color);
			if (Laid.Style->bDrawShadow)
			{
				FLinearColor ShadowColor = Laid.Style->ShadowColor;
				ShadowColor.A *= Laid.Opacity;
				Item.EnableShadow(ShadowColor, Laid.Style->ShadowOffset * Laid.FontScale);
			}
			Canvas->DrawItem(Item);
		}

		LineY += Laid.LineHeight;
	}
}

bool UCaptionCueSubsystem::DrawDirectionArrow(UCanvas* Canvas, const FCaptionCueEntry& Entry, const FLaidOutCaption& Laid, const FBox2D& SafeArea) const
{
	if (!Entry.WorldLocation.IsSet() || !Laid.Style->bAllowDirectionIndicator)
	{
		return false;
	}

	FVector ViewLocation;
	FVector ViewForward;
	if (!ResolveView(Canvas, ViewLocation, ViewForward))
	{
		return false;
	}

	ViewForward = ViewForward.GetSafeNormal();

	// A roll-free camera basis. The caption text is not rolled either, so an arrow that ignored roll and text
	// that ignored roll agree with each other, which is what matters for reading it.
	const FVector UpReference = FMath::Abs(ViewForward.Z) > 0.99 ? FVector::ForwardVector : FVector::UpVector;
	const FVector ViewRight = FVector::CrossProduct(ViewForward, UpReference).GetSafeNormal();
	const FVector ViewUp = FVector::CrossProduct(ViewRight, ViewForward).GetSafeNormal();

	const FVector ToSound = *Entry.WorldLocation - ViewLocation;
	const double Forward = FVector::DotProduct(ToSound, ViewForward);

	const FVector2D ScreenCenter = (SafeArea.Min + SafeArea.Max) * 0.5f;

	// Screen space has Y pointing down, so a sound above the camera is a negative Y.
	FVector2D Direction(
		static_cast<float>(FVector::DotProduct(ToSound, ViewRight)),
		static_cast<float>(-FVector::DotProduct(ToSound, ViewUp)));

	if (Forward > 1.0)
	{
		if (!Canvas->SceneView)
		{
			// In front of the camera but there is no projection to test against, so whether it is on screen
			// cannot be known. Guessing would mean an arrow pointing at something the player can already see.
			return false;
		}

		const FVector Projected = Canvas->Project(*Entry.WorldLocation);
		const FVector2D ScreenPosition(static_cast<float>(Projected.X), static_cast<float>(Projected.Y));

		if (SafeArea.IsInside(ScreenPosition))
		{
			// Visible. A closed caption points at what the player cannot see; pointing at what they can see
			// is clutter.
			return false;
		}

		Direction = ScreenPosition - ScreenCenter;
	}

	if (Direction.IsNearlyZero())
	{
		// Directly behind the camera, dead centre. Down is the convention: it means "turn around".
		Direction = FVector2D(0.0f, 1.0f);
	}
	Direction.Normalize();

	const float ArrowSize = FMath::Max(Laid.Style->DirectionIndicatorSize * Laid.FontScale, 4.0f);

	// Walk out from the middle of the screen until the safe area's edge, inset by the arrow so the whole
	// triangle stays inside the area a television will actually show.
	const FVector2D Inset(ArrowSize, ArrowSize);
	const FVector2D BoxMin = SafeArea.Min + Inset;
	const FVector2D BoxMax = SafeArea.Max - Inset;

	if (BoxMin.X >= BoxMax.X || BoxMin.Y >= BoxMax.Y)
	{
		return false;
	}

	float Distance = TNumericLimits<float>::Max();

	if (FMath::Abs(Direction.X) > KINDA_SMALL_NUMBER)
	{
		const float Edge = Direction.X > 0.0f ? BoxMax.X : BoxMin.X;
		Distance = FMath::Min(Distance, (Edge - ScreenCenter.X) / Direction.X);
	}
	if (FMath::Abs(Direction.Y) > KINDA_SMALL_NUMBER)
	{
		const float Edge = Direction.Y > 0.0f ? BoxMax.Y : BoxMin.Y;
		Distance = FMath::Min(Distance, (Edge - ScreenCenter.Y) / Direction.Y);
	}

	if (Distance <= 0.0f || Distance == TNumericLimits<float>::Max())
	{
		return false;
	}

	const FVector2D Tip = ScreenCenter + Direction * Distance;
	const FVector2D Back = Tip - Direction * ArrowSize;
	const FVector2D Side(-Direction.Y, Direction.X);

	FLinearColor ArrowColor = Laid.Style->DirectionIndicatorColor;
	ArrowColor.A *= Laid.Opacity;

	// Drawn from the white texture rather than an image asset: no texture ships with the plugin, so there is
	// nothing to cook, nothing to reference and nothing that can go missing in a customer's project.
	FCanvasTriangleItem Arrow(
		Tip,
		Back + Side * (ArrowSize * 0.45f),
		Back - Side * (ArrowSize * 0.45f),
		GWhiteTexture);
	Arrow.BlendMode = SE_BLEND_Translucent;
	Arrow.SetColor(ArrowColor);
	Canvas->DrawItem(Arrow);

	return true;
}

void UCaptionCueSubsystem::DrawStatsBox(UCanvas* Canvas) const
{
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (!Font)
	{
		return;
	}

	const UCaptionCueSettings& Settings = UCaptionCueSettings::Get();

	constexpr float BoxX = 24.0f;
	constexpr float BoxY = 90.0f;
	constexpr float BoxWidth = 300.0f;
	constexpr float LineHeight = 15.0f;
	constexpr int32 LineCount = 9;

	FCanvasTileItem Background(
		FVector2D(BoxX - 8.0f, BoxY - 8.0f),
		GWhiteTexture,
		FVector2D(BoxWidth, LineCount * LineHeight + 16.0f),
		FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
	Background.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Background);

	float LineY = BoxY;

	auto DrawLine = [&](FStringView Line, const FLinearColor& Color)
	{
		FCanvasTextStringViewItem Item(FVector2D(BoxX, LineY), Line, Font, Color);
		Canvas->DrawItem(Item);
		LineY += LineHeight;
	};

	const FLinearColor Heading(0.55f, 0.85f, 1.0f, 1.0f);
	const FLinearColor Body(0.9f, 0.9f, 0.9f, 1.0f);

	TStringBuilder<160> Line;

	Line.Reset();
	Line.Append(TEXT("CaptionCue"));
	DrawLine(Line.ToView(), Heading);

	Line.Reset();
	Line.Appendf(TEXT("Visible           %d / %d"), Stats.Visible, Settings.MaxVisibleLines);
	DrawLine(Line.ToView(), Body);

	Line.Reset();
	Line.Appendf(TEXT("Queued            %d / %d"), Stats.Pending, Settings.MaxQueuedCaptions);
	DrawLine(Line.ToView(), Body);

	Line.Reset();
	Line.Appendf(TEXT("Total queued      %d  (dropped %d)"), Stats.TotalQueued, Stats.TotalDropped);
	DrawLine(Line.ToView(), Body);

	Line.Reset();
	Line.Appendf(TEXT("Displaced         %d"), Stats.TotalDisplaced);
	DrawLine(Line.ToView(), Body);

	Line.Reset();
	Line.Appendf(TEXT("Engine bridge     %s  (%d line%s)"),
		IsEngineBridgeAttached() ? TEXT("attached") : TEXT("off"),
		Stats.TotalFromEngineBridge,
		Stats.TotalFromEngineBridge == 1 ? TEXT("") : TEXT("s"));
	DrawLine(Line.ToView(), Body);

	Line.Reset();
	Line.Appendf(TEXT("Text scale        %.2f   box %s   names %s"),
		Settings.TextScale,
		Settings.bBackgroundBox ? TEXT("on") : TEXT("off"),
		Settings.bShowSpeakerNames ? TEXT("on") : TEXT("off"));
	DrawLine(Line.ToView(), Body);

	Line.Reset();
	Line.Appendf(TEXT("Sound captions    %s   arrows %s (%d drawn)"),
		Settings.bShowSoundCaptions ? TEXT("on") : TEXT("off"),
		Settings.bDirectionIndicators ? TEXT("on") : TEXT("off"),
		Stats.ArrowsDrawn);
	DrawLine(Line.ToView(), Body);

	Line.Reset();
	Line.Appendf(TEXT("Draw              %.3f ms"), Stats.DrawMs);
	DrawLine(Line.ToView(), Body);
}

#if WITH_EDITOR
void UCaptionCueSubsystem::OnEditorPreviewDraw(UCanvas* Canvas, APlayerController* PlayerController)
{
	if (!Canvas || !UCaptionCueSettings::Get().bEnableEditorPreview)
	{
		return;
	}

	// A real HUD pass has already drawn this frame — that one wins, always.
	if (LastGameDrawFrame == GFrameCounter)
	{
		return;
	}

	// The debug draw service is engine-wide: every world's caption layer is called for every viewport that
	// draws. Work out which world this viewport is showing and stay out of the other ones.
	UWorld* ViewWorld = nullptr;
	if (Canvas->SceneView && Canvas->SceneView->Family && Canvas->SceneView->Family->Scene)
	{
		ViewWorld = Canvas->SceneView->Family->Scene->GetWorld();
	}
	if (!ViewWorld && PlayerController)
	{
		ViewWorld = PlayerController->GetWorld();
	}

	if (ViewWorld && ViewWorld != GetWorld())
	{
		return;
	}

	DrawPass(Canvas);
}
#endif

#undef LOCTEXT_NAMESPACE

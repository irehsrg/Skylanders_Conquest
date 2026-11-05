// Skylanders Conquest - Character Select Screen Implementation

#include "SkylandersCharacterSelectWidget.h"
#include "SkylandersCharacterPreviewActor.h"
#include "SkylandersGameInstance.h"
#include "SkylandersUIStyle.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequenceBase.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

// Tile fills on the shared palette; outlines come from SkylandersUI::PanelOutline
// (idle) and the entry's Accent (selected).
static const FLinearColor SelTile(0.06f, 0.08f, 0.15f, 1.0f);
static const FLinearColor SelTileHover(0.12f, 0.15f, 0.27f, 1.0f);
static const FLinearColor SelTileLocked(0.04f, 0.05f, 0.09f, 0.8f);
static constexpr float SelTileRadius = 10.0f;

const TArray<FSkylanderSelectEntry>& FSkylanderSelectEntry::GetRoster()
{
	static TArray<FSkylanderSelectEntry> Roster;
	if (Roster.Num() == 0)
	{
		FSkylanderSelectEntry TH;
		TH.ID = TEXT("TriggerHappy");
		TH.DisplayName = TEXT("TRIGGER HAPPY");
		TH.Role = TEXT("Gunslinger  ·  Ranged Carry");
		TH.Accent = SkylandersUI::Gold;
		TH.StatLine = TEXT("HP 140   MP 130   ·   Fast attacks, gold-fueled burst");
		TH.AutoAttack = TEXT("Dual golden pistols — rapid alternating shots.");
		TH.Abilities[0] = TEXT("Gatling Gun|Piercing golden beam that damages everything in a line. (350% power)");
		TH.Abilities[1] = TEXT("Pot o' Gold|Lob a pot that explodes at the target area. (400% power)");
		TH.Abilities[2] = TEXT("Golden Machine Gun|Plant and unleash a storm of bullets. (50% power per shot)");
		TH.Abilities[3] = TEXT("Yamato Cannon  [ULT]|Charge up, then obliterate everything in a frontal cone. (2000% power)");
		TH.MeshPath = TEXT("/Game/Characters/TriggerHappy/Models/TriggerHappy");
		TH.IdleAnimPath = TEXT("/Game/Characters/TriggerHappy/Animations/idle");
		TH.MeshScale = 1.0f;
		TH.MeshZOffset = 0.0f;      // Pivot at feet
		TH.CaptureDistance = 300.0f;
		TH.AimHeight = 45.0f;
		Roster.Add(TH);

		FSkylanderSelectEntry Hex;
		Hex.ID = TEXT("Hex");
		Hex.DisplayName = TEXT("HEX");
		Hex.Role = TEXT("Undead Sorceress  ·  Mage");
		Hex.Accent = FLinearColor(0.70f, 0.35f, 1.0f, 1.0f);
		Hex.StatLine = TEXT("HP 124   MP 174   ·   Fragile, devastating spell damage");
		Hex.AutoAttack = TEXT("Phantom bolts — slow, hard-hitting magic.");
		Hex.Abilities[0] = TEXT("Phantom Orb|Piercing orb of spectral energy. (300% power)");
		Hex.Abilities[1] = TEXT("Wall of Bones|Raise a blocking wall of bone at the target point.");
		Hex.Abilities[2] = TEXT("Rain of Skulls|Skulls crash down on an area after a short delay. (450% power)");
		Hex.Abilities[3] = TEXT("Skull Storm  [ULT]|Unleash a massive nova of skulls around Hex. (1000% power)");
		Hex.MeshPath = TEXT("/Game/Characters/Hex/Models/Hex");
		Hex.IdleAnimPath = TEXT("/Game/Characters/Hex/Animations/drive_idle");
		Hex.MeshScale = 1.0f;
		Hex.MeshZOffset = 47.0f;    // Pivot at center; lift so she hovers above the floor
		Hex.CaptureDistance = 340.0f;
		Hex.AimHeight = 50.0f;
		Roster.Add(Hex);

		FSkylanderSelectEntry TR;
		TR.ID = TEXT("TreeRex");
		TR.DisplayName = TEXT("TREE REX");
		TR.Role = TEXT("Giant Guardian  ·  Tank / Support");
		TR.Accent = FLinearColor(0.35f, 0.85f, 0.30f, 1.0f);
		TR.StatLine = TEXT("HP 233   MP 104   ·   Massive frontline with self-sustain");
		TR.AutoAttack = TEXT("Melee cleave — smashes everything in front (structures too).");
		TR.Abilities[0] = TEXT("Shockwave Slam|Smash the ground, damaging everything around. (240% power)");
		TR.Abilities[1] = TEXT("Barkskin|Harden your bark: +30–80 protections for 6 seconds.");
		TR.Abilities[2] = TEXT("Healing Grove|Regenerate up to 24% max HP over 4 seconds.");
		TR.Abilities[3] = TEXT("Titan's Wrath  [ULT]|Colossal cone smash; heals 6% HP per target hit. (1200% power)");
		TR.MeshPath = TEXT("/Game/Characters/TreeRex/Models/TreeRex");
		TR.IdleAnimPath = TEXT("/Game/Characters/TreeRex/Animations/sequoiastampede_outwall");
		TR.MeshScale = 1.0f;        // Animations are authored at game scale (ground at origin)
		TR.MeshZOffset = 0.0f;
		TR.CaptureDistance = 420.0f;
		TR.AimHeight = 80.0f;
		Roster.Add(TR);

		FSkylanderSelectEntry Spyro;
		Spyro.ID = TEXT("Spyro");
		Spyro.DisplayName = TEXT("SPYRO");
		Spyro.Role = TEXT("Purple Dragon  ·  Bruiser");
		Spyro.Accent = FLinearColor(0.55f, 0.30f, 0.95f, 1.0f);
		Spyro.StatLine = TEXT("HP 172   MP 130   ·   Mobile brawler, fire and charges");
		Spyro.AutoAttack = TEXT("Flameball — lobbed fireballs at a steady clip.");
		Spyro.Abilities[0] = TEXT("Flame Breath|Breathe a cone of fire in front of Spyro. (220% power)");
		Spyro.Abilities[1] = TEXT("Charge|Horns down: dash forward, ramming everything in the path, then +25% speed for 2s. (200% power)");
		Spyro.Abilities[2] = TEXT("Comet Dash|Leap to the target area and crash down on it. (280% power)");
		Spyro.Abilities[3] = TEXT("Daybringer Flame  [ULT]|Rear up for a second, then scorch a huge frontal cone. (900% power)");
		Spyro.MeshPath = TEXT("/Game/Characters/Spyro/Models/Spyro");
		Spyro.IdleAnimPath = TEXT("/Game/Characters/Spyro/Animations/drive_modswap_pose01");
		Spyro.MeshScale = 0.52f;    // Same scale as the in-match model (AI table / player pawn)
		Spyro.MeshZOffset = 0.0f;   // Animations put the ground at the skeleton origin
		Spyro.CaptureDistance = 240.0f; // Small quadruped: pull the camera in
		Spyro.AimHeight = 25.0f;
		Roster.Add(Spyro);
	}
	return Roster;
}

// Left-justified body text, optionally wrapped (the info panel's paragraphs)
static UTextBlock* SelMakeText(UWidgetTree* Tree, const FString& Str, int32 Size, FLinearColor Color, bool bWrap = false)
{
	UTextBlock* TB = SkylandersUI::MakeText(Tree, NAME_None, Str, Size, Color, ETextJustify::Left);
	if (TB && bWrap) TB->SetAutoWrapText(true);
	return TB;
}

// A tile's button style: a rounded fill per state; the selected tile swaps the
// 1px panel rim for a 2px rim in its Accent (see UpdateTileHighlights)
static FButtonStyle SelTileStyle(FButtonStyle Style, const FLinearColor& Accent, bool bSelected)
{
	const FLinearColor Rim = bSelected ? Accent : SkylandersUI::PanelOutline;
	const float RimWidth = bSelected ? 2.0f : 1.0f;
	const FLinearColor Fill = bSelected ? FMath::Lerp(SelTile, Accent, 0.18f) : SelTile;
	Style.Normal = SkylandersUI::RoundedBrush(Fill, SelTileRadius, Rim, RimWidth);
	Style.Hovered = SkylandersUI::RoundedBrush(FMath::Lerp(SelTileHover, Accent, bSelected ? 0.22f : 0.08f), SelTileRadius, Rim, RimWidth);
	Style.Pressed = SkylandersUI::RoundedBrush(FMath::Lerp(SelTileHover, Accent, 0.35f), SelTileRadius, Accent, 2.0f);
	Style.Disabled = SkylandersUI::RoundedBrush(SelTileLocked, SelTileRadius, SkylandersUI::PanelOutline * FLinearColor(1.0f, 1.0f, 1.0f, 0.5f), 1.0f);
	return Style;
}

UButton* USkylandersCharacterSelectWidget::MakeTile(const FSkylanderSelectEntry& Entry, int32 Index)
{
	UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),
		FName(*FString::Printf(TEXT("Tile_%s"), *Entry.ID.ToString())));
	Btn->SetStyle(SelTileStyle(Btn->GetStyle(), Entry.Accent, false));

	UVerticalBox* Inner = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

	// Short Accent bar: identifies the character before the name is read
	{
		UBorder* Bar = SkylandersUI::MakePanel(WidgetTree, NAME_None, Entry.Accent, 2.0f, FLinearColor::Transparent, 0.0f, FMargin(0.0f));
		USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BarBox->SetWidthOverride(36.0f);
		BarBox->SetHeightOverride(4.0f);
		BarBox->AddChild(Bar);
		UVerticalBoxSlot* BarSlot = Inner->AddChildToVerticalBox(BarBox);
		BarSlot->SetHorizontalAlignment(HAlign_Center);
		BarSlot->SetPadding(FMargin(6.f, 20.f, 6.f, 8.f));
	}

	UTextBlock* Name = SkylandersUI::MakeTitle(WidgetTree, NAME_None, Entry.DisplayName, 19, SkylandersUI::White);
	Inner->AddChildToVerticalBox(Name)->SetPadding(FMargin(6.f, 0.f, 6.f, 2.f));
	UTextBlock* Role = SkylandersUI::MakeText(WidgetTree, NAME_None, Entry.Role, 11, SkylandersUI::Dim);
	Role->SetAutoWrapText(true);
	Inner->AddChildToVerticalBox(Role)->SetPadding(FMargin(6.f, 0.f, 6.f, 20.f));
	Btn->AddChild(Inner);

	return Btn;
}

void USkylandersCharacterSelectWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree) return;

	const TArray<FSkylanderSelectEntry>& Roster = FSkylanderSelectEntry::GetRoster();

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SelectRoot"));
	WidgetTree->RootWidget = RootCanvas;

	// Opaque backdrop
	UBorder* BG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SelectBG"));
	BG->SetBrushColor(SkylandersUI::ScreenBG);
	UCanvasPanelSlot* BGSlot = RootCanvas->AddChildToCanvas(BG);
	BGSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	BGSlot->SetOffsets(FMargin(0));

	// Title
	UTextBlock* Title = SkylandersUI::MakeTitle(WidgetTree, TEXT("SelectTitle"), TEXT("CHOOSE YOUR SKYLANDER"), 34, SkylandersUI::Gold);
	UCanvasPanelSlot* TitleSlot = RootCanvas->AddChildToCanvas(Title);
	TitleSlot->SetAnchors(FAnchors(0.5f, 0.045f, 0.5f, 0.045f));
	TitleSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	TitleSlot->SetAutoSize(true);

	// ---- Left third: roster grid ----
	UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("RosterGrid"));
	Grid->SetSlotPadding(FMargin(6.f));
	UCanvasPanelSlot* GridSlot = RootCanvas->AddChildToCanvas(Grid);
	GridSlot->SetAnchors(FAnchors(0.02f, 0.12f, 0.34f, 0.86f));
	GridSlot->SetOffsets(FMargin(0));

	TileButtons.Reset();
	for (int32 i = 0; i < Roster.Num(); i++)
	{
		UButton* Tile = MakeTile(Roster[i], i);
		UUniformGridSlot* GS = Grid->AddChildToUniformGrid(Tile, i / 2, i % 2);
		GS->SetHorizontalAlignment(HAlign_Fill);
		GS->SetVerticalAlignment(VAlign_Fill);
		TileButtons.Add(Tile);
	}
	// AddDynamic needs literal function names - bind each roster slot
	if (TileButtons.Num() > 0) TileButtons[0]->OnClicked.AddDynamic(this, &USkylandersCharacterSelectWidget::OnTile0);
	if (TileButtons.Num() > 1) TileButtons[1]->OnClicked.AddDynamic(this, &USkylandersCharacterSelectWidget::OnTile1);
	if (TileButtons.Num() > 2) TileButtons[2]->OnClicked.AddDynamic(this, &USkylandersCharacterSelectWidget::OnTile2);
	if (TileButtons.Num() > 3) TileButtons[3]->OnClicked.AddDynamic(this, &USkylandersCharacterSelectWidget::OnTile3);

	// Locked placeholder tiles to fill the grid (future roster)
	for (int32 i = Roster.Num(); i < 6; i++)
	{
		FSkylanderSelectEntry Locked;
		Locked.ID = *FString::Printf(TEXT("Locked%d"), i);
		Locked.DisplayName = TEXT("???");
		Locked.Role = TEXT("Locked");
		Locked.Accent = FLinearColor(0.3f, 0.3f, 0.35f, 1.0f);
		UButton* Tile = MakeTile(Locked, i);
		Tile->SetIsEnabled(false);
		UUniformGridSlot* GS = Grid->AddChildToUniformGrid(Tile, i / 2, i % 2);
		GS->SetHorizontalAlignment(HAlign_Fill);
		GS->SetVerticalAlignment(VAlign_Fill);
	}

	// ---- Center: live 3D preview on an Accent-tinted backdrop. The stage has
	// no ring of its own, so the backdrop's tint and rim carry the Accent
	// (re-tinted by SelectCharacter). ----
	PreviewBackdrop = SkylandersUI::MakePanel(WidgetTree, TEXT("PreviewBackdrop"), SkylandersUI::PanelBG, 16.f,
		SkylandersUI::PanelOutline, 1.f, FMargin(6.f));
	PreviewBackdrop->SetHorizontalAlignment(HAlign_Fill);
	PreviewBackdrop->SetVerticalAlignment(VAlign_Fill);
	UCanvasPanelSlot* BackdropSlot = RootCanvas->AddChildToCanvas(PreviewBackdrop);
	BackdropSlot->SetAnchors(FAnchors(0.36f, 0.10f, 0.66f, 0.86f));
	BackdropSlot->SetOffsets(FMargin(0));

	PreviewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PreviewImage"));
	PreviewBackdrop->SetContent(PreviewImage);

	// ---- Right third: info panel ----
	UBorder* InfoBG = SkylandersUI::MakePanel(WidgetTree, TEXT("InfoBG"), SkylandersUI::PanelBG, 12.f,
		SkylandersUI::PanelOutline, 1.f, FMargin(20.f, 18.f));
	UCanvasPanelSlot* InfoSlot = RootCanvas->AddChildToCanvas(InfoBG);
	InfoSlot->SetAnchors(FAnchors(0.68f, 0.10f, 0.98f, 0.86f));
	InfoSlot->SetOffsets(FMargin(0));

	InfoPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InfoPanel"));
	InfoBG->SetContent(InfoPanel);

	// ---- Bottom bar: back + play ----
	UButton* BackBtn = SkylandersUI::MakeButton(WidgetTree, TEXT("BackBtn"), TEXT("BACK"), 18);
	BackBtn->OnClicked.AddDynamic(this, &USkylandersCharacterSelectWidget::OnBack);
	UCanvasPanelSlot* BackSlot = RootCanvas->AddChildToCanvas(BackBtn);
	BackSlot->SetAnchors(FAnchors(0.02f, 0.90f, 0.14f, 0.96f));
	BackSlot->SetOffsets(FMargin(0));

	// PLAY is the primary action: gold rim, bold gold label naming the pick
	UButton* LockBtn = SkylandersUI::MakeButton(WidgetTree, TEXT("LockInBtn"), TEXT("PLAY"), 24);
	SkylandersUI::SetButtonHighlight(LockBtn, true);
	LockInLabel = SkylandersUI::GetButtonLabel(LockBtn);
	if (LockInLabel)
	{
		LockInLabel->SetColorAndOpacity(FSlateColor(SkylandersUI::Gold));
		SkylandersUI::SetFontStyle(LockInLabel, true, false);
	}
	LockBtn->OnClicked.AddDynamic(this, &USkylandersCharacterSelectWidget::OnLockIn);
	UCanvasPanelSlot* LockSlot = RootCanvas->AddChildToCanvas(LockBtn);
	LockSlot->SetAnchors(FAnchors(0.68f, 0.885f, 0.98f, 0.965f));
	LockSlot->SetOffsets(FMargin(0));
}

void USkylandersCharacterSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Spawn the off-screen preview stage far below the menu level
	if (!PreviewActor && GetWorld())
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		PreviewActor = GetWorld()->SpawnActor<ASkylandersCharacterPreviewActor>(
			ASkylandersCharacterPreviewActor::StaticClass(),
			FVector(0.0f, 0.0f, -5000.0f), FRotator::ZeroRotator, Params);
	}

	// Wire the render target into the preview image
	if (PreviewActor && PreviewImage && PreviewActor->GetRenderTarget())
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(PreviewActor->GetRenderTarget());
		Brush.ImageSize = FVector2D(768.0f, 1024.0f);
		PreviewImage->SetBrush(Brush);
	}

	// Default to the current selection (or Trigger Happy)
	FName Initial = TEXT("TriggerHappy");
	if (USkylandersGameInstance* GI = GetGameInstance<USkylandersGameInstance>())
	{
		Initial = GI->SelectedCharacterID;
	}
	SelectCharacter(Initial);
}

void USkylandersCharacterSelectWidget::NativeDestruct()
{
	if (PreviewActor)
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}
	Super::NativeDestruct();
}

void USkylandersCharacterSelectWidget::SelectCharacter(FName CharacterID)
{
	const TArray<FSkylanderSelectEntry>& Roster = FSkylanderSelectEntry::GetRoster();
	const FSkylanderSelectEntry* Entry = Roster.FindByPredicate(
		[&](const FSkylanderSelectEntry& E) { return E.ID == CharacterID; });
	if (!Entry) return;

	SelectedID = CharacterID;
	RebuildInfoPanel(*Entry);
	UpdatePreview(*Entry);
	UpdateTileHighlights();

	if (LockInLabel)
	{
		LockInLabel->SetText(FText::FromString(FString::Printf(TEXT("PLAY  ·  %s"), *Entry->DisplayName)));
	}

	// The preview stage has no ring, so its backdrop carries the Accent: a dark
	// tint of it behind the model and a strong rim
	if (PreviewBackdrop)
	{
		const FLinearColor Tint = FMath::Lerp(SkylandersUI::PanelBG, Entry->Accent, 0.16f);
		PreviewBackdrop->SetBrush(SkylandersUI::RoundedBrush(Tint, 16.f, Entry->Accent * FLinearColor(1, 1, 1, 0.7f), 1.5f));
	}
}

void USkylandersCharacterSelectWidget::RebuildInfoPanel(const FSkylanderSelectEntry& Entry)
{
	if (!InfoPanel) return;
	InfoPanel->ClearChildren();

	UTextBlock* Name = SkylandersUI::MakeTitle(WidgetTree, NAME_None, Entry.DisplayName, 30, Entry.Accent, ETextJustify::Left);
	InfoPanel->AddChildToVerticalBox(Name)->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));

	UTextBlock* Role = SelMakeText(WidgetTree, Entry.Role, 14, SkylandersUI::Dim);
	InfoPanel->AddChildToVerticalBox(Role)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));

	// Stat line in its own chip so it reads as data, not prose
	{
		UBorder* Chip = SkylandersUI::MakePanel(WidgetTree, NAME_None, FLinearColor(0.05f, 0.06f, 0.12f, 1.0f), 6.f,
			SkylandersUI::PanelOutline, 1.f, FMargin(10.f, 6.f));
		Chip->SetContent(SelMakeText(WidgetTree, Entry.StatLine, 12, SkylandersUI::White, true));
		InfoPanel->AddChildToVerticalBox(Chip)->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
	}

	UTextBlock* AAHeader = SkylandersUI::MakeTitle(WidgetTree, NAME_None, TEXT("BASIC ATTACK"), 13, SkylandersUI::Gold, ETextJustify::Left);
	InfoPanel->AddChildToVerticalBox(AAHeader)->SetPadding(FMargin(0.f, 0.f, 0.f, 3.f));
	UTextBlock* AAText = SelMakeText(WidgetTree, Entry.AutoAttack, 12, SkylandersUI::White, true);
	InfoPanel->AddChildToVerticalBox(AAText)->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));

	UTextBlock* AbHeader = SkylandersUI::MakeTitle(WidgetTree, NAME_None, TEXT("ABILITIES"), 13, SkylandersUI::Gold, ETextJustify::Left);
	InfoPanel->AddChildToVerticalBox(AbHeader)->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

	for (int32 i = 0; i < 4; i++)
	{
		FString AbilityName, AbilityDesc;
		if (!Entry.Abilities[i].Split(TEXT("|"), &AbilityName, &AbilityDesc))
		{
			AbilityName = Entry.Abilities[i];
		}

		// Key badge 1-4 (the HUD's ability-slot badge, rimmed in the Accent),
		// then the name over "- description"
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UHorizontalBoxSlot* BadgeSlot = Row->AddChildToHorizontalBox(
			SkylandersUI::MakeKeyBadge(WidgetTree, FString::FromInt(i + 1), 24.f, Entry.Accent));
		BadgeSlot->SetVerticalAlignment(VAlign_Top);
		BadgeSlot->SetPadding(FMargin(0.f, 1.f, 10.f, 0.f));

		UVerticalBox* Text = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UTextBlock* AbName = SkylandersUI::MakeTitle(WidgetTree, NAME_None, AbilityName, 14, Entry.Accent, ETextJustify::Left);
		Text->AddChildToVerticalBox(AbName);
		if (!AbilityDesc.IsEmpty())
		{
			UTextBlock* AbDesc = SelMakeText(WidgetTree, FString::Printf(TEXT("— %s"), *AbilityDesc), 12, SkylandersUI::White, true);
			Text->AddChildToVerticalBox(AbDesc)->SetPadding(FMargin(0.f, 1.f, 0.f, 0.f));
		}
		UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(Text);
		TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		InfoPanel->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 6.f, 0.f, 2.f));
	}
}

void USkylandersCharacterSelectWidget::UpdatePreview(const FSkylanderSelectEntry& Entry)
{
	if (!PreviewActor) return;

	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *Entry.MeshPath);
	UAnimSequenceBase* Idle = LoadObject<UAnimSequenceBase>(nullptr, *Entry.IdleAnimPath);
	if (Mesh)
	{
		PreviewActor->SetPreview(Mesh, Idle, Entry.MeshScale, Entry.MeshZOffset,
			Entry.CaptureDistance, Entry.AimHeight);
	}
}

void USkylandersCharacterSelectWidget::UpdateTileHighlights()
{
	const TArray<FSkylanderSelectEntry>& Roster = FSkylanderSelectEntry::GetRoster();
	for (int32 i = 0; i < TileButtons.Num() && i < Roster.Num(); i++)
	{
		if (!TileButtons[i]) continue;
		const bool bSelected = (Roster[i].ID == SelectedID);
		TileButtons[i]->SetStyle(SelTileStyle(TileButtons[i]->GetStyle(), Roster[i].Accent, bSelected));
	}
}

void USkylandersCharacterSelectWidget::OnTile0() { SelectCharacter(FSkylanderSelectEntry::GetRoster()[0].ID); }
void USkylandersCharacterSelectWidget::OnTile1() { SelectCharacter(FSkylanderSelectEntry::GetRoster()[1].ID); }
void USkylandersCharacterSelectWidget::OnTile2() { SelectCharacter(FSkylanderSelectEntry::GetRoster()[2].ID); }
void USkylandersCharacterSelectWidget::OnTile3() { SelectCharacter(FSkylanderSelectEntry::GetRoster()[3].ID); }

void USkylandersCharacterSelectWidget::OnLockIn()
{
	if (SelectedID.IsNone()) return;

	if (USkylandersGameInstance* GI = GetGameInstance<USkylandersGameInstance>())
	{
		GI->SelectedCharacterID = SelectedID;
	}

	// FInputModeUIOnly survives OpenLevel — restore game input before travel
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
	UGameplayStatics::OpenLevel(this, GameLevelName);
}

void USkylandersCharacterSelectWidget::OnBack()
{
	RemoveFromParent(); // NativeDestruct cleans up the preview stage
}

// Skylanders Conquest - Character Select Screen Implementation

#include "SkylandersCharacterSelectWidget.h"
#include "SkylandersCharacterPreviewActor.h"
#include "SkylandersGameInstance.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequenceBase.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

// ---- Style constants (shared look with the main menu) ----
static const FLinearColor SelBG(0.012f, 0.016f, 0.04f, 1.0f);
static const FLinearColor SelGold(0.95f, 0.78f, 0.20f, 1.0f);
static const FLinearColor SelWhite(0.92f, 0.92f, 0.95f, 1.0f);
static const FLinearColor SelGrey(0.62f, 0.62f, 0.70f, 1.0f);
static const FLinearColor SelTile(0.06f, 0.08f, 0.15f, 1.0f);
static const FLinearColor SelTileHover(0.14f, 0.18f, 0.32f, 1.0f);

const TArray<FSkylanderSelectEntry>& FSkylanderSelectEntry::GetRoster()
{
	static TArray<FSkylanderSelectEntry> Roster;
	if (Roster.Num() == 0)
	{
		FSkylanderSelectEntry TH;
		TH.ID = TEXT("TriggerHappy");
		TH.DisplayName = TEXT("TRIGGER HAPPY");
		TH.Role = TEXT("Gunslinger  ·  Ranged Carry");
		TH.Accent = SelGold;
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
		TR.IdleAnimPath = TEXT("/Game/Characters/TreeRex/Animations/magicmoment_failloop");
		TR.MeshScale = 0.5f;
		TR.MeshZOffset = 117.0f;    // Pivot at center of the (half-scaled) giant
		TR.CaptureDistance = 500.0f;
		TR.AimHeight = 117.0f;
		Roster.Add(TR);
	}
	return Roster;
}

static UTextBlock* SelMakeText(UWidgetTree* Tree, const FString& Str, int32 Size, FLinearColor Color, bool bWrap = false)
{
	UTextBlock* TB = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TB->SetText(FText::FromString(Str));
	FSlateFontInfo Font = TB->GetFont();
	Font.Size = Size;
	TB->SetFont(Font);
	TB->SetColorAndOpacity(FSlateColor(Color));
	if (bWrap) TB->SetAutoWrapText(true);
	return TB;
}

UButton* USkylandersCharacterSelectWidget::MakeTile(const FSkylanderSelectEntry& Entry, int32 Index)
{
	UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),
		FName(*FString::Printf(TEXT("Tile_%s"), *Entry.ID.ToString())));

	FButtonStyle Style = Btn->GetStyle();
	auto SetBrush = [](FSlateBrush& Brush, FLinearColor Color)
	{
		Brush.TintColor = FSlateColor(Color);
		Brush.DrawAs = ESlateBrushDrawType::Box;
	};
	SetBrush(Style.Normal, SelTile);
	SetBrush(Style.Hovered, SelTileHover);
	SetBrush(Style.Pressed, Entry.Accent * 0.5f);
	Btn->SetStyle(Style);

	UVerticalBox* Inner = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock* Name = SelMakeText(WidgetTree, Entry.DisplayName, 19, Entry.Accent);
	Name->SetJustification(ETextJustify::Center);
	Inner->AddChildToVerticalBox(Name)->SetPadding(FMargin(6.f, 22.f, 6.f, 2.f));
	UTextBlock* Role = SelMakeText(WidgetTree, Entry.Role, 11, SelGrey);
	Role->SetJustification(ETextJustify::Center);
	Inner->AddChildToVerticalBox(Role)->SetPadding(FMargin(6.f, 0.f, 6.f, 22.f));
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
	BG->SetBrushColor(SelBG);
	UCanvasPanelSlot* BGSlot = RootCanvas->AddChildToCanvas(BG);
	BGSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	BGSlot->SetOffsets(FMargin(0));

	// Title
	UTextBlock* Title = SelMakeText(WidgetTree, TEXT("CHOOSE YOUR SKYLANDER"), 34, SelGold);
	Title->SetJustification(ETextJustify::Center);
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
	// AddDynamic needs literal function names — bind the three roster slots
	if (TileButtons.Num() > 0) TileButtons[0]->OnClicked.AddDynamic(this, &USkylandersCharacterSelectWidget::OnTile0);
	if (TileButtons.Num() > 1) TileButtons[1]->OnClicked.AddDynamic(this, &USkylandersCharacterSelectWidget::OnTile1);
	if (TileButtons.Num() > 2) TileButtons[2]->OnClicked.AddDynamic(this, &USkylandersCharacterSelectWidget::OnTile2);

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

	// ---- Center: live 3D preview ----
	PreviewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PreviewImage"));
	UCanvasPanelSlot* PrevSlot = RootCanvas->AddChildToCanvas(PreviewImage);
	PrevSlot->SetAnchors(FAnchors(0.36f, 0.10f, 0.66f, 0.90f));
	PrevSlot->SetOffsets(FMargin(0));

	// ---- Right third: info panel ----
	UBorder* InfoBG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InfoBG"));
	InfoBG->SetBrushColor(FLinearColor(0.03f, 0.04f, 0.09f, 0.9f));
	InfoBG->SetPadding(FMargin(18.f));
	UCanvasPanelSlot* InfoSlot = RootCanvas->AddChildToCanvas(InfoBG);
	InfoSlot->SetAnchors(FAnchors(0.68f, 0.10f, 0.98f, 0.86f));
	InfoSlot->SetOffsets(FMargin(0));

	InfoPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InfoPanel"));
	InfoBG->SetContent(InfoPanel);

	// ---- Bottom bar: back + lock in ----
	UButton* BackBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("BackBtn"));
	{
		FButtonStyle Style = BackBtn->GetStyle();
		auto SetBrush = [](FSlateBrush& Brush, FLinearColor Color)
		{
			Brush.TintColor = FSlateColor(Color);
			Brush.DrawAs = ESlateBrushDrawType::Box;
		};
		SetBrush(Style.Normal, SelTile);
		SetBrush(Style.Hovered, SelTileHover);
		SetBrush(Style.Pressed, SelTileHover);
		BackBtn->SetStyle(Style);
	}
	UTextBlock* BackText = SelMakeText(WidgetTree, TEXT("BACK"), 18, SelWhite);
	BackText->SetJustification(ETextJustify::Center);
	BackBtn->AddChild(BackText);
	BackBtn->OnClicked.AddDynamic(this, &USkylandersCharacterSelectWidget::OnBack);
	UCanvasPanelSlot* BackSlot = RootCanvas->AddChildToCanvas(BackBtn);
	BackSlot->SetAnchors(FAnchors(0.02f, 0.90f, 0.14f, 0.96f));
	BackSlot->SetOffsets(FMargin(0));

	UButton* LockBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("LockInBtn"));
	{
		FButtonStyle Style = LockBtn->GetStyle();
		auto SetBrush = [](FSlateBrush& Brush, FLinearColor Color)
		{
			Brush.TintColor = FSlateColor(Color);
			Brush.DrawAs = ESlateBrushDrawType::Box;
		};
		SetBrush(Style.Normal, FLinearColor(0.55f, 0.42f, 0.06f, 1.0f));
		SetBrush(Style.Hovered, SelGold * 0.8f);
		SetBrush(Style.Pressed, SelGold);
		LockBtn->SetStyle(Style);
	}
	LockInLabel = SelMakeText(WidgetTree, TEXT("LOCK IN"), 24, FLinearColor(0.05f, 0.04f, 0.01f, 1.0f));
	LockInLabel->SetJustification(ETextJustify::Center);
	LockBtn->AddChild(LockInLabel);
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
		LockInLabel->SetText(FText::FromString(FString::Printf(TEXT("LOCK IN  ·  %s"), *Entry->DisplayName)));
	}
}

void USkylandersCharacterSelectWidget::RebuildInfoPanel(const FSkylanderSelectEntry& Entry)
{
	if (!InfoPanel) return;
	InfoPanel->ClearChildren();

	UTextBlock* Name = SelMakeText(WidgetTree, Entry.DisplayName, 30, Entry.Accent);
	InfoPanel->AddChildToVerticalBox(Name)->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));

	UTextBlock* Role = SelMakeText(WidgetTree, Entry.Role, 15, SelWhite);
	InfoPanel->AddChildToVerticalBox(Role)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

	UTextBlock* Stats = SelMakeText(WidgetTree, Entry.StatLine, 12, SelGrey, true);
	InfoPanel->AddChildToVerticalBox(Stats)->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));

	UTextBlock* AAHeader = SelMakeText(WidgetTree, TEXT("BASIC ATTACK"), 13, SelGold);
	InfoPanel->AddChildToVerticalBox(AAHeader)->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
	UTextBlock* AAText = SelMakeText(WidgetTree, Entry.AutoAttack, 12, SelWhite, true);
	InfoPanel->AddChildToVerticalBox(AAText)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

	UTextBlock* AbHeader = SelMakeText(WidgetTree, TEXT("ABILITIES"), 13, SelGold);
	InfoPanel->AddChildToVerticalBox(AbHeader)->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

	for (int32 i = 0; i < 4; i++)
	{
		FString AbilityName, AbilityDesc;
		Entry.Abilities[i].Split(TEXT("|"), &AbilityName, &AbilityDesc);

		UTextBlock* AbName = SelMakeText(WidgetTree,
			FString::Printf(TEXT("[%d]  %s"), i + 1, *AbilityName), 14, Entry.Accent);
		InfoPanel->AddChildToVerticalBox(AbName)->SetPadding(FMargin(0.f, 4.f, 0.f, 1.f));

		UTextBlock* AbDesc = SelMakeText(WidgetTree, AbilityDesc, 12, SelWhite, true);
		InfoPanel->AddChildToVerticalBox(AbDesc)->SetPadding(FMargin(14.f, 0.f, 0.f, 3.f));
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
		FButtonStyle Style = TileButtons[i]->GetStyle();
		const bool bSelected = (Roster[i].ID == SelectedID);
		Style.Normal.TintColor = FSlateColor(bSelected ? Roster[i].Accent * 0.35f : SelTile);
		TileButtons[i]->SetStyle(Style);
	}
}

void USkylandersCharacterSelectWidget::OnTile0() { SelectCharacter(FSkylanderSelectEntry::GetRoster()[0].ID); }
void USkylandersCharacterSelectWidget::OnTile1() { SelectCharacter(FSkylanderSelectEntry::GetRoster()[1].ID); }
void USkylandersCharacterSelectWidget::OnTile2() { SelectCharacter(FSkylanderSelectEntry::GetRoster()[2].ID); }

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

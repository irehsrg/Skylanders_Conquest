// Skylanders Conquest - Victory/Defeat End Screen Widget Implementation

#include "SkylandersVictoryWidget.h"
#include "SkylandersCharacter.h"
#include "SkylandersEnemyGod.h"
#include "SkylandersActorRegistry.h"
#include "SkylandersItemCatalog.h"
#include "SkylandersItem.h"
#include "SkylandersSmokeTest.h"
#include "SkylandersUIStyle.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"

// Same columns and weights as the Tab scoreboard, so the end screen reads as
// the final state of that board.
static const float VictoryColumnFill[6] = { 2.4f, 0.6f, 1.3f, 0.7f, 0.9f, 4.0f };

static const FLinearColor VictoryBlueRow(0.10f, 0.18f, 0.35f, 0.85f);
static const FLinearColor VictoryRedRow(0.35f, 0.10f, 0.10f, 0.85f);
static const FLinearColor VictoryBlueLabel(0.45f, 0.65f, 1.0f, 1.0f);
static const FLinearColor VictoryRedLabel(1.0f, 0.45f, 0.4f, 1.0f);
static const FLinearColor VictoryTileFill(0.05f, 0.06f, 0.12f, 1.0f);

static constexpr float VictoryPanelWidth = 1000.0f;
static constexpr int32 VictoryItemNameChars = 12;

static FString VictoryItemList(const TArray<int32>& Inventory)
{
	FString Out;
	for (int32 ItemID : Inventory)
	{
		if (ItemID <= 0) continue;
		const FSkylandersItemData* Item = USkylandersItemCatalog::FindItem(ItemID);
		if (!Item) continue;
		if (!Out.IsEmpty()) Out += TEXT(", ");
		Out += Item->ItemName.Left(VictoryItemNameChars);
	}
	return Out.IsEmpty() ? FString(TEXT("---")) : Out;
}

static void VictorySetText(UTextBlock* TB, const FString& Str)
{
	// Every text is optional: a Blueprint subclass or a partial build must not
	// turn the end of a match into a crash.
	if (TB) TB->SetText(FText::FromString(Str));
}

void USkylandersVictoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree) return;

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("VictoryRoot"));
	WidgetTree->RootWidget = Root;

	// Dim the frozen match behind the results
	UBorder* Overlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("VictoryOverlay"));
	Overlay->SetBrushColor(SkylandersUI::OverlayBG);
	UCanvasPanelSlot* OverlaySlot = Root->AddChildToCanvas(Overlay);
	OverlaySlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	OverlaySlot->SetOffsets(FMargin(0.0f));

	// One centered panel; fixed width (the fill columns need one to divide),
	// height from the rows
	UBorder* Panel = SkylandersUI::MakePanel(WidgetTree, TEXT("VictoryPanel"), SkylandersUI::PanelBG, 14.0f,
		SkylandersUI::PanelOutline, 1.0f, FMargin(28.0f, 22.0f));
	USizeBox* WidthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("VictoryWidth"));
	WidthBox->SetWidthOverride(VictoryPanelWidth);
	WidthBox->AddChild(Panel);

	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(WidthBox);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetPosition(FVector2D(0.0f, 0.0f));
	PanelSlot->SetAutoSize(true);

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VictoryVBox"));
	Panel->AddChild(VBox);

	auto AddLine = [&](UWidget* W, float BottomPad, EHorizontalAlignment HAlign = HAlign_Fill)
	{
		UVerticalBoxSlot* LineSlot = VBox->AddChildToVerticalBox(W);
		LineSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BottomPad));
		LineSlot->SetHorizontalAlignment(HAlign);
	};

	// ---- Title block (text and color set by ShowResult) ----
	ResultText = SkylandersUI::MakeTitle(WidgetTree, TEXT("VResultText"), TEXT("---"), 56, SkylandersUI::White);
	AddLine(ResultText, 0.0f, HAlign_Center);
	SubtitleText = SkylandersUI::MakeText(WidgetTree, TEXT("VSubtitle"), TEXT(""), 16, SkylandersUI::Dim);
	AddLine(SubtitleText, 14.0f, HAlign_Center);

	// Hairline divider
	{
		UBorder* Divider = SkylandersUI::MakePanel(WidgetTree, TEXT("VDivider"), SkylandersUI::PanelOutline, 0.0f,
			FLinearColor::Transparent, 0.0f, FMargin(0.0f));
		USizeBox* DividerBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("VDividerBox"));
		DividerBox->SetHeightOverride(1.0f);
		DividerBox->AddChild(Divider);
		AddLine(DividerBox, 12.0f);
	}

	// ---- Results table ----
	{
		UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("VHeader"));
		static const TCHAR* const Titles[6] = { TEXT("NAME"), TEXT("LVL"), TEXT("K / D / A"), TEXT("CS"), TEXT("GOLD"), TEXT("ITEMS") };
		for (int32 c = 0; c < 6; c++)
		{
			AddCell(Header, c, Titles[c], 10, SkylandersUI::Dim);
		}
		// Same horizontal padding as a row's border so the columns align
		UBorder* HeaderPad = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("VHeaderPad"));
		HeaderPad->SetBrushColor(FLinearColor::Transparent);
		HeaderPad->SetPadding(FMargin(8.0f, 2.0f));
		HeaderPad->AddChild(Header);
		AddLine(HeaderPad, 4.0f);
	}

	UTextBlock* BlueLabel = SkylandersUI::MakeTitle(WidgetTree, TEXT("VBlueLabel"), TEXT("BLUE"), 12, VictoryBlueLabel, ETextJustify::Left);
	AddLine(BlueLabel, 3.0f);
	BlueBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VBlueBox"));
	AddLine(BlueBox, 10.0f);

	UTextBlock* RedLabel = SkylandersUI::MakeTitle(WidgetTree, TEXT("VRedLabel"), TEXT("RED"), 12, VictoryRedLabel, ETextJustify::Left);
	AddLine(RedLabel, 3.0f);
	RedBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VRedBox"));
	AddLine(RedBox, 18.0f);

	// ---- Match stat tiles. Damage dealt is not tracked, so the three numbers
	// are the ones the match does keep for the player. ----
	{
		UTextBlock* StatsLabel = SkylandersUI::MakeTitle(WidgetTree, TEXT("VStatsLabel"), TEXT("YOUR MATCH"), 12, SkylandersUI::Gold, ETextJustify::Left);
		AddLine(StatsLabel, 6.0f);

		UHorizontalBox* Tiles = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("VTiles"));
		GoldTileText = AddStatTile(Tiles, TEXT("GOLD EARNED"), false);
		CSTileText = AddStatTile(Tiles, TEXT("CREEP SCORE"), false);
		KATileText = AddStatTile(Tiles, TEXT("KILLS + ASSISTS"), true);
		AddLine(Tiles, 22.0f);
	}

	// ---- Exits - without these the match ends on a paused screen with no way out ----
	{
		UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("VActions"));

		UButton* PlayAgainBtn = SkylandersUI::MakeButton(WidgetTree, TEXT("VPlayAgain"), TEXT("PLAY AGAIN"), 18);
		PlayAgainBtn->OnClicked.AddDynamic(this, &USkylandersVictoryWidget::OnPlayAgainClicked);
		USizeBox* PlayAgainBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("VPlayAgainBox"));
		PlayAgainBox->SetWidthOverride(240.0f);
		PlayAgainBox->SetHeightOverride(48.0f);
		PlayAgainBox->AddChild(PlayAgainBtn);
		Actions->AddChildToHorizontalBox(PlayAgainBox)->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));

		UButton* MainMenuBtn = SkylandersUI::MakeButton(WidgetTree, TEXT("VMainMenu"), TEXT("MAIN MENU"), 18);
		MainMenuBtn->OnClicked.AddDynamic(this, &USkylandersVictoryWidget::OnMainMenuClicked);
		USizeBox* MainMenuBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("VMainMenuBox"));
		MainMenuBox->SetWidthOverride(240.0f);
		MainMenuBox->SetHeightOverride(48.0f);
		MainMenuBox->AddChild(MainMenuBtn);
		Actions->AddChildToHorizontalBox(MainMenuBox)->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));

		AddLine(Actions, 0.0f, HAlign_Center);
	}
}

UTextBlock* USkylandersVictoryWidget::AddCell(UHorizontalBox* Row, int32 Column, const FString& Str, int32 FontSize, FLinearColor Color)
{
	// Name and items read left-aligned; the numeric columns center under their header
	const bool bLeft = (Column == 0 || Column == 5);
	UTextBlock* TB = SkylandersUI::MakeText(WidgetTree, NAME_None, Str, FontSize, Color,
		bLeft ? ETextJustify::Left : ETextJustify::Center);
	// Long item lists end in "..." instead of pushing the other columns around
	TB->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);

	UHorizontalBoxSlot* CellSlot = Row->AddChildToHorizontalBox(TB);
	FSlateChildSize Size(ESlateSizeRule::Fill);
	Size.Value = VictoryColumnFill[Column];
	CellSlot->SetSize(Size);
	CellSlot->SetVerticalAlignment(VAlign_Center);
	CellSlot->SetPadding(FMargin(4.0f, 0.0f));
	return TB;
}

UTextBlock* USkylandersVictoryWidget::AddStatTile(UHorizontalBox* Row, const FString& Label, bool bLast)
{
	UBorder* Tile = SkylandersUI::MakePanel(WidgetTree, NAME_None, VictoryTileFill, 10.0f,
		SkylandersUI::PanelOutline, 1.0f, FMargin(16.0f, 12.0f));

	UVerticalBox* Inner = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock* Value = SkylandersUI::MakeTitle(WidgetTree, NAME_None, TEXT("0"), 30, SkylandersUI::White);
	Inner->AddChildToVerticalBox(Value)->SetHorizontalAlignment(HAlign_Center);
	UTextBlock* Caption = SkylandersUI::MakeText(WidgetTree, NAME_None, Label, 11, SkylandersUI::Dim);
	Inner->AddChildToVerticalBox(Caption)->SetHorizontalAlignment(HAlign_Center);
	Tile->AddChild(Inner);

	UHorizontalBoxSlot* TileSlot = Row->AddChildToHorizontalBox(Tile);
	TileSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TileSlot->SetPadding(FMargin(0.0f, 0.0f, bLast ? 0.0f : 12.0f, 0.0f));
	return Value;
}

void USkylandersVictoryWidget::AddResultRow(UVerticalBox* Box, const FResultLine& L, const FLinearColor& RowColor)
{
	if (!Box || !WidgetTree) return;

	// The player's row carries a gold rim and gold text so it is found at a glance
	UBorder* Background = SkylandersUI::MakePanel(WidgetTree, NAME_None, RowColor, 6.0f,
		L.bIsPlayer ? SkylandersUI::Gold : FLinearColor::Transparent, L.bIsPlayer ? 1.5f : 0.0f, FMargin(8.0f, 5.0f));

	const FLinearColor TextColor = L.bIsPlayer ? SkylandersUI::Gold : SkylandersUI::White;
	UHorizontalBox* Cells = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* NameCell = AddCell(Cells, 0, L.Name, 13, TextColor);
	if (L.bIsPlayer) SkylandersUI::SetFontStyle(NameCell, true, false);
	AddCell(Cells, 1, FString::FromInt(L.Level), 13, TextColor);
	AddCell(Cells, 2, FString::Printf(TEXT("%d / %d / %d"), L.Kills, L.Deaths, L.Assists), 13, TextColor);
	AddCell(Cells, 3, FString::FromInt(L.CS), 13, TextColor);
	AddCell(Cells, 4, FString::FromInt(L.Gold), 13, TextColor);
	AddCell(Cells, 5, L.Items, 10, L.bIsPlayer ? SkylandersUI::Gold : SkylandersUI::Dim);
	Background->AddChild(Cells);

	UVerticalBoxSlot* RowSlot = Box->AddChildToVerticalBox(Background);
	RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
	RowSlot->SetHorizontalAlignment(HAlign_Fill);
}

void USkylandersVictoryWidget::LeaveToLevel(FName LevelName)
{
	UWorld* World = GetWorld();

	// ShowEndScreen pauses the match; travelling while paused would carry the
	// pause into the next level.
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetPause(false);
	}

	RemoveFromParent();

	if (World && !LevelName.IsNone())
	{
		UGameplayStatics::OpenLevel(World, LevelName);
	}
}

void USkylandersVictoryWidget::OnPlayAgainClicked()
{
	LeaveToLevel(FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}

void USkylandersVictoryWidget::OnMainMenuClicked()
{
	LeaveToLevel(MainMenuLevelName);
}

void USkylandersVictoryWidget::ShowResult(bool bVictory, ASkylandersCharacter* Player)
{
	if (ResultText)
	{
		ResultText->SetText(FText::FromString(bVictory ? TEXT("VICTORY") : TEXT("DEFEAT")));
		ResultText->SetColorAndOpacity(FSlateColor(bVictory ? SkylandersUI::Gold : SkylandersUI::Red));
	}

	// The player's clock is the match clock; without a player fall back to world time
	UWorld* World = GetWorld();
	const float MatchSeconds = Player ? Player->GameElapsedTime : (World ? World->GetTimeSeconds() : 0.0f);
	const int32 Minutes = FMath::FloorToInt(MatchSeconds / 60.0f);
	const int32 Seconds = FMath::FloorToInt(FMath::Fmod(MatchSeconds, 60.0f));
	VictorySetText(SubtitleText, FString::Printf(TEXT("%02d:%02d  ·  %s"), Minutes, Seconds,
		bVictory ? TEXT("Titan destroyed") : TEXT("Your titan fell")));

	// Rebuilt from scratch: ShowResult may be called more than once
	TArray<FResultLine> Blue;
	TArray<FResultLine> Red;

	if (Player)
	{
		FResultLine& L = Blue.AddDefaulted_GetRef();
		L.Name = (Player->CharacterName.IsEmpty() ? FString(TEXT("You")) : Player->CharacterName) + TEXT(" (You)");
		L.Level = Player->PlayerLevel;
		L.Kills = Player->Kills;
		L.Deaths = Player->Deaths;
		L.Assists = Player->Assists;
		L.CS = Player->CreepScore;
		L.Gold = Player->TotalGoldEarned;
		L.Items = VictoryItemList(Player->Inventory);
		L.bIsPlayer = true;
	}

	if (World)
	{
		TArray<AActor*> AllGods;
		USkylandersActorRegistry::GetAllActorsOfClass(World, ASkylandersEnemyGod::StaticClass(), AllGods);
		for (AActor* A : AllGods)
		{
			ASkylandersEnemyGod* God = Cast<ASkylandersEnemyGod>(A);
			if (!God) continue;

			FResultLine& L = (God->Team == ETowerTeam::Friendly) ? Blue.AddDefaulted_GetRef() : Red.AddDefaulted_GetRef();
			L.Name = God->GodName;
			L.Level = God->Level;
			L.Kills = God->Kills;
			L.Deaths = God->Deaths;
			L.Assists = God->Assists;
			L.CS = God->CreepScore;
			L.Gold = God->Gold;
			L.Items = VictoryItemList(God->Inventory);
		}
	}

	if (BlueBox)
	{
		BlueBox->ClearChildren();
		for (const FResultLine& L : Blue) AddResultRow(BlueBox, L, VictoryBlueRow);
	}
	if (RedBox)
	{
		RedBox->ClearChildren();
		for (const FResultLine& L : Red) AddResultRow(RedBox, L, VictoryRedRow);
	}

	VictorySetText(GoldTileText, Player ? FString::FromInt(Player->TotalGoldEarned) : FString(TEXT("-")));
	VictorySetText(CSTileText, Player ? FString::FromInt(Player->CreepScore) : FString(TEXT("-")));
	VictorySetText(KATileText, Player ? FString::FromInt(Player->Kills + Player->Assists) : FString(TEXT("-")));
	if (GoldTileText) GoldTileText->SetColorAndOpacity(FSlateColor(SkylandersUI::Gold));

	SKY_COUNT("ui_endscreen_built");
}

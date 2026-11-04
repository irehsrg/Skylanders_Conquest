// Skylanders Conquest - Scoreboard Widget Implementation

#include "SkylandersScoreboardWidget.h"
#include "SkylandersUIStyle.h"
#include "Components/TextBlock.h"
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
#include "SkylandersActorRegistry.h"
#include "SkylandersCharacter.h"
#include "SkylandersEnemyGod.h"
#include "SkylandersItemCatalog.h"
#include "SkylandersItem.h"

// Column fill weights, shared by the header and every row so they line up
static const float ScoreboardColumnFill[6] = { 2.4f, 0.6f, 1.3f, 0.7f, 0.9f, 4.0f };

static const FLinearColor ScoreboardBlueRow(0.10f, 0.18f, 0.35f, 0.85f);
static const FLinearColor ScoreboardRedRow(0.35f, 0.10f, 0.10f, 0.85f);
static const FLinearColor ScoreboardBlueLabel(0.45f, 0.65f, 1.0f, 1.0f);
static const FLinearColor ScoreboardRedLabel(1.0f, 0.45f, 0.4f, 1.0f);

// Item names are cut to this many characters each so six fit on one line
static const int32 ScoreboardItemNameChars = 12;

static UTextBlock* AddScoreboardCell(UWidgetTree* Tree, UHorizontalBox* Row, int32 Column, const FString& Str,
	int32 FontSize, FLinearColor Color)
{
	// Name and items read left-aligned; the numeric columns center under their header
	const bool bLeft = (Column == 0 || Column == 5);
	UTextBlock* TB = SkylandersUI::MakeText(Tree, NAME_None, Str, FontSize, Color,
		bLeft ? ETextJustify::Left : ETextJustify::Center);
	// Long item lists end in "..." instead of pushing the other columns around
	TB->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);

	UHorizontalBoxSlot* CellSlot = Row->AddChildToHorizontalBox(TB);
	FSlateChildSize Size(ESlateSizeRule::Fill);
	Size.Value = ScoreboardColumnFill[Column];
	CellSlot->SetSize(Size);
	CellSlot->SetVerticalAlignment(VAlign_Center);
	CellSlot->SetPadding(FMargin(4.0f, 0.0f));
	return TB;
}

static FString ScoreboardItemList(const TArray<int32>& Inventory)
{
	FString Out;
	for (int32 ItemID : Inventory)
	{
		if (ItemID <= 0) continue;
		const FSkylandersItemData* Item = USkylandersItemCatalog::FindItem(ItemID);
		if (!Item) continue;
		if (!Out.IsEmpty()) Out += TEXT(", ");
		Out += Item->ItemName.Left(ScoreboardItemNameChars);
	}
	return Out.IsEmpty() ? FString(TEXT("---")) : Out;
}

void USkylandersScoreboardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree) return;

	// Root canvas
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SBRoot"));
	WidgetTree->RootWidget = Root;

	// Dark panel centered on screen; height follows the number of rows
	UBorder* BG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SBBG"));
	BG->SetBrushColor(SkylandersUI::OverlayBG);
	BG->SetPadding(FMargin(20.0f, 14.0f));

	// Fixed width (fill columns need one to divide), height from the rows
	USizeBox* WidthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SBWidth"));
	WidthBox->SetWidthOverride(900.0f);
	WidthBox->AddChild(BG);

	UCanvasPanelSlot* BGSlot = Root->AddChildToCanvas(WidthBox);
	BGSlot->SetAnchors(FAnchors(0.5f, 0.2f, 0.5f, 0.2f));
	BGSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	BGSlot->SetPosition(FVector2D(0.0f, 0.0f));
	BGSlot->SetAutoSize(true);

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SBVBox"));
	BG->AddChild(VBox);

	auto AddLine = [&](UWidget* W, float BottomPad)
	{
		UVerticalBoxSlot* LineSlot = VBox->AddChildToVerticalBox(W);
		LineSlot->SetPadding(FMargin(0, 0, 0, BottomPad));
		LineSlot->SetHorizontalAlignment(HAlign_Fill);
	};

	// Title + match time
	AddLine(SkylandersUI::MakeText(WidgetTree, TEXT("SBTitle"), TEXT("SCOREBOARD"), 16, FLinearColor(0.7f, 0.8f, 1.0f)), 2);
	GameTimeText = SkylandersUI::MakeText(WidgetTree, TEXT("SBTime"), TEXT("Time: 00:00"), 12, SkylandersUI::Dim);
	AddLine(GameTimeText, 10);

	// Column header
	{
		UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SBHeader"));
		static const TCHAR* const Titles[6] = { TEXT("NAME"), TEXT("LVL"), TEXT("K / D / A"), TEXT("CS"), TEXT("GOLD"), TEXT("ITEMS") };
		for (int32 c = 0; c < 6; c++)
		{
			AddScoreboardCell(WidgetTree, Header, c, Titles[c], 10, SkylandersUI::Dim);
		}
		// Same horizontal padding as a row's border so the columns align
		UBorder* HeaderPad = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SBHeaderPad"));
		HeaderPad->SetBrushColor(FLinearColor::Transparent);
		HeaderPad->SetPadding(FMargin(8.0f, 2.0f));
		HeaderPad->AddChild(Header);
		AddLine(HeaderPad, 4);
	}

	// Team sections
	AddLine(SkylandersUI::MakeText(WidgetTree, TEXT("SBBlueLabel"), TEXT("BLUE"), 12, ScoreboardBlueLabel, ETextJustify::Left), 2);
	BlueBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SBBlueBox"));
	AddLine(BlueBox, 10);

	AddLine(SkylandersUI::MakeText(WidgetTree, TEXT("SBRedLabel"), TEXT("RED"), 12, ScoreboardRedLabel, ETextJustify::Left), 2);
	RedBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SBRedBox"));
	AddLine(RedBox, 0);
}

USkylandersScoreboardWidget::FRow USkylandersScoreboardWidget::MakeRow(UVerticalBox* Box)
{
	FRow Row;
	Row.Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Row.Background->SetPadding(FMargin(8.0f, 4.0f));

	UHorizontalBox* Cells = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Row.Name = AddScoreboardCell(WidgetTree, Cells, 0, TEXT(""), 12, SkylandersUI::White);
	Row.Level = AddScoreboardCell(WidgetTree, Cells, 1, TEXT(""), 12, SkylandersUI::White);
	Row.KDA = AddScoreboardCell(WidgetTree, Cells, 2, TEXT(""), 12, SkylandersUI::White);
	Row.CS = AddScoreboardCell(WidgetTree, Cells, 3, TEXT(""), 12, SkylandersUI::White);
	Row.Gold = AddScoreboardCell(WidgetTree, Cells, 4, TEXT(""), 12, SkylandersUI::White);
	Row.Items = AddScoreboardCell(WidgetTree, Cells, 5, TEXT(""), 10, SkylandersUI::Dim);
	Row.Background->AddChild(Cells);

	UVerticalBoxSlot* RowSlot = Box->AddChildToVerticalBox(Row.Background);
	RowSlot->SetPadding(FMargin(0, 0, 0, 3));
	RowSlot->SetHorizontalAlignment(HAlign_Fill);
	return Row;
}

void USkylandersScoreboardWidget::ShowLines(UVerticalBox* Box, TArray<FRow>& Rows, const TArray<FHeroLine>& Lines, const FLinearColor& RowColor)
{
	if (!Box) return;
	while (Rows.Num() < Lines.Num())
	{
		Rows.Add(MakeRow(Box));
	}

	for (int32 i = 0; i < Rows.Num(); i++)
	{
		FRow& Row = Rows[i];
		if (i >= Lines.Num())
		{
			Row.Background->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		const FHeroLine& L = Lines[i];
		Row.Background->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Row.Background->SetBrushColor(RowColor);
		// Dead heroes fade back so the living ones read first
		Row.Background->SetRenderOpacity(L.bDead ? 0.45f : 1.0f);

		const FLinearColor TextColor = L.bIsPlayer ? SkylandersUI::Gold : SkylandersUI::White;
		Row.Name->SetText(FText::FromString(L.bDead ? L.Name + TEXT(" (dead)") : L.Name));
		Row.Level->SetText(FText::AsNumber(L.Level));
		Row.KDA->SetText(FText::FromString(FString::Printf(TEXT("%d / %d / %d"), L.Kills, L.Deaths, L.Assists)));
		Row.CS->SetText(FText::AsNumber(L.CS));
		Row.Gold->SetText(FText::AsNumber(L.Gold));
		Row.Items->SetText(FText::FromString(L.Items));
		for (UTextBlock* TB : { Row.Name, Row.Level, Row.KDA, Row.CS, Row.Gold })
		{
			TB->SetColorAndOpacity(FSlateColor(TextColor));
		}
		Row.Items->SetColorAndOpacity(FSlateColor(L.bIsPlayer ? SkylandersUI::Gold : SkylandersUI::Dim));
	}
}

void USkylandersScoreboardWidget::Refresh()
{
	UWorld* World = GetWorld();
	if (!World) return;

	TArray<FHeroLine> Blue;
	TArray<FHeroLine> Red;

	ASkylandersCharacter* Player = Cast<ASkylandersCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (Player)
	{
		int32 Minutes = FMath::FloorToInt(Player->GameElapsedTime / 60.0f);
		int32 Seconds = FMath::FloorToInt(FMath::Fmod(Player->GameElapsedTime, 60.0f));
		if (GameTimeText)
		{
			GameTimeText->SetText(FText::FromString(FString::Printf(TEXT("Time: %02d:%02d"), Minutes, Seconds)));
		}

		FHeroLine& L = Blue.AddDefaulted_GetRef();
		L.Name = (Player->CharacterName.IsEmpty() ? FString(TEXT("You")) : Player->CharacterName) + TEXT(" (You)");
		L.Level = Player->PlayerLevel;
		L.Kills = Player->Kills;
		L.Deaths = Player->Deaths;
		L.Assists = Player->Assists;
		L.CS = Player->CreepScore;
		L.Gold = Player->Coins;
		L.Items = ScoreboardItemList(Player->Inventory);
		L.bDead = Player->CurrentHealth <= 0.0f;
		L.bIsPlayer = true;
	}

	TArray<AActor*> AllGods;
	USkylandersActorRegistry::GetAllActorsOfClass(World, ASkylandersEnemyGod::StaticClass(), AllGods);
	for (AActor* A : AllGods)
	{
		ASkylandersEnemyGod* God = Cast<ASkylandersEnemyGod>(A);
		if (!God) continue;

		FHeroLine& L = (God->Team == ETowerTeam::Friendly) ? Blue.AddDefaulted_GetRef() : Red.AddDefaulted_GetRef();
		L.Name = God->GodName;
		L.Level = God->Level;
		L.Kills = God->Kills;
		L.Deaths = God->Deaths;
		L.Assists = God->Assists;
		L.CS = God->CreepScore;
		L.Gold = God->Gold;
		L.Items = ScoreboardItemList(God->Inventory);
		L.bDead = God->CurrentState == EGodAIState::Dead;
	}

	ShowLines(BlueBox, BlueRows, Blue, ScoreboardBlueRow);
	ShowLines(RedBox, RedRows, Red, ScoreboardRedRow);
}

void USkylandersScoreboardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (GetVisibility() == ESlateVisibility::Collapsed)
	{
		// Next time Tab opens the board, refresh on the first visible frame
		SinceRefresh = RefreshInterval;
		return;
	}

	// 4 Hz is plenty for K/D/A and gold, and keeps a held Tab from rebuilding
	// every row's text every frame
	SinceRefresh += InDeltaTime;
	if (SinceRefresh < RefreshInterval) return;
	SinceRefresh = 0.0f;

	Refresh();
}

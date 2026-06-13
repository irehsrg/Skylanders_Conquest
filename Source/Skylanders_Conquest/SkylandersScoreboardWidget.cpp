// Skylanders Conquest - Scoreboard Widget Implementation

#include "SkylandersScoreboardWidget.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "SkylandersCharacter.h"
#include "SkylandersEnemyGod.h"
#include "SkylandersItemCatalog.h"
#include "SkylandersItem.h"
#include "Fonts/SlateFontInfo.h"

static UTextBlock* MakeText(UWidgetTree* Tree, const FName& Name, int32 FontSize, FLinearColor Color, const FString& Default)
{
	UTextBlock* TB = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	TB->SetText(FText::FromString(Default));
	FSlateFontInfo Font = TB->GetFont();
	Font.Size = FontSize;
	TB->SetFont(Font);
	TB->SetColorAndOpacity(FSlateColor(Color));
	return TB;
}

void USkylandersScoreboardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree) return;

	// Root canvas
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SBRoot"));
	WidgetTree->RootWidget = Root;

	// Dark background centered on screen
	UBorder* BG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SBBG"));
	BG->SetBrushColor(FLinearColor(0.01f, 0.01f, 0.04f, 0.92f));
	BG->SetPadding(FMargin(20.0f, 14.0f));

	UCanvasPanelSlot* BGSlot = Root->AddChildToCanvas(BG);
	BGSlot->SetAnchors(FAnchors(0.5f, 0.3f, 0.5f, 0.3f));
	BGSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	BGSlot->SetPosition(FVector2D(0.0f, 0.0f));
	BGSlot->SetSize(FVector2D(420.0f, 280.0f));

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SBVBox"));
	BG->AddChild(VBox);

	auto AddRow = [&](UTextBlock* TB, float BottomPad = 4.0f)
	{
		UVerticalBoxSlot* Slot = VBox->AddChildToVerticalBox(TB);
		Slot->SetPadding(FMargin(0, 0, 0, BottomPad));
	};

	// Title
	UTextBlock* Title = MakeText(WidgetTree, TEXT("SBTitle"), 16, FLinearColor(0.7f, 0.8f, 1.0f), TEXT("SCOREBOARD"));
	Title->SetJustification(ETextJustify::Center);
	AddRow(Title, 2);

	// Game time
	GameTimeText = MakeText(WidgetTree, TEXT("SBTime"), 12, FLinearColor(0.6f, 0.6f, 0.6f), TEXT("Time: 00:00"));
	GameTimeText->SetJustification(ETextJustify::Center);
	AddRow(GameTimeText, 10);

	// Divider
	UBorder* Div1 = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SBDiv1"));
	Div1->SetBrushColor(FLinearColor(0.3f, 0.4f, 0.6f, 0.5f));
	Div1->SetPadding(FMargin(0));
	UVerticalBoxSlot* DivSlot = VBox->AddChildToVerticalBox(Div1);
	DivSlot->SetPadding(FMargin(0, 0, 0, 8));

	// Player section
	PlayerNameText = MakeText(WidgetTree, TEXT("SBPlayerName"), 13, FLinearColor(0.3f, 1.0f, 0.5f), TEXT("TRIGGER HAPPY"));
	AddRow(PlayerNameText, 2);

	PlayerStatsText = MakeText(WidgetTree, TEXT("SBPlayerStats"), 11, FLinearColor(0.8f, 0.8f, 0.8f), TEXT("LVL 1 | K: 0  D: 0 | CS: 0 | Gold: 0"));
	AddRow(PlayerStatsText, 2);

	PlayerItemsText = MakeText(WidgetTree, TEXT("SBPlayerItems"), 9, FLinearColor(0.6f, 0.6f, 0.7f), TEXT("Items: ---"));
	AddRow(PlayerItemsText, 10);

	// Divider 2
	UBorder* Div2 = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SBDiv2"));
	Div2->SetBrushColor(FLinearColor(0.3f, 0.4f, 0.6f, 0.5f));
	Div2->SetPadding(FMargin(0));
	UVerticalBoxSlot* DivSlot2 = VBox->AddChildToVerticalBox(Div2);
	DivSlot2->SetPadding(FMargin(0, 0, 0, 8));

	// Enemy section
	EnemyNameText = MakeText(WidgetTree, TEXT("SBEnemyName"), 13, FLinearColor(1.0f, 0.3f, 0.3f), TEXT("ENEMY GOD"));
	AddRow(EnemyNameText, 2);

	EnemyStatsText = MakeText(WidgetTree, TEXT("SBEnemyStats"), 11, FLinearColor(0.8f, 0.8f, 0.8f), TEXT("LVL 1 | HP: ---"));
	AddRow(EnemyStatsText, 2);
}

void USkylandersScoreboardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (GetVisibility() == ESlateVisibility::Collapsed) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// Update game time
	ASkylandersCharacter* Player = Cast<ASkylandersCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (Player && GameTimeText)
	{
		int32 Minutes = FMath::FloorToInt(Player->GameElapsedTime / 60.0f);
		int32 Seconds = FMath::FloorToInt(FMath::Fmod(Player->GameElapsedTime, 60.0f));
		GameTimeText->SetText(FText::FromString(FString::Printf(TEXT("Time: %02d:%02d"), Minutes, Seconds)));
	}

	// Update player stats
	if (Player && PlayerStatsText)
	{
		PlayerStatsText->SetText(FText::FromString(FString::Printf(
			TEXT("LVL %d | K: %d  D: %d | CS: %d | Gold: %d"),
			Player->PlayerLevel, Player->Kills, Player->Deaths, Player->CreepScore, Player->Coins)));

		// Items
		FString ItemStr = TEXT("Items: ");
		bool bAnyItem = false;
		for (int32 i = 0; i < Player->MaxInventorySlots; i++)
		{
			if (Player->Inventory[i] > 0)
			{
				const FSkylandersItemData* Item = USkylandersItemCatalog::FindItem(Player->Inventory[i]);
				if (Item)
				{
					if (bAnyItem) ItemStr += TEXT(", ");
					ItemStr += Item->ItemName;
					bAnyItem = true;
				}
			}
		}
		if (!bAnyItem) ItemStr += TEXT("---");
		if (PlayerItemsText) PlayerItemsText->SetText(FText::FromString(ItemStr));
	}

	// Update enemy god stats
	TArray<AActor*> AllGods;
	UGameplayStatics::GetAllActorsOfClass(World, ASkylandersEnemyGod::StaticClass(), AllGods);
	if (AllGods.Num() > 0)
	{
		ASkylandersEnemyGod* God = Cast<ASkylandersEnemyGod>(AllGods[0]);
		if (God)
		{
			if (EnemyNameText)
				EnemyNameText->SetText(FText::FromString(God->GodName.ToUpper()));

			if (EnemyStatsText)
			{
				FString StateStr = (God->CurrentState == EGodAIState::Dead) ? TEXT("DEAD") :
					FString::Printf(TEXT("HP: %.0f/%.0f"), God->CurrentHealth, God->MaxHealth);
				EnemyStatsText->SetText(FText::FromString(FString::Printf(
					TEXT("LVL %d | %s"), God->Level, *StateStr)));
			}
		}
	}
}

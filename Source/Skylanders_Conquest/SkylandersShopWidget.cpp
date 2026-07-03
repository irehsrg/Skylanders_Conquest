// Skylanders Conquest - Shop UI Widget Implementation (Grid Layout)

#include "SkylandersShopWidget.h"
#include "SkylandersCharacter.h"
#include "SkylandersItemCatalog.h"
#include "SkylandersItem.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Border.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/BorderSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Spacer.h"
#include "Components/ScrollBox.h"
#include "Blueprint/WidgetTree.h"

// Running counter for unique widget names
static int32 GShopWidgetCounter = 0;

void USkylandersShopWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		OwnerCharacter = Cast<ASkylandersCharacter>(PC->GetPawn());
	}

	CurrentFilter = -1;
	GShopWidgetCounter = 0;

	BuildLayout();
	ApplyFilter();
	UpdateInventory();
	UpdateHeader();
	UpdateBuyButtons();
}

void USkylandersShopWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (GetVisibility() == ESlateVisibility::Collapsed) return;

	UpdateHeader();
	UpdateInventory();
	UpdateBuyButtons();
}

UTextBlock* USkylandersShopWidget::MakeText(const FString& Content, int32 FontSize, FLinearColor Color)
{
	if (!WidgetTree) return nullptr;

	FString Name = FString::Printf(TEXT("ST_%d"), GShopWidgetCounter++);
	UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *Name);
	T->SetText(FText::FromString(Content));

	FSlateFontInfo Font = T->GetFont();
	Font.Size = FontSize;
	T->SetFont(Font);
	T->SetColorAndOpacity(FSlateColor(Color));

	return T;
}

UButton* USkylandersShopWidget::MakeButton(const FString& Label, int32 FontSize)
{
	if (!WidgetTree) return nullptr;

	FString Name = FString::Printf(TEXT("SB_%d"), GShopWidgetCounter++);
	UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *Name);

	FButtonStyle Style = B->GetStyle();

	FSlateBrush Normal;
	Normal.TintColor = FSlateColor(FLinearColor(0.15f, 0.15f, 0.2f, 1.0f));
	Style.SetNormal(Normal);

	FSlateBrush Hover;
	Hover.TintColor = FSlateColor(FLinearColor(0.25f, 0.25f, 0.35f, 1.0f));
	Style.SetHovered(Hover);

	FSlateBrush Press;
	Press.TintColor = FSlateColor(FLinearColor(0.35f, 0.35f, 0.45f, 1.0f));
	Style.SetPressed(Press);

	B->SetStyle(Style);

	UTextBlock* BtnText = MakeText(Label, FontSize, FLinearColor::White);
	B->AddChild(BtnText);

	return B;
}

FString USkylandersShopWidget::BuildStatSummary(const FSkylandersItemStats& Stats)
{
	TArray<FString> Parts;

	if (Stats.Power > 0.0f)
		Parts.Add(FString::Printf(TEXT("+%.0f Power"), Stats.Power));
	if (Stats.AttackSpeed > 0.0f)
		Parts.Add(FString::Printf(TEXT("+%.1f AtkSpd"), Stats.AttackSpeed));
	if (Stats.CritChance > 0.0f)
		Parts.Add(FString::Printf(TEXT("+%.0f%% Crit"), Stats.CritChance * 100.0f));
	if (Stats.Lifesteal > 0.0f)
		Parts.Add(FString::Printf(TEXT("+%.0f%% Lifesteal"), Stats.Lifesteal * 100.0f));
	if (Stats.MaxHealth > 0.0f)
		Parts.Add(FString::Printf(TEXT("+%.0f HP"), Stats.MaxHealth));
	if (Stats.MaxMana > 0.0f)
		Parts.Add(FString::Printf(TEXT("+%.0f MP"), Stats.MaxMana));
	if (Stats.HealthRegen > 0.0f)
		Parts.Add(FString::Printf(TEXT("+%.0f HP/s"), Stats.HealthRegen));
	if (Stats.ManaRegen > 0.0f)
		Parts.Add(FString::Printf(TEXT("+%.0f MP/s"), Stats.ManaRegen));
	if (Stats.Protections > 0.0f)
		Parts.Add(FString::Printf(TEXT("+%.0f Prot"), Stats.Protections));
	if (Stats.MovementSpeed > 0.0f)
		Parts.Add(FString::Printf(TEXT("+%.0f MoveSpd"), Stats.MovementSpeed));

	// Join with newlines for card layout
	FString Result;
	for (int32 i = 0; i < Parts.Num(); i++)
	{
		if (i > 0) Result += TEXT("\n");
		Result += Parts[i];
	}
	return Result;
}

void USkylandersShopWidget::BuildLayout()
{
	if (!WidgetTree) return;

	// ===== ROOT =====
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ShopRoot"));
	WidgetTree->RootWidget = Root;

	// ===== BACKGROUND =====
	UBorder* BG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ShopBG"));
	BG->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.05f, 0.94f));
	BG->SetPadding(FMargin(24.0f, 18.0f));

	UCanvasPanelSlot* BGSlot = Root->AddChildToCanvas(BG);
	BGSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
	BGSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	BGSlot->SetSize(FVector2D(920.0f, 700.0f));
	BGSlot->SetPosition(FVector2D(0.0f, 0.0f));
	BGSlot->SetAutoSize(false);

	UVerticalBox* MainVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShopVBox"));
	BG->AddChild(MainVBox);

	// ===== ROW 1: Title + Gold + Spawn Status =====
	{
		UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TitleRow"));
		UVerticalBoxSlot* TRSlot = MainVBox->AddChildToVerticalBox(TitleRow);
		TRSlot->SetPadding(FMargin(0, 0, 0, 6));

		UTextBlock* Title = MakeText(TEXT("SHOP"), 24, FLinearColor(1.0f, 0.85f, 0.0f));
		UHorizontalBoxSlot* TSlot = TitleRow->AddChildToHorizontalBox(Title);
		TSlot->SetPadding(FMargin(0, 0, 24, 0));
		TSlot->SetVerticalAlignment(VAlign_Center);

		GoldText = MakeText(TEXT("Gold: 0"), 20, FLinearColor(1.0f, 0.9f, 0.3f));
		UHorizontalBoxSlot* GSlot = TitleRow->AddChildToHorizontalBox(GoldText);
		GSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		GSlot->SetVerticalAlignment(VAlign_Center);

		SpawnAreaText = MakeText(TEXT("NOT IN SPAWN"), 14, FLinearColor(1.0f, 0.3f, 0.3f));
		UHorizontalBoxSlot* SASlot = TitleRow->AddChildToHorizontalBox(SpawnAreaText);
		SASlot->SetVerticalAlignment(VAlign_Center);
	}

	// ===== ROW 2: Filter Buttons + Close Hint =====
	{
		UHorizontalBox* FilterRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FilterRow"));
		UVerticalBoxSlot* FRSlot = MainVBox->AddChildToVerticalBox(FilterRow);
		FRSlot->SetPadding(FMargin(0, 0, 0, 8));

		auto AddFilterBtn = [&](const FString& Label, FName FuncName)
		{
			UButton* Btn = MakeButton(Label, 13);
			FScriptDelegate Del;
			Del.BindUFunction(this, FuncName);
			Btn->OnClicked.Add(Del);
			UHorizontalBoxSlot* BSlot = FilterRow->AddChildToHorizontalBox(Btn);
			BSlot->SetPadding(FMargin(0, 0, 6, 0));
			FilterButtons.Add(Btn);
		};

		AddFilterBtn(TEXT("All"), TEXT("OnFilterAll"));
		AddFilterBtn(TEXT("Offense"), TEXT("OnFilterOffense"));
		AddFilterBtn(TEXT("Defense"), TEXT("OnFilterDefense"));
		AddFilterBtn(TEXT("Utility"), TEXT("OnFilterUtility"));

		// Spawn area reminder
		UTextBlock* Reminder = MakeText(TEXT("Must be in spawn area to buy/sell"), 11, FLinearColor(0.6f, 0.5f, 0.3f));
		UHorizontalBoxSlot* RSlot = FilterRow->AddChildToHorizontalBox(Reminder);
		RSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		RSlot->SetHorizontalAlignment(HAlign_Right);
		RSlot->SetVerticalAlignment(VAlign_Center);
	}

	// ===== SEPARATOR =====
	{
		UBorder* Sep = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Sep1"));
		Sep->SetBrushColor(FLinearColor(0.4f, 0.35f, 0.1f, 0.6f));
		USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("Sp1"));
		Sp->SetSize(FVector2D(1, 2));
		Sep->AddChild(Sp);
		UVerticalBoxSlot* SepSlot = MainVBox->AddChildToVerticalBox(Sep);
		SepSlot->SetPadding(FMargin(0, 2, 0, 8));
	}

	// ===== ITEM GRID (scrollable, 3 columns) =====
	{
		UScrollBox* ScrollArea = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ItemScroll"));
		UVerticalBoxSlot* ScrollSlot = MainVBox->AddChildToVerticalBox(ScrollArea);
		ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ScrollSlot->SetPadding(FMargin(0, 0, 0, 8));

		ItemGridContainer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ItemGrid"));
		ScrollArea->AddChild(ItemGridContainer);

		const TArray<FSkylandersItemData>& AllItems = USkylandersItemCatalog::GetAllItems();

		// Build all 12 item cards (will be arranged into grid rows later)
		for (const FSkylandersItemData& Item : AllItems)
		{
			// Outer border (card frame)
			FLinearColor BorderColor(0.3f, 0.3f, 0.3f, 1.0f);
			FString CatLabel = TEXT("ITEM");
			switch (Item.Category)
			{
			case ESkylandersItemCategory::Offense:
				BorderColor = FLinearColor(0.6f, 0.15f, 0.15f, 1.0f);
				CatLabel = TEXT("OFFENSE");
				break;
			case ESkylandersItemCategory::Defense:
				BorderColor = FLinearColor(0.15f, 0.3f, 0.6f, 1.0f);
				CatLabel = TEXT("DEFENSE");
				break;
			case ESkylandersItemCategory::Utility:
				BorderColor = FLinearColor(0.15f, 0.5f, 0.25f, 1.0f);
				CatLabel = TEXT("UTILITY");
				break;
			}

			UBorder* CardBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
				*FString::Printf(TEXT("Card_%d"), Item.ItemID));
			CardBorder->SetBrushColor(BorderColor);
			CardBorder->SetPadding(FMargin(2.0f));

			ItemCardBorders.Add(CardBorder);
			ItemCardCategories.Add(static_cast<int32>(Item.Category));

			// Inner background
			UBorder* CardInner = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
				*FString::Printf(TEXT("CardInner_%d"), Item.ItemID));
			CardInner->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.09f, 1.0f));
			CardInner->SetPadding(FMargin(10.0f, 8.0f));
			CardBorder->AddChild(CardInner);

			UVerticalBox* CardVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
				*FString::Printf(TEXT("CardVB_%d"), Item.ItemID));
			CardInner->AddChild(CardVBox);

			// Category tag (small, colored)
			FLinearColor CatTextColor(0.8f, 0.8f, 0.8f);
			switch (Item.Category)
			{
			case ESkylandersItemCategory::Offense:
				CatTextColor = FLinearColor(1.0f, 0.4f, 0.4f);
				break;
			case ESkylandersItemCategory::Defense:
				CatTextColor = FLinearColor(0.4f, 0.7f, 1.0f);
				break;
			case ESkylandersItemCategory::Utility:
				CatTextColor = FLinearColor(0.4f, 1.0f, 0.6f);
				break;
			}

			UTextBlock* CatText = MakeText(CatLabel, 9, CatTextColor);
			UVerticalBoxSlot* CatSlot = CardVBox->AddChildToVerticalBox(CatText);
			CatSlot->SetPadding(FMargin(0, 0, 0, 2));

			// Item name
			UTextBlock* NameText = MakeText(Item.ItemName, 15, FLinearColor::White);
			UVerticalBoxSlot* NameSlot = CardVBox->AddChildToVerticalBox(NameText);
			NameSlot->SetPadding(FMargin(0, 0, 0, 4));

			// Stat summary (multi-line)
			FString StatStr = BuildStatSummary(Item.Stats);
			UTextBlock* StatText = MakeText(StatStr, 11, FLinearColor(0.75f, 0.8f, 0.65f));
			StatText->SetAutoWrapText(true);
			UVerticalBoxSlot* StatSlot = CardVBox->AddChildToVerticalBox(StatText);
			StatSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			StatSlot->SetPadding(FMargin(0, 0, 0, 6));

			// Bottom row: cost + buy button
			UHorizontalBox* BottomRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
				*FString::Printf(TEXT("CardBot_%d"), Item.ItemID));
			CardVBox->AddChildToVerticalBox(BottomRow);

			UTextBlock* CostText = MakeText(FString::Printf(TEXT("%d gold"), Item.Cost), 13, FLinearColor(1.0f, 0.9f, 0.3f));
			UHorizontalBoxSlot* CostSlot = BottomRow->AddChildToHorizontalBox(CostText);
			CostSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			CostSlot->SetVerticalAlignment(VAlign_Center);
			ItemCostTexts.Add(CostText);

			UButton* BuyBtn = MakeButton(TEXT("BUY"), 12);
			UHorizontalBoxSlot* BuySlot = BottomRow->AddChildToHorizontalBox(BuyBtn);
			BuySlot->SetVerticalAlignment(VAlign_Center);
			ItemBuyButtons.Add(BuyBtn);

			// Bind buy delegate
			FScriptDelegate BuyDelegate;
			BuyDelegate.BindUFunction(this, FName(*FString::Printf(TEXT("OnBuyItem%d"), Item.ItemID)));
			BuyBtn->OnClicked.Add(BuyDelegate);
		}

		// Arrange cards into grid rows (3 per row)
		RebuildGrid();
	}

	// ===== SEPARATOR 2 =====
	{
		UBorder* Sep2 = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Sep2"));
		Sep2->SetBrushColor(FLinearColor(0.4f, 0.35f, 0.1f, 0.6f));
		USpacer* Sp2 = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("Sp2"));
		Sp2->SetSize(FVector2D(1, 2));
		Sep2->AddChild(Sp2);
		UVerticalBoxSlot* Sep2Slot = MainVBox->AddChildToVerticalBox(Sep2);
		Sep2Slot->SetPadding(FMargin(0, 2, 0, 6));
	}

	// ===== INVENTORY LABEL =====
	{
		UHorizontalBox* InvHeader = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("InvHeader"));
		UVerticalBoxSlot* IHSlot = MainVBox->AddChildToVerticalBox(InvHeader);
		IHSlot->SetPadding(FMargin(0, 0, 0, 4));

		UTextBlock* InvLabel = MakeText(TEXT("INVENTORY"), 16, FLinearColor(0.7f, 0.85f, 1.0f));
		UHorizontalBoxSlot* ILSlot = InvHeader->AddChildToHorizontalBox(InvLabel);
		ILSlot->SetVerticalAlignment(VAlign_Center);

		UTextBlock* CloseHint = MakeText(TEXT("  [B] Close Shop"), 12, FLinearColor(0.5f, 0.5f, 0.5f));
		UHorizontalBoxSlot* CHSlot = InvHeader->AddChildToHorizontalBox(CloseHint);
		CHSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CHSlot->SetHorizontalAlignment(HAlign_Right);
		CHSlot->SetVerticalAlignment(VAlign_Center);
	}

	// ===== INVENTORY SLOTS (6 slots in a horizontal row) =====
	{
		UHorizontalBox* InvRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("InvRow"));
		UVerticalBoxSlot* InvRowSlot = MainVBox->AddChildToVerticalBox(InvRow);
		InvRowSlot->SetPadding(FMargin(0, 0, 0, 4));

		InventorySlotTexts.SetNum(6);
		InventorySlotStatTexts.SetNum(6);
		SellButtons.SetNum(6);

		for (int32 i = 0; i < 6; i++)
		{
			// Slot border
			UBorder* SlotBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
				*FString::Printf(TEXT("InvBorder_%d"), i));
			SlotBorder->SetBrushColor(FLinearColor(0.2f, 0.2f, 0.3f, 0.6f));
			SlotBorder->SetPadding(FMargin(2.0f));

			UHorizontalBoxSlot* SBSlot = InvRow->AddChildToHorizontalBox(SlotBorder);
			SBSlot->SetPadding(FMargin(0, 0, (i < 5) ? 4.0f : 0.0f, 0));
			SBSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

			// Inner bg
			UBorder* SlotInner = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
				*FString::Printf(TEXT("InvInner_%d"), i));
			SlotInner->SetBrushColor(FLinearColor(0.06f, 0.06f, 0.1f, 1.0f));
			SlotInner->SetPadding(FMargin(6, 4));
			SlotBorder->AddChild(SlotInner);

			UVerticalBox* SlotVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
				*FString::Printf(TEXT("InvVB_%d"), i));
			SlotInner->AddChild(SlotVBox);

			// Item name
			InventorySlotTexts[i] = MakeText(TEXT("Empty"), 11, FLinearColor(0.4f, 0.4f, 0.4f));
			SlotVBox->AddChildToVerticalBox(InventorySlotTexts[i]);

			// Item stat line
			InventorySlotStatTexts[i] = MakeText(TEXT(""), 9, FLinearColor(0.5f, 0.5f, 0.4f));
			UVerticalBoxSlot* StatLine = SlotVBox->AddChildToVerticalBox(InventorySlotStatTexts[i]);
			StatLine->SetPadding(FMargin(0, 1, 0, 0));

			// Sell button
			SellButtons[i] = MakeButton(TEXT("Sell"), 10);
			SellButtons[i]->SetVisibility(ESlateVisibility::Collapsed);
			UVerticalBoxSlot* BtnVSlot = SlotVBox->AddChildToVerticalBox(SellButtons[i]);
			BtnVSlot->SetPadding(FMargin(0, 3, 0, 0));
		}

		// Bind sell buttons
		SellButtons[0]->OnClicked.AddDynamic(this, &USkylandersShopWidget::OnSellSlot0);
		SellButtons[1]->OnClicked.AddDynamic(this, &USkylandersShopWidget::OnSellSlot1);
		SellButtons[2]->OnClicked.AddDynamic(this, &USkylandersShopWidget::OnSellSlot2);
		SellButtons[3]->OnClicked.AddDynamic(this, &USkylandersShopWidget::OnSellSlot3);
		SellButtons[4]->OnClicked.AddDynamic(this, &USkylandersShopWidget::OnSellSlot4);
		SellButtons[5]->OnClicked.AddDynamic(this, &USkylandersShopWidget::OnSellSlot5);
	}
}

void USkylandersShopWidget::RebuildGrid()
{
	if (!ItemGridContainer || !WidgetTree) return;

	// Remove old children
	ItemGridContainer->ClearChildren();

	// Collect visible cards
	TArray<UBorder*> VisibleCards;
	for (int32 i = 0; i < ItemCardBorders.Num(); i++)
	{
		if (!ItemCardBorders[i]) continue;
		bool bShow = (CurrentFilter < 0) || (ItemCardCategories[i] == CurrentFilter);
		if (bShow)
		{
			VisibleCards.Add(ItemCardBorders[i]);
		}
	}

	// Arrange into rows of 3
	const int32 Columns = 3;
	for (int32 Row = 0; Row * Columns < VisibleCards.Num(); Row++)
	{
		UHorizontalBox* GridRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
			*FString::Printf(TEXT("GR_%d"), GShopWidgetCounter++));
		UVerticalBoxSlot* RowVSlot = ItemGridContainer->AddChildToVerticalBox(GridRow);
		RowVSlot->SetPadding(FMargin(0, 0, 0, 6));

		for (int32 Col = 0; Col < Columns; Col++)
		{
			int32 Idx = Row * Columns + Col;
			if (Idx < VisibleCards.Num())
			{
				UHorizontalBoxSlot* CardHSlot = GridRow->AddChildToHorizontalBox(VisibleCards[Idx]);
				CardHSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				CardHSlot->SetPadding(FMargin(0, 0, (Col < Columns - 1) ? 6.0f : 0.0f, 0));
			}
			else
			{
				// Empty spacer to keep grid aligned
				USpacer* EmptySpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(),
					*FString::Printf(TEXT("GSp_%d"), GShopWidgetCounter++));
				UHorizontalBoxSlot* SpHSlot = GridRow->AddChildToHorizontalBox(EmptySpacer);
				SpHSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				SpHSlot->SetPadding(FMargin(0, 0, (Col < Columns - 1) ? 6.0f : 0.0f, 0));
			}
		}
	}
}

void USkylandersShopWidget::ApplyFilter()
{
	// All cards are always "visible" in terms of ESlateVisibility;
	// the grid rebuild handles which ones appear
	RebuildGrid();
}

void USkylandersShopWidget::UpdateInventory()
{
	if (!OwnerCharacter) return;

	for (int32 i = 0; i < 6; i++)
	{
		if (i >= InventorySlotTexts.Num() || i >= SellButtons.Num()) continue;

		int32 ItemID = OwnerCharacter->Inventory[i];
		if (ItemID > 0)
		{
			const FSkylandersItemData* Item = USkylandersItemCatalog::FindItem(ItemID);
			if (Item)
			{
				InventorySlotTexts[i]->SetText(FText::FromString(Item->ItemName));
				InventorySlotTexts[i]->SetColorAndOpacity(FSlateColor(FLinearColor::White));

				// Show sell price
				if (i < InventorySlotStatTexts.Num() && InventorySlotStatTexts[i])
				{
					InventorySlotStatTexts[i]->SetText(FText::FromString(
						FString::Printf(TEXT("Sell: %dg"), Item->GetSellPrice())));
					InventorySlotStatTexts[i]->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.4f)));
				}

				SellButtons[i]->SetVisibility(
					OwnerCharacter->bIsInSpawnArea ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			}
		}
		else
		{
			InventorySlotTexts[i]->SetText(FText::FromString(TEXT("Empty")));
			InventorySlotTexts[i]->SetColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.4f, 0.4f)));

			if (i < InventorySlotStatTexts.Num() && InventorySlotStatTexts[i])
			{
				InventorySlotStatTexts[i]->SetText(FText::FromString(TEXT("")));
			}

			SellButtons[i]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void USkylandersShopWidget::UpdateHeader()
{
	if (!OwnerCharacter) return;

	if (GoldText)
	{
		GoldText->SetText(FText::FromString(FString::Printf(TEXT("Gold: %d"), OwnerCharacter->Coins)));
	}

	if (SpawnAreaText)
	{
		if (OwnerCharacter->bIsInSpawnArea)
		{
			SpawnAreaText->SetText(FText::FromString(TEXT("IN SPAWN - Ready to Buy!")));
			SpawnAreaText->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 1.0f, 0.3f)));
		}
		else
		{
			SpawnAreaText->SetText(FText::FromString(TEXT("NOT IN SPAWN - Browse Only")));
			SpawnAreaText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.3f, 0.3f)));
		}
	}
}

void USkylandersShopWidget::UpdateBuyButtons()
{
	if (!OwnerCharacter) return;

	const TArray<FSkylandersItemData>& AllItems = USkylandersItemCatalog::GetAllItems();

	// Check if inventory is full
	bool bInventoryFull = true;
	for (int32 i = 0; i < OwnerCharacter->MaxInventorySlots; i++)
	{
		if (OwnerCharacter->Inventory[i] == 0)
		{
			bInventoryFull = false;
			break;
		}
	}

	for (int32 i = 0; i < AllItems.Num() && i < ItemBuyButtons.Num(); i++)
	{
		if (!ItemBuyButtons[i]) continue;

		bool bCanAfford = (OwnerCharacter->Coins >= AllItems[i].Cost);
		bool bInSpawn = OwnerCharacter->bIsInSpawnArea;
		bool bEnabled = bCanAfford && bInSpawn && !bInventoryFull;

		ItemBuyButtons[i]->SetIsEnabled(bEnabled);

		// Update cost text color: green if affordable, red if not
		if (i < ItemCostTexts.Num() && ItemCostTexts[i])
		{
			if (bCanAfford)
			{
				ItemCostTexts[i]->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.9f, 0.3f)));
			}
			else
			{
				ItemCostTexts[i]->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.3f, 0.3f)));
			}
		}
	}
}

// ===== Filter Callbacks =====
void USkylandersShopWidget::OnFilterAll()     { CurrentFilter = -1; ApplyFilter(); }
void USkylandersShopWidget::OnFilterOffense() { CurrentFilter = 0;  ApplyFilter(); }
void USkylandersShopWidget::OnFilterDefense() { CurrentFilter = 1;  ApplyFilter(); }
void USkylandersShopWidget::OnFilterUtility() { CurrentFilter = 2;  ApplyFilter(); }

// ===== Buy Callbacks =====
void USkylandersShopWidget::OnBuyItem(int32 ItemID)
{
	if (!OwnerCharacter) return;
	OwnerCharacter->BuyItem(ItemID);
}

void USkylandersShopWidget::OnBuyItem1()  { OnBuyItem(1); }
void USkylandersShopWidget::OnBuyItem2()  { OnBuyItem(2); }
void USkylandersShopWidget::OnBuyItem3()  { OnBuyItem(3); }
void USkylandersShopWidget::OnBuyItem4()  { OnBuyItem(4); }
void USkylandersShopWidget::OnBuyItem5()  { OnBuyItem(5); }
void USkylandersShopWidget::OnBuyItem6()  { OnBuyItem(6); }
void USkylandersShopWidget::OnBuyItem7()  { OnBuyItem(7); }
void USkylandersShopWidget::OnBuyItem8()  { OnBuyItem(8); }
void USkylandersShopWidget::OnBuyItem9()  { OnBuyItem(9); }
void USkylandersShopWidget::OnBuyItem10() { OnBuyItem(10); }
void USkylandersShopWidget::OnBuyItem11() { OnBuyItem(11); }
void USkylandersShopWidget::OnBuyItem12() { OnBuyItem(12); }

// ===== Sell Callbacks =====
void USkylandersShopWidget::OnSellSlot0() { if (OwnerCharacter) { OwnerCharacter->SellItem(0); } }
void USkylandersShopWidget::OnSellSlot1() { if (OwnerCharacter) { OwnerCharacter->SellItem(1); } }
void USkylandersShopWidget::OnSellSlot2() { if (OwnerCharacter) { OwnerCharacter->SellItem(2); } }
void USkylandersShopWidget::OnSellSlot3() { if (OwnerCharacter) { OwnerCharacter->SellItem(3); } }
void USkylandersShopWidget::OnSellSlot4() { if (OwnerCharacter) { OwnerCharacter->SellItem(4); } }
void USkylandersShopWidget::OnSellSlot5() { if (OwnerCharacter) { OwnerCharacter->SellItem(5); } }

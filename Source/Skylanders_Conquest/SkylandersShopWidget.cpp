// Skylanders Conquest - Shop UI Widget Implementation (Grid Layout)

#include "SkylandersShopWidget.h"
#include "SkylandersCharacter.h"
#include "SkylandersItemCatalog.h"
#include "SkylandersItem.h"
#include "SkylandersSmokeTest.h"
#include "SkylandersUIStyle.h"
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
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/ScrollBox.h"
#include "Blueprint/WidgetTree.h"
#include "SkylandersSoundLibrary.h"

// Running counter for unique widget names
static int32 GShopWidgetCounter = 0;

// Card and inventory fills; rims are the category color (cards) or the shared
// panel outline (slots)
static const FLinearColor ShopCardFill(0.05f, 0.06f, 0.11f, 1.0f);
static const FLinearColor ShopSlotFill(0.05f, 0.06f, 0.11f, 1.0f);
static const FLinearColor ShopStatColor(0.75f, 0.80f, 0.65f, 1.0f);
static const FLinearColor ShopUnaffordable(0.75f, 0.32f, 0.30f, 1.0f);
static const FLinearColor ShopInSpawn(0.35f, 0.95f, 0.45f, 1.0f);

static FLinearColor ShopCategoryColor(ESkylandersItemCategory Category)
{
	switch (Category)
	{
	case ESkylandersItemCategory::Offense: return FLinearColor(0.90f, 0.35f, 0.32f, 1.0f);
	case ESkylandersItemCategory::Defense: return FLinearColor(0.38f, 0.62f, 1.00f, 1.0f);
	case ESkylandersItemCategory::Utility: return FLinearColor(0.38f, 0.88f, 0.52f, 1.0f);
	}
	return SkylandersUI::Dim;
}

static const TCHAR* ShopCategoryLabel(ESkylandersItemCategory Category)
{
	switch (Category)
	{
	case ESkylandersItemCategory::Offense: return TEXT("OFFENSE");
	case ESkylandersItemCategory::Defense: return TEXT("DEFENSE");
	case ESkylandersItemCategory::Utility: return TEXT("UTILITY");
	}
	return TEXT("ITEM");
}

const TArray<int32>& USkylandersShopWidget::GetRecommendedBuild(const FString& CharacterName)
{
	// Final six of each build in docs/ITEMS-DESIGN.md section 4, in the order
	// they are bought (the early component that gets sold is left out).
	static const TArray<int32> TriggerHappy = { 15, 6, 23, 7, 35, 8 };   // ranged crit
	static const TArray<int32> Hex = { 31, 23, 34, 24, 2, 20 };          // mage CDR / pen
	static const TArray<int32> TreeRex = { 26, 29, 12, 30, 13, 36 };     // tank
	// Bruiser (Spyro): health + power + CDR. Celerity boots for the HP and
	// chase speed, Giant's Girdle for HP + power, Sky-Iron for protections +
	// CDR, Traptanium for the power spike, Core of Light to finish the CDR at
	// 35% (under the 40% cap) and Blade of Fire for raw power: 9,450 g.
	static const TArray<int32> Bruiser = { 19, 28, 27, 24, 30, 2 };

	if (CharacterName == TEXT("Trigger Happy")) return TriggerHappy;
	if (CharacterName == TEXT("Hex")) return Hex;
	if (CharacterName == TEXT("Tree Rex")) return TreeRex;
	return Bruiser;
}

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

void USkylandersShopWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// Every OpenShop re-adds the widget to the viewport, so this counts opens
	SKY_COUNT("ui_shop_opened");
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
	return SkylandersUI::MakeText(WidgetTree, *FString::Printf(TEXT("ST_%d"), GShopWidgetCounter++),
		Content, FontSize, Color, ETextJustify::Left);
}

UButton* USkylandersShopWidget::MakeButton(const FString& Label, int32 FontSize)
{
	if (!WidgetTree) return nullptr;
	return SkylandersUI::MakeButton(WidgetTree, *FString::Printf(TEXT("SB_%d"), GShopWidgetCounter++), Label, FontSize);
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
	if (Stats.CooldownReduction > 0.0f)
		Parts.Add(FString::Printf(TEXT("+%.0f%% CDR"), Stats.CooldownReduction * 100.0f));
	if (Stats.Penetration > 0.0f)
		Parts.Add(FString::Printf(TEXT("+%.0f Pen"), Stats.Penetration));

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

	// ===== PANEL =====
	UBorder* BG = SkylandersUI::MakePanel(WidgetTree, TEXT("ShopBG"), SkylandersUI::PanelBG, 14.0f,
		SkylandersUI::PanelOutline, 1.0f, FMargin(24.0f, 18.0f));

	UCanvasPanelSlot* BGSlot = Root->AddChildToCanvas(BG);
	BGSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
	BGSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	BGSlot->SetSize(FVector2D(960.0f, 740.0f));
	BGSlot->SetPosition(FVector2D(0.0f, 0.0f));
	BGSlot->SetAutoSize(false);

	UVerticalBox* MainVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShopVBox"));
	BG->AddChild(MainVBox);

	auto AddRule = [&](const TCHAR* Name, FMargin RulePadding)
	{
		UBorder* Rule = SkylandersUI::MakePanel(WidgetTree, Name, SkylandersUI::PanelOutline, 0.0f,
			FLinearColor::Transparent, 0.0f, FMargin(0.0f));
		USizeBox* RuleBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		RuleBox->SetHeightOverride(1.0f);
		RuleBox->AddChild(Rule);
		MainVBox->AddChildToVerticalBox(RuleBox)->SetPadding(RulePadding);
	};

	// ===== ROW 1: Title + Gold + Spawn Status =====
	{
		UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TitleRow"));
		UVerticalBoxSlot* TRSlot = MainVBox->AddChildToVerticalBox(TitleRow);
		TRSlot->SetPadding(FMargin(0, 0, 0, 10));

		UTextBlock* Title = SkylandersUI::MakeTitle(WidgetTree, TEXT("ShopTitle"), TEXT("SHOP"), 26, SkylandersUI::Gold, ETextJustify::Left);
		UHorizontalBoxSlot* TSlot = TitleRow->AddChildToHorizontalBox(Title);
		TSlot->SetPadding(FMargin(0, 0, 24, 0));
		TSlot->SetVerticalAlignment(VAlign_Center);

		// Coin glyph + amount, the HUD's gold readout
		UHorizontalBoxSlot* CoinSlot = TitleRow->AddChildToHorizontalBox(SkylandersUI::MakeCoin(WidgetTree, 22.0f));
		CoinSlot->SetVerticalAlignment(VAlign_Center);
		CoinSlot->SetPadding(FMargin(0, 0, 8, 0));

		GoldText = MakeText(TEXT("0"), 22, SkylandersUI::Gold);
		SkylandersUI::SetFontStyle(GoldText, true, false);
		UHorizontalBoxSlot* GSlot = TitleRow->AddChildToHorizontalBox(GoldText);
		GSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		GSlot->SetVerticalAlignment(VAlign_Center);

		SpawnAreaText = MakeText(TEXT("NOT IN SPAWN"), 14, SkylandersUI::Red);
		SkylandersUI::SetFontStyle(SpawnAreaText, true, false);
		UHorizontalBoxSlot* SASlot = TitleRow->AddChildToHorizontalBox(SpawnAreaText);
		SASlot->SetVerticalAlignment(VAlign_Center);
	}

	// ===== ROW 2: Filter Buttons + Spawn Reminder =====
	{
		UHorizontalBox* FilterRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FilterRow"));
		UVerticalBoxSlot* FRSlot = MainVBox->AddChildToVerticalBox(FilterRow);
		FRSlot->SetPadding(FMargin(0, 0, 0, 10));

		// Index in FilterButtons is CurrentFilter + 1 (ApplyFilter relies on it)
		auto AddFilterBtn = [&](const FString& Label, FName FuncName)
		{
			UButton* Btn = MakeButton(Label, 13);
			FScriptDelegate Del;
			Del.BindUFunction(this, FuncName);
			Btn->OnClicked.Add(Del);
			USizeBox* BtnBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			BtnBox->SetHeightOverride(32.0f);
			BtnBox->SetMinDesiredWidth(96.0f);
			BtnBox->AddChild(Btn);
			UHorizontalBoxSlot* BSlot = FilterRow->AddChildToHorizontalBox(BtnBox);
			BSlot->SetPadding(FMargin(0, 0, 6, 0));
			FilterButtons.Add(Btn);
		};

		AddFilterBtn(TEXT("ALL"), TEXT("OnFilterAll"));
		AddFilterBtn(TEXT("OFFENSE"), TEXT("OnFilterOffense"));
		AddFilterBtn(TEXT("DEFENSE"), TEXT("OnFilterDefense"));
		AddFilterBtn(TEXT("UTILITY"), TEXT("OnFilterUtility"));
		AddFilterBtn(TEXT("RECOMMENDED"), TEXT("OnFilterRecommended"));

		// Spawn area reminder
		UTextBlock* Reminder = MakeText(TEXT("Buy and sell in your spawn area"), 11, SkylandersUI::Dim);
		UHorizontalBoxSlot* RSlot = FilterRow->AddChildToHorizontalBox(Reminder);
		RSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		RSlot->SetHorizontalAlignment(HAlign_Right);
		RSlot->SetVerticalAlignment(VAlign_Center);
	}

	AddRule(TEXT("Sep1"), FMargin(0, 0, 0, 8));

	// ===== RECOMMENDED HINT (only under the RECOMMENDED filter) =====
	{
		const FString Who = (OwnerCharacter && !OwnerCharacter->CharacterName.IsEmpty())
			? OwnerCharacter->CharacterName : FString(TEXT("a bruiser"));
		RecommendedHint = MakeText(FString::Printf(TEXT("Recommended build for %s · buy in order, #1 to #6"), *Who),
			12, SkylandersUI::Gold);
		RecommendedHint->SetVisibility(ESlateVisibility::Collapsed);
		MainVBox->AddChildToVerticalBox(RecommendedHint)->SetPadding(FMargin(2, 0, 0, 6));
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

		// Build one card per catalog item (will be arranged into grid rows later)
		for (const FSkylandersItemData& Item : AllItems)
		{
			const FLinearColor CatColor = ShopCategoryColor(Item.Category);

			// Card: rounded fill, 1px rim in the category color
			UBorder* CardBorder = SkylandersUI::MakePanel(WidgetTree, *FString::Printf(TEXT("Card_%d"), Item.ItemID),
				ShopCardFill, 8.0f, CatColor, 1.0f, FMargin(12.0f, 10.0f));

			ItemCardBorders.Add(CardBorder);
			ItemCardCategories.Add(static_cast<int32>(Item.Category));

			UVerticalBox* CardVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
				*FString::Printf(TEXT("CardVB_%d"), Item.ItemID));
			CardBorder->AddChild(CardVBox);

			// Category tag (small, colored) + build-order badge on the right
			{
				UHorizontalBox* TagRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
				UTextBlock* CatText = MakeText(ShopCategoryLabel(Item.Category), 9, CatColor);
				SkylandersUI::SetFontStyle(CatText, true, false);
				UHorizontalBoxSlot* CatSlot = TagRow->AddChildToHorizontalBox(CatText);
				CatSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				CatSlot->SetVerticalAlignment(VAlign_Center);

				UTextBlock* OrderText = MakeText(TEXT(""), 12, SkylandersUI::Gold);
				SkylandersUI::SetFontStyle(OrderText, true, true);
				TagRow->AddChildToHorizontalBox(OrderText)->SetVerticalAlignment(VAlign_Center);
				ItemCardOrderTexts.Add(OrderText);

				CardVBox->AddChildToVerticalBox(TagRow)->SetPadding(FMargin(0, 0, 0, 2));
			}

			// Item name
			UTextBlock* NameText = MakeText(Item.ItemName, 15, SkylandersUI::White);
			SkylandersUI::SetFontStyle(NameText, true, false);
			UVerticalBoxSlot* NameSlot = CardVBox->AddChildToVerticalBox(NameText);
			NameSlot->SetPadding(FMargin(0, 0, 0, 4));

			// Stat summary (multi-line)
			UTextBlock* StatText = MakeText(BuildStatSummary(Item.Stats), 11, ShopStatColor);
			StatText->SetAutoWrapText(true);
			UVerticalBoxSlot* StatSlot = CardVBox->AddChildToVerticalBox(StatText);
			StatSlot->SetPadding(FMargin(0, 0, 0, 4));

			// Description: the tooltip-ish line under the numbers
			UTextBlock* DescText = MakeText(Item.Description, 10, SkylandersUI::Dim);
			DescText->SetAutoWrapText(true);
			UVerticalBoxSlot* DescSlot = CardVBox->AddChildToVerticalBox(DescText);
			DescSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			DescSlot->SetPadding(FMargin(0, 0, 0, 8));

			// Bottom row: coin + cost, BUY button
			UHorizontalBox* BottomRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
				*FString::Printf(TEXT("CardBot_%d"), Item.ItemID));
			CardVBox->AddChildToVerticalBox(BottomRow);

			UHorizontalBoxSlot* CoinSlot = BottomRow->AddChildToHorizontalBox(SkylandersUI::MakeCoin(WidgetTree, 16.0f));
			CoinSlot->SetVerticalAlignment(VAlign_Center);
			CoinSlot->SetPadding(FMargin(0, 0, 6, 0));

			UTextBlock* CostText = MakeText(FString::FromInt(Item.Cost), 14, SkylandersUI::Gold);
			SkylandersUI::SetFontStyle(CostText, true, false);
			UHorizontalBoxSlot* CostSlot = BottomRow->AddChildToHorizontalBox(CostText);
			CostSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			CostSlot->SetVerticalAlignment(VAlign_Center);
			ItemCostTexts.Add(CostText);

			UButton* BuyBtn = MakeButton(TEXT("BUY"), 12);
			USizeBox* BuyBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			BuyBox->SetWidthOverride(72.0f);
			BuyBox->SetHeightOverride(28.0f);
			BuyBox->AddChild(BuyBtn);
			UHorizontalBoxSlot* BuySlot = BottomRow->AddChildToHorizontalBox(BuyBox);
			BuySlot->SetVerticalAlignment(VAlign_Center);
			ItemBuyButtons.Add(BuyBtn);

			// Bind buy delegate through a per-button handler so every catalog item
			// gets a working button by construction, whatever its ID or the catalog size.
			USkylandersShopBuyHandler* Handler = NewObject<USkylandersShopBuyHandler>(this);
			Handler->OwnerWidget = this;
			Handler->ItemID = Item.ItemID;
			BuyHandlers.Add(Handler);
			BuyBtn->OnClicked.AddDynamic(Handler, &USkylandersShopBuyHandler::OnClicked);
		}

		// Arrange cards into grid rows (3 per row)
		RebuildGrid();
	}

	AddRule(TEXT("Sep2"), FMargin(0, 0, 0, 8));

	// ===== INVENTORY LABEL =====
	{
		UHorizontalBox* InvHeader = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("InvHeader"));
		UVerticalBoxSlot* IHSlot = MainVBox->AddChildToVerticalBox(InvHeader);
		IHSlot->SetPadding(FMargin(0, 0, 0, 6));

		UTextBlock* InvLabel = SkylandersUI::MakeTitle(WidgetTree, TEXT("InvLabel"), TEXT("INVENTORY"), 15, SkylandersUI::White, ETextJustify::Left);
		UHorizontalBoxSlot* ILSlot = InvHeader->AddChildToHorizontalBox(InvLabel);
		ILSlot->SetVerticalAlignment(VAlign_Center);

		UTextBlock* CloseHint = MakeText(TEXT("[B] Close Shop"), 12, SkylandersUI::Dim);
		UHorizontalBoxSlot* CHSlot = InvHeader->AddChildToHorizontalBox(CloseHint);
		CHSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CHSlot->SetHorizontalAlignment(HAlign_Right);
		CHSlot->SetVerticalAlignment(VAlign_Center);
	}

	// ===== INVENTORY SLOTS (6 slots in a horizontal row) =====
	{
		UHorizontalBox* InvRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("InvRow"));
		UVerticalBoxSlot* InvRowSlot = MainVBox->AddChildToVerticalBox(InvRow);
		InvRowSlot->SetPadding(FMargin(0, 0, 0, 0));

		InventorySlotTexts.SetNum(6);
		InventorySlotStatTexts.SetNum(6);
		SellButtons.SetNum(6);

		for (int32 i = 0; i < 6; i++)
		{
			UBorder* SlotBorder = SkylandersUI::MakePanel(WidgetTree, *FString::Printf(TEXT("InvBorder_%d"), i),
				ShopSlotFill, 6.0f, SkylandersUI::PanelOutline, 1.0f, FMargin(8.0f, 6.0f));

			UHorizontalBoxSlot* SBSlot = InvRow->AddChildToHorizontalBox(SlotBorder);
			SBSlot->SetPadding(FMargin(0, 0, (i < 5) ? 6.0f : 0.0f, 0));
			SBSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

			UVerticalBox* SlotVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
				*FString::Printf(TEXT("InvVB_%d"), i));
			SlotBorder->AddChild(SlotVBox);

			// Item name
			InventorySlotTexts[i] = MakeText(TEXT("Empty"), 11, SkylandersUI::Dim);
			InventorySlotTexts[i]->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
			SlotVBox->AddChildToVerticalBox(InventorySlotTexts[i]);

			// Sell price line
			InventorySlotStatTexts[i] = MakeText(TEXT(""), 9, SkylandersUI::Dim);
			UVerticalBoxSlot* StatLine = SlotVBox->AddChildToVerticalBox(InventorySlotStatTexts[i]);
			StatLine->SetPadding(FMargin(0, 1, 0, 0));

			// Sell button
			SellButtons[i] = MakeButton(TEXT("SELL"), 10);
			SellButtons[i]->SetVisibility(ESlateVisibility::Collapsed);
			UVerticalBoxSlot* BtnVSlot = SlotVBox->AddChildToVerticalBox(SellButtons[i]);
			BtnVSlot->SetPadding(FMargin(0, 4, 0, 0));
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

	for (UTextBlock* OrderText : ItemCardOrderTexts)
	{
		if (OrderText) OrderText->SetText(FText::GetEmpty());
	}

	// Collect visible cards
	TArray<UBorder*> VisibleCards;
	if (CurrentFilter == FilterRecommended)
	{
		// The character's build, in buy order; card i is catalog item i
		const TArray<FSkylandersItemData>& AllItems = USkylandersItemCatalog::GetAllItems();
		const TArray<int32>& Build = GetRecommendedBuild(OwnerCharacter ? OwnerCharacter->CharacterName : FString());
		for (int32 ItemID : Build)
		{
			const int32 Idx = AllItems.IndexOfByPredicate([ItemID](const FSkylandersItemData& It) { return It.ItemID == ItemID; });
			if (!ItemCardBorders.IsValidIndex(Idx) || !ItemCardBorders[Idx]) continue;
			VisibleCards.Add(ItemCardBorders[Idx]);
			if (ItemCardOrderTexts.IsValidIndex(Idx) && ItemCardOrderTexts[Idx])
			{
				ItemCardOrderTexts[Idx]->SetText(FText::FromString(FString::Printf(TEXT("#%d"), VisibleCards.Num())));
			}
		}
	}
	else
	{
		for (int32 i = 0; i < ItemCardBorders.Num(); i++)
		{
			if (!ItemCardBorders[i]) continue;
			bool bShow = (CurrentFilter < 0) || (ItemCardCategories[i] == CurrentFilter);
			if (bShow)
			{
				VisibleCards.Add(ItemCardBorders[i]);
			}
		}
	}
	VisibleCardCount = VisibleCards.Num();

	// Arrange into rows of 3
	const int32 Columns = 3;
	for (int32 Row = 0; Row * Columns < VisibleCards.Num(); Row++)
	{
		UHorizontalBox* GridRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
			*FString::Printf(TEXT("GR_%d"), GShopWidgetCounter++));
		UVerticalBoxSlot* RowVSlot = ItemGridContainer->AddChildToVerticalBox(GridRow);
		RowVSlot->SetPadding(FMargin(0, 0, 0, 8));

		for (int32 Col = 0; Col < Columns; Col++)
		{
			int32 Idx = Row * Columns + Col;
			if (Idx < VisibleCards.Num())
			{
				UHorizontalBoxSlot* CardHSlot = GridRow->AddChildToHorizontalBox(VisibleCards[Idx]);
				CardHSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				CardHSlot->SetPadding(FMargin(0, 0, (Col < Columns - 1) ? 8.0f : 0.0f, 0));
			}
			else
			{
				// Empty spacer to keep grid aligned
				USpacer* EmptySpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(),
					*FString::Printf(TEXT("GSp_%d"), GShopWidgetCounter++));
				UHorizontalBoxSlot* SpHSlot = GridRow->AddChildToHorizontalBox(EmptySpacer);
				SpHSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				SpHSlot->SetPadding(FMargin(0, 0, (Col < Columns - 1) ? 8.0f : 0.0f, 0));
			}
		}
	}
}

void USkylandersShopWidget::ApplyFilter()
{
	// All cards are always "visible" in terms of ESlateVisibility;
	// the grid rebuild handles which ones appear
	RebuildGrid();

	// The active filter button gets the gold rim and a gold label
	for (int32 i = 0; i < FilterButtons.Num(); i++)
	{
		const bool bActive = (i == CurrentFilter + 1);
		SkylandersUI::SetButtonHighlight(FilterButtons[i], bActive);
		if (UTextBlock* Label = SkylandersUI::GetButtonLabel(FilterButtons[i]))
		{
			Label->SetColorAndOpacity(FSlateColor(bActive ? SkylandersUI::Gold : SkylandersUI::White));
		}
	}

	if (RecommendedHint)
	{
		RecommendedHint->SetVisibility(CurrentFilter == FilterRecommended
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void USkylandersShopWidget::SetFilter(int32 Filter)
{
	CurrentFilter = FMath::Clamp(Filter, -1, FilterRecommended);
	ApplyFilter();
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
				InventorySlotTexts[i]->SetColorAndOpacity(FSlateColor(SkylandersUI::White));

				// Show sell price
				if (i < InventorySlotStatTexts.Num() && InventorySlotStatTexts[i])
				{
					InventorySlotStatTexts[i]->SetText(FText::FromString(
						FString::Printf(TEXT("Sells for %dg"), Item->GetSellPrice())));
					InventorySlotStatTexts[i]->SetColorAndOpacity(FSlateColor(SkylandersUI::Gold * FLinearColor(1.0f, 1.0f, 1.0f, 0.75f)));
				}

				SellButtons[i]->SetVisibility(
					OwnerCharacter->bIsInSpawnArea ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			}
		}
		else
		{
			InventorySlotTexts[i]->SetText(FText::FromString(TEXT("Empty")));
			InventorySlotTexts[i]->SetColorAndOpacity(FSlateColor(SkylandersUI::Dim * FLinearColor(1.0f, 1.0f, 1.0f, 0.6f)));

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
		GoldText->SetText(FText::AsNumber(OwnerCharacter->Coins));
	}

	if (SpawnAreaText)
	{
		if (OwnerCharacter->bIsInSpawnArea)
		{
			SpawnAreaText->SetText(FText::FromString(TEXT("IN SPAWN · READY TO BUY")));
			SpawnAreaText->SetColorAndOpacity(FSlateColor(ShopInSpawn));
		}
		else
		{
			SpawnAreaText->SetText(FText::FromString(TEXT("NOT IN SPAWN · BROWSE ONLY")));
			SpawnAreaText->SetColorAndOpacity(FSlateColor(SkylandersUI::Red));
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

		// Cost reads gold when affordable, red when not
		if (i < ItemCostTexts.Num() && ItemCostTexts[i])
		{
			ItemCostTexts[i]->SetColorAndOpacity(FSlateColor(bCanAfford ? SkylandersUI::Gold : ShopUnaffordable));
		}
	}
}

// Every shop button routes through one of the callbacks below; each plays the
// UI click so the shop feels responsive even when a purchase is refused.
static void PlayShopClick(const UObject* Widget)
{
	SkylandersSound::Play2D(Widget, TEXT("ui_click"));
}

// ===== Filter Callbacks =====
void USkylandersShopWidget::OnFilterAll()         { PlayShopClick(this); SetFilter(-1); }
void USkylandersShopWidget::OnFilterOffense()     { PlayShopClick(this); SetFilter(0); }
void USkylandersShopWidget::OnFilterDefense()     { PlayShopClick(this); SetFilter(1); }
void USkylandersShopWidget::OnFilterUtility()     { PlayShopClick(this); SetFilter(2); }
void USkylandersShopWidget::OnFilterRecommended() { PlayShopClick(this); SetFilter(FilterRecommended); }

// ===== Buy Callbacks =====
void USkylandersShopWidget::OnBuyItem(int32 ItemID)
{
	PlayShopClick(this);
	if (!OwnerCharacter) return;
	OwnerCharacter->BuyItem(ItemID);
}

void USkylandersShopBuyHandler::OnClicked()
{
	if (OwnerWidget)
	{
		OwnerWidget->OnBuyItem(ItemID);
	}
}

// ===== Sell Callbacks =====
void USkylandersShopWidget::OnSellSlot0() { PlayShopClick(this); if (OwnerCharacter) { OwnerCharacter->SellItem(0); } }
void USkylandersShopWidget::OnSellSlot1() { PlayShopClick(this); if (OwnerCharacter) { OwnerCharacter->SellItem(1); } }
void USkylandersShopWidget::OnSellSlot2() { PlayShopClick(this); if (OwnerCharacter) { OwnerCharacter->SellItem(2); } }
void USkylandersShopWidget::OnSellSlot3() { PlayShopClick(this); if (OwnerCharacter) { OwnerCharacter->SellItem(3); } }
void USkylandersShopWidget::OnSellSlot4() { PlayShopClick(this); if (OwnerCharacter) { OwnerCharacter->SellItem(4); } }
void USkylandersShopWidget::OnSellSlot5() { PlayShopClick(this); if (OwnerCharacter) { OwnerCharacter->SellItem(5); } }

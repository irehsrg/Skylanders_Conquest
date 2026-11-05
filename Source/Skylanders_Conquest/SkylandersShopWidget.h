// Skylanders Conquest - Shop UI Widget (fully C++ driven, grid layout)
//
// One rounded panel on the shared SkylandersUI palette: a title row (SHOP, the
// coin + gold, spawn status), a filter row (ALL / OFFENSE / DEFENSE / UTILITY /
// RECOMMENDED, the active one rimmed in gold), a scrolling 3-column grid of item
// cards (outlined in their category color: name, stats, description, coin + cost,
// BUY) and the 6-slot inventory row with SELL buttons. RECOMMENDED shows the
// current character's six-item build in buy order (docs/ITEMS-DESIGN.md section 4).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkylandersShopWidget.generated.h"

class ASkylandersCharacter;
class UTextBlock;
class UButton;
class UVerticalBox;
class UHorizontalBox;
class UBorder;
class UScrollBox;
class USkylandersShopWidget;

// One per BUY button: UButton::OnClicked carries no payload, so each button binds
// to its own handler that remembers which catalog item it buys. Replaces a fixed
// OnBuyItem1..N UFUNCTION fan-out that silently left any item past N unbuyable.
UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersShopBuyHandler : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	USkylandersShopWidget* OwnerWidget = nullptr;

	int32 ItemID = 0;

	UFUNCTION()
	void OnClicked();
};

UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersShopWidget : public UUserWidget
{
	GENERATED_BODY()

	friend class USkylandersShopBuyHandler;

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Filter values for SetFilter: -1 all, 0 offense, 1 defense, 2 utility, 3 recommended. */
	static constexpr int32 FilterRecommended = 3;

	/** Switches the grid to a filter, as the filter buttons do (no click sound). */
	void SetFilter(int32 Filter);

	/** Cards the grid currently shows (after the filter). */
	int32 GetVisibleCardCount() const { return VisibleCardCount; }

	/**
	 * A character's recommended six items in buy order, keyed by CharacterName
	 * ("Trigger Happy", "Hex", "Tree Rex", "Spyro"). Unknown names get the
	 * bruiser build (Spyro's).
	 */
	static const TArray<int32>& GetRecommendedBuild(const FString& CharacterName);

private:
	// Owner character reference
	UPROPERTY()
	ASkylandersCharacter* OwnerCharacter;

	// Header texts
	UPROPERTY()
	UTextBlock* GoldText;

	UPROPERTY()
	UTextBlock* SpawnAreaText;

	// Item card borders (one per catalog item, used for filter show/hide)
	UPROPERTY()
	TArray<UBorder*> ItemCardBorders;

	// Category per card (0=offense, 1=defense, 2=utility)
	TArray<int32> ItemCardCategories;

	// Build-order badge per card ("#1".."#6"), filled only under RECOMMENDED
	UPROPERTY()
	TArray<UTextBlock*> ItemCardOrderTexts;

	// "Recommended for X" line above the grid; collapsed under the other filters
	UPROPERTY()
	UTextBlock* RecommendedHint;

	int32 VisibleCardCount = 0;

	// Cost text per card (to color red/green based on affordability)
	UPROPERTY()
	TArray<UTextBlock*> ItemCostTexts;

	// Buy buttons per card (to disable when can't afford or inventory full)
	UPROPERTY()
	TArray<UButton*> ItemBuyButtons;

	// Click handlers bound to ItemBuyButtons (parallel array). Held here because a
	// delegate binding is weak; without this reference GC would collect them.
	UPROPERTY()
	TArray<USkylandersShopBuyHandler*> BuyHandlers;

	// Inventory slot texts (6)
	UPROPERTY()
	TArray<UTextBlock*> InventorySlotTexts;

	// Inventory slot stat texts (6)
	UPROPERTY()
	TArray<UTextBlock*> InventorySlotStatTexts;

	UPROPERTY()
	TArray<UButton*> SellButtons;

	// Grid rows container (for filter visibility)
	UPROPERTY()
	UVerticalBox* ItemGridContainer;

	// Current filter (-1 = all, 0 = offense, 1 = defense, 2 = utility, 3 = recommended)
	int32 CurrentFilter;

	// Filter button references (to highlight active filter)
	UPROPERTY()
	TArray<UButton*> FilterButtons;

	// Build the UI layout
	void BuildLayout();

	// Rebuild grid rows based on visible items (3 per row)
	void RebuildGrid();

	// Show/hide items based on filter, and mark the active filter button
	void ApplyFilter();

	// Update inventory display
	void UpdateInventory();

	// Update gold and spawn area status
	void UpdateHeader();

	// Update buy button states (enabled/disabled based on gold + spawn area)
	void UpdateBuyButtons();

	// Build a stat summary string from item stats
	static FString BuildStatSummary(const struct FSkylandersItemStats& Stats);

	// Filter callbacks
	UFUNCTION() void OnFilterAll();
	UFUNCTION() void OnFilterOffense();
	UFUNCTION() void OnFilterDefense();
	UFUNCTION() void OnFilterUtility();
	UFUNCTION() void OnFilterRecommended();

	// Buy item by ID
	void OnBuyItem(int32 ItemID);

	// Sell handlers (6 slots)
	UFUNCTION() void OnSellSlot0();
	UFUNCTION() void OnSellSlot1();
	UFUNCTION() void OnSellSlot2();
	UFUNCTION() void OnSellSlot3();
	UFUNCTION() void OnSellSlot4();
	UFUNCTION() void OnSellSlot5();

	// Helpers
	UTextBlock* MakeText(const FString& Content, int32 FontSize = 14, FLinearColor Color = FLinearColor::White);
	UButton* MakeButton(const FString& Label, int32 FontSize = 12);
};

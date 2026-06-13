// Skylanders Conquest - Shop UI Widget (fully C++ driven, grid layout)

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

UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	// Owner character reference
	UPROPERTY()
	ASkylandersCharacter* OwnerCharacter;

	// Header texts
	UPROPERTY()
	UTextBlock* GoldText;

	UPROPERTY()
	UTextBlock* SpawnAreaText;

	// Item card borders (12 items, used for filter show/hide)
	UPROPERTY()
	TArray<UBorder*> ItemCardBorders;

	// Category per card (0=offense, 1=defense, 2=utility)
	TArray<int32> ItemCardCategories;

	// Cost text per card (to color red/green based on affordability)
	UPROPERTY()
	TArray<UTextBlock*> ItemCostTexts;

	// Buy buttons per card (to disable when can't afford or inventory full)
	UPROPERTY()
	TArray<UButton*> ItemBuyButtons;

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

	// Current filter (-1 = all, 0 = offense, 1 = defense, 2 = utility)
	int32 CurrentFilter;

	// Filter button references (to highlight active filter)
	UPROPERTY()
	TArray<UButton*> FilterButtons;

	// Build the UI layout
	void BuildLayout();

	// Rebuild grid rows based on visible items (3 per row)
	void RebuildGrid();

	// Show/hide items based on filter
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

	// Buy item by ID
	void OnBuyItem(int32 ItemID);

	// Buy handlers (12 items)
	UFUNCTION() void OnBuyItem1();
	UFUNCTION() void OnBuyItem2();
	UFUNCTION() void OnBuyItem3();
	UFUNCTION() void OnBuyItem4();
	UFUNCTION() void OnBuyItem5();
	UFUNCTION() void OnBuyItem6();
	UFUNCTION() void OnBuyItem7();
	UFUNCTION() void OnBuyItem8();
	UFUNCTION() void OnBuyItem9();
	UFUNCTION() void OnBuyItem10();
	UFUNCTION() void OnBuyItem11();
	UFUNCTION() void OnBuyItem12();

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

// Skylanders Conquest - Main gameplay HUD (vitals, abilities, items, gold)
//
// Replaces the WBP_MainHUD Blueprint. Built in C++ with the shared
// SkylandersUI look so every panel matches instead of each one picking its own
// fonts and flat bars. Layout at 1080p reference:
//   bottom-center: one rounded panel [portrait + level badge][HP / mana / XP bars][4 ability slots],
//                  six item cells underneath it
//   top-right:     gold counter, sitting just above the minimap
// The character pushes values in through the setters; the widget never reads
// gameplay state itself, so the same widget works for every playable hero.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkylandersMainHUDWidget.generated.h"

class UBorder;
class UImage;
class UProgressBar;
class UTextBlock;
class UTexture2D;
class UWidget;
class UWidgetTree;

UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersMainHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	static constexpr int32 NumAbilities = 4;
	static constexpr int32 NumItemSlots = 6;
	static constexpr int32 MaxRankPips = 5;

	virtual void NativeOnInitialized() override;

	/** Health/mana bars and text, level badge, XP bar and gold counter. */
	void SetVitals(float HP, float MaxHP, float MP, float MaxMP, int32 Level, float XPFrac, int32 Gold);

	/**
	 * One ability slot. Remaining > 0 and !bReady shows the dark overlay with the
	 * whole-seconds countdown; Rank fills that many pips and Rank 0 dims the slot.
	 * Cheap to call every frame: unchanged values write nothing.
	 */
	void SetCooldown(int32 Index, float Remaining, bool bReady, int32 Rank);

	/** Ability icons (up to 4, null = keep the empty slot look) and the portrait. */
	void SetAbilityArt(const TArray<UTexture2D*>& Icons, UTexture2D* Portrait);

	/** Item names per inventory slot; an empty string is an empty slot. */
	void SetItems(const TArray<FString>& Names);

private:
	UWidget* BuildVitalsBar(UWidgetTree* Tree, float Height, FLinearColor Fill, FLinearColor Highlight,
		int32 FontSize, UProgressBar*& OutBar, UProgressBar*& OutHighlight, UTextBlock*& OutText);
	UWidget* BuildAbilitySlot(UWidgetTree* Tree, int32 Index);
	void ApplyAbilitySlotLook(int32 Index);

	UPROPERTY() UImage* PortraitImage = nullptr;
	UPROPERTY() UTextBlock* LevelText = nullptr;

	UPROPERTY() UProgressBar* HealthBar = nullptr;
	UPROPERTY() UProgressBar* HealthHighlight = nullptr;
	UPROPERTY() UTextBlock* HealthText = nullptr;
	UPROPERTY() UProgressBar* ManaBar = nullptr;
	UPROPERTY() UProgressBar* ManaHighlight = nullptr;
	UPROPERTY() UTextBlock* ManaText = nullptr;
	UPROPERTY() UProgressBar* XPBar = nullptr;

	UPROPERTY() UTextBlock* GoldText = nullptr;

	// Per ability slot (NumAbilities each)
	UPROPERTY() TArray<UWidget*> AbilitySlotRoots;
	UPROPERTY() TArray<UImage*> AbilityIcons;
	UPROPERTY() TArray<UBorder*> CooldownOverlays;
	UPROPERTY() TArray<UTextBlock*> CooldownTexts;
	// NumAbilities * MaxRankPips, slot-major
	UPROPERTY() TArray<UBorder*> RankPips;

	// Per item slot (NumItemSlots each)
	UPROPERTY() TArray<UBorder*> ItemCells;
	UPROPERTY() TArray<UTextBlock*> ItemLabels;

	// Last values written, so per-frame calls with unchanged state are no-ops
	int32 LastHP = -1, LastMaxHP = -1, LastMP = -1, LastMaxMP = -1, LastLevel = -1, LastGold = INT32_MIN;
	float LastXPFrac = -1.0f;
	int32 LastCooldownSeconds[NumAbilities] = { -1, -1, -1, -1 };
	int32 LastRank[NumAbilities] = { -1, -1, -1, -1 };
	bool bHasIcon[NumAbilities] = { false, false, false, false };
	TArray<FString> LastItems;
};

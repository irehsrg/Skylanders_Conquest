// Skylanders Conquest - Player Character: Shop & Items
//
// Part of ASkylandersCharacter. The class is split across several
// translation units by concern; see SkylandersCharacter.cpp for the index.
// Shop open/close, buy/sell, item bonus recalculation, ability leveling.

#include "SkylandersCharacter.h"
#include "SkylandersSmokeTest.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/RichTextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "SkylandersActorRegistry.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "SkylandersProjectile.h"
#include "SkylandersAoEExplosion.h"
#include "SkylandersTower.h"
#include "SkylandersDamageNumber.h"
#include "SkylandersEnemy.h"
#include "SkylandersMinion.h"
#include "SkylandersBuffCamp.h"
#include "SkylandersEnemyGod.h"
#include "SkylandersSpawnArea.h"
#include "SkylandersTitan.h"
#include "SkylandersItemCatalog.h"
#include "SkylandersSimpleAnimInstance.h"
#include "SkylandersInventoryWidget.h"
#include "SkylandersItemHUDWidget.h"
#include "SkylandersMinimapWidget.h"
#include "SkylandersScoreboardWidget.h"
#include "SkylandersPauseMenuWidget.h"
#include "SkylandersGameSettings.h"
#include "SkylandersKillFeedWidget.h"
#include "SkylandersMatchStatusWidget.h"
#include "SkylandersTelemetry.h"
#include "Sound/SoundBase.h"
#include "DrawDebugHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Animation/AnimInstance.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Fonts/SlateFontInfo.h"

// ========== SHOP & INVENTORY ==========

void ASkylandersCharacter::ToggleShop()
{
	if (bIsShopOpen)
	{
		CloseShop();
	}
	else
	{
		OpenShop();
	}
}

void ASkylandersCharacter::OpenShop()
{
	if (bIsShopOpen) return;

	bIsShopOpen = true;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	// Create shop widget if not already created
	if (!ShopWidget && ShopWidgetClass)
	{
		ShopWidget = CreateWidget<UUserWidget>(PC, ShopWidgetClass);
	}

	if (ShopWidget)
	{
		ShopWidget->AddToViewport(20); // Above HUD
		ShopWidget->SetVisibility(ESlateVisibility::Visible);

		// Show mouse cursor for shop interaction
		PC->bShowMouseCursor = true;
		PC->SetInputMode(FInputModeGameAndUI());
	}
	else
	{
		// No shop widget class set - use debug message as fallback
		UE_LOG(LogTemp, Warning, TEXT("ShopWidgetClass not set! Using debug shop."));

		if (GEngine)
		{
			FString ShopText = TEXT("=== SHOP (B to close) ===\n");
			ShopText += bIsInSpawnArea ? TEXT("IN SPAWN - Can Buy!\n") : TEXT("NOT in spawn - Browse only\n");
			ShopText += FString::Printf(TEXT("Gold: %d\n\n"), Coins);

			const TArray<FSkylandersItemData>& Items = USkylandersItemCatalog::GetAllItems();
			for (const FSkylandersItemData& Item : Items)
			{
				ShopText += FString::Printf(TEXT("[%d] %s - %d gold - %s\n"),
					Item.ItemID, *Item.ItemName, Item.Cost, *Item.Description);
			}

			ShopText += TEXT("\nInventory: ");
			for (int32 i = 0; i < MaxInventorySlots; i++)
			{
				if (Inventory[i] > 0)
				{
					const FSkylandersItemData* Item = USkylandersItemCatalog::FindItem(Inventory[i]);
					ShopText += Item ? FString::Printf(TEXT("[%s] "), *Item->ItemName) : TEXT("[???] ");
				}
				else
				{
					ShopText += TEXT("[Empty] ");
				}
			}

			GEngine->AddOnScreenDebugMessage(100, 30.0f, FColor::Yellow, ShopText);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Shop opened. In spawn: %s"), bIsInSpawnArea ? TEXT("YES") : TEXT("NO"));
}

void ASkylandersCharacter::CloseShop()
{
	if (!bIsShopOpen) return;

	bIsShopOpen = false;

	if (ShopWidget)
	{
		ShopWidget->RemoveFromParent();
	}

	// Clear debug message shop
	if (GEngine)
	{
		GEngine->RemoveOnScreenDebugMessage(100);
	}

	// Restore game-only input
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
	}

	UE_LOG(LogTemp, Log, TEXT("Shop closed."));
}


bool ASkylandersCharacter::BuyItem(int32 ItemID)
{
	// Must be in spawn area to buy
	if (!bIsInSpawnArea)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot buy items outside spawn area!"));
		USkylandersKillFeedWidget::Post(this, TEXT("Must be in spawn area to buy!"), FLinearColor(1.0f, 0.4f, 0.35f));
		return false;
	}

	// Find the item
	const FSkylandersItemData* Item = USkylandersItemCatalog::FindItem(ItemID);
	if (!Item)
	{
		UE_LOG(LogTemp, Warning, TEXT("Item ID %d not found!"), ItemID);
		return false;
	}

	// Check gold
	if (Coins < Item->Cost)
	{
		UE_LOG(LogTemp, Warning, TEXT("Not enough gold! Need %d, have %d"), Item->Cost, Coins);
		USkylandersKillFeedWidget::Post(this, FString::Printf(TEXT("Not enough gold! Need %d"), Item->Cost), FLinearColor(1.0f, 0.4f, 0.35f));
		return false;
	}

	// Find empty inventory slot
	int32 EmptySlot = -1;
	for (int32 i = 0; i < MaxInventorySlots; i++)
	{
		if (Inventory[i] == 0)
		{
			EmptySlot = i;
			break;
		}
	}

	if (EmptySlot < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Inventory full! Cannot buy %s"), *Item->ItemName);
		USkylandersKillFeedWidget::Post(this, TEXT("Inventory full!"), FLinearColor(1.0f, 0.4f, 0.35f));
		return false;
	}

	// Purchase successful — negative delta via AddCoins keeps the HUD coin
	// accumulator event in sync (direct Coins writes desync the Blueprint side)
	AddCoins(-Item->Cost);
	Inventory[EmptySlot] = ItemID;

	RecalculateItemBonuses();
	SKY_COUNT("item_bought");

	// Play item buy sound
	if (ItemBuySound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ItemBuySound, GetActorLocation());
	}

	UE_LOG(LogTemp, Log, TEXT("Bought %s for %d gold (slot %d). Gold remaining: %d"),
		*Item->ItemName, Item->Cost, EmptySlot, Coins);

	USkylandersKillFeedWidget::Post(this, FString::Printf(TEXT("Bought %s!"), *Item->ItemName), FLinearColor(0.4f, 1.0f, 0.5f));

	UpdateHUD();
	return true;
}

bool ASkylandersCharacter::SellItem(int32 SlotIndex)
{
	// Must be in spawn area to sell
	if (!bIsInSpawnArea)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot sell items outside spawn area!"));
		USkylandersKillFeedWidget::Post(this, TEXT("Must be in spawn area to sell!"), FLinearColor(1.0f, 0.4f, 0.35f));
		return false;
	}

	if (SlotIndex < 0 || SlotIndex >= MaxInventorySlots)
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid inventory slot: %d"), SlotIndex);
		return false;
	}

	if (Inventory[SlotIndex] == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Slot %d is empty!"), SlotIndex);
		return false;
	}

	const FSkylandersItemData* Item = USkylandersItemCatalog::FindItem(Inventory[SlotIndex]);
	if (!Item)
	{
		Inventory[SlotIndex] = 0;
		return false;
	}

	int32 SellPrice = Item->GetSellPrice();
	FString ItemName = Item->ItemName;

	// Sell it (AddCoins keeps the HUD coin accumulator event in sync)
	AddCoins(SellPrice);
	Inventory[SlotIndex] = 0;

	RecalculateItemBonuses();

	UE_LOG(LogTemp, Log, TEXT("Sold %s for %d gold. Gold: %d"), *ItemName, SellPrice, Coins);

	USkylandersKillFeedWidget::Post(this, FString::Printf(TEXT("Sold %s for %d gold"), *ItemName, SellPrice), FLinearColor(1.0f, 0.85f, 0.2f));

	UpdateHUD();
	return true;
}

void ASkylandersCharacter::RecalculateItemBonuses()
{
	// Reset bonus stats
	ItemBonusStats = FSkylandersItemStats();

	// Sum all equipped item stats
	for (int32 i = 0; i < MaxInventorySlots; i++)
	{
		if (Inventory[i] > 0)
		{
			const FSkylandersItemData* Item = USkylandersItemCatalog::FindItem(Inventory[i]);
			if (Item)
			{
				ItemBonusStats = ItemBonusStats + Item->Stats;
			}
		}
	}

	// Apply max health/mana changes
	// Effective max = base max (with level scaling already applied) + item bonus
	// Don't let current exceed effective max
	float EffectiveMaxHealth = MaxHealth + ItemBonusStats.MaxHealth;
	float EffectiveMaxMana = MaxMana + ItemBonusStats.MaxMana;
	CurrentHealth = FMath::Min(CurrentHealth, EffectiveMaxHealth);
	CurrentMana = FMath::Min(CurrentMana, EffectiveMaxMana);

	// Apply movement speed bonus
	BaseMaxWalkSpeed = 500.0f + ItemBonusStats.MovementSpeed;

	UE_LOG(LogTemp, Log, TEXT("Item bonuses recalculated: +%.0f Power, +%.1f AS, +%.0f HP, +%.0f MP, +%.0f Prot, +%.0f MS, +%.0f%% CDR, +%.0f Pen"),
		ItemBonusStats.Power, ItemBonusStats.AttackSpeed, ItemBonusStats.MaxHealth,
		ItemBonusStats.MaxMana, ItemBonusStats.Protections, ItemBonusStats.MovementSpeed,
		ItemBonusStats.CooldownReduction * 100.0f, ItemBonusStats.Penetration);
}

float ASkylandersCharacter::GetEffectivePower() const
{
	return BasePower + ItemBonusStats.Power;
}

float ASkylandersCharacter::GetEffectiveAttackSpeed() const
{
	return FireRate + ItemBonusStats.AttackSpeed;
}

float ASkylandersCharacter::GetEffectiveProtections() const
{
	const float LevelProtections = BaseProtections + ProtectionsPerLevel * FMath::Max(PlayerLevel - 1, 0);
	return LevelProtections + ItemBonusStats.Protections + BuffProtections;
}

float ASkylandersCharacter::GetCooldownMultiplier() const
{
	// CDR is summed uncapped on purchase; the 40% cap lives here so an over-cap build just wastes the excess.
	return 1.0f - FMath::Clamp(ItemBonusStats.CooldownReduction, 0.0f, 0.40f);
}

float ASkylandersCharacter::GetUltCooldownMultiplier() const
{
	// Same sum, lower cap (ITEMS-DESIGN.md: ults at 40% CDR would be up too often)
	return 1.0f - FMath::Clamp(ItemBonusStats.CooldownReduction, 0.0f, 0.25f);
}

// ========== ABILITY LEVELING ==========

void ASkylandersCharacter::LevelUpAbility(int32 AbilityIndex)
{
	if (!CanLevelAbility(AbilityIndex))
	{
		const FLinearColor ErrorColor(1.0f, 0.4f, 0.35f);
		if (AbilityPoints <= 0)
		{
			USkylandersKillFeedWidget::Post(this, TEXT("No skill points available"), ErrorColor);
		}
		else if (AbilityIndex >= 0 && AbilityIndex < 4 && AbilityLevels[AbilityIndex] >= MaxAbilityRank)
		{
			USkylandersKillFeedWidget::Post(this, TEXT("Ability already at max rank"), ErrorColor);
		}
		else if (AbilityIndex == 3)
		{
			USkylandersKillFeedWidget::Post(this, TEXT("Ultimate not available at this level"), ErrorColor);
		}
		return;
	}

	AbilityLevels[AbilityIndex]++;
	AbilityPoints--;

	// Feedback in the kill feed (AbilityNames member — set per character)
	USkylandersKillFeedWidget::Post(this, FString::Printf(TEXT("%s rank %d (%d points left)"),
		*AbilityNames[AbilityIndex], AbilityLevels[AbilityIndex], AbilityPoints), FLinearColor(1.0f, 0.85f, 0.2f));

	UE_LOG(LogTemp, Log, TEXT("Ability %d (%s) leveled to rank %d. Points remaining: %d"),
		AbilityIndex, *AbilityNames[AbilityIndex], AbilityLevels[AbilityIndex], AbilityPoints);
}

bool ASkylandersCharacter::CanLevelAbility(int32 AbilityIndex) const
{
	if (AbilityIndex < 0 || AbilityIndex > 3) return false;
	if (AbilityPoints <= 0) return false;
	if (AbilityLevels[AbilityIndex] >= MaxAbilityRank) return false;

	// Ultimate (index 3): rank-ups unlock at levels 5, 9, 13, 17, 20
	// (capped at MaxLevel so rank 5 is actually reachable)
	if (AbilityIndex == 3)
	{
		int32 RequiredLevel = FMath::Min(5 + AbilityLevels[3] * 4, MaxLevel);
		if (PlayerLevel < RequiredLevel) return false;
	}

	return true;
}

int32 ASkylandersCharacter::GetAbilityRank(int32 AbilityIndex) const
{
	if (AbilityIndex < 0 || AbilityIndex > 3) return 0;
	return AbilityLevels[AbilityIndex];
}

// Ability leveling key handlers
void ASkylandersCharacter::Debug_LevelAbility1() { LevelUpAbility(0); }
void ASkylandersCharacter::Debug_LevelAbility2() { LevelUpAbility(1); }
void ASkylandersCharacter::Debug_LevelAbility3() { LevelUpAbility(2); }
void ASkylandersCharacter::Debug_LevelAbility4() { LevelUpAbility(3); }

// Debug shop: F5-F6 buy items by catalog position, F7 sells last occupied slot
// These work when shop is open OR closed (for quick testing)
void ASkylandersCharacter::Debug_ShopBuy5() { BuyItem(5); }
void ASkylandersCharacter::Debug_ShopBuy6() { BuyItem(6); }

void ASkylandersCharacter::Debug_ShopSell()
{
	// Sell the last occupied slot
	for (int32 i = MaxInventorySlots - 1; i >= 0; i--)
	{
		if (Inventory[i] > 0)
		{
			SellItem(i);
			return;
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("No items to sell!"));
	}
}


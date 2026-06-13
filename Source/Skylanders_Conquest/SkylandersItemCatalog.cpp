// Skylanders Conquest - Item Catalog Implementation

#include "SkylandersItemCatalog.h"

TArray<FSkylandersItemData> USkylandersItemCatalog::AllItems;
bool USkylandersItemCatalog::bInitialized = false;

void USkylandersItemCatalog::InitializeItems()
{
	if (bInitialized) return;
	bInitialized = true;

	// Helper lambda to create items cleanly
	auto MakeItem = [](int32 ID, const FString& Name, const FString& Desc, int32 Cost,
		ESkylandersItemCategory Cat, FSkylandersItemStats Stats) -> FSkylandersItemData
	{
		FSkylandersItemData Item;
		Item.ItemID = ID;
		Item.ItemName = Name;
		Item.Description = Desc;
		Item.Cost = Cost;
		Item.Category = Cat;
		Item.Stats = Stats;
		return Item;
	};

	// ===== OFFENSE ITEMS =====

	{
		FSkylandersItemStats S;
		S.Power = 15.0f;
		AllItems.Add(MakeItem(1, TEXT("Short Sword"), TEXT("+15 Power"), 500, ESkylandersItemCategory::Offense, S));
	}
	{
		FSkylandersItemStats S;
		S.Power = 30.0f;
		S.AttackSpeed = 0.3f;
		AllItems.Add(MakeItem(2, TEXT("Elemental Blade"), TEXT("+30 Power, +0.3 Attack Speed"), 1200, ESkylandersItemCategory::Offense, S));
	}
	{
		FSkylandersItemStats S;
		S.AttackSpeed = 0.5f;
		S.CritChance = 0.10f;
		AllItems.Add(MakeItem(3, TEXT("Berserker Gauntlets"), TEXT("+0.5 Attack Speed, +10% Crit"), 800, ESkylandersItemCategory::Offense, S));
	}
	{
		FSkylandersItemStats S;
		S.Power = 10.0f;
		S.Lifesteal = 0.10f;
		AllItems.Add(MakeItem(4, TEXT("Vampire Fangs"), TEXT("+10 Power, +10% Lifesteal"), 900, ESkylandersItemCategory::Offense, S));
	}
	{
		FSkylandersItemStats S;
		S.Power = 45.0f;
		S.CritChance = 0.15f;
		AllItems.Add(MakeItem(5, TEXT("Golden Gatling"), TEXT("+45 Power, +15% Crit"), 1800, ESkylandersItemCategory::Offense, S));
	}

	// ===== DEFENSE ITEMS =====

	{
		FSkylandersItemStats S;
		S.MaxHealth = 100.0f;
		S.Protections = 10.0f;
		AllItems.Add(MakeItem(6, TEXT("Skystone Shield"), TEXT("+100 Health, +10 Protections"), 600, ESkylandersItemCategory::Defense, S));
	}
	{
		FSkylandersItemStats S;
		S.MaxHealth = 200.0f;
		S.HealthRegen = 5.0f;
		AllItems.Add(MakeItem(7, TEXT("Heart of Skylands"), TEXT("+200 Health, +5 HP/s"), 850, ESkylandersItemCategory::Defense, S));
	}
	{
		FSkylandersItemStats S;
		S.MaxHealth = 150.0f;
		S.Protections = 25.0f;
		AllItems.Add(MakeItem(8, TEXT("Guardian Plate"), TEXT("+150 Health, +25 Protections"), 1100, ESkylandersItemCategory::Defense, S));
	}
	{
		FSkylandersItemStats S;
		S.Protections = 40.0f;
		S.HealthRegen = 8.0f;
		AllItems.Add(MakeItem(9, TEXT("Portal Master's Armor"), TEXT("+40 Protections, +8 HP/s"), 1500, ESkylandersItemCategory::Defense, S));
	}

	// ===== UTILITY ITEMS =====

	{
		FSkylandersItemStats S;
		S.MovementSpeed = 50.0f;
		AllItems.Add(MakeItem(10, TEXT("Winged Boots"), TEXT("+50 Movement Speed"), 450, ESkylandersItemCategory::Utility, S));
	}
	{
		FSkylandersItemStats S;
		S.MaxMana = 50.0f;
		S.ManaRegen = 3.0f;
		AllItems.Add(MakeItem(11, TEXT("Crystal Wand"), TEXT("+50 Mana, +3 MP/s"), 550, ESkylandersItemCategory::Utility, S));
	}
	{
		FSkylandersItemStats S;
		S.Power = 20.0f;
		S.MaxMana = 75.0f;
		S.ManaRegen = 5.0f;
		AllItems.Add(MakeItem(12, TEXT("Sage's Ring"), TEXT("+20 Power, +75 Mana, +5 MP/s"), 1300, ESkylandersItemCategory::Utility, S));
	}
}

const TArray<FSkylandersItemData>& USkylandersItemCatalog::GetAllItems()
{
	InitializeItems();
	return AllItems;
}

const FSkylandersItemData* USkylandersItemCatalog::FindItem(int32 ItemID)
{
	InitializeItems();
	for (const FSkylandersItemData& Item : AllItems)
	{
		if (Item.ItemID == ItemID)
		{
			return &Item;
		}
	}
	return nullptr;
}

TArray<FSkylandersItemData> USkylandersItemCatalog::GetItemsByCategory(ESkylandersItemCategory Category)
{
	InitializeItems();
	TArray<FSkylandersItemData> Result;
	for (const FSkylandersItemData& Item : AllItems)
	{
		if (Item.Category == Category)
		{
			Result.Add(Item);
		}
	}
	return Result;
}

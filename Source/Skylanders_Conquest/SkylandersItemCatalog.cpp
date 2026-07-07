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

	// Compact stat builder: (power, atkSpeed, crit, health, mana, hpRegen, mpRegen, prot, lifesteal, moveSpeed)
	auto St = [](float Pow = 0, float AS = 0, float Crit = 0, float HP = 0, float Mana = 0,
		float HPR = 0, float MPR = 0, float Prot = 0, float LS = 0, float MS = 0) -> FSkylandersItemStats
	{
		FSkylandersItemStats S;
		S.Power = Pow; S.AttackSpeed = AS; S.CritChance = Crit; S.MaxHealth = HP; S.MaxMana = Mana;
		S.HealthRegen = HPR; S.ManaRegen = MPR; S.Protections = Prot; S.Lifesteal = LS; S.MovementSpeed = MS;
		return S;
	};

	const ESkylandersItemCategory OFF = ESkylandersItemCategory::Offense;
	const ESkylandersItemCategory DEF = ESkylandersItemCategory::Defense;
	const ESkylandersItemCategory UTL = ESkylandersItemCategory::Utility;

	// ===== OFFENSE (basic -> capstone) =====
	AllItems.Add(MakeItem(1,  TEXT("Short Sword"),         TEXT("+15 Power"),                                    500,  OFF, St(15)));
	AllItems.Add(MakeItem(2,  TEXT("Blade of Fire"),       TEXT("+35 Power"),                                    1150, OFF, St(35)));
	AllItems.Add(MakeItem(3,  TEXT("Berserker Gauntlets"), TEXT("+0.6 Attack Speed, +10% Crit"),                 900,  OFF, St(0, 0.6f, 0.10f)));
	AllItems.Add(MakeItem(4,  TEXT("Vampire Fangs"),       TEXT("+15 Power, +12% Lifesteal"),                    950,  OFF, St(15, 0, 0, 0, 0, 0, 0, 0, 0.12f)));
	AllItems.Add(MakeItem(5,  TEXT("Storm Hammer"),        TEXT("+25 Power, +0.4 Attack Speed"),                 1400, OFF, St(25, 0.4f)));
	AllItems.Add(MakeItem(6,  TEXT("Golden Gatling"),      TEXT("+45 Power, +20% Crit"),                         1900, OFF, St(45, 0, 0.20f)));
	AllItems.Add(MakeItem(7,  TEXT("Executioner"),         TEXT("+30 Power, +0.3 Attack Speed, +25% Crit"),      2400, OFF, St(30, 0.3f, 0.25f)));
	AllItems.Add(MakeItem(8,  TEXT("Bloodthirster"),       TEXT("+40 Power, +20% Lifesteal"),                    2200, OFF, St(40, 0, 0, 0, 0, 0, 0, 0, 0.20f)));

	// ===== DEFENSE (basic -> capstone) =====
	AllItems.Add(MakeItem(9,  TEXT("Skystone Shield"),        TEXT("+100 Health, +12 Protections"),             600,  DEF, St(0, 0, 0, 100, 0, 0, 0, 12)));
	AllItems.Add(MakeItem(10, TEXT("Heart of Skylands"),      TEXT("+250 Health, +6 HP/s"),                      950,  DEF, St(0, 0, 0, 250, 0, 6)));
	AllItems.Add(MakeItem(11, TEXT("Guardian Plate"),         TEXT("+150 Health, +30 Protections"),              1200, DEF, St(0, 0, 0, 150, 0, 0, 0, 30)));
	AllItems.Add(MakeItem(12, TEXT("Portal Master's Armor"),  TEXT("+100 Health, +45 Protections, +8 HP/s"),     1700, DEF, St(0, 0, 0, 100, 0, 8, 0, 45)));
	AllItems.Add(MakeItem(13, TEXT("Titan's Bulwark"),        TEXT("+400 Health, +20 Protections, +10 HP/s"),    2300, DEF, St(0, 0, 0, 400, 0, 10, 0, 20)));
	AllItems.Add(MakeItem(14, TEXT("Aegis of Life"),          TEXT("+150 Health, +25 Protections, +15% Lifesteal"), 2000, DEF, St(0, 0, 0, 150, 0, 0, 0, 25, 0.15f)));

	// ===== UTILITY (basic -> capstone) =====
	AllItems.Add(MakeItem(15, TEXT("Winged Boots"),      TEXT("+60 Movement Speed"),                            500,  UTL, St(0, 0, 0, 0, 0, 0, 0, 0, 0, 60)));
	AllItems.Add(MakeItem(16, TEXT("Crystal Wand"),      TEXT("+60 Mana, +4 MP/s"),                             600,  UTL, St(0, 0, 0, 0, 60, 0, 4)));
	AllItems.Add(MakeItem(17, TEXT("Swiftstrike Charm"), TEXT("+0.3 Attack Speed, +80 Movement Speed"),         1100, UTL, St(0, 0.3f, 0, 0, 0, 0, 0, 0, 0, 80)));
	AllItems.Add(MakeItem(18, TEXT("Sage's Ring"),       TEXT("+25 Power, +80 Mana, +6 MP/s"),                  1400, UTL, St(25, 0, 0, 0, 80, 0, 6)));
	AllItems.Add(MakeItem(19, TEXT("Boots of Celerity"), TEXT("+100 Health, +110 Movement Speed"),              1300, UTL, St(0, 0, 0, 100, 0, 0, 0, 0, 0, 110)));
	AllItems.Add(MakeItem(20, TEXT("Arcane Battery"),    TEXT("+20 Power, +150 Mana, +10 MP/s"),                1800, UTL, St(20, 0, 0, 0, 150, 0, 10)));
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

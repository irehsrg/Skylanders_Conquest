// Skylanders Conquest - Item Catalog (all purchasable items)

#pragma once

#include "CoreMinimal.h"
#include "SkylandersItem.h"

class SKYLANDERS_CONQUEST_API USkylandersItemCatalog
{
public:
	// Get the full list of all items
	static const TArray<FSkylandersItemData>& GetAllItems();

	// Find an item by ID (returns nullptr if not found)
	static const FSkylandersItemData* FindItem(int32 ItemID);

	// Get items filtered by category
	static TArray<FSkylandersItemData> GetItemsByCategory(ESkylandersItemCategory Category);

private:
	static TArray<FSkylandersItemData> AllItems;
	static bool bInitialized;
	static void InitializeItems();
};

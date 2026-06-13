// Skylanders Conquest - Inventory HUD Widget (2x3 grid, bottom-right)

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkylandersInventoryWidget.generated.h"

class UTextBlock;

UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;

	// Slot text references (set during construct)
	UPROPERTY()
	TArray<UTextBlock*> SlotTexts;
};

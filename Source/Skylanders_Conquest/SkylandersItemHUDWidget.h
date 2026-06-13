// Skylanders Conquest - Item HUD Widget (6 slots, bottom-center, above abilities)

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkylandersItemHUDWidget.generated.h"

class UTextBlock;
class UBorder;

UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersItemHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Slot label references (abbreviation text per slot)
	UPROPERTY()
	TArray<UTextBlock*> SlotLabels;

	// Slot background borders (for color changes)
	UPROPERTY()
	TArray<UBorder*> SlotBorders;
};

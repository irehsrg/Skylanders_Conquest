// Skylanders Conquest - Minimap Widget (top-right corner, shows all entities)

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkylandersMinimapWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UCanvasPanelSlot;

UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersMinimapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Map parameters (editable)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
	float MapHalfExtent = 12000.0f; // World half-size that minimap covers

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
	FVector2D MapWorldCenter = FVector2D(0.0f, 0.0f);

	static const int32 MaxDots = 80;
	static constexpr float MinimapSize = 170.0f;

private:
	UPROPERTY()
	UCanvasPanel* DotCanvas;

	UPROPERTY()
	TArray<UBorder*> Dots;

	UPROPERTY()
	TArray<UCanvasPanelSlot*> DotSlots;

	int32 ActiveDotCount;

	void PlaceDot(int32& Index, FVector WorldPos, FLinearColor Color, FVector2D Size);
};

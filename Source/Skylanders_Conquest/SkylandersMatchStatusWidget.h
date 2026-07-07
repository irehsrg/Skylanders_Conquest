// Skylanders Conquest - Top-center match status bar (team gold + timer)

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkylandersMatchStatusWidget.generated.h"

class UTextBlock;

UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersMatchStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;

private:
	UPROPERTY() UTextBlock* BlueGoldText;
	UPROPERTY() UTextBlock* TimerText;
	UPROPERTY() UTextBlock* RedGoldText;
};

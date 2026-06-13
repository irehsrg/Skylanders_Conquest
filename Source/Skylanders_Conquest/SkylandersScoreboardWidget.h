// Skylanders Conquest - Scoreboard Widget (Tab overlay)

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkylandersScoreboardWidget.generated.h"

class UTextBlock;

UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersScoreboardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY() UTextBlock* GameTimeText;
	UPROPERTY() UTextBlock* PlayerNameText;
	UPROPERTY() UTextBlock* PlayerStatsText;
	UPROPERTY() UTextBlock* PlayerItemsText;
	UPROPERTY() UTextBlock* EnemyNameText;
	UPROPERTY() UTextBlock* EnemyStatsText;
};

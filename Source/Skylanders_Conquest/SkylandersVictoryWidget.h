// Skylanders Conquest - Victory/Defeat End Screen Widget

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkylandersVictoryWidget.generated.h"

class UTextBlock;
class ASkylandersCharacter;

UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersVictoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;

	// Called after widget is on screen to fill in results
	void ShowResult(bool bVictory, ASkylandersCharacter* Player);

private:
	UPROPERTY() UTextBlock* ResultText;
	UPROPERTY() UTextBlock* TimeText;
	UPROPERTY() UTextBlock* KDAText;
	UPROPERTY() UTextBlock* GoldText;
	UPROPERTY() UTextBlock* LevelText;
};

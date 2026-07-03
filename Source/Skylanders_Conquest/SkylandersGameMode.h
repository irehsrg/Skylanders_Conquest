// Skylanders Conquest - Game Mode

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SkylandersGameMode.generated.h"

UCLASS()
class SKYLANDERS_CONQUEST_API ASkylandersGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASkylandersGameMode();

	// Spawns the character picked on the menu (falls back to the Blueprint
	// default pawn — Trigger Happy — when nothing else is selected)
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
};

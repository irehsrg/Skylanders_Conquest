// Skylanders Conquest - Front-End Menu Game Mode

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SkylandersMenuGameMode.generated.h"

/**
 * Game mode for the front-end menu map. No pawn is spawned; the menu player
 * controller drives a UI-only experience.
 */
UCLASS()
class SKYLANDERS_CONQUEST_API ASkylandersMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASkylandersMenuGameMode();
};

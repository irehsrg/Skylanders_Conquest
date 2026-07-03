// Skylanders Conquest - Game Instance (carries state across level travel)

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "SkylandersGameInstance.generated.h"

class ASkylandersCharacter;

/**
 * Persists across OpenLevel: holds the character picked on the menu so the
 * game mode knows which pawn to spawn when the match level loads.
 */
UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	// "TriggerHappy" (default), "Hex", or "TreeRex"
	UPROPERTY(BlueprintReadWrite, Category = "Character Select")
	FName SelectedCharacterID = TEXT("TriggerHappy");

	// Resolves the selection to a pawn class. Returns nullptr for TriggerHappy —
	// his pawn is the Blueprint (BP_SkylandersCharacter_CPP) already set as the
	// game mode's default, so callers fall back to that.
	TSubclassOf<ASkylandersCharacter> ResolveSelectedCharacterClass() const;
};

// Skylanders Conquest - Game Mode Implementation

#include "SkylandersGameMode.h"
#include "SkylandersCharacter.h"
#include "SkylandersGameInstance.h"

ASkylandersGameMode::ASkylandersGameMode()
{
	// Don't hardcode default pawn - let it be set in blueprint or project settings
	// DefaultPawnClass can be set in the editor
}

UClass* ASkylandersGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	// Character picked on the main menu (persists across level travel)
	if (USkylandersGameInstance* GI = GetGameInstance<USkylandersGameInstance>())
	{
		if (TSubclassOf<ASkylandersCharacter> Selected = GI->ResolveSelectedCharacterClass())
		{
			return Selected;
		}
	}
	// Trigger Happy (BP default pawn set in BP_SkylandersGameMode)
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

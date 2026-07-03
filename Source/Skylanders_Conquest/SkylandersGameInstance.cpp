// Skylanders Conquest - Game Instance Implementation

#include "SkylandersGameInstance.h"
#include "SkylandersHexCharacter.h"
#include "SkylandersTreeRexCharacter.h"

TSubclassOf<ASkylandersCharacter> USkylandersGameInstance::ResolveSelectedCharacterClass() const
{
	if (SelectedCharacterID == TEXT("Hex"))
	{
		return ASkylandersHexCharacter::StaticClass();
	}
	if (SelectedCharacterID == TEXT("TreeRex"))
	{
		return ASkylandersTreeRexCharacter::StaticClass();
	}
	// Trigger Happy: use the game mode's default pawn (the tuned Blueprint)
	return nullptr;
}

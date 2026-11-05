// Skylanders Conquest - Game Instance Implementation

#include "SkylandersGameInstance.h"
#include "SkylandersHexCharacter.h"
#include "SkylandersTreeRexCharacter.h"
#include "SkylandersSpyroCharacter.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void USkylandersGameInstance::Init()
{
	Super::Init();

	// The smoke test skips the menu (it boots straight into the match map), so
	// this is the only way it can play a kit other than the default.
	FString CharacterArg;
	if (FParse::Value(FCommandLine::Get(), TEXT("SkyCharacter="), CharacterArg) && !CharacterArg.IsEmpty())
	{
		SelectedCharacterID = FName(*CharacterArg);
		UE_LOG(LogTemp, Warning, TEXT("SelectedCharacterID=%s (from -SkyCharacter)"), *CharacterArg);
	}
}

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
	if (SelectedCharacterID == TEXT("Spyro"))
	{
		return ASkylandersSpyroCharacter::StaticClass();
	}
	// Trigger Happy: use the game mode's default pawn (the tuned Blueprint)
	return nullptr;
}

// Skylanders Conquest - Front-End Menu Game Mode Implementation

#include "SkylandersMenuGameMode.h"
#include "SkylandersMenuPlayerController.h"
#include "GameFramework/SpectatorPawn.h"

ASkylandersMenuGameMode::ASkylandersMenuGameMode()
{
	PlayerControllerClass = ASkylandersMenuPlayerController::StaticClass();

	// No gameplay pawn on the menu; a spectator pawn keeps the framework happy.
	DefaultPawnClass = ASpectatorPawn::StaticClass();
}

// Skylanders Conquest - Front-End Menu Player Controller

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SkylandersMenuPlayerController.generated.h"

class USkylandersMainMenuWidget;

/**
 * Player controller used on the front-end menu map. Creates and displays the
 * main menu widget, switches to UI-only input, and shows the mouse cursor.
 */
UCLASS()
class SKYLANDERS_CONQUEST_API ASkylandersMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// Automation hook: opens the character select as if PLAY was clicked
	// (synthetic UI clicks can't reach UMG buttons in PIE)
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void OpenCharacterSelectScreen();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() USkylandersMainMenuWidget* MenuWidget = nullptr;
};

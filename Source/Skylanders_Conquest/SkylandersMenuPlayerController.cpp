// Skylanders Conquest - Front-End Menu Player Controller Implementation

#include "SkylandersMenuPlayerController.h"
#include "SkylandersMainMenuWidget.h"
#include "Blueprint/UserWidget.h"

void ASkylandersMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	MenuWidget = CreateWidget<USkylandersMainMenuWidget>(this, USkylandersMainMenuWidget::StaticClass());
	if (MenuWidget)
	{
		MenuWidget->SetIsFocusable(true); // Needed for SetWidgetToFocus below to take
		MenuWidget->AddToViewport();
	}

	// UI-only input + visible cursor for menu navigation
	FInputModeUIOnly InputMode;
	if (MenuWidget)
	{
		InputMode.SetWidgetToFocus(MenuWidget->TakeWidget());
	}
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void ASkylandersMenuPlayerController::OpenCharacterSelectScreen()
{
	if (!MenuWidget) return;
	// Same path as clicking PLAY (handler is private; invoke via reflection)
	if (UFunction* Fn = MenuWidget->FindFunction(FName("OnPlayClicked")))
	{
		MenuWidget->ProcessEvent(Fn, nullptr);
	}
}

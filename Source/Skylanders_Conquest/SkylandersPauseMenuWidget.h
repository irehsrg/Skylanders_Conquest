// Skylanders Conquest - In-match pause menu

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkylandersPauseMenuWidget.generated.h"

class UWidget;
class USkylandersSettingsWidget;

/** Fired after the menu unpauses and tears down, so the pawn can restore input. */
DECLARE_DELEGATE(FOnSkylandersPauseMenuClosed);

/**
 * Escape menu shown during a match. Pausing itself is driven by the pawn
 * (see ASkylandersCharacter::TogglePauseMenu); this widget owns the buttons and
 * the trip back to the front-end.
 */
UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Level opened by QUIT TO MAIN MENU. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	FName MainMenuLevelName = TEXT("MainMenu");

	FOnSkylandersPauseMenuClosed OnClosed;

	/** Unpause, remove the menu and notify the owner. */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void Resume();

private:
	UPROPERTY() UWidget* MenuPanel = nullptr;
	UPROPERTY() USkylandersSettingsWidget* SettingsWidget = nullptr;

	UFUNCTION() void OnResumeClicked();
	UFUNCTION() void OnSettingsClicked();
	UFUNCTION() void OnMainMenuClicked();
	UFUNCTION() void OnQuitClicked();

	void HandleSettingsClosed();

	/** Route UI-only input and keyboard focus to a specific widget. */
	void FocusWidget(UUserWidget* Widget);
};

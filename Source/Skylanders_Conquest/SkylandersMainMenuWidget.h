// Skylanders Conquest - Front-End Main Menu Widget

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkylandersMainMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UWidgetSwitcher;
class UVerticalBox;
class USkylandersCharacterSelectWidget;

/**
 * Fully code-built front-end menu. PLAY and CHARACTERS open the full-screen
 * character select (grid + live 3D preview + abilities); Settings lives in
 * the widget switcher.
 */
UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;

	/** Level (map) locking in a character loads. Defaults to "Joust". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	FName GameLevelName = TEXT("Joust");

private:
	// ----- Screen indices in the switcher -----
	enum EMenuScreen : int32 { Screen_Main = 0, Screen_Settings = 1 };

	UPROPERTY() UWidgetSwitcher* ScreenSwitcher = nullptr;
	UPROPERTY() USkylandersCharacterSelectWidget* CharacterSelect = nullptr;

	// Build helpers
	UVerticalBox* BuildMainScreen();
	UVerticalBox* BuildSettingsScreen();
	UButton* MakeMenuButton(const FString& Label, FName Name);

	// Opens the full-screen character select overlay
	void OpenCharacterSelect();

	// Button handlers
	UFUNCTION() void OnPlayClicked();
	UFUNCTION() void OnCharactersClicked();
	UFUNCTION() void OnSettingsClicked();
	UFUNCTION() void OnQuitClicked();
	UFUNCTION() void OnBackClicked();
};

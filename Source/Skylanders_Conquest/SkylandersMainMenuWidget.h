// Skylanders Conquest - Front-End Main Menu Widget

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkylandersMainMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class USkylandersCharacterSelectWidget;
class USkylandersSettingsWidget;

/**
 * Fully code-built front-end menu. PLAY and CHARACTERS open the full-screen
 * character select (grid + live 3D preview + abilities); SETTINGS opens the
 * shared settings screen, which the in-match pause menu also uses.
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
	UPROPERTY() USkylandersCharacterSelectWidget* CharacterSelect = nullptr;
	UPROPERTY() USkylandersSettingsWidget* SettingsWidget = nullptr;

	// Build helpers
	UVerticalBox* BuildMainScreen();
	UButton* MakeMenuButton(const FString& Label, FName Name);

	// Opens the full-screen character select overlay
	void OpenCharacterSelect();

	void HandleSettingsClosed();

	/** Route UI-only input and keyboard focus to a specific widget. */
	void FocusWidget(UUserWidget* Widget);

	// Button handlers
	UFUNCTION() void OnPlayClicked();
	UFUNCTION() void OnCharactersClicked();
	UFUNCTION() void OnSettingsClicked();
	UFUNCTION() void OnQuitClicked();
};

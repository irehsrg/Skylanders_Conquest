// Skylanders Conquest - Front-End Main Menu Widget

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkylandersMainMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UWidgetSwitcher;
class UVerticalBox;

/**
 * Fully code-built front-end menu. Screens (Main / Character Select / Settings)
 * are swapped via a WidgetSwitcher. PLAY opens the game level.
 */
UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;

	/** Level (map) the PLAY button loads. Defaults to "Joust". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	FName GameLevelName = TEXT("Joust");

private:
	// ----- Screen indices in the switcher -----
	enum EMenuScreen : int32 { Screen_Main = 0, Screen_Characters = 1, Screen_Settings = 2 };

	UPROPERTY() UWidgetSwitcher* ScreenSwitcher = nullptr;

	// Build helpers
	UVerticalBox* BuildMainScreen();
	UVerticalBox* BuildCharacterScreen();
	UVerticalBox* BuildSettingsScreen();
	UButton* MakeMenuButton(const FString& Label, FName Name);

	// Adds one selectable character row (name + role + kit blurb) to the box;
	// caller binds OnClicked on the returned button
	UButton* AddCharacterButton(UVerticalBox* Box, const FString& Name, const FString& Role,
		const FString& KitLine, FLinearColor AccentColor, FName WidgetName);

	// Stores the pick in the game instance and opens the match level
	void StartGameAs(FName CharacterID);

	// Button handlers
	UFUNCTION() void OnPlayClicked();
	UFUNCTION() void OnCharactersClicked();
	UFUNCTION() void OnSettingsClicked();
	UFUNCTION() void OnQuitClicked();
	UFUNCTION() void OnBackClicked();
	UFUNCTION() void OnSelectTriggerHappy();
	UFUNCTION() void OnSelectHex();
	UFUNCTION() void OnSelectTreeRex();
};

// Skylanders Conquest - Character Select Screen (SMITE/LoL style)
//
// Full-screen, fully code-built: a portrait grid on the left third, a live
// rotating 3D preview of the hovered character in the center (SceneCapture
// render target), and the character's role/stats/abilities on the right.
// LOCK IN stores the pick in the game instance and starts the match.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkylandersCharacterSelectWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class UVerticalBox;
class UUniformGridPanel;
class ASkylandersCharacterPreviewActor;

// Everything the select screen needs to know about one roster entry
struct FSkylanderSelectEntry
{
	FName ID;
	FString DisplayName;
	FString Role;
	FLinearColor Accent;
	FString StatLine;
	FString AutoAttack;
	FString Abilities[4]; // "Name|Description"

	// Preview stage tuning
	FString MeshPath;
	FString IdleAnimPath;
	float MeshScale = 1.0f;
	float MeshZOffset = 0.0f;
	float CaptureDistance = 300.0f;
	float AimHeight = 50.0f;

	static const TArray<FSkylanderSelectEntry>& GetRoster();
};

UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersCharacterSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Level the LOCK IN button loads (set by the main menu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	FName GameLevelName = TEXT("Joust");

	// Highlights a character and updates preview + info panel
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void SelectCharacter(FName CharacterID);

private:
	// Build helpers
	UButton* MakeTile(const FSkylanderSelectEntry& Entry, int32 Index);
	void RebuildInfoPanel(const FSkylanderSelectEntry& Entry);
	void UpdatePreview(const FSkylanderSelectEntry& Entry);
	void UpdateTileHighlights();

	// Button handlers (one per roster slot — AddDynamic needs literal functions)
	UFUNCTION() void OnTile0();
	UFUNCTION() void OnTile1();
	UFUNCTION() void OnTile2();
	UFUNCTION() void OnLockIn();
	UFUNCTION() void OnBack();

	FName SelectedID;

	UPROPERTY() UImage* PreviewImage = nullptr;
	UPROPERTY() UVerticalBox* InfoPanel = nullptr;
	UPROPERTY() UTextBlock* LockInLabel = nullptr;
	UPROPERTY() TArray<UButton*> TileButtons;

	UPROPERTY() ASkylandersCharacterPreviewActor* PreviewActor = nullptr;
};

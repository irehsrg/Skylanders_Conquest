// Skylanders Conquest - Victory/Defeat End Screen Widget
//
// Dimmed overlay with one centered panel: a big VICTORY / DEFEAT title and a
// subtitle (match time, how it ended), the full two-team results table in the
// scoreboard's layout (BLUE: the player then allies, RED: enemies; name, level,
// K/D/A, CS, gold, items; the player's row outlined in gold), three match stat
// tiles (gold earned, creep score, kills + assists) and PLAY AGAIN / MAIN MENU.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkylandersVictoryWidget.generated.h"

class UTextBlock;
class UVerticalBox;
class UHorizontalBox;
class ASkylandersCharacter;

UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersVictoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;

	// Called after widget is on screen to fill in results. Player may be null
	// (pawn already gone, or a test harness): the table then lists the gods only.
	void ShowResult(bool bVictory, ASkylandersCharacter* Player);

	/** Level opened by MAIN MENU. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	FName MainMenuLevelName = TEXT("MainMenu");

private:
	// What one results row displays; gathered from the player or a god
	struct FResultLine
	{
		FString Name;
		int32 Level = 0, Kills = 0, Deaths = 0, Assists = 0, CS = 0, Gold = 0;
		FString Items;
		bool bIsPlayer = false;
	};

	void AddResultRow(UVerticalBox* Box, const FResultLine& Line, const FLinearColor& RowColor);
	UTextBlock* AddCell(UHorizontalBox* Row, int32 Column, const FString& Str, int32 FontSize, FLinearColor Color);
	UTextBlock* AddStatTile(UHorizontalBox* Row, const FString& Label, bool bLast);

	UPROPERTY() UTextBlock* ResultText;
	UPROPERTY() UTextBlock* SubtitleText;
	UPROPERTY() UVerticalBox* BlueBox;
	UPROPERTY() UVerticalBox* RedBox;
	UPROPERTY() UTextBlock* GoldTileText;
	UPROPERTY() UTextBlock* CSTileText;
	UPROPERTY() UTextBlock* KATileText;

	/** The end screen pauses the match, so leaving it has to unpause first. */
	void LeaveToLevel(FName LevelName);

	UFUNCTION() void OnPlayAgainClicked();
	UFUNCTION() void OnMainMenuClicked();
};

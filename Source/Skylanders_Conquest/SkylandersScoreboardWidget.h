// Skylanders Conquest - Scoreboard Widget (Tab overlay)
//
// SMITE-style table: a header row (NAME, LVL, K / D / A, CS, GOLD, ITEMS),
// then a BLUE section (the player first, then ally gods) and a RED section
// (enemy gods), one team-tinted row per hero. Built in C++ like the other HUD
// widgets; rows are pooled and refreshed at 4 Hz while the board is visible.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkylandersScoreboardWidget.generated.h"

class UTextBlock;
class UBorder;
class UVerticalBox;

UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersScoreboardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	// One hero's line. The widgets live in the tree under their section box;
	// this just keeps typed handles so a refresh doesn't search for them.
	struct FRow
	{
		UBorder* Background = nullptr;
		UTextBlock* Name = nullptr;
		UTextBlock* Level = nullptr;
		UTextBlock* KDA = nullptr;
		UTextBlock* CS = nullptr;
		UTextBlock* Gold = nullptr;
		UTextBlock* Items = nullptr;
	};

	// What a row displays; gathered from the player or a god
	struct FHeroLine
	{
		FString Name;
		int32 Level = 0, Kills = 0, Deaths = 0, Assists = 0, CS = 0, Gold = 0;
		FString Items;
		bool bDead = false;
		bool bIsPlayer = false;
	};

	void Refresh();

	// Grows Rows to Count (new rows go into Box), fills the first Count, collapses the rest
	void ShowLines(UVerticalBox* Box, TArray<FRow>& Rows, const TArray<FHeroLine>& Lines, const FLinearColor& RowColor);
	FRow MakeRow(UVerticalBox* Box);

	UPROPERTY() UTextBlock* GameTimeText;
	UPROPERTY() UVerticalBox* BlueBox;
	UPROPERTY() UVerticalBox* RedBox;

	TArray<FRow> BlueRows;
	TArray<FRow> RedRows;

	// Seconds since the last refresh; starts "due" so the first show is immediate
	float SinceRefresh = 1.0f;
	static constexpr float RefreshInterval = 0.25f;
};

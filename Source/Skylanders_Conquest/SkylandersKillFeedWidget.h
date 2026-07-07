// Skylanders Conquest - Kill Feed (recent kills, top-right, auto-expiring)

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkylandersKillFeedWidget.generated.h"

class UVerticalBox;

UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersKillFeedWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;

	// Add a line to the top of the feed; it auto-removes after a few seconds.
	void AddEntry(const FString& Text, FLinearColor Color);

	// Finds the on-screen kill feed and posts to it (safe to call from anywhere).
	static void Post(UObject* WorldContext, const FString& Text, FLinearColor Color);

private:
	UPROPERTY()
	UVerticalBox* FeedBox;

	static const int32 MaxEntries = 5;
};

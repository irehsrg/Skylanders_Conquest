// Skylanders Conquest - Announcer banner (big centered call-outs)
//
// "FIRST BLOOD", "<God> SLAIN", "TOWER DESTROYED", the 3-2-1-FIGHT countdown.
// Built entirely in C++ like the kill feed: one large outlined TextBlock at
// 50%/22% of the screen, collapsed while idle. Messages queue (max 3 pending)
// and play one at a time: fade in over 0.15 s, hold, collapse.
//
// A second, smaller status line sits under the banner for persistent state
// ("Respawning in 7...", "Recalling... 3s"): SetStatus shows it until ClearStatus.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkylandersAnnouncerWidget.generated.h"

class UTextBlock;

UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersAnnouncerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	// Queue a banner on the on-screen announcer (safe to call from anywhere).
	// SoundName is a SkylandersSound short name played when the banner shows;
	// with no announcer on screen (e.g. headless) the sound plays immediately.
	static void Announce(UObject* WorldContext, const FString& Text, FLinearColor Color, float Seconds = 2.2f, const TCHAR* SoundName = nullptr);

	// True exactly once per match world: the caller gets to post "FIRST BLOOD".
	static bool ClaimFirstBlood(const UObject* WorldContext);

	// Persistent status line under the banner, shown until ClearStatus. Cheap to
	// call every frame: the widget is cached and unchanged text is not re-set.
	static void SetStatus(UObject* WorldContext, const FString& Text, FLinearColor Color);
	static void ClearStatus(UObject* WorldContext);

private:
	struct FEntry
	{
		FString Text;
		FLinearColor Color;
		float Seconds = 2.2f;
		FString SoundName;
	};

	void Enqueue(FEntry&& Entry);
	void ShowNext();
	void Step();

	// The on-screen announcer for WorldContext's world (cached; null if none)
	static USkylandersAnnouncerWidget* Find(UObject* WorldContext);

	UPROPERTY()
	UTextBlock* BannerText;

	UPROPERTY()
	UTextBlock* StatusText;

	FString ShownStatus;
	FLinearColor ShownStatusColor = FLinearColor::Transparent;

	TArray<FEntry> Pending;
	bool bShowing = false;
	float ShownTime = 0.0f;
	float HoldSeconds = 0.0f;
	FLinearColor ShownColor = FLinearColor::White;
	FTimerHandle StepTimer;

	static const int32 MaxPending = 3;
	static constexpr float FadeInSeconds = 0.15f;
	static constexpr float StepSeconds = 1.0f / 60.0f;
};

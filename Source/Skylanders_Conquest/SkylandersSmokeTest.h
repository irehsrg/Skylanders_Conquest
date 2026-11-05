// Skylanders Conquest - Headless smoke test
//
// Runs a real match without a human and reports whether the game still works.
// Active only when the game is launched with -SkySmoke=<seconds>, e.g.
//
//   UnrealEditor-Cmd.exe <project>.uproject Joust -game -nullrhi -unattended -SkySmoke=120
//
// (Scripts/smoke_test.py wraps this; add -SkyItemTest for the item stat check,
// see RunItemTest). Every second it samples the match through
// the actor registry; gameplay code bumps named counters via SKY_COUNT(...).
// At the end it writes Saved/SmokeTest/report.json, evaluates the invariants in
// EvaluateInvariants(), logs PASS/FAIL and exits with status 0 or 1.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Misc/OutputDevice.h"
#include "SkylandersSmokeTest.generated.h"

// Bump a named counter. Free to call anywhere; costs one TMap lookup and only
// when the smoke test is running.
#define SKY_COUNT(Name) USkylandersSmokeTestSubsystem::Count(TEXT(Name))

UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersSmokeTestSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static void Count(const TCHAR* Name, int32 Delta = 1);
	static bool IsRunning() { return bActive; }

private:
	// Captures Error-level log output so a silent failure fails the run.
	class FErrorSink : public FOutputDevice
	{
	public:
		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override;
		int32 ErrorCount = 0;
		TArray<FString> FirstErrors;
	};

	bool Tick(float DeltaTime);
	void Sample(UWorld* World);
	void Finish();
	TArray<FString> EvaluateInvariants() const;

	// -SkyItemTest: a deterministic check of every catalog item's stats, the CDR
	// caps, penetration math and the full-inventory refusal, run once on the
	// player at the first sample; the player is restored afterwards so the rest
	// of the match plays normally. Results go to report.json under "item_test".
	void RunItemTest(UWorld* World, class ASkylandersCharacter* Player);
	bool bItemTestRequested = false;
	bool bItemTestRan = false;
	int32 ItemTestItemsChecked = 0;
	int32 ItemTestChecks = 0;
	TArray<FString> ItemTestFailures;
	FString ItemTestNotes; // JSON fields (no braces) with the measured headline numbers

	static bool bActive;
	static TMap<FName, int32> Counters;

	float DurationSeconds = 0.0f;
	float Elapsed = 0.0f;
	float SinceSample = 0.0f;
	bool bFinished = false;
	FTSTicker::FDelegateHandle TickHandle;
	FErrorSink ErrorSink;

	struct FSample
	{
		float T = 0.0f;
		int32 Minions = 0, Gods = 0, GodsAlive = 0, Towers = 0, TowersAlive = 0, Titans = 0, Camps = 0;
		float GodHealthSum = 0.0f, GodHealthMaxSum = 0.0f;
		float PlayerHealth = 0.0f;
		int32 PlayerLevel = 0, PlayerCoins = 0, PlayerKills = 0, PlayerDeaths = 0;
		FString ToJson() const;
	};
	TArray<FSample> Samples;
	int32 MaxMinionsSeen = 0;

	// Which kit the player pawn is running (logged once at match start and
	// written to the report, so a run proves which character it exercised)
	FString CharacterName;
	FString CharacterID;
};

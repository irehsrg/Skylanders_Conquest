// Skylanders Conquest - Telemetry / event logging subsystem
//
// Writes a structured JSONL event log per match to Saved/Telemetry/. One JSON
// object per line: { "t": <matchSeconds>, "type": "<event>", ...fields }.
// Open it in any JSON/CSV tool to analyze deaths, ability usage, economy, and
// (via the periodic snapshots) unit movement / stuck AI.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "SkylandersTelemetry.generated.h"

class ASkylandersEnemyGod;
class ASkylandersCharacter;

UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersTelemetrySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Convenience accessor from any actor/world context (null-safe).
	static USkylandersTelemetrySubsystem* Get(const UObject* WorldContext);

	// Generic event. Fields must be a valid JSON fragment WITHOUT leading comma,
	// e.g. TEXT("\"killer\":\"Kaos\",\"x\":123.0"). Empty is fine.
	void LogEvent(const FString& EventType, const FString& Fields = FString());

	// Typed helpers (build the JSON internally).
	void LogMatchStart(const FString& MapName);
	void LogMatchEnd(bool bVictory, float MatchSeconds);
	void LogGodDeath(ASkylandersEnemyGod* God, const FString& Cause);
	void LogPlayerDeath(ASkylandersCharacter* Player, const FString& Cause);
	void LogStructureDestroyed(const FString& Name, const FString& Team);
	void LogAbility(const FString& InstigatorName, const FString& Team, const FString& AbilityName, const FVector& Location);

private:
	void OpenLog();
	void WriteLine(const FString& Line);
	float MatchTime() const;

	// Periodic per-god snapshot (movement, hp, state, economy) + stuck detection.
	bool Snapshot(float DeltaTime);

	FString LogFilePath;
	bool bReady = false;

	FTSTicker::FDelegateHandle SnapshotHandle;
	TMap<FString, FVector> LastGodPos; // for movement/stuck deltas
};

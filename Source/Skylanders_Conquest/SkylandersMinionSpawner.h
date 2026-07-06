// Skylanders Conquest - Minion Wave Spawner

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkylandersTower.h"
#include "SkylandersMinion.h"
#include "SkylandersMinionSpawner.generated.h"

UCLASS()
class SKYLANDERS_CONQUEST_API ASkylandersMinionSpawner : public AActor
{
	GENERATED_BODY()

public:
	ASkylandersMinionSpawner();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	// Which team's minions to spawn
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	ETowerTeam Team;

	// Where enemy minions march toward (set to enemy base location)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	FVector LaneTargetPoint;

	// Ordered lane path (this side's spawn -> enemy base) handed to each spawned
	// minion so they follow the curved lane. Last point should be the base.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	TArray<FVector> LaneWaypoints;

	// Wave timing
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	float WaveInterval; // Seconds between waves

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	float FirstWaveDelay; // Delay before first wave

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	int32 MeleeCount; // Melee minions per wave

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	int32 RangedCount; // Ranged minions per wave

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	float SpawnSpread; // Horizontal spread between minions

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	float SpawnStaggerDelay; // Delay between each minion spawn

	int32 WaveNumber;

	UFUNCTION(BlueprintCallable, Category = "Spawning")
	void SpawnWave();

	void SpawnNextMinion();

	FTimerHandle WaveTimerHandle;
	FTimerHandle StaggerTimerHandle;

	// Queue of minions to spawn this wave
	TArray<TPair<EMinionType, FVector>> SpawnQueue;
	int32 SpawnQueueIndex;
};

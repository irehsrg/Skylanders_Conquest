// Skylanders Conquest - Map Builder (Joust-style single-lane MOBA map)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkylandersMapBuilder.generated.h"

class ASkylandersTower;
class ASkylandersTitan;
class ASkylandersMinionSpawner;
class ASkylandersBuffCamp;
class ASkylandersSpawnArea;

UCLASS()
class SKYLANDERS_CONQUEST_API ASkylandersMapBuilder : public AActor
{
	GENERATED_BODY()

public:
	ASkylandersMapBuilder();

protected:
	virtual void BeginPlay() override;

	void BuildMap();
	AActor* SpawnWall(FVector Location, FVector Scale, FRotator Rotation = FRotator::ZeroRotator);
	AActor* SpawnGround(FVector Location, FVector Scale, FLinearColor Color);

	// Joust-shape helpers (greybox geometry)
	AActor* SpawnFloorDisc(FVector Center, float Radius, float Z, FLinearColor Color);
	AActor* SpawnFloorSeg(FVector From, FVector To, float Width, float Z, FLinearColor Color);
	AActor* SpawnWallSeg(FVector From, FVector To, float Height, float Thickness, FLinearColor Color);

	// ========== STRUCTURES ==========

	// Blue (Player) side
	UPROPERTY() ASkylandersTower* BlueTower;
	UPROPERTY() ASkylandersTower* BluePhoenix;
	UPROPERTY() ASkylandersTitan* BlueTitan;
	UPROPERTY() ASkylandersMinionSpawner* BlueSpawner;
	UPROPERTY() ASkylandersSpawnArea* BlueSpawnArea;

	// Red (Enemy) side
	UPROPERTY() ASkylandersTower* RedTower;
	UPROPERTY() ASkylandersTower* RedPhoenix;
	UPROPERTY() ASkylandersTitan* RedTitan;
	UPROPERTY() ASkylandersMinionSpawner* RedSpawner;
	UPROPERTY() ASkylandersSpawnArea* RedSpawnArea;

	// ========== JUNGLE CAMPS ==========

	UPROPERTY() ASkylandersBuffCamp* BullDemonKing;
	UPROPERTY() ASkylandersBuffCamp* DamageCamp;
	UPROPERTY() ASkylandersBuffCamp* MidCamp;
	UPROPERTY() ASkylandersBuffCamp* BlueBlueBuff;  // Blue side blue buff
	UPROPERTY() ASkylandersBuffCamp* RedBlueBuff;   // Red side blue buff
};

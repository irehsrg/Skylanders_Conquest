// Skylanders Conquest - Spawn Area (visual zone where players can shop)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkylandersTower.h" // ETowerTeam
#include "SkylandersSpawnArea.generated.h"

class USphereComponent;
class UDecalComponent;
class UStaticMeshComponent;

UCLASS()
class SKYLANDERS_CONQUEST_API ASkylandersSpawnArea : public AActor
{
	GENERATED_BODY()

public:
	ASkylandersSpawnArea();

	// Trigger sphere for detecting player presence
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawn Area")
	USphereComponent* SpawnZone;

	// Visual platform mesh
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawn Area")
	UStaticMeshComponent* PlatformMesh;

	// Radius of the spawn zone
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Area")
	float SpawnRadius;

	// Fountain damage per second to enemies inside spawn
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Area")
	float FountainDPS;

	// Which side owns this base — decides who it heals and who it burns
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Area")
	ETowerTeam Team;

	virtual void Tick(float DeltaTime) override;

protected:
	// Fountain damage is applied in discrete pulses so we don't spawn a
	// damage number actor every frame per victim
	float FountainPulseAccumulator;
	static constexpr float FountainPulseInterval = 0.25f;

public:

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnPlayerEnterSpawn(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnPlayerExitSpawn(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};

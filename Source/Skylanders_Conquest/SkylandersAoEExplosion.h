// Skylanders Conquest - AoE Explosion for Pot o' Gold ability

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkylandersAoEExplosion.generated.h"

class USphereComponent;

UCLASS()
class SKYLANDERS_CONQUEST_API ASkylandersAoEExplosion : public AActor
{
	GENERATED_BODY()

public:
	ASkylandersAoEExplosion();

protected:
	virtual void BeginPlay() override;

public:
	// Explosion settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosion")
	float DamageAmount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosion")
	float ExplosionRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosion")
	float DetonationDelay;

	// Visual
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComponent;

	void Detonate();

	FTimerHandle DetonationTimer;
};

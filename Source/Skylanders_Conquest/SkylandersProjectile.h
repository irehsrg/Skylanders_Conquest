// Skylanders Conquest - Projectile with Working Collision

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkylandersProjectile.generated.h"

// Forward declarations
class USphereComponent;
class UProjectileMovementComponent;
class UStaticMeshComponent;

UCLASS()
class SKYLANDERS_CONQUEST_API ASkylandersProjectile : public AActor
{
	GENERATED_BODY()

public:
	ASkylandersProjectile();

protected:
	virtual void BeginPlay() override;

public:
	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProjectileMovementComponent* ProjectileMovement;

	// Projectile settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float InitialSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float Lifetime;

	// Damage dealt on hit (set by the spawner based on player power)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float Damage;

	// Lifesteal fraction (0-1), heals shooter on hit
	float Lifesteal;

	// Whether this shot is a critical hit (extra damage + visual)
	bool bIsCrit;

	// Only auto-attack projectiles damage towers/titans; ability shots
	// (e.g. Golden Machine Gun) clear this flag on spawn
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	bool bDamagesStructures;

	// Hit callback (walls, towers, titans - WorldStatic/WorldDynamic)
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	// Overlap callback (minions, enemies, enemy god - Pawn objects)
	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// Apply lifesteal if applicable
	void ApplyLifesteal();
};

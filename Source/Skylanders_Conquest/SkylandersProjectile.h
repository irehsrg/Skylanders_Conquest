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

	// Tint used by the C++ fallback visual (characters without a projectile
	// Blueprint get a small colored sphere; set by the shooter on spawn)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	FLinearColor ProjectileColor;

	// Uniform scale of the fallback sphere visual (sphere default radius 50).
	// Tanks/heavy autos set this bigger for a chunkier shot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float VisualScale;

	// Cleave: when > 0, on hitting its primary target the shot also splashes
	// every other enemy within this radius (SMITE-style multi-target auto).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float CleaveRadius;

	// Fraction of the shot's damage dealt to secondary cleave targets (0-1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float CleaveDamageFraction;

	// Hit callback (walls, towers, titans - WorldStatic/WorldDynamic)
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	// Overlap callback (minions, enemies, enemy god - Pawn objects)
	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// Apply lifesteal if applicable
	void ApplyLifesteal();

	// Splash cleave damage to enemies near the impact (excludes PrimaryTarget
	// and the shooter). No-op when CleaveRadius <= 0.
	void CleaveNearby(const FVector& ImpactPoint, AActor* PrimaryTarget);
};

// Skylanders Conquest - Tower (Lane Structure)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkylandersTower.generated.h"

class UStaticMeshComponent;
class UWidgetComponent;
class UProgressBar;
class UTextBlock;
class USoundBase;
class USceneComponent;
class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class ETowerTeam : uint8
{
	Friendly,	// Player's tower - shoots enemies
	Enemy		// Enemy tower - shoots player
};

UCLASS()
class SKYLANDERS_CONQUEST_API ASkylandersTower : public AActor
{
	GENERATED_BODY()

public:
	ASkylandersTower();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaTime) override;

	// ========== COMPONENTS ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* BaseMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* TowerMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	UWidgetComponent* HealthBarComp;

	// ========== STATS ==========

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float CurrentHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	FString TowerName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float AttackDamage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float AttackRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float AttackCooldown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	ETowerTeam Team;

	// The tower protecting this tower - must be destroyed before this tower can be damaged
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	ASkylandersTower* ProtectingTower;

	float AttackTimer;
	AActor* CurrentTarget;
	bool bDestroyed;

	// Phoenix: respawns after destruction
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	bool bIsPhoenix;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float PhoenixRespawnTime;

	FTimerHandle PhoenixRespawnTimer;
	void RespawnPhoenix();

	// ========== TOWER AGGRO (SMITE RULE) ==========
	// When a player attacks an enemy while inside this tower's range,
	// the tower temporarily switches to targeting that player.

	UPROPERTY()
	AActor* ForcedPlayerTarget;

	float ForcedPlayerTimer;

	// ========== AUDIO ==========

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* AttackSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* DestroySound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* PhoenixRespawnSound;

	// ========== FUNCTIONS ==========

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void TakeDamage_Custom(float DamageAmount, AActor* DamageCauser);

	/** Called when a player attacks an enemy while in tower range - forces tower to target that player temporarily. */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void NotifyPlayerAggro(AActor* Player);

	bool IsVulnerable() const;
	void Die();
	void FindTarget();
	void AttackTarget();
	void UpdateHealthBar();

	// Health bar cache
	bool bHealthBarInitialized;
	UProgressBar* CachedHealthBar;
	UTextBlock* CachedNameText;
	UTextBlock* CachedHealthText;

	// ========== VISUALS (code-built, not exposed) ==========
	// BaseMesh is the blocking upper step and TowerMesh the column; the rest only
	// dress the silhouette: a wide lower slab, a flared skirt, a chalice under a
	// ring platform and a floating team crystal (two on a phoenix).

	// Cancels BaseMesh's scale so every part below is laid out in world units
	UPROPERTY() USceneComponent* VisualRoot;
	UPROPERTY() UStaticMeshComponent* SlabMesh;
	UPROPERTY() UStaticMeshComponent* SkirtMesh;
	UPROPERTY() UStaticMeshComponent* ChaliceMesh;
	UPROPERTY() UStaticMeshComponent* PlatformMesh;
	UPROPERTY() USceneComponent* CrystalPivot;
	// Two cones per crystal (top, bottom); [2],[3] are the phoenix's second crystal
	UPROPERTY() TArray<UStaticMeshComponent*> CrystalParts;
	UPROPERTY() UMaterialInstanceDynamic* StoneMID;
	UPROPERTY() UMaterialInstanceDynamic* TrimMID;

	float CrystalTime;
	float CrystalBaseZ;

	void LayoutVisuals();
	// Phoenix ruin look (dark stone, crystals out) vs. alive
	void SetRuinedLook(bool bRuined);
};

// Skylanders Conquest - Lane Minion

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SkylandersTower.h"
#include "SkylandersMinion.generated.h"

class UWidgetComponent;
class UProgressBar;
class UTextBlock;
class ASkylandersTitan;

UENUM(BlueprintType)
enum class EMinionType : uint8
{
	Melee,
	Ranged
};

UCLASS()
class SKYLANDERS_CONQUEST_API ASkylandersMinion : public ACharacter
{
	GENERATED_BODY()

public:
	ASkylandersMinion();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	// ========== COMPONENTS ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	UStaticMeshComponent* BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	UWidgetComponent* HealthBarComp;

	// ========== STATS ==========

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float CurrentHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float AttackDamage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float AttackRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float AggroRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MoveSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float AttackCooldown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float XPReward;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 CoinReward;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	ETowerTeam Team;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	EMinionType MinionType;

	// Where this minion is heading (enemy base direction)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FVector LaneTargetPoint;

	// Ordered lane path toward the enemy base (last point = the base). When set,
	// the minion follows these bends instead of walking straight at the target,
	// so it stays on a curved lane. Empty = legacy straight-line behaviour.
	UPROPERTY()
	TArray<FVector> LaneWaypoints;

	int32 CurrentWaypointIndex;

	// The point to march toward right now (advances through LaneWaypoints as the
	// minion reaches each one; falls back to LaneTargetPoint if no waypoints).
	FVector GetLaneGoal();

	bool bDead;
	float AttackTimer;

	UPROPERTY()
	AActor* CurrentTarget;

	// Forced aggro target (e.g. player attacked this minion)
	UPROPERTY()
	AActor* ForcedAggroTarget;
	float ForcedAggroTimer; // How long forced aggro lasts

	// Stuck detection for obstacle avoidance
	FVector LastPosition;
	float StuckTimer;
	float StuckCheckInterval;
	bool bAvoidingObstacle;
	float AvoidanceDirection; // +1 or -1 (go left or right around obstacle)
	float AvoidanceTimer;

	// ========== FUNCTIONS ==========

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void TakeDamage_Custom(float DamageAmount, AActor* DamageCauser);

	void Die();
	void UpdateAI(float DeltaTime);
	void FindTarget();
	void AttackTarget();
	void FaceTarget(AActor* Target);
	void UpdateHealthBar();

	// Health bar cache
	bool bHealthBarInitialized;
	UProgressBar* CachedHealthBar;
	UTextBlock* CachedNameText;
	UTextBlock* CachedHealthText;
};

// Skylanders Conquest - Enemy with AI

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SkylandersEnemy.generated.h"

class UWidgetComponent;
class UEnemyHealthBarWidget;
class UProgressBar;
class UTextBlock;
class ASkylandersCharacter;

UENUM(BlueprintType)
enum class EEnemyState : uint8
{
	Idle,
	Chasing,
	Attacking,
	Returning
};

UCLASS()
class SKYLANDERS_CONQUEST_API ASkylandersEnemy : public ACharacter
{
	GENERATED_BODY()

public:
	ASkylandersEnemy();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	bool bHealthBarInitialized;

public:
	// Visible body mesh
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	UStaticMeshComponent* BodyMesh;

	// Health bar widget above head
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	UWidgetComponent* HealthBarComp;

	// ========== STATS ==========

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float CurrentHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	FString EnemyName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float RespawnDelay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float AttackDamage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float XPReward;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 CoinReward;

	// ========== AI ==========

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float AggroRange; // Distance to detect player

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float AttackRange; // Distance to start attacking

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float LeashRange; // Max distance from spawn before returning

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float ChaseSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float AttackCooldown; // Seconds between attacks

	UPROPERTY(BlueprintReadOnly, Category = "AI")
	EEnemyState CurrentState;

	float AttackTimer; // Time until next attack
	ASkylandersCharacter* TargetPlayer;

	// ========== FUNCTIONS ==========

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void TakeDamage_Custom(float DamageAmount, AActor* DamageCauser);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void Die();

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void RespawnEnemy();

	void UpdateHealthBar();

	// AI
	void UpdateAI(float DeltaTime);
	ASkylandersCharacter* FindPlayer();
	void FaceTarget(AActor* Target);

	FTimerHandle RespawnTimerHandle;
	FVector SpawnLocation;
	FRotator SpawnRotation;

	// Cached widget references
	UProgressBar* CachedHealthBar;
	UTextBlock* CachedNameText;
	UTextBlock* CachedHealthText;
};

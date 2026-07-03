// Skylanders Conquest - Jungle Buff Camp (Neutral Objective)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkylandersBuffCamp.generated.h"

class UStaticMeshComponent;
class UWidgetComponent;
class UProgressBar;
class UTextBlock;
class ASkylandersCharacter;
class USoundBase;

UENUM(BlueprintType)
enum class EBuffType : uint8
{
	Damage,     // +25% damage
	Speed,      // +30% movement speed
	Mana,       // +100% mana regen (blue buff)
	None        // XP/Gold only (mid camp)
};

UCLASS()
class SKYLANDERS_CONQUEST_API ASkylandersBuffCamp : public AActor
{
	GENERATED_BODY()

public:
	ASkylandersBuffCamp();

protected:
	virtual void BeginPlay() override;

public:
	// ========== COMPONENTS ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	UWidgetComponent* HealthBarComp;

	// ========== STATS ==========

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float CurrentHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	FString CampName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float RespawnDelay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff")
	float BuffDuration;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff")
	float BuffDamageMultiplier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float XPReward;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 CoinReward;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	bool bDead;

	// ========== BUFF TYPE ==========

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff")
	EBuffType BuffType;

	// ========== BUFF TRACKING ==========

	UPROPERTY()
	AActor* BuffedPlayer;

	// Buff removal subtracts the exact bonus that was applied — never restore an
	// absolute snapshot, since the player may level up or gain other buffs meanwhile
	float AppliedPowerBonus;
	float AppliedSpeedBonus;
	float AppliedManaRegenBonus;

	// ========== AI ==========

	FVector HomeLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float LeashRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float AttackRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float AttackDamage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float AttackCooldown;

	float AttackTimer;

	UPROPERTY()
	AActor* AggroTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float MoveSpeed;

	// ========== AUDIO ==========

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* DeathSound;

	// ========== FUNCTIONS ==========

	virtual void Tick(float DeltaTime) override;
	void UpdateAI(float DeltaTime);
	void FaceTarget(AActor* Target);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void TakeDamage_Custom(float DamageAmount, AActor* DamageCauser);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void Die();

	void RemoveBuff();
	void RespawnCamp();
	void UpdateHealthBar();

	// ========== TIMERS ==========

	FTimerHandle RespawnTimerHandle;
	FTimerHandle BuffTimerHandle;

	// ========== HEALTH BAR CACHE ==========

	bool bHealthBarInitialized;
	UProgressBar* CachedHealthBar;
	UTextBlock* CachedNameText;
	UTextBlock* CachedHealthText;
};

// Skylanders Conquest - AI-Controlled Enemy God (Player Equivalent)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SkylandersItem.h"
#include "SkylandersEnemyGod.generated.h"

class UWidgetComponent;
class UProgressBar;
class UTextBlock;
class USoundBase;
class ASkylandersCharacter;
class ASkylandersMinion;
class ASkylandersTower;
class ASkylandersTitan;

UENUM(BlueprintType)
enum class EGodAIState : uint8
{
	Laning,      // Farming minions in lane
	Fighting,    // Engaging the player
	Retreating,  // Low HP, running to base
	Dead         // Waiting to respawn
};

UCLASS()
class SKYLANDERS_CONQUEST_API ASkylandersEnemyGod : public ACharacter
{
	GENERATED_BODY()

public:
	ASkylandersEnemyGod();

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
	float MaxMana;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float CurrentMana;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float AttackDamage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float AttackRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float AttackCooldown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MoveSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float AggroRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 Level;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float XPReward;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 CoinReward;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	FString GodName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float RespawnDelay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float BasePower;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float Protections;

	// ========== AI STATE ==========

	UPROPERTY(BlueprintReadOnly, Category = "AI")
	EGodAIState CurrentState;

	// ========== INVENTORY & GOLD ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	int32 Gold;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	float PassiveGoldPerSecond;

	float PassiveGoldAccumulator;

	static const int32 MaxInventorySlots = 6;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TArray<int32> Inventory;

	// Bonus stats from items (recalculated after purchases)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item Stats")
	FSkylandersItemStats ItemBonusStats;

	// ========== AUDIO ==========

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* AttackSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* AbilitySound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* DeathSound;

	// ========== FUNCTIONS ==========

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void TakeDamage_Custom(float DamageAmount, AActor* DamageCauser);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void Die();

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void RespawnGod();

	void UpdateAI(float DeltaTime);
	void UpdateHealthBar();
	void FaceTarget(AActor* Target);

	/** Get effective power including item bonuses */
	float GetEffectivePower() const;

	/** Get effective protections including item bonuses */
	float GetEffectiveProtections() const;

private:
	// Internal timers
	float AttackTimer;
	float AbilityCooldownTimer;   // 8-second ability timer
	float GameTimeAccumulator;    // Tracks total game time for level-ups
	float LevelUpInterval;        // Seconds between level-ups (60)

	// Passive regen
	void TickPassiveRegen(float DeltaTime);

	// Passive gold income
	void TickPassiveGold(float DeltaTime);

	// AI helpers
	ASkylandersCharacter* FindPlayer();
	ASkylandersMinion* FindNearestEnemyMinion(); // Finds player-team minions to attack
	void PerformAttack(AActor* Target);
	void UseAbility(); // Burst damage ability

	// Tower awareness: check if a position is in range of a Friendly (player-team) tower
	bool IsPositionUnderFriendlyTower(const FVector& Position) const;

	// Check if chasing the player would put us under a friendly tower
	bool WouldChaseIntoFriendlyTower(AActor* Target) const;

	// Item purchasing AI
	void TryPurchaseItems();
	void RecalculateItemBonuses();

	// Priority list of items for the AI to buy (damage first, then defense)
	static const TArray<int32>& GetItemPurchasePriority();

	// Cached references
	ASkylandersCharacter* CachedPlayer;

	// Spawn / respawn
	FVector BasePosition; // Where the god retreats to and respawns
	FTimerHandle RespawnTimerHandle;

	// Health bar widget cache
	bool bHealthBarInitialized;
	UProgressBar* CachedHealthBar;
	UTextBlock* CachedNameText;
	UTextBlock* CachedHealthText;
};

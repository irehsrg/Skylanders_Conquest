// Skylanders Conquest - AI-Controlled Enemy God (Player Equivalent)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SkylandersItem.h"
#include "SkylandersTower.h" // ETowerTeam
#include "SkylandersBuffCamp.h" // EBuffType
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
	Retreating,  // Low HP, running to base to heal
	Dead,        // Waiting to respawn
	TowerBackoff,// Stepped into an enemy tower without cover: hold outside its range
	Jungling     // Clearing a neutral buff camp
};

// Which item purchase list the god's shopping AI walks (see GetItemPurchasePriority).
UENUM(BlueprintType)
enum class EGodBuildArchetype : uint8
{
	Bruiser,     // Health + power
	Assassin,    // Power + penetration + crit
	Tank         // Protections + health + CDR
};

UCLASS()
class SKYLANDERS_CONQUEST_API ASkylandersEnemyGod : public ACharacter
{
	GENERATED_BODY()

public:
	ASkylandersEnemyGod();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

	// Base protections at level 1. The per-level term is added at read time in
	// GetEffectiveProtections() (not accumulated here), so level-ups never mutate
	// stored stats and Penetration has something to bite on before items.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float Protections;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float ProtectionsPerLevel;

	// ========== TEAM ==========

	// Which side this god fights for. Enemy = red team (default, opposes the
	// player); Friendly = blue team (AI ally that fights alongside the player).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	ETowerTeam Team = ETowerTeam::Enemy;

	// The opposing team this god targets.
	ETowerTeam FoeTeam() const { return Team == ETowerTeam::Enemy ? ETowerTeam::Friendly : ETowerTeam::Enemy; }

	// Push direction along the lane: enemy pushes toward -X (blue base), a
	// friendly ally pushes toward +X (red base).
	float LaneSign() const { return Team == ETowerTeam::Enemy ? -1.0f : 1.0f; }

	// Real skeletal model this god wears ("Hex" or "TreeRex"). Empty = the old
	// placeholder cylinder. Set by the map builder before BeginPlay.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	FString GodModel;

	// Flat team-colored ring under the god (so teams read at a glance once the
	// bodies are real Skylander models instead of red/blue cylinders).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	UStaticMeshComponent* TeamRing;

	// ========== AI STATE ==========

	UPROPERTY(BlueprintReadOnly, Category = "AI")
	EGodAIState CurrentState;

	// Number of actual state changes this match (SetState only counts real changes).
	// Read by the smoke test for its per-god thrash summary.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	int32 StateChangeCount = 0;

	// Item build this god shops for. Set by the map builder before FinishSpawning.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	EGodBuildArchetype BuildArchetype = EGodBuildArchetype::Bruiser;

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

	// ========== SCOREBOARD (K / D / A, CS) ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	int32 Kills = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	int32 Deaths = 0;

	// Damaged an opposing hero within the assist window of its death without
	// landing the kill
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	int32 Assists = 0;

	// Minions this god was paid for (last hits)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	int32 CreepScore = 0;

	// Opposing heroes that hit this god recently -> world time of their last hit
	// (resolved through the instigator chain). Die() grants assists from it.
	TMap<TWeakObjectPtr<AActor>, float> RecentHeroDamagers;

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

	// Every CurrentState change after construction goes through here.
	void SetState(EGodAIState NewState);
	void UpdateHealthBar();
	void FaceTarget(AActor* Target);

	// Swap the placeholder cylinder for a real animated skeletal model (GodModel).
	void ApplyGodModel();

	/** Get effective power including item bonuses */
	float GetEffectivePower() const;

	/** Get effective protections including item bonuses */
	float GetEffectiveProtections() const;

	/** 1 - clamp(item CDR, 0, 0.40); applied once when the ability timer is set */
	float GetCooldownMultiplier() const;

	// Called by ASkylandersBuffCamp when this god lands the killing blow: pays the
	// camp's gold and applies its buff to this god's own stat fields for Duration.
	void ApplyCampBuff(EBuffType Type, float DamageMultiplier, float Duration, int32 Coins, const FString& CampName);

	// Is a camp buff of this type currently active on the god?
	bool HasCampBuff(EBuffType Type) const;

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
	// Nearest opposing hero: the player and/or ally gods for an enemy god, or
	// enemy gods for a friendly ally. Returns null if none in the world.
	AActor* FindEnemyHero();
	// Health fraction (0..1) of a hero actor (player or another god).
	static float GetHeroHealthPct(AActor* Hero);
	// Nearest living FoeTeam() minion within SearchRadius. Pass a radius wider than
	// AttackRange when the god should *walk toward* the wave (laning); pass the
	// default (AttackRange) when it should only consider what it can already hit.
	ASkylandersMinion* FindNearestEnemyMinion(float SearchRadius = -1.0f);
	// Lowest-health living FoeTeam() minion within Radius (last-hit priority).
	ASkylandersMinion* FindWeakestEnemyMinion(float Radius);
	// Living own-team minions within Radius of Pos (tower-dive cover).
	int32 CountOwnMinionsNear(const FVector& Pos, float Radius) const;
	// Every living opposing hero (player and/or gods).
	void GatherEnemyHeroes(TArray<AActor*>& OutHeroes);
	static bool IsHeroAlive(AActor* Hero);
	void PerformAttack(AActor* Target);
	// Burst damage ability on Target (null = nearest opposing hero)
	void UseAbility(AActor* Target = nullptr);

	// Fighting target choice: lowest-health opposing hero in attack range, else the
	// nearest within FightLeash; never one under an enemy tower. The current target
	// is kept for at least MinTargetCommitSeconds so the god doesn't flicker.
	AActor* SelectFightTarget(float Now);
	void BeginFighting(AActor* Target, float Leash);
	TWeakObjectPtr<AActor> FightTarget;
	float FightTargetSince = 0.0f;
	float FightLeash = 1500.0f;
	float NextEngageTime = 0.0f; // Laning won't start a fight before this (world time)
	static constexpr float MinTargetCommitSeconds = 1.5f;

	// Blue allies join the player's fight (player within 2500 and took/dealt hero
	// damage in the last 4 s). Returns true if it switched to Fighting.
	bool TryAllyAssist(float Now, float HealthPct);

	// Tower back-off (distinct from the low-health Retreating): walk to a hold
	// point outside the tower's range + margin and stay for TowerBackoffMinHold
	// seconds, re-entering only with minion cover or once the tower falls.
	void EnterTowerBackoff(ASkylandersTower* Tower);
	TWeakObjectPtr<ASkylandersTower> BackoffTower;
	FVector BackoffHoldPoint = FVector::ZeroVector;
	float BackoffStartTime = 0.0f;
	static constexpr float TowerBackoffMinHold = 4.0f;
	static constexpr float TowerBackoffMargin = 350.0f;
	static constexpr int32 TowerCoverMinions = 3;

	// Jungling: clear a buff camp this god doesn't already hold the buff of.
	ASkylandersBuffCamp* FindJungleCamp(float MaxDist) const;
	bool TryStartJungling(float MaxDist);
	TWeakObjectPtr<ASkylandersBuffCamp> JungleTarget;
	float JungleStartTime = 0.0f;
	float NextJungleTime = 0.0f; // cooldown after an attempt, so lane <-> jungle can't flip
	// Stall detection on the walk to the camp (see the Jungling case)
	float JungleProgressTime = 0.0f;
	float JungleBestDist = 0.0f;
	bool bJungleSteerDirect = false;

	// Active camp buffs: world time each expires (0 = inactive) and the exact
	// bonus applied, so expiry subtracts what was added (level-ups stay intact).
	float DamageBuffExpire = 0.0f;
	float SpeedBuffExpire = 0.0f;
	float ManaBuffExpire = 0.0f;
	float CampPowerBonus = 0.0f;
	float CampAttackBonus = 0.0f;
	float CampSpeedBonus = 0.0f;
	// Drop expired buffs (all of them when bForceAll, e.g. on death).
	void ExpireCampBuffs(bool bForceAll);

	// Walk speed with items and the speed camp buff
	float GetWalkSpeed() const { return MoveSpeed + ItemBonusStats.MovementSpeed + CampSpeedBonus; }

	// NavMesh-based movement: paths around structures/terrain via the AIController
	// (falls back to straight steering if no navmesh/path). Throttled so it doesn't
	// restart the path every tick. StopMoving halts the current path.
	void MoveToPoint(const FVector& Dest, float AcceptRadius, float FallbackScale = 1.0f);
	void StopMoving();
	FVector LastMoveGoal = FVector(FLT_MAX);

	// Tower awareness: is a position inside an enemy (FoeTeam) tower's range?
	bool IsPositionUnderEnemyTower(const FVector& Position) const;
	// The living FoeTeam tower whose range covers Position (nearest), or null.
	ASkylandersTower* FindEnemyTowerCovering(const FVector& Position) const;

	// Check if chasing a target would put us under an enemy tower
	bool WouldChaseIntoEnemyTower(AActor* Target) const;

	// Item purchasing AI
	void TryPurchaseItems();
	void RecalculateItemBonuses();

	// Purchase list for an archetype, ascending in cost. TryPurchaseItems buys the
	// most expensive affordable entry not yet owned (one copy per list occurrence).
	static const TArray<int32>& GetItemPurchasePriority(EGodBuildArchetype Archetype);

	// Cached references
	ASkylandersCharacter* CachedPlayer;

	// Who landed the most recent hit, so Die() can credit the actual killer
	// instead of handing every red-god death to the player.
	UPROPERTY()
	AActor* LastDamageCauser = nullptr;

	// Spawn / respawn
	FVector BasePosition; // Where the god retreats to and respawns
	FTimerHandle RespawnTimerHandle;

	// Health bar widget cache
	bool bHealthBarInitialized;
	UProgressBar* CachedHealthBar;
	UTextBlock* CachedNameText;
	UTextBlock* CachedHealthText;
};

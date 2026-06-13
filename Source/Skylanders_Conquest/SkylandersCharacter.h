// Skylanders Conquest - Player Character with Health, Mana, Coins, Abilities, Inventory

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SkylandersItem.h"
#include "SkylandersCharacter.generated.h"

// Forward declarations
class UInputMappingContext;
class UInputAction;
class UUserWidget;
class UWidget;
class UTextBlock;
class USpringArmComponent;
class UCameraComponent;
class UAnimSequenceBase;
class UStaticMeshComponent;
class USoundBase;
struct FInputActionValue;

UCLASS()
class SKYLANDERS_CONQUEST_API ASkylandersCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASkylandersCharacter();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// ========== COMPONENTS ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* FollowCamera;

	// Gun mesh components attached to hands
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	USkeletalMeshComponent* LeftGunMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	USkeletalMeshComponent* RightGunMesh;

	// ========== VFX INDICATOR COMPONENTS ==========

	// Ground aim circle (flat cylinder, repositioned each tick)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VFX")
	UStaticMeshComponent* GroundAimIndicator;

	// Recall channeling circle
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VFX")
	UStaticMeshComponent* RecallIndicator;

	// Yamato charge-up sphere (spawned actor, destroyed on fire)
	AActor* YamatoChargeVFX;

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
	float ManaRegenRate; // Mana per second

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 PlayerLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 Coins;

	// XP
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float CurrentXP;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float XPToNextLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 MaxLevel;

	// Stat growth per level
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float HealthPerLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float ManaPerLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float ManaRegenPerLevel;

	// ========== INVENTORY & SHOP ==========

	static const int32 MaxInventorySlots = 6;

	// Inventory: stores ItemIDs (0 = empty slot)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TArray<int32> Inventory;

	// Whether player is in spawn area (can purchase)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	bool bIsInSpawnArea;

	// Whether shop UI is open
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	bool bIsShopOpen;

	// Passive gold income
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float PassiveGoldPerSecond;

	float PassiveGoldAccumulator; // Fractional gold accumulator

	// Base stats (before items) - used for recalculation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float BasePower;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float HealthRegenRate; // Base HP per second

	// Bonus stats from items (read-only, recalculated)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item Stats")
	FSkylandersItemStats ItemBonusStats;

	// Shop widget class (set in Blueprint)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UUserWidget> ShopWidgetClass;

	// Shop widget instance
	UPROPERTY(BlueprintReadOnly, Category = "UI")
	UUserWidget* ShopWidget;

	// ========== RECALL ==========

	UPROPERTY(BlueprintReadOnly, Category = "Recall")
	bool bIsRecalling;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recall")
	float RecallChannelTime; // 5.0 seconds

	UPROPERTY(BlueprintReadOnly, Category = "Recall")
	float RecallRemainingTime;

	FTimerHandle RecallTimerHandle;

	// Death respawn countdown
	float DeathRespawnTimeTotal;
	float DeathRespawnTimeRemaining;

	// ========== KDA & GAME TRACKING ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	int32 Kills;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	int32 Deaths;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	int32 CreepScore;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	float GameElapsedTime;

	UFUNCTION(BlueprintCallable, Category = "Recall")
	void StartRecall();

	UFUNCTION(BlueprintCallable, Category = "Recall")
	void CancelRecall();

	UFUNCTION(BlueprintCallable, Category = "Recall")
	void CompleteRecall();

	UFUNCTION(BlueprintCallable, Category = "Recall")
	void TeleportToBase();

	// ========== ABILITY LEVELING ==========

	// Unspent skill points
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
	int32 AbilityPoints;

	// Current rank of each ability (0 = not learned, 1-5)
	int32 AbilityLevels[4];

	// Max rank per ability
	static const int32 MaxAbilityRank = 5;

	UFUNCTION(BlueprintCallable, Category = "Abilities")
	void LevelUpAbility(int32 AbilityIndex);

	UFUNCTION(BlueprintCallable, Category = "Abilities")
	bool CanLevelAbility(int32 AbilityIndex) const;

	int32 GetAbilityRank(int32 AbilityIndex) const;

	// ========== ABILITIES ==========

	// Ability 1
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
	float Ability1_Cooldown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
	float Ability1_ManaCost;

	UPROPERTY(BlueprintReadOnly, Category = "Abilities")
	float Ability1_RemainingCooldown;

	// Ability 2
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
	float Ability2_Cooldown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
	float Ability2_ManaCost;

	UPROPERTY(BlueprintReadOnly, Category = "Abilities")
	float Ability2_RemainingCooldown;

	// Ability 3
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
	float Ability3_Cooldown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
	float Ability3_ManaCost;

	UPROPERTY(BlueprintReadOnly, Category = "Abilities")
	float Ability3_RemainingCooldown;

	// Ability 4 (Ultimate)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
	float Ability4_Cooldown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
	float Ability4_ManaCost;

	UPROPERTY(BlueprintReadOnly, Category = "Abilities")
	float Ability4_RemainingCooldown;

	// ========== HUD ==========

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UUserWidget> MainHUDClass;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	UUserWidget* MainHUDWidget;

	// ========== INVENTORY HUD ==========

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	class USkylandersInventoryWidget* InventoryHUDWidget;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	class USkylandersItemHUDWidget* ItemHUDWidget;

	void CreateInventoryHUD();

	// ========== MINIMAP & SCOREBOARD ==========

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	class USkylandersMinimapWidget* MinimapWidget;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	class USkylandersScoreboardWidget* ScoreboardWidget;

	void ShowScoreboard();
	void HideScoreboard();

	// ========== PROJECTILE ==========

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	TSubclassOf<AActor> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	FVector ProjectileSpawnOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float ProjectileSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float FireRate; // Shots per second (e.g., 2.0 = fire every 0.5 seconds)

	// Movement penalties
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float StrafeSpeedMultiplier; // Speed when strafing (0-1)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float BackpedalSpeedMultiplier; // Speed when moving backward (0-1)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float AttackMoveSpeedMultiplier; // Speed while attack slow is active (0-1)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float AttackSlowDuration; // How long the attack slow lasts

	float AttackSlowRemainingTime; // Timer for attack movement penalty
	float BaseMaxWalkSpeed; // Stored base walk speed

	// Ability 3 - Golden Machine Gun state
	UPROPERTY(BlueprintReadOnly, Category = "Abilities")
	bool bMachineGunActive;

	UPROPERTY(BlueprintReadOnly, Category = "Abilities")
	float MachineGunRemainingTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
	float MachineGunDuration;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
	float MachineGunFireRate; // Shots per second during minigun

	// Ability 4 - Yamato Blast state
	UPROPERTY(BlueprintReadOnly, Category = "Abilities")
	bool bChargingYamato;

	UPROPERTY(BlueprintReadOnly, Category = "Abilities")
	float YamatoChargeRemaining;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
	float YamatoChargeTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
	float YamatoDamage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
	float YamatoRange; // Close range blast radius

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float LastFireTime;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bFireFromLeftGun; // Alternates between left and right gun

	// ========== INPUT ==========

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* FireAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* Ability1Action;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* Ability2Action;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* Ability3Action;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* Ability4Action;

	// ========== PUBLIC FUNCTIONS ==========

	// Damage and healing
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void TakeDamage_Custom(float DamageAmount);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void Heal(float HealAmount);

	// Mana management
	UFUNCTION(BlueprintCallable, Category = "Stats")
	bool UseMana(float ManaCost);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void RestoreMana(float ManaAmount);

	// Coins
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void AddCoins(int32 Amount);

	// Level & XP
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void LevelUp();

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void AddXP(float Amount);

	// Death and respawn
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void Die();

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void Respawn();

	// HUD updates
	UFUNCTION(BlueprintCallable, Category = "UI")
	void UpdateHUD();

	// Abilities
	UFUNCTION(BlueprintCallable, Category = "Abilities")
	void UseAbility1();

	UFUNCTION(BlueprintCallable, Category = "Abilities")
	void UseAbility2();

	UFUNCTION(BlueprintCallable, Category = "Abilities")
	void UseAbility3();

	UFUNCTION(BlueprintCallable, Category = "Abilities")
	void UseAbility4();

	// Shooting
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void FireProjectile();

	// ========== SHOP & INVENTORY ==========

	// Open/close/toggle the shop UI (browsing works anywhere)
	UFUNCTION(BlueprintCallable, Category = "Shop")
	void ToggleShop();

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void OpenShop();

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void CloseShop();

	// Buy item by ID - returns true if successful
	UFUNCTION(BlueprintCallable, Category = "Shop")
	bool BuyItem(int32 ItemID);

	// Sell item from inventory slot (0-5) - returns true if successful
	UFUNCTION(BlueprintCallable, Category = "Shop")
	bool SellItem(int32 SlotIndex);

	// Get effective stat value (base + level + items)
	UFUNCTION(BlueprintCallable, Category = "Stats")
	float GetEffectivePower() const;

	UFUNCTION(BlueprintCallable, Category = "Stats")
	float GetEffectiveAttackSpeed() const;

	UFUNCTION(BlueprintCallable, Category = "Stats")
	float GetEffectiveProtections() const;

	// Recalculate all item bonus stats from inventory
	void RecalculateItemBonuses();

protected:
	// Input callbacks
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Fire(const FInputActionValue& Value);

	// Update cooldowns
	void UpdateCooldowns(float DeltaTime);

	// Debug key handlers
	void Debug_TakeDamage();
	void Debug_Heal();
	void Debug_UseMana();
	void Debug_LevelUp();
	void Debug_AddGold();

	// Cooldown UI update
	void UpdateCooldownUI();
	bool bCooldownWidgetsCached;
	UWidget* CooldownOverlay[4];
	UWidget* CooldownText[4];

	// Ability leveling key handlers
	void Debug_LevelAbility1();
	void Debug_LevelAbility2();
	void Debug_LevelAbility3();
	void Debug_LevelAbility4();

	// Debug shop: buy/sell with keys while shop is open
	void Debug_ShopBuy5();
	void Debug_ShopBuy6();
	void Debug_ShopSell();
	int32 DebugShopCursor; // Which item ID to buy next (cycles with key)

	// Ground targeting - traces camera through crosshair to ground plane
	FVector GetGroundAimPoint(float MaxRange = 1500.0f) const;

	// Respawn timer
	FTimerHandle RespawnTimerHandle;

	// Player start for respawn
	AActor* GetPlayerStart();

	// ========== AUDIO (assign in Blueprint Class Defaults) ==========

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* AttackSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* AbilitySound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* DeathSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* LevelUpSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* ItemBuySound;

	// ========== ANIMATION SEQUENCES (set in Blueprint Class Defaults) ==========

	// Attack animations (played as dynamic montages on "DefaultSlot")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimSequenceBase* AttackLeftAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimSequenceBase* AttackRightAnim;

	// Ability animations
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimSequenceBase* Ability1Anim; // Golden Super Charge

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimSequenceBase* Ability2Anim; // Pot o' Gold (lob)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimSequenceBase* MachineGunStartAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimSequenceBase* MachineGunLoopAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimSequenceBase* MachineGunEndAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimSequenceBase* YamatoAnim; // Ability 4

	// Hit reaction
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimSequenceBase* HitReactAnim;

	// Death
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimSequenceBase* DeathAnim;

	// Helper: play an anim sequence as a dynamic montage overlay
	void PlayAnimOnSlot(UAnimSequenceBase* Anim, float PlayRate = 1.0f, float BlendIn = 0.1f, float BlendOut = 0.1f, FName SlotName = TEXT("DefaultSlot"));
};

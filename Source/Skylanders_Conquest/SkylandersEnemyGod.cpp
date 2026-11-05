// Skylanders Conquest - AI-Controlled Enemy God Implementation

#include "SkylandersEnemyGod.h"
#include "SkylandersSmokeTest.h"
#include "SkylandersSoundLibrary.h"
#include "SkylandersHitFlash.h"
#include "SkylandersKillFeedWidget.h"
#include "SkylandersAnnouncerWidget.h"
#include "SkylandersTelemetry.h"
#include "SkylandersSimpleAnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "SkylandersCharacter.h"
#include "SkylandersMinion.h"
#include "SkylandersTower.h"
#include "SkylandersTitan.h"
#include "SkylandersBuffCamp.h"
#include "SkylandersDamageNumber.h"
#include "SkylandersItemCatalog.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/WidgetComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Kismet/GameplayStatics.h"
#include "SkylandersActorRegistry.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

ASkylandersEnemyGod::ASkylandersEnemyGod()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// AI controller
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;

	// Capsule
	GetCapsuleComponent()->InitCapsuleSize(30.0f, 50.0f);
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
	// Overlap the player so body contact can never lift them off the ground.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// Movement
	GetCharacterMovement()->MaxWalkSpeed = 450.0f;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	GetCharacterMovement()->GravityScale = 1.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 1000.0f;
	// RVO avoidance for planar separation from other units
	GetCharacterMovement()->bUseRVOAvoidance = true;
	GetCharacterMovement()->AvoidanceConsiderationRadius = 200.0f;

	// Body mesh - larger cylinder, dark red
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	BodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -50.0f));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CylinderMesh.Object);
		BodyMesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.5f));
	}

	// Inherited character skeletal mesh (used when GodModel names a real model).
	// Skylander rips face +X, so no -90 yaw; visual only, no collision.
	GetMesh()->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Flat team-colored ring at the god's feet
	TeamRing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TeamRing"));
	TeamRing->SetupAttachment(RootComponent);
	TeamRing->SetRelativeLocation(FVector(0.0f, 0.0f, -48.0f));
	TeamRing->SetRelativeScale3D(FVector(1.7f, 1.7f, 0.03f));
	TeamRing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TeamRing->SetCastShadow(false);
	if (CylinderMesh.Succeeded())
	{
		TeamRing->SetStaticMesh(CylinderMesh.Object);
	}

	// Health bar widget component
	HealthBarComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
	HealthBarComp->SetupAttachment(RootComponent);
	HealthBarComp->SetRelativeLocation(FVector(0.0f, 0.0f, 110.0f));
	HealthBarComp->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarComp->SetDrawSize(FVector2D(180.0f, 40.0f));
	HealthBarComp->SetPivot(FVector2D(0.5f, 1.0f));

	// Stats
	MaxHealth = 600.0f;
	CurrentHealth = 600.0f;
	MaxMana = 300.0f;
	CurrentMana = 300.0f;
	AttackDamage = 25.0f;
	AttackRange = 800.0f;
	AttackCooldown = 1.0f;
	MoveSpeed = 450.0f;
	AggroRange = 1200.0f;
	Level = 1;
	XPReward = 300.0f;
	CoinReward = 150;
	GodName = TEXT("Enemy God");
	RespawnDelay = 15.0f;
	BasePower = 25.0f;
	Protections = 10.0f;
	ProtectionsPerLevel = 1.0f;

	// AI
	CurrentState = EGodAIState::Laning;
	AttackTimer = 0.0f;
	AbilityCooldownTimer = 8.0f;
	GameTimeAccumulator = 0.0f;
	LevelUpInterval = 60.0f;

	// Inventory & Gold
	Gold = 0;
	PassiveGoldPerSecond = 2.0f;
	PassiveGoldAccumulator = 0.0f;
	Inventory.Init(0, MaxInventorySlots);

	// Cached refs
	CachedPlayer = nullptr;
	BasePosition = FVector(4500.0f, 0.0f, 0.0f);

	// Health bar cache
	bHealthBarInitialized = false;
	CachedHealthBar = nullptr;
	CachedNameText = nullptr;
	CachedHealthText = nullptr;

	// Audio
	AttackSound = nullptr;
	AbilitySound = nullptr;
	DeathSound = nullptr;
}

void ASkylandersEnemyGod::BeginPlay()
{
	Super::BeginPlay();
	if (USkylandersActorRegistry* Registry = USkylandersActorRegistry::Get(this))
	{
		Registry->Register(this);
	}

	// Default SFX for any sound slot the Blueprint left empty (assigned sounds win)
	if (!AttackSound) AttackSound = SkylandersSound::Get(TEXT("hit_light"));
	if (!AbilitySound) AbilitySound = SkylandersSound::Get(TEXT("ability_cast"));
	if (!DeathSound) DeathSound = SkylandersSound::Get(TEXT("death"));

	BasePosition = GetActorLocation(); // Use placed position as base, fallback to (4500,0,0)
	if (BasePosition.IsNearlyZero())
	{
		BasePosition = FVector(4500.0f, 0.0f, 0.0f);
	}

	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;

	// SMITE-style starting gold, same as the player: gods spawn inside the
	// 500-unit shopping radius, so the first UpdateAI buys a starter item.
	Gold = ASkylandersCharacter::StartingGold;

	// Load health bar widget
	UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UserInterface/WBP_EnemyHealthBar.WBP_EnemyHealthBar_C"));
	if (WidgetClass && HealthBarComp)
	{
		HealthBarComp->SetWidgetClass(WidgetClass);
		HealthBarComp->InitWidget();
	}

	// Team color: enemy red, friendly ally blue. Build from BasicShapeMaterial
	// explicitly — the cylinder's default slot is the engine DefaultMaterial, which
	// has no "Color" parameter (so the tint was silently ignored -> gray gods).
	const FLinearColor TeamColor = (Team == ETowerTeam::Enemy)
		? FLinearColor(0.85f, 0.12f, 0.12f) : FLinearColor(0.15f, 0.4f, 1.0f);
	if (UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial")))
	{
		if (BodyMesh)
		{
			UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, this);
			DynMat->SetVectorParameterValue(FName("Color"), TeamColor);
			BodyMesh->SetMaterial(0, DynMat);
		}
		// Team ring under the god's feet (unlit flat color reads clearly on grass)
		if (TeamRing)
		{
			UMaterialInterface* RingBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/VFX/M_MapFloor.M_MapFloor"));
			if (!RingBase) RingBase = BaseMat;
			UMaterialInstanceDynamic* RingMat = UMaterialInstanceDynamic::Create(RingBase, this);
			RingMat->SetVectorParameterValue(FName("Color"), TeamColor);
			TeamRing->SetMaterial(0, RingMat);
		}
	}

	// Swap the cylinder for a real animated Skylander model if one is assigned.
	ApplyGodModel();

	UE_LOG(LogTemp, Log, TEXT("Enemy God '%s' spawned at (%.0f, %.0f, %.0f) with %.0f HP, Level %d"),
		*GodName, BasePosition.X, BasePosition.Y, BasePosition.Z, MaxHealth, Level);

	// Delayed health bar init
	FTimerHandle InitTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(InitTimerHandle, this, &ASkylandersEnemyGod::UpdateHealthBar, 0.1f, false);
}

void ASkylandersEnemyGod::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CurrentState == EGodAIState::Dead) return;

	// Distance-based health bar rendering
	if (HealthBarComp)
	{
		APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		if (Player)
		{
			float DistToPlayer = FVector::Dist(GetActorLocation(), Player->GetActorLocation());
			HealthBarComp->SetVisibility(DistToPlayer < 3000.0f);
		}
	}

	// === Passive regen ===
	TickPassiveRegen(DeltaTime);

	// === Passive gold income ===
	TickPassiveGold(DeltaTime);

	// === Scaling over time ===
	GameTimeAccumulator += DeltaTime;
	if (GameTimeAccumulator >= LevelUpInterval)
	{
		GameTimeAccumulator -= LevelUpInterval;
		Level++;
		MaxHealth += 50.0f;
		CurrentHealth += 50.0f;
		AttackDamage += 3.0f;
		BasePower += 2.0f;
		// Attack speed scaling (cap at 0.4s minimum)
		AttackCooldown = FMath::Max(0.4f, AttackCooldown - 0.03f);
		// Aggro range scaling (cap at 2000)
		AggroRange = FMath::Min(2000.0f, AggroRange + 30.0f);
		// Movement speed scaling (cap at 600)
		MoveSpeed = FMath::Min(600.0f, MoveSpeed + 5.0f);
		GetCharacterMovement()->MaxWalkSpeed = GetWalkSpeed();
		// Scale rewards with level
		XPReward = 300.0f + (Level - 1) * 30.0f;
		CoinReward = 150 + (Level - 1) * 15;
		UE_LOG(LogTemp, Warning, TEXT("Enemy God '%s' leveled up to %d! HP: %.0f, ATK: %.0f, Power: %.0f, AtkCD: %.2f, Aggro: %.0f, Speed: %.0f, Gold: %d"),
			*GodName, Level, MaxHealth, AttackDamage, BasePower, AttackCooldown, AggroRange, MoveSpeed, Gold);
		UpdateHealthBar();
	}

	// === Tick timers ===
	if (AttackTimer > 0.0f)
	{
		AttackTimer -= DeltaTime;
	}
	if (AbilityCooldownTimer > 0.0f)
	{
		AbilityCooldownTimer -= DeltaTime;
	}

	// === Camp buff expiry ===
	ExpireCampBuffs(false);

	// === AI ===
	UpdateAI(DeltaTime);
}

// ========== PASSIVE REGEN ==========

void ASkylandersEnemyGod::TickPassiveRegen(float DeltaTime)
{
	float EffMaxHealth = MaxHealth + ItemBonusStats.MaxHealth;
	float EffMaxMana = MaxMana + ItemBonusStats.MaxMana;

	// Boosted regen near base (fountain healing: 10% per second, like the player)
	float DistToBase = FVector::Dist(GetActorLocation(), BasePosition);
	float HpRegenRate = (DistToBase < 300.0f) ? 0.10f : 0.01f;
	float MpRegenRate = (DistToBase < 300.0f) ? 0.10f : 0.02f;

	// Add item health regen bonus
	float HpRegen = (EffMaxHealth * HpRegenRate + ItemBonusStats.HealthRegen) * DeltaTime;
	CurrentHealth = FMath::Min(CurrentHealth + HpRegen, EffMaxHealth);

	float MpRegen = (EffMaxMana * MpRegenRate + ItemBonusStats.ManaRegen) * DeltaTime;
	if (ManaBuffExpire > 0.0f)
	{
		MpRegen *= 2.0f; // blue buff: +100% mana regen, same as the player's
	}
	CurrentMana = FMath::Min(CurrentMana + MpRegen, EffMaxMana);
}

// ========== PASSIVE GOLD INCOME ==========

void ASkylandersEnemyGod::TickPassiveGold(float DeltaTime)
{
	// Base: 2 gold/sec + 0.5 per level, level term capped at 6. Uncapped, late-game
	// gods out-earned the player ~2:1 and finished six items while the player had four.
	float GoldRate = PassiveGoldPerSecond + 0.5f * FMath::Min(Level, 6);
	PassiveGoldAccumulator += GoldRate * DeltaTime;

	if (PassiveGoldAccumulator >= 1.0f)
	{
		int32 GoldToAdd = FMath::FloorToInt(PassiveGoldAccumulator);
		Gold += GoldToAdd;
		PassiveGoldAccumulator -= GoldToAdd;
	}
}

// ========== EFFECTIVE STATS (BASE + ITEMS) ==========

float ASkylandersEnemyGod::GetEffectivePower() const
{
	return BasePower + ItemBonusStats.Power;
}

float ASkylandersEnemyGod::GetEffectiveProtections() const
{
	return Protections + ProtectionsPerLevel * FMath::Max(Level - 1, 0) + ItemBonusStats.Protections;
}

float ASkylandersEnemyGod::GetCooldownMultiplier() const
{
	// Same rule as the player: item CDR sums uncapped, the 40% cap is applied here.
	return 1.0f - FMath::Clamp(ItemBonusStats.CooldownReduction, 0.0f, 0.40f);
}

// ========== CAMP BUFFS ==========

bool ASkylandersEnemyGod::HasCampBuff(EBuffType Type) const
{
	switch (Type)
	{
	case EBuffType::Damage: return DamageBuffExpire > 0.0f;
	case EBuffType::Speed:  return SpeedBuffExpire > 0.0f;
	case EBuffType::Mana:   return ManaBuffExpire > 0.0f;
	default:                return false;
	}
}

void ASkylandersEnemyGod::ApplyCampBuff(EBuffType Type, float DamageMultiplier, float Duration, int32 Coins, const FString& CampName)
{
	const float Expire = GetWorld()->GetTimeSeconds() + Duration;
	Gold += Coins;
	SKY_COUNT("god_camp_kill");

	// Same magnitudes as the player's camp buffs. Re-killing a camp whose buff is
	// still active only refreshes the timer; the bonus is applied once.
	FString BuffDesc = TEXT("no buff");
	switch (Type)
	{
	case EBuffType::Damage:
		if (DamageBuffExpire <= 0.0f)
		{
			// Both damage paths: AttackDamage (basic attacks) and BasePower (ability)
			CampAttackBonus = AttackDamage * (DamageMultiplier - 1.0f);
			CampPowerBonus = BasePower * (DamageMultiplier - 1.0f);
			AttackDamage += CampAttackBonus;
			BasePower += CampPowerBonus;
		}
		DamageBuffExpire = Expire;
		BuffDesc = FString::Printf(TEXT("+%.0f%% damage"), (DamageMultiplier - 1.0f) * 100.0f);
		break;
	case EBuffType::Speed:
		if (SpeedBuffExpire <= 0.0f)
		{
			CampSpeedBonus = MoveSpeed * 0.30f;
		}
		SpeedBuffExpire = Expire;
		GetCharacterMovement()->MaxWalkSpeed = GetWalkSpeed();
		BuffDesc = TEXT("+30% movement speed");
		break;
	case EBuffType::Mana:
		ManaBuffExpire = Expire; // TickPassiveRegen doubles mana regen while set
		BuffDesc = TEXT("+100% mana regen");
		break;
	default:
		break;
	}

	UE_LOG(LogTemp, Log, TEXT("God '%s' (%s) cleared '%s': +%d gold, %s for %.0fs"),
		*GodName, Team == ETowerTeam::Enemy ? TEXT("red") : TEXT("blue"), *CampName, Coins, *BuffDesc, Duration);
	USkylandersKillFeedWidget::Post(this, FString::Printf(TEXT("%s took %s"), *GodName, *CampName),
		Team == ETowerTeam::Enemy ? FLinearColor(1.0f, 0.5f, 0.3f) : FLinearColor(0.4f, 0.8f, 1.0f));
}

void ASkylandersEnemyGod::ExpireCampBuffs(bool bForceAll)
{
	const float Now = GetWorld()->GetTimeSeconds();
	auto Due = [&](float Expire) { return Expire > 0.0f && (bForceAll || Now >= Expire); };

	// Subtract exactly what was added, so level-ups during the buff are kept
	if (Due(DamageBuffExpire))
	{
		AttackDamage = FMath::Max(AttackDamage - CampAttackBonus, 0.0f);
		BasePower = FMath::Max(BasePower - CampPowerBonus, 0.0f);
		CampAttackBonus = 0.0f;
		CampPowerBonus = 0.0f;
		DamageBuffExpire = 0.0f;
		UE_LOG(LogTemp, Log, TEXT("God '%s' damage buff ended"), *GodName);
	}
	if (Due(SpeedBuffExpire))
	{
		CampSpeedBonus = 0.0f;
		SpeedBuffExpire = 0.0f;
		GetCharacterMovement()->MaxWalkSpeed = GetWalkSpeed();
		UE_LOG(LogTemp, Log, TEXT("God '%s' speed buff ended"), *GodName);
	}
	if (Due(ManaBuffExpire))
	{
		ManaBuffExpire = 0.0f;
		UE_LOG(LogTemp, Log, TEXT("God '%s' mana buff ended"), *GodName);
	}
}

// ========== TOWER AWARENESS ==========

ASkylandersTower* ASkylandersEnemyGod::FindEnemyTowerCovering(const FVector& Position) const
{
	// Towers of the opposing team — this god should not dive into these.
	TArray<AActor*> AllTowers;
	USkylandersActorRegistry::GetAllActorsOfClass(GetWorld(), ASkylandersTower::StaticClass(), AllTowers);

	ASkylandersTower* Nearest = nullptr;
	float NearestDist = TNumericLimits<float>::Max();
	for (AActor* TowerActor : AllTowers)
	{
		ASkylandersTower* Tower = Cast<ASkylandersTower>(TowerActor);
		if (Tower && Tower->Team == FoeTeam() && !Tower->bDestroyed)
		{
			float DistToTower = FVector::Dist(Position, Tower->GetActorLocation());
			if (DistToTower < Tower->AttackRange && DistToTower < NearestDist)
			{
				NearestDist = DistToTower;
				Nearest = Tower;
			}
		}
	}
	return Nearest;
}

bool ASkylandersEnemyGod::IsPositionUnderEnemyTower(const FVector& Position) const
{
	return FindEnemyTowerCovering(Position) != nullptr;
}

bool ASkylandersEnemyGod::WouldChaseIntoEnemyTower(AActor* Target) const
{
	if (!Target) return false;
	return IsPositionUnderEnemyTower(Target->GetActorLocation());
}

// ========== ITEM PURCHASING ==========

const TArray<int32>& ASkylandersEnemyGod::GetItemPurchasePriority(EGodBuildArchetype Archetype)
{
	// Each list is ascending in cost (IDs per SkylandersItemCatalog.cpp); the final
	// six fall out of TryPurchaseItems' "most expensive affordable" rule.
	// Bruiser: health + power.   500, 650, 950, 1200, 1250, 1900, 2000, 2200, 2300
	// Short Sword, Healing Elixir, Vampire Fangs, Guardian Plate, Giant's Girdle,
	// Golden Gatling, Aegis of Life, Bloodthirster, Titan's Bulwark
	static TArray<int32> Bruiser  = { 1, 26, 4, 11, 28, 6, 14, 8, 13 };
	// Assassin: power + pen + crit.   450, 500, 650, 900, 1300, 1900, 2200, 2400, 2500
	// Chompy Fang, Short Sword, Ghost Pirate Cutlass, Berserker Gauntlets, Arkeyan Crossbow,
	// Golden Gatling, Bloodthirster, Executioner, Traptanium Shard
	static TArray<int32> Assassin = { 21, 1, 22, 3, 23, 6, 8, 7, 24 };
	// Tank: prot + health + CDR.   450, 600, 600, 1050, 1200, 1300, 1700, 2200, 2300, 2300
	// Mabu Hide Cloak, Skystone Shield, Time Twist Hourglass, Sky-Iron Shield, Guardian Plate,
	// Lifespring Pendant, Portal Master's Armor, Core of Light Bulwark, Titan's Bulwark, Sky-Chi Sigil
	static TArray<int32> Tank     = { 25, 9, 31, 27, 11, 29, 12, 30, 13, 36 };

	switch (Archetype)
	{
	case EGodBuildArchetype::Assassin: return Assassin;
	case EGodBuildArchetype::Tank:     return Tank;
	default:                           return Bruiser;
	}
}

void ASkylandersEnemyGod::TryPurchaseItems()
{
	// Only buy items when near base (within 500 units of spawn)
	float DistToBase = FVector::Dist(GetActorLocation(), BasePosition);
	if (DistToBase > 500.0f) return;

	int32 EmptySlot = -1;
	for (int32 i = 0; i < MaxInventorySlots; i++)
	{
		if (Inventory[i] == 0) { EmptySlot = i; break; }
	}

	// Full inventory: the cheapest owned item (a starter, by list order) may be
	// sold to make room, otherwise late builds stall at six early items and the
	// capstones at the end of every list are never reached (ITEMS-DESIGN.md §3.2).
	int32 SellSlot = -1;
	int32 SellRefund = 0;
	int32 SellCost = 0;
	if (EmptySlot < 0)
	{
		for (int32 i = 0; i < MaxInventorySlots; i++)
		{
			const FSkylandersItemData* Owned = USkylandersItemCatalog::FindItem(Inventory[i]);
			if (Owned && (SellSlot < 0 || Owned->Cost < SellCost))
			{
				SellSlot = i;
				SellCost = Owned->Cost;
				SellRefund = Owned->GetSellPrice();
			}
		}
		if (SellSlot < 0) return;
	}
	// Gold the purchase can draw on: with a sale pending, the refund counts
	const int32 Budget = Gold + (EmptySlot < 0 ? SellRefund : 0);

	const TArray<int32>& Priority = GetItemPurchasePriority(BuildArchetype);

	// Pick the most expensive affordable entry we don't already own. "First
	// affordable" made a god returning with 1500 gold buy the cheapest starter
	// over and over, so builds ended as doubled starters.
	const FSkylandersItemData* Best = nullptr;
	for (int32 ItemID : Priority)
	{
		const FSkylandersItemData* ItemData = USkylandersItemCatalog::FindItem(ItemID);
		if (!ItemData) continue;
		if (Budget < ItemData->Cost) continue;
		// A swap must be an upgrade: the inventory's total cost strictly rises with
		// every sale, so a full god can never sell/buy in a loop.
		if (EmptySlot < 0 && ItemData->Cost <= SellCost) continue;

		// One copy per occurrence in the list (an ID listed twice may be bought twice)
		int32 AllowedCopies = 0;
		for (int32 Id : Priority)
		{
			if (Id == ItemID) AllowedCopies++;
		}
		int32 OwnedCountOfThis = 0;
		for (int32 i = 0; i < MaxInventorySlots; i++)
		{
			if (Inventory[i] == ItemID) OwnedCountOfThis++;
		}
		if (OwnedCountOfThis >= AllowedCopies) continue;

		if (!Best || ItemData->Cost > Best->Cost)
		{
			Best = ItemData;
		}
	}
	if (!Best) return;

	if (EmptySlot < 0)
	{
		const FSkylandersItemData* Sold = USkylandersItemCatalog::FindItem(Inventory[SellSlot]);
		Gold += SellRefund;
		Inventory[SellSlot] = 0;
		EmptySlot = SellSlot;
		SKY_COUNT("god_item_sold");
		UE_LOG(LogTemp, Warning, TEXT("Enemy God '%s' sold '%s' for %d gold to make room for '%s'"),
			*GodName, Sold ? *Sold->ItemName : TEXT("?"), SellRefund, *Best->ItemName);
	}

	// Purchase (one item per tick to simulate realistic shopping)
	Gold -= Best->Cost;
	Inventory[EmptySlot] = Best->ItemID;
	RecalculateItemBonuses();
	SKY_COUNT("item_bought");
	SKY_COUNT("god_item_bought");

	UE_LOG(LogTemp, Warning, TEXT("Enemy God '%s' bought '%s' (ID %d) for %d gold (remaining: %d). Slot %d"),
		*GodName, *Best->ItemName, Best->ItemID, Best->Cost, Gold, EmptySlot);
}

void ASkylandersEnemyGod::RecalculateItemBonuses()
{
	FSkylandersItemStats NewStats;

	for (int32 i = 0; i < MaxInventorySlots; i++)
	{
		if (Inventory[i] == 0) continue;
		const FSkylandersItemData* ItemData = USkylandersItemCatalog::FindItem(Inventory[i]);
		if (ItemData)
		{
			NewStats = NewStats + ItemData->Stats;
		}
	}

	ItemBonusStats = NewStats;

	// Apply movement speed from items
	GetCharacterMovement()->MaxWalkSpeed = GetWalkSpeed();

	UE_LOG(LogTemp, Log, TEXT("Enemy God '%s' item bonuses: +%.0f Power, +%.0f HP, +%.0f Protections, +%.0f Speed, +%.0f%% Crit, +%.0f%% Lifesteal, +%.0f%% CDR, +%.0f Pen"),
		*GodName, ItemBonusStats.Power, ItemBonusStats.MaxHealth, ItemBonusStats.Protections,
		ItemBonusStats.MovementSpeed, ItemBonusStats.CritChance * 100.0f, ItemBonusStats.Lifesteal * 100.0f,
		ItemBonusStats.CooldownReduction * 100.0f, ItemBonusStats.Penetration);
}

// ========== AI STATE MACHINE ==========

ASkylandersCharacter* ASkylandersEnemyGod::FindPlayer()
{
	if (!CachedPlayer)
	{
		APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		CachedPlayer = Cast<ASkylandersCharacter>(PlayerPawn);
	}
	return CachedPlayer;
}

ASkylandersMinion* ASkylandersEnemyGod::FindNearestEnemyMinion(float SearchRadius)
{
	// Find the nearest opposing-team minion to attack
	TArray<AActor*> AllMinions;
	USkylandersActorRegistry::GetAllActorsOfClass(GetWorld(), ASkylandersMinion::StaticClass(), AllMinions);

	ASkylandersMinion* Nearest = nullptr;
	float NearestDist = (SearchRadius > 0.0f) ? SearchRadius : AttackRange;

	for (AActor* Actor : AllMinions)
	{
		ASkylandersMinion* Minion = Cast<ASkylandersMinion>(Actor);
		if (Minion && Minion->Team == FoeTeam() && !Minion->bDead)
		{
			float Dist = FVector::Dist(GetActorLocation(), Minion->GetActorLocation());
			if (Dist < NearestDist)
			{
				NearestDist = Dist;
				Nearest = Minion;
			}
		}
	}
	return Nearest;
}

ASkylandersMinion* ASkylandersEnemyGod::FindWeakestEnemyMinion(float Radius)
{
	TArray<AActor*> AllMinions;
	USkylandersActorRegistry::GetAllActorsOfClass(GetWorld(), ASkylandersMinion::StaticClass(), AllMinions);

	ASkylandersMinion* Weakest = nullptr;
	float WeakestHp = TNumericLimits<float>::Max();
	for (AActor* Actor : AllMinions)
	{
		ASkylandersMinion* Minion = Cast<ASkylandersMinion>(Actor);
		if (Minion && Minion->Team == FoeTeam() && !Minion->bDead
			&& FVector::Dist(GetActorLocation(), Minion->GetActorLocation()) <= Radius
			&& Minion->CurrentHealth < WeakestHp)
		{
			WeakestHp = Minion->CurrentHealth;
			Weakest = Minion;
		}
	}
	return Weakest;
}

int32 ASkylandersEnemyGod::CountOwnMinionsNear(const FVector& Pos, float Radius) const
{
	TArray<AActor*> AllMinions;
	USkylandersActorRegistry::GetAllActorsOfClass(GetWorld(), ASkylandersMinion::StaticClass(), AllMinions);

	int32 Count = 0;
	for (AActor* Actor : AllMinions)
	{
		ASkylandersMinion* Minion = Cast<ASkylandersMinion>(Actor);
		if (Minion && Minion->Team == Team && !Minion->bDead && FVector::Dist(Pos, Minion->GetActorLocation()) < Radius)
		{
			Count++;
		}
	}
	return Count;
}

bool ASkylandersEnemyGod::IsHeroAlive(AActor* Hero)
{
	if (ASkylandersCharacter* Player = Cast<ASkylandersCharacter>(Hero))
	{
		return Player->CurrentHealth > 0.0f;
	}
	if (ASkylandersEnemyGod* God = Cast<ASkylandersEnemyGod>(Hero))
	{
		return God->CurrentState != EGodAIState::Dead;
	}
	return false;
}

float ASkylandersEnemyGod::GetHeroHealthPct(AActor* Hero)
{
	if (ASkylandersCharacter* Player = Cast<ASkylandersCharacter>(Hero))
	{
		return (Player->MaxHealth > 0.0f) ? Player->CurrentHealth / Player->MaxHealth : 0.0f;
	}
	if (ASkylandersEnemyGod* God = Cast<ASkylandersEnemyGod>(Hero))
	{
		float EffMax = God->MaxHealth + God->ItemBonusStats.MaxHealth;
		return (EffMax > 0.0f) ? God->CurrentHealth / EffMax : 0.0f;
	}
	return 0.0f;
}

// Every living opposing hero: for an enemy god that's the player plus any
// friendly ally gods; for a friendly ally that's the enemy gods.
void ASkylandersEnemyGod::GatherEnemyHeroes(TArray<AActor*>& OutHeroes)
{
	OutHeroes.Reset();
	if (FoeTeam() == ETowerTeam::Friendly)
	{
		ASkylandersCharacter* Player = FindPlayer();
		if (IsHeroAlive(Player))
		{
			OutHeroes.Add(Player);
		}
	}

	TArray<AActor*> AllGods;
	USkylandersActorRegistry::GetAllActorsOfClass(GetWorld(), ASkylandersEnemyGod::StaticClass(), AllGods);
	for (AActor* Actor : AllGods)
	{
		ASkylandersEnemyGod* God = Cast<ASkylandersEnemyGod>(Actor);
		if (God && God != this && God->Team == FoeTeam() && God->CurrentState != EGodAIState::Dead)
		{
			OutHeroes.Add(God);
		}
	}
}

// Nearest living opposing hero (see GatherEnemyHeroes for who counts).
AActor* ASkylandersEnemyGod::FindEnemyHero()
{
	TArray<AActor*> Heroes;
	GatherEnemyHeroes(Heroes);

	AActor* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (AActor* H : Heroes)
	{
		const float Dist = FVector::Dist(GetActorLocation(), H->GetActorLocation());
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = H;
		}
	}
	return Best;
}

void ASkylandersEnemyGod::ApplyGodModel()
{
	if (GodModel.IsEmpty() || !GetMesh()) return; // keep the placeholder cylinder

	// Per-character model table. Scale normalises each rip to roughly the god
	// capsule height; MeshZ = -50 - (meshBottomZ * Scale) puts the model's feet on
	// the capsule bottom (pivots sit at the model centre, and differ per rip).
	struct FGodModelDef
	{
		const TCHAR* Key;
		const TCHAR* Folder;
		const TCHAR* Idle;
		const TCHAR* Run;
		float Scale;
		float MeshZ;
	};
	static const FGodModelDef Models[] =
	{
		// Key           Folder         Idle                        Run           Scale  MeshZ
		{ TEXT("Spyro"),      TEXT("Spyro"),      TEXT("drive_modswap_pose01"), TEXT("drive_run"), 0.52f,   5.2f }, // no drive_idle in the rip
		{ TEXT("StealthElf"), TEXT("StealthElf"), TEXT("drive_idle"),           TEXT("drive_run"), 1.20f,   3.5f },
		{ TEXT("KaosSensei"), TEXT("KaosSensei"), TEXT("drive_idle"),           TEXT("drive_run"), 0.78f,   4.9f },
		{ TEXT("ChompyMage"), TEXT("ChompyMage"), TEXT("drive_idle"),           TEXT("drive_run"), 0.53f,  -6.6f },
		{ TEXT("Cynder"),     TEXT("Cynder"),     TEXT("drive_idle"),           TEXT("drive_run"), 0.53f,   5.1f },
		{ TEXT("Hex"),        TEXT("Hex"),        TEXT("drive_idle"),           TEXT("drive_run"), 1.00f,  -2.7f },
		{ TEXT("TreeRex"),    TEXT("TreeRex"),    TEXT("sequoiastampede_outwall"), TEXT("photosynthesiscannon_hold"), 0.55f, 78.8f },
	};

	const FGodModelDef* Def = nullptr;
	for (const FGodModelDef& Candidate : Models)
	{
		if (GodModel.Equals(Candidate.Key, ESearchCase::IgnoreCase))
		{
			Def = &Candidate;
			break;
		}
	}
	if (!Def)
	{
		UE_LOG(LogTemp, Warning, TEXT("God '%s': unknown GodModel '%s', keeping placeholder"), *GodName, *GodModel);
		return;
	}

	const FString MeshPath = FString::Printf(TEXT("/Game/Characters/%s/Models/%s"), Def->Folder, Def->Folder);
	const FString IdlePath = FString::Printf(TEXT("/Game/Characters/%s/Animations/%s"), Def->Folder, Def->Idle);
	const FString RunPath  = FString::Printf(TEXT("/Game/Characters/%s/Animations/%s"), Def->Folder, Def->Run);
	const float MeshZ = Def->MeshZ;
	const float Scale = Def->Scale;

	USkeletalMesh* SkelMesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
	if (!SkelMesh) return; // asset missing — leave the cylinder

	USkeletalMeshComponent* MeshComp = GetMesh();
	MeshComp->SetSkeletalMesh(SkelMesh);
	MeshComp->SetRelativeLocation(FVector(0.0f, 0.0f, MeshZ));
	MeshComp->SetRelativeScale3D(FVector(Scale));
	MeshComp->SetAnimInstanceClass(USkylandersSimpleAnimInstance::StaticClass());
	if (USkylandersSimpleAnimInstance* Simple = Cast<USkylandersSimpleAnimInstance>(MeshComp->GetAnimInstance()))
	{
		Simple->IdleAnim = LoadObject<UAnimSequenceBase>(nullptr, *IdlePath);
		Simple->RunAnim  = LoadObject<UAnimSequenceBase>(nullptr, *RunPath);
	}

	// Real body replaces the placeholder cylinder
	if (BodyMesh) BodyMesh->SetVisibility(false);
}

void ASkylandersEnemyGod::MoveToPoint(const FVector& Dest, float AcceptRadius, float FallbackScale)
{
	AAIController* AI = Cast<AAIController>(GetController());
	if (AI)
	{
		// Re-issue only when the goal moved a lot or we've stopped, so the path
		// isn't torn down every tick.
		const bool bGoalChanged = FVector::Dist2D(Dest, LastMoveGoal) > 200.0f;
		const bool bIdle = AI->GetMoveStatus() != EPathFollowingStatus::Moving;
		if (bGoalChanged || bIdle)
		{
			LastMoveGoal = Dest;
			EPathFollowingRequestResult::Type Result = AI->MoveToLocation(
				Dest, AcceptRadius, /*bStopOnOverlap*/ false, /*bUsePathfinding*/ true,
				/*bProjectDestinationToNavigation*/ true, /*bCanStrafe*/ false, nullptr, /*bAllowPartialPath*/ true);
			if (Result != EPathFollowingRequestResult::Failed)
			{
				return; // pathing (or already at goal)
			}
		}
		else
		{
			return; // already moving toward this goal
		}
	}

	// Fallback: no controller or no navmesh path — steer straight (old behavior).
	FVector Dir = (Dest - GetActorLocation()).GetSafeNormal2D();
	if (!Dir.IsNearlyZero())
	{
		AddMovementInput(Dir, FallbackScale);
	}
}

void ASkylandersEnemyGod::StopMoving()
{
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
	}
	LastMoveGoal = FVector(FLT_MAX); // force a fresh MoveTo next time
}

void ASkylandersEnemyGod::FaceTarget(AActor* Target)
{
	if (!Target) return;

	FVector Direction = Target->GetActorLocation() - GetActorLocation();
	Direction.Z = 0.0f;
	if (Direction.SizeSquared() > 1.0f)
	{
		FRotator TargetRot = Direction.Rotation();
		FRotator NewRot = FMath::RInterpTo(GetActorRotation(), TargetRot, GetWorld()->GetDeltaSeconds(), 10.0f);
		SetActorRotation(NewRot);
	}
}

void ASkylandersEnemyGod::SetState(EGodAIState NewState)
{
	// Single choke point for state changes so thrash is measurable: the smoke test
	// sums god_state_change, and the per-god count is logged at match end.
	if (CurrentState == NewState) return;
	CurrentState = NewState;
	StateChangeCount++;
	SKY_COUNT("god_state_change");
}

AActor* ASkylandersEnemyGod::SelectFightTarget(float Now)
{
	// A target is usable while it lives, stays inside our leash and is not sitting
	// under one of its own towers (chasing it there is a dive).
	auto IsUsable = [&](AActor* H)
	{
		return IsHeroAlive(H) && !WouldChaseIntoEnemyTower(H)
			&& FVector::Dist(GetActorLocation(), H->GetActorLocation()) <= FightLeash;
	};

	AActor* Current = FightTarget.Get();
	const bool bCurrentUsable = Current && IsUsable(Current);

	// Commit to a fresh target for a moment: re-picking every tick made gods
	// flicker between two foes and never finish either.
	if (bCurrentUsable && Now - FightTargetSince < MinTargetCommitSeconds)
	{
		return Current;
	}

	// SMITE AI "focus the squishy": among heroes we can already hit, the lowest
	// health fraction; if none is in range, the nearest one inside the leash.
	TArray<AActor*> Heroes;
	GatherEnemyHeroes(Heroes);
	AActor* Best = nullptr;
	bool bBestInRange = false;
	float BestPct = 2.0f;
	float BestDist = TNumericLimits<float>::Max();
	for (AActor* H : Heroes)
	{
		if (!IsUsable(H)) continue;
		const float Dist = FVector::Dist(GetActorLocation(), H->GetActorLocation());
		const bool bInRange = Dist <= AttackRange;
		const float Pct = GetHeroHealthPct(H);
		bool bBetter = false;
		if (!Best || (bInRange && !bBestInRange))
		{
			bBetter = true;
		}
		else if (bInRange == bBestInRange)
		{
			bBetter = bInRange ? (Pct < BestPct) : (Dist < BestDist);
		}
		if (bBetter)
		{
			Best = H;
			bBestInRange = bInRange;
			BestPct = Pct;
			BestDist = Dist;
		}
	}

	if (Best != Current)
	{
		FightTarget = Best;
		FightTargetSince = Now;
	}
	return Best;
}

void ASkylandersEnemyGod::BeginFighting(AActor* Target, float Leash)
{
	FightTarget = Target;
	FightTargetSince = GetWorld()->GetTimeSeconds();
	FightLeash = Leash;
	JungleTarget = nullptr;
	SetState(EGodAIState::Fighting);
}

bool ASkylandersEnemyGod::TryAllyAssist(float Now, float HealthPct)
{
	if (Team != ETowerTeam::Friendly || HealthPct < 0.4f) return false;

	ASkylandersCharacter* Player = FindPlayer();
	if (!IsHeroAlive(Player)) return false;
	if (FVector::Dist(GetActorLocation(), Player->GetActorLocation()) > 2500.0f) return false;

	const float FightWindow = 4.0f;

	// Took hero damage recently (the player's own assist bookkeeping)...
	bool bPlayerFighting = false;
	for (const TPair<TWeakObjectPtr<AActor>, float>& Hit : Player->RecentHeroDamagers)
	{
		if (Hit.Key.IsValid() && Now - Hit.Value <= FightWindow)
		{
			bPlayerFighting = true;
			break;
		}
	}

	// ...or dealt it: every enemy god records the player when it gets hit.
	TArray<AActor*> AllGods;
	USkylandersActorRegistry::GetAllActorsOfClass(GetWorld(), ASkylandersEnemyGod::StaticClass(), AllGods);
	if (!bPlayerFighting)
	{
		for (AActor* Actor : AllGods)
		{
			ASkylandersEnemyGod* God = Cast<ASkylandersEnemyGod>(Actor);
			if (!God || God->Team != FoeTeam() || God->CurrentState == EGodAIState::Dead) continue;
			const float* HitTime = God->RecentHeroDamagers.Find(TWeakObjectPtr<AActor>(Player));
			if (HitTime && Now - *HitTime <= FightWindow)
			{
				bPlayerFighting = true;
				break;
			}
		}
	}
	if (!bPlayerFighting) return false;

	// Whoever is hitting the player (if that is a live enemy god and the hit is
	// recent), else the enemy god nearest the player.
	AActor* Target = nullptr;
	if (ASkylandersEnemyGod* Attacker = Cast<ASkylandersEnemyGod>(Player->LastDamageCauser.Get()))
	{
		const float* HitTime = Player->RecentHeroDamagers.Find(TWeakObjectPtr<AActor>(Attacker));
		if (Attacker->Team == FoeTeam() && Attacker->CurrentState != EGodAIState::Dead
			&& HitTime && Now - *HitTime <= FightWindow)
		{
			Target = Attacker;
		}
	}
	if (!Target)
	{
		float BestDist = 1500.0f;
		for (AActor* Actor : AllGods)
		{
			ASkylandersEnemyGod* God = Cast<ASkylandersEnemyGod>(Actor);
			if (!God || God->Team != FoeTeam() || God->CurrentState == EGodAIState::Dead) continue;
			const float Dist = FVector::Dist(Player->GetActorLocation(), God->GetActorLocation());
			if (Dist < BestDist)
			{
				BestDist = Dist;
				Target = God;
			}
		}
	}

	// Allies still never dive towers for the player
	if (!Target || WouldChaseIntoEnemyTower(Target)) return false;

	// The fight can be up to ~4000 away (2500 to the player + 1500 to the foe), so
	// widen the leash or Fighting would drop it on the very next tick.
	BeginFighting(Target, 4000.0f);
	SKY_COUNT("ally_assist_engage");
	UE_LOG(LogTemp, Log, TEXT("Ally god '%s' joins the player's fight against '%s'"), *GodName, *Cast<ASkylandersEnemyGod>(Target)->GodName);
	return true;
}

void ASkylandersEnemyGod::EnterTowerBackoff(ASkylandersTower* Tower)
{
	const FVector Loc = GetActorLocation();
	const FVector TowerLoc = Tower->GetActorLocation();
	BackoffTower = Tower;
	BackoffStartTime = GetWorld()->GetTimeSeconds();
	// Straight back down the lane toward our own base, to just outside the tower's
	// range plus a margin, so a small shove or a minion body can't nudge us back in.
	BackoffHoldPoint = FVector(TowerLoc.X - LaneSign() * (Tower->AttackRange + TowerBackoffMargin), Loc.Y, Loc.Z);
	FightTarget = nullptr;
	JungleTarget = nullptr;
	SetState(EGodAIState::TowerBackoff);
	SKY_COUNT("god_tower_backoff");
	UE_LOG(LogTemp, Verbose, TEXT("God '%s' backing off '%s' to X=%.0f"), *GodName, *Tower->TowerName, BackoffHoldPoint.X);
}

ASkylandersBuffCamp* ASkylandersEnemyGod::FindJungleCamp(float MaxDist) const
{
	TArray<AActor*> Camps;
	USkylandersActorRegistry::GetAllActorsOfClass(GetWorld(), ASkylandersBuffCamp::StaticClass(), Camps);
	TArray<AActor*> Gods;
	USkylandersActorRegistry::GetAllActorsOfClass(GetWorld(), ASkylandersEnemyGod::StaticClass(), Gods);

	ASkylandersBuffCamp* Best = nullptr;
	float BestDist = MaxDist;
	for (AActor* Actor : Camps)
	{
		ASkylandersBuffCamp* Camp = Cast<ASkylandersBuffCamp>(Actor);
		if (!Camp || Camp->bDead || Camp->BuffType == EBuffType::None) continue;
		// The boss camp (Golden Queen, 2500 HP) is a team objective, not a solo clear
		if (Camp->MaxHealth > 1000.0f) continue;
		if (HasCampBuff(Camp->BuffType)) continue;
		if (IsPositionUnderEnemyTower(Camp->HomeLocation)) continue;

		// One god per team per camp, so the whole team doesn't leave lane for it
		bool bTaken = false;
		for (AActor* GodActor : Gods)
		{
			const ASkylandersEnemyGod* Mate = Cast<ASkylandersEnemyGod>(GodActor);
			if (Mate && Mate != this && Mate->Team == Team && Mate->CurrentState == EGodAIState::Jungling
				&& Mate->JungleTarget.Get() == Camp)
			{
				bTaken = true;
				break;
			}
		}
		if (bTaken) continue;

		const float Dist = FVector::Dist2D(GetActorLocation(), Camp->HomeLocation);
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = Camp;
		}
	}
	return Best;
}

bool ASkylandersEnemyGod::TryStartJungling(float MaxDist)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextJungleTime) return false;

	ASkylandersBuffCamp* Camp = FindJungleCamp(MaxDist);
	if (!Camp) return false;

	JungleTarget = Camp;
	JungleStartTime = Now;
	JungleProgressTime = Now;
	JungleBestDist = FVector::Dist(GetActorLocation(), Camp->GetActorLocation());
	bJungleSteerDirect = false;
	FightTarget = nullptr;
	SetState(EGodAIState::Jungling);
	SKY_COUNT("god_jungle_start");
	UE_LOG(LogTemp, Log, TEXT("God '%s' goes to clear '%s'"), *GodName, *Camp->CampName);
	return true;
}

void ASkylandersEnemyGod::UpdateAI(float DeltaTime)
{
	const float Now = GetWorld()->GetTimeSeconds();

	// The nearest opposing hero (player for an enemy god; an enemy god for a
	// friendly ally). FindEnemyHero only returns living heroes. Fighting picks its
	// own target via SelectFightTarget; this one drives engage/cornered checks.
	AActor* Hero = FindEnemyHero();
	float DistToHero = Hero ? FVector::Dist(GetActorLocation(), Hero->GetActorLocation()) : 99999.0f;
	bool bHeroAlive = Hero != nullptr;
	float EffMaxHealth = MaxHealth + ItemBonusStats.MaxHealth;
	float HealthPct = (EffMaxHealth > 0.0f) ? (CurrentHealth / EffMaxHealth) : 0.0f;

	// Low-HP retreat threshold (gets braver as it levels up, min 15%)
	const float RetreatThreshold = FMath::Max(0.15f, 0.30f - Level * 0.01f);

	// === Item purchasing (any state, checks distance to base internally) ===
	TryPurchaseItems();

	// Respect enemy towers: if caught inside one without a real minion wave for
	// cover, back off instead of feeding. Towers shoot gods, so diving solo is a
	// death sentence. This is a tower back-off, not the low-health Retreating:
	// it holds just outside range, so the two can't ping-pong (the old version
	// flipped to Retreating, instantly "recovered" at >= 60% HP, stepped back in,
	// and repeated many times per second).
	if (CurrentState == EGodAIState::Laning || CurrentState == EGodAIState::Fighting || CurrentState == EGodAIState::Jungling)
	{
		if (ASkylandersTower* Tower = FindEnemyTowerCovering(GetActorLocation()))
		{
			const int32 Cover = CountOwnMinionsNear(GetActorLocation(), 900.0f);
			if (Cover < TowerCoverMinions)
			{
				EnterTowerBackoff(Tower);
				return;
			}
		}
	}

	switch (CurrentState)
	{
	case EGodAIState::Laning:
	{
		// Low health: walk home to heal before minion/tower chip damage finishes us
		// (Laning used to have no exit for this and gods bled out in lane).
		if (HealthPct < FMath::Max(0.25f, RetreatThreshold))
		{
			SetState(EGodAIState::Retreating);
			UE_LOG(LogTemp, Log, TEXT("God '%s' low in lane (%.0f%%), heading home"), *GodName, HealthPct * 100.0f);
			break;
		}

		// Nearest opposing minion to farm. Search out to AggroRange (wider than
		// AttackRange) so the god actually walks up to the wave — searching only
		// AttackRange made the approach branch below unreachable, and the god
		// marched past live minions straight at the structures.
		ASkylandersMinion* TargetMinion = FindNearestEnemyMinion(FMath::Max(AggroRange, AttackRange));

		// Frontmost own-team minion along the push direction, for cover checks. A
		// higher (X * LaneSign) is more advanced toward the foe base.
		TArray<AActor*> AllMinions;
		USkylandersActorRegistry::GetAllActorsOfClass(GetWorld(), ASkylandersMinion::StaticClass(), AllMinions);
		float OwnFrontX = GetActorLocation().X;
		bool bHasOwnMinions = false;
		int32 OwnMinionsNearby = 0;
		for (AActor* Actor : AllMinions)
		{
			ASkylandersMinion* Minion = Cast<ASkylandersMinion>(Actor);
			if (Minion && Minion->Team == Team && !Minion->bDead)
			{
				bHasOwnMinions = true;
				if (Minion->GetActorLocation().X * LaneSign() > OwnFrontX * LaneSign())
				{
					OwnFrontX = Minion->GetActorLocation().X;
				}
				if (FVector::Dist(GetActorLocation(), Minion->GetActorLocation()) < 1500.0f)
				{
					OwnMinionsNearby++;
				}
			}
		}

		// Clamp a move target so we don't push more than ~200 past our front minion.
		auto StayBehindWave = [&](FVector TargetPos) -> FVector
		{
			if (bHasOwnMinions && (TargetPos.X - OwnFrontX) * LaneSign() > 200.0f)
			{
				TargetPos.X = OwnFrontX - 200.0f * LaneSign();
			}
			return TargetPos;
		};

		if (TargetMinion)
		{
			float DistToMinion = FVector::Dist(GetActorLocation(), TargetMinion->GetActorLocation());
			if (DistToMinion <= AttackRange)
			{
				// In range - attack
				FaceTarget(TargetMinion);
				if (AttackTimer <= 0.0f)
				{
					PerformAttack(TargetMinion);
					AttackTimer = AttackCooldown;
				}
			}
			else
			{
				FVector TargetPos = StayBehindWave(TargetMinion->GetActorLocation());
				if (!IsPositionUnderEnemyTower(TargetPos))
				{
					MoveToPoint(TargetPos, AttackRange * 0.85f, 1.0f);
				}
				else
				{
					// Not calling MoveToPoint isn't enough: the AIController keeps
					// following the previous path, which is how gods kept walking
					// into tower range. Halt it.
					StopMoving();
				}
			}
		}
		else
		{
			// No opposing minions in range - look for structures to push or advance

			// === Structure targeting: nearest vulnerable FoeTeam tower/phoenix/titan ===
			AActor* TargetStructure = nullptr;
			float NearestStructureDist = 99999.0f;

			TArray<AActor*> AllTowers;
			USkylandersActorRegistry::GetAllActorsOfClass(GetWorld(), ASkylandersTower::StaticClass(), AllTowers);
			for (AActor* TowerActor : AllTowers)
			{
				ASkylandersTower* Tower = Cast<ASkylandersTower>(TowerActor);
				if (Tower && Tower->Team == FoeTeam() && !Tower->bDestroyed && Tower->IsVulnerable())
				{
					float Dist = FVector::Dist(GetActorLocation(), Tower->GetActorLocation());
					if (Dist < NearestStructureDist)
					{
						NearestStructureDist = Dist;
						TargetStructure = Tower;
					}
				}
			}

			TArray<AActor*> AllTitans;
			USkylandersActorRegistry::GetAllActorsOfClass(GetWorld(), ASkylandersTitan::StaticClass(), AllTitans);
			for (AActor* TitanActor : AllTitans)
			{
				ASkylandersTitan* Titan = Cast<ASkylandersTitan>(TitanActor);
				if (Titan && Titan->Team == FoeTeam() && !Titan->bDead && Titan->IsVulnerable())
				{
					float Dist = FVector::Dist(GetActorLocation(), Titan->GetActorLocation());
					if (Dist < NearestStructureDist)
					{
						NearestStructureDist = Dist;
						TargetStructure = Titan;
					}
				}
			}

			// Only push structures with minion cover, so the god doesn't solo-dive.
			bool bHasMinionCover = OwnMinionsNearby >= 2;

			if (TargetStructure && bHasMinionCover && NearestStructureDist < AggroRange)
			{
				if (NearestStructureDist <= AttackRange)
				{
					FaceTarget(TargetStructure);
					StopMoving();
					if (AttackTimer <= 0.0f)
					{
						PerformAttack(TargetStructure);
						AttackTimer = AttackCooldown;
					}
				}
				else
				{
					FVector TargetPos = StayBehindWave(TargetStructure->GetActorLocation());
					if (!IsPositionUnderEnemyTower(TargetPos))
					{
						MoveToPoint(TargetPos, AttackRange * 0.85f, 0.8f);
					}
					else
					{
						StopMoving();
					}
				}
			}
			else
			{
				// No valid structure target or no cover - advance slowly down the lane
				FVector LaneDir = FVector(LaneSign(), 0.0f, 0.0f);
				FVector ProjectedPos = GetActorLocation() + LaneDir * 100.0f;
				bool bTowerBlock = IsPositionUnderEnemyTower(ProjectedPos);

				// Advance only if behind our own front minion (or no minions yet)
				if (!bTowerBlock && (!bHasOwnMinions || (GetActorLocation().X - OwnFrontX) * LaneSign() < -200.0f))
				{
					// Path toward a point well down the lane so the navmesh routes
					// around our own base structures instead of bumping into them.
					MoveToPoint(GetActorLocation() + LaneDir * 2500.0f, 150.0f, 0.5f);
				}
				else
				{
					// The goal above is 2500 ahead: without an explicit stop the
					// path carried the god past this check straight into tower range.
					StopMoving();
				}
			}
		}

		// Check if a hero is close and we have enough HP to fight
		if (bHeroAlive && DistToHero <= AggroRange && HealthPct > 0.5f && Now >= NextEngageTime)
		{
			// Tower awareness: don't engage a hero under their tower
			if (!WouldChaseIntoEnemyTower(Hero))
			{
				float FoeHealthPct = GetHeroHealthPct(Hero);
				bool bFoeVulnerable = FoeHealthPct < 0.5f;

				if (DistToHero <= AggroRange * 0.7f || bFoeVulnerable)
				{
					BeginFighting(Hero, FMath::Max(1500.0f, AggroRange));
					UE_LOG(LogTemp, Verbose, TEXT("God '%s' engages a hero! Distance: %.0f, FoeHP: %.0f%%"),
						*GodName, DistToHero, FoeHealthPct * 100.0f);
					break;
				}
			}
		}

		// Blue allies group up with the player when the player is in a fight
		if (TryAllyAssist(Now, HealthPct))
		{
			break;
		}

		// Objective play: a nearby buff camp we don't hold the buff of
		if (HealthPct >= 0.6f && TryStartJungling(2500.0f))
		{
			break;
		}

		break;
	}

	case EGodAIState::Fighting:
	{
		// Retreat if low HP
		if (HealthPct < RetreatThreshold)
		{
			SetState(EGodAIState::Retreating);
			UE_LOG(LogTemp, Warning, TEXT("God '%s' retreating! HP: %.0f/%.0f (%.0f%%)"),
				*GodName, CurrentHealth, EffMaxHealth, HealthPct * 100.0f);
			break;
		}

		// Lowest-health hero in range (committed for MinTargetCommitSeconds). It
		// never returns a hero under an enemy tower, so "no target" also covers
		// the old "hero is under tower, disengage" case; standing under a tower
		// ourselves is caught by the back-off check above.
		AActor* Target = SelectFightTarget(Now);
		if (!Target)
		{
			// Brief re-engage cooldown: a foe stepping in and out of its tower's
			// range would otherwise flip Laning <-> Fighting every few frames.
			NextEngageTime = Now + 1.5f;
			SetState(EGodAIState::Laning);
			UE_LOG(LogTemp, Verbose, TEXT("God '%s' returns to laning - no reachable hero."), *GodName);
			break;
		}
		const float DistToTarget = FVector::Dist(GetActorLocation(), Target->GetActorLocation());

		// Last-hit low-HP minions for gold even during fights
		ASkylandersMinion* NearMinion = FindNearestEnemyMinion();
		if (NearMinion)
		{
			float MinionDist = FVector::Dist(GetActorLocation(), NearMinion->GetActorLocation());
			float MinionHpPct = NearMinion->CurrentHealth / NearMinion->MaxHealth;

			if (MinionDist <= AttackRange && MinionHpPct < 0.2f && AttackTimer <= 0.0f)
			{
				FaceTarget(NearMinion);
				PerformAttack(NearMinion);
				AttackTimer = AttackCooldown;
				break; // Skip the hero attack this tick, focus on the last hit
			}
		}

		// Chase and kite: approach when attack is ready, back off when on cooldown
		FaceTarget(Target);
		if (DistToTarget > AttackRange)
		{
			MoveToPoint(Target->GetActorLocation(), AttackRange * 0.85f, 1.0f);
		}
		else if (DistToTarget < AttackRange * 0.4f && AttackTimer > 0.0f)
		{
			// Kite straight back a short hop (no pathfinding needed for a nudge)
			FVector AwayDir = (GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
			AddMovementInput(AwayDir, 0.6f);
		}
		else
		{
			StopMoving();
		}

		// Auto-attack
		if (DistToTarget <= AttackRange && AttackTimer <= 0.0f)
		{
			PerformAttack(Target);
			AttackTimer = AttackCooldown;
		}

		// Use ability (burst damage every 8 seconds)
		if (AbilityCooldownTimer <= 0.0f && DistToTarget <= AttackRange)
		{
			UseAbility(Target);
			AbilityCooldownTimer = 8.0f * GetCooldownMultiplier();
		}

		break;
	}

	case EGodAIState::TowerBackoff:
	{
		if (HealthPct < RetreatThreshold)
		{
			SetState(EGodAIState::Retreating);
			break;
		}

		ASkylandersTower* Tower = BackoffTower.Get();
		const bool bTowerGone = !Tower || Tower->bDestroyed;
		if (!bTowerGone)
		{
			MoveToPoint(BackoffHoldPoint, 100.0f, 1.0f);
		}

		// Keep farming from the hold point: last-hit the weakest minion in reach,
		// else poke an exposed hero that wandered into range.
		if (AttackTimer <= 0.0f)
		{
			if (ASkylandersMinion* Minion = FindWeakestEnemyMinion(AttackRange))
			{
				FaceTarget(Minion);
				PerformAttack(Minion);
				AttackTimer = AttackCooldown;
			}
			else if (bHeroAlive && DistToHero <= AttackRange && !WouldChaseIntoEnemyTower(Hero))
			{
				FaceTarget(Hero);
				PerformAttack(Hero);
				AttackTimer = AttackCooldown;
			}
		}

		// Hysteresis: hold for a minimum time, then re-enter only with a wave to
		// tank the tower or once the tower is down.
		if (Now - BackoffStartTime >= TowerBackoffMinHold)
		{
			const int32 Cover = CountOwnMinionsNear(GetActorLocation(), 900.0f);
			if (bTowerGone || Cover >= TowerCoverMinions)
			{
				SetState(EGodAIState::Laning);
				UE_LOG(LogTemp, Verbose, TEXT("God '%s' ends tower back-off (cover %d, tower %s)"),
					*GodName, Cover, bTowerGone ? TEXT("down") : TEXT("up"));
				break;
			}

			// Lane is pushed into their tower with no wave to tank it: take a
			// buff camp instead of idling outside the tower.
			if (HealthPct >= 0.6f && TryStartJungling(6000.0f))
			{
				break;
			}
		}
		break;
	}

	case EGodAIState::Jungling:
	{
		ASkylandersBuffCamp* Camp = JungleTarget.Get();
		if (!Camp || Camp->bDead)
		{
			// Cleared (by us or anyone else) or gone
			JungleTarget = nullptr;
			NextJungleTime = Now + 10.0f;
			SetState(EGodAIState::Laning);
			break;
		}
		if (HealthPct < RetreatThreshold)
		{
			JungleTarget = nullptr;
			NextJungleTime = Now + 20.0f;
			SetState(EGodAIState::Retreating);
			break;
		}
		// Give up on a camp we can't reach or finish (blocked path, contested)
		// rather than standing in the jungle forever.
		if (Now - JungleStartTime > 40.0f)
		{
			UE_LOG(LogTemp, Log, TEXT("God '%s' gives up on '%s'"), *GodName, *Camp->CampName);
			JungleTarget = nullptr;
			NextJungleTime = Now + 30.0f;
			SetState(EGodAIState::Laning);
			break;
		}

		// The camp only takes hits from attackers inside its leash, so stand well
		// within both the leash (from its home) and our attack range (from it).
		const float DistToCamp = FVector::Dist(GetActorLocation(), Camp->GetActorLocation());
		const float DistFromHome = FVector::Dist(GetActorLocation(), Camp->HomeLocation);
		if (DistToCamp > AttackRange * 0.9f || DistFromHome > Camp->LeashRange * 0.8f)
		{
			// Progress check: the navmesh path to the center damage room can dead-end
			// (the request "succeeds" with a partial path that ends where we stand,
			// so the god re-requests forever without moving). If 2 s pass without
			// getting 150 closer, steer straight at the camp instead.
			if (Now - JungleProgressTime >= 2.0f)
			{
				if (!bJungleSteerDirect && JungleBestDist - DistToCamp < 150.0f)
				{
					bJungleSteerDirect = true;
					StopMoving();
					SKY_COUNT("god_jungle_steer_direct");
					UE_LOG(LogTemp, Log, TEXT("God '%s' path to '%s' stalled, steering directly"), *GodName, *Camp->CampName);
				}
				JungleBestDist = DistToCamp;
				JungleProgressTime = Now;
			}

			if (bJungleSteerDirect)
			{
				AddMovementInput((Camp->GetActorLocation() - GetActorLocation()).GetSafeNormal2D(), 1.0f);
			}
			else
			{
				MoveToPoint(Camp->GetActorLocation(), FMath::Min(AttackRange, Camp->LeashRange) * 0.5f, 1.0f);
			}
		}
		else
		{
			JungleProgressTime = Now;
			JungleBestDist = DistToCamp;
			StopMoving();
			FaceTarget(Camp);
			if (AttackTimer <= 0.0f)
			{
				PerformAttack(Camp);
				AttackTimer = AttackCooldown;
			}
		}
		break;
	}

	case EGodAIState::Retreating:
	{
		// Move back to base at 1.3x speed (navmesh-pathed around structures)
		GetCharacterMovement()->MaxWalkSpeed = GetWalkSpeed() * 1.3f;
		MoveToPoint(BasePosition, 100.0f, 1.0f);

		// Fight back if cornered (hero within 200 units)
		if (bHeroAlive && DistToHero <= 200.0f)
		{
			FaceTarget(Hero);
			if (AttackTimer <= 0.0f)
			{
				PerformAttack(Hero);
				AttackTimer = AttackCooldown;
			}

			// Use ability defensively when cornered
			if (AbilityCooldownTimer <= 0.0f)
			{
				UseAbility(Hero);
				AbilityCooldownTimer = 8.0f * GetCooldownMultiplier();
			}
		}

		// Low-health retreat ends only once healed (the fountain heals 10%/s), far
		// above the ~30% entry threshold, so it can't flap. Tower back-offs no
		// longer come through here at all.
		if (HealthPct >= 0.90f)
		{
			GetCharacterMovement()->MaxWalkSpeed = GetWalkSpeed();
			SetState(EGodAIState::Laning);
			UE_LOG(LogTemp, Verbose, TEXT("Enemy God '%s' recovered, returning to lane. HP: %.0f/%.0f"),
				*GodName, CurrentHealth, EffMaxHealth);
			break;
		}

		// If arrived at base, use fountain regen (handled in TickPassiveRegen)
		float DistToBase = FVector::Dist(GetActorLocation(), BasePosition);
		if (DistToBase < 100.0f)
		{
			GetCharacterMovement()->MaxWalkSpeed = GetWalkSpeed();
			UpdateHealthBar();
		}

		break;
	}

	case EGodAIState::Dead:
		// Handled by respawn timer
		break;
	}
}

// ========== ATTACK ==========

void ASkylandersEnemyGod::PerformAttack(AActor* Target)
{
	if (!Target) return;

	// Play attack sound
	if (AttackSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, AttackSound, GetActorLocation());
	}

	// Effective damage: base attack + item power bonus
	float EffectiveDamage = AttackDamage + ItemBonusStats.Power;

	// Crit chance from items
	bool bCrit = false;
	if (ItemBonusStats.CritChance > 0.0f)
	{
		float Roll = FMath::FRand();
		if (Roll < ItemBonusStats.CritChance)
		{
			EffectiveDamage *= 2.0f;
			bCrit = true;
		}
	}

	// Attack beam visual - spawned stretched cube (gold for crits, red for normal)
	{
		FVector AttackStart = GetActorLocation();
		FVector AttackEnd = Target->GetActorLocation();
		FVector BeamMid = (AttackStart + AttackEnd) * 0.5f;
		FVector BeamDir = (AttackEnd - AttackStart).GetSafeNormal();
		float BeamLength = FVector::Dist(AttackStart, AttackEnd);
		FRotator BeamRot = BeamDir.Rotation();
		FActorSpawnParameters AtkBeamParams;
		AtkBeamParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* AtkBeamVFX = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), BeamMid, BeamRot, AtkBeamParams);
		if (AtkBeamVFX)
		{
			UStaticMeshComponent* AtkBeamMesh = NewObject<UStaticMeshComponent>(AtkBeamVFX);
			AtkBeamVFX->SetRootComponent(AtkBeamMesh);
			AtkBeamMesh->RegisterComponent();
			UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube"));
			if (CubeMesh) AtkBeamMesh->SetStaticMesh(CubeMesh);
			// Cube 100x100x100 default. X = length, Y/Z = thin beam
			AtkBeamMesh->SetWorldScale3D(FVector(BeamLength / 100.0f, 0.15f, 0.15f));
			AtkBeamMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			AtkBeamMesh->SetCastShadow(false);
			UMaterialInterface* AtkBaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
			if (AtkBaseMat)
			{
				UMaterialInstanceDynamic* AtkDynMat = UMaterialInstanceDynamic::Create(AtkBaseMat, AtkBeamVFX);
				FLinearColor BeamColor = bCrit ? FLinearColor(1.0f, 0.8f, 0.0f) : FLinearColor(1.0f, 0.15f, 0.0f);
				AtkDynMat->SetVectorParameterValue(FName("Color"), BeamColor);
				AtkBeamMesh->SetMaterial(0, AtkDynMat);
			}
			AtkBeamVFX->SetLifeSpan(0.3f);
		}
	}

	// Deal damage based on target type
	ASkylandersCharacter* PlayerTarget = Cast<ASkylandersCharacter>(Target);
	if (PlayerTarget)
	{
		// The player's TakeDamage_Custom applies the protections formula itself —
		// pre-mitigating here would count protections twice. Compute the mitigated
		// value only for lifesteal. (It also spawns the damage number.)
		float TargetProtections = FMath::Max(0.0f, PlayerTarget->GetEffectiveProtections() - ItemBonusStats.Penetration);
		float DamageReduction = TargetProtections / (TargetProtections + 100.0f);
		float MitigatedDamage = EffectiveDamage * (1.0f - DamageReduction);

		PlayerTarget->TakeDamage_Custom(EffectiveDamage, this);

		// Lifesteal from items (based on damage actually dealt)
		if (ItemBonusStats.Lifesteal > 0.0f)
		{
			float HealAmount = MitigatedDamage * ItemBonusStats.Lifesteal;
			float EffMaxHP = MaxHealth + ItemBonusStats.MaxHealth;
			CurrentHealth = FMath::Min(CurrentHealth + HealAmount, EffMaxHP);
		}
		return;
	}

	// Opposing god (ally-vs-enemy god duels). Same protections-are-applied-by-the
	// -target rule as the player branch above.
	ASkylandersEnemyGod* GodTarget = Cast<ASkylandersEnemyGod>(Target);
	if (GodTarget)
	{
		float TargetProtections = FMath::Max(0.0f, GodTarget->GetEffectiveProtections() - ItemBonusStats.Penetration);
		float DamageReduction = TargetProtections / (TargetProtections + 100.0f);
		float MitigatedDamage = EffectiveDamage * (1.0f - DamageReduction);

		GodTarget->TakeDamage_Custom(EffectiveDamage, this);

		if (ItemBonusStats.Lifesteal > 0.0f)
		{
			float HealAmount = MitigatedDamage * ItemBonusStats.Lifesteal;
			float EffMaxHP = MaxHealth + ItemBonusStats.MaxHealth;
			CurrentHealth = FMath::Min(CurrentHealth + HealAmount, EffMaxHP);
		}
		return;
	}

	ASkylandersMinion* MinionTarget = Cast<ASkylandersMinion>(Target);
	if (MinionTarget)
	{
		MinionTarget->TakeDamage_Custom(EffectiveDamage, this);
		return;
	}

	ASkylandersTower* TowerTarget = Cast<ASkylandersTower>(Target);
	if (TowerTarget)
	{
		SKY_COUNT("god_structure_attack");
		TowerTarget->TakeDamage_Custom(EffectiveDamage, this);
		return;
	}

	ASkylandersTitan* TitanTarget = Cast<ASkylandersTitan>(Target);
	if (TitanTarget)
	{
		SKY_COUNT("god_structure_attack");
		SKY_COUNT("god_titan_attack");
		TitanTarget->TakeDamage_Custom(EffectiveDamage, this);
		return;
	}

	// Neutral buff camp (Jungling). The camp credits the killing god itself and
	// calls back into ApplyCampBuff.
	ASkylandersBuffCamp* CampTarget = Cast<ASkylandersBuffCamp>(Target);
	if (CampTarget)
	{
		CampTarget->TakeDamage_Custom(EffectiveDamage, this);
		return;
	}
}

// ========== ABILITY ==========

void ASkylandersEnemyGod::UseAbility(AActor* Target)
{
	// Burst the hero we are actually fighting; the nearest one is only a fallback.
	AActor* Hero = IsHeroAlive(Target) ? Target : FindEnemyHero();
	if (!Hero) return;

	float DistToHero = FVector::Dist(GetActorLocation(), Hero->GetActorLocation());
	if (DistToHero > AttackRange) return;

	SKY_COUNT("god_ability");

	if (USkylandersTelemetrySubsystem* Tele = USkylandersTelemetrySubsystem::Get(this))
	{
		Tele->LogAbility(GodName, Team == ETowerTeam::Enemy ? TEXT("red") : TEXT("blue"), TEXT("burst"), GetActorLocation());
	}

	// Play ability sound
	if (AbilitySound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, AbilitySound, GetActorLocation());
	}

	// Ability damage scales with effective power (base + items)
	float EffPower = GetEffectivePower();
	float BurstDamage = EffPower * (3.0f + Level * 0.5f);

	// Purple ability burst visual - spawned sphere that auto-destroys
	{
		FActorSpawnParameters AbilVFXParams;
		AbilVFXParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* AbilVFX = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), Hero->GetActorLocation(), FRotator::ZeroRotator, AbilVFXParams);
		if (AbilVFX)
		{
			UStaticMeshComponent* AbilMesh = NewObject<UStaticMeshComponent>(AbilVFX);
			AbilVFX->SetRootComponent(AbilMesh);
			AbilMesh->RegisterComponent();
			UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere"));
			if (SphereMesh) AbilMesh->SetStaticMesh(SphereMesh);
			// Sphere default radius = 50, scale to 100 radius = 2.0
			AbilMesh->SetWorldScale3D(FVector(2.0f));
			AbilMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			AbilMesh->SetCastShadow(false);
			UMaterialInterface* AbilBaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
			if (AbilBaseMat)
			{
				UMaterialInstanceDynamic* AbilDynMat = UMaterialInstanceDynamic::Create(AbilBaseMat, AbilVFX);
				AbilDynMat->SetVectorParameterValue(FName("Color"), FLinearColor(0.63f, 0.13f, 0.94f)); // Purple
				AbilMesh->SetMaterial(0, AbilDynMat);
			}
			AbilVFX->SetLifeSpan(1.0f); // Auto-destroy after 1s
		}
	}

	// The target's TakeDamage_Custom applies the protections formula itself —
	// pre-mitigating here would count protections twice. Compute the mitigated
	// value only for lifesteal. Route to the player or god damage path.
	float TargetProtections = 0.0f;
	if (ASkylandersCharacter* PlayerHero = Cast<ASkylandersCharacter>(Hero))
	{
		TargetProtections = FMath::Max(0.0f, PlayerHero->GetEffectiveProtections() - ItemBonusStats.Penetration);
		PlayerHero->TakeDamage_Custom(BurstDamage, this);
	}
	else if (ASkylandersEnemyGod* GodHero = Cast<ASkylandersEnemyGod>(Hero))
	{
		TargetProtections = FMath::Max(0.0f, GodHero->GetEffectiveProtections() - ItemBonusStats.Penetration);
		GodHero->TakeDamage_Custom(BurstDamage, this);
	}
	float DamageReduction = TargetProtections / (TargetProtections + 100.0f);
	float MitigatedDamage = BurstDamage * (1.0f - DamageReduction);

	// Lifesteal from ability damage (based on damage actually dealt)
	if (ItemBonusStats.Lifesteal > 0.0f)
	{
		float HealAmount = MitigatedDamage * ItemBonusStats.Lifesteal;
		float EffMaxHP = MaxHealth + ItemBonusStats.MaxHealth;
		CurrentHealth = FMath::Min(CurrentHealth + HealAmount, EffMaxHP);
	}

	UE_LOG(LogTemp, Verbose, TEXT("Enemy God '%s' used ability! Burst: %.0f, After protections: %.0f"),
		*GodName, BurstDamage, MitigatedDamage);
}

// ========== DAMAGE AND DEATH ==========

void ASkylandersEnemyGod::TakeDamage_Custom(float DamageAmount, AActor* DamageCauser)
{
	if (CurrentState == EGodAIState::Dead) return;

	// Apply protections: DamageReduction = Protections / (Protections + 100),
	// after the attacker's penetration strips them (floor 0).
	float EffProtections = FMath::Max(0.0f, GetEffectiveProtections() - ASkylandersCharacter::GetPenetrationOf(DamageCauser));
	float DamageReduction = EffProtections / (EffProtections + 100.0f);
	float FinalDamage = DamageAmount * (1.0f - DamageReduction);

	float EffMaxHealth = MaxHealth + ItemBonusStats.MaxHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth - FinalDamage, 0.0f, EffMaxHealth);
	USkylandersHitFlashComponent::Flash(this);
	SKY_COUNT("god_damage");

	// Remember the pawn behind the hit (projectiles pass themselves as the causer
	// and their shooter as the instigator) so Die() can credit the real killer.
	{
		APawn* CauserPawn = Cast<APawn>(DamageCauser ? DamageCauser->GetInstigator() : nullptr);
		if (!CauserPawn) CauserPawn = Cast<APawn>(DamageCauser);
		if (CauserPawn)
		{
			LastDamageCauser = CauserPawn;
		}

		// Opposing heroes only: a same-team hit or a minion/tower is never an assist
		AActor* Hero = ASkylandersCharacter::ResolveDamageHero(DamageCauser);
		const bool bOpposingHero = Cast<ASkylandersCharacter>(Hero)
			? (Team == ETowerTeam::Enemy)
			: (Cast<ASkylandersEnemyGod>(Hero) && Cast<ASkylandersEnemyGod>(Hero)->Team != Team);
		if (bOpposingHero)
		{
			RecentHeroDamagers.Add(Hero, GetWorld()->GetTimeSeconds());
		}
	}

	// Spawn floating damage number
	FVector NumberLoc = GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASkylandersDamageNumber* DmgNum = GetWorld()->SpawnActor<ASkylandersDamageNumber>(
		ASkylandersDamageNumber::StaticClass(), NumberLoc, FRotator::ZeroRotator, SpawnParams);
	if (DmgNum)
	{
		bool bBigHit = FinalDamage >= 80.0f;
		FColor DmgColor = bBigHit ? FColor(255, 140, 0) : FColor::Yellow;
		DmgNum->SetDamageNumber(FinalDamage, DmgColor, bBigHit);
	}

	// Getting hit by an opposing *hero* while Laning switches to Fighting (Fighting
	// re-derives the foe hero). Don't chase an attacker that is safe under their
	// own tower. Minion and tower chip damage is deliberately excluded: now that
	// minions attack gods, letting any hit flip the state yanked the god out of
	// lane on every wave tick. Tested against the resolved attacking pawn rather
	// than the raw causer so a projectile's own location isn't what gets checked.
	if ((CurrentState == EGodAIState::Laning || CurrentState == EGodAIState::Jungling) && LastDamageCauser)
	{
		bool bHeroAttacker = false;
		if (Cast<ASkylandersCharacter>(LastDamageCauser))
		{
			bHeroAttacker = (FoeTeam() == ETowerTeam::Friendly);
		}
		else if (ASkylandersEnemyGod* AttackingGod = Cast<ASkylandersEnemyGod>(LastDamageCauser))
		{
			bHeroAttacker = (AttackingGod->Team == FoeTeam());
		}

		if (bHeroAttacker && !WouldChaseIntoEnemyTower(LastDamageCauser))
		{
			// Fight back at whoever hit us (a jungling god drops the camp for it)
			BeginFighting(LastDamageCauser, FMath::Max(1500.0f, AggroRange));
			UE_LOG(LogTemp, Verbose, TEXT("God '%s' aggro'd by damage!"), *GodName);
		}
	}

	// SMITE tower aggro: if a player hits the god, notify enemy towers
	if (DamageCauser)
	{
		ASkylandersCharacter* AttackingPlayer = Cast<ASkylandersCharacter>(DamageCauser);
		if (!AttackingPlayer)
		{
			APawn* DmgInstigator = DamageCauser->GetInstigator();
			AttackingPlayer = Cast<ASkylandersCharacter>(DmgInstigator);
		}
		if (AttackingPlayer)
		{
			// Notify the towers/minions that protect *this* god (its own team),
			// not a hardcoded red team — otherwise hitting a blue ally would call
			// red structures onto you.
			TArray<AActor*> AllTowers;
			USkylandersActorRegistry::GetAllActorsOfClass(GetWorld(), ASkylandersTower::StaticClass(), AllTowers);
			for (AActor* TowerActor : AllTowers)
			{
				ASkylandersTower* Tower = Cast<ASkylandersTower>(TowerActor);
				if (Tower && Tower->Team == Team)
				{
					Tower->NotifyPlayerAggro(AttackingPlayer);
				}
			}

			// Nearby minions of this god's team aggro the attacker
			TArray<AActor*> AllMinions;
			USkylandersActorRegistry::GetAllActorsOfClass(GetWorld(), ASkylandersMinion::StaticClass(), AllMinions);
			for (AActor* MinionActor : AllMinions)
			{
				ASkylandersMinion* Minion = Cast<ASkylandersMinion>(MinionActor);
				if (Minion && Minion->Team == Team && !Minion->bDead)
				{
					float Dist = FVector::Dist(GetActorLocation(), Minion->GetActorLocation());
					if (Dist < 500.0f)
					{
						Minion->ForcedAggroTarget = AttackingPlayer;
						Minion->ForcedAggroTimer = 4.0f;
					}
				}
			}
		}
	}

	UpdateHealthBar();

	if (CurrentHealth <= 0.0f)
	{
		Die();
	}
}

void ASkylandersEnemyGod::Die()
{
	SKY_COUNT("god_death");
	Deaths++;
	UE_LOG(LogTemp, Warning, TEXT("Enemy God '%s' has been slain! Level %d"), *GodName, Level);

	if (USkylandersTelemetrySubsystem* Tele = USkylandersTelemetrySubsystem::Get(this))
	{
		Tele->LogGodDeath(this, TEXT("combat"));
	}

	// Play death sound
	if (DeathSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation());
	}

	if (USkylandersAnnouncerWidget::ClaimFirstBlood(this))
	{
		USkylandersAnnouncerWidget::Announce(this, TEXT("FIRST BLOOD"), FLinearColor(1.0f, 0.8f, 0.2f), 2.2f, TEXT("kill"));
	}

	// An ally god (or the player) killing a red god is a kill for whoever landed
	// the blow — the player's KDA and gold shouldn't absorb their teammates' work.
	// A god killing the player, or a red god killing a blue one, pays that god.
	ASkylandersEnemyGod* KillerGod = Cast<ASkylandersEnemyGod>(LastDamageCauser);
	if (KillerGod && (KillerGod == this || KillerGod->Team == Team))
	{
		KillerGod = nullptr; // Self/same-team damage isn't a kill for anyone
	}
	if (KillerGod)
	{
		KillerGod->Gold += CoinReward;
		KillerGod->Kills++;
	}

	// Whoever is credited with the kill below: the killer god, else (for a red
	// god) the player, who takes credit for minion/tower/fountain finishes too.
	ASkylandersCharacter* Player = FindPlayer();
	AActor* KillCredit = KillerGod ? static_cast<AActor*>(KillerGod)
		: (Team == ETowerTeam::Enemy ? static_cast<AActor*>(Player) : nullptr);

	// Assists: every opposing hero that hit this god inside the window and
	// didn't get the kill earns half the bounty (the player also half the XP).
	{
		const float Now = GetWorld()->GetTimeSeconds();
		for (const TPair<TWeakObjectPtr<AActor>, float>& Hit : RecentHeroDamagers)
		{
			AActor* Hero = Hit.Key.Get();
			if (!Hero || Hero == KillCredit || Now - Hit.Value > ASkylandersCharacter::AssistWindowSeconds) continue;

			if (ASkylandersCharacter* AssistPlayer = Cast<ASkylandersCharacter>(Hero))
			{
				AssistPlayer->Assists++;
				AssistPlayer->AddCoins(CoinReward / 2);
				AssistPlayer->AddXP(XPReward * 0.5f);
				USkylandersKillFeedWidget::Post(this, FString::Printf(TEXT("Assist: %s"), *GodName),
					FLinearColor(0.5f, 0.9f, 1.0f));
				SKY_COUNT("assist");
			}
			else if (ASkylandersEnemyGod* AssistGod = Cast<ASkylandersEnemyGod>(Hero))
			{
				AssistGod->Assists++;
				AssistGod->Gold += CoinReward / 2;
				SKY_COUNT("assist");
			}
		}
		RecentHeroDamagers.Reset();
	}

	// Only an enemy-team god rewards the player / posts the "you slew" feed. An
	// ally god dying is a loss for the player's team, not a kill.
	if (Team == ETowerTeam::Enemy)
	{
		if (KillerGod)
		{
			// An ally god got it — report it, but don't pay the player for it.
			USkylandersKillFeedWidget::Post(this, FString::Printf(TEXT("%s slew %s"), *KillerGod->GodName, *GodName),
				FLinearColor(0.4f, 0.8f, 1.0f));
		}
		else
		{
			if (Player)
			{
				Player->AddXP(XPReward);
				Player->AddCoins(CoinReward);
				Player->Kills++;
				UE_LOG(LogTemp, Log, TEXT("Player rewarded for god kill: %.0f XP, %d coins (Kills: %d)"), XPReward, CoinReward, Player->Kills);
			}
			USkylandersKillFeedWidget::Post(this, FString::Printf(TEXT("You slew %s"), *GodName),
				FLinearColor(0.3f, 1.0f, 0.4f));
			// The banner carries the kill sting (and plays it directly when no
			// announcer is on screen), so there is exactly one kill sound.
			USkylandersAnnouncerWidget::Announce(this, FString::Printf(TEXT("%s SLAIN"), *GodName.ToUpper()),
				FLinearColor(0.3f, 1.0f, 0.4f), 2.2f, TEXT("kill"));
		}
	}
	else
	{
		USkylandersKillFeedWidget::Post(this, FString::Printf(TEXT("Ally %s was slain"), *GodName),
			FLinearColor(1.0f, 0.5f, 0.3f));
	}

	SetState(EGodAIState::Dead);
	// Camp buffs are lost on death, as in SMITE (and as the player's are)
	ExpireCampBuffs(true);
	FightTarget = nullptr;
	JungleTarget = nullptr;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);

	// Respawn delay scales with level and match length (SMITE-like):
	// 15s + Level * 2s + 1.5s per full match minute, capped at 45s. Match time is
	// world time, the same clock telemetry stamps events with.
	const float MatchMinutes = FMath::FloorToFloat(GetWorld()->GetTimeSeconds() / 60.0f);
	float ActualRespawnDelay = FMath::Min(RespawnDelay + Level * 2.0f + MatchMinutes * 1.5f, 45.0f);
	UE_LOG(LogTemp, Log, TEXT("Enemy God '%s' will respawn in %.0f seconds."), *GodName, ActualRespawnDelay);

	GetWorld()->GetTimerManager().SetTimer(RespawnTimerHandle, this, &ASkylandersEnemyGod::RespawnGod, ActualRespawnDelay, false);
}

void ASkylandersEnemyGod::RespawnGod()
{
	UE_LOG(LogTemp, Log, TEXT("Enemy God '%s' respawned at base! Level %d, Gold: %d"), *GodName, Level, Gold);

	// Full heal (including item bonus HP/MP)
	CurrentHealth = MaxHealth + ItemBonusStats.MaxHealth;
	CurrentMana = MaxMana + ItemBonusStats.MaxMana;

	// Reset state
	SetState(EGodAIState::Laning);
	AttackTimer = 0.0f;
	AbilityCooldownTimer = 8.0f;
	LastDamageCauser = nullptr; // Don't let last life's killer credit this one's death
	RecentHeroDamagers.Reset();

	// Restore visibility and collision
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorLocation(BasePosition);

	// Reset speed (include item bonuses)
	GetCharacterMovement()->MaxWalkSpeed = GetWalkSpeed();

	// Reset health bar
	CachedHealthBar = nullptr;
	CachedNameText = nullptr;
	CachedHealthText = nullptr;
	bHealthBarInitialized = false;

	UpdateHealthBar();
}

// ========== HEALTH BAR ==========

void ASkylandersEnemyGod::UpdateHealthBar()
{
	if (!HealthBarComp) return;

	UUserWidget* Widget = HealthBarComp->GetWidget();
	if (!Widget) return;

	if (!bHealthBarInitialized)
	{
		CachedNameText = Cast<UTextBlock>(Widget->WidgetTree->FindWidget(FName("EnemyNameText")));
		CachedHealthBar = Cast<UProgressBar>(Widget->WidgetTree->FindWidget(FName("EnemyHealthBar")));
		CachedHealthText = Cast<UTextBlock>(Widget->WidgetTree->FindWidget(FName("EnemyHealthText")));

		if (CachedNameText && CachedHealthBar && CachedHealthText)
		{
			bHealthBarInitialized = true;

			// Purple/magenta background for the god health bar
			FProgressBarStyle BarStyle = CachedHealthBar->GetWidgetStyle();
			BarStyle.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.2f, 0.02f, 0.3f, 1.0f));
			CachedHealthBar->SetWidgetStyle(BarStyle);
		}
		else
		{
			return;
		}
	}

	// Display name with level and item count
	int32 ItemCount = 0;
	for (int32 i = 0; i < MaxInventorySlots; i++)
	{
		if (Inventory[i] != 0) ItemCount++;
	}

	FString DisplayName;
	if (ItemCount > 0)
	{
		DisplayName = FString::Printf(TEXT("%s [Lv.%d] (%d items)"), *GodName, Level, ItemCount);
	}
	else
	{
		DisplayName = FString::Printf(TEXT("%s [Lv.%d]"), *GodName, Level);
	}
	CachedNameText->SetText(FText::FromString(DisplayName));

	float EffMaxHealth = MaxHealth + ItemBonusStats.MaxHealth;
	float Pct = (EffMaxHealth > 0.0f) ? (CurrentHealth / EffMaxHealth) : 0.0f;
	CachedHealthBar->SetPercent(Pct);

	// Purple/magenta bar color for the god
	FLinearColor BarColor = FLinearColor(0.7f, 0.1f, 0.9f, 1.0f); // Magenta/purple
	if (Pct < 0.3f) BarColor = FLinearColor(0.9f, 0.1f, 0.1f, 1.0f); // Red when low
	CachedHealthBar->SetFillColorAndOpacity(BarColor);

	FString HealthStr = FString::Printf(TEXT("%.0f/%.0f"), CurrentHealth, EffMaxHealth);
	CachedHealthText->SetText(FText::FromString(HealthStr));
}

void ASkylandersEnemyGod::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USkylandersActorRegistry* Registry = USkylandersActorRegistry::Get(this))
	{
		Registry->Unregister(this);
	}
	Super::EndPlay(EndPlayReason);
}

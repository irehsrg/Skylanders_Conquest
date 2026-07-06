// Skylanders Conquest - AI-Controlled Enemy God Implementation

#include "SkylandersEnemyGod.h"
#include "SkylandersCharacter.h"
#include "SkylandersMinion.h"
#include "SkylandersTower.h"
#include "SkylandersTitan.h"
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
#include "Kismet/GameplayStatics.h"
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
	Protections = 0.0f;

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

	BasePosition = GetActorLocation(); // Use placed position as base, fallback to (4500,0,0)
	if (BasePosition.IsNearlyZero())
	{
		BasePosition = FVector(4500.0f, 0.0f, 0.0f);
	}

	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;

	// Load health bar widget
	UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UserInterface/WBP_EnemyHealthBar.WBP_EnemyHealthBar_C"));
	if (WidgetClass && HealthBarComp)
	{
		HealthBarComp->SetWidgetClass(WidgetClass);
		HealthBarComp->InitWidget();
	}

	// Dark red body color
	if (BodyMesh)
	{
		UMaterialInterface* DefaultMat = BodyMesh->GetMaterial(0);
		if (DefaultMat)
		{
			UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(DefaultMat, this);
			DynMat->SetVectorParameterValue(FName("Color"), FLinearColor(0.8f, 0.1f, 0.1f));
			BodyMesh->SetMaterial(0, DynMat);
		}
	}

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
		GetCharacterMovement()->MaxWalkSpeed = MoveSpeed + ItemBonusStats.MovementSpeed;
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
	CurrentMana = FMath::Min(CurrentMana + MpRegen, EffMaxMana);
}

// ========== PASSIVE GOLD INCOME ==========

void ASkylandersEnemyGod::TickPassiveGold(float DeltaTime)
{
	// Base: 2 gold/sec + 0.5 per level (scales so the god can keep buying items)
	float GoldRate = PassiveGoldPerSecond + Level * 0.5f;
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
	return Protections + ItemBonusStats.Protections;
}

// ========== TOWER AWARENESS ==========

bool ASkylandersEnemyGod::IsPositionUnderFriendlyTower(const FVector& Position) const
{
	// "Friendly" towers from the player's perspective (ETowerTeam::Friendly)
	// The enemy god should NOT walk into these
	TArray<AActor*> AllTowers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersTower::StaticClass(), AllTowers);

	for (AActor* TowerActor : AllTowers)
	{
		ASkylandersTower* Tower = Cast<ASkylandersTower>(TowerActor);
		if (Tower && Tower->Team == ETowerTeam::Friendly && !Tower->bDestroyed)
		{
			float DistToTower = FVector::Dist(Position, Tower->GetActorLocation());
			if (DistToTower < Tower->AttackRange)
			{
				return true;
			}
		}
	}
	return false;
}

bool ASkylandersEnemyGod::WouldChaseIntoFriendlyTower(AActor* Target) const
{
	if (!Target) return false;
	return IsPositionUnderFriendlyTower(Target->GetActorLocation());
}

// ========== ITEM PURCHASING ==========

const TArray<int32>& ASkylandersEnemyGod::GetItemPurchasePriority()
{
	// Priority: cheap damage -> boots -> cheap defense -> mid damage -> mid defense -> expensive damage -> expensive defense
	// 1=Short Sword(500), 10=Winged Boots(450), 6=Skystone Shield(600),
	// 3=Berserker Gauntlets(800), 4=Vampire Fangs(900), 7=Heart of Skylands(850),
	// 2=Elemental Blade(1200), 8=Guardian Plate(1100),
	// 5=Golden Gatling(1800), 9=Portal Master's Armor(1500)
	static TArray<int32> Priority = { 1, 10, 6, 3, 4, 2, 7, 8, 5, 9 };
	return Priority;
}

void ASkylandersEnemyGod::TryPurchaseItems()
{
	// Only buy items when near base (within 500 units of spawn)
	float DistToBase = FVector::Dist(GetActorLocation(), BasePosition);
	if (DistToBase > 500.0f) return;

	// Count current items
	int32 OwnedCount = 0;
	for (int32 i = 0; i < MaxInventorySlots; i++)
	{
		if (Inventory[i] != 0) OwnedCount++;
	}
	if (OwnedCount >= MaxInventorySlots) return; // Full inventory

	const TArray<int32>& Priority = GetItemPurchasePriority();

	for (int32 ItemID : Priority)
	{
		const FSkylandersItemData* ItemData = USkylandersItemCatalog::FindItem(ItemID);
		if (!ItemData) continue;
		if (Gold < ItemData->Cost) continue;

		// Don't buy more than 2 of any single item
		int32 OwnedCountOfThis = 0;
		for (int32 i = 0; i < MaxInventorySlots; i++)
		{
			if (Inventory[i] == ItemID) OwnedCountOfThis++;
		}
		if (OwnedCountOfThis >= 2) continue;

		// Find empty slot
		int32 EmptySlot = -1;
		for (int32 i = 0; i < MaxInventorySlots; i++)
		{
			if (Inventory[i] == 0) { EmptySlot = i; break; }
		}
		if (EmptySlot < 0) break;

		// Purchase
		Gold -= ItemData->Cost;
		Inventory[EmptySlot] = ItemID;
		RecalculateItemBonuses();

		UE_LOG(LogTemp, Warning, TEXT("Enemy God '%s' bought '%s' for %d gold (remaining: %d). Slot %d"),
			*GodName, *ItemData->ItemName, ItemData->Cost, Gold, EmptySlot);

		// Only buy one item per tick to simulate realistic shopping
		break;
	}
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
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed + ItemBonusStats.MovementSpeed;

	UE_LOG(LogTemp, Log, TEXT("Enemy God '%s' item bonuses: +%.0f Power, +%.0f HP, +%.0f Protections, +%.0f Speed, +%.0f%% Crit, +%.0f%% Lifesteal"),
		*GodName, ItemBonusStats.Power, ItemBonusStats.MaxHealth, ItemBonusStats.Protections,
		ItemBonusStats.MovementSpeed, ItemBonusStats.CritChance * 100.0f, ItemBonusStats.Lifesteal * 100.0f);
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

ASkylandersMinion* ASkylandersEnemyGod::FindNearestEnemyMinion()
{
	// Find the nearest player-team (Friendly) minion to attack
	TArray<AActor*> AllMinions;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersMinion::StaticClass(), AllMinions);

	ASkylandersMinion* Nearest = nullptr;
	float NearestDist = AttackRange;

	for (AActor* Actor : AllMinions)
	{
		ASkylandersMinion* Minion = Cast<ASkylandersMinion>(Actor);
		if (Minion && Minion->Team == ETowerTeam::Friendly && !Minion->bDead)
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

void ASkylandersEnemyGod::UpdateAI(float DeltaTime)
{
	ASkylandersCharacter* Player = FindPlayer();
	float DistToPlayer = Player ? FVector::Dist(GetActorLocation(), Player->GetActorLocation()) : 99999.0f;
	bool bPlayerAlive = Player && Player->CurrentHealth > 0.0f;
	float EffMaxHealth = MaxHealth + ItemBonusStats.MaxHealth;
	float HealthPct = (EffMaxHealth > 0.0f) ? (CurrentHealth / EffMaxHealth) : 0.0f;

	// === Item purchasing (any state, checks distance to base internally) ===
	TryPurchaseItems();

	switch (CurrentState)
	{
	case EGodAIState::Laning:
	{
		// Auto-attack nearest player-team minion
		ASkylandersMinion* TargetMinion = FindNearestEnemyMinion();
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
				// Move toward minion but stay behind friendly (enemy team) minion wave
				// Find the frontmost enemy-team minion to stay behind
				TArray<AActor*> AllMinions;
				UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersMinion::StaticClass(), AllMinions);
				float FriendlyFrontX = GetActorLocation().X; // Default to our position
				bool bHasFriendlyMinions = false;
				for (AActor* Actor : AllMinions)
				{
					ASkylandersMinion* Minion = Cast<ASkylandersMinion>(Actor);
					if (Minion && Minion->Team == ETowerTeam::Enemy && !Minion->bDead)
					{
						bHasFriendlyMinions = true;
						// Enemy team minions push toward negative X (toward player base)
						if (Minion->GetActorLocation().X < FriendlyFrontX)
						{
							FriendlyFrontX = Minion->GetActorLocation().X;
						}
					}
				}

				// Only advance as far as the front friendly minion (stay ~200 units behind)
				FVector TargetPos = TargetMinion->GetActorLocation();
				if (bHasFriendlyMinions && TargetPos.X < FriendlyFrontX - 200.0f)
				{
					// Don't push past our minion wave
					TargetPos.X = FriendlyFrontX + 200.0f;
				}

				// Tower awareness: don't walk into friendly (player-team) tower range
				if (!IsPositionUnderFriendlyTower(TargetPos))
				{
					FVector MoveDir = (TargetPos - GetActorLocation()).GetSafeNormal2D();
					AddMovementInput(MoveDir, 1.0f);
				}
				// else: stay put, don't advance into tower range
			}
		}
		else
		{
			// No enemy minions in range - look for structures to push or advance down lane

			// Gather friendly (Enemy-team) minion info for cover checks
			TArray<AActor*> AllMinions;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersMinion::StaticClass(), AllMinions);
			float FriendlyFrontX = GetActorLocation().X;
			bool bHasFriendlyMinions = false;
			int32 FriendlyMinionsNearby = 0;
			for (AActor* Actor : AllMinions)
			{
				ASkylandersMinion* Minion = Cast<ASkylandersMinion>(Actor);
				if (Minion && Minion->Team == ETowerTeam::Enemy && !Minion->bDead)
				{
					bHasFriendlyMinions = true;
					if (Minion->GetActorLocation().X < FriendlyFrontX)
					{
						FriendlyFrontX = Minion->GetActorLocation().X;
					}
					// Count friendly minions within 1500 units (nearby for cover)
					float MinionDist = FVector::Dist(GetActorLocation(), Minion->GetActorLocation());
					if (MinionDist < 1500.0f)
					{
						FriendlyMinionsNearby++;
					}
				}
			}

			// === Structure targeting: find nearest vulnerable Friendly-team tower/phoenix/titan ===
			AActor* TargetStructure = nullptr;
			float NearestStructureDist = 99999.0f;

			// Check towers (includes phoenixes)
			TArray<AActor*> AllTowers;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersTower::StaticClass(), AllTowers);
			for (AActor* TowerActor : AllTowers)
			{
				ASkylandersTower* Tower = Cast<ASkylandersTower>(TowerActor);
				if (Tower && Tower->Team == ETowerTeam::Friendly && !Tower->bDestroyed && Tower->IsVulnerable())
				{
					float Dist = FVector::Dist(GetActorLocation(), Tower->GetActorLocation());
					if (Dist < NearestStructureDist)
					{
						NearestStructureDist = Dist;
						TargetStructure = Tower;
					}
				}
			}

			// Check titans (only if their protecting tower is destroyed)
			TArray<AActor*> AllTitans;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersTitan::StaticClass(), AllTitans);
			for (AActor* TitanActor : AllTitans)
			{
				ASkylandersTitan* Titan = Cast<ASkylandersTitan>(TitanActor);
				if (Titan && Titan->Team == ETowerTeam::Friendly && !Titan->bDead && Titan->IsVulnerable())
				{
					float Dist = FVector::Dist(GetActorLocation(), Titan->GetActorLocation());
					if (Dist < NearestStructureDist)
					{
						NearestStructureDist = Dist;
						TargetStructure = Titan;
					}
				}
			}

			// Only push structures if we have minion cover (at least 2 friendly minions nearby)
			// This prevents the god from solo-diving towers
			bool bHasMinionCover = FriendlyMinionsNearby >= 2;

			if (TargetStructure && bHasMinionCover && NearestStructureDist < AggroRange)
			{
				if (NearestStructureDist <= AttackRange)
				{
					// In range - attack the structure
					FaceTarget(TargetStructure);
					if (AttackTimer <= 0.0f)
					{
						PerformAttack(TargetStructure);
						AttackTimer = AttackCooldown;
					}
				}
				else
				{
					// Move toward structure but stay behind friendly minion wave
					FVector TargetPos = TargetStructure->GetActorLocation();
					if (bHasFriendlyMinions && TargetPos.X < FriendlyFrontX - 200.0f)
					{
						// Don't push past our minion wave
						TargetPos.X = FriendlyFrontX + 200.0f;
					}

					// Tower awareness: don't walk into tower range without minion wave ahead of us
					if (!IsPositionUnderFriendlyTower(TargetPos))
					{
						FVector MoveDir = (TargetPos - GetActorLocation()).GetSafeNormal2D();
						AddMovementInput(MoveDir, 0.8f);
					}
				}
			}
			else
			{
				// No valid structure target or no minion cover - advance slowly down lane
				FVector LaneDir = FVector(-1.0f, 0.0f, 0.0f);

				// Tower awareness: check if advancing would put us under a friendly tower
				FVector ProjectedPos = GetActorLocation() + LaneDir * 100.0f;
				bool bTowerBlock = IsPositionUnderFriendlyTower(ProjectedPos);

				if (!bTowerBlock && (!bHasFriendlyMinions || GetActorLocation().X > FriendlyFrontX + 200.0f))
				{
					AddMovementInput(LaneDir, 0.5f); // Slow advance
				}
			}
		}

		// Check if player is close and we have enough HP to fight
		if (bPlayerAlive && DistToPlayer <= AggroRange && HealthPct > 0.5f)
		{
			// Tower awareness: don't engage if player is under their tower
			if (!WouldChaseIntoFriendlyTower(Player))
			{
				// Smarter engagement: prefer fighting low-HP players
				float PlayerHealthPct = Player->CurrentHealth / Player->MaxHealth;
				bool bPlayerVulnerable = PlayerHealthPct < 0.5f;

				// Engage if player is within 70% of aggro range, or if player is vulnerable
				if (DistToPlayer <= AggroRange * 0.7f || bPlayerVulnerable)
				{
					CurrentState = EGodAIState::Fighting;
					UE_LOG(LogTemp, Log, TEXT("Enemy God '%s' engages player! Distance: %.0f, PlayerHP: %.0f%%"),
						*GodName, DistToPlayer, PlayerHealthPct * 100.0f);
				}
			}
		}

		break;
	}

	case EGodAIState::Fighting:
	{
		if (!bPlayerAlive)
		{
			CurrentState = EGodAIState::Laning;
			UE_LOG(LogTemp, Log, TEXT("Enemy God '%s' returns to laning - player dead."), *GodName);
			break;
		}

		// Disengage if player too far
		if (DistToPlayer > 1500.0f)
		{
			CurrentState = EGodAIState::Laning;
			UE_LOG(LogTemp, Log, TEXT("Enemy God '%s' lost aggro on player, returning to lane."), *GodName);
			break;
		}

		// Retreat if low HP (gets braver as it levels up, min 15%)
		float RetreatThreshold = FMath::Max(0.15f, 0.30f - Level * 0.01f);
		if (HealthPct < RetreatThreshold)
		{
			CurrentState = EGodAIState::Retreating;
			UE_LOG(LogTemp, Warning, TEXT("Enemy God '%s' retreating! HP: %.0f/%.0f (%.0f%%)"),
				*GodName, CurrentHealth, EffMaxHealth, HealthPct * 100.0f);
			break;
		}

		// Tower awareness: back off if chasing would put us under a friendly tower
		if (WouldChaseIntoFriendlyTower(Player))
		{
			if (IsPositionUnderFriendlyTower(GetActorLocation()))
			{
				// Already inside tower range - retreat immediately
				CurrentState = EGodAIState::Retreating;
				UE_LOG(LogTemp, Warning, TEXT("Enemy God '%s' retreating - under friendly tower!"), *GodName);
				break;
			}
			else
			{
				// Player is under tower but we're safe - disengage
				CurrentState = EGodAIState::Laning;
				UE_LOG(LogTemp, Log, TEXT("Enemy God '%s' disengaging - player is under tower."), *GodName);
				break;
			}
		}

		// Smarter target priority: last-hit low-HP minions for gold even during fights
		ASkylandersMinion* NearMinion = FindNearestEnemyMinion();
		if (NearMinion)
		{
			float MinionDist = FVector::Dist(GetActorLocation(), NearMinion->GetActorLocation());
			float MinionHpPct = NearMinion->CurrentHealth / NearMinion->MaxHealth;

			// Prioritize last-hitting a very low HP minion within attack range
			if (MinionDist <= AttackRange && MinionHpPct < 0.2f && AttackTimer <= 0.0f)
			{
				FaceTarget(NearMinion);
				PerformAttack(NearMinion);
				AttackTimer = AttackCooldown;
				break; // Skip player attack this tick, focus on the last hit
			}
		}

		// Chase and kite: approach when attack is ready, back off when on cooldown
		FaceTarget(Player);
		if (DistToPlayer > AttackRange)
		{
			// Chase player
			FVector MoveDir = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			AddMovementInput(MoveDir, 1.0f);
		}
		else if (DistToPlayer < AttackRange * 0.4f && AttackTimer > 0.0f)
		{
			// Kiting: too close and attack on cooldown - back up to maintain optimal range
			FVector AwayDir = (GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
			AddMovementInput(AwayDir, 0.6f);
		}

		// Auto-attack
		if (DistToPlayer <= AttackRange && AttackTimer <= 0.0f)
		{
			PerformAttack(Player);
			AttackTimer = AttackCooldown;
		}

		// Use ability (burst damage every 8 seconds)
		if (AbilityCooldownTimer <= 0.0f && DistToPlayer <= AttackRange)
		{
			UseAbility();
			AbilityCooldownTimer = 8.0f;
		}

		break;
	}

	case EGodAIState::Retreating:
	{
		// Move back to base at 1.3x speed
		GetCharacterMovement()->MaxWalkSpeed = (MoveSpeed + ItemBonusStats.MovementSpeed) * 1.3f;

		FVector ReturnDir = (BasePosition - GetActorLocation()).GetSafeNormal2D();
		AddMovementInput(ReturnDir, 1.0f);

		// Face movement direction
		FRotator ReturnRot = ReturnDir.Rotation();
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), ReturnRot, DeltaTime, 10.0f));

		// Fight back if cornered (player within 200 units)
		if (bPlayerAlive && DistToPlayer <= 200.0f)
		{
			FaceTarget(Player);
			if (AttackTimer <= 0.0f)
			{
				PerformAttack(Player);
				AttackTimer = AttackCooldown;
			}

			// Use ability defensively when cornered
			if (AbilityCooldownTimer <= 0.0f)
			{
				UseAbility();
				AbilityCooldownTimer = 8.0f;
			}
		}

		// Resume when HP recovers above 50% (but prefer to be near base first)
		if (HealthPct >= 0.50f)
		{
			float DistToBase = FVector::Dist(GetActorLocation(), BasePosition);
			// Only re-engage if healed at base or HP is high enough
			if (DistToBase < 400.0f || HealthPct >= 0.60f)
			{
				GetCharacterMovement()->MaxWalkSpeed = MoveSpeed + ItemBonusStats.MovementSpeed;
				CurrentState = EGodAIState::Laning;
				UE_LOG(LogTemp, Log, TEXT("Enemy God '%s' recovered, returning to lane. HP: %.0f/%.0f"),
					*GodName, CurrentHealth, EffMaxHealth);
				break;
			}
		}

		// If arrived at base, use fountain regen (handled in TickPassiveRegen)
		float DistToBase = FVector::Dist(GetActorLocation(), BasePosition);
		if (DistToBase < 100.0f)
		{
			GetCharacterMovement()->MaxWalkSpeed = MoveSpeed + ItemBonusStats.MovementSpeed;
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
		float TargetProtections = PlayerTarget->GetEffectiveProtections();
		float DamageReduction = TargetProtections / (TargetProtections + 100.0f);
		float MitigatedDamage = EffectiveDamage * (1.0f - DamageReduction);

		PlayerTarget->TakeDamage_Custom(EffectiveDamage);

		// Lifesteal from items (based on damage actually dealt)
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
		TowerTarget->TakeDamage_Custom(EffectiveDamage, this);
		return;
	}

	ASkylandersTitan* TitanTarget = Cast<ASkylandersTitan>(Target);
	if (TitanTarget)
	{
		TitanTarget->TakeDamage_Custom(EffectiveDamage, this);
		return;
	}
}

// ========== ABILITY ==========

void ASkylandersEnemyGod::UseAbility()
{
	ASkylandersCharacter* Player = FindPlayer();
	if (!Player || Player->CurrentHealth <= 0.0f) return;

	float DistToPlayer = FVector::Dist(GetActorLocation(), Player->GetActorLocation());
	if (DistToPlayer > AttackRange) return;

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
		AActor* AbilVFX = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), Player->GetActorLocation(), FRotator::ZeroRotator, AbilVFXParams);
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

	// The player's TakeDamage_Custom applies the protections formula itself —
	// pre-mitigating here would count protections twice. Compute the mitigated
	// value only for lifesteal. (It also spawns the damage number.)
	float TargetProtections = Player->GetEffectiveProtections();
	float DamageReduction = TargetProtections / (TargetProtections + 100.0f);
	float MitigatedDamage = BurstDamage * (1.0f - DamageReduction);

	// Deal burst damage
	Player->TakeDamage_Custom(BurstDamage);

	// Lifesteal from ability damage (based on damage actually dealt)
	if (ItemBonusStats.Lifesteal > 0.0f)
	{
		float HealAmount = MitigatedDamage * ItemBonusStats.Lifesteal;
		float EffMaxHP = MaxHealth + ItemBonusStats.MaxHealth;
		CurrentHealth = FMath::Min(CurrentHealth + HealAmount, EffMaxHP);
	}

	UE_LOG(LogTemp, Log, TEXT("Enemy God '%s' used ability! Burst: %.0f, After protections: %.0f"),
		*GodName, BurstDamage, MitigatedDamage);
}

// ========== DAMAGE AND DEATH ==========

void ASkylandersEnemyGod::TakeDamage_Custom(float DamageAmount, AActor* DamageCauser)
{
	if (CurrentState == EGodAIState::Dead) return;

	// Apply protections: DamageReduction = Protections / (Protections + 100)
	float EffProtections = GetEffectiveProtections();
	float DamageReduction = EffProtections / (EffProtections + 100.0f);
	float FinalDamage = DamageAmount * (1.0f - DamageReduction);

	float EffMaxHealth = MaxHealth + ItemBonusStats.MaxHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth - FinalDamage, 0.0f, EffMaxHealth);

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

	// Getting hit while Laning switches to Fighting if a player did it
	if (CurrentState == EGodAIState::Laning && DamageCauser)
	{
		ASkylandersCharacter* AttackingPlayer = Cast<ASkylandersCharacter>(DamageCauser);
		if (!AttackingPlayer)
		{
			APawn* DmgInstigator = DamageCauser->GetInstigator();
			AttackingPlayer = Cast<ASkylandersCharacter>(DmgInstigator);
		}
		if (AttackingPlayer)
		{
			// Only aggro if the player is NOT under a friendly tower
			if (!WouldChaseIntoFriendlyTower(AttackingPlayer))
			{
				CurrentState = EGodAIState::Fighting;
				UE_LOG(LogTemp, Log, TEXT("Enemy God '%s' aggro'd by player damage!"), *GodName);
			}
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
			TArray<AActor*> AllTowers;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersTower::StaticClass(), AllTowers);
			for (AActor* TowerActor : AllTowers)
			{
				ASkylandersTower* Tower = Cast<ASkylandersTower>(TowerActor);
				if (Tower && Tower->Team == ETowerTeam::Enemy)
				{
					Tower->NotifyPlayerAggro(AttackingPlayer);
				}
			}

			// Nearby enemy minions aggro the player
			TArray<AActor*> AllMinions;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersMinion::StaticClass(), AllMinions);
			for (AActor* MinionActor : AllMinions)
			{
				ASkylandersMinion* Minion = Cast<ASkylandersMinion>(MinionActor);
				if (Minion && Minion->Team == ETowerTeam::Enemy && !Minion->bDead)
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
	UE_LOG(LogTemp, Warning, TEXT("Enemy God '%s' has been slain! Level %d"), *GodName, Level);

	// Play death sound
	if (DeathSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation());
	}

	// Reward player with XP and coins (scaled with level)
	ASkylandersCharacter* Player = FindPlayer();
	if (Player)
	{
		Player->AddXP(XPReward);
		Player->AddCoins(CoinReward);
		Player->Kills++;
		UE_LOG(LogTemp, Log, TEXT("Player rewarded for god kill: %.0f XP, %d coins (Kills: %d)"), XPReward, CoinReward, Player->Kills);
	}

	// Kill feed: enemy god slain
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(105, 5.0f, FColor::Yellow,
			FString::Printf(TEXT("You have slain %s!"), *GodName));
	}

	CurrentState = EGodAIState::Dead;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);

	// Respawn delay scales with level: 15s + Level * 2s
	float ActualRespawnDelay = RespawnDelay + Level * 2.0f;
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
	CurrentState = EGodAIState::Laning;
	AttackTimer = 0.0f;
	AbilityCooldownTimer = 8.0f;

	// Restore visibility and collision
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorLocation(BasePosition);

	// Reset speed (include item bonuses)
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed + ItemBonusStats.MovementSpeed;

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

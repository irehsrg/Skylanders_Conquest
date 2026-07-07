// Skylanders Conquest - Minion Implementation

#include "SkylandersMinion.h"
#include "SkylandersCharacter.h"
#include "SkylandersEnemy.h"
#include "SkylandersTower.h"
#include "SkylandersTitan.h"
#include "SkylandersDamageNumber.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

ASkylandersMinion::ASkylandersMinion()
{
	PrimaryActorTick.bCanEverTick = true;

	// Ensure AI controller is spawned for dynamically created minions
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Small capsule
	GetCapsuleComponent()->InitCapsuleSize(20.0f, 30.0f);

	// Movement - must disable controller rotation so movement input works
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->MaxWalkSpeed = 350.0f;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	// RVO avoidance: minions steer around each other and the player on the
	// ground plane instead of piling up (flat crowd behaviour).
	GetCharacterMovement()->bUseRVOAvoidance = true;
	GetCharacterMovement()->AvoidanceConsiderationRadius = 150.0f;

	// Body mesh - small cylinder for melee, will be configured in BeginPlay
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -30.0f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CylinderMesh.Object);
		BodyMesh->SetRelativeScale3D(FVector(0.4f, 0.4f, 0.5f));
	}

	// Health bar
	HealthBarComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
	HealthBarComp->SetupAttachment(RootComponent);
	HealthBarComp->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
	HealthBarComp->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarComp->SetDrawSize(FVector2D(60.0f, 10.0f));
	HealthBarComp->SetPivot(FVector2D(0.5f, 1.0f));

	// Defaults (overridden per type in BeginPlay)
	MaxHealth = 150.0f;
	CurrentHealth = 150.0f;
	AttackDamage = 12.0f;
	AttackRange = 120.0f;
	AggroRange = 500.0f;
	MoveSpeed = 350.0f;
	AttackCooldown = 1.5f;
	XPReward = 15.0f;
	CoinReward = 8;

	Team = ETowerTeam::Friendly;
	MinionType = EMinionType::Melee;
	LaneTargetPoint = FVector::ZeroVector;
	CurrentWaypointIndex = 0;

	bDead = false;
	AttackTimer = 0.0f;
	CurrentTarget = nullptr;
	ForcedAggroTarget = nullptr;
	ForcedAggroTimer = 0.0f;
	bHealthBarInitialized = false;
	CachedHealthBar = nullptr;
	CachedNameText = nullptr;
	CachedHealthText = nullptr;

	// Stuck detection
	LastPosition = FVector::ZeroVector;
	StuckTimer = 0.0f;
	StuckCheckInterval = 0.3f;
	bAvoidingObstacle = false;
	AvoidanceDirection = 1.0f;
	AvoidanceTimer = 0.0f;
}

void ASkylandersMinion::BeginPlay()
{
	Super::BeginPlay();

	// Friendly minions: player and projectiles pass through
	if (Team == ETowerTeam::Friendly)
	{
		// Overlap with Pawn channel only — player walks through
		// Ground is WorldStatic so floor collision is unaffected
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	}

	// Configure stats based on type
	if (MinionType == EMinionType::Ranged)
	{
		MaxHealth = 80.0f;
		CurrentHealth = 80.0f;
		AttackDamage = 18.0f;
		AttackRange = 600.0f;
		MoveSpeed = 300.0f;
		XPReward = 20.0f;
		CoinReward = 12;

		// Ranged minions are slightly taller/thinner
		if (BodyMesh)
		{
			BodyMesh->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.7f));
		}
	}

	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;

	// Init stuck detection position
	LastPosition = GetActorLocation();

	// Load health bar widget
	UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UserInterface/WBP_EnemyHealthBar.WBP_EnemyHealthBar_C"));
	if (WidgetClass && HealthBarComp)
	{
		HealthBarComp->SetWidgetClass(WidgetClass);
		HealthBarComp->InitWidget();
	}

	// Color based on team
	if (BodyMesh)
	{
		UMaterialInterface* DefaultMat = BodyMesh->GetMaterial(0);
		if (DefaultMat)
		{
			UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(DefaultMat, this);
			if (Team == ETowerTeam::Friendly)
			{
				// Blue team minions - lighter blue
				DynMat->SetVectorParameterValue(FName("Color"), FLinearColor(0.2f, 0.4f, 1.0f));
			}
			else
			{
				// Red team minions - lighter red
				DynMat->SetVectorParameterValue(FName("Color"), FLinearColor(1.0f, 0.3f, 0.2f));
			}
			BodyMesh->SetMaterial(0, DynMat);
		}
	}

	FTimerHandle InitTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(InitTimerHandle, this, &ASkylandersMinion::UpdateHealthBar, 0.1f, false);
}

void ASkylandersMinion::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bDead) return;

	// Distance-based health bar rendering
	if (HealthBarComp)
	{
		APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		if (Player)
		{
			float DistToPlayer = FVector::Dist(GetActorLocation(), Player->GetActorLocation());
			HealthBarComp->SetVisibility(DistToPlayer < 2500.0f);
		}
	}

	if (AttackTimer > 0.0f)
	{
		AttackTimer -= DeltaTime;
	}

	UpdateAI(DeltaTime);
}

void ASkylandersMinion::UpdateAI(float DeltaTime)
{
	// Tick forced aggro timer
	if (ForcedAggroTimer > 0.0f)
	{
		ForcedAggroTimer -= DeltaTime;
		if (ForcedAggroTimer <= 0.0f)
		{
			ForcedAggroTarget = nullptr;
		}
	}

	// Stuck detection
	StuckTimer += DeltaTime;
	if (StuckTimer >= StuckCheckInterval)
	{
		float DistMoved = FVector::Dist2D(GetActorLocation(), LastPosition);
		if (DistMoved < 10.0f && !bAvoidingObstacle)
		{
			// Barely moved = stuck on something
			bAvoidingObstacle = true;
			// Pick a random side to go around (perpendicular to desired direction)
			AvoidanceDirection = FMath::RandBool() ? 1.0f : -1.0f;
			AvoidanceTimer = 1.5f; // Avoid for 1.5 seconds
		}
		LastPosition = GetActorLocation();
		StuckTimer = 0.0f;
	}

	// Tick avoidance timer
	if (bAvoidingObstacle)
	{
		AvoidanceTimer -= DeltaTime;
		if (AvoidanceTimer <= 0.0f)
		{
			bAvoidingObstacle = false;
		}
	}

	// Find a target
	FindTarget();

	if (CurrentTarget)
	{
		float DistToTarget = FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());

		if (DistToTarget <= AttackRange)
		{
			// In range - stop moving and face the target, reset avoidance
			bAvoidingObstacle = false;
			FaceTarget(CurrentTarget);

			if (AttackTimer <= 0.0f)
			{
				AttackTarget();
				AttackTimer = AttackCooldown;
			}
		}
		else if (DistToTarget <= AggroRange)
		{
			// Close enough to chase
			FVector DesiredDir = (CurrentTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			FVector MoveDir = DesiredDir;

			if (bAvoidingObstacle)
			{
				// Rotate desired direction 90 degrees to go around the obstacle
				FVector AvoidDir = FVector(-DesiredDir.Y * AvoidanceDirection, DesiredDir.X * AvoidanceDirection, 0.0f);
				// Blend: mostly avoidance, some forward progress
				MoveDir = (AvoidDir * 0.7f + DesiredDir * 0.3f).GetSafeNormal();
			}

			AddMovementInput(MoveDir, 1.0f);
		}
		else
		{
			// Target is a structure far away - march along the lane waypoints
			FVector DesiredDir = (GetLaneGoal() - GetActorLocation()).GetSafeNormal2D();
			FVector MoveDir = DesiredDir;

			if (bAvoidingObstacle)
			{
				FVector AvoidDir = FVector(-DesiredDir.Y * AvoidanceDirection, DesiredDir.X * AvoidanceDirection, 0.0f);
				MoveDir = (AvoidDir * 0.7f + DesiredDir * 0.3f).GetSafeNormal();
			}

			AddMovementInput(MoveDir, 1.0f);
		}
	}
	else
	{
		// No target - march along the lane waypoints toward the enemy base
		FVector LaneGoal = GetLaneGoal();
		float DistToLaneTarget = FVector::Dist2D(GetActorLocation(), LaneGoal);
		if (DistToLaneTarget > 50.0f)
		{
			FVector DesiredDir = (LaneGoal - GetActorLocation()).GetSafeNormal2D();
			FVector MoveDir = DesiredDir;

			if (bAvoidingObstacle)
			{
				FVector AvoidDir = FVector(-DesiredDir.Y * AvoidanceDirection, DesiredDir.X * AvoidanceDirection, 0.0f);
				MoveDir = (AvoidDir * 0.7f + DesiredDir * 0.3f).GetSafeNormal();
			}

			AddMovementInput(MoveDir, 1.0f);
		}
	}
}

FVector ASkylandersMinion::GetLaneGoal()
{
	// No path set — legacy straight march at the base
	if (LaneWaypoints.Num() == 0)
	{
		return LaneTargetPoint;
	}

	// Advance past any waypoints we've already reached (2D, ignore height).
	// Never advance past the last one (the enemy base) so minions settle there.
	while (CurrentWaypointIndex < LaneWaypoints.Num() - 1 &&
		FVector::Dist2D(GetActorLocation(), LaneWaypoints[CurrentWaypointIndex]) < 300.0f)
	{
		CurrentWaypointIndex++;
	}
	return LaneWaypoints[CurrentWaypointIndex];
}

void ASkylandersMinion::FindTarget()
{
	CurrentTarget = nullptr;

	// Priority 0: Forced aggro (player attacked this minion or attacked nearby)
	if (ForcedAggroTarget && ForcedAggroTimer > 0.0f)
	{
		ASkylandersCharacter* Player = Cast<ASkylandersCharacter>(ForcedAggroTarget);
		if (Player && Player->CurrentHealth > 0.0f)
		{
			float DistToPlayer = FVector::Dist(GetActorLocation(), Player->GetActorLocation());
			// Break aggro if player is too far away (leash back to lane)
			if (DistToPlayer < AggroRange)
			{
				CurrentTarget = ForcedAggroTarget;
				return;
			}
		}
		// Target died, invalid, or too far - clear forced aggro
		ForcedAggroTarget = nullptr;
		ForcedAggroTimer = 0.0f;
	}

	// Priority 1: Enemy minions (within aggro range)
	float ClosestDist = AggroRange;
	TArray<AActor*> AllMinions;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersMinion::StaticClass(), AllMinions);

	for (AActor* Actor : AllMinions)
	{
		ASkylandersMinion* OtherMinion = Cast<ASkylandersMinion>(Actor);
		if (OtherMinion && OtherMinion != this && OtherMinion->Team != Team && !OtherMinion->bDead)
		{
			float Dist = FVector::Dist(GetActorLocation(), Actor->GetActorLocation());
			if (Dist < ClosestDist)
			{
				ClosestDist = Dist;
				CurrentTarget = Actor;
			}
		}
	}
	if (CurrentTarget) return;

	// Priority 2: Player (enemy minions only, within aggro range)
	if (Team == ETowerTeam::Enemy)
	{
		APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		if (Player)
		{
			ASkylandersCharacter* SkyChar = Cast<ASkylandersCharacter>(Player);
			if (SkyChar && SkyChar->CurrentHealth > 0.0f)
			{
				float Dist = FVector::Dist(GetActorLocation(), Player->GetActorLocation());
				if (Dist < AggroRange)
				{
					CurrentTarget = Player;
					return;
				}
			}
		}
	}

	// Priority 3: Enemy towers (no range limit - always march toward them)
	// Only target vulnerable towers (protecting tower must be destroyed first)
	float ClosestStructDist = 99999.0f;
	TArray<AActor*> AllTowers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersTower::StaticClass(), AllTowers);

	for (AActor* Actor : AllTowers)
	{
		ASkylandersTower* Tower = Cast<ASkylandersTower>(Actor);
		if (Tower && Tower->Team != Team && !Tower->bDestroyed && Tower->IsVulnerable())
		{
			float Dist = FVector::Dist(GetActorLocation(), Actor->GetActorLocation());
			if (Dist < ClosestStructDist)
			{
				ClosestStructDist = Dist;
				CurrentTarget = Actor;
			}
		}
	}
	if (CurrentTarget) return;

	// Priority 4: Enemy titan (no range limit, but only if vulnerable - protecting tower must be destroyed)
	TArray<AActor*> AllTitans;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersTitan::StaticClass(), AllTitans);

	for (AActor* Actor : AllTitans)
	{
		ASkylandersTitan* Titan = Cast<ASkylandersTitan>(Actor);
		if (Titan && Titan->Team != Team && !Titan->bDead && Titan->IsVulnerable())
		{
			float Dist = FVector::Dist(GetActorLocation(), Actor->GetActorLocation());
			if (Dist < ClosestStructDist)
			{
				ClosestStructDist = Dist;
				CurrentTarget = Actor;
			}
		}
	}
}

void ASkylandersMinion::AttackTarget()
{
	if (!CurrentTarget) return;

	// Visual: beam for ranged, debug line for melee
	FVector MyLoc = GetActorLocation() + FVector(0, 0, 20.0f);
	FVector TargetLoc = CurrentTarget->GetActorLocation() + FVector(0, 0, 20.0f);

	FColor AttackColor = (Team == ETowerTeam::Friendly) ? FColor::Cyan : FColor::Orange;
	float LineThickness = (MinionType == EMinionType::Ranged) ? 2.0f : 3.0f;
	DrawDebugLine(GetWorld(), MyLoc, TargetLoc, AttackColor, false, 0.15f, 0, LineThickness);

	// Deal damage based on target type
	ASkylandersMinion* EnemyMinion = Cast<ASkylandersMinion>(CurrentTarget);
	if (EnemyMinion)
	{
		EnemyMinion->TakeDamage_Custom(AttackDamage, this);
		return;
	}

	ASkylandersEnemy* JungleEnemy = Cast<ASkylandersEnemy>(CurrentTarget);
	if (JungleEnemy)
	{
		JungleEnemy->TakeDamage_Custom(AttackDamage, this);
		return;
	}

	ASkylandersTower* Tower = Cast<ASkylandersTower>(CurrentTarget);
	if (Tower)
	{
		Tower->TakeDamage_Custom(AttackDamage, this);
		return;
	}

	ASkylandersTitan* Titan = Cast<ASkylandersTitan>(CurrentTarget);
	if (Titan)
	{
		Titan->TakeDamage_Custom(AttackDamage, this);
		return;
	}

	ASkylandersCharacter* Player = Cast<ASkylandersCharacter>(CurrentTarget);
	if (Player)
	{
		Player->TakeDamage_Custom(AttackDamage);
		return;
	}
}

void ASkylandersMinion::TakeDamage_Custom(float DamageAmount, AActor* DamageCauser)
{
	if (bDead) return;

	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);

	// If a player attacked us, force aggro this minion AND nearby allies onto the player
	APawn* PlayerPawn = Cast<APawn>(DamageCauser ? DamageCauser->GetInstigator() : nullptr);
	if (!PlayerPawn) PlayerPawn = Cast<APawn>(DamageCauser);
	ASkylandersCharacter* AttackingPlayer = Cast<ASkylandersCharacter>(PlayerPawn);
	if (AttackingPlayer)
	{
		// Check if we're currently engaged with enemy minions - if so, don't switch to player
		bool bFightingMinions = false;
		TArray<AActor*> NearbyMinions;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersMinion::StaticClass(), NearbyMinions);
		for (AActor* Actor : NearbyMinions)
		{
			ASkylandersMinion* OtherMinion = Cast<ASkylandersMinion>(Actor);
			if (OtherMinion && OtherMinion != this && OtherMinion->Team != Team && !OtherMinion->bDead)
			{
				float Dist = FVector::Dist(GetActorLocation(), OtherMinion->GetActorLocation());
				if (Dist < AggroRange)
				{
					bFightingMinions = true;
					break;
				}
			}
		}

		// NOTE: attacking MINIONS under a tower does NOT draw tower fire in SMITE —
		// only damaging an enemy god in range does (handled in SkylandersEnemyGod).
		// Pulling aggro here meant last-hitting the wave got the player towered even
		// with friendly minions present, so the tower call was removed.

		if (!bFightingMinions)
		{
			ForcedAggroTarget = AttackingPlayer;
			ForcedAggroTimer = 4.0f; // Aggro player for 4 seconds

			// Alert nearby same-team minions (only if they aren't fighting enemy minions)
			for (AActor* Actor : NearbyMinions)
			{
				ASkylandersMinion* Ally = Cast<ASkylandersMinion>(Actor);
				if (Ally && Ally != this && Ally->Team == Team && !Ally->bDead)
				{
					float Dist = FVector::Dist(GetActorLocation(), Ally->GetActorLocation());
					if (Dist < 400.0f) // Nearby allies also aggro
					{
						// Check if this ally is fighting enemy minions
						bool bAllyFightingMinions = false;
						for (AActor* CheckActor : NearbyMinions)
						{
							ASkylandersMinion* CheckMinion = Cast<ASkylandersMinion>(CheckActor);
							if (CheckMinion && CheckMinion != Ally && CheckMinion->Team != Ally->Team && !CheckMinion->bDead)
							{
								float CheckDist = FVector::Dist(Ally->GetActorLocation(), CheckMinion->GetActorLocation());
								if (CheckDist < Ally->AggroRange)
								{
									bAllyFightingMinions = true;
									break;
								}
							}
						}

						if (!bAllyFightingMinions)
						{
							Ally->ForcedAggroTarget = AttackingPlayer;
							Ally->ForcedAggroTimer = 3.0f;
						}
					}
				}
			}
		}
	}

	// Spawn damage number
	FVector NumberLoc = GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASkylandersDamageNumber* DmgNum = GetWorld()->SpawnActor<ASkylandersDamageNumber>(
		ASkylandersDamageNumber::StaticClass(), NumberLoc, FRotator::ZeroRotator, SpawnParams);
	if (DmgNum)
	{
		DmgNum->SetDamageNumber(DamageAmount, FColor::Yellow, false);
	}

	UpdateHealthBar();

	if (CurrentHealth <= 0.0f)
	{
		Die();
	}
}

void ASkylandersMinion::Die()
{
	bDead = true;

	// Reward player if enemy minion killed
	if (Team == ETowerTeam::Enemy)
	{
		ASkylandersCharacter* Player = Cast<ASkylandersCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
		if (Player)
		{
			Player->AddXP(XPReward);
			Player->AddCoins(CoinReward);
			Player->CreepScore++;
		}
	}

	// Destroy after short delay (let damage number show)
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);

	FTimerHandle DestroyTimer;
	GetWorld()->GetTimerManager().SetTimer(DestroyTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		Destroy();
	}), 0.5f, false);
}

void ASkylandersMinion::FaceTarget(AActor* Target)
{
	if (!Target) return;
	FVector Dir = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (!Dir.IsNearlyZero())
	{
		SetActorRotation(Dir.Rotation());
	}
}

void ASkylandersMinion::UpdateHealthBar()
{
	if (!HealthBarComp) return;

	UUserWidget* Widget = HealthBarComp->GetWidget();
	if (!Widget) return;

	if (!bHealthBarInitialized)
	{
		CachedNameText = Cast<UTextBlock>(Widget->WidgetTree->FindWidget(FName("EnemyNameText")));
		CachedHealthBar = Cast<UProgressBar>(Widget->WidgetTree->FindWidget(FName("EnemyHealthBar")));
		CachedHealthText = Cast<UTextBlock>(Widget->WidgetTree->FindWidget(FName("EnemyHealthText")));

		if (CachedHealthBar)
		{
			bHealthBarInitialized = true;

			// Hide name and health text for minimal minion display
			if (CachedNameText)
			{
				CachedNameText->SetVisibility(ESlateVisibility::Collapsed);
			}
			if (CachedHealthText)
			{
				CachedHealthText->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		else
		{
			return;
		}
	}

	// Only update the progress bar - no name or health text for minions
	float Pct = (MaxHealth > 0.0f) ? (CurrentHealth / MaxHealth) : 0.0f;
	CachedHealthBar->SetPercent(Pct);

	// Bar color based on team: blue for friendly, red for enemy
	// SMITE-style: solid green for allied minions, solid red for enemy — no
	// health-percentage color shift (the bar only shrinks).
	FLinearColor BarColor = (Team == ETowerTeam::Friendly)
		? FLinearColor(0.15f, 0.85f, 0.2f)   // green (your team)
		: FLinearColor(0.9f, 0.15f, 0.15f);  // red (enemy)
	CachedHealthBar->SetFillColorAndOpacity(BarColor);
}

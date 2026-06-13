// Skylanders Conquest - Enemy with AI Implementation

#include "SkylandersEnemy.h"
#include "SkylandersCharacter.h"
#include "SkylandersTower.h"
#include "SkylandersMinion.h"
#include "EnemyHealthBarWidget.h"
#include "SkylandersDamageNumber.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

ASkylandersEnemy::ASkylandersEnemy()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// Capsule for collision
	GetCapsuleComponent()->InitCapsuleSize(35.f, 60.0f);
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	GetCapsuleComponent()->SetNotifyRigidBodyCollision(true);

	// Movement - enable for AI chasing
	GetCharacterMovement()->GravityScale = 1.0f;
	GetCharacterMovement()->MaxWalkSpeed = 300.0f;
	GetCharacterMovement()->bOrientRotationToMovement = false; // We handle rotation manually
	GetCharacterMovement()->BrakingDecelerationWalking = 1000.0f;

	// Create a visible body (cylinder placeholder)
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	BodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -60.0f));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CylinderMesh.Object);
		BodyMesh->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.2f));
	}

	// Health bar widget component
	HealthBarComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
	HealthBarComp->SetupAttachment(RootComponent);
	HealthBarComp->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	HealthBarComp->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarComp->SetDrawSize(FVector2D(200.0f, 60.0f));
	HealthBarComp->SetPivot(FVector2D(0.5f, 1.0f));

	// Stats
	MaxHealth = 200.0f;
	CurrentHealth = 200.0f;
	EnemyName = TEXT("Training Dummy");
	RespawnDelay = 5.0f;
	AttackDamage = 15.0f;
	XPReward = 50.0f;
	CoinReward = 10;

	// AI
	AggroRange = 800.0f;
	AttackRange = 120.0f;
	LeashRange = 1500.0f;
	ChaseSpeed = 300.0f;
	AttackCooldown = 1.5f;
	AttackTimer = 0.0f;
	CurrentState = EEnemyState::Idle;
	TargetPlayer = nullptr;

	// Cached widget refs
	CachedHealthBar = nullptr;
	CachedNameText = nullptr;
	CachedHealthText = nullptr;
}

void ASkylandersEnemy::BeginPlay()
{
	Super::BeginPlay();

	SpawnLocation = GetActorLocation();
	SpawnRotation = GetActorRotation();
	bHealthBarInitialized = false;

	// Load widget Blueprint class
	UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UserInterface/WBP_EnemyHealthBar.WBP_EnemyHealthBar_C"));
	if (WidgetClass && HealthBarComp)
	{
		HealthBarComp->SetWidgetClass(WidgetClass);
		HealthBarComp->InitWidget();
	}

	// Set chase speed
	GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;

	UE_LOG(LogTemp, Log, TEXT("Enemy '%s' spawned with %.0f HP, Aggro: %.0f, Leash: %.0f"), *EnemyName, MaxHealth, AggroRange, LeashRange);

	FTimerHandle InitTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(InitTimerHandle, this, &ASkylandersEnemy::UpdateHealthBar, 0.1f, false);
}

void ASkylandersEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CurrentHealth > 0.0f)
	{
		UpdateAI(DeltaTime);
	}
}

// ========== AI STATE MACHINE ==========

ASkylandersCharacter* ASkylandersEnemy::FindPlayer()
{
	// Cache the player reference
	if (!TargetPlayer)
	{
		APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		TargetPlayer = Cast<ASkylandersCharacter>(PlayerPawn);
	}
	return TargetPlayer;
}

void ASkylandersEnemy::FaceTarget(AActor* Target)
{
	if (!Target) return;

	FVector Direction = Target->GetActorLocation() - GetActorLocation();
	Direction.Z = 0.0f; // Only rotate on yaw
	if (Direction.SizeSquared() > 1.0f)
	{
		FRotator TargetRot = Direction.Rotation();
		FRotator NewRot = FMath::RInterpTo(GetActorRotation(), TargetRot, GetWorld()->GetDeltaSeconds(), 10.0f);
		SetActorRotation(NewRot);
	}
}

void ASkylandersEnemy::UpdateAI(float DeltaTime)
{
	ASkylandersCharacter* Player = FindPlayer();
	if (!Player || Player->CurrentHealth <= 0.0f)
	{
		// No valid target, go idle
		if (CurrentState != EEnemyState::Idle && CurrentState != EEnemyState::Returning)
		{
			CurrentState = EEnemyState::Returning;
		}
	}

	// Tick attack timer
	if (AttackTimer > 0.0f)
	{
		AttackTimer -= DeltaTime;
	}

	float DistToPlayer = Player ? FVector::Dist(GetActorLocation(), Player->GetActorLocation()) : 99999.0f;
	float DistToSpawn = FVector::Dist(GetActorLocation(), SpawnLocation);

	switch (CurrentState)
	{
	case EEnemyState::Idle:
	{
		// Check for aggro
		if (Player && DistToPlayer <= AggroRange && Player->CurrentHealth > 0.0f)
		{
			CurrentState = EEnemyState::Chasing;
			UE_LOG(LogTemp, Log, TEXT("Enemy '%s' aggro on player! Distance: %.0f"), *EnemyName, DistToPlayer);
		}
		break;
	}

	case EEnemyState::Chasing:
	{
		if (!Player || Player->CurrentHealth <= 0.0f)
		{
			CurrentState = EEnemyState::Returning;
			break;
		}

		// Check leash
		if (DistToSpawn > LeashRange)
		{
			CurrentState = EEnemyState::Returning;
			UE_LOG(LogTemp, Log, TEXT("Enemy '%s' leashed! Returning to spawn."), *EnemyName);
			break;
		}

		// Check if in attack range
		if (DistToPlayer <= AttackRange)
		{
			CurrentState = EEnemyState::Attacking;
			break;
		}

		// Chase - move toward player
		FaceTarget(Player);
		FVector MoveDir = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		AddMovementInput(MoveDir, 1.0f);
		break;
	}

	case EEnemyState::Attacking:
	{
		if (!Player || Player->CurrentHealth <= 0.0f)
		{
			CurrentState = EEnemyState::Returning;
			break;
		}

		// Check leash
		if (DistToSpawn > LeashRange)
		{
			CurrentState = EEnemyState::Returning;
			break;
		}

		// If player moved out of attack range, chase again
		if (DistToPlayer > AttackRange * 1.3f) // Small buffer to prevent flickering
		{
			CurrentState = EEnemyState::Chasing;
			break;
		}

		// Face the player
		FaceTarget(Player);

		// Attack when timer is ready
		if (AttackTimer <= 0.0f)
		{
			AttackTimer = AttackCooldown;
			Player->TakeDamage_Custom(AttackDamage);
			UE_LOG(LogTemp, Log, TEXT("Enemy '%s' attacks player for %.0f damage!"), *EnemyName, AttackDamage);
		}
		break;
	}

	case EEnemyState::Returning:
	{
		// Move back to spawn
		if (DistToSpawn < 50.0f)
		{
			// Arrived at spawn
			CurrentState = EEnemyState::Idle;
			SetActorRotation(SpawnRotation);

			// Heal to full on return
			CurrentHealth = MaxHealth;
			UpdateHealthBar();
			UE_LOG(LogTemp, Log, TEXT("Enemy '%s' returned to spawn, healed to full."), *EnemyName);
			break;
		}

		FVector ReturnDir = (SpawnLocation - GetActorLocation()).GetSafeNormal2D();
		AddMovementInput(ReturnDir, 1.0f);

		// Face movement direction
		FRotator ReturnRot = ReturnDir.Rotation();
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), ReturnRot, DeltaTime, 10.0f));

		// Re-aggro if player is close while returning
		if (Player && DistToPlayer <= AggroRange * 0.6f && Player->CurrentHealth > 0.0f)
		{
			CurrentState = EEnemyState::Chasing;
		}
		break;
	}
	}
}

// ========== DAMAGE AND DEATH ==========

void ASkylandersEnemy::TakeDamage_Custom(float DamageAmount, AActor* DamageCauser)
{
	if (CurrentHealth <= 0.0f) return;

	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);

	// Spawn floating damage number
	FVector NumberLoc = GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASkylandersDamageNumber* DmgNum = GetWorld()->SpawnActor<ASkylandersDamageNumber>(
		ASkylandersDamageNumber::StaticClass(), NumberLoc, FRotator::ZeroRotator, SpawnParams);
	if (DmgNum)
	{
		bool bBigHit = DamageAmount >= 60.0f;
		FColor DmgColor = bBigHit ? FColor(255, 140, 0) : FColor::Yellow;
		DmgNum->SetDamageNumber(DamageAmount, DmgColor, bBigHit);
	}

	// Getting hit forces aggro (even from outside aggro range)
	if (CurrentState == EEnemyState::Idle || CurrentState == EEnemyState::Returning)
	{
		ASkylandersCharacter* Player = FindPlayer();
		if (Player)
		{
			CurrentState = EEnemyState::Chasing;
			UE_LOG(LogTemp, Log, TEXT("Enemy '%s' aggro'd by damage!"), *EnemyName);
		}
	}

	// SMITE tower aggro rule: if the damage causer is a player, notify enemy towers
	// so they switch aggro to the player temporarily
	if (DamageCauser)
	{
		// DamageCauser might be a projectile - get the actual player via GetInstigator
		ASkylandersCharacter* AttackingPlayer = Cast<ASkylandersCharacter>(DamageCauser);
		if (!AttackingPlayer)
		{
			APawn* DmgInstigator = DamageCauser->GetInstigator();
			AttackingPlayer = Cast<ASkylandersCharacter>(DmgInstigator);
		}
		if (AttackingPlayer)
		{
			// Notify enemy towers
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

			// SMITE minion aggro rule: nearby enemy minions aggro the player
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

void ASkylandersEnemy::Die()
{
	UE_LOG(LogTemp, Warning, TEXT("Enemy '%s' died!"), *EnemyName);

	// Reward player with XP and coins
	ASkylandersCharacter* Player = FindPlayer();
	if (Player)
	{
		Player->AddXP(XPReward);
		Player->AddCoins(CoinReward);
		UE_LOG(LogTemp, Log, TEXT("Player rewarded: %.0f XP, %d coins"), XPReward, CoinReward);
	}

	CurrentState = EEnemyState::Idle;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);

	GetWorld()->GetTimerManager().SetTimer(RespawnTimerHandle, this, &ASkylandersEnemy::RespawnEnemy, RespawnDelay, false);
}

void ASkylandersEnemy::RespawnEnemy()
{
	UE_LOG(LogTemp, Log, TEXT("Enemy '%s' respawned!"), *EnemyName);

	CurrentHealth = MaxHealth;
	CurrentState = EEnemyState::Idle;
	TargetPlayer = nullptr;
	AttackTimer = 0.0f;

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorLocation(SpawnLocation);
	SetActorRotation(SpawnRotation);

	CachedHealthBar = nullptr;
	CachedNameText = nullptr;
	CachedHealthText = nullptr;
	bHealthBarInitialized = false;

	UpdateHealthBar();
}

// ========== HEALTH BAR ==========

void ASkylandersEnemy::UpdateHealthBar()
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

			FProgressBarStyle BarStyle = CachedHealthBar->GetWidgetStyle();
			BarStyle.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.3f, 0.02f, 0.02f, 1.0f));
			CachedHealthBar->SetWidgetStyle(BarStyle);
		}
		else
		{
			return;
		}
	}

	CachedNameText->SetText(FText::FromString(EnemyName));

	float Pct = (MaxHealth > 0.0f) ? (CurrentHealth / MaxHealth) : 0.0f;
	CachedHealthBar->SetPercent(Pct);

	FLinearColor BarColor = FLinearColor::Green;
	if (Pct < 0.3f) BarColor = FLinearColor::Red;
	else if (Pct < 0.6f) BarColor = FLinearColor::Yellow;
	CachedHealthBar->SetFillColorAndOpacity(BarColor);

	FString HealthStr = FString::Printf(TEXT("%.0f / %.0f"), CurrentHealth, MaxHealth);
	CachedHealthText->SetText(FText::FromString(HealthStr));

	UEnemyHealthBarWidget* EnemyWidget = Cast<UEnemyHealthBarWidget>(Widget);
	if (EnemyWidget)
	{
		EnemyWidget->SetHealth(CurrentHealth, MaxHealth);
		EnemyWidget->SetEnemyName(EnemyName);
	}
}

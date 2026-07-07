// Skylanders Conquest - Tower Implementation

#include "SkylandersTower.h"
#include "SkylandersKillFeedWidget.h"
#include "SkylandersCharacter.h"
#include "SkylandersEnemy.h"
#include "SkylandersMinion.h"
#include "SkylandersEnemyGod.h"
#include "SkylandersDamageNumber.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

ASkylandersTower::ASkylandersTower()
{
	PrimaryActorTick.bCanEverTick = true;

	// Base platform
	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	RootComponent = BaseMesh;
	BaseMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		BaseMesh->SetStaticMesh(CylinderMesh.Object);
		BaseMesh->SetRelativeScale3D(FVector(1.5f, 1.5f, 0.3f)); // Wide flat base
	}

	// Tower body (tall cylinder on top)
	TowerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TowerMesh"));
	TowerMesh->SetupAttachment(BaseMesh);
	TowerMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 200.0f));
	TowerMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TowerMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	if (CylinderMesh.Succeeded())
	{
		TowerMesh->SetStaticMesh(CylinderMesh.Object);
		TowerMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 2.0f)); // Tall narrow tower
	}

	// Health bar
	HealthBarComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
	HealthBarComp->SetupAttachment(BaseMesh);
	HealthBarComp->SetRelativeLocation(FVector(0.0f, 0.0f, 450.0f));
	HealthBarComp->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarComp->SetDrawSize(FVector2D(180.0f, 40.0f));
	HealthBarComp->SetPivot(FVector2D(0.5f, 1.0f));

	// Stats
	MaxHealth = 2000.0f;
	CurrentHealth = 2000.0f;
	TowerName = TEXT("Tower");
	AttackDamage = 50.0f;
	AttackRange = 900.0f;
	AttackCooldown = 1.0f;
	Team = ETowerTeam::Friendly;

	ProtectingTower = nullptr;
	AttackTimer = 0.0f;
	CurrentTarget = nullptr;
	bDestroyed = false;
	ForcedPlayerTarget = nullptr;
	ForcedPlayerTimer = 0.0f;
	bIsPhoenix = false;
	PhoenixRespawnTime = 180.0f;
	bHealthBarInitialized = false;
	CachedHealthBar = nullptr;
	CachedNameText = nullptr;
	CachedHealthText = nullptr;

	// Audio
	AttackSound = nullptr;
	DestroySound = nullptr;
	PhoenixRespawnSound = nullptr;
}

void ASkylandersTower::BeginPlay()
{
	Super::BeginPlay();

	// All towers overlap with WorldDynamic (projectiles) — damage handled via OnOverlap
	// Player (Pawn channel) is still blocked, so characters can't walk through towers
	BaseMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	BaseMesh->SetGenerateOverlapEvents(true);
	TowerMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	TowerMesh->SetGenerateOverlapEvents(true);

	// Load health bar widget
	UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UserInterface/WBP_EnemyHealthBar.WBP_EnemyHealthBar_C"));
	if (WidgetClass && HealthBarComp)
	{
		HealthBarComp->SetWidgetClass(WidgetClass);
		HealthBarComp->InitWidget();
	}

	// Color the tower based on team
	if (BaseMesh)
	{
		UMaterialInterface* DefaultMat = BaseMesh->GetMaterial(0);
		if (DefaultMat)
		{
			UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(DefaultMat, this);
			if (Team == ETowerTeam::Friendly)
			{
				DynMat->SetVectorParameterValue(FName("Color"), FLinearColor(0.1f, 0.3f, 1.0f)); // Blue
			}
			else
			{
				DynMat->SetVectorParameterValue(FName("Color"), FLinearColor(1.0f, 0.15f, 0.1f)); // Red
			}
			BaseMesh->SetMaterial(0, DynMat);
			TowerMesh->SetMaterial(0, DynMat);
		}
	}

	// Phoenix override: golden/orange color
	if (bIsPhoenix && BaseMesh)
	{
		UMaterialInterface* PhxMat = BaseMesh->GetMaterial(0);
		if (!PhxMat) PhxMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
		if (PhxMat)
		{
			UMaterialInstanceDynamic* PhxDynMat = UMaterialInstanceDynamic::Create(PhxMat, this);
			FLinearColor PhxColor = (Team == ETowerTeam::Friendly)
				? FLinearColor(0.2f, 0.4f, 1.0f) : FLinearColor(0.9f, 0.4f, 0.05f);
			PhxDynMat->SetVectorParameterValue(FName("Color"), PhxColor);
			BaseMesh->SetMaterial(0, PhxDynMat);
			TowerMesh->SetMaterial(0, PhxDynMat);
		}
	}

	FTimerHandle InitTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(InitTimerHandle, this, &ASkylandersTower::UpdateHealthBar, 0.1f, false);

	UE_LOG(LogTemp, Log, TEXT("%s '%s' (%s) spawned with %.0f HP, Range: %.0f"),
		bIsPhoenix ? TEXT("Phoenix") : TEXT("Tower"),
		*TowerName, Team == ETowerTeam::Friendly ? TEXT("Friendly") : TEXT("Enemy"), MaxHealth, AttackRange);
}

void ASkylandersTower::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bDestroyed) return;

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

	// Tick attack timer
	if (AttackTimer > 0.0f)
	{
		AttackTimer -= DeltaTime;
	}

	// Tick forced player aggro timer
	if (ForcedPlayerTimer > 0.0f)
	{
		ForcedPlayerTimer -= DeltaTime;
		if (ForcedPlayerTimer <= 0.0f)
		{
			ForcedPlayerTarget = nullptr;
			ForcedPlayerTimer = 0.0f;
		}
	}

	// Find and attack targets
	FindTarget();

	if (CurrentTarget && AttackTimer <= 0.0f)
	{
		AttackTarget();
		AttackTimer = AttackCooldown;
	}

	// Draw range ring above the ground (DepthPriority=1 = always on top)
	FVector RingCenter = GetActorLocation();
	RingCenter.Z = 12.0f; // hug the ground so the ring doesn't float or block UI
	FColor RingColor = (Team == ETowerTeam::Friendly) ? FColor(20, 40, 160) : FColor::Red; // friendly dark blue, enemy red
	int32 Segments = 64;
	float AngleStep = 2.0f * PI / Segments;
	for (int32 i = 0; i < Segments; i++)
	{
		float A1 = i * AngleStep;
		float A2 = (i + 1) * AngleStep;
		FVector P1 = RingCenter + FVector(FMath::Cos(A1) * AttackRange, FMath::Sin(A1) * AttackRange, 0.0f);
		FVector P2 = RingCenter + FVector(FMath::Cos(A2) * AttackRange, FMath::Sin(A2) * AttackRange, 0.0f);
		DrawDebugLine(GetWorld(), P1, P2, RingColor, false, 0.05f, 1, 4.0f);
	}
}

bool ASkylandersTower::IsVulnerable() const
{
	// Tower can only be damaged if protecting tower is destroyed or not assigned
	if (!ProtectingTower) return true;
	return ProtectingTower->bDestroyed;
}

void ASkylandersTower::FindTarget()
{
	CurrentTarget = nullptr;
	float ClosestDist = AttackRange;

	if (Team == ETowerTeam::Friendly)
	{
		// Friendly tower: prioritize enemy minions, then jungle enemies

		// Check enemy minions first
		TArray<AActor*> AllMinions;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersMinion::StaticClass(), AllMinions);
		for (AActor* Actor : AllMinions)
		{
			ASkylandersMinion* Minion = Cast<ASkylandersMinion>(Actor);
			if (Minion && Minion->Team == ETowerTeam::Enemy && !Minion->bDead)
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

		// Then jungle enemies
		TArray<AActor*> AllEnemies;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersEnemy::StaticClass(), AllEnemies);
		for (AActor* Actor : AllEnemies)
		{
			ASkylandersEnemy* Enemy = Cast<ASkylandersEnemy>(Actor);
			if (Enemy && Enemy->CurrentHealth > 0.0f)
			{
				float Dist = FVector::Dist(GetActorLocation(), Actor->GetActorLocation());
				if (Dist < ClosestDist)
				{
					ClosestDist = Dist;
					CurrentTarget = Actor;
				}
			}
		}

		// Check for enemy god (lowest priority for friendly tower)
		if (!CurrentTarget)
		{
			TArray<AActor*> AllGods;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersEnemyGod::StaticClass(), AllGods);
			for (AActor* Actor : AllGods)
			{
				ASkylandersEnemyGod* God = Cast<ASkylandersEnemyGod>(Actor);
				// Only enemy-team gods — never shoot the player's ally gods
				if (God && God->Team == ETowerTeam::Enemy && God->CurrentState != EGodAIState::Dead)
				{
					float Dist = FVector::Dist(GetActorLocation(), Actor->GetActorLocation());
					if (Dist < AttackRange)
					{
						CurrentTarget = Actor;
						break;
					}
				}
			}
		}
	}
	else
	{
		// Enemy tower: prioritize friendly minions, then player
		// SMITE rule: tower targets minions first; only targets player if no minions in range
		// EXCEPTION: if player attacked an enemy while in tower range, forced aggro overrides minion priority

		// Check forced player aggro first (SMITE aggro rule)
		if (ForcedPlayerTarget && IsValid(ForcedPlayerTarget))
		{
			ASkylandersCharacter* ForcedChar = Cast<ASkylandersCharacter>(ForcedPlayerTarget);
			if (ForcedChar && ForcedChar->CurrentHealth > 0.0f)
			{
				float Dist = FVector::Dist(GetActorLocation(), ForcedPlayerTarget->GetActorLocation());
				if (Dist < AttackRange)
				{
					CurrentTarget = ForcedPlayerTarget;
					return;
				}
			}
			// Player left range or died - clear forced aggro
			ForcedPlayerTarget = nullptr;
			ForcedPlayerTimer = 0.0f;
		}

		// Normal priority: friendly minions first
		TArray<AActor*> AllMinions;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersMinion::StaticClass(), AllMinions);
		for (AActor* Actor : AllMinions)
		{
			ASkylandersMinion* Minion = Cast<ASkylandersMinion>(Actor);
			if (Minion && Minion->Team == ETowerTeam::Friendly && !Minion->bDead)
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

		// No minions - target player
		APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		if (Player)
		{
			ASkylandersCharacter* SkyChar = Cast<ASkylandersCharacter>(Player);
			if (SkyChar && SkyChar->CurrentHealth > 0.0f)
			{
				float Dist = FVector::Dist(GetActorLocation(), Player->GetActorLocation());
				if (Dist < AttackRange)
				{
					CurrentTarget = Player;
				}
			}
		}
	}
}

void ASkylandersTower::AttackTarget()
{
	if (!CurrentTarget) return;

	// Play attack sound
	if (AttackSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, AttackSound, GetActorLocation());
	}

	// Visual beam from tower top to target
	FVector TowerTop = GetActorLocation() + FVector(0.0f, 0.0f, 400.0f);
	FVector TargetLoc = CurrentTarget->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);

	FColor BeamColor = (Team == ETowerTeam::Friendly) ? FColor(20, 40, 160) : FColor::Red;
	DrawDebugLine(GetWorld(), TowerTop, TargetLoc, BeamColor, false, 0.15f, 0, 3.0f);

	// Deal damage - handle all target types
	ASkylandersMinion* Minion = Cast<ASkylandersMinion>(CurrentTarget);
	if (Minion)
	{
		Minion->TakeDamage_Custom(AttackDamage, this);
	}
	else if (Team == ETowerTeam::Friendly)
	{
		ASkylandersEnemy* Enemy = Cast<ASkylandersEnemy>(CurrentTarget);
		if (Enemy)
		{
			Enemy->TakeDamage_Custom(AttackDamage, this);
		}

		ASkylandersEnemyGod* God = Cast<ASkylandersEnemyGod>(CurrentTarget);
		if (God)
		{
			God->TakeDamage_Custom(AttackDamage, this);
		}
	}
	else
	{
		ASkylandersCharacter* Player = Cast<ASkylandersCharacter>(CurrentTarget);
		if (Player)
		{
			Player->TakeDamage_Custom(AttackDamage);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Tower '%s' attacks %s for %.0f damage!"),
		*TowerName, *CurrentTarget->GetName(), AttackDamage);
}

void ASkylandersTower::NotifyPlayerAggro(AActor* Player)
{
	if (bDestroyed || Team != ETowerTeam::Enemy) return;
	if (!Player || !IsValid(Player)) return;

	// Only aggro if the player is actually within tower range
	float Dist = FVector::Dist(GetActorLocation(), Player->GetActorLocation());
	if (Dist > AttackRange) return;

	ForcedPlayerTarget = Player;
	ForcedPlayerTimer = 3.0f;

	UE_LOG(LogTemp, Log, TEXT("Tower '%s' forced aggro on %s for 3 seconds (player attacked enemy in tower range)"),
		*TowerName, *Player->GetName());
}

void ASkylandersTower::TakeDamage_Custom(float DamageAmount, AActor* DamageCauser)
{
	if (bDestroyed) return;

	// Check if tower is protected by outer tower
	if (!IsVulnerable())
	{
		// Show "IMMUNE" text instead of damage
		FVector NumberLoc = GetActorLocation() + FVector(0.0f, 0.0f, 300.0f);
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASkylandersDamageNumber* DmgNum = GetWorld()->SpawnActor<ASkylandersDamageNumber>(
			ASkylandersDamageNumber::StaticClass(), NumberLoc, FRotator::ZeroRotator, SpawnParams);
		if (DmgNum)
		{
			DmgNum->SetTextLabel(TEXT("IMMUNE"), FColor::White);
		}
		return;
	}

	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);

	// Spawn damage number
	FVector NumberLoc = GetActorLocation() + FVector(0.0f, 0.0f, 300.0f);
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASkylandersDamageNumber* DmgNum = GetWorld()->SpawnActor<ASkylandersDamageNumber>(
		ASkylandersDamageNumber::StaticClass(), NumberLoc, FRotator::ZeroRotator, SpawnParams);
	if (DmgNum)
	{
		DmgNum->SetDamageNumber(DamageAmount, FColor::Yellow, DamageAmount >= 60.0f);
	}

	UpdateHealthBar();

	if (CurrentHealth <= 0.0f)
	{
		Die();
	}
}

void ASkylandersTower::Die()
{
	bDestroyed = true;
	UE_LOG(LogTemp, Warning, TEXT("Tower '%s' DESTROYED!"), *TowerName);

	// Kill feed: structure destroyed (green if enemy's, red if ours)
	USkylandersKillFeedWidget::Post(this, FString::Printf(TEXT("%s destroyed"), *TowerName),
		Team == ETowerTeam::Enemy ? FLinearColor(0.3f, 1.0f, 0.4f) : FLinearColor(1.0f, 0.4f, 0.3f));

	// Play destroy sound
	if (DestroySound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DestroySound, GetActorLocation());
	}

	// Reward player if enemy tower
	if (Team == ETowerTeam::Enemy)
	{
		ASkylandersCharacter* Player = Cast<ASkylandersCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
		if (Player)
		{
			Player->AddXP(200.0f);
			Player->AddCoins(50);
		}
	}

	// Hide but don't destroy (rubble could go here later).
	// Collision must go too, or the invisible tower keeps blocking movement
	// and absorbing projectiles.
	BaseMesh->SetVisibility(false);
	TowerMesh->SetVisibility(false);
	SetActorEnableCollision(false);
	CurrentTarget = nullptr;
	ForcedPlayerTarget = nullptr;
	ForcedPlayerTimer = 0.0f;
	if (HealthBarComp) HealthBarComp->SetVisibility(false);

	// Kill feed notifications
	FString TeamStr = (Team == ETowerTeam::Friendly) ? TEXT("Friendly") : TEXT("Enemy");

	// Phoenix respawns after a timer
	if (bIsPhoenix)
	{
		GetWorld()->GetTimerManager().SetTimer(PhoenixRespawnTimer, this,
			&ASkylandersTower::RespawnPhoenix, PhoenixRespawnTime, false);
		UE_LOG(LogTemp, Warning, TEXT("Phoenix '%s' will respawn in %.0f seconds!"), *TowerName, PhoenixRespawnTime);

		// Kill feed: Phoenix destroyed
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(101, 5.0f, FColor::Yellow,
				FString::Printf(TEXT("%s Phoenix has been destroyed! It will respawn in %.0fs"), *TeamStr, PhoenixRespawnTime));
		}
	}
	else
	{
		// Kill feed: Tower destroyed
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(100, 4.0f, FColor::Yellow,
				FString::Printf(TEXT("%s Tower has been destroyed!"), *TeamStr));
		}
	}
}

void ASkylandersTower::RespawnPhoenix()
{
	// Play respawn sound
	if (PhoenixRespawnSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PhoenixRespawnSound, GetActorLocation());
	}

	bDestroyed = false;
	CurrentHealth = MaxHealth;
	BaseMesh->SetVisibility(true);
	TowerMesh->SetVisibility(true);
	SetActorEnableCollision(true);
	if (HealthBarComp) HealthBarComp->SetVisibility(true);
	bHealthBarInitialized = false; // Re-cache health bar
	UpdateHealthBar();

	FString TeamStr = (Team == ETowerTeam::Friendly) ? TEXT("Friendly") : TEXT("Enemy");
	UE_LOG(LogTemp, Warning, TEXT("Phoenix '%s' has RESPAWNED!"), *TowerName);

	// Kill feed: Phoenix respawned
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(102, 5.0f, FColor::Yellow,
			FString::Printf(TEXT("%s Phoenix has respawned!"), *TeamStr));
	}
}

void ASkylandersTower::UpdateHealthBar()
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

			// Hide name text - structures don't show names
			CachedNameText->SetVisibility(ESlateVisibility::Collapsed);
			CachedHealthText->SetVisibility(ESlateVisibility::Collapsed);

			FProgressBarStyle BarStyle = CachedHealthBar->GetWidgetStyle();
			BarStyle.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.3f, 0.02f, 0.02f, 1.0f));
			CachedHealthBar->SetWidgetStyle(BarStyle);
		}
		else
		{
			return;
		}
	}

	float Pct = (MaxHealth > 0.0f) ? (CurrentHealth / MaxHealth) : 0.0f;
	CachedHealthBar->SetPercent(Pct);

	// Tower health bar color based on team; gray when shielded
	FLinearColor BarColor;
	if (!IsVulnerable())
	{
		BarColor = FLinearColor(0.4f, 0.4f, 0.4f); // Gray when shielded
	}
	else if (Pct < 0.3f)
	{
		BarColor = FLinearColor(1.0f, 0.3f, 0.0f); // Orange when low
	}
	else
	{
		BarColor = (Team == ETowerTeam::Friendly) ? FLinearColor(0.2f, 0.5f, 1.0f) : FLinearColor::Red;
	}
	CachedHealthBar->SetFillColorAndOpacity(BarColor);

	FString HealthStr = FString::Printf(TEXT("%.0f/%.0f"), CurrentHealth, MaxHealth);
	CachedHealthText->SetText(FText::FromString(HealthStr));
}

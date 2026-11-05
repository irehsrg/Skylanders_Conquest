// Skylanders Conquest - Tower Implementation

#include "SkylandersTower.h"
#include "SkylandersSmokeTest.h"
#include "SkylandersSoundLibrary.h"
#include "SkylandersHitFlash.h"
#include "SkylandersKillFeedWidget.h"
#include "SkylandersAnnouncerWidget.h"
#include "SkylandersTitan.h"
#include "SkylandersTelemetry.h"
#include "SkylandersCharacter.h"
#include "SkylandersEnemy.h"
#include "SkylandersMinion.h"
#include "SkylandersEnemyGod.h"
#include "SkylandersDamageNumber.h"
#include "SkylandersStructureVisuals.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "SkylandersActorRegistry.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

ASkylandersTower::ASkylandersTower()
{
	PrimaryActorTick.bCanEverTick = true;

	// Blocking upper step of the base (root). Sized in LayoutVisuals once
	// bIsPhoenix is known (it is set between construction and BeginPlay).
	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	RootComponent = BaseMesh;
	BaseMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone"));
	UStaticMesh* Cyl = CylinderMesh.Succeeded() ? CylinderMesh.Object : nullptr;
	UStaticMesh* Cone = ConeMesh.Succeeded() ? ConeMesh.Object : nullptr;
	if (Cyl)
	{
		BaseMesh->SetStaticMesh(Cyl);
		BaseMesh->SetRelativeScale3D(FVector(1.5f, 1.5f, 1.0f));
	}

	// Children hang off VisualRoot, which undoes the root's scale: a child of a
	// scaled parent inherits that scale in both size and offset, which is why the
	// old 200-unit column offset really sat 60 units up.
	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(BaseMesh);

	// Column (keeps its collision: it is the part projectiles aimed high hit)
	TowerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TowerMesh"));
	TowerMesh->SetupAttachment(VisualRoot);
	TowerMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TowerMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	if (Cyl) TowerMesh->SetStaticMesh(Cyl);

	// Visual-only dressing (no collision, no navmesh)
	SlabMesh = SkyStructVis::MakePart(this, VisualRoot, TEXT("SlabMesh"), Cyl);
	SkirtMesh = SkyStructVis::MakePart(this, VisualRoot, TEXT("SkirtMesh"), Cone);
	ChaliceMesh = SkyStructVis::MakePart(this, VisualRoot, TEXT("ChaliceMesh"), Cone);
	PlatformMesh = SkyStructVis::MakePart(this, VisualRoot, TEXT("TopPlatform"), Cyl);

	CrystalPivot = CreateDefaultSubobject<USceneComponent>(TEXT("CrystalPivot"));
	CrystalPivot->SetupAttachment(VisualRoot);
	static const TCHAR* CrystalNames[] = { TEXT("CrystalA_Top"), TEXT("CrystalA_Bottom"), TEXT("CrystalB_Top"), TEXT("CrystalB_Bottom") };
	for (const TCHAR* Name : CrystalNames)
	{
		CrystalParts.Add(SkyStructVis::MakePart(this, CrystalPivot, Name, Cone));
	}

	// Health bar (height set in LayoutVisuals, above the crystal)
	HealthBarComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
	HealthBarComp->SetupAttachment(VisualRoot);
	HealthBarComp->SetRelativeLocation(FVector(0.0f, 0.0f, 900.0f));
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
	StoneMID = nullptr;
	TrimMID = nullptr;
	CrystalTime = 0.0f;
	CrystalBaseZ = 700.0f;

	// Audio
	AttackSound = nullptr;
	DestroySound = nullptr;
	PhoenixRespawnSound = nullptr;
}

void ASkylandersTower::BeginPlay()
{
	Super::BeginPlay();
	if (USkylandersActorRegistry* Registry = USkylandersActorRegistry::Get(this))
	{
		Registry->Register(this);
	}

	// Default SFX for any sound slot the Blueprint left empty (assigned sounds win)
	if (!AttackSound) AttackSound = SkylandersSound::Get(TEXT("tower_shot"));
	if (!DestroySound) DestroySound = SkylandersSound::Get(TEXT("hit_heavy"));
	if (!PhoenixRespawnSound) PhoenixRespawnSound = SkylandersSound::Get(TEXT("respawn"));

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

	// Shape + team materials (needs bIsPhoenix/Team, both set before BeginPlay)
	LayoutVisuals();

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

	// Crystal: slow spin + bob. Two relative-transform writes a tick, no physics.
	CrystalTime += DeltaTime;
	if (CrystalPivot)
	{
		CrystalPivot->SetRelativeLocationAndRotation(
			FVector(0.0f, 0.0f, CrystalBaseZ + 15.0f * FMath::Sin(CrystalTime * 1.6f)),
			FRotator(0.0f, FMath::Fmod(CrystalTime * 40.0f, 360.0f), 0.0f));
	}

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
		USkylandersActorRegistry::GetAllActorsOfClass(GetWorld(), ASkylandersMinion::StaticClass(), AllMinions);
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
		USkylandersActorRegistry::GetAllActorsOfClass(GetWorld(), ASkylandersEnemy::StaticClass(), AllEnemies);
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
			USkylandersActorRegistry::GetAllActorsOfClass(GetWorld(), ASkylandersEnemyGod::StaticClass(), AllGods);
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
		USkylandersActorRegistry::GetAllActorsOfClass(GetWorld(), ASkylandersMinion::StaticClass(), AllMinions);
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

		// No minions - target the nearest hero in range: the player OR an ally
		// (Friendly-team) god. Without this, AI allies dove enemy towers for free.
		float BestDist = AttackRange;
		APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		if (ASkylandersCharacter* SkyChar = Cast<ASkylandersCharacter>(Player))
		{
			if (SkyChar->CurrentHealth > 0.0f)
			{
				float Dist = FVector::Dist(GetActorLocation(), Player->GetActorLocation());
				if (Dist < BestDist) { BestDist = Dist; CurrentTarget = Player; }
			}
		}

		TArray<AActor*> AllGods;
		USkylandersActorRegistry::GetAllActorsOfClass(GetWorld(), ASkylandersEnemyGod::StaticClass(), AllGods);
		for (AActor* Actor : AllGods)
		{
			ASkylandersEnemyGod* God = Cast<ASkylandersEnemyGod>(Actor);
			if (God && God->Team == ETowerTeam::Friendly && God->CurrentState != EGodAIState::Dead)
			{
				float Dist = FVector::Dist(GetActorLocation(), Actor->GetActorLocation());
				if (Dist < BestDist) { BestDist = Dist; CurrentTarget = Actor; }
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
	// Beam leaves from the crystal (purely visual)
	FVector TowerTop = CrystalPivot ? CrystalPivot->GetComponentLocation() : GetActorLocation() + FVector(0.0f, 0.0f, 400.0f);
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
	else // Enemy tower
	{
		if (ASkylandersCharacter* Player = Cast<ASkylandersCharacter>(CurrentTarget))
		{
			Player->TakeDamage_Custom(AttackDamage, this);
		}
		else if (ASkylandersEnemyGod* AllyGod = Cast<ASkylandersEnemyGod>(CurrentTarget))
		{
			AllyGod->TakeDamage_Custom(AttackDamage, this);
		}
	}

	UE_LOG(LogTemp, Verbose, TEXT("Tower '%s' attacks %s for %.0f damage!"),
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
	USkylandersHitFlashComponent::Flash(this);
	SKY_COUNT("structure_damage");

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

	if (USkylandersTelemetrySubsystem* Tele = USkylandersTelemetrySubsystem::Get(this))
	{
		Tele->LogStructureDestroyed(TowerName, Team == ETowerTeam::Enemy ? TEXT("red") : TEXT("blue"));
	}

	// Play destroy sound
	if (DestroySound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DestroySound, GetActorLocation());
	}

	// Announcer: banner in the owning team's color (blue ours, red theirs)
	const FLinearColor TeamColor = (Team == ETowerTeam::Friendly)
		? FLinearColor(0.35f, 0.6f, 1.0f) : FLinearColor(1.0f, 0.35f, 0.3f);
	USkylandersAnnouncerWidget::Announce(this, bIsPhoenix ? TEXT("PHOENIX DESTROYED") : TEXT("TOWER DESTROYED"), TeamColor);

	// This structure was the last thing shielding the enemy titan
	if (Team == ETowerTeam::Enemy)
	{
		TArray<AActor*> Titans;
		USkylandersActorRegistry::GetAllActorsOfClass(GetWorld(), ASkylandersTitan::StaticClass(), Titans);
		for (AActor* A : Titans)
		{
			ASkylandersTitan* Titan = Cast<ASkylandersTitan>(A);
			if (Titan && Titan->Team == ETowerTeam::Enemy && Titan->ProtectingTower == this && !Titan->bDead)
			{
				USkylandersAnnouncerWidget::Announce(this, TEXT("ENEMY TITAN VULNERABLE"), FLinearColor(1.0f, 0.8f, 0.2f));
				break;
			}
		}
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
	// A phoenix stays as a dark ruin (crystals out) until it respawns; a plain
	// tower vanishes along with all of its dressing.
	if (bIsPhoenix)
	{
		SetRuinedLook(true);
	}
	else
	{
		BaseMesh->SetVisibility(false, /*bPropagateToChildren*/ true);
	}
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

		// "<name> destroyed" is already in the kill feed (top of Die); add when it returns
		USkylandersKillFeedWidget::Post(this, FString::Printf(TEXT("%s Phoenix respawns in %.0fs"), *TeamStr, PhoenixRespawnTime),
			FLinearColor(1.0f, 0.85f, 0.2f));
	}
	// A plain tower's destruction is already posted to the kill feed and announcer above
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
	SetRuinedLook(false);
	SetActorEnableCollision(true);
	if (HealthBarComp) HealthBarComp->SetVisibility(true);
	bHealthBarInitialized = false; // Re-cache health bar
	UpdateHealthBar();

	FString TeamStr = (Team == ETowerTeam::Friendly) ? TEXT("Friendly") : TEXT("Enemy");
	UE_LOG(LogTemp, Warning, TEXT("Phoenix '%s' has RESPAWNED!"), *TowerName);

	// Kill feed: Phoenix respawned
	USkylandersKillFeedWidget::Post(this, FString::Printf(TEXT("%s Phoenix has respawned"), *TeamStr),
		FLinearColor(1.0f, 0.85f, 0.2f));
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

void ASkylandersTower::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USkylandersActorRegistry* Registry = USkylandersActorRegistry::Get(this))
	{
		Registry->Unregister(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ASkylandersTower::LayoutVisuals()
{
	using namespace SkyStructVis;
	const bool bBlue = (Team == ETowerTeam::Friendly);
	const bool bPhx = bIsPhoenix;

	// Every height below is measured from the floor (Z=0); G converts to the
	// actor-relative space VisualRoot uses (towers spawn a little above the floor).
	const float G = -GetActorLocation().Z;

	// Blocking step. Its radius stays where the old base was (75; phoenix 85):
	// melee minions (range 120, capsule 20) measure to the actor center and must
	// still be able to stand close enough to hit it.
	const float StepR = bPhx ? 85.0f : 75.0f;
	const float StepTop = bPhx ? 70.0f : 65.0f;
	if (BaseMesh && BaseMesh->GetStaticMesh())
	{
		const FVector Ext = BaseMesh->GetStaticMesh()->GetBounds().BoxExtent;
		// Root is centered on the actor, so it spans [-H/2, H/2] relative
		const float H = FMath::Max(2.0f * (StepTop + G), 20.0f);
		const FVector RootScale(StepR / Ext.X, StepR / Ext.Y, H / (2.0f * Ext.Z));
		BaseMesh->SetRelativeScale3D(RootScale);
		VisualRoot->SetRelativeScale3D(FVector(1.0f / RootScale.X, 1.0f / RootScale.Y, 1.0f / RootScale.Z));
	}

	// Silhouette (world units from the floor). The tower tops out around 850,
	// the phoenix is squatter and wider with twin crystals.
	const float SlabR    = bPhx ? 170.0f : 130.0f;
	const float SkirtR   = bPhx ? 105.0f : 85.0f;
	const float SkirtH   = bPhx ? 260.0f : 240.0f;
	const float ColR     = bPhx ? 62.0f : 48.0f;
	const float ColTop   = bPhx ? 500.0f : 560.0f;
	const float CupR     = bPhx ? 175.0f : 130.0f;
	const float CupH     = bPhx ? 130.0f : 110.0f;
	const float RingR    = bPhx ? 180.0f : 135.0f;
	const float RingH    = 22.0f;
	const float GemZ     = bPhx ? 650.0f : 720.0f;
	const float GemR     = bPhx ? 36.0f : 45.0f;
	const float GemHalfH = bPhx ? 95.0f : 115.0f;
	const float GemOrbit = bPhx ? 90.0f : 0.0f;
	const float BarZ     = bPhx ? 830.0f : 900.0f;

	Fit(SlabMesh, SlabR, G, bPhx ? 14.0f : 12.0f);
	Fit(SkirtMesh, SkirtR, G + StepTop, SkirtH);
	Fit(TowerMesh, ColR, G + StepTop, ColTop - StepTop);
	Fit(ChaliceMesh, CupR, G + ColTop - CupH, CupH, /*bFlip*/ true);
	Fit(PlatformMesh, RingR, G + ColTop, RingH);

	// Crystals are two cones base to base, laid out around CrystalPivot (which
	// Tick spins and bobs). A tower uses one centered gem, a phoenix two orbiting.
	CrystalBaseZ = G + GemZ;
	CrystalPivot->SetRelativeLocation(FVector(0.0f, 0.0f, CrystalBaseZ));
	for (int32 Gem = 0; Gem < 2 && CrystalParts.Num() >= 4; Gem++)
	{
		const FVector2D XY(Gem == 0 ? GemOrbit : -GemOrbit, 0.0f);
		Fit(CrystalParts[Gem * 2], GemR, 0.0f, GemHalfH, false, XY);
		Fit(CrystalParts[Gem * 2 + 1], GemR, -GemHalfH, GemHalfH, true, XY);
	}

	if (HealthBarComp) HealthBarComp->SetRelativeLocation(FVector(0.0f, 0.0f, G + BarZ));

	// Materials: neutral stone body, team trim (gold on a phoenix so the two
	// structure types read apart), glowing team crystals.
	StoneMID = MakeLitMID(this, StoneColor(bBlue));
	TrimMID = MakeLitMID(this, bPhx ? FLinearColor(0.95f, 0.7f, 0.25f) : TeamColor(bBlue));
	UMaterialInstanceDynamic* GlowMID = MakeGlowMID(this, TeamColor(bBlue) * 3.0f);
	for (UStaticMeshComponent* C : { BaseMesh, TowerMesh, SlabMesh, SkirtMesh })
	{
		if (C && StoneMID) C->SetMaterial(0, StoneMID);
	}
	for (UStaticMeshComponent* C : { ChaliceMesh, PlatformMesh })
	{
		if (C && TrimMID) C->SetMaterial(0, TrimMID);
	}
	for (UStaticMeshComponent* C : CrystalParts)
	{
		if (C && GlowMID) C->SetMaterial(0, GlowMID);
	}

	SetRuinedLook(false);
}

void ASkylandersTower::SetRuinedLook(bool bRuined)
{
	const bool bBlue = (Team == ETowerTeam::Friendly);
	if (StoneMID)
	{
		StoneMID->SetVectorParameterValue(FName("Color"),
			bRuined ? FLinearColor(0.08f, 0.08f, 0.08f) : SkyStructVis::StoneColor(bBlue));
	}
	if (TrimMID)
	{
		TrimMID->SetVectorParameterValue(FName("Color"), bRuined ? FLinearColor(0.12f, 0.12f, 0.12f)
			: (bIsPhoenix ? FLinearColor(0.95f, 0.7f, 0.25f) : SkyStructVis::TeamColor(bBlue)));
	}
	// Shell parts are always shown here; Die() hides a plain tower wholesale
	for (UStaticMeshComponent* C : { BaseMesh, TowerMesh, SlabMesh, SkirtMesh, ChaliceMesh, PlatformMesh })
	{
		if (C) C->SetVisibility(true);
	}
	for (int32 i = 0; i < CrystalParts.Num(); i++)
	{
		// The second gem only exists on a phoenix
		if (CrystalParts[i]) CrystalParts[i]->SetVisibility(!bRuined && (i < 2 || bIsPhoenix));
	}
}

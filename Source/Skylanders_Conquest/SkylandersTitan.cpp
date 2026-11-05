// Skylanders Conquest - Titan Implementation

#include "SkylandersTitan.h"
#include "SkylandersSmokeTest.h"
#include "SkylandersSoundLibrary.h"
#include "SkylandersHitFlash.h"
#include "SkylandersTelemetry.h"
#include "SkylandersCharacter.h"
#include "SkylandersEnemy.h"
#include "SkylandersMinion.h"
#include "SkylandersEnemyGod.h"
#include "SkylandersDamageNumber.h"
#include "SkylandersVictoryWidget.h"
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
#include "Components/TextRenderComponent.h"

ASkylandersTitan::ASkylandersTitan()
{
	PrimaryActorTick.bCanEverTick = true;

	// Thick body column (root, keeps the blocking collision). Its footprint stays
	// close to the old 200x200 box so nothing that walks up to the titan changes.
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	RootComponent = BodyMesh;
	BodyMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere"));
	UStaticMesh* Cyl = CylinderMesh.Succeeded() ? CylinderMesh.Object : nullptr;
	UStaticMesh* Cone = ConeMesh.Succeeded() ? ConeMesh.Object : nullptr;
	UStaticMesh* Sphere = SphereMesh.Succeeded() ? SphereMesh.Object : nullptr;
	if (Cyl)
	{
		BodyMesh->SetStaticMesh(Cyl);
		BodyMesh->SetRelativeScale3D(FVector(2.2f, 2.2f, 3.0f));
	}

	// Children hang off VisualRoot, which undoes the root's scale (a child of a
	// scaled parent inherits it in size AND offset).
	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(BodyMesh);

	// Capital on top of the column (was the sphere head)
	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(VisualRoot);
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadMesh->SetCanEverAffectNavigation(false);
	if (Cyl) HeadMesh->SetStaticMesh(Cyl);

	CollarMesh = SkyStructVis::MakePart(this, VisualRoot, TEXT("CollarMesh"), Cone);
	PlinthParts.Add(SkyStructVis::MakePart(this, VisualRoot, TEXT("PlinthLower"), Cyl));
	PlinthParts.Add(SkyStructVis::MakePart(this, VisualRoot, TEXT("PlinthUpper"), Cyl));
	static const TCHAR* PillarNames[] = { TEXT("Pillar0"), TEXT("Pillar1"), TEXT("Pillar2"), TEXT("Pillar3") };
	static const TCHAR* CapNames[] = { TEXT("PillarCap0"), TEXT("PillarCap1"), TEXT("PillarCap2"), TEXT("PillarCap3") };
	for (int32 i = 0; i < 4; i++)
	{
		PillarParts.Add(SkyStructVis::MakePart(this, VisualRoot, PillarNames[i], Cyl));
		PillarCaps.Add(SkyStructVis::MakePart(this, VisualRoot, CapNames[i], Sphere));
	}

	CorePivot = CreateDefaultSubobject<USceneComponent>(TEXT("CorePivot"));
	CorePivot->SetupAttachment(VisualRoot);
	CoreParts.Add(SkyStructVis::MakePart(this, CorePivot, TEXT("CoreTop"), Cone));
	CoreParts.Add(SkyStructVis::MakePart(this, CorePivot, TEXT("CoreBottom"), Cone));

	// Health bar (height set in LayoutVisuals, above the core)
	HealthBarComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
	HealthBarComp->SetupAttachment(VisualRoot);
	HealthBarComp->SetRelativeLocation(FVector(0.0f, 0.0f, 950.0f));
	HealthBarComp->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarComp->SetDrawSize(FVector2D(200.0f, 45.0f));
	HealthBarComp->SetPivot(FVector2D(0.5f, 1.0f));

	// Stats - beefy boss
	MaxHealth = 5000.0f;
	CurrentHealth = 5000.0f;
	TitanName = TEXT("Titan");
	AttackDamage = 80.0f;
	AttackRange = 700.0f;
	AttackCooldown = 0.8f;
	Team = ETowerTeam::Friendly;

	ProtectingTower = nullptr;
	AttackTimer = 0.0f;
	CurrentTarget = nullptr;
	bDead = false;
	bHealthBarInitialized = false;
	CachedHealthBar = nullptr;
	CachedNameText = nullptr;
	CachedHealthText = nullptr;
	CoreMID = nullptr;
	CoreTime = 0.0f;
	CoreBaseZ = 640.0f;
	CoreShownVulnerable = -1;

	// Audio
	AttackSound = nullptr;
	DestroySound = nullptr;
}

void ASkylandersTitan::BeginPlay()
{
	Super::BeginPlay();
	if (USkylandersActorRegistry* Registry = USkylandersActorRegistry::Get(this))
	{
		Registry->Register(this);
	}

	// Default SFX for any sound slot the Blueprint left empty (assigned sounds win)
	if (!AttackSound) AttackSound = SkylandersSound::Get(TEXT("tower_shot"));
	if (!DestroySound) DestroySound = SkylandersSound::Get(TEXT("ability_ult"));

	// All titans overlap with WorldDynamic (projectiles) — damage handled via OnOverlap
	// Player (Pawn channel) is still blocked
	BodyMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	BodyMesh->SetGenerateOverlapEvents(true);

	// Load health bar widget
	UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UserInterface/WBP_EnemyHealthBar.WBP_EnemyHealthBar_C"));
	if (WidgetClass && HealthBarComp)
	{
		HealthBarComp->SetWidgetClass(WidgetClass);
		HealthBarComp->InitWidget();
	}

	// Shape + team materials (needs Team, set before BeginPlay)
	LayoutVisuals();

	FTimerHandle InitTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(InitTimerHandle, this, &ASkylandersTitan::UpdateHealthBar, 0.1f, false);

	UE_LOG(LogTemp, Log, TEXT("Titan '%s' (%s) spawned with %.0f HP"),
		*TitanName, Team == ETowerTeam::Friendly ? TEXT("Friendly") : TEXT("Enemy"), MaxHealth);
}

void ASkylandersTitan::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bDead) return;

	// Core crystal: bright, fast and pulsing while vulnerable; dim and slow while
	// a structure still shields it. Recolor only when the state flips.
	CoreTime += DeltaTime;
	const bool bVuln = IsVulnerable();
	if (CoreMID && CoreShownVulnerable != (bVuln ? 1 : 0))
	{
		CoreShownVulnerable = bVuln ? 1 : 0;
		const FLinearColor Team3 = SkyStructVis::TeamColor(Team == ETowerTeam::Friendly);
		CoreMID->SetVectorParameterValue(FName("Color"), bVuln ? Team3 * 4.0f : Team3 * 0.6f);
	}
	if (CorePivot)
	{
		const float Pulse = bVuln ? 1.0f + 0.12f * FMath::Sin(CoreTime * 4.5f) : 1.0f;
		CorePivot->SetRelativeLocationAndRotation(
			FVector(0.0f, 0.0f, CoreBaseZ + 20.0f * FMath::Sin(CoreTime * 1.2f)),
			FRotator(0.0f, FMath::Fmod(CoreTime * (bVuln ? 45.0f : 15.0f), 360.0f), 0.0f));
		CorePivot->SetRelativeScale3D(FVector(Pulse));
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

	// Draw range ring above the ground (DepthPriority=1 = always on top)
	FVector RingCenter = GetActorLocation();
	RingCenter.Z = 12.0f; // hug the ground so the ring doesn't float or block UI
	FColor RingColor = (Team == ETowerTeam::Friendly) ? FColor::Blue : FColor::Magenta;
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

	// Tick attack timer
	if (AttackTimer > 0.0f)
	{
		AttackTimer -= DeltaTime;
	}

	FindTarget();

	if (CurrentTarget && AttackTimer <= 0.0f)
	{
		AttackTarget();
		AttackTimer = AttackCooldown;
	}
}

bool ASkylandersTitan::IsVulnerable() const
{
	// Titan can only be damaged if protecting tower is destroyed or not assigned
	if (!ProtectingTower) return true;
	return ProtectingTower->bDestroyed;
}

void ASkylandersTitan::FindTarget()
{
	CurrentTarget = nullptr;
	float ClosestDist = AttackRange;

	if (Team == ETowerTeam::Friendly)
	{
		// Friendly titan: prioritize enemy minions, then jungle enemies
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
		if (CurrentTarget) return;

		// Then enemy god
		TArray<AActor*> AllGods;
		USkylandersActorRegistry::GetAllActorsOfClass(GetWorld(), ASkylandersEnemyGod::StaticClass(), AllGods);
		for (AActor* Actor : AllGods)
		{
			ASkylandersEnemyGod* God = Cast<ASkylandersEnemyGod>(Actor);
			if (God && God->CurrentState != EGodAIState::Dead)
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
	else
	{
		// Enemy titan: prioritize friendly minions, then player
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

void ASkylandersTitan::AttackTarget()
{
	if (!CurrentTarget) return;

	// Play attack sound
	if (AttackSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, AttackSound, GetActorLocation());
	}

	// Big beam attack from head
	FVector HeadLoc = CorePivot ? CorePivot->GetComponentLocation() : GetActorLocation() + FVector(0.0f, 0.0f, 220.0f);
	FVector TargetLoc = CurrentTarget->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);

	// Thicker beam than tower
	FColor BeamColor = (Team == ETowerTeam::Friendly) ? FColor::Blue : FColor::Red;
	DrawDebugLine(GetWorld(), HeadLoc, TargetLoc, BeamColor, false, 0.2f, 0, 5.0f);

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
			Player->TakeDamage_Custom(AttackDamage, this);
		}
	}
}

void ASkylandersTitan::TakeDamage_Custom(float DamageAmount, AActor* DamageCauser)
{
	if (bDead) return;

	// Check if titan is protected by tower
	if (!IsVulnerable())
	{
		// Show "IMMUNE" text instead of damage
		FVector NumberLoc = GetActorLocation() + FVector(0.0f, 0.0f, 350.0f);
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
	FVector NumberLoc = GetActorLocation() + FVector(0.0f, 0.0f, 350.0f);
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASkylandersDamageNumber* DmgNum = GetWorld()->SpawnActor<ASkylandersDamageNumber>(
		ASkylandersDamageNumber::StaticClass(), NumberLoc, FRotator::ZeroRotator, SpawnParams);
	if (DmgNum)
	{
		DmgNum->SetDamageNumber(DamageAmount, FColor::Orange, DamageAmount >= 60.0f);
	}

	UpdateHealthBar();

	if (CurrentHealth <= 0.0f)
	{
		Die();
	}
}

void ASkylandersTitan::Die()
{
	bDead = true;
	UE_LOG(LogTemp, Warning, TEXT("Titan '%s' DESTROYED!"), *TitanName);

	if (USkylandersTelemetrySubsystem* Tele = USkylandersTelemetrySubsystem::Get(this))
	{
		Tele->LogStructureDestroyed(TitanName, Team == ETowerTeam::Enemy ? TEXT("red") : TEXT("blue"));
		// Destroying the enemy titan = player victory; losing ours = defeat.
		Tele->LogMatchEnd(Team == ETowerTeam::Enemy, GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	}

	// Play destroy sound
	if (DestroySound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DestroySound, GetActorLocation());
	}

	// Reward player if enemy titan
	if (Team == ETowerTeam::Enemy)
	{
		ASkylandersCharacter* Player = Cast<ASkylandersCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
		if (Player)
		{
			Player->AddXP(500.0f);
			Player->AddCoins(200);
		}
		ShowEndScreen(true); // Player wins
	}
	else
	{
		ShowEndScreen(false); // Player loses
	}

	// Hide titan (collision too, or the invisible body keeps blocking)
	BodyMesh->SetVisibility(false, /*bPropagateToChildren*/ true);
	SetActorEnableCollision(false);
	CurrentTarget = nullptr;
	if (HealthBarComp) HealthBarComp->SetVisibility(false);
}

void ASkylandersTitan::ShowEndScreen(bool bVictory)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC) return;

	FString Message = bVictory ? TEXT("VICTORY") : TEXT("DEFEAT");
	UE_LOG(LogTemp, Warning, TEXT("=== GAME OVER: %s ==="), *Message);

	// Create the victory/defeat widget
	VictoryWidget = CreateWidget<USkylandersVictoryWidget>(PC, USkylandersVictoryWidget::StaticClass());
	if (VictoryWidget)
	{
		VictoryWidget->AddToViewport(100); // High Z-order so it's on top of everything

		ASkylandersCharacter* Player = Cast<ASkylandersCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
		VictoryWidget->ShowResult(bVictory, Player);

		// UI-only input so the PLAY AGAIN / MAIN MENU buttons actually receive
		// the clicks instead of the game swallowing them.
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(VictoryWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
	}

	// Pause the game after a short delay (weak-bound so a vanished PC can't be called)
	FTimerHandle PauseTimer;
	GetWorld()->GetTimerManager().SetTimer(PauseTimer, FTimerDelegate::CreateWeakLambda(PC, [PC]()
	{
		PC->SetPause(true);
	}), 3.0f, false);
}

void ASkylandersTitan::UpdateHealthBar()
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

			// Hide name and health text - structures don't show names
			CachedNameText->SetVisibility(ESlateVisibility::Collapsed);
			CachedHealthText->SetVisibility(ESlateVisibility::Collapsed);

			// Darker background for titan
			FProgressBarStyle BarStyle = CachedHealthBar->GetWidgetStyle();
			BarStyle.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.2f, 0.02f, 0.2f, 1.0f));
			CachedHealthBar->SetWidgetStyle(BarStyle);
		}
		else
		{
			return;
		}
	}

	float Pct = (MaxHealth > 0.0f) ? (CurrentHealth / MaxHealth) : 0.0f;
	CachedHealthBar->SetPercent(Pct);

	// Purple health bar for titan; gray when shielded, orange when low
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
		BarColor = (Team == ETowerTeam::Friendly) ? FLinearColor(0.3f, 0.2f, 0.8f) : FLinearColor(0.7f, 0.1f, 0.7f);
	}
	CachedHealthBar->SetFillColorAndOpacity(BarColor);

	FString HealthStr = FString::Printf(TEXT("%.0f/%.0f"), CurrentHealth, MaxHealth);
	CachedHealthText->SetText(FText::FromString(HealthStr));
}

void ASkylandersTitan::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USkylandersActorRegistry* Registry = USkylandersActorRegistry::Get(this))
	{
		Registry->Unregister(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ASkylandersTitan::LayoutVisuals()
{
	using namespace SkyStructVis;
	const bool bBlue = (Team == ETowerTeam::Friendly);

	// Heights are from the floor (Z=0); G converts to actor-relative space (the
	// titan spawns at Z=150).
	const float G = -GetActorLocation().Z;

	// Column: r110 from below the floor up to 520. The root is centered on the
	// actor, so it spans [-H/2, H/2]; whatever is under the floor is never seen.
	const float ColR = 110.0f, ColTop = 520.0f;
	if (BodyMesh && BodyMesh->GetStaticMesh())
	{
		const FVector Ext = BodyMesh->GetStaticMesh()->GetBounds().BoxExtent;
		const float H = FMath::Max(2.0f * (ColTop + G), 50.0f);
		const FVector RootScale(ColR / Ext.X, ColR / Ext.Y, H / (2.0f * Ext.Z));
		BodyMesh->SetRelativeScale3D(RootScale);
		VisualRoot->SetRelativeScale3D(FVector(1.0f / RootScale.X, 1.0f / RootScale.Y, 1.0f / RootScale.Z));
	}

	// Two 20-unit plinth tiers: low enough to step onto (pawn step 45, navmesh
	// 35), so they get real collision and keep pawns from sinking into them.
	// Projectiles ignore them; only the body is a damage volume.
	Fit(PlinthParts[0], 340.0f, G, 20.0f);
	Fit(PlinthParts[1], 260.0f, G + 20.0f, 20.0f);
	for (UStaticMeshComponent* P : PlinthParts)
	{
		P->SetCollisionProfileName(TEXT("BlockAllDynamic"));
		P->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
		P->SetCanEverAffectNavigation(true);
	}

	// Four pillars on the diagonals (the lane approaches along X, so the axis
	// stays open), each with a glowing cap.
	for (int32 i = 0; i < 4; i++)
	{
		const float Ang = FMath::DegreesToRadians(45.0f + 90.0f * i);
		const FVector2D XY(FMath::Cos(Ang) * 215.0f, FMath::Sin(Ang) * 215.0f);
		Fit(PillarParts[i], 28.0f, G + 40.0f, 380.0f, false, XY);
		PillarParts[i]->SetCollisionProfileName(TEXT("BlockAllDynamic"));
		PillarParts[i]->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
		PillarParts[i]->SetCanEverAffectNavigation(true);
		Fit(PillarCaps[i], 42.0f, G + 420.0f, 84.0f, false, XY);
	}

	// Flared collar under a wide capital
	Fit(CollarMesh, 150.0f, G + ColTop - 120.0f, 120.0f, /*bFlip*/ true);
	Fit(HeadMesh, 155.0f, G + ColTop, 40.0f);

	// Core: two cones base to base, 380 tall, floating above the capital
	const float CoreHalfH = 190.0f;
	CoreBaseZ = G + ColTop + 40.0f + 60.0f + CoreHalfH;
	CorePivot->SetRelativeLocation(FVector(0.0f, 0.0f, CoreBaseZ));
	Fit(CoreParts[0], 95.0f, 0.0f, CoreHalfH);
	Fit(CoreParts[1], 95.0f, -CoreHalfH, CoreHalfH, /*bFlip*/ true);

	if (HealthBarComp) HealthBarComp->SetRelativeLocation(FVector(0.0f, 0.0f, CoreBaseZ + CoreHalfH + 140.0f));

	// Materials: neutral stone, team trim (capital/collar/upper plinth), glow
	UMaterialInstanceDynamic* Stone = MakeLitMID(this, StoneColor(bBlue));
	UMaterialInstanceDynamic* Trim = MakeLitMID(this, TeamColor(bBlue));
	UMaterialInstanceDynamic* CapGlow = MakeGlowMID(this, TeamColor(bBlue) * 2.0f);
	CoreMID = MakeGlowMID(this, TeamColor(bBlue) * 0.6f);
	CoreShownVulnerable = -1; // Tick picks the real state on its first pass
	for (UStaticMeshComponent* C : { BodyMesh, PlinthParts[0] })
	{
		if (C && Stone) C->SetMaterial(0, Stone);
	}
	for (UStaticMeshComponent* C : PillarParts)
	{
		if (C && Stone) C->SetMaterial(0, Stone);
	}
	for (UStaticMeshComponent* C : { HeadMesh, CollarMesh, PlinthParts[1] })
	{
		if (C && Trim) C->SetMaterial(0, Trim);
	}
	for (UStaticMeshComponent* C : PillarCaps)
	{
		if (C && CapGlow) C->SetMaterial(0, CapGlow);
	}
	for (UStaticMeshComponent* C : CoreParts)
	{
		if (C && CoreMID) C->SetMaterial(0, CoreMID);
	}
}

// Skylanders Conquest - Titan Implementation

#include "SkylandersTitan.h"
#include "SkylandersCharacter.h"
#include "SkylandersEnemy.h"
#include "SkylandersMinion.h"
#include "SkylandersEnemyGod.h"
#include "SkylandersDamageNumber.h"
#include "SkylandersVictoryWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Components/TextRenderComponent.h"

ASkylandersTitan::ASkylandersTitan()
{
	PrimaryActorTick.bCanEverTick = true;

	// Large body (big cube/cylinder)
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	RootComponent = BodyMesh;
	BodyMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube"));
	if (CubeMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CubeMesh.Object);
		BodyMesh->SetRelativeScale3D(FVector(2.0f, 2.0f, 3.0f)); // Big chunky body
	}

	// Head on top (sphere)
	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(BodyMesh);
	HeadMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 220.0f));
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere"));
	if (SphereMesh.Succeeded())
	{
		HeadMesh->SetStaticMesh(SphereMesh.Object);
		HeadMesh->SetRelativeScale3D(FVector(1.5f, 1.5f, 1.5f));
	}

	// Health bar
	HealthBarComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
	HealthBarComp->SetupAttachment(BodyMesh);
	HealthBarComp->SetRelativeLocation(FVector(0.0f, 0.0f, 280.0f));
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

	// Audio
	AttackSound = nullptr;
	DestroySound = nullptr;
}

void ASkylandersTitan::BeginPlay()
{
	Super::BeginPlay();

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

	// Color based on team
	if (BodyMesh)
	{
		UMaterialInterface* DefaultMat = BodyMesh->GetMaterial(0);
		if (DefaultMat)
		{
			UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(DefaultMat, this);
			if (Team == ETowerTeam::Friendly)
			{
				DynMat->SetVectorParameterValue(FName("Color"), FLinearColor(0.05f, 0.2f, 0.8f)); // Dark blue
			}
			else
			{
				DynMat->SetVectorParameterValue(FName("Color"), FLinearColor(0.8f, 0.05f, 0.05f)); // Dark red
			}
			BodyMesh->SetMaterial(0, DynMat);
			HeadMesh->SetMaterial(0, DynMat);
		}
	}

	FTimerHandle InitTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(InitTimerHandle, this, &ASkylandersTitan::UpdateHealthBar, 0.1f, false);

	UE_LOG(LogTemp, Log, TEXT("Titan '%s' (%s) spawned with %.0f HP"),
		*TitanName, Team == ETowerTeam::Friendly ? TEXT("Friendly") : TEXT("Enemy"), MaxHealth);
}

void ASkylandersTitan::Tick(float DeltaTime)
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
			HealthBarComp->SetVisibility(DistToPlayer < 3000.0f);
		}
	}

	// Draw range ring above the ground (DepthPriority=1 = always on top)
	FVector RingCenter = GetActorLocation();
	RingCenter.Z = 50.0f;
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
		if (CurrentTarget) return;

		// Then enemy god
		TArray<AActor*> AllGods;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersEnemyGod::StaticClass(), AllGods);
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
	FVector HeadLoc = GetActorLocation() + FVector(0.0f, 0.0f, 220.0f);
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
			Player->TakeDamage_Custom(AttackDamage);
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
	BodyMesh->SetVisibility(false);
	HeadMesh->SetVisibility(false);
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

		// Show mouse cursor
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

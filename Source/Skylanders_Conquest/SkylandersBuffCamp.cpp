// Skylanders Conquest - Jungle Buff Camp Implementation

#include "SkylandersBuffCamp.h"
#include "SkylandersCharacter.h"
#include "SkylandersDamageNumber.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

ASkylandersBuffCamp::ASkylandersBuffCamp()
{
	PrimaryActorTick.bCanEverTick = true;

	// Sphere body mesh as root
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	RootComponent = BodyMesh;
	BodyMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	BodyMesh->SetNotifyRigidBodyCollision(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere"));
	if (SphereMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(SphereMesh.Object);
		BodyMesh->SetRelativeScale3D(FVector(1.5f, 1.5f, 1.5f));
	}

	// Health bar widget component
	HealthBarComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
	HealthBarComp->SetupAttachment(RootComponent);
	HealthBarComp->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	HealthBarComp->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarComp->SetDrawSize(FVector2D(120.0f, 30.0f));
	HealthBarComp->SetPivot(FVector2D(0.5f, 1.0f));

	// Stats
	MaxHealth = 500.0f;
	CurrentHealth = 500.0f;
	CampName = TEXT("Damage Buff");
	RespawnDelay = 120.0f;
	BuffDuration = 90.0f;
	BuffDamageMultiplier = 1.25f;
	XPReward = 100.0f;
	CoinReward = 30;
	bDead = false;

	// Buff type
	BuffType = EBuffType::Damage;

	// Buff tracking
	BuffedPlayer = nullptr;
	AppliedPowerBonus = 0.0f;
	AppliedSpeedBonus = 0.0f;
	AppliedManaRegenBonus = 0.0f;

	// AI
	LeashRange = 800.0f;
	AttackRange = 100.0f;
	AttackDamage = 30.0f;
	AttackCooldown = 1.5f;
	AttackTimer = 0.0f;
	AggroTarget = nullptr;
	MoveSpeed = 200.0f;

	// Cached widget refs
	bHealthBarInitialized = false;
	CachedHealthBar = nullptr;
	CachedNameText = nullptr;
	CachedHealthText = nullptr;

	// Audio
	DeathSound = nullptr;
}

void ASkylandersBuffCamp::BeginPlay()
{
	Super::BeginPlay();

	// Save spawn location as home
	HomeLocation = GetActorLocation();

	// Color based on buff type (CampName is set by the map builder / editor)
	if (BodyMesh)
	{
		UMaterialInstanceDynamic* DynMat = BodyMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMat)
		{
			FLinearColor CampColor;
			switch (BuffType)
			{
			case EBuffType::Speed: CampColor = FLinearColor(0.2f, 1.0f, 0.3f, 1.0f); break; // Green
			case EBuffType::Mana:  CampColor = FLinearColor(0.2f, 0.5f, 1.0f, 1.0f); break; // Blue
			case EBuffType::None:  CampColor = FLinearColor(0.6f, 0.6f, 0.6f, 1.0f); break; // Gray (XP/gold only)
			default:               CampColor = FLinearColor(1.0f, 0.6f, 0.0f, 1.0f); break; // Orange (Damage)
			}
			DynMat->SetVectorParameterValue(FName("Color"), CampColor);
		}
	}

	// Load widget Blueprint class
	UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UserInterface/WBP_EnemyHealthBar.WBP_EnemyHealthBar_C"));
	if (WidgetClass && HealthBarComp)
	{
		HealthBarComp->SetWidgetClass(WidgetClass);
		HealthBarComp->InitWidget();
	}

	UE_LOG(LogTemp, Log, TEXT("Buff Camp '%s' spawned with %.0f HP, Respawn: %.0fs, Buff: %.0fs at %.0f%% damage"),
		*CampName, MaxHealth, RespawnDelay, BuffDuration, (BuffDamageMultiplier - 1.0f) * 100.0f);

	// Delayed health bar init (widget needs a frame to construct)
	FTimerHandle InitTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(InitTimerHandle, this, &ASkylandersBuffCamp::UpdateHealthBar, 0.1f, false);
}

// ========== AI ==========

void ASkylandersBuffCamp::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Distance-based health bar rendering
	if (HealthBarComp)
	{
		APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		if (Player)
		{
			float DistToPlayer = FVector::Dist(GetActorLocation(), Player->GetActorLocation());
			HealthBarComp->SetVisibility(!bDead && DistToPlayer < 3000.0f);
		}
	}

	if (!bDead)
	{
		// Draw leash range ring above the ground (DepthPriority=1 = always on top)
		FVector RingCenter = HomeLocation;
		RingCenter.Z = 50.0f;
		FColor RingColor = (BuffType == EBuffType::Damage) ? FColor::Orange : FColor::Green;
		int32 Segments = 64;
		float AngleStep = 2.0f * PI / Segments;
		for (int32 i = 0; i < Segments; i++)
		{
			float A1 = i * AngleStep;
			float A2 = (i + 1) * AngleStep;
			FVector P1 = RingCenter + FVector(FMath::Cos(A1) * LeashRange, FMath::Sin(A1) * LeashRange, 0.0f);
			FVector P2 = RingCenter + FVector(FMath::Cos(A2) * LeashRange, FMath::Sin(A2) * LeashRange, 0.0f);
			DrawDebugLine(GetWorld(), P1, P2, RingColor, false, 0.05f, 1, 4.0f);
		}

		if (AttackTimer > 0.0f) AttackTimer -= DeltaTime;
		UpdateAI(DeltaTime);
	}
}

void ASkylandersBuffCamp::UpdateAI(float DeltaTime)
{
	// Check if aggro target is still valid
	bool bHasValidTarget = AggroTarget != nullptr && !AggroTarget->IsPendingKillPending();

	// If target is a dead player, drop aggro
	if (bHasValidTarget)
	{
		ASkylandersCharacter* PlayerTarget = Cast<ASkylandersCharacter>(AggroTarget);
		if (PlayerTarget && PlayerTarget->CurrentHealth <= 0.0f)
		{
			bHasValidTarget = false;
			AggroTarget = nullptr;
		}
	}

	// Drop aggro once the target escapes the leash radius (the camp itself never
	// leaves the leash, so without this it would wait at the edge forever instead
	// of returning home to heal)
	if (bHasValidTarget)
	{
		float TargetDistFromHome = FVector::Dist2D(AggroTarget->GetActorLocation(), HomeLocation);
		if (TargetDistFromHome > LeashRange)
		{
			bHasValidTarget = false;
			AggroTarget = nullptr;
		}
	}

	if (!bHasValidTarget)
	{
		// No target - return home if not already there
		AggroTarget = nullptr;
		float DistToHome = FVector::Dist2D(GetActorLocation(), HomeLocation);
		if (DistToHome > 50.0f)
		{
			// Move toward home
			FVector MoveDir = (HomeLocation - GetActorLocation()).GetSafeNormal2D();
			FVector NewLoc = GetActorLocation() + MoveDir * MoveSpeed * DeltaTime;
			SetActorLocation(NewLoc);
		}
		else if (DistToHome > 0.1f)
		{
			// Close enough - snap to home and heal to full
			SetActorLocation(HomeLocation);
			if (CurrentHealth < MaxHealth)
			{
				CurrentHealth = MaxHealth;
				UpdateHealthBar();
			}
		}

		// Heal when no players are nearby (patience/reset mechanic)
		APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		bool bPlayerNearby = false;
		if (Player)
		{
			float PlayerDistToHome = FVector::Dist(Player->GetActorLocation(), HomeLocation);
			bPlayerNearby = (PlayerDistToHome < LeashRange);
		}

		if (!bPlayerNearby && CurrentHealth < MaxHealth)
		{
			// Heal at 5% max HP per second
			CurrentHealth = FMath::Clamp(CurrentHealth + MaxHealth * 0.05f * DeltaTime, 0.0f, MaxHealth);
			UpdateHealthBar();
		}

		return;
	}

	// Has a valid target - check leash range from home
	float DistFromHome = FVector::Dist2D(GetActorLocation(), HomeLocation);
	if (DistFromHome > LeashRange)
	{
		// Too far from home - drop aggro, return home, heal
		AggroTarget = nullptr;
		CurrentHealth = MaxHealth;
		UpdateHealthBar();
		return;
	}

	// Face the target
	FaceTarget(AggroTarget);

	// Check distance to target
	float DistToTarget = FVector::Dist(GetActorLocation(), AggroTarget->GetActorLocation());

	if (DistToTarget <= AttackRange)
	{
		// In attack range - attack if cooldown ready
		if (AttackTimer <= 0.0f)
		{
			ASkylandersCharacter* PlayerTarget = Cast<ASkylandersCharacter>(AggroTarget);
			if (PlayerTarget)
			{
				PlayerTarget->TakeDamage_Custom(AttackDamage);
				AttackTimer = AttackCooldown;

				// Debug attack line (orange)
				DrawDebugLine(
					GetWorld(),
					GetActorLocation(),
					AggroTarget->GetActorLocation(),
					FColor::Orange,
					false,
					0.3f,
					0,
					2.0f
				);

				UE_LOG(LogTemp, Log, TEXT("Buff Camp '%s' attacked player for %.0f damage!"), *CampName, AttackDamage);
			}
		}
	}
	else
	{
		// Chase target (only if within leash range from home)
		FVector DirToTarget = (AggroTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		FVector NewLoc = GetActorLocation() + DirToTarget * MoveSpeed * DeltaTime;

		// Only move if it wouldn't exceed leash range
		float NewDistFromHome = FVector::Dist2D(NewLoc, HomeLocation);
		if (NewDistFromHome <= LeashRange)
		{
			SetActorLocation(NewLoc);
		}
	}
}

void ASkylandersBuffCamp::FaceTarget(AActor* Target)
{
	if (!Target) return;

	FVector Direction = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (!Direction.IsNearlyZero())
	{
		FRotator TargetRot = Direction.Rotation();
		SetActorRotation(TargetRot);
	}
}

// ========== DAMAGE AND DEATH ==========

void ASkylandersBuffCamp::TakeDamage_Custom(float DamageAmount, AActor* DamageCauser)
{
	if (bDead || CurrentHealth <= 0.0f) return;

	// Only damageable if the attacker is within leash range of home location
	APawn* DmgPawn = Cast<APawn>(DamageCauser);
	if (!DmgPawn && DamageCauser)
	{
		DmgPawn = DamageCauser->GetInstigator();
	}
	if (DmgPawn)
	{
		float DistToHome = FVector::Dist(DmgPawn->GetActorLocation(), HomeLocation);
		if (DistToHome > LeashRange)
		{
			return; // Too far from camp - ignore damage (no sniping from lane)
		}
	}

	// Set aggro to the most recent attacker (last-hit aggro)
	ASkylandersCharacter* AttackingPlayer = Cast<ASkylandersCharacter>(DamageCauser);
	if (!AttackingPlayer && DamageCauser)
	{
		APawn* DmgInstigator = DamageCauser->GetInstigator();
		AttackingPlayer = Cast<ASkylandersCharacter>(DmgInstigator);
	}
	if (AttackingPlayer)
	{
		AggroTarget = AttackingPlayer;
	}

	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);

	// Spawn floating damage number
	FVector NumberLoc = GetActorLocation() + FVector(0.0f, 0.0f, 100.0f);
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

	UpdateHealthBar();

	if (CurrentHealth <= 0.0f)
	{
		// Resolve the actual player from the damage causer
		ASkylandersCharacter* KillingPlayer = Cast<ASkylandersCharacter>(DamageCauser);
		if (!KillingPlayer && DamageCauser)
		{
			APawn* DmgInstigator = DamageCauser->GetInstigator();
			KillingPlayer = Cast<ASkylandersCharacter>(DmgInstigator);
		}

		// Store the killing player for buff application
		BuffedPlayer = KillingPlayer;
		Die();
	}
}

void ASkylandersBuffCamp::Die()
{
	UE_LOG(LogTemp, Warning, TEXT("Buff Camp '%s' killed!"), *CampName);
	bDead = true;
	AggroTarget = nullptr;

	// Play death sound
	if (DeathSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation());
	}

	// Kill feed: buff camp slain
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(103, 4.0f, FColor::Yellow,
			FString::Printf(TEXT("You have slain %s!"), *CampName));
	}

	// Reward killing player with XP and coins, and apply buff
	ASkylandersCharacter* Player = Cast<ASkylandersCharacter>(BuffedPlayer);
	if (Player)
	{
		Player->AddXP(XPReward);
		Player->AddCoins(CoinReward);
		UE_LOG(LogTemp, Log, TEXT("Player rewarded: %.0f XP, %d coins"), XPReward, CoinReward);

		// Apply buff based on type — record the applied bonus so removal can
		// subtract exactly that amount (level-ups/other buffs stay intact)
		if (BuffType == EBuffType::Speed)
		{
			// Speed buff - +30% of current base speed
			AppliedSpeedBonus = Player->BaseMaxWalkSpeed * 0.30f;
			Player->BaseMaxWalkSpeed += AppliedSpeedBonus;
			Player->GetCharacterMovement()->MaxWalkSpeed = Player->BaseMaxWalkSpeed;

			UE_LOG(LogTemp, Log, TEXT("Speed Buff applied! BaseMaxWalkSpeed: %.1f -> %.1f for %.0fs"),
				Player->BaseMaxWalkSpeed - AppliedSpeedBonus, Player->BaseMaxWalkSpeed, BuffDuration);

			// Kill feed: buff acquired
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(104, 5.0f, FColor::Yellow,
					FString::Printf(TEXT("You gained Speed buff! +30%% movement speed for %.0fs"), BuffDuration));
			}
		}
		else if (BuffType == EBuffType::Mana)
		{
			// Mana buff (blue buff) - double mana regen
			AppliedManaRegenBonus = Player->ManaRegenRate;
			Player->ManaRegenRate += AppliedManaRegenBonus;

			UE_LOG(LogTemp, Log, TEXT("Mana Buff applied! ManaRegen: %.1f -> %.1f for %.0fs"),
				AppliedManaRegenBonus, Player->ManaRegenRate, BuffDuration);

			// Kill feed: buff acquired
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(104, 5.0f, FColor::Yellow,
					FString::Printf(TEXT("You gained Mana buff! +100%% mana regen for %.0fs"), BuffDuration));
			}
		}
		else if (BuffType == EBuffType::Damage)
		{
			// Damage buff - +X% of current base power
			AppliedPowerBonus = Player->BasePower * (BuffDamageMultiplier - 1.0f);
			Player->BasePower += AppliedPowerBonus;

			UE_LOG(LogTemp, Log, TEXT("Damage Buff applied! BasePower: %.1f -> %.1f for %.0fs"),
				Player->BasePower - AppliedPowerBonus, Player->BasePower, BuffDuration);

			// Kill feed: buff acquired
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(104, 5.0f, FColor::Yellow,
					FString::Printf(TEXT("You gained Damage buff! +%.0f%% damage for %.0fs"),
						(BuffDamageMultiplier - 1.0f) * 100.0f, BuffDuration));
			}
		}
		// BuffType::None = XP/Gold only, no buff to apply

		// Set timer to remove buff
		GetWorld()->GetTimerManager().SetTimer(BuffTimerHandle, this, &ASkylandersBuffCamp::RemoveBuff, BuffDuration, false);
	}

	// Hide and disable collision
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);

	// Set respawn timer
	GetWorld()->GetTimerManager().SetTimer(RespawnTimerHandle, this, &ASkylandersBuffCamp::RespawnCamp, RespawnDelay, false);
}

void ASkylandersBuffCamp::RemoveBuff()
{
	ASkylandersCharacter* Player = Cast<ASkylandersCharacter>(BuffedPlayer);
	if (Player)
	{
		if (BuffType == EBuffType::Speed)
		{
			// Never drop below the item-adjusted base (RecalculateItemBonuses may
			// have rewritten BaseMaxWalkSpeed while the buff was active)
			Player->BaseMaxWalkSpeed = FMath::Max(
				Player->BaseMaxWalkSpeed - AppliedSpeedBonus,
				500.0f + Player->ItemBonusStats.MovementSpeed);
			Player->GetCharacterMovement()->MaxWalkSpeed = Player->BaseMaxWalkSpeed;
			UE_LOG(LogTemp, Log, TEXT("Speed Buff expired! BaseMaxWalkSpeed now %.1f"), Player->BaseMaxWalkSpeed);
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Red, TEXT("Speed Buff Expired!"));
		}
		else if (BuffType == EBuffType::Mana)
		{
			Player->ManaRegenRate = FMath::Max(Player->ManaRegenRate - AppliedManaRegenBonus, 0.0f);
			UE_LOG(LogTemp, Log, TEXT("Mana Buff expired! ManaRegen now %.1f"), Player->ManaRegenRate);
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Red, TEXT("Blue Buff Expired!"));
		}
		else if (BuffType == EBuffType::Damage)
		{
			Player->BasePower = FMath::Max(Player->BasePower - AppliedPowerBonus, 0.0f);
			UE_LOG(LogTemp, Log, TEXT("Damage Buff expired! BasePower now %.1f"), Player->BasePower);
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Red, TEXT("Damage Buff Expired!"));
		}
		// BuffType::None has no buff to remove
	}

	BuffedPlayer = nullptr;
	AppliedPowerBonus = 0.0f;
	AppliedSpeedBonus = 0.0f;
	AppliedManaRegenBonus = 0.0f;
}

void ASkylandersBuffCamp::RespawnCamp()
{
	UE_LOG(LogTemp, Log, TEXT("Buff Camp '%s' respawned!"), *CampName);

	CurrentHealth = MaxHealth;
	bDead = false;
	AggroTarget = nullptr;
	AttackTimer = 0.0f;

	// Return to home location on respawn
	SetActorLocation(HomeLocation);

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	// Reset health bar cache so it re-initializes
	CachedHealthBar = nullptr;
	CachedNameText = nullptr;
	CachedHealthText = nullptr;
	bHealthBarInitialized = false;

	UpdateHealthBar();
}

// ========== HEALTH BAR ==========

void ASkylandersBuffCamp::UpdateHealthBar()
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

			// Hide name and health text - only show bar
			CachedNameText->SetVisibility(ESlateVisibility::Collapsed);
			CachedHealthText->SetVisibility(ESlateVisibility::Collapsed);

			// Dark background tint
			FProgressBarStyle BarStyle = CachedHealthBar->GetWidgetStyle();
			BarStyle.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.2f, 0.1f, 0.0f, 1.0f));
			CachedHealthBar->SetWidgetStyle(BarStyle);
		}
		else
		{
			return;
		}
	}

	// Health bar percentage
	float Pct = (MaxHealth > 0.0f) ? (CurrentHealth / MaxHealth) : 0.0f;
	CachedHealthBar->SetPercent(Pct);

	// Orange fill color for buff camp
	CachedHealthBar->SetFillColorAndOpacity(FLinearColor(1.0f, 0.6f, 0.0f, 1.0f));

	// Health text
	FString HealthStr = FString::Printf(TEXT("%.0f/%.0f"), CurrentHealth, MaxHealth);
	CachedHealthText->SetText(FText::FromString(HealthStr));
}

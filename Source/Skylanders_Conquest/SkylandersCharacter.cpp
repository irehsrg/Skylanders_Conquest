// Skylanders Conquest - Player Character Implementation

#include "SkylandersCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/RichTextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "SkylandersProjectile.h"
#include "SkylandersAoEExplosion.h"
#include "SkylandersTower.h"
#include "SkylandersDamageNumber.h"
#include "SkylandersEnemy.h"
#include "SkylandersMinion.h"
#include "SkylandersBuffCamp.h"
#include "SkylandersEnemyGod.h"
#include "SkylandersTitan.h"
#include "SkylandersItemCatalog.h"
#include "SkylandersInventoryWidget.h"
#include "SkylandersItemHUDWidget.h"
#include "SkylandersMinimapWidget.h"
#include "SkylandersScoreboardWidget.h"
#include "Sound/SoundBase.h"
#include "DrawDebugHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Animation/AnimInstance.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Fonts/SlateFontInfo.h"

ASkylandersCharacter::ASkylandersCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Set size for collision capsule - sized for Trigger Happy (small character)
	GetCapsuleComponent()->InitCapsuleSize(25.f, 35.0f);

	// SMITE-style: character always faces where camera looks
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true; // Character yaw follows camera
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = false; // Don't rotate to face movement direction
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 320.f; // Slightly higher hop
	GetCharacterMovement()->AirControl = 0.1f; // Minimal air control
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->GravityScale = 1.5f; // Slightly more floaty than before

	// Create camera boom (spring arm) - SMITE-style camera
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 450.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 100.0f);
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 15.0f;

	// Create follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->SetRelativeRotation(FRotator(-22.0f, 0.0f, 0.0f));
	FollowCamera->FieldOfView = 90.0f;

	// Configure character mesh - adjusted for Trigger Happy's smaller size
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -35.0f)); // Match capsule half-height
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f)); // Rotate to face forward

	// Create gun mesh components - tuned values from Blueprint editor
	LeftGunMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("LeftGunMesh"));
	LeftGunMesh->SetupAttachment(GetMesh(), FName("Bone-L-Hand"));
	LeftGunMesh->SetRelativeLocation(FVector(28.0f, 0.0f, -4.0f));
	LeftGunMesh->SetRelativeRotation(FRotator(-74.0f, -23.0f, 96.0f));

	RightGunMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("RightGunMesh"));
	RightGunMesh->SetupAttachment(GetMesh(), FName("Bone-R-Hand"));
	RightGunMesh->SetRelativeLocation(FVector(-28.0f, 0.0f, 8.0f));
	RightGunMesh->SetRelativeRotation(FRotator(99.0f, -17.0f, 98.0f));

	// ========== VFX INDICATOR MESHES ==========

	// Ground aim indicator - flat cylinder on the ground (targeting reticle)
	GroundAimIndicator = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GroundAimIndicator"));
	GroundAimIndicator->SetupAttachment(RootComponent);
	GroundAimIndicator->SetAbsolute(true, true, true); // World-space positioning
	GroundAimIndicator->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GroundAimIndicator->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> AimCylinderMesh(TEXT("/Engine/BasicShapes/Cylinder"));
	if (AimCylinderMesh.Succeeded())
	{
		GroundAimIndicator->SetStaticMesh(AimCylinderMesh.Object);
	}
	// Scale: radius 250 (cylinder default radius = 50, so scale XY = 5.0), very flat Z
	GroundAimIndicator->SetWorldScale3D(FVector(5.0f, 5.0f, 0.02f));
	GroundAimIndicator->SetVisibility(false); // Hidden until needed

	// Recall indicator - flat cyan cylinder around player
	RecallIndicator = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RecallIndicator"));
	RecallIndicator->SetupAttachment(RootComponent);
	RecallIndicator->SetAbsolute(true, true, true);
	RecallIndicator->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RecallIndicator->SetCastShadow(false);
	// Reuse same cylinder mesh
	if (AimCylinderMesh.Succeeded())
	{
		RecallIndicator->SetStaticMesh(AimCylinderMesh.Object);
	}
	// Radius 100 (scale XY = 2.0), very flat
	RecallIndicator->SetWorldScale3D(FVector(2.0f, 2.0f, 0.02f));
	RecallIndicator->SetVisibility(false);

	// Yamato charge VFX (spawned at runtime, not a constructor default)
	YamatoChargeVFX = nullptr;

	// Initialize stats - CRITICAL: These MUST be non-zero to avoid divide by zero in HUD
	MaxHealth = 100.0f;
	CurrentHealth = 100.0f; // Changed from MaxHealth to ensure it's 100
	MaxMana = 100.0f;
	CurrentMana = 100.0f; // Changed from MaxMana to ensure it's 100
	ManaRegenRate = 5.0f; // 5 mana per second
	PlayerLevel = 1;
	Coins = 0;

	// XP
	CurrentXP = 0.0f;
	XPToNextLevel = 100.0f; // First level needs 100 XP
	MaxLevel = 20;

	// Stat growth per level
	HealthPerLevel = 20.0f;
	ManaPerLevel = 15.0f;
	ManaRegenPerLevel = 0.5f;

	// Initialize abilities
	// 1 - Golden Super Charge (dash)
	Ability1_Cooldown = 8.0f;
	Ability1_ManaCost = 20.0f;
	Ability1_RemainingCooldown = 0.0f;

	// 2 - Pot o' Gold (AoE)
	Ability2_Cooldown = 10.0f;
	Ability2_ManaCost = 35.0f;
	Ability2_RemainingCooldown = 0.0f;

	// 3 - Golden Minigun (fire rate buff)
	Ability3_Cooldown = 18.0f;
	Ability3_ManaCost = 50.0f;
	Ability3_RemainingCooldown = 0.0f;

	// 4 - Yamato Blast (ultimate line nuke)
	Ability4_Cooldown = 60.0f;
	Ability4_ManaCost = 100.0f;
	Ability4_RemainingCooldown = 0.0f;

	// Initialize projectile settings
	ProjectileSpawnOffset = FVector(80.0f, 0.0f, -20.0f);
	ProjectileSpeed = 3000.0f;
	FireRate = 2.0f; // 2 shots per second (0.5 second cooldown between shots)
	LastFireTime = -999.0f; // Allow immediate first shot
	bFireFromLeftGun = true; // Start with left gun

	// Movement penalties (SMITE-style)
	StrafeSpeedMultiplier = 0.8f; // 80% speed when strafing
	BackpedalSpeedMultiplier = 0.6f; // 60% speed when backpedaling
	AttackMoveSpeedMultiplier = 0.5f; // 50% speed during attack slow
	AttackSlowDuration = 0.65f; // Slightly under 1 second
	AttackSlowRemainingTime = 0.0f;
	BaseMaxWalkSpeed = 500.0f;

	// Ability 3 - Golden Machine Gun state
	bMachineGunActive = false;
	MachineGunRemainingTime = 0.0f;
	MachineGunDuration = 4.0f;
	MachineGunFireRate = 10.0f; // 10 shots per second

	// Cooldown UI cache
	bCooldownWidgetsCached = false;
	for (int i = 0; i < 4; i++)
	{
		CooldownOverlay[i] = nullptr;
		CooldownText[i] = nullptr;
	}

	// Ability 4 - Yamato Blast state
	bChargingYamato = false;
	YamatoChargeRemaining = 0.0f;
	YamatoChargeTime = 2.0f; // 2 second charge
	YamatoDamage = 400.0f; // Base value, scaled with power at fire time
	YamatoRange = 400.0f; // Close range cone

	// Inventory & Shop
	Inventory.SetNum(MaxInventorySlots);
	for (int32 i = 0; i < MaxInventorySlots; i++)
	{
		Inventory[i] = 0; // Empty slots
	}
	bIsInSpawnArea = false;
	bIsShopOpen = false;
	ShopWidget = nullptr;
	InventoryHUDWidget = nullptr;
	ItemHUDWidget = nullptr;

	// Recall
	bIsRecalling = false;
	RecallChannelTime = 5.0f;
	RecallRemainingTime = 0.0f;
	DeathRespawnTimeTotal = 0.0f;
	DeathRespawnTimeRemaining = 0.0f;

	// KDA & game tracking
	Kills = 0;
	Deaths = 0;
	CreepScore = 0;
	GameElapsedTime = 0.0f;
	MinimapWidget = nullptr;
	ScoreboardWidget = nullptr;

	// Passive gold
	PassiveGoldPerSecond = 2.0f;
	PassiveGoldAccumulator = 0.0f;

	// Base stats
	BasePower = 20.0f; // Base auto attack damage, scales with level
	HealthRegenRate = 0.0f;

	// Animation references (set in Blueprint Class Defaults after importing)
	AttackLeftAnim = nullptr;
	AttackRightAnim = nullptr;
	Ability1Anim = nullptr;
	Ability2Anim = nullptr;
	MachineGunStartAnim = nullptr;
	MachineGunLoopAnim = nullptr;
	MachineGunEndAnim = nullptr;
	YamatoAnim = nullptr;
	HitReactAnim = nullptr;
	DeathAnim = nullptr;
	DebugShopCursor = 1;

	// Audio (assign in Blueprint Class Defaults)
	AttackSound = nullptr;
	AbilitySound = nullptr;
	DeathSound = nullptr;
	LevelUpSound = nullptr;
	ItemBuySound = nullptr;

	// Ability leveling
	AbilityPoints = 0;
	for (int32 i = 0; i < 4; i++)
	{
		AbilityLevels[i] = 0;
	}
}

void ASkylandersCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Force correct stats (Blueprint defaults may override C++ constructor with garbage)
	MaxHealth = 100.0f;
	CurrentHealth = 100.0f;
	MaxMana = 100.0f;
	CurrentMana = 100.0f;
	ManaRegenRate = 5.0f;
	BasePower = 20.0f;
	PlayerLevel = 1;

	// Start at level 3: apply 2 level-ups worth of stats (level 1 -> 3)
	for (int32 i = 0; i < 2; i++)
	{
		PlayerLevel++;
		MaxHealth += HealthPerLevel;
		MaxMana += ManaPerLevel;
		ManaRegenRate += ManaRegenPerLevel;
		BasePower += 2.0f;
		CurrentHealth += HealthPerLevel;
		CurrentMana += ManaPerLevel;
	}
	// Clamp
	CurrentHealth = FMath::Min(CurrentHealth, MaxHealth);
	CurrentMana = FMath::Min(CurrentMana, MaxMana);

	// 3 ability points to spend (one per level)
	AbilityPoints = 3;

	// Reset ability levels
	for (int32 i = 0; i < 4; i++)
	{
		AbilityLevels[i] = 0;
	}

	// Load and assign gun meshes
	static ConstructorHelpers::FObjectFinder<USkeletalMesh>* GunMeshFinder = nullptr;
	USkeletalMesh* GunMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/TriggerHappy/Meshes/Gun/TriggerHappyfbxTriggerHappy_Gun"));
	if (GunMesh)
	{
		if (LeftGunMesh)
		{
			LeftGunMesh->SetSkeletalMesh(GunMesh);
			UE_LOG(LogTemp, Log, TEXT("Left gun mesh attached!"));
		}
		if (RightGunMesh)
		{
			RightGunMesh->SetSkeletalMesh(GunMesh);
			UE_LOG(LogTemp, Log, TEXT("Right gun mesh attached!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not load gun mesh!"));
	}

	// Set up VFX indicator materials
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
	if (BaseMat)
	{
		// Gold translucent material for ground aim indicator
		if (GroundAimIndicator)
		{
			UMaterialInstanceDynamic* AimMat = UMaterialInstanceDynamic::Create(BaseMat, this);
			AimMat->SetVectorParameterValue(FName("Color"), FLinearColor(1.0f, 0.85f, 0.0f, 0.4f));
			GroundAimIndicator->SetMaterial(0, AimMat);
		}
		// Cyan translucent material for recall indicator
		if (RecallIndicator)
		{
			UMaterialInstanceDynamic* RecallMat = UMaterialInstanceDynamic::Create(BaseMat, this);
			RecallMat->SetVectorParameterValue(FName("Color"), FLinearColor(0.0f, 0.8f, 1.0f, 0.5f));
			RecallIndicator->SetMaterial(0, RecallMat);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("=== CHARACTER BEGIN PLAY ==="));
	UE_LOG(LogTemp, Warning, TEXT("MaxHealth: %.1f, CurrentHealth: %.1f"), MaxHealth, CurrentHealth);
	UE_LOG(LogTemp, Warning, TEXT("MaxMana: %.1f, CurrentMana: %.1f"), MaxMana, CurrentMana);

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
				UE_LOG(LogTemp, Log, TEXT("Input Mapping Context added"));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("DefaultMappingContext is NULL!"));
			}
		}

		// Create HUD
		if (MainHUDClass)
		{
			MainHUDWidget = CreateWidget<UUserWidget>(PlayerController, MainHUDClass);
			if (MainHUDWidget)
			{
				MainHUDWidget->AddToViewport();
				UE_LOG(LogTemp, Log, TEXT("HUD Created Successfully"));

				// Create inventory overlay
				CreateInventoryHUD();

				// Create item HUD (bottom-center, 6 slots)
				ItemHUDWidget = CreateWidget<USkylandersItemHUDWidget>(PlayerController, USkylandersItemHUDWidget::StaticClass());
				if (ItemHUDWidget)
				{
					ItemHUDWidget->AddToViewport(5);
					UE_LOG(LogTemp, Log, TEXT("Item HUD created (bottom-center, 6 slots)"));
				}

				// Create minimap
				MinimapWidget = CreateWidget<USkylandersMinimapWidget>(PlayerController, USkylandersMinimapWidget::StaticClass());
				if (MinimapWidget)
				{
					MinimapWidget->AddToViewport(3);
					UE_LOG(LogTemp, Log, TEXT("Minimap created (top-right)"));
				}

				// Create scoreboard (hidden by default, Tab to show)
				ScoreboardWidget = CreateWidget<USkylandersScoreboardWidget>(PlayerController, USkylandersScoreboardWidget::StaticClass());
				if (ScoreboardWidget)
				{
					ScoreboardWidget->AddToViewport(25);
					ScoreboardWidget->SetVisibility(ESlateVisibility::Collapsed);
					UE_LOG(LogTemp, Log, TEXT("Scoreboard created (Tab to show)"));
				}

				// Initial HUD update
				UpdateHUD();

				// Load and display crosshair widget
				UClass* CrosshairClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UserInterface/CrosshairWidget.CrosshairWidget_C"));
				if (CrosshairClass)
				{
					UUserWidget* Crosshair = CreateWidget<UUserWidget>(PlayerController, CrosshairClass);
					if (Crosshair)
					{
						Crosshair->AddToViewport(10);
						UE_LOG(LogTemp, Log, TEXT("Crosshair widget added"));
					}
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to create HUD widget!"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("MainHUDClass not set! Assign WBP_MainHUD in blueprint."));
		}
	}

	// Show skill point notification (player starts at level 3 with 3 points)
	if (GEngine && AbilityPoints > 0)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow,
			FString::Printf(TEXT("Level %d - %d SKILL POINTS AVAILABLE! (F1-F4 to level abilities)"), PlayerLevel, AbilityPoints));
	}
}

void ASkylandersCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Regenerate mana (base + item bonus)
	float TotalManaRegen = ManaRegenRate + ItemBonusStats.ManaRegen;
	if (CurrentMana < (MaxMana + ItemBonusStats.MaxMana))
	{
		RestoreMana(TotalManaRegen * DeltaTime);
	}

	// Health regen (base + item bonus)
	float TotalHealthRegen = HealthRegenRate + ItemBonusStats.HealthRegen;
	if (TotalHealthRegen > 0.0f && CurrentHealth > 0.0f && CurrentHealth < (MaxHealth + ItemBonusStats.MaxHealth))
	{
		Heal(TotalHealthRegen * DeltaTime);
	}

	// Passive gold income
	if (CurrentHealth > 0.0f)
	{
		PassiveGoldAccumulator += PassiveGoldPerSecond * DeltaTime;
		if (PassiveGoldAccumulator >= 1.0f)
		{
			int32 GoldToAdd = FMath::FloorToInt(PassiveGoldAccumulator);
			PassiveGoldAccumulator -= GoldToAdd;
			AddCoins(GoldToAdd);
		}
	}

	// Game timer
	GameElapsedTime += DeltaTime;
	if (GEngine)
	{
		int32 Minutes = FMath::FloorToInt(GameElapsedTime / 60.0f);
		int32 Seconds = FMath::FloorToInt(FMath::Fmod(GameElapsedTime, 60.0f));
		GEngine->AddOnScreenDebugMessage(400, 0.0f, FColor::White,
			FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds));
	}

	// Update ability cooldowns
	UpdateCooldowns(DeltaTime);
	UpdateCooldownUI();

	// Ground aim indicator - flat cylinder positioned at crosshair ground point
	// Auto attack range = projectile speed (3000) * lifetime (0.35) = 1050
	float AutoAttackRange = 1050.0f;
	float AoERadius = 250.0f;
	if (CurrentHealth > 0.0f && GroundAimIndicator)
	{
		FVector GroundPoint = GetGroundAimPoint(AutoAttackRange - AoERadius);
		GroundPoint.Z = GetActorLocation().Z + 2.0f; // Slightly above ground
		GroundAimIndicator->SetWorldLocation(GroundPoint);
		GroundAimIndicator->SetVisibility(true);
	}
	else if (GroundAimIndicator)
	{
		GroundAimIndicator->SetVisibility(false);
	}

	// Recall channeling
	if (bIsRecalling)
	{
		// Cancel if player is moving (has velocity)
		FVector CurrentVelocity = GetCharacterMovement()->Velocity;
		if (CurrentVelocity.SizeSquared2D() > 10.0f)
		{
			CancelRecall();
		}
		else
		{
			RecallRemainingTime -= DeltaTime;

			// Visual feedback: cyan cylinder around player
			if (RecallIndicator)
			{
				RecallIndicator->SetWorldLocation(GetActorLocation() + FVector(0, 0, 2.0f));
				RecallIndicator->SetVisibility(true);
			}

			// On-screen countdown
			if (GEngine)
			{
				int32 SecondsLeft = FMath::CeilToInt(RecallRemainingTime);
				GEngine->AddOnScreenDebugMessage(200, 0.1f, FColor::Cyan,
					FString::Printf(TEXT("Recalling... %ds"), SecondsLeft));
			}

			if (RecallRemainingTime <= 0.0f)
			{
				CompleteRecall();
			}
		}
	}
	else if (RecallIndicator)
	{
		RecallIndicator->SetVisibility(false);
	}

	// Death respawn countdown display
	if (CurrentHealth <= 0.0f && DeathRespawnTimeRemaining > 0.0f)
	{
		DeathRespawnTimeRemaining -= DeltaTime;
		if (GEngine)
		{
			int32 SecondsLeft = FMath::CeilToInt(FMath::Max(DeathRespawnTimeRemaining, 0.0f));
			GEngine->AddOnScreenDebugMessage(300, 0.1f, FColor::Red,
				FString::Printf(TEXT("Respawning in %d..."), SecondsLeft));
		}
	}

	// Attack slow timer
	if (AttackSlowRemainingTime > 0.0f)
	{
		AttackSlowRemainingTime -= DeltaTime;
	}

	// Golden Machine Gun timer - stationary rapid fire
	if (bMachineGunActive)
	{
		MachineGunRemainingTime -= DeltaTime;

		// Force stationary
		GetCharacterMovement()->Velocity = FVector::ZeroVector;

		// Auto-fire during machine gun
		float CurrentTime = GetWorld()->GetTimeSeconds();
		float MGFireCooldown = 1.0f / MachineGunFireRate;
		if (CurrentTime - LastFireTime >= MGFireCooldown)
		{
			if (ProjectileClass)
			{
				LastFireTime = CurrentTime;

				// Yaw only, fire flat like normal projectiles
				FRotator SpawnRotation(0.0f, GetControlRotation().Yaw, 0.0f);
				FVector ForwardDir = SpawnRotation.Vector();
				FVector RightDir = FRotationMatrix(SpawnRotation).GetUnitAxis(EAxis::Y);

				FActorSpawnParameters SpawnParams;
				SpawnParams.Owner = this;
				SpawnParams.Instigator = this;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

				// Toggle gun for visual purposes
				float Spread = FMath::RandRange(-2.0f, 2.0f);
				bFireFromLeftGun = !bFireFromLeftGun;

				// Spawn from center, fly straight to crosshair (SMITE-style)
				FVector SpawnLoc = GetActorLocation()
					+ ForwardDir * ProjectileSpawnOffset.X
					+ FVector::UpVector * ProjectileSpawnOffset.Z;

				FRotator SpreadRot = SpawnRotation;
				SpreadRot.Yaw += Spread;

				AActor* MGProj = GetWorld()->SpawnActor<AActor>(ProjectileClass, SpawnLoc, SpreadRot, SpawnParams);
				if (ASkylandersProjectile* SkyProj = Cast<ASkylandersProjectile>(MGProj))
				{
					// Machine gun damage scales with ability 3 rank
					float Ability3RankScale = 0.6f + 0.08f * AbilityLevels[2];
					float MGDmg = GetEffectivePower() * 0.5f * Ability3RankScale;
					bool bCrit = FMath::FRand() < ItemBonusStats.CritChance;
					if (bCrit) MGDmg *= 2.0f;

					SkyProj->Damage = MGDmg;
					SkyProj->Lifesteal = ItemBonusStats.Lifesteal;
					SkyProj->bIsCrit = bCrit;
				}
			}
		}

		if (MachineGunRemainingTime <= 0.0f)
		{
			bMachineGunActive = false;
			MachineGunRemainingTime = 0.0f;
			GetCharacterMovement()->MaxWalkSpeed = BaseMaxWalkSpeed;
			PlayAnimOnSlot(MachineGunEndAnim, 1.0f);
			UE_LOG(LogTemp, Log, TEXT("Golden Machine Gun ended!"));
		}
	}

	// Yamato Blast charge
	if (bChargingYamato)
	{
		// Force stationary during charge
		GetCharacterMovement()->Velocity = FVector::ZeroVector;

		YamatoChargeRemaining -= DeltaTime;

		// Visual charge indicator - growing red sphere mesh
		float ChargeProgress = 1.0f - (YamatoChargeRemaining / YamatoChargeTime);
		float VisualRadius = YamatoRange * ChargeProgress * 0.3f;
		FVector ChargeLoc = GetActorLocation() + GetActorForwardVector() * 100.0f;

		// Create or update charge VFX actor
		if (!YamatoChargeVFX || !IsValid(YamatoChargeVFX))
		{
			FActorSpawnParameters VFXParams;
			VFXParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			YamatoChargeVFX = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), ChargeLoc, FRotator::ZeroRotator, VFXParams);
			if (YamatoChargeVFX)
			{
				UStaticMeshComponent* ChargeMesh = NewObject<UStaticMeshComponent>(YamatoChargeVFX);
				YamatoChargeVFX->SetRootComponent(ChargeMesh);
				ChargeMesh->RegisterComponent();
				UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere"));
				if (SphereMesh) ChargeMesh->SetStaticMesh(SphereMesh);
				ChargeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				ChargeMesh->SetCastShadow(false);
				UMaterialInterface* ChargeBaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
				if (ChargeBaseMat)
				{
					UMaterialInstanceDynamic* ChargeDynMat = UMaterialInstanceDynamic::Create(ChargeBaseMat, YamatoChargeVFX);
					ChargeDynMat->SetVectorParameterValue(FName("Color"), FLinearColor(1.0f, 0.1f, 0.0f)); // Red-orange
					ChargeMesh->SetMaterial(0, ChargeDynMat);
				}
			}
		}
		if (YamatoChargeVFX)
		{
			// Sphere default radius = 50, so scale = VisualRadius / 50
			float SphereScale = FMath::Max(VisualRadius / 50.0f, 0.1f);
			YamatoChargeVFX->SetActorLocation(ChargeLoc);
			YamatoChargeVFX->SetActorScale3D(FVector(SphereScale));
		}

		if (YamatoChargeRemaining <= 0.0f)
		{
			// Destroy charge VFX
			if (YamatoChargeVFX && IsValid(YamatoChargeVFX))
			{
				YamatoChargeVFX->Destroy();
				YamatoChargeVFX = nullptr;
			}

			// FIRE the blast
			bChargingYamato = false;
			GetCharacterMovement()->MaxWalkSpeed = BaseMaxWalkSpeed;

			FVector BlastCenter = GetActorLocation() + GetActorForwardVector() * (YamatoRange * 0.5f);

			// Big visual explosion - spawned red sphere that auto-destroys
			{
				FActorSpawnParameters BlastVFXParams;
				BlastVFXParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				AActor* BlastVFX = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), BlastCenter, FRotator::ZeroRotator, BlastVFXParams);
				if (BlastVFX)
				{
					UStaticMeshComponent* BlastMesh = NewObject<UStaticMeshComponent>(BlastVFX);
					BlastVFX->SetRootComponent(BlastMesh);
					BlastMesh->RegisterComponent();
					UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere"));
					if (SphereMesh) BlastMesh->SetStaticMesh(SphereMesh);
					// Sphere default radius = 50, scale to YamatoRange
					float BlastScale = YamatoRange / 50.0f;
					BlastMesh->SetWorldScale3D(FVector(BlastScale));
					BlastMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					BlastMesh->SetCastShadow(false);
					UMaterialInterface* BlastBaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
					if (BlastBaseMat)
					{
						UMaterialInstanceDynamic* BlastDynMat = UMaterialInstanceDynamic::Create(BlastBaseMat, BlastVFX);
						BlastDynMat->SetVectorParameterValue(FName("Color"), FLinearColor(1.0f, 0.2f, 0.0f)); // Fiery red-orange
						BlastMesh->SetMaterial(0, BlastDynMat);
					}
					BlastVFX->SetLifeSpan(1.0f); // Auto-destroy after 1 second
				}
			}

			// Scale Yamato damage with power (20x power scaling for ultimate)
			// Ultimate rank scaling: at rank 1 = 60%, rank 5 = 100%
			float UltRankScale = 0.5f + 0.10f * AbilityLevels[3];
			float ScaledYamatoDmg = GetEffectivePower() * 20.0f * UltRankScale;
			UE_LOG(LogTemp, Warning, TEXT("YAMATO BLAST FIRES! Damage: %.0f, Range: %.0f"), ScaledYamatoDmg, YamatoRange);

			// Damage all enemies in the close-range cone/sphere
			TArray<AActor*> AllEnemies;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersEnemy::StaticClass(), AllEnemies);

			FVector AimDir = GetActorForwardVector();

			for (AActor* Actor : AllEnemies)
			{
				FVector ToEnemy = Actor->GetActorLocation() - GetActorLocation();
				float Distance = ToEnemy.Size();

				if (Distance < YamatoRange)
				{
					// Must be in front (close range cone)
					float DotForward = FVector::DotProduct(ToEnemy.GetSafeNormal(), AimDir);
					if (DotForward > 0.3f) // Roughly 70 degree cone in front
					{
						ASkylandersEnemy* Enemy = Cast<ASkylandersEnemy>(Actor);
						if (Enemy && Enemy->CurrentHealth > 0.0f)
						{
							// More damage the closer they are
							float DistFactor = 1.0f - (Distance / YamatoRange) * 0.3f;
							float FinalDmg = ScaledYamatoDmg * DistFactor;
							Enemy->TakeDamage_Custom(FinalDmg, this);
							UE_LOG(LogTemp, Log, TEXT("Yamato Blast hit '%s' for %.0f damage!"), *Enemy->EnemyName, FinalDmg);
							AddCoins(10);
						}
					}
				}
			}

			// Also hit enemy minions in the cone
			TArray<AActor*> AllMinions;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersMinion::StaticClass(), AllMinions);

			for (AActor* Actor : AllMinions)
			{
				ASkylandersMinion* Minion = Cast<ASkylandersMinion>(Actor);
				if (Minion && Minion->Team == ETowerTeam::Enemy && !Minion->bDead)
				{
					FVector ToMinion = Actor->GetActorLocation() - GetActorLocation();
					float Distance = ToMinion.Size();

					if (Distance < YamatoRange)
					{
						float DotForward = FVector::DotProduct(ToMinion.GetSafeNormal(), AimDir);
						if (DotForward > 0.3f)
						{
							float DistFactor = 1.0f - (Distance / YamatoRange) * 0.3f;
							float FinalDmg = ScaledYamatoDmg * DistFactor;
							Minion->TakeDamage_Custom(FinalDmg, this);
							UE_LOG(LogTemp, Log, TEXT("Yamato Blast hit minion for %.0f damage!"), FinalDmg);
						}
					}
				}
			}

			// Also hit buff camps in the cone
			TArray<AActor*> AllBuffCamps;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersBuffCamp::StaticClass(), AllBuffCamps);

			for (AActor* Actor : AllBuffCamps)
			{
				ASkylandersBuffCamp* BuffCamp = Cast<ASkylandersBuffCamp>(Actor);
				if (BuffCamp && !BuffCamp->bDead)
				{
					FVector ToCamp = Actor->GetActorLocation() - GetActorLocation();
					float Distance = ToCamp.Size();

					if (Distance < YamatoRange)
					{
						float DotForward = FVector::DotProduct(ToCamp.GetSafeNormal(), AimDir);
						if (DotForward > 0.3f)
						{
							float DistFactor = 1.0f - (Distance / YamatoRange) * 0.3f;
							float FinalDmg = ScaledYamatoDmg * DistFactor;
							BuffCamp->TakeDamage_Custom(FinalDmg, this);
							UE_LOG(LogTemp, Log, TEXT("Yamato Blast hit buff camp '%s' for %.0f damage!"), *BuffCamp->CampName, FinalDmg);
						}
					}
				}
			}

			// Also hit enemy gods in the cone
			TArray<AActor*> AllEnemyGods;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersEnemyGod::StaticClass(), AllEnemyGods);

			for (AActor* Actor : AllEnemyGods)
			{
				ASkylandersEnemyGod* EnemyGod = Cast<ASkylandersEnemyGod>(Actor);
				if (EnemyGod && EnemyGod->CurrentState != EGodAIState::Dead)
				{
					FVector ToGod = Actor->GetActorLocation() - GetActorLocation();
					float Distance = ToGod.Size();

					if (Distance < YamatoRange)
					{
						float DotForward = FVector::DotProduct(ToGod.GetSafeNormal(), AimDir);
						if (DotForward > 0.3f)
						{
							float DistFactor = 1.0f - (Distance / YamatoRange) * 0.3f;
							float FinalDmg = ScaledYamatoDmg * DistFactor;
							EnemyGod->TakeDamage_Custom(FinalDmg, this);
							UE_LOG(LogTemp, Log, TEXT("Yamato Blast hit enemy god '%s' for %.0f damage!"), *EnemyGod->GodName, FinalDmg);
						}
					}
				}
			}

		}
	}

	// Calculate movement speed based on direction relative to facing (SMITE-style)
	float SpeedMultiplier = 1.0f;

	FVector Velocity = GetCharacterMovement()->Velocity;
	if (Velocity.SizeSquared2D() > 1.0f) // Only if actually moving
	{
		FVector ForwardDir = GetActorForwardVector();
		FVector VelocityDir = Velocity.GetSafeNormal2D();
		float DotForward = FVector::DotProduct(ForwardDir, VelocityDir);

		if (DotForward < -0.3f)
		{
			// Moving backward
			SpeedMultiplier = BackpedalSpeedMultiplier;
		}
		else if (DotForward < 0.5f)
		{
			// Strafing (mostly sideways)
			SpeedMultiplier = StrafeSpeedMultiplier;
		}
	}

	// Apply attack slow (stacks with directional penalty)
	if (AttackSlowRemainingTime > 0.0f)
	{
		SpeedMultiplier *= AttackMoveSpeedMultiplier;
	}

	GetCharacterMovement()->MaxWalkSpeed = BaseMaxWalkSpeed * SpeedMultiplier;

	// Fountain healing - rapidly restore HP and mana while in spawn area
	if (bIsInSpawnArea && CurrentHealth > 0.0f)
	{
		float EffectiveMaxHealth = MaxHealth + ItemBonusStats.MaxHealth;
		float EffectiveMaxMana = MaxMana + ItemBonusStats.MaxMana;
		bool bHealed = false;

		// Heal 10% of max HP per second
		if (CurrentHealth < EffectiveMaxHealth)
		{
			float HealAmount = EffectiveMaxHealth * 0.10f * DeltaTime;
			CurrentHealth = FMath::Min(CurrentHealth + HealAmount, EffectiveMaxHealth);
			bHealed = true;
		}

		// Restore 10% of max mana per second
		if (CurrentMana < EffectiveMaxMana)
		{
			float ManaAmount = EffectiveMaxMana * 0.10f * DeltaTime;
			CurrentMana = FMath::Min(CurrentMana + ManaAmount, EffectiveMaxMana);
			bHealed = true;
		}

		if (bHealed)
		{
			UpdateHUD();
		}
	}
}

// Debug key handlers
void ASkylandersCharacter::Debug_TakeDamage()
{
	UE_LOG(LogTemp, Warning, TEXT("DEBUG KEY: H pressed - Taking 10 damage. Health before: %.1f"), CurrentHealth);
	TakeDamage_Custom(10.0f);
	UE_LOG(LogTemp, Warning, TEXT("DEBUG KEY: Health after: %.1f / %.1f"), CurrentHealth, MaxHealth);
}
void ASkylandersCharacter::Debug_Heal()
{
	UE_LOG(LogTemp, Warning, TEXT("DEBUG KEY: J pressed - Healing 10. Health before: %.1f"), CurrentHealth);
	Heal(10.0f);
	UE_LOG(LogTemp, Warning, TEXT("DEBUG KEY: Health after: %.1f / %.1f"), CurrentHealth, MaxHealth);
}
void ASkylandersCharacter::Debug_UseMana()
{
	UE_LOG(LogTemp, Warning, TEXT("DEBUG KEY: M pressed - Using 15 mana. Mana before: %.1f"), CurrentMana);
	UseMana(15.0f);
	UE_LOG(LogTemp, Warning, TEXT("DEBUG KEY: Mana after: %.1f / %.1f"), CurrentMana, MaxMana);
}
void ASkylandersCharacter::Debug_LevelUp()
{
	UE_LOG(LogTemp, Warning, TEXT("DEBUG KEY: L pressed - Leveling up. Level before: %d"), PlayerLevel);
	LevelUp();
	UE_LOG(LogTemp, Warning, TEXT("DEBUG KEY: Level after: %d"), PlayerLevel);
}
void ASkylandersCharacter::Debug_AddGold()
{
	UE_LOG(LogTemp, Warning, TEXT("DEBUG KEY: G pressed - Adding 500 gold"));
	AddCoins(500);
}

void ASkylandersCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Debug keys (always work, no Input Action setup needed)
	PlayerInputComponent->BindKey(EKeys::H, IE_Pressed, this, &ASkylandersCharacter::Debug_TakeDamage);
	PlayerInputComponent->BindKey(EKeys::J, IE_Pressed, this, &ASkylandersCharacter::Debug_Heal);
	PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &ASkylandersCharacter::Debug_UseMana);
	PlayerInputComponent->BindKey(EKeys::L, IE_Pressed, this, &ASkylandersCharacter::Debug_LevelUp);
	PlayerInputComponent->BindKey(EKeys::G, IE_Pressed, this, &ASkylandersCharacter::Debug_AddGold);

	// Shop toggle
	PlayerInputComponent->BindKey(EKeys::B, IE_Pressed, this, &ASkylandersCharacter::ToggleShop);

	// Recall to base (channeled)
	PlayerInputComponent->BindKey(EKeys::R, IE_Pressed, this, &ASkylandersCharacter::StartRecall);

	// Instant teleport to base
	PlayerInputComponent->BindKey(EKeys::T, IE_Pressed, this, &ASkylandersCharacter::TeleportToBase);

	// Ability leveling keys: F1-F4 level up abilities
	PlayerInputComponent->BindKey(EKeys::F1, IE_Pressed, this, &ASkylandersCharacter::Debug_LevelAbility1);
	PlayerInputComponent->BindKey(EKeys::F2, IE_Pressed, this, &ASkylandersCharacter::Debug_LevelAbility2);
	PlayerInputComponent->BindKey(EKeys::F3, IE_Pressed, this, &ASkylandersCharacter::Debug_LevelAbility3);
	PlayerInputComponent->BindKey(EKeys::F4, IE_Pressed, this, &ASkylandersCharacter::Debug_LevelAbility4);

	// Scoreboard (Tab hold)
	PlayerInputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ASkylandersCharacter::ShowScoreboard);
	PlayerInputComponent->BindKey(EKeys::Tab, IE_Released, this, &ASkylandersCharacter::HideScoreboard);

	// Debug shop keys: F5-F6 buy items, F7 sells last item
	PlayerInputComponent->BindKey(EKeys::F5, IE_Pressed, this, &ASkylandersCharacter::Debug_ShopBuy5);
	PlayerInputComponent->BindKey(EKeys::F6, IE_Pressed, this, &ASkylandersCharacter::Debug_ShopBuy6);
	PlayerInputComponent->BindKey(EKeys::F7, IE_Pressed, this, &ASkylandersCharacter::Debug_ShopSell);

	// Ability keys (1-4, always work)
	PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &ASkylandersCharacter::UseAbility1);
	PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ASkylandersCharacter::UseAbility2);
	PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ASkylandersCharacter::UseAbility3);
	PlayerInputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ASkylandersCharacter::UseAbility4);

	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Moving
		if (MoveAction)
		{
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASkylandersCharacter::Move);
			UE_LOG(LogTemp, Log, TEXT("Move action bound"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("MoveAction is NULL!"));
		}

		// Looking
		if (LookAction)
		{
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ASkylandersCharacter::Look);
			UE_LOG(LogTemp, Log, TEXT("Look action bound"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("LookAction is NULL! Mouse look will not work!"));
		}

		// Jumping
		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ACharacter::Jump);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		}

		// Firing
		if (FireAction)
		{
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Triggered, this, &ASkylandersCharacter::Fire);
		}

		// Abilities
		if (Ability1Action)
		{
			EnhancedInputComponent->BindAction(Ability1Action, ETriggerEvent::Triggered, this, &ASkylandersCharacter::UseAbility1);
		}
		if (Ability2Action)
		{
			EnhancedInputComponent->BindAction(Ability2Action, ETriggerEvent::Triggered, this, &ASkylandersCharacter::UseAbility2);
		}
		if (Ability3Action)
		{
			EnhancedInputComponent->BindAction(Ability3Action, ETriggerEvent::Triggered, this, &ASkylandersCharacter::UseAbility3);
		}
		if (Ability4Action)
		{
			EnhancedInputComponent->BindAction(Ability4Action, ETriggerEvent::Triggered, this, &ASkylandersCharacter::UseAbility4);
		}
	}
}

void ASkylandersCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ASkylandersCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);

		// Clamp pitch to SMITE-like range - camera always stays behind/above character
		APlayerController* PC = Cast<APlayerController>(Controller);
		if (PC && PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->ViewPitchMin = -20.0f; // Barely look up
			PC->PlayerCameraManager->ViewPitchMax = 40.0f;  // Moderate look down
		}
	}
}

void ASkylandersCharacter::Fire(const FInputActionValue& Value)
{
	FireProjectile();
}

// ========== DAMAGE AND HEALING ==========

void ASkylandersCharacter::TakeDamage_Custom(float DamageAmount)
{
	// Cancel recall if channeling
	if (bIsRecalling) CancelRecall();

	if (CurrentHealth <= 0.0f)
	{
		return; // Already dead
	}

	// Apply protections: damage reduction formula (like SMITE)
	// Reduction = Protections / (Protections + 100)
	float Prots = GetEffectiveProtections();
	float DamageReduction = Prots / (Prots + 100.0f);
	float FinalDamage = DamageAmount * (1.0f - DamageReduction);

	float EffectiveMaxHP = MaxHealth + ItemBonusStats.MaxHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth - FinalDamage, 0.0f, EffectiveMaxHP);

	// Spawn red damage number above player
	FVector NumberLoc = GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASkylandersDamageNumber* DmgNum = GetWorld()->SpawnActor<ASkylandersDamageNumber>(
		ASkylandersDamageNumber::StaticClass(), NumberLoc, FRotator::ZeroRotator, SpawnParams);
	if (DmgNum)
	{
		DmgNum->SetDamageNumber(DamageAmount, FColor::Red, false);
	}

	UpdateHUD();

	if (CurrentHealth <= 0.0f)
	{
		Die();
	}
	else
	{
		// Play hit reaction animation (only if still alive)
		PlayAnimOnSlot(HitReactAnim, 1.5f, 0.05f, 0.15f);
	}
}

void ASkylandersCharacter::Heal(float HealAmount)
{
	float EffectiveMaxHP = MaxHealth + ItemBonusStats.MaxHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth + HealAmount, 0.0f, EffectiveMaxHP);
	UpdateHUD();
	UE_LOG(LogTemp, Log, TEXT("Healed %f! Health: %f / %f"), HealAmount, CurrentHealth, MaxHealth);
}

// ========== MANA MANAGEMENT ==========

bool ASkylandersCharacter::UseMana(float ManaCost)
{
	if (CurrentMana >= ManaCost)
	{
		CurrentMana = FMath::Clamp(CurrentMana - ManaCost, 0.0f, MaxMana);
		UpdateHUD();
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("Not enough mana! Need %f, have %f"), ManaCost, CurrentMana);
	return false;
}

void ASkylandersCharacter::RestoreMana(float ManaAmount)
{
	float EffectiveMaxMP = MaxMana + ItemBonusStats.MaxMana;
	CurrentMana = FMath::Clamp(CurrentMana + ManaAmount, 0.0f, EffectiveMaxMP);
	UpdateHUD();
}

// ========== COINS ==========

void ASkylandersCharacter::AddCoins(int32 Amount)
{
	Coins += Amount;
	UE_LOG(LogTemp, Log, TEXT("Coins added: %d | Total: %d"), Amount, Coins);

	// Update coin display in HUD
	if (MainHUDWidget)
	{
		UFunction* AddCoinsFunc = MainHUDWidget->FindFunction(FName("AddCoins"));
		if (AddCoinsFunc)
		{
			struct FAddCoinsParams
			{
				int32 Amount;
			};

			FAddCoinsParams Params;
			Params.Amount = Amount;
			MainHUDWidget->ProcessEvent(AddCoinsFunc, &Params);
			UE_LOG(LogTemp, Log, TEXT("HUD coin display updated"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AddCoins function not found in widget!"));
		}
	}
}

// ========== LEVEL ==========

void ASkylandersCharacter::AddXP(float Amount)
{
	if (PlayerLevel >= MaxLevel) return;

	CurrentXP += Amount;
	UE_LOG(LogTemp, Log, TEXT("Gained %.0f XP! (%.0f / %.0f)"), Amount, CurrentXP, XPToNextLevel);

	// Check for level up (can multi-level from big XP gains)
	while (CurrentXP >= XPToNextLevel && PlayerLevel < MaxLevel)
	{
		CurrentXP -= XPToNextLevel;
		LevelUp();

		// Scale XP requirement: each level needs more
		XPToNextLevel = 100.0f + (PlayerLevel - 1) * 50.0f;
	}

	UpdateHUD();
}

void ASkylandersCharacter::LevelUp()
{
	PlayerLevel++;

	// Increase max stats
	float OldMaxHealth = MaxHealth;
	float OldMaxMana = MaxMana;

	MaxHealth += HealthPerLevel;
	MaxMana += ManaPerLevel;
	ManaRegenRate += ManaRegenPerLevel;
	BasePower += 2.0f; // +2 power per level

	// Heal the amount gained (don't full heal, just add the bonus)
	CurrentHealth += HealthPerLevel;
	CurrentMana += ManaPerLevel;

	// Clamp
	CurrentHealth = FMath::Min(CurrentHealth, MaxHealth);
	CurrentMana = FMath::Min(CurrentMana, MaxMana);

	// Grant 1 ability point per level
	AbilityPoints++;

	UE_LOG(LogTemp, Warning, TEXT("=== LEVEL UP! Level %d ==="), PlayerLevel);
	UE_LOG(LogTemp, Log, TEXT("  HP: %.0f -> %.0f | Mana: %.0f -> %.0f | Power: %.0f | Mana Regen: %.1f"),
		OldMaxHealth, MaxHealth, OldMaxMana, MaxMana, GetEffectivePower(), ManaRegenRate);

	// Play level up sound
	if (LevelUpSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, LevelUpSound, GetActorLocation());
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Yellow,
			FString::Printf(TEXT("LEVEL %d! NEW SKILL POINT AVAILABLE! (%d points) - F1-F4 to level abilities"), PlayerLevel, AbilityPoints));
	}

	UpdateHUD();
}

// ========== DEATH AND RESPAWN ==========

void ASkylandersCharacter::Die()
{
	Deaths++;
	UE_LOG(LogTemp, Warning, TEXT("Player Died! Deaths: %d"), Deaths);

	// Kill feed: player death
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(106, 4.0f, FColor::Red, TEXT("You have been slain!"));
	}

	// Play death sound
	if (DeathSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation());
	}

	// Play death animation
	PlayAnimOnSlot(DeathAnim, 1.0f, 0.1f, 0.0f);

	// Clean up any active VFX
	if (YamatoChargeVFX && IsValid(YamatoChargeVFX))
	{
		YamatoChargeVFX->Destroy();
		YamatoChargeVFX = nullptr;
	}
	bChargingYamato = false;
	bMachineGunActive = false;

	// Disable input and movement
	DisableInput(Cast<APlayerController>(GetController()));
	GetCharacterMovement()->DisableMovement();

	// Respawn time scales with level: 5s base + 2s per level
	float RespawnTime = 5.0f + (PlayerLevel * 2.0f);

	// After a short delay, hide the character and spectate the friendly titan
	FTimerHandle HideTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(HideTimerHandle, [this]()
	{
		// Hide character mesh, guns, and disable collision
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);

		// Find friendly titan and set camera to spectate it
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (PC)
		{
			TArray<AActor*> AllTitans;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersTitan::StaticClass(), AllTitans);
			for (AActor* Actor : AllTitans)
			{
				ASkylandersTitan* Titan = Cast<ASkylandersTitan>(Actor);
				if (Titan && Titan->Team == ETowerTeam::Friendly && !Titan->bDead)
				{
					PC->SetViewTargetWithBlend(Titan, 0.5f);
					break;
				}
			}
		}
	}, 0.75f, false); // Brief delay for death anim

	// Store respawn time for countdown display
	DeathRespawnTimeTotal = RespawnTime;
	DeathRespawnTimeRemaining = RespawnTime;

	GetWorld()->GetTimerManager().SetTimer(RespawnTimerHandle, this, &ASkylandersCharacter::Respawn, RespawnTime, false);
}

void ASkylandersCharacter::Respawn()
{
	UE_LOG(LogTemp, Log, TEXT("Respawning player..."));

	// Kill feed: player respawn
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(107, 3.0f, FColor::Green, TEXT("You have respawned!"));
	}

	// Restore health and mana to effective max (base + items)
	CurrentHealth = MaxHealth + ItemBonusStats.MaxHealth;
	CurrentMana = MaxMana + ItemBonusStats.MaxMana;

	// Teleport to spawn before showing
	AActor* PlayerStart = GetPlayerStart();
	if (PlayerStart)
	{
		SetActorLocation(PlayerStart->GetActorLocation());
		SetActorRotation(PlayerStart->GetActorRotation());
	}
	else
	{
		SetActorLocation(FVector(0.0f, 0.0f, 100.0f));
	}

	// Show character again and restore collision
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	// Restore camera to self
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		PC->SetViewTargetWithBlend(this, 0.3f);
	}

	// Re-enable movement and input
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	EnableInput(PC);

	UpdateHUD();
}

AActor* ASkylandersCharacter::GetPlayerStart()
{
	// Find the first Player Start in the level
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), FoundActors);

	if (FoundActors.Num() > 0)
	{
		return FoundActors[0];
	}

	return nullptr;
}

// ========== HUD UPDATES ==========

// Helper to set a float property on the widget by name
static void SetWidgetFloatProperty(UUserWidget* Widget, const FName& PropName, float Value)
{
	if (!Widget) return;
	FProperty* Prop = Widget->GetClass()->FindPropertyByName(PropName);
	if (Prop)
	{
		FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop);
		if (FloatProp)
		{
			FloatProp->SetPropertyValue_InContainer(Widget, Value);
			return;
		}
		// Try double (UE5 often uses double for Blueprint floats)
		FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop);
		if (DoubleProp)
		{
			DoubleProp->SetPropertyValue_InContainer(Widget, (double)Value);
		}
	}
}

void ASkylandersCharacter::UpdateHUD()
{
	if (!MainHUDWidget)
	{
		return;
	}

	// Safety check
	if (MaxHealth <= 0.0f) MaxHealth = 100.0f;
	if (MaxMana <= 0.0f) MaxMana = 100.0f;

	float EffectiveMaxHP = MaxHealth + ItemBonusStats.MaxHealth;
	float EffectiveMaxMP = MaxMana + ItemBonusStats.MaxMana;

	// Set the Blueprint's own variables directly so its bindings read correct values
	// Round to whole numbers to avoid long decimals in Blueprint text formatting
	float RoundedHP = FMath::FloorToFloat(CurrentHealth);
	float RoundedMaxHP = FMath::FloorToFloat(EffectiveMaxHP);
	float RoundedMP = FMath::FloorToFloat(CurrentMana);
	float RoundedMaxMP = FMath::FloorToFloat(EffectiveMaxMP);

	SetWidgetFloatProperty(MainHUDWidget, FName("Current Health"), RoundedHP);
	SetWidgetFloatProperty(MainHUDWidget, FName("Max Health"), RoundedMaxHP);
	SetWidgetFloatProperty(MainHUDWidget, FName("CurrentHealth"), RoundedHP);
	SetWidgetFloatProperty(MainHUDWidget, FName("MaxHealth"), RoundedMaxHP);
	SetWidgetFloatProperty(MainHUDWidget, FName("Current Mana"), RoundedMP);
	SetWidgetFloatProperty(MainHUDWidget, FName("Max Mana"), RoundedMaxMP);
	SetWidgetFloatProperty(MainHUDWidget, FName("CurrentMana"), RoundedMP);
	SetWidgetFloatProperty(MainHUDWidget, FName("MaxMana"), RoundedMaxMP);

	// Also directly set the widget components as a fallback
	URichTextBlock* HealthText = Cast<URichTextBlock>(MainHUDWidget->WidgetTree->FindWidget(FName("HealthText")));
	if (HealthText)
	{
		HealthText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), CurrentHealth, EffectiveMaxHP)));
	}

	UProgressBar* HealthBar = Cast<UProgressBar>(MainHUDWidget->WidgetTree->FindWidget(FName("HealthBar")));
	if (HealthBar)
	{
		HealthBar->SetPercent(CurrentHealth / EffectiveMaxHP);
	}

	URichTextBlock* ManaText = Cast<URichTextBlock>(MainHUDWidget->WidgetTree->FindWidget(FName("ManaText")));
	if (ManaText)
	{
		ManaText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), CurrentMana, EffectiveMaxMP)));
	}

	UProgressBar* ManaBar = Cast<UProgressBar>(MainHUDWidget->WidgetTree->FindWidget(FName("ManaBar")));
	if (ManaBar)
	{
		ManaBar->SetPercent(CurrentMana / EffectiveMaxMP);
	}

	FText LevelStr = FText::FromString(FString::Printf(TEXT("LVL %d"), PlayerLevel));
	UTextBlock* LevelText = Cast<UTextBlock>(MainHUDWidget->WidgetTree->FindWidget(FName("LevelText")));
	if (LevelText)
	{
		LevelText->SetText(LevelStr);
	}
	else
	{
		URichTextBlock* LevelRichText = Cast<URichTextBlock>(MainHUDWidget->WidgetTree->FindWidget(FName("LevelText")));
		if (LevelRichText)
		{
			LevelRichText->SetText(LevelStr);
		}
	}

	// XP Bar
	UProgressBar* XPBar = Cast<UProgressBar>(MainHUDWidget->WidgetTree->FindWidget(FName("XPBar")));
	if (XPBar)
	{
		float XPPercent = (XPToNextLevel > 0.0f) ? (CurrentXP / XPToNextLevel) : 0.0f;
		XPBar->SetPercent(XPPercent);
	}

	// Gold counter
	URichTextBlock* CoinText = Cast<URichTextBlock>(MainHUDWidget->WidgetTree->FindWidget(FName("CointCountText")));
	if (CoinText)
	{
		CoinText->SetText(FText::FromString(FString::Printf(TEXT("%d"), Coins)));
	}
	else
	{
		UTextBlock* CoinTextBlock = Cast<UTextBlock>(MainHUDWidget->WidgetTree->FindWidget(FName("CointCountText")));
		if (CoinTextBlock)
		{
			CoinTextBlock->SetText(FText::FromString(FString::Printf(TEXT("%d"), Coins)));
		}
	}

	// Inventory display (6 slots from inventory widget)
	if (InventoryHUDWidget)
	{
		for (int32 i = 0; i < InventoryHUDWidget->SlotTexts.Num() && i < MaxInventorySlots; i++)
		{
			UTextBlock* Slot = InventoryHUDWidget->SlotTexts[i];
			if (!Slot) continue;

			if (Inventory[i] > 0)
			{
				const FSkylandersItemData* Item = USkylandersItemCatalog::FindItem(Inventory[i]);
				if (Item)
				{
					Slot->SetText(FText::FromString(Item->ItemName));
					Slot->SetColorAndOpacity(FSlateColor(FLinearColor::White));
				}
			}
			else
			{
				Slot->SetText(FText::FromString(TEXT("---")));
				Slot->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.3f, 0.3f)));
			}
		}
	}
}

void ASkylandersCharacter::CreateInventoryHUD()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	InventoryHUDWidget = CreateWidget<USkylandersInventoryWidget>(PC, USkylandersInventoryWidget::StaticClass());
	if (InventoryHUDWidget)
	{
		InventoryHUDWidget->AddToViewport(5);
		UE_LOG(LogTemp, Log, TEXT("Inventory HUD created (2x3 grid, bottom-right)"));
	}
}

void ASkylandersCharacter::ShowScoreboard()
{
	if (ScoreboardWidget)
	{
		ScoreboardWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void ASkylandersCharacter::HideScoreboard()
{
	if (ScoreboardWidget)
	{
		ScoreboardWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

// ========== ABILITIES ==========

void ASkylandersCharacter::UpdateCooldowns(float DeltaTime)
{
	if (Ability1_RemainingCooldown > 0.0f)
	{
		Ability1_RemainingCooldown = FMath::Max(0.0f, Ability1_RemainingCooldown - DeltaTime);
	}

	if (Ability2_RemainingCooldown > 0.0f)
	{
		Ability2_RemainingCooldown = FMath::Max(0.0f, Ability2_RemainingCooldown - DeltaTime);
	}

	if (Ability3_RemainingCooldown > 0.0f)
	{
		Ability3_RemainingCooldown = FMath::Max(0.0f, Ability3_RemainingCooldown - DeltaTime);
	}

	if (Ability4_RemainingCooldown > 0.0f)
	{
		Ability4_RemainingCooldown = FMath::Max(0.0f, Ability4_RemainingCooldown - DeltaTime);
	}
}

// Helper to set text on either TextBlock or RichTextBlock
static void SetWidgetText(UWidget* Widget, const FText& Text)
{
	if (!Widget) return;

	if (UTextBlock* TB = Cast<UTextBlock>(Widget))
	{
		TB->SetText(Text);
	}
	else if (URichTextBlock* RTB = Cast<URichTextBlock>(Widget))
	{
		RTB->SetText(Text);
	}
}

void ASkylandersCharacter::UpdateCooldownUI()
{
	if (!MainHUDWidget) return;

	// Cache widget references once
	if (!bCooldownWidgetsCached)
	{
		// Widget names from WBP_MainHUD hierarchy
		const FName OverlayNames[] = {
			FName("CooldownOverlay1"), FName("CooldownOverlay2"),
			FName("CooldownOverlay3"), FName("CooldownOverlay4")
		};
		const FName TextNames[] = {
			FName("CoolDownText1"), FName("CoolDownText2"),
			FName("CoolDownText3"), FName("CoolDownText4")
		};

		bool bAllFound = true;
		for (int32 i = 0; i < 4; i++)
		{
			CooldownOverlay[i] = MainHUDWidget->WidgetTree->FindWidget(OverlayNames[i]);
			CooldownText[i] = MainHUDWidget->WidgetTree->FindWidget(TextNames[i]);

			if (!CooldownOverlay[i])
			{
				UE_LOG(LogTemp, Warning, TEXT("Could not find %s"), *OverlayNames[i].ToString());
				bAllFound = false;
			}
			if (!CooldownText[i])
			{
				UE_LOG(LogTemp, Warning, TEXT("Could not find %s"), *TextNames[i].ToString());
				bAllFound = false;
			}
		}

		if (bAllFound)
		{
			bCooldownWidgetsCached = true;
			UE_LOG(LogTemp, Log, TEXT("Cooldown UI widgets cached successfully!"));
		}
		else
		{
			return;
		}
	}

	// Cooldown values array for easy iteration
	float Cooldowns[] = {
		Ability1_RemainingCooldown, Ability2_RemainingCooldown,
		Ability3_RemainingCooldown, Ability4_RemainingCooldown
	};

	for (int32 i = 0; i < 4; i++)
	{
		if (!CooldownOverlay[i] || !CooldownText[i]) continue;

		if (Cooldowns[i] > 0.0f)
		{
			// On cooldown - show dark overlay and countdown text
			CooldownOverlay[i]->SetVisibility(ESlateVisibility::HitTestInvisible);
			CooldownOverlay[i]->SetRenderOpacity(0.85f);
			CooldownText[i]->SetVisibility(ESlateVisibility::HitTestInvisible);

			// Big gold countdown number with black outline, centered
			int32 SecondsLeft = FMath::CeilToInt(Cooldowns[i]);
			if (UTextBlock* TB = Cast<UTextBlock>(CooldownText[i]))
			{
				FSlateFontInfo Font = TB->GetFont();
				Font.Size = 28;
				Font.OutlineSettings.OutlineSize = 2;
				Font.OutlineSettings.OutlineColor = FLinearColor::Black;
				TB->SetFont(Font);
				TB->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.0f, 1.0f))); // Gold
				TB->SetJustification(ETextJustify::Center);
			}
			SetWidgetText(CooldownText[i], FText::FromString(FString::Printf(TEXT("%d"), SecondsLeft)));
		}
		else
		{
			// Ready - hide overlay and text
			CooldownOverlay[i]->SetVisibility(ESlateVisibility::Hidden);
			CooldownText[i]->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

// Ability 1 - Golden Super Charge: Piercing line skillshot
void ASkylandersCharacter::UseAbility1()
{
	// Not learned yet
	if (AbilityLevels[0] == 0)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Gatling Gun not learned! Press F1 to level it."));
		return;
	}

	// Cancel recall if channeling
	if (bIsRecalling) CancelRecall();

	if (bMachineGunActive || bChargingYamato) return; // Can't use while channeling

	if (Ability1_RemainingCooldown > 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Golden Super Charge on cooldown: %.1f seconds"), Ability1_RemainingCooldown);
		return;
	}

	if (UseMana(Ability1_ManaCost))
	{
		// Cooldown scales with rank: at rank 1 = 116%, rank 5 = 100%
		float CooldownScale = 1.2f - 0.04f * AbilityLevels[0];
		Ability1_RemainingCooldown = Ability1_Cooldown * CooldownScale;
		UE_LOG(LogTemp, Log, TEXT("Golden Super Charge!"));

		if (AbilitySound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, AbilitySound, GetActorLocation());
		}

		PlayAnimOnSlot(Ability1Anim, 1.0f);

		// Piercing line in aim direction (yaw only, fire flat like projectiles)
		FVector StartLoc = GetActorLocation() + FVector(0, 0, 20.0f);
		FVector AimDir = FRotator(0.0f, GetControlRotation().Yaw, 0.0f).Vector();
		float LineRange = 1500.0f;
		float LineWidth = 80.0f;
		// Damage scales with rank: at rank 1 = 68%, rank 5 = 100%
		float RankScale = 0.6f + 0.08f * AbilityLevels[0];
		float LineDamage = GetEffectivePower() * 3.5f * RankScale;
		FVector EndLoc = StartLoc + AimDir * LineRange;

		// Gold beam visual - spawned stretched cube mesh
		{
			FVector BeamMidpoint = (StartLoc + EndLoc) * 0.5f;
			FRotator BeamRotation = AimDir.Rotation();
			FActorSpawnParameters BeamParams;
			BeamParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AActor* BeamVFX = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), BeamMidpoint, BeamRotation, BeamParams);
			if (BeamVFX)
			{
				UStaticMeshComponent* BeamMesh = NewObject<UStaticMeshComponent>(BeamVFX);
				BeamVFX->SetRootComponent(BeamMesh);
				BeamMesh->RegisterComponent();
				UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube"));
				if (CubeMesh) BeamMesh->SetStaticMesh(CubeMesh);
				// Cube default is 100x100x100. Scale X = range/100, Y and Z thin
				BeamMesh->SetWorldScale3D(FVector(LineRange / 100.0f, LineWidth / 100.0f, 0.15f));
				BeamMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				BeamMesh->SetCastShadow(false);
				UMaterialInterface* BeamBaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
				if (BeamBaseMat)
				{
					UMaterialInstanceDynamic* BeamDynMat = UMaterialInstanceDynamic::Create(BeamBaseMat, BeamVFX);
					BeamDynMat->SetVectorParameterValue(FName("Color"), FLinearColor(1.0f, 0.85f, 0.0f)); // Gold
					BeamMesh->SetMaterial(0, BeamDynMat);
				}
				BeamVFX->SetLifeSpan(0.6f); // Auto-destroy after 0.6s
			}
		}

		// Hit ALL enemies in line (piercing)
		TArray<AActor*> AllEnemies;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersEnemy::StaticClass(), AllEnemies);

		int32 HitCount = 0;
		for (AActor* Actor : AllEnemies)
		{
			FVector ToEnemy = Actor->GetActorLocation() - StartLoc;
			float DotAlong = FVector::DotProduct(ToEnemy, AimDir);

			if (DotAlong > 0.0f && DotAlong < LineRange)
			{
				FVector ClosestPoint = StartLoc + AimDir * DotAlong;
				float PerpDist = FVector::Dist(Actor->GetActorLocation(), ClosestPoint);

				if (PerpDist < LineWidth)
				{
					ASkylandersEnemy* Enemy = Cast<ASkylandersEnemy>(Actor);
					if (Enemy && Enemy->CurrentHealth > 0.0f)
					{
						Enemy->TakeDamage_Custom(LineDamage, this);
						HitCount++;
						UE_LOG(LogTemp, Log, TEXT("Golden Super Charge pierced '%s' for %.0f damage!"), *Enemy->EnemyName, LineDamage);
					}
				}
			}
		}

		// Also hit enemy minions in the line
		TArray<AActor*> AllMinions;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersMinion::StaticClass(), AllMinions);

		for (AActor* Actor : AllMinions)
		{
			ASkylandersMinion* Minion = Cast<ASkylandersMinion>(Actor);
			if (Minion && Minion->Team == ETowerTeam::Enemy && !Minion->bDead)
			{
				FVector ToMinion = Actor->GetActorLocation() - StartLoc;
				float DotAlong = FVector::DotProduct(ToMinion, AimDir);

				if (DotAlong > 0.0f && DotAlong < LineRange)
				{
					FVector ClosestPoint = StartLoc + AimDir * DotAlong;
					float PerpDist = FVector::Dist(Actor->GetActorLocation(), ClosestPoint);

					if (PerpDist < LineWidth)
					{
						Minion->TakeDamage_Custom(LineDamage, this);
						HitCount++;
						UE_LOG(LogTemp, Log, TEXT("Golden Super Charge pierced minion for %.0f damage!"), LineDamage);
					}
				}
			}
		}

		// Also hit buff camps in the line
		TArray<AActor*> AllBuffCamps;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersBuffCamp::StaticClass(), AllBuffCamps);

		for (AActor* Actor : AllBuffCamps)
		{
			ASkylandersBuffCamp* BuffCamp = Cast<ASkylandersBuffCamp>(Actor);
			if (BuffCamp && !BuffCamp->bDead)
			{
				FVector ToBuffCamp = Actor->GetActorLocation() - StartLoc;
				float DotAlong = FVector::DotProduct(ToBuffCamp, AimDir);

				if (DotAlong > 0.0f && DotAlong < LineRange)
				{
					FVector ClosestPoint = StartLoc + AimDir * DotAlong;
					float PerpDist = FVector::Dist(Actor->GetActorLocation(), ClosestPoint);

					if (PerpDist < LineWidth)
					{
						BuffCamp->TakeDamage_Custom(LineDamage, this);
						HitCount++;
						UE_LOG(LogTemp, Log, TEXT("Golden Super Charge pierced buff camp '%s' for %.0f damage!"), *BuffCamp->CampName, LineDamage);
					}
				}
			}
		}

		// Also hit enemy gods in the line
		TArray<AActor*> AllEnemyGods;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersEnemyGod::StaticClass(), AllEnemyGods);

		for (AActor* Actor : AllEnemyGods)
		{
			ASkylandersEnemyGod* EnemyGod = Cast<ASkylandersEnemyGod>(Actor);
			if (EnemyGod && EnemyGod->CurrentState != EGodAIState::Dead)
			{
				FVector ToGod = Actor->GetActorLocation() - StartLoc;
				float DotAlong = FVector::DotProduct(ToGod, AimDir);

				if (DotAlong > 0.0f && DotAlong < LineRange)
				{
					FVector ClosestPoint = StartLoc + AimDir * DotAlong;
					float PerpDist = FVector::Dist(Actor->GetActorLocation(), ClosestPoint);

					if (PerpDist < LineWidth)
					{
						EnemyGod->TakeDamage_Custom(LineDamage, this);
						HitCount++;
						UE_LOG(LogTemp, Log, TEXT("Golden Super Charge pierced enemy god '%s' for %.0f damage!"), *EnemyGod->GodName, LineDamage);
					}
				}
			}
		}

		if (HitCount > 0)
		{
			AddCoins(HitCount);
		}
	}
}

// Ability 2 - Pot o' Gold: Small AoE damage at target location
void ASkylandersCharacter::UseAbility2()
{
	// Not learned yet
	if (AbilityLevels[1] == 0)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Pot o' Gold not learned! Press F2 to level it."));
		return;
	}

	// Cancel recall if channeling
	if (bIsRecalling) CancelRecall();

	if (bMachineGunActive || bChargingYamato) return;

	if (Ability2_RemainingCooldown > 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Pot o' Gold on cooldown: %.1f seconds"), Ability2_RemainingCooldown);
		return;
	}

	if (UseMana(Ability2_ManaCost))
	{
		// Cooldown scales with rank: at rank 1 = 116%, rank 5 = 100%
		float CooldownScale = 1.2f - 0.04f * AbilityLevels[1];
		Ability2_RemainingCooldown = Ability2_Cooldown * CooldownScale;
		UE_LOG(LogTemp, Log, TEXT("Pot o' Gold!"));

		if (AbilitySound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, AbilitySound, GetActorLocation());
		}

		PlayAnimOnSlot(Ability2Anim, 1.0f);

		// Spawn AoE where crosshair hits the ground (SMITE-style ground targeting)
		// Capped so circle center stays within range (range - radius)
		FVector SpawnLoc = GetGroundAimPoint(1050.0f - 250.0f);
		SpawnLoc.Z = GetActorLocation().Z; // Keep at ground level

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;

		ASkylandersAoEExplosion* AoE = GetWorld()->SpawnActor<ASkylandersAoEExplosion>(
			ASkylandersAoEExplosion::StaticClass(), SpawnLoc, FRotator::ZeroRotator, SpawnParams);

		if (AoE)
		{
			// Damage scales with rank: at rank 1 = 68%, rank 5 = 100%
			float RankScale = 0.6f + 0.08f * AbilityLevels[1];
			AoE->DamageAmount = GetEffectivePower() * 4.0f * RankScale;
			AoE->ExplosionRadius = 250.0f;
			AoE->DetonationDelay = 0.4f;
		}
	}
}

// Ability 3 - Golden Machine Gun: Stationary rapid-fire stance
void ASkylandersCharacter::UseAbility3()
{
	// Not learned yet
	if (AbilityLevels[2] == 0)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Golden Machine Gun not learned! Press F3 to level it."));
		return;
	}

	// Cancel recall if channeling
	if (bIsRecalling) CancelRecall();

	if (bChargingYamato) return; // Can't use while charging ult

	// If already active, cancel early
	if (bMachineGunActive)
	{
		bMachineGunActive = false;
		MachineGunRemainingTime = 0.0f;
		GetCharacterMovement()->MaxWalkSpeed = BaseMaxWalkSpeed;
		UE_LOG(LogTemp, Log, TEXT("Golden Machine Gun cancelled!"));
		return;
	}

	if (Ability3_RemainingCooldown > 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Golden Machine Gun on cooldown: %.1f seconds"), Ability3_RemainingCooldown);
		return;
	}

	if (UseMana(Ability3_ManaCost))
	{
		// Cooldown scales with rank: at rank 1 = 116%, rank 5 = 100%
		float CooldownScale = 1.2f - 0.04f * AbilityLevels[2];
		Ability3_RemainingCooldown = Ability3_Cooldown * CooldownScale;
		UE_LOG(LogTemp, Log, TEXT("Golden Machine Gun activated! Locked in place."));

		if (AbilitySound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, AbilitySound, GetActorLocation());
		}

		bMachineGunActive = true;
		MachineGunRemainingTime = MachineGunDuration;

		PlayAnimOnSlot(MachineGunStartAnim, 1.0f);

		// Lock movement
		GetCharacterMovement()->MaxWalkSpeed = 0.0f;
		GetCharacterMovement()->Velocity = FVector::ZeroVector;
	}
}

// Ability 4 - Golden Yamato Blast: Charged close-range nuke (ultimate)
void ASkylandersCharacter::UseAbility4()
{
	// Not learned yet
	if (AbilityLevels[3] == 0)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Yamato Cannon not learned! Press F4 to level it (requires level 5)."));
		return;
	}

	// Cancel recall if channeling
	if (bIsRecalling) CancelRecall();

	if (bMachineGunActive) return; // Can't ult during machine gun

	// If already charging, can't re-press
	if (bChargingYamato)
	{
		return;
	}

	if (Ability4_RemainingCooldown > 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Yamato Blast on cooldown: %.1f seconds"), Ability4_RemainingCooldown);
		return;
	}

	if (UseMana(Ability4_ManaCost))
	{
		// Cooldown scales with rank: at rank 1 = 116%, rank 5 = 100%
		float CooldownScale = 1.2f - 0.04f * AbilityLevels[3];
		Ability4_RemainingCooldown = Ability4_Cooldown * CooldownScale;
		UE_LOG(LogTemp, Warning, TEXT("YAMATO BLAST charging... Hold still!"));

		if (AbilitySound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, AbilitySound, GetActorLocation());
		}

		bChargingYamato = true;
		YamatoChargeRemaining = YamatoChargeTime;

		PlayAnimOnSlot(YamatoAnim, 1.0f, 0.15f, 0.15f);

		// Lock movement during charge
		GetCharacterMovement()->MaxWalkSpeed = 0.0f;
		GetCharacterMovement()->Velocity = FVector::ZeroVector;
	}
}

// ========== GROUND TARGETING ==========

FVector ASkylandersCharacter::GetGroundAimPoint(float MaxRange) const
{
	// Trace from camera through crosshair to find where it hits the ground
	FVector CamLoc = FollowCamera->GetComponentLocation();
	FVector CamFwd = FollowCamera->GetForwardVector();

	FVector TraceStart = CamLoc;
	FVector TraceEnd = CamLoc + CamFwd * 10000.0f;

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	FVector GroundPoint;
	if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		GroundPoint = HitResult.Location;
	}
	else
	{
		// No hit - project onto character's Z plane at max range
		FVector FlatDir = FRotator(0.0f, GetControlRotation().Yaw, 0.0f).Vector();
		GroundPoint = GetActorLocation() + FlatDir * MaxRange;
	}

	// Clamp distance from character
	FVector ToPoint = GroundPoint - GetActorLocation();
	ToPoint.Z = 0.0f; // Horizontal distance only
	float HorizDist = ToPoint.Size();
	if (HorizDist > MaxRange)
	{
		GroundPoint = GetActorLocation() + ToPoint.GetSafeNormal() * MaxRange;
		GroundPoint.Z = GetActorLocation().Z; // Keep at character height
	}

	// Minimum distance so it doesn't land on top of you
	if (HorizDist < 100.0f)
	{
		FVector FlatDir = FRotator(0.0f, GetControlRotation().Yaw, 0.0f).Vector();
		GroundPoint = GetActorLocation() + FlatDir * 100.0f;
	}

	return GroundPoint;
}

// ========== ANIMATION HELPER ==========

void ASkylandersCharacter::PlayAnimOnSlot(UAnimSequenceBase* Anim, float PlayRate, float BlendIn, float BlendOut, FName SlotName)
{
	if (!Anim) return;
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance)
	{
		AnimInstance->PlaySlotAnimationAsDynamicMontage(Anim, SlotName, BlendIn, BlendOut, PlayRate);
	}
}

// ========== PROJECTILE FIRING ==========

void ASkylandersCharacter::FireProjectile()
{
	// Cancel recall if channeling
	if (bIsRecalling) CancelRecall();

	// Can't manually fire during machine gun or yamato charge
	if (bMachineGunActive || bChargingYamato) return;

	// Check fire rate cooldown (base + item bonus attack speed)
	float CurrentTime = GetWorld()->GetTimeSeconds();
	float TimeSinceLastShot = CurrentTime - LastFireTime;
	float EffectiveFireRate = GetEffectiveAttackSpeed();
	float FireCooldown = 1.0f / EffectiveFireRate;

	if (TimeSinceLastShot < FireCooldown)
	{
		// Still on cooldown
		return;
	}

	if (!ProjectileClass)
	{
		UE_LOG(LogTemp, Error, TEXT("ProjectileClass not set!"));
		return;
	}

	// Update last fire time
	LastFireTime = CurrentTime;

	// Play attack sound
	if (AttackSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, AttackSound, GetActorLocation());
	}

	// === SMITE-style aiming: fire flat along facing direction, no pitch ===
	// Use only yaw from control rotation - projectiles always fly horizontal
	FRotator AimYaw(0.0f, GetControlRotation().Yaw, 0.0f);
	FVector ForwardDir = AimYaw.Vector();
	FVector RightDir = FRotationMatrix(AimYaw).GetUnitAxis(EAxis::Y);

	// Toggle gun for visual purposes
	bFireFromLeftGun = !bFireFromLeftGun;

	// Play attack animation on the correct arm slot only
	if (bFireFromLeftGun)
		PlayAnimOnSlot(AttackLeftAnim, 1.5f, 0.05f, 0.1f, FName("LeftArmSlot"));
	else
		PlayAnimOnSlot(AttackRightAnim, 1.5f, 0.05f, 0.1f, FName("RightArmSlot"));

	// Projectile always spawns from center and flies straight to crosshair (SMITE-style)
	FVector MuzzleLoc = GetActorLocation()
		+ ForwardDir * ProjectileSpawnOffset.X
		+ FVector::UpVector * ProjectileSpawnOffset.Z;

	FRotator SpawnRotation = AimYaw;

	// Spawn parameters
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Apply attack movement slow
	AttackSlowRemainingTime = AttackSlowDuration;

	AActor* Proj = GetWorld()->SpawnActor<AActor>(ProjectileClass, MuzzleLoc, SpawnRotation, SpawnParams);
	if (ASkylandersProjectile* SkyProj = Cast<ASkylandersProjectile>(Proj))
	{
		float FinalDmg = GetEffectivePower();
		bool bCrit = FMath::FRand() < ItemBonusStats.CritChance;
		if (bCrit) FinalDmg *= 2.0f;

		SkyProj->Damage = FinalDmg;
		SkyProj->Lifesteal = ItemBonusStats.Lifesteal;
		SkyProj->bIsCrit = bCrit;
	}
}

// ========== SHOP & INVENTORY ==========

void ASkylandersCharacter::ToggleShop()
{
	if (bIsShopOpen)
	{
		CloseShop();
	}
	else
	{
		OpenShop();
	}
}

void ASkylandersCharacter::OpenShop()
{
	if (bIsShopOpen) return;

	bIsShopOpen = true;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	// Create shop widget if not already created
	if (!ShopWidget && ShopWidgetClass)
	{
		ShopWidget = CreateWidget<UUserWidget>(PC, ShopWidgetClass);
	}

	if (ShopWidget)
	{
		ShopWidget->AddToViewport(20); // Above HUD
		ShopWidget->SetVisibility(ESlateVisibility::Visible);

		// Show mouse cursor for shop interaction
		PC->bShowMouseCursor = true;
		PC->SetInputMode(FInputModeGameAndUI());
	}
	else
	{
		// No shop widget class set - use debug message as fallback
		UE_LOG(LogTemp, Warning, TEXT("ShopWidgetClass not set! Using debug shop."));

		if (GEngine)
		{
			FString ShopText = TEXT("=== SHOP (B to close) ===\n");
			ShopText += bIsInSpawnArea ? TEXT("IN SPAWN - Can Buy!\n") : TEXT("NOT in spawn - Browse only\n");
			ShopText += FString::Printf(TEXT("Gold: %d\n\n"), Coins);

			const TArray<FSkylandersItemData>& Items = USkylandersItemCatalog::GetAllItems();
			for (const FSkylandersItemData& Item : Items)
			{
				ShopText += FString::Printf(TEXT("[%d] %s - %d gold - %s\n"),
					Item.ItemID, *Item.ItemName, Item.Cost, *Item.Description);
			}

			ShopText += TEXT("\nInventory: ");
			for (int32 i = 0; i < MaxInventorySlots; i++)
			{
				if (Inventory[i] > 0)
				{
					const FSkylandersItemData* Item = USkylandersItemCatalog::FindItem(Inventory[i]);
					ShopText += Item ? FString::Printf(TEXT("[%s] "), *Item->ItemName) : TEXT("[???] ");
				}
				else
				{
					ShopText += TEXT("[Empty] ");
				}
			}

			GEngine->AddOnScreenDebugMessage(100, 30.0f, FColor::Yellow, ShopText);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Shop opened. In spawn: %s"), bIsInSpawnArea ? TEXT("YES") : TEXT("NO"));
}

void ASkylandersCharacter::CloseShop()
{
	if (!bIsShopOpen) return;

	bIsShopOpen = false;

	if (ShopWidget)
	{
		ShopWidget->RemoveFromParent();
	}

	// Clear debug message shop
	if (GEngine)
	{
		GEngine->RemoveOnScreenDebugMessage(100);
	}

	// Restore game-only input
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
	}

	UE_LOG(LogTemp, Log, TEXT("Shop closed."));
}

bool ASkylandersCharacter::BuyItem(int32 ItemID)
{
	// Must be in spawn area to buy
	if (!bIsInSpawnArea)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot buy items outside spawn area!"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Must be in spawn area to buy!"));
		}
		return false;
	}

	// Find the item
	const FSkylandersItemData* Item = USkylandersItemCatalog::FindItem(ItemID);
	if (!Item)
	{
		UE_LOG(LogTemp, Warning, TEXT("Item ID %d not found!"), ItemID);
		return false;
	}

	// Check gold
	if (Coins < Item->Cost)
	{
		UE_LOG(LogTemp, Warning, TEXT("Not enough gold! Need %d, have %d"), Item->Cost, Coins);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
				FString::Printf(TEXT("Not enough gold! Need %d"), Item->Cost));
		}
		return false;
	}

	// Find empty inventory slot
	int32 EmptySlot = -1;
	for (int32 i = 0; i < MaxInventorySlots; i++)
	{
		if (Inventory[i] == 0)
		{
			EmptySlot = i;
			break;
		}
	}

	if (EmptySlot < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Inventory full! Cannot buy %s"), *Item->ItemName);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Inventory full!"));
		}
		return false;
	}

	// Purchase successful
	Coins -= Item->Cost;
	Inventory[EmptySlot] = ItemID;

	RecalculateItemBonuses();

	// Play item buy sound
	if (ItemBuySound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ItemBuySound, GetActorLocation());
	}

	UE_LOG(LogTemp, Log, TEXT("Bought %s for %d gold (slot %d). Gold remaining: %d"),
		*Item->ItemName, Item->Cost, EmptySlot, Coins);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
			FString::Printf(TEXT("Bought %s!"), *Item->ItemName));
	}

	UpdateHUD();
	return true;
}

bool ASkylandersCharacter::SellItem(int32 SlotIndex)
{
	// Must be in spawn area to sell
	if (!bIsInSpawnArea)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot sell items outside spawn area!"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Must be in spawn area to sell!"));
		}
		return false;
	}

	if (SlotIndex < 0 || SlotIndex >= MaxInventorySlots)
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid inventory slot: %d"), SlotIndex);
		return false;
	}

	if (Inventory[SlotIndex] == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Slot %d is empty!"), SlotIndex);
		return false;
	}

	const FSkylandersItemData* Item = USkylandersItemCatalog::FindItem(Inventory[SlotIndex]);
	if (!Item)
	{
		Inventory[SlotIndex] = 0;
		return false;
	}

	int32 SellPrice = Item->GetSellPrice();
	FString ItemName = Item->ItemName;

	// Sell it
	Coins += SellPrice;
	Inventory[SlotIndex] = 0;

	RecalculateItemBonuses();

	UE_LOG(LogTemp, Log, TEXT("Sold %s for %d gold. Gold: %d"), *ItemName, SellPrice, Coins);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
			FString::Printf(TEXT("Sold %s for %d gold"), *ItemName, SellPrice));
	}

	UpdateHUD();
	return true;
}

void ASkylandersCharacter::RecalculateItemBonuses()
{
	// Reset bonus stats
	ItemBonusStats = FSkylandersItemStats();

	// Sum all equipped item stats
	for (int32 i = 0; i < MaxInventorySlots; i++)
	{
		if (Inventory[i] > 0)
		{
			const FSkylandersItemData* Item = USkylandersItemCatalog::FindItem(Inventory[i]);
			if (Item)
			{
				ItemBonusStats = ItemBonusStats + Item->Stats;
			}
		}
	}

	// Apply max health/mana changes
	// Effective max = base max (with level scaling already applied) + item bonus
	// Don't let current exceed effective max
	float EffectiveMaxHealth = MaxHealth + ItemBonusStats.MaxHealth;
	float EffectiveMaxMana = MaxMana + ItemBonusStats.MaxMana;
	CurrentHealth = FMath::Min(CurrentHealth, EffectiveMaxHealth);
	CurrentMana = FMath::Min(CurrentMana, EffectiveMaxMana);

	// Apply movement speed bonus
	BaseMaxWalkSpeed = 500.0f + ItemBonusStats.MovementSpeed;

	UE_LOG(LogTemp, Log, TEXT("Item bonuses recalculated: +%.0f Power, +%.1f AS, +%.0f HP, +%.0f MP, +%.0f Prot, +%.0f MS"),
		ItemBonusStats.Power, ItemBonusStats.AttackSpeed, ItemBonusStats.MaxHealth,
		ItemBonusStats.MaxMana, ItemBonusStats.Protections, ItemBonusStats.MovementSpeed);
}

float ASkylandersCharacter::GetEffectivePower() const
{
	return BasePower + ItemBonusStats.Power;
}

float ASkylandersCharacter::GetEffectiveAttackSpeed() const
{
	return FireRate + ItemBonusStats.AttackSpeed;
}

float ASkylandersCharacter::GetEffectiveProtections() const
{
	return ItemBonusStats.Protections;
}

// ========== ABILITY LEVELING ==========

void ASkylandersCharacter::LevelUpAbility(int32 AbilityIndex)
{
	if (!CanLevelAbility(AbilityIndex))
	{
		if (GEngine)
		{
			if (AbilityPoints <= 0)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("No skill points available!"));
			}
			else if (AbilityIndex >= 0 && AbilityIndex < 4 && AbilityLevels[AbilityIndex] >= MaxAbilityRank)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Ability already at max rank!"));
			}
			else if (AbilityIndex == 3)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Ultimate not available at this level!"));
			}
		}
		return;
	}

	AbilityLevels[AbilityIndex]++;
	AbilityPoints--;

	// Show on-screen feedback
	FString AbilityNames[] = {TEXT("Gatling Gun"), TEXT("Pot o' Gold"), TEXT("Golden Machine Gun"), TEXT("Yamato Cannon")};
	if (GEngine)
	{
		FString Msg = FString::Printf(TEXT("%s leveled to Rank %d! (%d points remaining)"),
			*AbilityNames[AbilityIndex], AbilityLevels[AbilityIndex], AbilityPoints);
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, Msg);
	}

	UE_LOG(LogTemp, Log, TEXT("Ability %d (%s) leveled to rank %d. Points remaining: %d"),
		AbilityIndex, *AbilityNames[AbilityIndex], AbilityLevels[AbilityIndex], AbilityPoints);
}

bool ASkylandersCharacter::CanLevelAbility(int32 AbilityIndex) const
{
	if (AbilityIndex < 0 || AbilityIndex > 3) return false;
	if (AbilityPoints <= 0) return false;
	if (AbilityLevels[AbilityIndex] >= MaxAbilityRank) return false;

	// Ultimate (index 3): only at levels 5, 9, 13, 17
	if (AbilityIndex == 3)
	{
		if (PlayerLevel < 5 + AbilityLevels[3] * 4) return false;
	}

	return true;
}

int32 ASkylandersCharacter::GetAbilityRank(int32 AbilityIndex) const
{
	if (AbilityIndex < 0 || AbilityIndex > 3) return 0;
	return AbilityLevels[AbilityIndex];
}

// Ability leveling key handlers
void ASkylandersCharacter::Debug_LevelAbility1() { LevelUpAbility(0); }
void ASkylandersCharacter::Debug_LevelAbility2() { LevelUpAbility(1); }
void ASkylandersCharacter::Debug_LevelAbility3() { LevelUpAbility(2); }
void ASkylandersCharacter::Debug_LevelAbility4() { LevelUpAbility(3); }

// Debug shop: F5-F6 buy items by catalog position, F7 sells last occupied slot
// These work when shop is open OR closed (for quick testing)
void ASkylandersCharacter::Debug_ShopBuy5() { BuyItem(5); }
void ASkylandersCharacter::Debug_ShopBuy6() { BuyItem(6); }

void ASkylandersCharacter::Debug_ShopSell()
{
	// Sell the last occupied slot
	for (int32 i = MaxInventorySlots - 1; i >= 0; i--)
	{
		if (Inventory[i] > 0)
		{
			SellItem(i);
			return;
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("No items to sell!"));
	}
}

// ========== RECALL TO BASE ==========

void ASkylandersCharacter::StartRecall()
{
	// Ignore if already recalling
	if (bIsRecalling) return;

	// Ignore if dead
	if (CurrentHealth <= 0.0f) return;

	// Ignore if channeling abilities
	if (bMachineGunActive || bChargingYamato) return;

	bIsRecalling = true;
	RecallRemainingTime = RecallChannelTime;

	// Stop movement
	GetCharacterMovement()->MaxWalkSpeed = 0.0f;
	GetCharacterMovement()->Velocity = FVector::ZeroVector;

	UE_LOG(LogTemp, Log, TEXT("Recall started! Channeling for %.1f seconds..."), RecallChannelTime);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(200, RecallChannelTime + 1.0f, FColor::Cyan,
			FString::Printf(TEXT("Recalling... %ds"), FMath::CeilToInt(RecallChannelTime)));
	}
}

void ASkylandersCharacter::CancelRecall()
{
	if (!bIsRecalling) return;

	bIsRecalling = false;
	RecallRemainingTime = 0.0f;

	// Restore walk speed
	GetCharacterMovement()->MaxWalkSpeed = BaseMaxWalkSpeed;

	UE_LOG(LogTemp, Log, TEXT("Recall cancelled!"));

	if (GEngine)
	{
		GEngine->RemoveOnScreenDebugMessage(200);
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Recall cancelled!"));
	}
}

void ASkylandersCharacter::CompleteRecall()
{
	bIsRecalling = false;
	RecallRemainingTime = 0.0f;

	// Restore walk speed
	GetCharacterMovement()->MaxWalkSpeed = BaseMaxWalkSpeed;

	// Teleport to player start
	AActor* PlayerStart = GetPlayerStart();
	if (PlayerStart)
	{
		SetActorLocation(PlayerStart->GetActorLocation());
		SetActorRotation(PlayerStart->GetActorRotation());
		UE_LOG(LogTemp, Log, TEXT("Recall complete! Teleported to spawn."));
	}
	else
	{
		SetActorLocation(FVector(0.0f, 0.0f, 100.0f));
		UE_LOG(LogTemp, Warning, TEXT("No Player Start found for recall, teleported to origin."));
	}

	if (GEngine)
	{
		GEngine->RemoveOnScreenDebugMessage(200);
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("Recalled!"));
	}
}

void ASkylandersCharacter::TeleportToBase()
{
	if (CurrentHealth <= 0.0f) return; // Can't teleport while dead

	// Cancel recall if channeling
	if (bIsRecalling) CancelRecall();

	AActor* PlayerStart = GetPlayerStart();
	if (PlayerStart)
	{
		SetActorLocation(PlayerStart->GetActorLocation());
		SetActorRotation(PlayerStart->GetActorRotation());
	}
	else
	{
		SetActorLocation(FVector(0.0f, 0.0f, 100.0f));
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Cyan, TEXT("Teleported to base!"));
	}
}

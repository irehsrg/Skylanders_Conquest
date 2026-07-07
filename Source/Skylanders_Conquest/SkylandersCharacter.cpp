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
#include "InputMappingContext.h"
#include "InputAction.h"
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
#include "SkylandersSpawnArea.h"
#include "SkylandersTitan.h"
#include "SkylandersItemCatalog.h"
#include "SkylandersSimpleAnimInstance.h"
#include "SkylandersInventoryWidget.h"
#include "SkylandersItemHUDWidget.h"
#include "SkylandersMinimapWidget.h"
#include "SkylandersScoreboardWidget.h"
#include "SkylandersKillFeedWidget.h"
#include "SkylandersMatchStatusWidget.h"
#include "Sound/SoundBase.h"
#include "DrawDebugHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
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

	// SMITE-flat feel: the player lives on the ground plane. A low step height
	// means walking into a unit, camp, or structure can never let the capsule
	// ride up and over it — no accidental verticality. Units overlap the player
	// (set on each unit), so nothing can shove the player skyward either.
	GetCharacterMovement()->MaxStepHeight = 15.0f;

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

	// Ground aim LINE - thin flat cube from the character to the aim circle
	// (SMITE slider). World-space so it can stretch/orient independently.
	GroundAimLine = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GroundAimLine"));
	GroundAimLine->SetupAttachment(RootComponent);
	GroundAimLine->SetAbsolute(true, true, true);
	GroundAimLine->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GroundAimLine->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> AimLineMesh(TEXT("/Engine/BasicShapes/Cube"));
	if (AimLineMesh.Succeeded())
	{
		GroundAimLine->SetStaticMesh(AimLineMesh.Object);
	}
	GroundAimLine->SetVisibility(false);

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

	// Character identity (subclasses override in their constructors)
	CharacterName = TEXT("Trigger Happy");
	CharacterRole = TEXT("Gunslinger");
	AbilityNames[0] = TEXT("Gatling Gun");
	AbilityNames[1] = TEXT("Pot o' Gold");
	AbilityNames[2] = TEXT("Golden Machine Gun");
	AbilityNames[3] = TEXT("Yamato Cannon");

	// Starting stats (applied in BeginPlay; subclasses set their own)
	StartingMaxHealth = 100.0f;
	StartingMaxMana = 100.0f;
	StartingManaRegen = 5.0f;
	StartingBasePower = 20.0f;
	PowerPerLevel = 2.0f;

	// Initialize stats - CRITICAL: These MUST be non-zero to avoid divide by zero in HUD
	MaxHealth = 100.0f;
	CurrentHealth = 100.0f; // Changed from MaxHealth to ensure it's 100
	MaxMana = 100.0f;
	CurrentMana = 100.0f; // Changed from MaxMana to ensure it's 100
	ManaRegenRate = 5.0f; // 5 mana per second
	PlayerLevel = 1;
	Coins = 0;
	TotalGoldEarned = 0;
	BuffProtections = 0.0f;

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
	AutoAttackRange = 1050.0f; // Projectile speed (3000) * lifetime (0.35)
	AutoAttackProjectileColor = FLinearColor(1.0f, 0.85f, 0.1f, 1.0f); // Gold
	AutoAttackProjectileScale = 0.4f;
	LastFireTime = -999.0f; // Allow immediate first shot
	bFireFromLeftGun = true; // Start with left gun

	// Cleave (off by default; characters opt in)
	CleaveEveryNthHit = 0;
	CleaveRadius = 0.0f;
	CleaveDamageFraction = 0.5f;
	AutoAttackCounter = 0;

	// Ability aiming (all instant-cast by default; characters opt specific
	// abilities into ground targeting)
	AimingAbilityIndex = -1;
	for (int32 i = 0; i < 4; i++)
	{
		bAbilityUsesGroundAim[i] = false;
		AbilityAimRadius[i] = 250.0f;
		AbilityAimRange[i] = 1050.0f;
	}
	CurrentAimTarget = nullptr;
	HighlightMaterial = nullptr;

	// Trigger Happy's Pot o' Gold (ability 2 / index 1) is ground-placed.
	// Subclasses adjust these flags in their own constructors.
	bAbilityUsesGroundAim[1] = true;

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

	// HUD widget cache
	bHUDRefsCached = false;
	CachedHealthText = nullptr;
	CachedHealthBar = nullptr;
	CachedManaText = nullptr;
	CachedManaBar = nullptr;
	CachedLevelText = nullptr;
	CachedXPBar = nullptr;
	CachedCoinText = nullptr;

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
	HealthRegenRate = 1.6f; // ~HP8/s baseline (SMITE-like HP5 ≈ 8); boosted near fountain

	// Animation references (set in Blueprint Class Defaults after importing)
	IdleLocomotionAnim = nullptr;
	RunLocomotionAnim = nullptr;
	JumpAnim = nullptr;
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

	// Per-character HUD art (subclasses load their own; null = keep HUD defaults)
	for (int32 i = 0; i < 4; i++)
	{
		AbilityIconTextures[i] = nullptr;
	}
	PortraitTexture = nullptr;

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
	MaxHealth = StartingMaxHealth;
	CurrentHealth = StartingMaxHealth;
	MaxMana = StartingMaxMana;
	CurrentMana = StartingMaxMana;
	ManaRegenRate = StartingManaRegen;
	BasePower = StartingBasePower;
	PlayerLevel = 1;

	// Start at level 3: apply 2 level-ups worth of stats (level 1 -> 3)
	for (int32 i = 0; i < 2; i++)
	{
		PlayerLevel++;
		MaxHealth += HealthPerLevel;
		MaxMana += ManaPerLevel;
		ManaRegenRate += ManaRegenPerLevel;
		BasePower += PowerPerLevel;
		CurrentHealth += HealthPerLevel;
		CurrentMana += ManaPerLevel;
	}
	// Clamp
	CurrentHealth = FMath::Min(CurrentHealth, MaxHealth);
	CurrentMana = FMath::Min(CurrentMana, MaxMana);

	// XP requirement must match the boosted starting level
	XPToNextLevel = 100.0f + (PlayerLevel - 1) * 50.0f;

	// 3 ability points to spend (one per level)
	AbilityPoints = 3;

	// Reset ability levels
	for (int32 i = 0; i < 4; i++)
	{
		AbilityLevels[i] = 0;
	}

	// Per-character meshes/materials (Trigger Happy guns by default; subclasses override)
	LoadCharacterVisuals();

	// Push locomotion loops into the code-driven anim instance (Hex, Tree Rex)
	if (USkylandersSimpleAnimInstance* Simple = Cast<USkylandersSimpleAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		Simple->IdleAnim = IdleLocomotionAnim;
		Simple->RunAnim = RunLocomotionAnim;
	}

	// Set up VFX indicator materials
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
	if (BaseMat)
	{
		// Cyan translucent material for recall indicator
		if (RecallIndicator)
		{
			UMaterialInstanceDynamic* RecallMat = UMaterialInstanceDynamic::Create(BaseMat, this);
			RecallMat->SetVectorParameterValue(FName("Color"), FLinearColor(0.0f, 0.8f, 1.0f, 0.5f));
			RecallIndicator->SetMaterial(0, RecallMat);
		}
	}

	// Targeter circle + line use a genuinely translucent material so they read as
	// a see-through slider that never blocks the view (the BasicShapeMaterial is
	// opaque — its alpha does nothing, which is why the old circle "covered" the
	// ground). Falls back to a tinted BasicShape MID if the asset is missing.
	UMaterialInterface* TargeterMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/VFX/M_AimTargeter.M_AimTargeter"));
	if (GroundAimIndicator)
	{
		UMaterialInstanceDynamic* Mid = TargeterMat
			? UMaterialInstanceDynamic::Create(TargeterMat, this)
			: (BaseMat ? UMaterialInstanceDynamic::Create(BaseMat, this) : nullptr);
		if (Mid)
		{
			Mid->SetVectorParameterValue(FName("Color"), FLinearColor(1.0f, 0.85f, 0.0f, 1.0f));
			Mid->SetScalarParameterValue(FName("Opacity"), 0.30f);
			GroundAimIndicator->SetMaterial(0, Mid);
		}
	}
	if (GroundAimLine)
	{
		UMaterialInstanceDynamic* Mid = TargeterMat
			? UMaterialInstanceDynamic::Create(TargeterMat, this)
			: (BaseMat ? UMaterialInstanceDynamic::Create(BaseMat, this) : nullptr);
		if (Mid)
		{
			Mid->SetVectorParameterValue(FName("Color"), FLinearColor(1.0f, 0.95f, 0.6f, 1.0f));
			Mid->SetScalarParameterValue(FName("Opacity"), 0.45f);
			GroundAimLine->SetMaterial(0, Mid);
		}
	}

	// Highlight overlay for aimed enemies (a translucent emissive material,
	// authored as /Game/VFX/M_Highlight; falls back to null if absent)
	if (!HighlightMaterial)
	{
		HighlightMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/VFX/M_Highlight.M_Highlight"));
	}

	// HUD/shop/projectile fallbacks for characters without Blueprint class
	// defaults (widgets are only needed from BeginPlay onward)
	if (!MainHUDClass)
		MainHUDClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UserInterface/WBP_MainHUD.WBP_MainHUD_C"));
	if (!ShopWidgetClass)
		ShopWidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UserInterface/WBP_Shop.WBP_Shop_C"));
	if (!ProjectileClass)
		ProjectileClass = ASkylandersProjectile::StaticClass();

	UE_LOG(LogTemp, Warning, TEXT("=== CHARACTER BEGIN PLAY ==="));
	UE_LOG(LogTemp, Warning, TEXT("MaxHealth: %.1f, CurrentHealth: %.1f"), MaxHealth, CurrentHealth);
	UE_LOG(LogTemp, Warning, TEXT("MaxMana: %.1f, CurrentMana: %.1f"), MaxMana, CurrentMana);

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		// The main menu leaves the viewport in UI-only input mode, which survives
		// level travel — force game input at match start.
		PlayerController->SetInputMode(FInputModeGameOnly());
		PlayerController->bShowMouseCursor = false;

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

				// Kill feed (top-right, auto-expiring entries)
				if (USkylandersKillFeedWidget* Feed = CreateWidget<USkylandersKillFeedWidget>(PlayerController, USkylandersKillFeedWidget::StaticClass()))
				{
					Feed->AddToViewport(6);
				}

				// Match status bar (team gold + timer, top-center)
				if (USkylandersMatchStatusWidget* Status = CreateWidget<USkylandersMatchStatusWidget>(PlayerController, USkylandersMatchStatusWidget::StaticClass()))
				{
					Status->AddToViewport(6);
				}

				// Initial HUD update + per-character icon/portrait swap
				UpdateHUD();
				ApplyCharacterHUDArt();

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

	// Game timer (displayed by the match status bar widget now)
	GameElapsedTime += DeltaTime;

	// Update ability cooldowns
	UpdateCooldowns(DeltaTime);
	UpdateCooldownUI();

	// SMITE-style aiming: the line+circle targeter only appears while a ground
	// ability is being aimed (press-and-hold); it's hidden the rest of the time.
	UpdateAimTargeter();

	// Highlight the enemy the auto attack would currently hit
	UpdateAimHighlight();

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

		// Keep the firing loop animation running once the start anim finishes
		if (MachineGunLoopAnim)
		{
			UAnimInstance* AnimInst = GetMesh()->GetAnimInstance();
			if (AnimInst && !AnimInst->Montage_IsPlaying(nullptr))
			{
				PlayAnimOnSlot(MachineGunLoopAnim, 1.0f);
			}
		}

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
					SkyProj->bDamagesStructures = false; // Ability shots never damage towers/titans
					SkyProj->ProjectileColor = AutoAttackProjectileColor;
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

			// Big visual explosion - red sphere that auto-destroys
			// (sphere default radius = 50, scale to YamatoRange)
			SpawnColoredMeshVFX(TEXT("/Engine/BasicShapes/Sphere"),
				BlastCenter, FRotator::ZeroRotator,
				FVector(YamatoRange / 50.0f),
				FLinearColor(1.0f, 0.2f, 0.0f), 1.0f);

			// Scale Yamato damage with power (20x power scaling for ultimate)
			// Ultimate rank scaling: at rank 1 = 60%, rank 5 = 100%
			float UltRankScale = 0.5f + 0.10f * AbilityLevels[3];
			float ScaledYamatoDmg = GetEffectivePower() * 20.0f * UltRankScale;

			// Close-range frontal cone (~70 degrees), closer targets take more damage
			int32 HitCount = DamageEnemiesInCone(GetActorLocation(), GetActorForwardVector(),
				YamatoRange, 0.3f, ScaledYamatoDmg, 0.3f);
			UE_LOG(LogTemp, Warning, TEXT("YAMATO BLAST hit %d targets! Damage: %.0f, Range: %.0f"), HitCount, ScaledYamatoDmg, YamatoRange);

			if (HitCount > 0)
			{
				AddCoins(HitCount * 5);
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

	// Channeled abilities lock movement — don't let the per-frame speed recompute undo it
	if (IsChanneling())
	{
		GetCharacterMovement()->MaxWalkSpeed = 0.0f;
	}
	else
	{
		GetCharacterMovement()->MaxWalkSpeed = BaseMaxWalkSpeed * SpeedMultiplier;
	}

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

	// Shared input assets for characters without Blueprint class defaults
	// (Hex, Tree Rex). Loaded here because this runs BEFORE BeginPlay on level
	// travel, and the base CDO constructs too early in engine init to use
	// ConstructorHelpers for these. Trigger Happy's Blueprint overrides them.
	if (!DefaultMappingContext)
		DefaultMappingContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_Default.IMC_Default"));
	if (!MoveAction)
		MoveAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Move.IA_Move"));
	if (!LookAction)
		LookAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Look.IA_Look"));
	if (!JumpAction)
		JumpAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Jump.IA_Jump"));
	if (!FireAction)
		FireAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Fire.IA_Fire"));

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

	// Ability keys (1-4). Press starts the cast (or begins aiming for a ground
	// ability); release fires an aimed ability at the targeter.
	PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &ASkylandersCharacter::AbilityPressed1);
	PlayerInputComponent->BindKey(EKeys::One, IE_Released, this, &ASkylandersCharacter::AbilityReleased1);
	PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ASkylandersCharacter::AbilityPressed2);
	PlayerInputComponent->BindKey(EKeys::Two, IE_Released, this, &ASkylandersCharacter::AbilityReleased2);
	PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ASkylandersCharacter::AbilityPressed3);
	PlayerInputComponent->BindKey(EKeys::Three, IE_Released, this, &ASkylandersCharacter::AbilityReleased3);
	PlayerInputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ASkylandersCharacter::AbilityPressed4);
	PlayerInputComponent->BindKey(EKeys::Four, IE_Released, this, &ASkylandersCharacter::AbilityReleased4);

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

		// Abilities — Started (press edge) begins the cast/aim, Completed (release)
		// fires an aimed ground ability. Mirrors the 1-4 key bindings above.
		if (Ability1Action)
		{
			EnhancedInputComponent->BindAction(Ability1Action, ETriggerEvent::Started, this, &ASkylandersCharacter::AbilityPressed1);
			EnhancedInputComponent->BindAction(Ability1Action, ETriggerEvent::Completed, this, &ASkylandersCharacter::AbilityReleased1);
		}
		if (Ability2Action)
		{
			EnhancedInputComponent->BindAction(Ability2Action, ETriggerEvent::Started, this, &ASkylandersCharacter::AbilityPressed2);
			EnhancedInputComponent->BindAction(Ability2Action, ETriggerEvent::Completed, this, &ASkylandersCharacter::AbilityReleased2);
		}
		if (Ability3Action)
		{
			EnhancedInputComponent->BindAction(Ability3Action, ETriggerEvent::Started, this, &ASkylandersCharacter::AbilityPressed3);
			EnhancedInputComponent->BindAction(Ability3Action, ETriggerEvent::Completed, this, &ASkylandersCharacter::AbilityReleased3);
		}
		if (Ability4Action)
		{
			EnhancedInputComponent->BindAction(Ability4Action, ETriggerEvent::Started, this, &ASkylandersCharacter::AbilityPressed4);
			EnhancedInputComponent->BindAction(Ability4Action, ETriggerEvent::Completed, this, &ASkylandersCharacter::AbilityReleased4);
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

void ASkylandersCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();
	PlayAnimOnSlot(JumpAnim, 1.0f);
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
		DmgNum->SetDamageNumber(FinalDamage, FColor::Red, false);
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
		CurrentMana = FMath::Clamp(CurrentMana - ManaCost, 0.0f, MaxMana + ItemBonusStats.MaxMana);
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
	if (Amount > 0) TotalGoldEarned += Amount; // team-economy metric only counts income
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
		LevelUp(); // Updates XPToNextLevel for the new level
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
	BasePower += PowerPerLevel;

	// Heal the amount gained (don't full heal, just add the bonus).
	// Skip while dead so death-state checks (CurrentHealth <= 0) hold until respawn.
	if (CurrentHealth > 0.0f)
	{
		CurrentHealth = FMath::Min(CurrentHealth + HealthPerLevel, MaxHealth + ItemBonusStats.MaxHealth);
		CurrentMana = FMath::Min(CurrentMana + ManaPerLevel, MaxMana + ItemBonusStats.MaxMana);
	}

	// Keep the XP requirement in sync with the new level
	XPToNextLevel = 100.0f + (PlayerLevel - 1) * 50.0f;

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
	USkylandersKillFeedWidget::Post(this, TEXT("You were slain"), FLinearColor(1.0f, 0.25f, 0.25f));

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

	// Close UI that would be stuck open while input is disabled
	CloseShop();
	HideScoreboard();

	// Disable input and movement
	DisableInput(Cast<APlayerController>(GetController()));
	GetCharacterMovement()->DisableMovement();

	// Respawn time scales with level: 5s base + 2s per level
	float RespawnTime = 5.0f + (PlayerLevel * 2.0f);

	// After a short delay, hide the character and spectate the friendly titan
	FTimerHandle HideTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(HideTimerHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
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
	}), 0.75f, false); // Brief delay for death anim

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

	// Release the frozen death pose (code-driven anim instances hold the last frame)
	if (USkylandersSimpleAnimInstance* Simple = Cast<USkylandersSimpleAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		Simple->StopFullBodyAnim();
	}

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
	// Prefer the friendly spawn area (the fountain the map builder places at the
	// base). PlayerStart actors are Static and can't be repositioned at runtime,
	// so after a procedural map rebuild the .umap PlayerStart gets stranded
	// mid-map — use it only as a fallback.
	TArray<AActor*> Areas;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersSpawnArea::StaticClass(), Areas);
	for (AActor* A : Areas)
	{
		ASkylandersSpawnArea* Area = Cast<ASkylandersSpawnArea>(A);
		if (Area && Area->Team == ETowerTeam::Friendly)
		{
			return Area;
		}
	}

	// Fallback: first Player Start in the level
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

	// Cache widget-tree lookups once — UpdateHUD runs every frame during regen
	if (!bHUDRefsCached && MainHUDWidget->WidgetTree)
	{
		UWidgetTree* Tree = MainHUDWidget->WidgetTree;
		CachedHealthText = Cast<URichTextBlock>(Tree->FindWidget(FName("HealthText")));
		CachedHealthBar = Cast<UProgressBar>(Tree->FindWidget(FName("HealthBar")));
		CachedManaText = Cast<URichTextBlock>(Tree->FindWidget(FName("ManaText")));
		CachedManaBar = Cast<UProgressBar>(Tree->FindWidget(FName("ManaBar")));
		CachedLevelText = Tree->FindWidget(FName("LevelText"));
		CachedXPBar = Cast<UProgressBar>(Tree->FindWidget(FName("XPBar")));
		CachedCoinText = Tree->FindWidget(FName("CointCountText"));
		bHUDRefsCached = true;
	}

	// Also directly set the widget components as a fallback
	if (CachedHealthText)
	{
		CachedHealthText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), CurrentHealth, EffectiveMaxHP)));
	}
	if (CachedHealthBar)
	{
		CachedHealthBar->SetPercent(CurrentHealth / EffectiveMaxHP);
	}
	if (CachedManaText)
	{
		CachedManaText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), CurrentMana, EffectiveMaxMP)));
	}
	if (CachedManaBar)
	{
		CachedManaBar->SetPercent(CurrentMana / EffectiveMaxMP);
	}

	// Level (TextBlock or RichTextBlock)
	SetWidgetText(CachedLevelText, FText::FromString(FString::Printf(TEXT("LVL %d"), PlayerLevel)));

	// XP Bar
	if (CachedXPBar)
	{
		float XPPercent = (XPToNextLevel > 0.0f) ? (CurrentXP / XPToNextLevel) : 0.0f;
		CachedXPBar->SetPercent(XPPercent);
	}

	// Gold counter (TextBlock or RichTextBlock)
	SetWidgetText(CachedCoinText, FText::FromString(FString::Printf(TEXT("%d"), Coins)));

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

void ASkylandersCharacter::ApplyCharacterHUDArt()
{
	if (!MainHUDWidget || !MainHUDWidget->WidgetTree) return;

	const FName IconNames[4] = {
		FName("AbilityIcon1"), FName("AbilityIcon2"),
		FName("AbilityIcon3"), FName("AbilityIcon4")
	};
	for (int32 i = 0; i < 4; i++)
	{
		if (!AbilityIconTextures[i]) continue;
		if (UImage* Icon = Cast<UImage>(MainHUDWidget->WidgetTree->FindWidget(IconNames[i])))
		{
			Icon->SetBrushFromTexture(AbilityIconTextures[i], false);
		}
	}

	if (PortraitTexture)
	{
		if (UImage* Portrait = Cast<UImage>(MainHUDWidget->WidgetTree->FindWidget(FName("portrait"))))
		{
			Portrait->SetBrushFromTexture(PortraitTexture, false);
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
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("%s not learned! Press F1 to level it."), *AbilityNames[0]));
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

		// Gold beam visual - stretched cube along the line
		// (cube default is 100x100x100: scale X = range/100, Y and Z thin)
		SpawnColoredMeshVFX(TEXT("/Engine/BasicShapes/Cube"),
			(StartLoc + EndLoc) * 0.5f, AimDir.Rotation(),
			FVector(LineRange / 100.0f, LineWidth / 100.0f, 0.15f),
			FLinearColor(1.0f, 0.85f, 0.0f), 0.6f);

		// Pierce everything along the line
		int32 HitCount = DamageEnemiesInLine(StartLoc, AimDir, LineRange, LineWidth, LineDamage);
		UE_LOG(LogTemp, Log, TEXT("Golden Super Charge pierced %d targets for %.0f damage each"), HitCount, LineDamage);

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
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("%s not learned! Press F2 to level it."), *AbilityNames[1]));
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
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("%s not learned! Press F3 to level it."), *AbilityNames[2]));
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
		PlayAnimOnSlot(MachineGunEndAnim, 1.0f);
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
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("%s not learned! Press F4 to level it (requires level 5)."), *AbilityNames[3]));
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
	if (!AnimInstance) return;

	// Characters without an authored AnimBP (Hex, Tree Rex) run the code-driven
	// instance — montage slots don't exist there, use its full-body override.
	// Death poses freeze on the final frame instead of returning to locomotion.
	if (USkylandersSimpleAnimInstance* Simple = Cast<USkylandersSimpleAnimInstance>(AnimInstance))
	{
		Simple->PlayFullBodyAnim(Anim, PlayRate, Anim == DeathAnim);
		return;
	}

	AnimInstance->PlaySlotAnimationAsDynamicMontage(Anim, SlotName, BlendIn, BlendOut, PlayRate);
}

// ========== PROJECTILE FIRING ==========

void ASkylandersCharacter::FireProjectile()
{
	// Cancel recall if channeling
	if (bIsRecalling) CancelRecall();

	// Can't manually fire while channeling
	if (IsChanneling()) return;

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

	// Play attack animation on the correct arm slot, then alternate guns
	// (use-then-toggle so the first shot really comes from the left gun)
	if (bFireFromLeftGun)
		PlayAnimOnSlot(AttackLeftAnim, 1.5f, 0.05f, 0.1f, FName("LeftArmSlot"));
	else
		PlayAnimOnSlot(AttackRightAnim, 1.5f, 0.05f, 0.1f, FName("RightArmSlot"));
	bFireFromLeftGun = !bFireFromLeftGun;

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
		SkyProj->ProjectileColor = AutoAttackProjectileColor;
		SkyProj->VisualScale = AutoAttackProjectileScale;

		// Cleave cadence: every Nth auto in the chain splashes nearby enemies
		AutoAttackCounter++;
		if (CleaveEveryNthHit > 0 && (AutoAttackCounter % CleaveEveryNthHit) == 0)
		{
			SkyProj->CleaveRadius = CleaveRadius;
			SkyProj->CleaveDamageFraction = CleaveDamageFraction;
		}
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

	// Purchase successful — negative delta via AddCoins keeps the HUD coin
	// accumulator event in sync (direct Coins writes desync the Blueprint side)
	AddCoins(-Item->Cost);
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

	// Sell it (AddCoins keeps the HUD coin accumulator event in sync)
	AddCoins(SellPrice);
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
	return ItemBonusStats.Protections + BuffProtections;
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

	// Show on-screen feedback (AbilityNames member — set per character)
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

	// Ultimate (index 3): rank-ups unlock at levels 5, 9, 13, 17, 20
	// (capped at MaxLevel so rank 5 is actually reachable)
	if (AbilityIndex == 3)
	{
		int32 RequiredLevel = FMath::Min(5 + AbilityLevels[3] * 4, MaxLevel);
		if (PlayerLevel < RequiredLevel) return false;
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
	if (IsChanneling()) return;

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

// ========== PER-CHARACTER SETUP ==========

void ASkylandersCharacter::LoadCharacterVisuals()
{
	// Trigger Happy: dual gun meshes attached to hands
	USkeletalMesh* GunMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/TriggerHappy/Meshes/Gun/TriggerHappyfbxTriggerHappy_Gun"));
	if (GunMesh)
	{
		if (LeftGunMesh) LeftGunMesh->SetSkeletalMesh(GunMesh);
		if (RightGunMesh) RightGunMesh->SetSkeletalMesh(GunMesh);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not load gun mesh!"));
	}
}

bool ASkylandersCharacter::IsChanneling() const
{
	return bMachineGunActive || bChargingYamato;
}

// ========== SHARED KIT HELPERS ==========

// Applies damage to any enemy-side target type
static void ApplyDamageToTarget(AActor* Target, float Damage, AActor* Causer)
{
	if (ASkylandersEnemy* Enemy = Cast<ASkylandersEnemy>(Target))
	{
		Enemy->TakeDamage_Custom(Damage, Causer);
	}
	else if (ASkylandersMinion* Minion = Cast<ASkylandersMinion>(Target))
	{
		Minion->TakeDamage_Custom(Damage, Causer);
	}
	else if (ASkylandersBuffCamp* Camp = Cast<ASkylandersBuffCamp>(Target))
	{
		Camp->TakeDamage_Custom(Damage, Causer);
	}
	else if (ASkylandersEnemyGod* God = Cast<ASkylandersEnemyGod>(Target))
	{
		God->TakeDamage_Custom(Damage, Causer);
	}
}

// Collects every living enemy-side target (jungle enemies, enemy minions, buff camps, enemy gods)
static void CollectEnemyTargets(UWorld* World, TArray<AActor*>& OutTargets)
{
	TArray<AActor*> Found;

	UGameplayStatics::GetAllActorsOfClass(World, ASkylandersEnemy::StaticClass(), Found);
	for (AActor* Actor : Found)
	{
		ASkylandersEnemy* Enemy = Cast<ASkylandersEnemy>(Actor);
		if (Enemy && Enemy->CurrentHealth > 0.0f) OutTargets.Add(Actor);
	}

	Found.Reset();
	UGameplayStatics::GetAllActorsOfClass(World, ASkylandersMinion::StaticClass(), Found);
	for (AActor* Actor : Found)
	{
		ASkylandersMinion* Minion = Cast<ASkylandersMinion>(Actor);
		if (Minion && Minion->Team == ETowerTeam::Enemy && !Minion->bDead) OutTargets.Add(Actor);
	}

	Found.Reset();
	UGameplayStatics::GetAllActorsOfClass(World, ASkylandersBuffCamp::StaticClass(), Found);
	for (AActor* Actor : Found)
	{
		ASkylandersBuffCamp* Camp = Cast<ASkylandersBuffCamp>(Actor);
		if (Camp && !Camp->bDead) OutTargets.Add(Actor);
	}

	Found.Reset();
	UGameplayStatics::GetAllActorsOfClass(World, ASkylandersEnemyGod::StaticClass(), Found);
	for (AActor* Actor : Found)
	{
		ASkylandersEnemyGod* God = Cast<ASkylandersEnemyGod>(Actor);
		if (God && God->CurrentState != EGodAIState::Dead) OutTargets.Add(Actor);
	}
}

int32 ASkylandersCharacter::DamageEnemiesInLine(const FVector& Start, const FVector& Dir, float Range, float Width, float Damage)
{
	TArray<AActor*> Targets;
	CollectEnemyTargets(GetWorld(), Targets);

	int32 HitCount = 0;
	for (AActor* Target : Targets)
	{
		FVector ToTarget = Target->GetActorLocation() - Start;
		float DotAlong = FVector::DotProduct(ToTarget, Dir);
		if (DotAlong <= 0.0f || DotAlong >= Range) continue;

		FVector ClosestPoint = Start + Dir * DotAlong;
		if (FVector::Dist(Target->GetActorLocation(), ClosestPoint) < Width)
		{
			ApplyDamageToTarget(Target, Damage, this);
			HitCount++;
		}
	}
	return HitCount;
}

int32 ASkylandersCharacter::DamageEnemiesInCone(const FVector& Origin, const FVector& Dir, float Range, float MinDot, float Damage, float DistanceFalloff)
{
	TArray<AActor*> Targets;
	CollectEnemyTargets(GetWorld(), Targets);

	int32 HitCount = 0;
	for (AActor* Target : Targets)
	{
		FVector ToTarget = Target->GetActorLocation() - Origin;
		float Distance = ToTarget.Size();
		if (Distance >= Range) continue;

		if (FVector::DotProduct(ToTarget.GetSafeNormal(), Dir) > MinDot)
		{
			float DistFactor = 1.0f - (Distance / Range) * DistanceFalloff;
			ApplyDamageToTarget(Target, Damage * DistFactor, this);
			HitCount++;
		}
	}
	return HitCount;
}

int32 ASkylandersCharacter::DamageEnemiesInSphere(const FVector& Center, float Radius, float Damage)
{
	TArray<AActor*> Targets;
	CollectEnemyTargets(GetWorld(), Targets);

	int32 HitCount = 0;
	for (AActor* Target : Targets)
	{
		if (FVector::Dist(Target->GetActorLocation(), Center) < Radius)
		{
			ApplyDamageToTarget(Target, Damage, this);
			HitCount++;
		}
	}
	return HitCount;
}

AActor* ASkylandersCharacter::SpawnColoredMeshVFX(const TCHAR* MeshPath, const FVector& Location, const FRotator& Rotation, const FVector& Scale, const FLinearColor& Color, float Lifespan)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* VFX = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), Location, Rotation, Params);
	if (!VFX) return nullptr;

	UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(VFX);
	VFX->SetRootComponent(MeshComp);
	MeshComp->RegisterComponent();

	if (UStaticMesh* VFXMesh = LoadObject<UStaticMesh>(nullptr, MeshPath))
	{
		MeshComp->SetStaticMesh(VFXMesh);
	}
	// Assigning a fresh root AFTER SpawnActor discards the spawn transform, so the
	// actor snaps to the origin (this is why Hex's Wall of Bones and other ability
	// VFX appeared at the map center). Re-apply location/rotation explicitly.
	VFX->SetActorLocationAndRotation(Location, Rotation);
	MeshComp->SetWorldScale3D(Scale);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetCastShadow(false);

	if (UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, VFX);
		DynMat->SetVectorParameterValue(FName("Color"), Color);
		MeshComp->SetMaterial(0, DynMat);
	}

	if (Lifespan > 0.0f)
	{
		VFX->SetLifeSpan(Lifespan);
	}
	return VFX;
}

// ========== ABILITY AIMING (SMITE-style hold-to-aim) ==========

void ASkylandersCharacter::CastAbilityByIndex(int32 Index)
{
	switch (Index)
	{
	case 0: UseAbility1(); break;
	case 1: UseAbility2(); break;
	case 2: UseAbility3(); break;
	case 3: UseAbility4(); break;
	default: break;
	}
}

// Remaining cooldown for ability i (0-3)
static float GetAbilityCooldownByIndex(const ASkylandersCharacter* C, int32 i)
{
	switch (i)
	{
	case 0: return C->Ability1_RemainingCooldown;
	case 1: return C->Ability2_RemainingCooldown;
	case 2: return C->Ability3_RemainingCooldown;
	case 3: return C->Ability4_RemainingCooldown;
	default: return 0.0f;
	}
}

void ASkylandersCharacter::OnAbilityPressed(int32 Index)
{
	if (Index < 0 || Index > 3) return;

	// Non-ground abilities cast instantly on press (unchanged behavior)
	if (!bAbilityUsesGroundAim[Index])
	{
		CastAbilityByIndex(Index);
		return;
	}

	// Ground ability: only enter aim mode if it's actually castable, otherwise
	// fire the cast so the "not learned / on cooldown" feedback still shows.
	const bool bLearned = AbilityLevels[Index] > 0;
	const bool bReady = GetAbilityCooldownByIndex(this, Index) <= 0.0f;
	if (bLearned && bReady && !IsChanneling())
	{
		AimingAbilityIndex = Index;
	}
	else
	{
		CastAbilityByIndex(Index);
	}
}

void ASkylandersCharacter::OnAbilityReleased(int32 Index)
{
	if (Index < 0 || Index > 3) return;

	// Fire the aimed ability at the current targeter position on release
	if (AimingAbilityIndex == Index)
	{
		AimingAbilityIndex = -1;
		CastAbilityByIndex(Index);
	}
}

void ASkylandersCharacter::AbilityPressed1() { OnAbilityPressed(0); }
void ASkylandersCharacter::AbilityReleased1() { OnAbilityReleased(0); }
void ASkylandersCharacter::AbilityPressed2() { OnAbilityPressed(1); }
void ASkylandersCharacter::AbilityReleased2() { OnAbilityReleased(1); }
void ASkylandersCharacter::AbilityPressed3() { OnAbilityPressed(2); }
void ASkylandersCharacter::AbilityReleased3() { OnAbilityReleased(2); }
void ASkylandersCharacter::AbilityPressed4() { OnAbilityPressed(3); }
void ASkylandersCharacter::AbilityReleased4() { OnAbilityReleased(3); }

void ASkylandersCharacter::UpdateAimTargeter()
{
	// Not aiming — keep the slider hidden
	if (AimingAbilityIndex < 0)
	{
		if (GroundAimIndicator && GroundAimIndicator->IsVisible())
			GroundAimIndicator->SetVisibility(false);
		if (GroundAimLine && GroundAimLine->IsVisible())
			GroundAimLine->SetVisibility(false);
		return;
	}

	const int32 i = AimingAbilityIndex;
	const float Radius = AbilityAimRadius[i];
	const float Range = AbilityAimRange[i];

	// Aim point on the ground (same trace the ability uses at cast time)
	FVector AimPoint = GetGroundAimPoint(FMath::Max(Range - Radius, 100.0f));
	const float GroundZ = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 3.0f;
	AimPoint.Z = GroundZ;

	// Circle at the aim point (cylinder default radius 50 -> scale = R/50)
	if (GroundAimIndicator)
	{
		GroundAimIndicator->SetWorldLocation(AimPoint);
		GroundAimIndicator->SetWorldScale3D(FVector(Radius / 50.0f, Radius / 50.0f, 0.02f));
		GroundAimIndicator->SetVisibility(true);
	}

	// Line from the character to the circle (cube default 100 -> scale X = len/100)
	if (GroundAimLine)
	{
		FVector Start = GetActorLocation();
		Start.Z = GroundZ;
		FVector ToAim = AimPoint - Start;
		ToAim.Z = 0.0f;
		const float Length = ToAim.Size();
		if (Length > 10.0f)
		{
			FVector Mid = (Start + AimPoint) * 0.5f;
			FRotator LineRot = ToAim.Rotation(); // yaw toward the aim point
			GroundAimLine->SetWorldLocation(Mid);
			GroundAimLine->SetWorldRotation(LineRot);
			GroundAimLine->SetWorldScale3D(FVector(Length / 100.0f, 0.12f, 0.02f));
			GroundAimLine->SetVisibility(true);
		}
		else
		{
			GroundAimLine->SetVisibility(false);
		}
	}
}

// ========== AIM HIGHLIGHT (SMITE-style target outline) ==========

AActor* ASkylandersCharacter::FindAutoAttackTarget() const
{
	TArray<AActor*> Targets;
	CollectEnemyTargets(GetWorld(), Targets);
	if (Targets.Num() == 0) return nullptr;

	const FVector Origin = GetActorLocation();
	const FVector AimDir = FRotator(0.0f, GetControlRotation().Yaw, 0.0f).Vector();
	const float Reach = FMath::Max(AutoAttackRange, 350.0f);

	AActor* Best = nullptr;
	float BestDist = Reach;
	for (AActor* T : Targets)
	{
		FVector ToT = T->GetActorLocation() - Origin;
		ToT.Z = 0.0f;
		const float Dist = ToT.Size();
		if (Dist > Reach || Dist < 1.0f) continue;

		// Must be roughly in front of the aim (within ~30 degrees)
		if (FVector::DotProduct(ToT.GetSafeNormal(), AimDir) < 0.86f) continue;

		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = T;
		}
	}
	return Best;
}

// Sets or clears the highlight overlay on a target's mesh
static void SetTargetHighlight(AActor* Target, UMaterialInterface* Mat)
{
	if (!Target) return;
	if (UMeshComponent* Mesh = Target->FindComponentByClass<UMeshComponent>())
	{
		Mesh->SetOverlayMaterial(Mat);
	}
}

void ASkylandersCharacter::UpdateAimHighlight()
{
	AActor* NewTarget = FindAutoAttackTarget();
	if (NewTarget == CurrentAimTarget) return;

	// Clear the previous highlight, apply the new one
	SetTargetHighlight(CurrentAimTarget, nullptr);
	if (HighlightMaterial)
	{
		SetTargetHighlight(NewTarget, HighlightMaterial);
	}
	CurrentAimTarget = NewTarget;
}

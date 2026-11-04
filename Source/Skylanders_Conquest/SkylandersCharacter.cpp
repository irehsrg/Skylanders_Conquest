// Skylanders Conquest - Player Character Implementation

//
// ASkylandersCharacter is split across translation units by concern:
//   SkylandersCharacter.cpp            constructor, BeginPlay, Tick, input
//   SkylandersCharacter_Stats.cpp      health/mana/gold/XP, death & respawn
//   SkylandersCharacter_HUD.cpp        HUD, cooldown UI, scoreboard, pause menu
//   SkylandersCharacter_Shop.cpp       shop, items, ability leveling
//   SkylandersCharacter_Abilities.cpp  targeting, firing, recall, kit helpers, aiming
#include "SkylandersCharacter.h"
#include "SkylandersCameraShake.h"
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
#include "SkylandersActorRegistry.h"
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
#include "SkylandersPauseMenuWidget.h"
#include "SkylandersGameSettings.h"
#include "SkylandersKillFeedWidget.h"
#include "SkylandersAnnouncerWidget.h"
#include "SkylandersSoundLibrary.h"
#include "SkylandersMatchStatusWidget.h"
#include "SkylandersTelemetry.h"
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
	BaseProtections = 10.0f;
	ProtectionsPerLevel = 1.0f;

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
	PauseMenuWidget = nullptr;

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
	Assists = 0;
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

	// Default SFX for any sound slot the Blueprint left empty (assigned sounds win)
	if (!AttackSound) AttackSound = SkylandersSound::Get(TEXT("hit_light"));
	if (!AbilitySound) AbilitySound = SkylandersSound::Get(TEXT("ability_cast"));
	if (!DeathSound) DeathSound = SkylandersSound::Get(TEXT("death"));
	if (!LevelUpSound) LevelUpSound = SkylandersSound::Get(TEXT("levelup"));
	if (!ItemBuySound) ItemBuySound = SkylandersSound::Get(TEXT("buy_item"));

	// Push saved volume/sensitivity into the engine for this session.
	if (USkylandersGameSettings* Prefs = USkylandersGameSettings::Get())
	{
		Prefs->ApplyAll();
	}

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

				// Announcer banner (FIRST BLOOD, kills, structures, countdown)
				if (USkylandersAnnouncerWidget* Announcer = CreateWidget<USkylandersAnnouncerWidget>(PlayerController, USkylandersAnnouncerWidget::StaticClass()))
				{
					Announcer->AddToViewport(7);
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

	// 3-2-1-FIGHT countdown. Presentation only: gameplay is already live, the
	// banner just marks the start of the match.
	{
		static const TCHAR* const Steps[] = { TEXT("3"), TEXT("2"), TEXT("1"), TEXT("FIGHT!") };
		for (int32 i = 0; i < UE_ARRAY_COUNT(Steps); i++)
		{
			const bool bFight = (i == UE_ARRAY_COUNT(Steps) - 1);
			const FString Text = Steps[i];
			FTimerHandle Handle;
			GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Text, bFight]()
			{
				// Digits hold under a second so each one clears before the next
				USkylandersAnnouncerWidget::Announce(this, Text,
					bFight ? FLinearColor(1.0f, 0.85f, 0.2f) : FLinearColor::White, bFight ? 1.5f : 0.9f);
			}), 1.0f + i, false);
		}
	}

	// Show skill point notification (player starts at level 3 with 3 points)
	if (AbilityPoints > 0)
	{
		USkylandersKillFeedWidget::Post(this,
			FString::Printf(TEXT("%d skill points: F1-F4 to level abilities"), AbilityPoints), FLinearColor(1.0f, 0.85f, 0.2f));
	}

	// Widgets exist now; show the starting stats without waiting for the first Tick
	FlushHUD();
}

void ASkylandersCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Each phase owns one concern; see TICK PHASES below. Order matters:
	// regen before the HUD reads stats, movement speed after channel/ability
	// state has been updated for this frame.
	TickRegenAndIncome(DeltaTime);

	UpdateCooldowns(DeltaTime);
	UpdateCooldownUI();

	// SMITE-style aiming: the line+circle targeter only appears while a ground
	// ability is being aimed (press-and-hold); it's hidden the rest of the time.
	UpdateAimTargeter();

	// Highlight the enemy the auto attack would currently hit
	UpdateAimHighlight();

	TickRecall(DeltaTime);
	TickRespawnCountdown(DeltaTime);

	if (AttackSlowRemainingTime > 0.0f)
	{
		AttackSlowRemainingTime -= DeltaTime;
	}

	TickMachineGun(DeltaTime);
	TickYamatoCharge(DeltaTime);
	TickMovementSpeed();
	TickFountainHealing(DeltaTime);

	// Last: every phase above only marks the HUD dirty; write it once per frame
	FlushHUD();
}

// ========== TICK PHASES ==========
// Tick() is an ordered list of these; each owns one concern.

void ASkylandersCharacter::TickRegenAndIncome(float DeltaTime)
{
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
}

void ASkylandersCharacter::TickRecall(float DeltaTime)
{
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

			// Countdown under the announcer banner (SetStatus skips unchanged text)
			{
				int32 SecondsLeft = FMath::Max(FMath::CeilToInt(RecallRemainingTime), 0);
				USkylandersAnnouncerWidget::SetStatus(this,
					FString::Printf(TEXT("Recalling... %ds"), SecondsLeft), FLinearColor(0.3f, 0.85f, 1.0f));
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
}

void ASkylandersCharacter::TickRespawnCountdown(float DeltaTime)
{
	// Death respawn countdown display
	if (CurrentHealth <= 0.0f && DeathRespawnTimeRemaining > 0.0f)
	{
		DeathRespawnTimeRemaining -= DeltaTime;
		int32 SecondsLeft = FMath::CeilToInt(FMath::Max(DeathRespawnTimeRemaining, 0.0f));
		// Persistent line under the announcer banner; Respawn() clears it
		USkylandersAnnouncerWidget::SetStatus(this,
			FString::Printf(TEXT("Respawning in %d..."), SecondsLeft), FLinearColor(1.0f, 0.3f, 0.3f));
	}
}

void ASkylandersCharacter::TickMachineGun(float DeltaTime)
{
	// Golden Machine Gun timer - stationary rapid fire
	if (bMachineGunActive)
	{
		MachineGunRemainingTime -= DeltaTime;

		// Force stationary
		GetCharacterMovement()->Velocity = FVector::ZeroVector;

		// Keep the firing loop animation running once the start anim finishes.
		// Characters on the code-driven anim instance (Hex, Tree Rex) never create
		// montages, so Montage_IsPlaying is always false there — asking it alone
		// restarted the loop every tick and froze the anim on frame 0.
		if (MachineGunLoopAnim)
		{
			if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
			{
				bool bAnimBusy;
				if (USkylandersSimpleAnimInstance* Simple = Cast<USkylandersSimpleAnimInstance>(AnimInst))
				{
					bAnimBusy = Simple->IsFullBodyAnimPlaying();
				}
				else
				{
					bAnimBusy = AnimInst->Montage_IsPlaying(nullptr);
				}

				if (!bAnimBusy)
				{
					PlayAnimOnSlot(MachineGunLoopAnim, 1.0f);
				}
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
}

void ASkylandersCharacter::TickYamatoCharge(float DeltaTime)
{
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
			USkylandersCameraShakeLibrary::Shake(this, true, 1.0f);
			SkylandersSound::PlayAt(this, TEXT("ability_ult"), GetActorLocation());

			if (HitCount > 0)
			{
				AddCoins(HitCount * 5);
			}
		}
	}
}

void ASkylandersCharacter::TickMovementSpeed()
{
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
}

void ASkylandersCharacter::TickFountainHealing(float DeltaTime)
{
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

	// Pause menu. P is the PIE-friendly alternative, since the editor swallows
	// Escape to stop the session before the game ever sees it.
	PlayerInputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ASkylandersCharacter::TogglePauseMenu);
	PlayerInputComponent->BindKey(EKeys::P, IE_Pressed, this, &ASkylandersCharacter::TogglePauseMenu);

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
		const USkylandersGameSettings* Prefs = USkylandersGameSettings::Get();
		const float Sensitivity = Prefs ? Prefs->MouseSensitivity : 1.0f;

		AddControllerYawInput(LookAxisVector.X * Sensitivity);
		AddControllerPitchInput(LookAxisVector.Y * Sensitivity);

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


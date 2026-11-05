// Skylanders Conquest - Spyro Implementation

#include "SkylandersSpyroCharacter.h"
#include "SkylandersKillFeedWidget.h"
#include "SkylandersSimpleAnimInstance.h"
#include "SkylandersCameraShake.h"
#include "SkylandersSoundLibrary.h"
#include "SkylandersSmokeTest.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequenceBase.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

// Spyro's fire, and a hotter core for the big blasts
static const FLinearColor SpyroFire(1.0f, 0.45f, 0.1f, 1.0f);
static const FLinearColor SpyroFireCore(1.0f, 0.80f, 0.25f, 1.0f);
static const FLinearColor SpyroPurple(0.55f, 0.30f, 0.95f, 1.0f);

ASkylandersSpyroCharacter::ASkylandersSpyroCharacter()
{
	CharacterName = TEXT("Spyro");
	CharacterRole = TEXT("Purple Dragon");
	AbilityNames[0] = TEXT("Flame Breath");
	AbilityNames[1] = TEXT("Charge");
	AbilityNames[2] = TEXT("Comet Dash");
	AbilityNames[3] = TEXT("Daybringer Flame");

	// Bruiser statline: sturdier than the mage, less mana, solid scaling
	StartingMaxHealth = 130.0f;
	HealthPerLevel = 21.0f;
	StartingMaxMana = 100.0f;
	ManaPerLevel = 15.0f;
	StartingManaRegen = 5.0f;
	StartingBasePower = 20.0f;
	PowerPerLevel = 2.4f;

	// Flameball auto: a steady orange fire shot. Spawned at mouth height
	// (~30 above the ground for a small quadruped) so the flat shot still
	// connects with short minions.
	FireRate = 1.4f;
	AutoAttackProjectileColor = SpyroFire;
	AutoAttackProjectileScale = 0.5f;
	ProjectileSpawnOffset = FVector(70.0f, 0.0f, 0.0f);

	// Only Comet Dash (index 2) is ground-placed. Flame Breath, Charge and
	// Daybringer fire along the facing, so undo the base kit's ground-aimed
	// Pot o' Gold slot (index 1).
	bAbilityUsesGroundAim[1] = false;
	bAbilityUsesGroundAim[2] = true;
	AbilityAimRadius[2] = 260.0f;
	AbilityAimRange[2] = 900.0f;

	// Ability tuning
	Ability1_Cooldown = 7.0f;
	Ability1_ManaCost = 20.0f;
	Ability2_Cooldown = 11.0f;
	Ability2_ManaCost = 30.0f;
	Ability3_Cooldown = 13.0f;
	Ability3_ManaCost = 40.0f;
	Ability4_Cooldown = 80.0f;
	Ability4_ManaCost = 100.0f;

	// Spyro's body at the AI table's 0.52 scale: a small quadruped ~45 units
	// tall and longer than he is tall. Capsule: a touch wider than Trigger
	// Happy's (the snout and tail stick out), half-height just over his
	// standing height so the hit band still covers the horns. Like Hex and
	// Tree Rex, the ripped animations put the ground at the skeleton origin,
	// so the mesh sits capsule-half-height down. (The AI table's +5.2 MeshZ
	// comes from the bind-pose bounds of a center-pivoted model, which the
	// first-frame-pinned animated poses don't use.)
	GetCapsuleComponent()->SetCapsuleSize(30.0f, 32.0f);
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -32.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f)); // Interchange meshes face +X already
	GetMesh()->SetRelativeScale3D(FVector(0.52f));

	// Small character: Trigger Happy's framing, look point dropped a little
	// since Spyro stands lower to the ground.
	if (CameraBoom)
	{
		CameraBoom->TargetArmLength = 450.0f;
		CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 90.0f);
	}

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BodyMesh(TEXT("/Game/Characters/Spyro/Models/Spyro"));
	if (BodyMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(BodyMesh.Object);
	}
	// Code-driven anim instance (no AnimBP authored for Spyro)
	GetMesh()->SetAnimInstanceClass(USkylandersSimpleAnimInstance::StaticClass());

	// Native Spyro animations from the ripped set. There is no drive_idle in
	// the rip, so the kart mod-swap pose stands in as idle (same as the AI god).
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> IdleSeq(TEXT("/Game/Characters/Spyro/Animations/drive_modswap_pose01"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> RunSeq(TEXT("/Game/Characters/Spyro/Animations/drive_run"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> JumpSeq(TEXT("/Game/Characters/Spyro/Animations/jumppad_start"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Flameball(TEXT("/Game/Characters/Spyro/Animations/flameball_shoot_partial"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Breath(TEXT("/Game/Characters/Spyro/Animations/emotion_traversal_partial"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> ChargeIn(TEXT("/Game/Characters/Spyro/Animations/charge_in"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> ChargeFast(TEXT("/Game/Characters/Spyro/Animations/charge_fast"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> CometLand(TEXT("/Game/Characters/Spyro/Animations/railslide_land"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Daybringer(TEXT("/Game/Characters/Spyro/Animations/flight_out"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> HitReact(TEXT("/Game/Characters/Spyro/Animations/takehit_groundfront"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> DeathSeq(TEXT("/Game/Characters/Spyro/Animations/knockaway_back"));
	if (IdleSeq.Succeeded()) IdleLocomotionAnim = IdleSeq.Object;
	if (RunSeq.Succeeded()) RunLocomotionAnim = RunSeq.Object;
	if (JumpSeq.Succeeded()) JumpAnim = JumpSeq.Object;
	if (JumpSeq.Succeeded()) CometLaunchAnim = JumpSeq.Object;   // Comet Dash take-off
	if (Flameball.Succeeded()) AttackLeftAnim = Flameball.Object;
	if (Flameball.Succeeded()) AttackRightAnim = Flameball.Object;
	if (Breath.Succeeded()) Ability1Anim = Breath.Object;         // Flame Breath
	if (ChargeIn.Succeeded()) ChargeInAnim = ChargeIn.Object;     // Charge wind-up
	if (ChargeFast.Succeeded()) ChargeFastAnim = ChargeFast.Object;
	if (ChargeFast.Succeeded()) Ability2Anim = ChargeFast.Object;
	if (CometLand.Succeeded()) CometLandAnim = CometLand.Object;  // Comet Dash touchdown
	if (Daybringer.Succeeded()) YamatoAnim = Daybringer.Object;   // Daybringer Flame (ultimate)
	if (HitReact.Succeeded()) HitReactAnim = HitReact.Object;
	if (DeathSeq.Succeeded()) DeathAnim = DeathSeq.Object;

	// HUD art: none exists for Spyro yet. PortraitTexture / AbilityIconTextures
	// are left null on purpose (the C++ HUD renders an empty dark slot).
}

void ASkylandersSpyroCharacter::LoadCharacterVisuals()
{
	// Real mesh + materials come from the imported assets; no gun meshes
	// (Spyro breathes his attacks).
}

// Dashes, the leap and the ult wind-up all count as channeling: the base class
// then zeroes walk speed (input can't fight the scripted motion) and blocks
// autos and other casts until they finish.
bool ASkylandersSpyroCharacter::IsChanneling() const
{
	return Super::IsChanneling() || bCharging || bCometLeaping || bDaybringerChanneling;
}

// Flattened cone of fire from Origin along Dir: the engine cone's tip points
// +Z and its pivot is the shape's center, so aim -Z down the breath (tip at
// the mouth, base out at range) and squash the vertical axis.
AActor* ASkylandersSpyroCharacter::SpawnFireCone(const FVector& Origin, const FVector& Dir, float Length, float HalfWidth,
	float Thickness, const FLinearColor& Color, float Lifespan)
{
	const FRotator Rot = FRotationMatrix::MakeFromZX(-Dir, FVector::UpVector).Rotator();
	return SpawnColoredMeshVFX(TEXT("/Engine/BasicShapes/Cone"),
		Origin + Dir * (Length * 0.5f), Rot,
		FVector(Thickness / 100.0f, (HalfWidth * 2.0f) / 100.0f, Length / 100.0f),
		Color, Lifespan);
}

void ASkylandersSpyroCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Dying mid-dash must not keep sliding the corpse (or leave him flying)
	if (CurrentHealth <= 0.0f)
	{
		CancelSpyroStates();
		SpeedBoostRemaining = 0.0f;
		return;
	}

	TickCharge(DeltaTime);
	TickComet(DeltaTime);
	TickDaybringer(DeltaTime);

	// Post-charge speed burst. The base TickMovementSpeed rebuilds MaxWalkSpeed
	// from BaseMaxWalkSpeed every frame, so scaling the result here never stacks
	// and never has to be undone (unlike editing BaseMaxWalkSpeed, which item
	// recalculation and the speed camp also write).
	if (SpeedBoostRemaining > 0.0f)
	{
		SpeedBoostRemaining -= DeltaTime;
		if (!IsChanneling())
		{
			GetCharacterMovement()->MaxWalkSpeed *= SpeedBoostMultiplier;
		}
	}
}

void ASkylandersSpyroCharacter::CancelSpyroStates()
{
	if (bCometLeaping)
	{
		// Hand him back to gravity so he lands wherever the leap stopped
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	}
	bCharging = false;
	bCometLeaping = false;
	bDaybringerChanneling = false;
}

// Ability 1 - Flame Breath: frontal fire cone
void ASkylandersSpyroCharacter::UseAbility1()
{
	if (AbilityLevels[0] == 0)
	{
		USkylandersKillFeedWidget::Post(this, TEXT("Flame Breath not learned! Press F1 to level it."), FLinearColor(1.0f, 0.4f, 0.35f));
		return;
	}
	if (bIsRecalling) CancelRecall();
	if (IsChanneling()) return;
	if (Ability1_RemainingCooldown > 0.0f) return;

	if (UseMana(Ability1_ManaCost))
	{
		float CooldownScale = 1.2f - 0.04f * AbilityLevels[0];
		Ability1_RemainingCooldown = Ability1_Cooldown * CooldownScale * GetCooldownMultiplier();

		if (AbilitySound) UGameplayStatics::PlaySoundAtLocation(this, AbilitySound, GetActorLocation());
		PlayAnimOnSlot(Ability1Anim, 1.0f);

		const float BreathRange = 650.0f;
		const FVector Dir = FRotator(0.0f, GetControlRotation().Yaw, 0.0f).Vector();
		const FVector Mouth = GetActorLocation() + Dir * 30.0f;
		float RankScale = 0.6f + 0.08f * AbilityLevels[0];
		float BreathDamage = GetEffectivePower() * 2.2f * RankScale;

		// Visual narrower than the true hit arc (MinDot 0.55 is ~57 degrees each
		// side): a full-width wedge would blanket the screen
		SpawnFireCone(Mouth, Dir, BreathRange, BreathRange * 0.6f, 25.0f, SpyroFire, 0.5f);

		int32 HitCount = DamageEnemiesInCone(Mouth, Dir, BreathRange, 0.55f, BreathDamage, 0.3f);
		UE_LOG(LogTemp, Log, TEXT("Flame Breath hit %d targets for up to %.0f damage"), HitCount, BreathDamage);
		if (HitCount > 0) AddCoins(HitCount);
	}
}

// Ability 2 - Charge: dash along the facing, ram everything on the path
void ASkylandersSpyroCharacter::UseAbility2()
{
	if (AbilityLevels[1] == 0)
	{
		USkylandersKillFeedWidget::Post(this, TEXT("Charge not learned! Press F2 to level it."), FLinearColor(1.0f, 0.4f, 0.35f));
		return;
	}
	if (bIsRecalling) CancelRecall();
	if (IsChanneling()) return;
	if (Ability2_RemainingCooldown > 0.0f) return;

	if (UseMana(Ability2_ManaCost))
	{
		float CooldownScale = 1.2f - 0.04f * AbilityLevels[1];
		Ability2_RemainingCooldown = Ability2_Cooldown * CooldownScale * GetCooldownMultiplier();

		if (AbilitySound) UGameplayStatics::PlaySoundAtLocation(this, AbilitySound, GetActorLocation());
		PlayAnimOnSlot(ChargeInAnim, 1.5f);

		// Lock the direction at cast; Tick() drives the travel and
		// FinishCharge() deals the ram damage along the path actually covered
		ChargeDir = FRotator(0.0f, GetControlRotation().Yaw, 0.0f).Vector();
		ChargeStart = GetActorLocation();
		ChargeRank = AbilityLevels[1];
		ChargeElapsed = 0.0f;
		bChargeFastPlaying = false;
		bCharging = true;
	}
}

void ASkylandersSpyroCharacter::TickCharge(float DeltaTime)
{
	if (!bCharging) return;

	// The dash fully owns movement while it runs
	GetCharacterMovement()->Velocity = FVector::ZeroVector;

	const float Step = FMath::Min(DeltaTime, ChargeDuration - ChargeElapsed);
	FHitResult SweepHit;
	AddActorWorldOffset(ChargeDir * (ChargeDistance / ChargeDuration) * Step, true, &SweepHit);
	ChargeElapsed += Step;

	// Wind-up reads for the first moment, then the horns-down sprint
	if (!bChargeFastPlaying && ChargeElapsed >= ChargeDuration * 0.3f)
	{
		bChargeFastPlaying = true;
		PlayAnimOnSlot(ChargeFastAnim, 1.5f);
	}

	if (ChargeElapsed >= ChargeDuration || SweepHit.bBlockingHit)
	{
		FinishCharge();
	}
}

void ASkylandersSpyroCharacter::FinishCharge()
{
	bCharging = false;

	// Everything within 120 of the path he actually covered (a wall can cut it short)
	const float Traveled = FVector::Dist2D(ChargeStart, GetActorLocation());
	const float RamWidth = 120.0f;
	const float PathLength = Traveled + 60.0f; // catch whoever he stopped against
	float RankScale = 0.6f + 0.08f * FMath::Max(ChargeRank, 1);
	float RamDamage = GetEffectivePower() * 2.0f * RankScale;

	SpawnColoredMeshVFX(TEXT("/Engine/BasicShapes/Cube"),
		ChargeStart + ChargeDir * (PathLength * 0.5f), ChargeDir.Rotation(),
		FVector(PathLength / 100.0f, (RamWidth * 2.0f) / 100.0f, 0.1f),
		SpyroPurple, 0.4f);

	int32 HitCount = DamageEnemiesInLine(ChargeStart, ChargeDir, PathLength, RamWidth, RamDamage);
	UE_LOG(LogTemp, Log, TEXT("Charge rammed %d targets for %.0f damage"), HitCount, RamDamage);
	if (HitCount > 0) AddCoins(HitCount);

	SpeedBoostRemaining = SpeedBoostDuration;
}

// Ability 3 - Comet Dash: leap to the aim point, crash down on it
void ASkylandersSpyroCharacter::UseAbility3()
{
	if (AbilityLevels[2] == 0)
	{
		USkylandersKillFeedWidget::Post(this, TEXT("Comet Dash not learned! Press F3 to level it."), FLinearColor(1.0f, 0.4f, 0.35f));
		return;
	}
	if (bIsRecalling) CancelRecall();
	if (IsChanneling()) return;
	if (Ability3_RemainingCooldown > 0.0f) return;

	if (UseMana(Ability3_ManaCost))
	{
		float CooldownScale = 1.2f - 0.04f * AbilityLevels[2];
		Ability3_RemainingCooldown = Ability3_Cooldown * CooldownScale * GetCooldownMultiplier();

		if (AbilitySound) UGameplayStatics::PlaySoundAtLocation(this, AbilitySound, GetActorLocation());
		PlayAnimOnSlot(CometLaunchAnim, 1.2f);

		// Land where the targeter circle sits (same range-minus-radius cap the
		// circle uses), at his current height: the lane is flat and the trace
		// hit point is the ground surface, not capsule-center height
		CometStart = GetActorLocation();
		CometEnd = GetGroundAimPoint(AbilityAimRange[2] - AbilityAimRadius[2]);
		CometEnd.Z = CometStart.Z;
		CometRank = AbilityLevels[2];
		CometElapsed = 0.0f;
		bCometLeaping = true;

		// Flying mode so gravity and floor checks don't fight the scripted arc
		GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		GetCharacterMovement()->Velocity = FVector::ZeroVector;
	}
}

void ASkylandersSpyroCharacter::TickComet(float DeltaTime)
{
	if (!bCometLeaping) return;

	GetCharacterMovement()->Velocity = FVector::ZeroVector;

	CometElapsed = FMath::Min(CometElapsed + DeltaTime, CometDuration);
	const float Alpha = CometElapsed / CometDuration;

	// Parabola: straight line between the endpoints plus a 4h*a*(1-a) hop
	FVector Pos = FMath::Lerp(CometStart, CometEnd, Alpha);
	Pos.Z += 4.0f * CometArcHeight * Alpha * (1.0f - Alpha);

	FHitResult SweepHit;
	SetActorLocation(Pos, true, &SweepHit);

	// A wall or structure in the way ends the leap early; he crashes down there
	if (Alpha >= 1.0f || SweepHit.bBlockingHit)
	{
		FinishComet();
	}
}

void ASkylandersSpyroCharacter::FinishComet()
{
	bCometLeaping = false;
	GetCharacterMovement()->SetMovementMode(MOVE_Falling); // settles onto the floor next move tick
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	PlayAnimOnSlot(CometLandAnim, 1.2f);

	const float ImpactRadius = AbilityAimRadius[2];
	float RankScale = 0.6f + 0.08f * FMath::Max(CometRank, 1);
	float ImpactDamage = GetEffectivePower() * 2.8f * RankScale;
	const FVector Impact = GetActorLocation();

	SpawnColoredMeshVFX(TEXT("/Engine/BasicShapes/Sphere"),
		Impact, FRotator::ZeroRotator, FVector(ImpactRadius / 50.0f), SpyroFire, 0.35f);
	SpawnColoredMeshVFX(TEXT("/Engine/BasicShapes/Cylinder"),
		Impact - FVector(0, 0, 30.0f), FRotator::ZeroRotator,
		FVector(ImpactRadius / 50.0f, ImpactRadius / 50.0f, 0.04f), SpyroFireCore, 0.5f);

	int32 HitCount = DamageEnemiesInSphere(Impact, ImpactRadius, ImpactDamage);
	UE_LOG(LogTemp, Log, TEXT("Comet Dash hit %d targets for %.0f damage"), HitCount, ImpactDamage);
	if (HitCount > 0) AddCoins(HitCount);
}

// Ability 4 - Daybringer Flame: 1 s wind-up, then a huge fire cone (ultimate)
void ASkylandersSpyroCharacter::UseAbility4()
{
	if (AbilityLevels[3] == 0)
	{
		USkylandersKillFeedWidget::Post(this, TEXT("Daybringer Flame not learned! Press F4 to level it (requires level 5)."), FLinearColor(1.0f, 0.4f, 0.35f));
		return;
	}
	if (bIsRecalling) CancelRecall();
	if (IsChanneling()) return;
	if (Ability4_RemainingCooldown > 0.0f) return;

	if (UseMana(Ability4_ManaCost))
	{
		float CooldownScale = 1.2f - 0.04f * AbilityLevels[3];
		Ability4_RemainingCooldown = Ability4_Cooldown * CooldownScale * GetCooldownMultiplier();

		if (AbilitySound) UGameplayStatics::PlaySoundAtLocation(this, AbilitySound, GetActorLocation());
		PlayAnimOnSlot(YamatoAnim, 1.0f);

		// Rooted wind-up like Yamato: IsChanneling() zeroes walk speed and
		// TickDaybringer() releases the blast when the channel completes
		DaybringerRank = AbilityLevels[3];
		DaybringerRemaining = DaybringerChannelTime;
		bDaybringerChanneling = true;
		GetCharacterMovement()->MaxWalkSpeed = 0.0f;
		GetCharacterMovement()->Velocity = FVector::ZeroVector;

		// Gathering-flame glow at the mouth for the wind-up
		SpawnColoredMeshVFX(TEXT("/Engine/BasicShapes/Sphere"),
			GetActorLocation() + GetActorForwardVector() * 40.0f, FRotator::ZeroRotator,
			FVector(0.8f), SpyroFireCore, DaybringerChannelTime);

		UE_LOG(LogTemp, Warning, TEXT("DAYBRINGER FLAME winding up..."));
	}
}

void ASkylandersSpyroCharacter::TickDaybringer(float DeltaTime)
{
	if (!bDaybringerChanneling) return;

	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	DaybringerRemaining -= DeltaTime;
	if (DaybringerRemaining <= 0.0f)
	{
		ReleaseDaybringer();
	}
}

void ASkylandersSpyroCharacter::ReleaseDaybringer()
{
	bDaybringerChanneling = false;
	DaybringerRemaining = 0.0f;

	const float FlameRange = 1000.0f;
	// Fire along the current aim: the yaw still follows the camera while rooted
	const FVector Dir = FRotator(0.0f, GetControlRotation().Yaw, 0.0f).Vector();
	const FVector Mouth = GetActorLocation() + Dir * 30.0f;
	float UltRankScale = 0.5f + 0.10f * FMath::Max(DaybringerRank, 1);
	float FlameDamage = GetEffectivePower() * 9.0f * UltRankScale;

	// Two stacked wedges (wide orange sheet, hot narrow core) plus a flash
	SpawnFireCone(Mouth, Dir, FlameRange, FlameRange * 0.9f, 60.0f, SpyroFire, 0.9f);
	SpawnFireCone(Mouth + FVector(0, 0, 10.0f), Dir, FlameRange * 0.8f, FlameRange * 0.35f, 40.0f, SpyroFireCore, 0.9f);
	SpawnColoredMeshVFX(TEXT("/Engine/BasicShapes/Sphere"),
		Mouth, FRotator::ZeroRotator, FVector(3.0f), SpyroFireCore, 0.4f);

	int32 HitCount = DamageEnemiesInCone(Mouth, Dir, FlameRange, 0.35f, FlameDamage, 0.0f);
	UE_LOG(LogTemp, Warning, TEXT("DAYBRINGER FLAME hit %d targets for %.0f damage!"), HitCount, FlameDamage);
	SKY_COUNT("player_ult");
	USkylandersCameraShakeLibrary::Shake(this, true, 1.0f);
	SkylandersSound::PlayAt(this, TEXT("ability_ult"), GetActorLocation());

	if (HitCount > 0) AddCoins(HitCount * 5);
}

// Skylanders Conquest - Tree Rex Implementation

#include "SkylandersTreeRexCharacter.h"
#include "SkylandersTower.h"
#include "SkylandersTitan.h"
#include "SkylandersSimpleAnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

// Tree Rex's signature bark green
static const FLinearColor BarkGreen(0.18f, 0.55f, 0.15f, 1.0f);
static const FLinearColor GroveGreen(0.35f, 0.90f, 0.30f, 1.0f);

ASkylandersTreeRexCharacter::ASkylandersTreeRexCharacter()
{
	CharacterName = TEXT("Tree Rex");
	CharacterRole = TEXT("Giant Guardian");
	AbilityNames[0] = TEXT("Shockwave Slam");
	AbilityNames[1] = TEXT("Barkskin");
	AbilityNames[2] = TEXT("Healing Grove");
	AbilityNames[3] = TEXT("Titan's Wrath");

	// Tank statline: huge health pool, modest damage and mana
	StartingMaxHealth = 165.0f;
	HealthPerLevel = 34.0f;
	StartingMaxMana = 80.0f;
	ManaPerLevel = 12.0f;
	StartingManaRegen = 4.5f;
	StartingBasePower = 22.0f;
	PowerPerLevel = 2.2f;

	// Ranged cannon auto: a heavy, chunky green single-target shot at a tank's
	// slow cadence. Shots fly FLAT (yaw only), so the muzzle height has to sit
	// in the target band — minions are short (~Z52-112 collision). Spawning at
	// TR's cannon-arm height sailed clean over their heads; drop it to hip level
	// where his cannon actually hangs, so the flat shot connects at melee AND
	// max range.
	FireRate = 0.9f;
	AutoAttackRange = 900.0f;       // aim-highlight reach (shots reach ~1050)
	ProjectileSpawnOffset = FVector(120.0f, 0.0f, -45.0f); // hip-height cannon, forward
	AutoAttackProjectileColor = FLinearColor(0.30f, 0.95f, 0.28f, 1.0f); // bright bark green
	AutoAttackProjectileScale = 0.95f; // chunky boulder-sized shot
	// Single target (no cleave) — CleaveEveryNthHit stays 0 from the base ctor

	// Shockwave Slam (ability 1 / index 0) is ground-placed — aim it with the slider
	bAbilityUsesGroundAim[0] = true;
	AbilityAimRadius[0] = 420.0f;
	AbilityAimRange[0] = 900.0f;
	// Barkskin (index 1) is a self-buff, not ground-placed — undo the base default
	bAbilityUsesGroundAim[1] = false;

	// Titan's Wrath charge defaults
	bTitanCharging = false;
	TitanChargeRemaining = 0.0f;
	TitanChargeDuration = 0.55f;    // ~2-3 steps of forward travel
	TitanChargeSpeed = 1000.0f;     // 1000 * 0.55 ~= 550 uu of travel
	TitanChargeDir = FVector::ForwardVector;
	TitanChargeAbilityRank = 0;

	// Ability tuning
	Ability1_Cooldown = 9.0f;
	Ability1_ManaCost = 25.0f;
	Ability2_Cooldown = 14.0f;
	Ability2_ManaCost = 30.0f;
	Ability3_Cooldown = 16.0f;
	Ability3_ManaCost = 45.0f;
	Ability4_Cooldown = 80.0f;
	Ability4_ManaCost = 100.0f;

	HealingGroveTicksRemaining = 0;
	HealingGroveHealPerTick = 0.0f;

	// Tree Rex's body: the source MODEL is ~470 units tall (pivot centered), but
	// the ripped ANIMATIONS are authored at game scale with the ground at the
	// skeleton origin — animated poses stand only ~130-160 units. So: full mesh
	// scale (animation dictates the visual size), capsule fitted to the ANIMATED
	// height, and the mesh dropped so the anim ground plane hits the capsule
	// bottom. Still roughly double Trigger Happy — reads as the giant he is.
	GetCapsuleComponent()->SetCapsuleSize(60.0f, 80.0f);
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -80.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f)); // Interchange meshes face +X already
	GetMesh()->SetRelativeScale3D(FVector(1.0f));

	// Giant-god camera (like SMITE's dynamic framing for large gods): pulled
	// back and looking at a raised point so Tree Rex sits centered in frame
	// with the lane ahead visible, rather than filling the top of the screen.
	if (CameraBoom)
	{
		CameraBoom->TargetArmLength = 680.0f;
		CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 250.0f);
	}

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BodyMesh(TEXT("/Game/Characters/TreeRex/Models/TreeRex"));
	if (BodyMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(BodyMesh.Object);
	}
	// Code-driven anim instance (no AnimBP authored for Tree Rex yet)
	GetMesh()->SetAnimInstanceClass(USkylandersSimpleAnimInstance::StaticClass());

	// Native Tree Rex animations, hand-triaged in the editor (the rip's names
	// are unreliable — several "drive"/pose files were kart poses or broken).
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> IdleSeq(TEXT("/Game/Characters/TreeRex/Animations/sequoiastampede_outwall"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> RunSeq(TEXT("/Game/Characters/TreeRex/Animations/photosynthesiscannon_hold"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> JumpSeq(TEXT("/Game/Characters/TreeRex/Animations/sequoiastampede_out"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> PunchA(TEXT("/Game/Characters/TreeRex/Animations/shockwave_out_upgrade"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> SlamOut(TEXT("/Game/Characters/TreeRex/Animations/photosynthesiscannon_in"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Brace(TEXT("/Game/Characters/TreeRex/Animations/shockwave_in"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> GroveCast(TEXT("/Game/Characters/TreeRex/Animations/magicmoment_intro"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Stampede(TEXT("/Game/Characters/TreeRex/Animations/emotion_idle_partial"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> HitReact(TEXT("/Game/Characters/TreeRex/Animations/takehit_groundfront"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> DeathSeq(TEXT("/Game/Characters/TreeRex/Animations/knockaway_back"));
	if (IdleSeq.Succeeded()) IdleLocomotionAnim = IdleSeq.Object;
	if (RunSeq.Succeeded()) RunLocomotionAnim = RunSeq.Object;
	if (JumpSeq.Succeeded()) JumpAnim = JumpSeq.Object;
	if (PunchA.Succeeded()) AttackLeftAnim = PunchA.Object;
	if (PunchA.Succeeded()) AttackRightAnim = PunchA.Object; // shockwave_out_upgrade cannon swing (user pick)
	if (SlamOut.Succeeded()) Ability1Anim = SlamOut.Object;   // Shockwave Slam
	if (Brace.Succeeded()) Ability2Anim = Brace.Object;       // Barkskin
	if (GroveCast.Succeeded()) HealingGroveAnim = GroveCast.Object; // Healing Grove
	if (Stampede.Succeeded()) YamatoAnim = Stampede.Object;   // Titan's Wrath (ultimate)
	if (HitReact.Succeeded()) HitReactAnim = HitReact.Object;
	if (DeathSeq.Succeeded()) DeathAnim = DeathSeq.Object;

	// HUD art (wiki power icons + portrait)
	static ConstructorHelpers::FObjectFinder<UTexture2D> Icon1(TEXT("/Game/Characters/TreeRex/UI/TreeRex_Icon1"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Icon2(TEXT("/Game/Characters/TreeRex/UI/TreeRex_Icon2"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Icon3(TEXT("/Game/Characters/TreeRex/UI/TreeRex_Icon3"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Icon4(TEXT("/Game/Characters/TreeRex/UI/TreeRex_Icon4"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Portrait(TEXT("/Game/Characters/TreeRex/UI/TreeRex_Portrait"));
	if (Icon1.Succeeded()) AbilityIconTextures[0] = Icon1.Object;
	if (Icon2.Succeeded()) AbilityIconTextures[1] = Icon2.Object;
	if (Icon3.Succeeded()) AbilityIconTextures[2] = Icon3.Object;
	if (Icon4.Succeeded()) AbilityIconTextures[3] = Icon4.Object;
	if (Portrait.Succeeded()) PortraitTexture = Portrait.Object;
}

void ASkylandersTreeRexCharacter::LoadCharacterVisuals()
{
	// Real mesh + materials come from the imported assets — nothing extra to load.
}

// The charge counts as channeling so the base class locks walk speed and blocks
// auto-fire while Tree Rex is barreling forward.
bool ASkylandersTreeRexCharacter::IsChanneling() const
{
	return Super::IsChanneling() || bTitanCharging;
}

void ASkylandersTreeRexCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Drive the Titan's Wrath charge: sweep the capsule forward each frame so
	// the elbow-slam anim reads as a real 2-3 step lunge, not a locked pose.
	if (bTitanCharging)
	{
		// Override any residual walk velocity — the charge fully owns movement
		GetCharacterMovement()->Velocity = FVector::ZeroVector;

		FHitResult SweepHit;
		AddActorWorldOffset(TitanChargeDir * TitanChargeSpeed * DeltaTime, true, &SweepHit);

		// Bark dust trail under him as he charges
		SpawnColoredMeshVFX(TEXT("/Engine/BasicShapes/Cylinder"),
			GetActorLocation() - FVector(0, 0, 78.0f), FRotator::ZeroRotator,
			FVector(1.6f, 1.6f, 0.03f), BarkGreen, 0.25f);

		TitanChargeRemaining -= DeltaTime;
		if (TitanChargeRemaining <= 0.0f || SweepHit.bBlockingHit)
		{
			FinishTitanCharge();
		}
	}
}

// Ability 1 - Shockwave Slam: ground slam AoE around Tree Rex
void ASkylandersTreeRexCharacter::UseAbility1()
{
	if (AbilityLevels[0] == 0)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Shockwave Slam not learned! Press F1 to level it."));
		return;
	}
	if (bIsRecalling) CancelRecall();
	if (IsChanneling()) return;
	if (Ability1_RemainingCooldown > 0.0f) return;

	if (UseMana(Ability1_ManaCost))
	{
		float CooldownScale = 1.2f - 0.04f * AbilityLevels[0];
		Ability1_RemainingCooldown = Ability1_Cooldown * CooldownScale;

		if (AbilitySound) UGameplayStatics::PlaySoundAtLocation(this, AbilitySound, GetActorLocation());
		PlayAnimOnSlot(Ability1Anim, 1.0f);

		const float SlamRadius = 420.0f;
		float RankScale = 0.6f + 0.08f * AbilityLevels[0];
		float SlamDamage = GetEffectivePower() * 2.4f * RankScale;

		// Ground-targeted: land the slam where the slider circle sits
		FVector SlamCenter = GetGroundAimPoint(AbilityAimRange[0] - SlamRadius);
		SlamCenter.Z = GetActorLocation().Z;

		// Shockwave ring visual
		SpawnColoredMeshVFX(TEXT("/Engine/BasicShapes/Cylinder"),
			SlamCenter - FVector(0, 0, 90.0f), FRotator::ZeroRotator,
			FVector(SlamRadius / 50.0f, SlamRadius / 50.0f, 0.06f), BarkGreen, 0.6f);

		int32 HitCount = DamageEnemiesInSphere(SlamCenter, SlamRadius, SlamDamage);
		UE_LOG(LogTemp, Log, TEXT("Shockwave Slam hit %d targets for %.0f damage"), HitCount, SlamDamage);
		if (HitCount > 0) AddCoins(HitCount);
	}
}

// Ability 2 - Barkskin: temporary protections buff
void ASkylandersTreeRexCharacter::UseAbility2()
{
	if (AbilityLevels[1] == 0)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Barkskin not learned! Press F2 to level it."));
		return;
	}
	if (bIsRecalling) CancelRecall();
	if (IsChanneling()) return;
	if (Ability2_RemainingCooldown > 0.0f) return;

	if (UseMana(Ability2_ManaCost))
	{
		float CooldownScale = 1.2f - 0.04f * AbilityLevels[1];
		Ability2_RemainingCooldown = Ability2_Cooldown * CooldownScale;

		if (AbilitySound) UGameplayStatics::PlaySoundAtLocation(this, AbilitySound, GetActorLocation());
		PlayAnimOnSlot(Ability2Anim, 1.0f);

		const float BuffDuration = 6.0f;
		BuffProtections = 30.0f + 10.0f * AbilityLevels[1];

		// Bark shell visual around the body while active
		SpawnColoredMeshVFX(TEXT("/Engine/BasicShapes/Cylinder"),
			GetActorLocation() - FVector(0, 0, 100.0f), FRotator::ZeroRotator,
			FVector(3.5f, 3.5f, 0.08f), FLinearColor(0.45f, 0.30f, 0.10f), BuffDuration);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
				FString::Printf(TEXT("Barkskin! +%.0f protections for %.0fs"), BuffProtections, BuffDuration));
		}

		// Clear the buff after the duration (re-cast just refreshes the timer)
		GetWorld()->GetTimerManager().SetTimer(BarkskinTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				BuffProtections = 0.0f;
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Silver, TEXT("Barkskin faded."));
			}), BuffDuration, false);
	}
}

// Ability 3 - Healing Grove: heal over time
void ASkylandersTreeRexCharacter::UseAbility3()
{
	if (AbilityLevels[2] == 0)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Healing Grove not learned! Press F3 to level it."));
		return;
	}
	if (bIsRecalling) CancelRecall();
	if (IsChanneling()) return;
	if (Ability3_RemainingCooldown > 0.0f) return;

	if (UseMana(Ability3_ManaCost))
	{
		float CooldownScale = 1.2f - 0.04f * AbilityLevels[2];
		Ability3_RemainingCooldown = Ability3_Cooldown * CooldownScale;

		if (AbilitySound) UGameplayStatics::PlaySoundAtLocation(this, AbilitySound, GetActorLocation());
		PlayAnimOnSlot(HealingGroveAnim, 1.0f);

		// Heal (14% + 2% per rank) of effective max HP over 4 seconds, in 8 ticks
		float EffectiveMaxHP = MaxHealth + ItemBonusStats.MaxHealth;
		float TotalHeal = EffectiveMaxHP * (0.14f + 0.02f * AbilityLevels[2]);
		HealingGroveTicksRemaining = 8;
		HealingGroveHealPerTick = TotalHeal / 8.0f;

		// Grove visual under Tree Rex
		SpawnColoredMeshVFX(TEXT("/Engine/BasicShapes/Cylinder"),
			GetActorLocation() - FVector(0, 0, 110.0f), FRotator::ZeroRotator,
			FVector(4.5f, 4.5f, 0.05f), GroveGreen, 4.0f);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
				FString::Printf(TEXT("Healing Grove! Restoring %.0f HP over 4s"), TotalHeal));
		}

		GetWorld()->GetTimerManager().SetTimer(HealingGroveTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (HealingGroveTicksRemaining > 0 && CurrentHealth > 0.0f)
				{
					Heal(HealingGroveHealPerTick);
					HealingGroveTicksRemaining--;
				}
				if (HealingGroveTicksRemaining <= 0)
				{
					GetWorld()->GetTimerManager().ClearTimer(HealingGroveTimerHandle);
				}
			}), 0.5f, true);
	}
}

// Ability 4 - Titan's Wrath: charges forward a few steps, then an elbow-slam
// smash lands at the end of the lunge (ultimate). Movement is NOT locked in
// place — Tree Rex physically travels while the elbow-slam anim plays.
void ASkylandersTreeRexCharacter::UseAbility4()
{
	if (AbilityLevels[3] == 0)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Titan's Wrath not learned! Press F4 to level it (requires level 5)."));
		return;
	}
	if (bIsRecalling) CancelRecall();
	if (IsChanneling()) return;
	if (Ability4_RemainingCooldown > 0.0f) return;

	if (UseMana(Ability4_ManaCost))
	{
		float CooldownScale = 1.2f - 0.04f * AbilityLevels[3];
		Ability4_RemainingCooldown = Ability4_Cooldown * CooldownScale;

		if (AbilitySound) UGameplayStatics::PlaySoundAtLocation(this, AbilitySound, GetActorLocation());
		PlayAnimOnSlot(YamatoAnim, 1.0f); // elbow-slam charge anim, full body

		// Lock in the charge direction (flat, aim yaw) and begin the lunge.
		// Tick() drives the forward travel; FinishTitanCharge() lands the smash.
		TitanChargeDir = FRotator(0.0f, GetControlRotation().Yaw, 0.0f).Vector();
		TitanChargeAbilityRank = AbilityLevels[3];
		TitanChargeRemaining = TitanChargeDuration;
		bTitanCharging = true;

		UE_LOG(LogTemp, Warning, TEXT("TITAN'S WRATH — charging forward!"));
	}
}

void ASkylandersTreeRexCharacter::FinishTitanCharge()
{
	bTitanCharging = false;
	TitanChargeRemaining = 0.0f;

	// Smash lands in a frontal arc where the charge ended
	const float WrathRange = 550.0f;
	float UltRankScale = 0.5f + 0.10f * FMath::Max(TitanChargeAbilityRank, 1);
	float WrathDamage = GetEffectivePower() * 12.0f * UltRankScale;

	FVector Dir = TitanChargeDir;

	// Massive smash visual in front of the landing spot
	SpawnColoredMeshVFX(TEXT("/Engine/BasicShapes/Sphere"),
		GetActorLocation() + Dir * (WrathRange * 0.5f), FRotator::ZeroRotator,
		FVector(WrathRange / 60.0f), BarkGreen, 0.9f);

	int32 HitCount = DamageEnemiesInCone(GetActorLocation(), Dir, WrathRange, 0.35f, WrathDamage, 0.2f);

	// Sustain: heal 6% of effective max HP per target smashed
	if (HitCount > 0)
	{
		float EffectiveMaxHP = MaxHealth + ItemBonusStats.MaxHealth;
		Heal(EffectiveMaxHP * 0.06f * HitCount);
		AddCoins(HitCount * 5);
	}

	UE_LOG(LogTemp, Warning, TEXT("TITAN'S WRATH smash hit %d targets for %.0f damage!"), HitCount, WrathDamage);
}

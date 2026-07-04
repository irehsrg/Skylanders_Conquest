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

	// Melee: swings once per second, short reach
	FireRate = 1.0f;
	AutoAttackRange = 320.0f;

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

	// Tree Rex's real body: the source model is ~470 units tall with the pivot
	// at its center — run it at half scale for a ~235-unit giant.
	GetCapsuleComponent()->SetCapsuleSize(60.0f, 118.0f);
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f)); // Interchange meshes face +X already
	GetMesh()->SetRelativeScale3D(FVector(0.5f));

	// Camera pulled back a little further for the bigger frame
	if (CameraBoom)
	{
		CameraBoom->TargetArmLength = 560.0f;
		CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 140.0f);
	}

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BodyMesh(TEXT("/Game/Characters/TreeRex/Models/TreeRex"));
	if (BodyMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(BodyMesh.Object);
	}
	// Code-driven anim instance (no AnimBP authored for Tree Rex yet)
	GetMesh()->SetAnimInstanceClass(USkylandersSimpleAnimInstance::StaticClass());

	// Native Tree Rex animations from the ripped set.
	// The rip has no plain idle/run: magicmoment_failloop is a standing loop that
	// reads as a heavy idle, and sequoiastampede_loop is his charge run.
	// (drive_idle/drive_run are kart-driving poses — verified wrong in PIE.)
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> IdleSeq(TEXT("/Game/Characters/TreeRex/Animations/magicmoment_failloop"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> RunSeq(TEXT("/Game/Characters/TreeRex/Animations/sequoiastampede_loop"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> PunchA(TEXT("/Game/Characters/TreeRex/Animations/photosynthesiscannon_basic"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> PunchB(TEXT("/Game/Characters/TreeRex/Animations/photosynthesiscannon_triple"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> SlamOut(TEXT("/Game/Characters/TreeRex/Animations/shockwave_out"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Brace(TEXT("/Game/Characters/TreeRex/Animations/shockwave_in"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> GroveCast(TEXT("/Game/Characters/TreeRex/Animations/magicmoment_intro"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Stampede(TEXT("/Game/Characters/TreeRex/Animations/sequoiastampede_out"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> HitReact(TEXT("/Game/Characters/TreeRex/Animations/takehit_groundfront"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> DeathSeq(TEXT("/Game/Characters/TreeRex/Animations/knockaway_back"));
	if (IdleSeq.Succeeded()) IdleLocomotionAnim = IdleSeq.Object;
	if (RunSeq.Succeeded()) RunLocomotionAnim = RunSeq.Object;
	if (PunchA.Succeeded()) AttackLeftAnim = PunchA.Object;
	if (PunchB.Succeeded()) AttackRightAnim = PunchB.Object;
	if (SlamOut.Succeeded()) Ability1Anim = SlamOut.Object;   // Shockwave Slam
	if (Brace.Succeeded()) Ability2Anim = Brace.Object;       // Barkskin
	if (GroveCast.Succeeded()) HealingGroveAnim = GroveCast.Object; // Healing Grove
	if (Stampede.Succeeded()) YamatoAnim = Stampede.Object;   // Titan's Wrath (ultimate)
	if (HitReact.Succeeded()) HitReactAnim = HitReact.Object;
	if (DeathSeq.Succeeded()) DeathAnim = DeathSeq.Object;
}

void ASkylandersTreeRexCharacter::LoadCharacterVisuals()
{
	// Real mesh + materials come from the imported assets — nothing extra to load.
}

// Melee swing helper: cone damage + enemy structures in reach
void ASkylandersTreeRexCharacter::MeleeCleave(float Damage, float Range, float MinDot)
{
	FVector Origin = GetActorLocation();
	FVector Dir = FRotator(0.0f, GetControlRotation().Yaw, 0.0f).Vector();

	// Enemies, minions, camps, gods
	int32 HitCount = DamageEnemiesInCone(Origin, Dir, Range, MinDot, Damage);

	// Auto attacks also damage structures — punch enemy towers/titans in reach
	TArray<AActor*> Structures;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersTower::StaticClass(), Structures);
	for (AActor* Actor : Structures)
	{
		ASkylandersTower* Tower = Cast<ASkylandersTower>(Actor);
		if (!Tower || Tower->Team != ETowerTeam::Enemy || Tower->bDestroyed) continue;
		FVector ToTower = Actor->GetActorLocation() - Origin;
		if (ToTower.Size2D() < Range + 150.0f && FVector::DotProduct(ToTower.GetSafeNormal2D(), Dir) > MinDot)
		{
			Tower->TakeDamage_Custom(Damage, this);
			HitCount++;
		}
	}
	Structures.Reset();
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersTitan::StaticClass(), Structures);
	for (AActor* Actor : Structures)
	{
		ASkylandersTitan* Titan = Cast<ASkylandersTitan>(Actor);
		if (!Titan || Titan->Team != ETowerTeam::Enemy || Titan->bDead) continue;
		FVector ToTitan = Actor->GetActorLocation() - Origin;
		if (ToTitan.Size2D() < Range + 200.0f && FVector::DotProduct(ToTitan.GetSafeNormal2D(), Dir) > MinDot)
		{
			Titan->TakeDamage_Custom(Damage, this);
			HitCount++;
		}
	}

	// Lifesteal on melee hits
	if (HitCount > 0 && ItemBonusStats.Lifesteal > 0.0f)
	{
		Heal(Damage * ItemBonusStats.Lifesteal * HitCount);
	}
}

// Auto attack: melee cleave instead of a projectile
void ASkylandersTreeRexCharacter::FireProjectile()
{
	if (bIsRecalling) CancelRecall();
	if (IsChanneling()) return;

	// Swing speed uses the same attack-speed pipeline as ranged autos
	float CurrentTime = GetWorld()->GetTimeSeconds();
	float SwingCooldown = 1.0f / GetEffectiveAttackSpeed();
	if (CurrentTime - LastFireTime < SwingCooldown) return;
	LastFireTime = CurrentTime;

	if (AttackSound) UGameplayStatics::PlaySoundAtLocation(this, AttackSound, GetActorLocation());

	// Alternate punch animations (reuses the L/R alternation flag)
	if (bFireFromLeftGun)
		PlayAnimOnSlot(AttackLeftAnim, 1.3f, 0.05f, 0.1f);
	else
		PlayAnimOnSlot(AttackRightAnim, 1.3f, 0.05f, 0.1f);
	bFireFromLeftGun = !bFireFromLeftGun;

	AttackSlowRemainingTime = AttackSlowDuration;

	float Damage = GetEffectivePower() * 1.15f;
	bool bCrit = FMath::FRand() < ItemBonusStats.CritChance;
	if (bCrit) Damage *= 2.0f;

	// Swing arc visual
	FVector Dir = FRotator(0.0f, GetControlRotation().Yaw, 0.0f).Vector();
	SpawnColoredMeshVFX(TEXT("/Engine/BasicShapes/Cylinder"),
		GetActorLocation() + Dir * (AutoAttackRange * 0.5f) - FVector(0, 0, 60.0f),
		FRotator::ZeroRotator,
		FVector(AutoAttackRange / 100.0f, AutoAttackRange / 100.0f, 0.03f),
		BarkGreen, 0.25f);

	MeleeCleave(Damage, AutoAttackRange, 0.35f);
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

		// Shockwave ring visual
		SpawnColoredMeshVFX(TEXT("/Engine/BasicShapes/Cylinder"),
			GetActorLocation() - FVector(0, 0, 90.0f), FRotator::ZeroRotator,
			FVector(SlamRadius / 50.0f, SlamRadius / 50.0f, 0.06f), BarkGreen, 0.6f);

		int32 HitCount = DamageEnemiesInSphere(GetActorLocation(), SlamRadius, SlamDamage);
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

// Ability 4 - Titan's Wrath: huge frontal cone smash that heals per target hit (ultimate)
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
		PlayAnimOnSlot(YamatoAnim, 0.9f);

		const float WrathRange = 550.0f;
		float UltRankScale = 0.5f + 0.10f * AbilityLevels[3];
		float WrathDamage = GetEffectivePower() * 12.0f * UltRankScale;

		FVector Dir = FRotator(0.0f, GetControlRotation().Yaw, 0.0f).Vector();

		// Massive smash visual in front
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

		UE_LOG(LogTemp, Warning, TEXT("TITAN'S WRATH hit %d targets for %.0f damage!"), HitCount, WrathDamage);
	}
}

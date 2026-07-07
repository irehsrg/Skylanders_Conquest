// Skylanders Conquest - Hex Implementation

#include "SkylandersHexCharacter.h"
#include "SkylandersAoEExplosion.h"
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

// Hex's signature purple
static const FLinearColor HexPurple(0.55f, 0.15f, 0.95f, 1.0f);
static const FLinearColor BoneWhite(0.92f, 0.90f, 0.80f, 1.0f);

ASkylandersHexCharacter::ASkylandersHexCharacter()
{
	CharacterName = TEXT("Hex");
	CharacterRole = TEXT("Undead Sorceress");
	AbilityNames[0] = TEXT("Phantom Orb");
	AbilityNames[1] = TEXT("Wall of Bones");
	AbilityNames[2] = TEXT("Rain of Skulls");
	AbilityNames[3] = TEXT("Skull Storm");

	// Mage statline: fragile, mana-rich, hits hard per shot
	StartingMaxHealth = 90.0f;
	HealthPerLevel = 17.0f;
	StartingMaxMana = 130.0f;
	ManaPerLevel = 22.0f;
	StartingManaRegen = 6.5f;
	StartingBasePower = 25.0f;
	PowerPerLevel = 2.6f;

	// Slow, heavy autos
	FireRate = 1.2f;
	AutoAttackProjectileColor = HexPurple;
	AutoAttackProjectileScale = 0.55f; // a slightly bigger orb than Trigger Happy's shot

	// Aimed abilities all show the hold-to-aim targeter. Phantom Orb (0) is a
	// line skillshot, Wall of Bones (1) and Rain of Skulls (2) place at a point;
	// Skull Storm (3) is a self-centered nova so it stays instant-cast.
	bAbilityUsesGroundAim[0] = true;
	AbilityAimRadius[0] = 110.0f;   // orb width — thin reticle for the skillshot
	AbilityAimRange[0] = 1400.0f;
	bAbilityUsesGroundAim[1] = true;
	AbilityAimRadius[1] = 150.0f;   // the wall is a line, keep the reticle tight
	AbilityAimRange[1] = 800.0f;
	bAbilityUsesGroundAim[2] = true;
	AbilityAimRadius[2] = 250.0f;
	AbilityAimRange[2] = 1050.0f;

	// She hovers, so the base camera framed her too high with the crosshair below
	// her. Raise the look point (like the giant-god camera) so she sits centered.
	if (CameraBoom)
	{
		CameraBoom->TargetArmLength = 480.0f;
		CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 200.0f);
	}

	// Ability tuning
	Ability1_Cooldown = 7.0f;
	Ability1_ManaCost = 25.0f;
	Ability2_Cooldown = 12.0f;
	Ability2_ManaCost = 40.0f;
	Ability3_Cooldown = 10.0f;
	Ability3_ManaCost = 45.0f;
	Ability4_Cooldown = 75.0f;
	Ability4_ManaCost = 100.0f;

	// Hex's real body: ~94 units tall, pivot at the model's center (she hovers).
	// Interchange-imported meshes already face +X — no yaw correction needed.
	// The ripped animations put the ground at the skeleton origin, so the mesh
	// sits capsule-half-height down (see Tree Rex for the full story).
	GetCapsuleComponent()->SetCapsuleSize(34.0f, 52.0f);
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -52.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BodyMesh(TEXT("/Game/Characters/Hex/Models/Hex"));
	if (BodyMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(BodyMesh.Object);
	}
	// Code-driven anim instance (no AnimBP authored for Hex yet)
	GetMesh()->SetAnimInstanceClass(USkylandersSimpleAnimInstance::StaticClass());

	// Native Hex animations from the ripped set
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> IdleSeq(TEXT("/Game/Characters/Hex/Animations/drive_idle"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> RunSeq(TEXT("/Game/Characters/Hex/Animations/drive_run"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> JumpSeq(TEXT("/Game/Characters/Hex/Animations/jumppad_launch"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> OrbL(TEXT("/Game/Characters/Hex/Animations/conjurephantomorb_left_partial"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> OrbR(TEXT("/Game/Characters/Hex/Animations/conjurephantomorb_right_partial"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> BigOrb(TEXT("/Game/Characters/Hex/Animations/twicetheorbage_partial"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> WallIn(TEXT("/Game/Characters/Hex/Animations/wallofbones_in"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> SkullsIn(TEXT("/Game/Characters/Hex/Animations/rainofskulls_in_partial"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> SkullsLoop(TEXT("/Game/Characters/Hex/Animations/rainofskulls_loop_partial"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> HitReact(TEXT("/Game/Characters/Hex/Animations/takehit_groundfront"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> DeathSeq(TEXT("/Game/Characters/Hex/Animations/knockaway_back"));
	if (IdleSeq.Succeeded()) IdleLocomotionAnim = IdleSeq.Object;
	if (RunSeq.Succeeded()) RunLocomotionAnim = RunSeq.Object;
	if (JumpSeq.Succeeded()) JumpAnim = JumpSeq.Object;
	if (OrbL.Succeeded()) AttackLeftAnim = OrbL.Object;
	if (OrbR.Succeeded()) AttackRightAnim = OrbR.Object;
	if (BigOrb.Succeeded()) Ability1Anim = BigOrb.Object;        // Phantom Orb
	if (WallIn.Succeeded()) Ability2Anim = WallIn.Object;        // Wall of Bones
	if (SkullsIn.Succeeded()) RainOfSkullsAnim = SkullsIn.Object; // Rain of Skulls
	if (SkullsLoop.Succeeded()) YamatoAnim = SkullsLoop.Object;  // Skull Storm (ultimate)
	if (HitReact.Succeeded()) HitReactAnim = HitReact.Object;
	if (DeathSeq.Succeeded()) DeathAnim = DeathSeq.Object;

	// HUD art (wiki power icons + portrait)
	static ConstructorHelpers::FObjectFinder<UTexture2D> Icon1(TEXT("/Game/Characters/Hex/UI/Hex_Icon1"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Icon2(TEXT("/Game/Characters/Hex/UI/Hex_Icon2"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Icon3(TEXT("/Game/Characters/Hex/UI/Hex_Icon3"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Icon4(TEXT("/Game/Characters/Hex/UI/Hex_Icon4"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Portrait(TEXT("/Game/Characters/Hex/UI/Hex_Portrait"));
	if (Icon1.Succeeded()) AbilityIconTextures[0] = Icon1.Object;
	if (Icon2.Succeeded()) AbilityIconTextures[1] = Icon2.Object;
	if (Icon3.Succeeded()) AbilityIconTextures[2] = Icon3.Object;
	if (Icon4.Succeeded()) AbilityIconTextures[3] = Icon4.Object;
	if (Portrait.Succeeded()) PortraitTexture = Portrait.Object;
}

void ASkylandersHexCharacter::LoadCharacterVisuals()
{
	// Real mesh + materials come from the imported assets — nothing extra to tint.
	// (No gun meshes: Hex casts from her hands.)
}

// Ability 1 - Phantom Orb: piercing line skillshot
void ASkylandersHexCharacter::UseAbility1()
{
	if (AbilityLevels[0] == 0)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Phantom Orb not learned! Press F1 to level it."));
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
		PlayAnimOnSlot(Ability1Anim, 1.2f);

		FVector StartLoc = GetActorLocation() + FVector(0, 0, 20.0f);
		FVector AimDir = FRotator(0.0f, GetControlRotation().Yaw, 0.0f).Vector();
		const float OrbRange = 1400.0f;
		const float OrbWidth = 90.0f;
		float RankScale = 0.6f + 0.08f * AbilityLevels[0];
		float OrbDamage = GetEffectivePower() * 3.0f * RankScale;

		// Purple orb trail visual
		SpawnColoredMeshVFX(TEXT("/Engine/BasicShapes/Cube"),
			StartLoc + AimDir * (OrbRange * 0.5f), AimDir.Rotation(),
			FVector(OrbRange / 100.0f, OrbWidth / 100.0f, 0.15f),
			HexPurple, 0.6f);
		SpawnColoredMeshVFX(TEXT("/Engine/BasicShapes/Sphere"),
			StartLoc + AimDir * OrbRange, FRotator::ZeroRotator,
			FVector(2.2f), HexPurple, 0.6f);

		int32 HitCount = DamageEnemiesInLine(StartLoc, AimDir, OrbRange, OrbWidth, OrbDamage);
		UE_LOG(LogTemp, Log, TEXT("Phantom Orb pierced %d targets for %.0f damage each"), HitCount, OrbDamage);
		if (HitCount > 0) AddCoins(HitCount);
	}
}

// Ability 2 - Wall of Bones: blocking wall at the aim point
void ASkylandersHexCharacter::UseAbility2()
{
	if (AbilityLevels[1] == 0)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Wall of Bones not learned! Press F2 to level it."));
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
		PlayAnimOnSlot(Ability2Anim, 1.2f);

		// Wall rises perpendicular to the aim direction, longer at higher ranks
		FVector WallCenter = GetGroundAimPoint(800.0f);
		float WallHalfHeight = 110.0f;
		WallCenter.Z = GetActorLocation().Z + WallHalfHeight - 40.0f; // Roughly ground level

		FRotator WallRot(0.0f, GetControlRotation().Yaw, 0.0f);
		float WallLength = 400.0f + 40.0f * AbilityLevels[1];
		float WallLifetime = 3.5f + 0.3f * AbilityLevels[1];

		AActor* Wall = SpawnColoredMeshVFX(TEXT("/Engine/BasicShapes/Cube"),
			WallCenter, WallRot,
			FVector(0.6f, WallLength / 100.0f, (WallHalfHeight * 2.0f) / 100.0f),
			BoneWhite, WallLifetime);

		// SpawnColoredMeshVFX makes decoration (no collision) — this one must block
		if (Wall)
		{
			if (UStaticMeshComponent* WallMesh = Cast<UStaticMeshComponent>(Wall->GetRootComponent()))
			{
				WallMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
				WallMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			}
		}

		UE_LOG(LogTemp, Log, TEXT("Wall of Bones raised for %.1fs"), WallLifetime);
	}
}

// Ability 3 - Rain of Skulls: ground-targeted AoE
void ASkylandersHexCharacter::UseAbility3()
{
	if (AbilityLevels[2] == 0)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Rain of Skulls not learned! Press F3 to level it."));
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
		PlayAnimOnSlot(RainOfSkullsAnim, 1.0f);

		FVector SpawnLoc = GetGroundAimPoint(1050.0f - 250.0f);
		SpawnLoc.Z = GetActorLocation().Z;

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;

		ASkylandersAoEExplosion* AoE = GetWorld()->SpawnActor<ASkylandersAoEExplosion>(
			ASkylandersAoEExplosion::StaticClass(), SpawnLoc, FRotator::ZeroRotator, SpawnParams);
		if (AoE)
		{
			float RankScale = 0.6f + 0.08f * AbilityLevels[2];
			AoE->DamageAmount = GetEffectivePower() * 4.5f * RankScale;
			AoE->ExplosionRadius = 300.0f;
			AoE->DetonationDelay = 0.5f;
		}
	}
}

// Ability 4 - Skull Storm: massive nova around Hex (ultimate)
void ASkylandersHexCharacter::UseAbility4()
{
	if (AbilityLevels[3] == 0)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Skull Storm not learned! Press F4 to level it (requires level 5)."));
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
		PlayAnimOnSlot(YamatoAnim, 1.0f);

		const float NovaRadius = 550.0f;
		float UltRankScale = 0.5f + 0.10f * AbilityLevels[3];
		float NovaDamage = GetEffectivePower() * 10.0f * UltRankScale;

		// Expanding purple nova visual
		SpawnColoredMeshVFX(TEXT("/Engine/BasicShapes/Sphere"),
			GetActorLocation(), FRotator::ZeroRotator,
			FVector(NovaRadius / 50.0f), HexPurple, 0.8f);
		SpawnColoredMeshVFX(TEXT("/Engine/BasicShapes/Cylinder"),
			GetActorLocation() - FVector(0, 0, 80.0f), FRotator::ZeroRotator,
			FVector(NovaRadius / 50.0f, NovaRadius / 50.0f, 0.05f), BoneWhite, 0.8f);

		int32 HitCount = DamageEnemiesInSphere(GetActorLocation(), NovaRadius, NovaDamage);
		UE_LOG(LogTemp, Warning, TEXT("SKULL STORM hit %d targets for %.0f damage!"), HitCount, NovaDamage);
		if (HitCount > 0) AddCoins(HitCount * 5);
	}
}

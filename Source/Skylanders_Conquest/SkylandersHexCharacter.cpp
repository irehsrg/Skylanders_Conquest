// Skylanders Conquest - Hex Implementation

#include "SkylandersHexCharacter.h"
#include "SkylandersAoEExplosion.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
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

	// Ability tuning
	Ability1_Cooldown = 7.0f;
	Ability1_ManaCost = 25.0f;
	Ability2_Cooldown = 12.0f;
	Ability2_ManaCost = 40.0f;
	Ability3_Cooldown = 10.0f;
	Ability3_ManaCost = 45.0f;
	Ability4_Cooldown = 75.0f;
	Ability4_ManaCost = 100.0f;

	// Mannequin-sized body (Trigger Happy's base capsule is tuned for a tiny character)
	GetCapsuleComponent()->SetCapsuleSize(35.0f, 90.0f);
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

	// Placeholder body: Quinn mannequin + unarmed AnimBP
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BodyMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"));
	if (BodyMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(BodyMesh.Object);
	}
	static ConstructorHelpers::FClassFinder<UAnimInstance> AnimBP(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
	if (AnimBP.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(AnimBP.Class);
	}

	// Cast gestures + reactions from the mannequin animation set
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> CastA(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> CastB(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_02"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> BigCast(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_ChargedAttack"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> HitReact(TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_01"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> DeathA(TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_01"));
	if (CastA.Succeeded()) AttackLeftAnim = CastA.Object;
	if (CastB.Succeeded()) AttackRightAnim = CastB.Object;
	if (CastA.Succeeded()) Ability1Anim = CastA.Object;
	if (CastB.Succeeded()) Ability2Anim = CastB.Object;
	if (BigCast.Succeeded()) YamatoAnim = BigCast.Object; // reused for ability visuals
	if (HitReact.Succeeded()) HitReactAnim = HitReact.Object;
	if (DeathA.Succeeded()) DeathAnim = DeathA.Object;
}

void ASkylandersHexCharacter::LoadCharacterVisuals()
{
	// No gun meshes — instead tint the mannequin purple so Hex reads as Hex.
	// Try the common tint parameter names; missing params are silently ignored.
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		for (int32 i = 0; i < Body->GetNumMaterials(); i++)
		{
			if (UMaterialInstanceDynamic* MID = Body->CreateDynamicMaterialInstance(i))
			{
				MID->SetVectorParameterValue(FName("Tint"), HexPurple);
				MID->SetVectorParameterValue(FName("Color"), HexPurple);
				MID->SetVectorParameterValue(FName("BodyColor"), HexPurple);
			}
		}
	}
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
		PlayAnimOnSlot(Ability2Anim, 1.0f);

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

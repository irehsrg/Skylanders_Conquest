// Skylanders Conquest - Player Character: Abilities
//
// Part of ASkylandersCharacter. The class is split across several
// translation units by concern; see SkylandersCharacter.cpp for the index.
// Ground targeting, projectile firing, recall, shared kit helpers, SMITE-style aiming and target highlight.

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
#include "SkylandersSmokeTest.h"
#include "SkylandersCameraShake.h"
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

	USkylandersActorRegistry::GetAllActorsOfClass(World, ASkylandersEnemy::StaticClass(), Found);
	for (AActor* Actor : Found)
	{
		ASkylandersEnemy* Enemy = Cast<ASkylandersEnemy>(Actor);
		if (Enemy && Enemy->CurrentHealth > 0.0f) OutTargets.Add(Actor);
	}

	Found.Reset();
	USkylandersActorRegistry::GetAllActorsOfClass(World, ASkylandersMinion::StaticClass(), Found);
	for (AActor* Actor : Found)
	{
		ASkylandersMinion* Minion = Cast<ASkylandersMinion>(Actor);
		if (Minion && Minion->Team == ETowerTeam::Enemy && !Minion->bDead) OutTargets.Add(Actor);
	}

	Found.Reset();
	USkylandersActorRegistry::GetAllActorsOfClass(World, ASkylandersBuffCamp::StaticClass(), Found);
	for (AActor* Actor : Found)
	{
		ASkylandersBuffCamp* Camp = Cast<ASkylandersBuffCamp>(Actor);
		if (Camp && !Camp->bDead) OutTargets.Add(Actor);
	}

	Found.Reset();
	USkylandersActorRegistry::GetAllActorsOfClass(World, ASkylandersEnemyGod::StaticClass(), Found);
	for (AActor* Actor : Found)
	{
		ASkylandersEnemyGod* God = Cast<ASkylandersEnemyGod>(Actor);
		// Only enemy-team gods — the player's abilities never hit ally gods
		if (God && God->Team == ETowerTeam::Enemy && God->CurrentState != EGodAIState::Dead) OutTargets.Add(Actor);
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
	if (Index >= 0 && Index <= 3)
	{
		if (USkylandersTelemetrySubsystem* Tele = USkylandersTelemetrySubsystem::Get(this))
		{
			Tele->LogAbility(CharacterName.IsEmpty() ? GetName() : CharacterName,
				TEXT("blue"), FString::Printf(TEXT("ability%d"), Index + 1), GetActorLocation());
		}
	}

	// Every ability starts its cooldown when it really fires, so a cooldown that
	// went from ready to running means the cast happened (not a rejected press).
	auto Cooldown = [this](int32 i) -> float
	{
		switch (i)
		{
		case 0: return Ability1_RemainingCooldown;
		case 1: return Ability2_RemainingCooldown;
		case 2: return Ability3_RemainingCooldown;
		case 3: return Ability4_RemainingCooldown;
		default: return 0.0f;
		}
	};
	const bool bWasReady = Cooldown(Index) <= 0.0f;

	switch (Index)
	{
	case 0: UseAbility1(); break;
	case 1: UseAbility2(); break;
	case 2: UseAbility3(); break;
	case 3: UseAbility4(); break;
	default: return;
	}

	if (bWasReady && Cooldown(Index) > 0.0f)
	{
		SKY_COUNT("player_ability");
		USkylandersCameraShakeLibrary::Shake(this, false, 0.6f);
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

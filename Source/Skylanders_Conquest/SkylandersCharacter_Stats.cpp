// Skylanders Conquest - Player Character: Stats
//
// Part of ASkylandersCharacter. The class is split across several
// translation units by concern; see SkylandersCharacter.cpp for the index.
// Damage, healing, mana, gold, XP/levels, death and respawn.

#include "SkylandersCharacter.h"
#include "SkylandersEnemyGod.h"
#include "SkylandersSmokeTest.h"
#include "SkylandersHitFlash.h"
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

// ========== DAMAGE AND HEALING ==========

AActor* ASkylandersCharacter::ResolveDamageHero(AActor* DamageCauser)
{
	if (!DamageCauser) return nullptr;
	// Same chain as GetPenetrationOf / the gods' LastDamageCauser: a projectile or
	// AoE is the causer, its shooter the instigator.
	AActor* Src = DamageCauser->GetInstigator() ? static_cast<AActor*>(DamageCauser->GetInstigator()) : DamageCauser;
	return (Cast<ASkylandersCharacter>(Src) || Cast<ASkylandersEnemyGod>(Src)) ? Src : nullptr;
}

float ASkylandersCharacter::GetPenetrationOf(const AActor* DamageCauser)
{
	if (!DamageCauser) return 0.0f;
	// Projectiles and AoEs pass themselves as the causer with the shooter as instigator,
	// same chain the kill-credit code follows.
	const AActor* Src = DamageCauser->GetInstigator() ? static_cast<const AActor*>(DamageCauser->GetInstigator()) : DamageCauser;
	if (const ASkylandersCharacter* C = Cast<ASkylandersCharacter>(Src)) return C->ItemBonusStats.Penetration;
	if (const ASkylandersEnemyGod* G = Cast<ASkylandersEnemyGod>(Src)) return G->ItemBonusStats.Penetration;
	return 0.0f;
}

void ASkylandersCharacter::TakeDamage_Custom(float DamageAmount, AActor* DamageCauser)
{
	// Cancel recall if channeling
	if (bIsRecalling) CancelRecall();

	if (CurrentHealth <= 0.0f)
	{
		return; // Already dead
	}

	// Remember who hit us, for kill credit and assists in Die()
	if (DamageCauser)
	{
		AActor* CauserPawn = DamageCauser->GetInstigator() ? static_cast<AActor*>(DamageCauser->GetInstigator()) : DamageCauser;
		LastDamageCauser = CauserPawn;
		if (ASkylandersEnemyGod* HeroGod = Cast<ASkylandersEnemyGod>(ResolveDamageHero(DamageCauser)))
		{
			if (HeroGod->Team == ETowerTeam::Enemy)
			{
				RecentHeroDamagers.Add(HeroGod, GetWorld()->GetTimeSeconds());
			}
		}
	}

	// Apply protections: damage reduction formula (like SMITE)
	// Reduction = Protections / (Protections + 100)
	// Attacker penetration strips protections first (floor 0: pen never adds damage).
	float Prots = FMath::Max(0.0f, GetEffectiveProtections() - GetPenetrationOf(DamageCauser));
	float DamageReduction = Prots / (Prots + 100.0f);
	float FinalDamage = DamageAmount * (1.0f - DamageReduction);

	float EffectiveMaxHP = MaxHealth + ItemBonusStats.MaxHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth - FinalDamage, 0.0f, EffectiveMaxHP);
	USkylandersHitFlashComponent::Flash(this, FLinearColor(3.0f, 0.6f, 0.5f, 1.0f));
	SKY_COUNT("player_damage");
	// Shake scales with the share of max HP lost: a quarter of the bar is a full light shake.
	USkylandersCameraShakeLibrary::Shake(this, false,
		FMath::Clamp(EffectiveMaxHP > 0.0f ? FinalDamage / EffectiveMaxHP * 4.0f : 1.0f, 0.3f, 1.0f));

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
	// Verbose: regen calls this every frame
	UE_LOG(LogTemp, Verbose, TEXT("Healed %f! Health: %f / %f"), HealAmount, CurrentHealth, MaxHealth);
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
	// Verbose: passive income lands here every second
	UE_LOG(LogTemp, Verbose, TEXT("Coins added: %d | Total: %d"), Amount, Coins);
	UpdateHUD();

	// A Blueprint HUD override keeps its own coin accumulator event; the C++
	// HUD just shows Coins from FlushHUD.
	if (MainHUDWidget && !NativeHUD)
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
			UE_LOG(LogTemp, Verbose, TEXT("HUD coin display updated"));
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

	USkylandersKillFeedWidget::Post(this,
		FString::Printf(TEXT("Level %d! %d skill point%s (F1-F4)"), PlayerLevel, AbilityPoints, AbilityPoints == 1 ? TEXT("") : TEXT("s")),
		FLinearColor(1.0f, 0.85f, 0.2f));

	UpdateHUD();
}

// ========== DEATH AND RESPAWN ==========

void ASkylandersCharacter::Die()
{
	Deaths++;
	SKY_COUNT("player_death");
	USkylandersCameraShakeLibrary::Shake(this, true, 0.8f);
	UE_LOG(LogTemp, Warning, TEXT("Player Died! Deaths: %d"), Deaths);

	// Kill feed: player death
	USkylandersKillFeedWidget::Post(this, TEXT("You were slain"), FLinearColor(1.0f, 0.25f, 0.25f));
	// The player is a god too, so dying first is first blood. No kill sting on
	// our own death banner: DeathSound already plays below.
	if (USkylandersAnnouncerWidget::ClaimFirstBlood(this))
	{
		USkylandersAnnouncerWidget::Announce(this, TEXT("FIRST BLOOD"), FLinearColor(1.0f, 0.8f, 0.2f), 2.2f, TEXT("kill"));
	}
	USkylandersAnnouncerWidget::Announce(this, TEXT("YOU HAVE BEEN SLAIN"), FLinearColor(1.0f, 0.2f, 0.2f));

	if (USkylandersTelemetrySubsystem* Tele = USkylandersTelemetrySubsystem::Get(this))
	{
		Tele->LogPlayerDeath(this, TEXT("combat"));
	}

	// Kill credit and assists for the red gods. The player's bounty mirrors a
	// god's CoinReward curve (150 + 15/level) so assists have something to split.
	{
		const int32 Bounty = 150 + (PlayerLevel - 1) * 15;
		ASkylandersEnemyGod* KillerGod = Cast<ASkylandersEnemyGod>(LastDamageCauser.Get());
		if (KillerGod && KillerGod->Team == ETowerTeam::Enemy)
		{
			KillerGod->Kills++;
			KillerGod->Gold += Bounty;
			USkylandersKillFeedWidget::Post(this, FString::Printf(TEXT("%s slew you"), *KillerGod->GodName),
				FLinearColor(1.0f, 0.4f, 0.35f));
		}
		else
		{
			KillerGod = nullptr;
		}

		const float Now = GetWorld()->GetTimeSeconds();
		for (const TPair<TWeakObjectPtr<AActor>, float>& Hit : RecentHeroDamagers)
		{
			ASkylandersEnemyGod* God = Cast<ASkylandersEnemyGod>(Hit.Key.Get());
			if (!God || God == KillerGod || Now - Hit.Value > AssistWindowSeconds) continue;
			God->Assists++;
			God->Gold += Bounty / 2;
			SKY_COUNT("assist");
		}
		RecentHeroDamagers.Reset();
		LastDamageCauser.Reset();
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

	// Close UI that would be stuck open while input is disabled
	CloseShop();
	HideScoreboard();

	// Disable input and movement
	DisableInput(Cast<APlayerController>(GetController()));
	GetCharacterMovement()->DisableMovement();

	// Respawn time scales with level and match length (SMITE-like): 5s base
	// + 2s per level + 1.5s per full match minute, capped at 45s
	float RespawnTime = FMath::Min(5.0f + (PlayerLevel * 2.0f) + FMath::FloorToFloat(GameElapsedTime / 60.0f) * 1.5f, 45.0f);

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
			USkylandersActorRegistry::GetAllActorsOfClass(GetWorld(), ASkylandersTitan::StaticClass(), AllTitans);
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
	USkylandersAnnouncerWidget::ClearStatus(this);
	DeathRespawnTimeRemaining = 0.0f;
	// Hits from the last life must not count toward the next death's assists
	RecentHeroDamagers.Reset();
	LastDamageCauser.Reset();

	// Kill feed: player respawn
	USkylandersKillFeedWidget::Post(this, TEXT("You have respawned"), FLinearColor(0.4f, 1.0f, 0.5f));
	SkylandersSound::Play2D(this, TEXT("respawn"));

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
	FlushHUD();
}

AActor* ASkylandersCharacter::GetPlayerStart()
{
	// Prefer the friendly spawn area (the fountain the map builder places at the
	// base). PlayerStart actors are Static and can't be repositioned at runtime,
	// so after a procedural map rebuild the .umap PlayerStart gets stranded
	// mid-map — use it only as a fallback.
	TArray<AActor*> Areas;
	USkylandersActorRegistry::GetAllActorsOfClass(GetWorld(), ASkylandersSpawnArea::StaticClass(), Areas);
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
	USkylandersActorRegistry::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), FoundActors);
	if (FoundActors.Num() > 0)
	{
		return FoundActors[0];
	}

	return nullptr;
}


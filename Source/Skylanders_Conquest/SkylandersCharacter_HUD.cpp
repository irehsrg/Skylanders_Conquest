// Skylanders Conquest - Player Character: HUD
//
// Part of ASkylandersCharacter. The class is split across several
// translation units by concern; see SkylandersCharacter.cpp for the index.
// Main HUD, cooldown UI, scoreboard and pause menu.

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

// ========== HUD UPDATES ==========

// Names of the HUD Blueprint's own variables its bindings read. Both spellings
// exist across HUD revisions; whichever the class has gets written.
static const TCHAR* const HUDValuePropNames[8] = {
	TEXT("Current Health"), TEXT("Max Health"), TEXT("CurrentHealth"), TEXT("MaxHealth"),
	TEXT("Current Mana"), TEXT("Max Mana"), TEXT("CurrentMana"), TEXT("MaxMana")
};

// Resolves a HUD variable to a float or double property (UE5 Blueprint floats
// are usually doubles). Anything else counts as missing.
static FProperty* FindHUDValueProperty(UUserWidget* Widget, const TCHAR* Name)
{
	FProperty* Prop = Widget->GetClass()->FindPropertyByName(FName(Name));
	return (CastField<FFloatProperty>(Prop) || CastField<FDoubleProperty>(Prop)) ? Prop : nullptr;
}

// Writes a property resolved by FindHUDValueProperty
static void SetWidgetFloatProperty(UUserWidget* Widget, FProperty* Prop, float Value)
{
	if (!Widget || !Prop) return;
	if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
	{
		FloatProp->SetPropertyValue_InContainer(Widget, Value);
	}
	else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
	{
		DoubleProp->SetPropertyValue_InContainer(Widget, (double)Value);
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
	// Callers (regen, damage, gold, XP, shop...) fire many times per frame;
	// the actual widget writes are batched into one FlushHUD() at the end of Tick.
	bHUDDirty = true;
}

void ASkylandersCharacter::FlushHUD()
{
	if (!bHUDDirty || !MainHUDWidget)
	{
		return;
	}
	bHUDDirty = false;

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
	float XPPercent = (XPToNextLevel > 0.0f) ? (CurrentXP / XPToNextLevel) : 0.0f;

	// Regen marks the HUD dirty every frame but usually moves HP/mana by a
	// fraction of a point; nothing visible changes until a whole number ticks over.
	if (RoundedHP == LastHUD.HP && RoundedMaxHP == LastHUD.MaxHP
		&& RoundedMP == LastHUD.MP && RoundedMaxMP == LastHUD.MaxMP
		&& XPPercent == LastHUD.XPPct && PlayerLevel == LastHUD.Level
		&& Coins == LastHUD.Coins && Inventory == LastHUD.Inventory)
	{
		return;
	}
	LastHUD.HP = RoundedHP;
	LastHUD.MaxHP = RoundedMaxHP;
	LastHUD.MP = RoundedMP;
	LastHUD.MaxMP = RoundedMaxMP;
	LastHUD.XPPct = XPPercent;
	LastHUD.Level = PlayerLevel;
	LastHUD.Coins = Coins;
	LastHUD.Inventory = Inventory;

	if (!bHUDPropsResolved)
	{
		for (int32 i = 0; i < UE_ARRAY_COUNT(HUDValuePropNames); i++)
		{
			HUDValueProps[i] = FindHUDValueProperty(MainHUDWidget, HUDValuePropNames[i]);
		}
		bHUDPropsResolved = true;
	}

	// Same order as HUDValuePropNames
	const float PropValues[8] = {
		RoundedHP, RoundedMaxHP, RoundedHP, RoundedMaxHP,
		RoundedMP, RoundedMaxMP, RoundedMP, RoundedMaxMP
	};
	for (int32 i = 0; i < UE_ARRAY_COUNT(PropValues); i++)
	{
		SetWidgetFloatProperty(MainHUDWidget, HUDValueProps[i], PropValues[i]);
	}

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


// ========== COOLDOWNS & COOLDOWN UI ==========

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

	// Runs every tick: when nothing is cooling down and nothing was last time,
	// the overlays are already hidden and there is no work at all.
	const bool bAllReady = Cooldowns[0] <= 0.0f && Cooldowns[1] <= 0.0f && Cooldowns[2] <= 0.0f && Cooldowns[3] <= 0.0f;
	if (bAllReady && bCooldownsIdle) return;
	bCooldownsIdle = bAllReady;

	for (int32 i = 0; i < 4; i++)
	{
		if (!CooldownOverlay[i] || !CooldownText[i]) continue;

		// Whole seconds shown on the slot; 0 = ready (hidden)
		const int32 SecondsLeft = (Cooldowns[i] > 0.0f) ? FMath::CeilToInt(Cooldowns[i]) : 0;
		if (SecondsLeft == LastCooldownSeconds[i]) continue;
		const bool bWasShown = LastCooldownSeconds[i] > 0;
		LastCooldownSeconds[i] = SecondsLeft;

		if (SecondsLeft > 0)
		{
			// Big gold countdown number with black outline, centered. Styling
			// survives hide/show, so it only needs applying the first time.
			if (!bCooldownTextStyled[i])
			{
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
				bCooldownTextStyled[i] = true;
			}

			if (!bWasShown)
			{
				// On cooldown - show dark overlay and countdown text
				CooldownOverlay[i]->SetVisibility(ESlateVisibility::HitTestInvisible);
				CooldownOverlay[i]->SetRenderOpacity(0.85f);
				CooldownText[i]->SetVisibility(ESlateVisibility::HitTestInvisible);
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
		USkylandersKillFeedWidget::Post(this, FString::Printf(TEXT("%s not learned! Press F1 to level it."), *AbilityNames[0]), FLinearColor(1.0f, 0.4f, 0.35f));
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
		Ability1_RemainingCooldown = Ability1_Cooldown * CooldownScale * GetCooldownMultiplier();
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
		USkylandersKillFeedWidget::Post(this, FString::Printf(TEXT("%s not learned! Press F2 to level it."), *AbilityNames[1]), FLinearColor(1.0f, 0.4f, 0.35f));
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
		Ability2_RemainingCooldown = Ability2_Cooldown * CooldownScale * GetCooldownMultiplier();
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
		USkylandersKillFeedWidget::Post(this, FString::Printf(TEXT("%s not learned! Press F3 to level it."), *AbilityNames[2]), FLinearColor(1.0f, 0.4f, 0.35f));
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
		Ability3_RemainingCooldown = Ability3_Cooldown * CooldownScale * GetCooldownMultiplier();
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
		USkylandersKillFeedWidget::Post(this, FString::Printf(TEXT("%s not learned! Press F4 to level it (requires level 5)."), *AbilityNames[3]), FLinearColor(1.0f, 0.4f, 0.35f));
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
		Ability4_RemainingCooldown = Ability4_Cooldown * CooldownScale * GetCooldownMultiplier();
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


// ========== PAUSE MENU ==========

void ASkylandersCharacter::TogglePauseMenu()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	if (PauseMenuWidget && PauseMenuWidget->IsInViewport())
	{
		PauseMenuWidget->Resume();
		return;
	}

	PauseMenuWidget = CreateWidget<USkylandersPauseMenuWidget>(PC, USkylandersPauseMenuWidget::StaticClass());
	if (!PauseMenuWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to create pause menu widget."));
		return;
	}

	PauseMenuWidget->OnClosed.BindUObject(this, &ASkylandersCharacter::OnPauseMenuClosed);
	PauseMenuWidget->AddToViewport(200); // above HUD and shop

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(PauseMenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = true;

	PC->SetPause(true);
}

void ASkylandersCharacter::OnPauseMenuClosed()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	// The shop keeps its own cursor and GameAndUI mode, so only drop back to
	// game-only input when it is not the thing the player was doing.
	if (bIsShopOpen)
	{
		PC->bShowMouseCursor = true;
		PC->SetInputMode(FInputModeGameAndUI());
	}
	else
	{
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
	}
}


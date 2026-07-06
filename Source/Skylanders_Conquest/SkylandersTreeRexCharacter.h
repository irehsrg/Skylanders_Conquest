// Skylanders Conquest - Tree Rex, the Giant Guardian (Tank / Support)
// Placeholder visuals: scaled-up Manny mannequin with a bark-green tint until the real model is imported.

#pragma once

#include "CoreMinimal.h"
#include "SkylandersCharacter.h"
#include "SkylandersTreeRexCharacter.generated.h"

/**
 * Tree Rex - frontline bruiser with self-sustain and a cannon-arm auto.
 *   Auto:  ranged cannon shot; every 3rd shot in the chain cleaves nearby foes
 *   1 - Shockwave Slam: ground-targeted slam AoE (aim with the slider)
 *   2 - Barkskin:       temporary protections buff (self)
 *   3 - Healing Grove:  heal over time (self)
 *   4 - Titan's Wrath:  charges forward a few steps, then an elbow-slam smash (ultimate)
 */
UCLASS()
class SKYLANDERS_CONQUEST_API ASkylandersTreeRexCharacter : public ASkylandersCharacter
{
	GENERATED_BODY()

public:
	ASkylandersTreeRexCharacter();

	virtual void Tick(float DeltaTime) override;
	virtual void UseAbility1() override;
	virtual void UseAbility2() override;
	virtual void UseAbility3() override;
	virtual void UseAbility4() override;

protected:
	virtual void LoadCharacterVisuals() override;
	virtual bool IsChanneling() const override;

	// Cast animation for Healing Grove (ability 3)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimSequenceBase* HealingGroveAnim = nullptr;

	// Barkskin state
	FTimerHandle BarkskinTimerHandle;

	// Healing Grove state
	FTimerHandle HealingGroveTimerHandle;
	int32 HealingGroveTicksRemaining;
	float HealingGroveHealPerTick;

	// ===== Titan's Wrath charge (ultimate) =====
	// The ult moves Tree Rex forward a couple of steps while the elbow-slam
	// anim plays, then lands the smash at the end of the charge.
	bool bTitanCharging;
	float TitanChargeRemaining;   // seconds left in the charge
	float TitanChargeDuration;    // total charge time
	float TitanChargeSpeed;       // forward uu/s during the charge
	FVector TitanChargeDir;       // locked-in charge direction
	int32 TitanChargeAbilityRank; // rank captured at cast (for smash damage)

	// Lands the Titan's Wrath smash at the end of the charge
	void FinishTitanCharge();
};

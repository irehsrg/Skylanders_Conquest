// Skylanders Conquest - Tree Rex, the Giant Guardian (Tank / Support)
// Placeholder visuals: scaled-up Manny mannequin with a bark-green tint until the real model is imported.

#pragma once

#include "CoreMinimal.h"
#include "SkylandersCharacter.h"
#include "SkylandersTreeRexCharacter.generated.h"

/**
 * Tree Rex - melee frontline tank with self-sustain.
 *   Auto:  melee cleave in front (also damages towers/titans, like any auto attack)
 *   1 - Shockwave Slam: ground slam AoE around Tree Rex
 *   2 - Barkskin:       temporary protections buff
 *   3 - Healing Grove:  heal over time
 *   4 - Titan's Wrath:  huge frontal cone smash that heals per target hit (ultimate)
 */
UCLASS()
class SKYLANDERS_CONQUEST_API ASkylandersTreeRexCharacter : public ASkylandersCharacter
{
	GENERATED_BODY()

public:
	ASkylandersTreeRexCharacter();

	virtual void FireProjectile() override; // Melee cleave instead of a projectile
	virtual void UseAbility1() override;
	virtual void UseAbility2() override;
	virtual void UseAbility3() override;
	virtual void UseAbility4() override;

protected:
	virtual void LoadCharacterVisuals() override;

	// Melee swing that also checks enemy towers/titans (auto attacks damage structures)
	void MeleeCleave(float Damage, float Range, float MinDot);

	// Barkskin state
	FTimerHandle BarkskinTimerHandle;

	// Healing Grove state
	FTimerHandle HealingGroveTimerHandle;
	int32 HealingGroveTicksRemaining;
	float HealingGroveHealPerTick;
};

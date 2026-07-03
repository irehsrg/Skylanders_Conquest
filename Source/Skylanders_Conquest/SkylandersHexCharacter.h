// Skylanders Conquest - Hex, the Undead Sorceress (Mage)
// Placeholder visuals: Quinn mannequin with a purple tint until the real model is imported.

#pragma once

#include "CoreMinimal.h"
#include "SkylandersCharacter.h"
#include "SkylandersHexCharacter.generated.h"

/**
 * Hex - ranged burst mage.
 *   Auto:  slow, hard-hitting purple bolts
 *   1 - Phantom Orb:   piercing line skillshot
 *   2 - Wall of Bones: blocking wall at the aim point
 *   3 - Rain of Skulls: ground-targeted AoE
 *   4 - Skull Storm:   massive nova around Hex (ultimate)
 */
UCLASS()
class SKYLANDERS_CONQUEST_API ASkylandersHexCharacter : public ASkylandersCharacter
{
	GENERATED_BODY()

public:
	ASkylandersHexCharacter();

	virtual void UseAbility1() override;
	virtual void UseAbility2() override;
	virtual void UseAbility3() override;
	virtual void UseAbility4() override;

protected:
	virtual void LoadCharacterVisuals() override;
};

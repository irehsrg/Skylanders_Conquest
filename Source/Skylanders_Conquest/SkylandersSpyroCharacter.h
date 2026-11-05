// Skylanders Conquest - Spyro, the Purple Dragon (Bruiser)
// No portrait / power-icon art exists for Spyro yet: PortraitTexture and
// AbilityIconTextures stay null and the C++ HUD draws its dark empty slot.

#pragma once

#include "CoreMinimal.h"
#include "SkylandersCharacter.h"
#include "SkylandersSpyroCharacter.generated.h"

/**
 * Spyro - mobile mid-range brawler.
 *   Auto:  Flameball, a ranged fire projectile
 *   1 - Flame Breath:     frontal fire cone
 *   2 - Charge:           horns-down dash that rams everything on the path, then a speed burst
 *   3 - Comet Dash:       ground-aimed leap that crashes down on the target area
 *   4 - Daybringer Flame: 1 s wind-up, then a huge fire cone (ultimate)
 */
UCLASS()
class SKYLANDERS_CONQUEST_API ASkylandersSpyroCharacter : public ASkylandersCharacter
{
	GENERATED_BODY()

public:
	ASkylandersSpyroCharacter();

	virtual void Tick(float DeltaTime) override;
	virtual void UseAbility1() override;
	virtual void UseAbility2() override;
	virtual void UseAbility3() override;
	virtual void UseAbility4() override;

protected:
	virtual void LoadCharacterVisuals() override;
	virtual bool IsChanneling() const override;

	// Charge wind-up and full-speed loop (ability 2)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimSequenceBase* ChargeInAnim = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimSequenceBase* ChargeFastAnim = nullptr;

	// Comet Dash take-off and touchdown (ability 3)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimSequenceBase* CometLaunchAnim = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimSequenceBase* CometLandAnim = nullptr;

	// ===== Charge (ability 2) =====
	bool bCharging = false;
	bool bChargeFastPlaying = false;  // switched from charge_in to charge_fast mid-dash
	float ChargeElapsed = 0.0f;
	float ChargeDuration = 0.35f;
	float ChargeDistance = 700.0f;
	FVector ChargeDir = FVector::ForwardVector;
	FVector ChargeStart = FVector::ZeroVector;
	int32 ChargeRank = 0;             // rank captured at cast (for the ram damage)

	// Move-speed burst after the charge lands
	float SpeedBoostRemaining = 0.0f;
	float SpeedBoostDuration = 2.0f;
	float SpeedBoostMultiplier = 1.25f;

	// ===== Comet Dash (ability 3) =====
	bool bCometLeaping = false;
	float CometElapsed = 0.0f;
	float CometDuration = 0.45f;
	float CometArcHeight = 180.0f;
	FVector CometStart = FVector::ZeroVector;
	FVector CometEnd = FVector::ZeroVector;
	int32 CometRank = 0;

	// ===== Daybringer Flame (ultimate) =====
	bool bDaybringerChanneling = false;
	float DaybringerRemaining = 0.0f;
	float DaybringerChannelTime = 1.0f;
	int32 DaybringerRank = 0;

	void TickCharge(float DeltaTime);
	void FinishCharge();
	void TickComet(float DeltaTime);
	void FinishComet();
	void TickDaybringer(float DeltaTime);
	void ReleaseDaybringer();

	// Spawns a flattened cone VFX with its tip at Origin, opening along Dir
	AActor* SpawnFireCone(const FVector& Origin, const FVector& Dir, float Length, float HalfWidth,
		float Thickness, const FLinearColor& Color, float Lifespan);

	// Drops any in-flight dash/leap/channel without its payoff (death)
	void CancelSpyroStates();
};

// Skylanders Conquest - Animation Instance for TriggerHappy
// Pulls gameplay state from SkylandersCharacter every frame for the ABP state machine

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "SkylandersAnimInstance.generated.h"

UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// ========== LOCOMOTION (used by ABP state machine) ==========

	// Ground speed (0 = idle, >0 = moving)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float Speed;

	// Movement direction relative to facing (-180 to 180)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float Direction;

	// Whether character is in the air
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	bool bIsInAir;

	// ========== COMBAT STATE (used by ABP transitions/blends) ==========

	// True during attack slow window (just fired)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	bool bIsAttacking;

	// Which gun just fired (for left/right attack anim selection)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	bool bFireFromLeftGun;

	// Ability 3 active (machine gun stance)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	bool bIsMachineGunActive;

	// Ability 4 charging (yamato stance)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	bool bIsChargingYamato;

	// Dead
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	bool bIsDead;

private:
	UPROPERTY()
	TObjectPtr<class ASkylandersCharacter> OwnerCharacter;

	UPROPERTY()
	TObjectPtr<class UCharacterMovementComponent> MovementComponent;
};

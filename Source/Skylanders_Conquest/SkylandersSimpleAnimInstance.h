// Skylanders Conquest - Code-driven AnimInstance (no AnimBP graph needed)
//
// Characters whose animation Blueprints haven't been authored yet (Hex, Tree Rex)
// use this instance: it samples an idle and a run sequence directly, blends them
// by movement speed, and supports a full-body override slot for attacks/abilities/
// death. Assign IdleAnim/RunAnim in the character constructor; play one-shots via
// PlayFullBodyAnim (ASkylandersCharacter::PlayAnimOnSlot routes here automatically).

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "SkylandersSimpleAnimInstance.generated.h"

class UAnimSequenceBase;

USTRUCT()
struct FSkylandersSimpleAnimProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FSkylandersSimpleAnimProxy() {}
	FSkylandersSimpleAnimProxy(UAnimInstance* InInstance) : FAnimInstanceProxy(InInstance) {}

	virtual bool Evaluate(FPoseContext& Output) override;
	virtual void UpdateAnimationNode(const FAnimationUpdateContext& InContext) override;

	// Copied from the game thread each frame (PreUpdate)
	UAnimSequenceBase* IdleSequence = nullptr;
	UAnimSequenceBase* RunSequence = nullptr;
	UAnimSequenceBase* OverrideSequence = nullptr;
	float LocomotionAlpha = 0.0f;   // 0 = idle, 1 = run
	float OverrideWeight = 0.0f;    // 0 = locomotion only, 1 = override only
	float OverrideTime = 0.0f;      // Playback position of the override
	float OverridePlayRate = 1.0f;

	// Advanced on the worker thread
	float IdleTime = 0.0f;
	float RunTime = 0.0f;
};

UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersSimpleAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// Locomotion loops (set in the character constructor or Blueprint)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimSequenceBase* IdleAnim = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimSequenceBase* RunAnim = nullptr;

	// Speed (uu/s) at which the run animation is fully blended in
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float RunAtSpeed = 300.0f;

	// Plays a one-shot sequence over the whole body (attacks, abilities, death).
	// bHoldLastFrame keeps the final pose instead of returning to locomotion (death).
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void PlayFullBodyAnim(UAnimSequenceBase* Anim, float PlayRate = 1.0f, bool bHoldLastFrame = false);

	UFUNCTION(BlueprintCallable, Category = "Animation")
	void StopFullBodyAnim();

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// Active override state (game thread)
	UPROPERTY(Transient)
	UAnimSequenceBase* ActiveOverride = nullptr;
	float OverrideElapsed = 0.0f;
	float ActivePlayRate = 1.0f;
	bool bHoldOverrideLastFrame = false;
	float OverrideBlendWeight = 0.0f;

	friend struct FSkylandersSimpleAnimProxy;
};

// Skylanders Conquest - Code-driven AnimInstance Implementation

#include "SkylandersSimpleAnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "GameFramework/Pawn.h"

// ========== Game thread ==========

FAnimInstanceProxy* USkylandersSimpleAnimInstance::CreateAnimInstanceProxy()
{
	return new FSkylandersSimpleAnimProxy(this);
}

void USkylandersSimpleAnimInstance::PlayFullBodyAnim(UAnimSequenceBase* Anim, float PlayRate, bool bHoldLastFrame)
{
	if (!Anim) return;
	ActiveOverride = Anim;
	OverrideElapsed = 0.0f;
	ActivePlayRate = FMath::Max(PlayRate, 0.01f);
	bHoldOverrideLastFrame = bHoldLastFrame;
}

void USkylandersSimpleAnimInstance::StopFullBodyAnim()
{
	ActiveOverride = nullptr;
	bHoldOverrideLastFrame = false;
}

void USkylandersSimpleAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Advance / expire the override
	float TargetOverrideWeight = 0.0f;
	if (ActiveOverride)
	{
		OverrideElapsed += DeltaSeconds * ActivePlayRate;
		const float Length = ActiveOverride->GetPlayLength();
		if (OverrideElapsed >= Length)
		{
			if (bHoldOverrideLastFrame)
			{
				OverrideElapsed = Length - 0.001f; // Freeze on the final pose (death)
			}
			else
			{
				ActiveOverride = nullptr;
			}
		}
		if (ActiveOverride) TargetOverrideWeight = 1.0f;
	}

	// Quick blend in/out so overrides don't pop (~0.1s)
	OverrideBlendWeight = FMath::FInterpConstantTo(OverrideBlendWeight, TargetOverrideWeight, DeltaSeconds, 10.0f);

	// Locomotion alpha from pawn speed
	float Speed = 0.0f;
	if (APawn* Pawn = TryGetPawnOwner())
	{
		Speed = Pawn->GetVelocity().Size2D();
	}
	const float Alpha = (RunAtSpeed > 1.0f) ? FMath::Clamp(Speed / RunAtSpeed, 0.0f, 1.0f) : 0.0f;

	// Push state to the proxy
	FSkylandersSimpleAnimProxy& Proxy = GetProxyOnGameThread<FSkylandersSimpleAnimProxy>();
	Proxy.IdleSequence = IdleAnim;
	Proxy.RunSequence = RunAnim;
	Proxy.OverrideSequence = ActiveOverride;
	Proxy.LocomotionAlpha = Alpha;
	Proxy.OverrideWeight = OverrideBlendWeight;
	Proxy.OverrideTime = OverrideElapsed;
	Proxy.OverridePlayRate = ActivePlayRate;
}

// ========== Worker thread (proxy) ==========

void FSkylandersSimpleAnimProxy::UpdateAnimationNode(const FAnimationUpdateContext& InContext)
{
	const float Dt = InContext.GetDeltaTime();
	IdleTime += Dt;
	RunTime += Dt;
}

// The ripped animations bake world travel into the skeleton — sometimes on the
// ROOT bone, sometimes on the PELVIS one level below (Tree Rex's stampede anims
// fly the pelvis hundreds of units). Played raw the mesh drifts away from its
// capsule. Pin both bones' translations to the ANIMATION'S OWN FIRST FRAME:
// baked travel is removed while each pose keeps its authored ground height.
// (Pinning to the reference pose instead lifted characters into the air —
// these meshes pivot at their center, so the ref-pose root floats high.)
using FFirstFramePin = FSkylandersSimpleAnimProxy::FFirstFramePin;

static const FFirstFramePin& GetFirstFramePin(UAnimSequenceBase* Sequence, FPoseContext& Scratch,
	TMap<const UAnimSequenceBase*, FFirstFramePin>& Cache)
{
	if (const FFirstFramePin* Found = Cache.Find(Sequence))
	{
		return *Found;
	}
	FPoseContext FirstFrame(Scratch);
	FAnimationPoseData PoseData(FirstFrame);
	FAnimExtractContext Extract(0.0, false);
	Sequence->GetAnimationPose(PoseData, Extract);

	FFirstFramePin Pin;
	if (FirstFrame.Pose.GetNumBones() > 0)
	{
		Pin.Root = FirstFrame.Pose[FCompactPoseBoneIndex(0)].GetTranslation();
	}
	if (FirstFrame.Pose.GetNumBones() > 1)
	{
		Pin.Pelvis = FirstFrame.Pose[FCompactPoseBoneIndex(1)].GetTranslation();
		Pin.bHasPelvis = true;
	}
	return Cache.Add(Sequence, Pin);
}

static void LockRootBone(UAnimSequenceBase* Sequence, FPoseContext& Pose,
	TMap<const UAnimSequenceBase*, FFirstFramePin>& Cache)
{
	if (Pose.Pose.GetNumBones() <= 0) return;
	const FFirstFramePin& Pin = GetFirstFramePin(Sequence, Pose, Cache);

	// Root: centered on the capsule horizontally, first-frame height (charge
	// anims can even START displaced, so the anim's X/Y can't be trusted)
	const FCompactPoseBoneIndex Root(0);
	FTransform RootXf = Pose.Pose[Root];
	RootXf.SetTranslation(FVector(0.0f, 0.0f, Pin.Root.Z));
	Pose.Pose[Root] = RootXf;

	// Pelvis: hold the first-frame offset so travel baked one level down
	// can't carry the body away either
	if (Pin.bHasPelvis && Pose.Pose.GetNumBones() > 1)
	{
		const FCompactPoseBoneIndex Pelvis(1);
		FTransform PelvisXf = Pose.Pose[Pelvis];
		PelvisXf.SetTranslation(Pin.Pelvis);
		Pose.Pose[Pelvis] = PelvisXf;
	}
}

// Samples one looping sequence at the given accumulated time
static void SampleLooping(UAnimSequenceBase* Sequence, float Time, FPoseContext& Output,
	TMap<const UAnimSequenceBase*, FFirstFramePin>& Cache)
{
	const float Length = Sequence->GetPlayLength();
	const float Wrapped = (Length > 0.0f) ? FMath::Fmod(Time, Length) : 0.0f;
	FAnimationPoseData PoseData(Output);
	FAnimExtractContext Extract(static_cast<double>(Wrapped), false);
	Sequence->GetAnimationPose(PoseData, Extract);
	LockRootBone(Sequence, Output, Cache);
}

bool FSkylandersSimpleAnimProxy::Evaluate(FPoseContext& Output)
{
	const bool bHasIdle = IdleSequence != nullptr;
	const bool bHasRun = RunSequence != nullptr;

	if (!bHasIdle && !bHasRun && !OverrideSequence)
	{
		Output.ResetToRefPose();
		return true;
	}

	// --- Locomotion pose (idle/run blend) ---
	FPoseContext Locomotion(Output);
	if (bHasIdle && bHasRun && LocomotionAlpha > KINDA_SMALL_NUMBER && LocomotionAlpha < 1.0f - KINDA_SMALL_NUMBER)
	{
		FPoseContext IdlePose(Output);
		FPoseContext RunPose(Output);
		SampleLooping(IdleSequence, IdleTime, IdlePose, RootFirstFrameCache);
		SampleLooping(RunSequence, RunTime, RunPose, RootFirstFrameCache);

		const FAnimationPoseData IdleData(IdlePose);
		const FAnimationPoseData RunData(RunPose);
		FAnimationPoseData BlendedData(Locomotion);
		FAnimationRuntime::BlendTwoPosesTogether(IdleData, RunData, 1.0f - LocomotionAlpha, BlendedData);
	}
	else if (bHasRun && LocomotionAlpha >= 1.0f - KINDA_SMALL_NUMBER)
	{
		SampleLooping(RunSequence, RunTime, Locomotion, RootFirstFrameCache);
	}
	else if (bHasIdle)
	{
		SampleLooping(IdleSequence, IdleTime, Locomotion, RootFirstFrameCache);
	}
	else
	{
		Locomotion.ResetToRefPose();
	}

	// --- Full-body override (attack/ability/death), blended over locomotion ---
	if (OverrideSequence && OverrideWeight > KINDA_SMALL_NUMBER)
	{
		FPoseContext OverridePose(Output);
		const float Length = OverrideSequence->GetPlayLength();
		const float Time = FMath::Clamp(OverrideTime, 0.0f, FMath::Max(Length - 0.001f, 0.0f));
		FAnimationPoseData OverrideData(OverridePose);
		FAnimExtractContext Extract(static_cast<double>(Time), false);
		OverrideSequence->GetAnimationPose(OverrideData, Extract);
		LockRootBone(OverrideSequence, OverridePose, RootFirstFrameCache);

		if (OverrideWeight >= 1.0f - KINDA_SMALL_NUMBER)
		{
			Output.Pose.CopyBonesFrom(OverridePose.Pose);
			Output.Curve.CopyFrom(OverridePose.Curve);
			Output.CustomAttributes.CopyFrom(OverridePose.CustomAttributes);
		}
		else
		{
			const FAnimationPoseData LocomotionData(Locomotion);
			const FAnimationPoseData OverrideConstData(OverridePose);
			FAnimationPoseData OutData(Output);
			FAnimationRuntime::BlendTwoPosesTogether(OverrideConstData, LocomotionData, OverrideWeight, OutData);
		}
	}
	else
	{
		Output.Pose.CopyBonesFrom(Locomotion.Pose);
		Output.Curve.CopyFrom(Locomotion.Curve);
		Output.CustomAttributes.CopyFrom(Locomotion.CustomAttributes);
	}

	return true;
}

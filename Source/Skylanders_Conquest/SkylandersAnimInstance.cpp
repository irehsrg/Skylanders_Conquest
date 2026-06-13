// Skylanders Conquest - Animation Instance Implementation

#include "SkylandersAnimInstance.h"
#include "SkylandersCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

void USkylandersAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	APawn* Owner = TryGetPawnOwner();
	if (Owner)
	{
		OwnerCharacter = Cast<ASkylandersCharacter>(Owner);
		MovementComponent = Owner->FindComponentByClass<UCharacterMovementComponent>();
	}
}

void USkylandersAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwnerCharacter || !MovementComponent) return;

	// Locomotion
	FVector Velocity = MovementComponent->Velocity;
	Speed = Velocity.Size2D();
	bIsInAir = MovementComponent->IsFalling();

	// Direction relative to character facing (for strafe/backpedal blending)
	if (Speed > 5.0f)
	{
		// Calculate direction relative to facing (manual to avoid AnimGraphRuntime dependency)
		FRotator ActorRotation = OwnerCharacter->GetActorRotation();
		FMatrix RotMatrix = FRotationMatrix(ActorRotation);
		FVector ForwardVector = RotMatrix.GetScaledAxis(EAxis::X);
		FVector RightVector = RotMatrix.GetScaledAxis(EAxis::Y);
		FVector NormalizedVel = Velocity.GetSafeNormal2D();
		float ForwardDot = FVector::DotProduct(ForwardVector, NormalizedVel);
		float RightDot = FVector::DotProduct(RightVector, NormalizedVel);
		Direction = FMath::RadiansToDegrees(FMath::Atan2(RightDot, ForwardDot));
	}
	else
	{
		Direction = 0.0f;
	}

	// Combat states pulled directly from character
	bIsAttacking = OwnerCharacter->AttackSlowRemainingTime > 0.0f;
	bFireFromLeftGun = OwnerCharacter->bFireFromLeftGun;
	bIsMachineGunActive = OwnerCharacter->bMachineGunActive;
	bIsChargingYamato = OwnerCharacter->bChargingYamato;
	bIsDead = OwnerCharacter->CurrentHealth <= 0.0f;

}

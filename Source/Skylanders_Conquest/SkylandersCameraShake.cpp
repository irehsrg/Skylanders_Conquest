// Skylanders Conquest - Camera shakes implementation

#include "SkylandersCameraShake.h"
#include "SkylandersSmokeTest.h"
#include "Shakes/WaveOscillatorCameraShakePattern.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

static void ConfigureShake(UWaveOscillatorCameraShakePattern* P, float Duration, float BlendIn, float BlendOut,
	float RotAmplitude, float LocZAmplitude, float Frequency)
{
	P->Duration = Duration;
	P->BlendInTime = BlendIn;
	P->BlendOutTime = BlendOut;
	P->Pitch.Amplitude = RotAmplitude;
	P->Pitch.Frequency = Frequency;
	// Slightly detuned yaw so the motion is not a straight diagonal line.
	P->Yaw.Amplitude = RotAmplitude;
	P->Yaw.Frequency = Frequency * 0.9f;
	P->Roll.Amplitude = 0.0f;
	P->X.Amplitude = 0.0f;
	P->Y.Amplitude = 0.0f;
	P->Z.Amplitude = LocZAmplitude;
	P->Z.Frequency = Frequency;
	P->FOV.Amplitude = 0.0f;
}

USkylandersShake_Light::USkylandersShake_Light(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = true; // rapid hits restart one shake instead of stacking
	UWaveOscillatorCameraShakePattern* P = CreateDefaultSubobject<UWaveOscillatorCameraShakePattern>(TEXT("WavePattern"));
	ConfigureShake(P, 0.18f, 0.02f, 0.12f, 0.6f, 2.0f, 22.0f);
	SetRootShakePattern(P);
}

USkylandersShake_Heavy::USkylandersShake_Heavy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = true;
	UWaveOscillatorCameraShakePattern* P = CreateDefaultSubobject<UWaveOscillatorCameraShakePattern>(TEXT("WavePattern"));
	ConfigureShake(P, 0.45f, 0.03f, 0.25f, 2.2f, 8.0f, 14.0f);
	SetRootShakePattern(P);
}

void USkylandersCameraShakeLibrary::Shake(const UObject* WorldContext, bool bHeavy, float Scale)
{
	if (!WorldContext || Scale <= 0.0f) return;
	APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContext, 0);
	if (!PC || !PC->PlayerCameraManager) return;
	if (bHeavy)
	{
		PC->ClientStartCameraShake(USkylandersShake_Heavy::StaticClass(), Scale);
	}
	else
	{
		PC->ClientStartCameraShake(USkylandersShake_Light::StaticClass(), Scale);
	}
	SKY_COUNT("camera_shake");
}

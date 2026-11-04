// Skylanders Conquest - Camera shakes
//
// Two code-defined shakes (no assets) built on the EngineCameras wave
// oscillator pattern, plus a one-call helper that plays one on the local
// player's camera. Light: hits taken and ability casts. Heavy: ultimates and
// death.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SkylandersCameraShake.generated.h"

UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersShake_Light : public UCameraShakeBase
{
	GENERATED_BODY()
public:
	USkylandersShake_Light(const FObjectInitializer& ObjectInitializer);
};

UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersShake_Heavy : public UCameraShakeBase
{
	GENERATED_BODY()
public:
	USkylandersShake_Heavy(const FObjectInitializer& ObjectInitializer);
};

UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersCameraShakeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	// Plays a light or heavy shake on player controller 0. Safe to call with no
	// player controller or camera manager (does nothing).
	UFUNCTION(BlueprintCallable, Category = "Skylanders|Camera", meta = (WorldContext = "WorldContext"))
	static void Shake(const UObject* WorldContext, bool bHeavy, float Scale = 1.0f);
};

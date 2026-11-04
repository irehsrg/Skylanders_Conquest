// Skylanders Conquest - Hit flash
//
// One call makes any damaged actor pop for a few frames: its mesh components
// get an additive overlay material, restored on a timer. Lives on a component
// that is added to the actor on first use so each actor owns its own timer and
// saved overlays. The aim highlight also uses overlays; a flash that ends only
// restores what it replaced, so it never clears a highlight applied mid-flash.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SkylandersHitFlash.generated.h"

class UMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersHitFlashComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Flash Target's meshes. Color is HDR (values above 1 glow). Default is a
	// hot white; a critical hit can pass a warmer color.
	static void Flash(AActor* Target, FLinearColor Color = FLinearColor(3.0f, 2.6f, 2.2f, 1.0f), float Duration = 0.09f);

private:
	void Begin(FLinearColor Color, float Duration);
	void End();

	UPROPERTY()
	UMaterialInstanceDynamic* FlashMID = nullptr;

	TMap<TWeakObjectPtr<UMeshComponent>, TWeakObjectPtr<UMaterialInterface>> Saved;
	FTimerHandle Handle;
	bool bFlashing = false;
};

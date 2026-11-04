// Skylanders Conquest - Hit flash implementation

#include "SkylandersHitFlash.h"
#include "SkylandersSmokeTest.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "Engine/World.h"

static UMaterialInterface* LoadFlashBase()
{
	// M_HitFlash (parameterised color) if the project has it, else the aim
	// highlight material, which is the same additive fresnel in fixed cyan.
	static TWeakObjectPtr<UMaterialInterface> Cached;
	if (Cached.IsValid()) return Cached.Get();
	UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/VFX/M_HitFlash.M_HitFlash"));
	if (!M) M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/VFX/M_Highlight.M_Highlight"));
	Cached = M;
	return M;
}

void USkylandersHitFlashComponent::Flash(AActor* Target, FLinearColor Color, float Duration)
{
	if (!Target || Target->IsActorBeingDestroyed() || Target->IsHidden()) return;
	USkylandersHitFlashComponent* Comp = Target->FindComponentByClass<USkylandersHitFlashComponent>();
	if (!Comp)
	{
		Comp = NewObject<USkylandersHitFlashComponent>(Target, TEXT("HitFlash"));
		Comp->RegisterComponent();
	}
	Comp->Begin(Color, Duration);
}

void USkylandersHitFlashComponent::Begin(FLinearColor Color, float Duration)
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner) return;

	if (!FlashMID)
	{
		if (UMaterialInterface* Base = LoadFlashBase())
		{
			FlashMID = UMaterialInstanceDynamic::Create(Base, this);
		}
		if (!FlashMID) return;
	}
	FlashMID->SetVectorParameterValue(FName("Color"), Color);

	if (!bFlashing)
	{
		Saved.Reset();
		TInlineComponentArray<UMeshComponent*> Meshes;
		Owner->GetComponents<UMeshComponent>(Meshes);
		for (UMeshComponent* Mesh : Meshes)
		{
			// Skip ground rings, targeters and other decorations: only the body pops.
			const FString N = Mesh->GetName();
			if (!Mesh->IsVisible() || N.Contains(TEXT("Ring")) || N.Contains(TEXT("Indicator")) || N.Contains(TEXT("Targeter")) || N.Contains(TEXT("Line")))
			{
				continue;
			}
			Saved.Add(Mesh, Mesh->GetOverlayMaterial());
			Mesh->SetOverlayMaterial(FlashMID);
		}
		bFlashing = true;
		SKY_COUNT("hit_flash");
	}
	World->GetTimerManager().SetTimer(Handle, this, &USkylandersHitFlashComponent::End, Duration, false);
}

void USkylandersHitFlashComponent::End()
{
	for (auto& Pair : Saved)
	{
		UMeshComponent* Mesh = Pair.Key.Get();
		if (Mesh && Mesh->GetOverlayMaterial() == FlashMID)
		{
			Mesh->SetOverlayMaterial(Pair.Value.Get());
		}
	}
	Saved.Reset();
	bFlashing = false;
}

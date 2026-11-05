// Skylanders Conquest - shared helpers for the code-built structure visuals
// (towers, phoenixes, titans, spawn fountains). Header-only on purpose: these
// are a handful of inline layout helpers, not a system.

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace SkyStructVis
{
	static const TCHAR* CylinderPath = TEXT("/Engine/BasicShapes/Cylinder");
	static const TCHAR* ConePath     = TEXT("/Engine/BasicShapes/Cone");
	static const TCHAR* SpherePath   = TEXT("/Engine/BasicShapes/Sphere");
	static const TCHAR* CubePath     = TEXT("/Engine/BasicShapes/Cube");

	// Team palette shared by every structure so blue/red read the same everywhere.
	inline FLinearColor TeamColor(bool bBlue)
	{
		return bBlue ? FLinearColor(0.2f, 0.5f, 1.0f) : FLinearColor(1.0f, 0.25f, 0.2f);
	}

	// Stone with a faint team cast: the silhouette carries the shape, the trim and
	// crystal carry the team, so the bulk of the structure stays neutral.
	inline FLinearColor StoneColor(bool bBlue)
	{
		return bBlue ? FLinearColor(0.30f, 0.33f, 0.40f) : FLinearColor(0.40f, 0.31f, 0.29f);
	}

	// Lit, tintable ("Color" param) material for stone and trim.
	inline UMaterialInstanceDynamic* MakeLitMID(UObject* Outer, const FLinearColor& Color)
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
		UMaterialInstanceDynamic* MID = Base ? UMaterialInstanceDynamic::Create(Base, Outer) : nullptr;
		if (MID) MID->SetVectorParameterValue(FName("Color"), Color);
		return MID;
	}

	// Glowing crystal material. M_MapFloor is unlit + opaque with a "Color"
	// param, so an HDR color (>1) renders as a flat bright glow that blooms and
	// stays readable in shadow. BasicShapeMaterial has no emissive and M_HitFlash
	// is an additive fresnel (only the rim shows), so neither reads as a solid gem.
	inline UMaterialInstanceDynamic* MakeGlowMID(UObject* Outer, const FLinearColor& Color)
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/VFX/M_MapFloor.M_MapFloor"));
		if (!Base) Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
		UMaterialInstanceDynamic* MID = Base ? UMaterialInstanceDynamic::Create(Base, Outer) : nullptr;
		if (MID) MID->SetVectorParameterValue(FName("Color"), Color);
		return MID;
	}

	// Fit a mesh component (whose parent is unscaled and sits at the actor origin)
	// so its mesh spans Z0..Z0+Height with the given XY radius, from the mesh's own
	// bounds -- no assumptions about where an engine shape keeps its pivot.
	// bFlip turns it upside down (a cone becomes a downward point / a chalice).
	inline void Fit(UStaticMeshComponent* C, float Radius, float Z0, float Height,
		bool bFlip = false, const FVector2D& XY = FVector2D::ZeroVector)
	{
		if (!C || !C->GetStaticMesh()) return;
		const FBoxSphereBounds B = C->GetStaticMesh()->GetBounds();
		const FVector Ext = B.BoxExtent;
		const FVector S(Radius / FMath::Max(Ext.X, 1.0f), Radius / FMath::Max(Ext.Y, 1.0f),
			Height / FMath::Max(Ext.Z * 2.0f, 1.0f));
		C->SetRelativeScale3D(S);
		// Pitch 180 maps local (x,y,z) to (-x,y,-z): the old top becomes the bottom.
		C->SetRelativeRotation(bFlip ? FRotator(180.0f, 0.0f, 0.0f) : FRotator::ZeroRotator);
		const float LocalBottom = bFlip ? -(B.Origin.Z + Ext.Z) : (B.Origin.Z - Ext.Z);
		const float OX = bFlip ? B.Origin.X * S.X : -B.Origin.X * S.X;
		C->SetRelativeLocation(FVector(XY.X + OX, XY.Y - B.Origin.Y * S.Y, Z0 - LocalBottom * S.Z));
	}

	// Create a visual-only part in a constructor. Collision off, never touches
	// the navmesh; callers opt specific parts back into collision.
	inline UStaticMeshComponent* MakePart(UObject* Owner, USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Mesh)
	{
		UStaticMeshComponent* C = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Parent);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		C->SetGenerateOverlapEvents(false);
		if (Mesh) C->SetStaticMesh(Mesh);
		return C;
	}
}

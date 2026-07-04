// Skylanders Conquest - Character Select 3D Preview Stage
//
// A small off-screen "photo booth": a skeletal mesh playing its idle animation,
// lit by its own lights, captured by a SceneCapture2D into a render target that
// the character select UI displays. The mesh slowly rotates like SMITE/LoL
// champion previews.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkylandersCharacterPreviewActor.generated.h"

class USkeletalMeshComponent;
class USceneCaptureComponent2D;
class UPointLightComponent;
class UTextureRenderTarget2D;
class UAnimSequenceBase;
class USkeletalMesh;

UCLASS()
class SKYLANDERS_CONQUEST_API ASkylandersCharacterPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	ASkylandersCharacterPreviewActor();

	virtual void Tick(float DeltaTime) override;

	// Swaps the stage to a new character
	void SetPreview(USkeletalMesh* Mesh, UAnimSequenceBase* IdleAnim,
		float MeshScale, float MeshZOffset, float CaptureDistance, float AimHeight);

	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere)
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere)
	USkeletalMeshComponent* PreviewMesh;

	UPROPERTY(VisibleAnywhere)
	USceneCaptureComponent2D* Capture;

	UPROPERTY(VisibleAnywhere)
	UPointLightComponent* KeyLight;

	UPROPERTY(VisibleAnywhere)
	UPointLightComponent* FillLight;

	UPROPERTY(Transient)
	UTextureRenderTarget2D* RenderTarget;

	// Slow turntable spin (degrees/second)
	UPROPERTY(EditAnywhere, Category = "Preview")
	float SpinSpeed = 25.0f;
};

// Skylanders Conquest - Character Select 3D Preview Stage Implementation

#include "SkylandersCharacterPreviewActor.h"
#include "SkylandersSimpleAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/PointLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/SkeletalMesh.h"
#include "Kismet/KismetRenderingLibrary.h"

ASkylandersCharacterPreviewActor::ASkylandersCharacterPreviewActor()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	PreviewMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PreviewMesh"));
	PreviewMesh->SetupAttachment(Root);
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Menu levels have no player nearby; make sure the mesh still animates
	PreviewMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	Capture->SetupAttachment(Root);
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Capture->FOVAngle = 30.0f;
	Capture->bCaptureEveryFrame = true;

	// The menu level has no lighting — the stage brings its own
	KeyLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(Root);
	KeyLight->SetRelativeLocation(FVector(220.0f, -160.0f, 200.0f));
	KeyLight->SetIntensity(30000.0f);
	KeyLight->SetAttenuationRadius(2500.0f);

	FillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(Root);
	FillLight->SetRelativeLocation(FVector(180.0f, 220.0f, 60.0f));
	FillLight->SetIntensity(12000.0f);
	FillLight->SetAttenuationRadius(2500.0f);
	FillLight->SetLightColor(FLinearColor(0.6f, 0.7f, 1.0f)); // Cool fill
}

void ASkylandersCharacterPreviewActor::BeginPlay()
{
	Super::BeginPlay();

	RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, 768, 1024, RTF_RGBA8);
	Capture->TextureTarget = RenderTarget;
	Capture->ShowOnlyComponents.Add(PreviewMesh);
}

void ASkylandersCharacterPreviewActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (PreviewMesh)
	{
		PreviewMesh->AddLocalRotation(FRotator(0.0f, SpinSpeed * DeltaTime, 0.0f));
	}
}

void ASkylandersCharacterPreviewActor::SetPreview(USkeletalMesh* Mesh, UAnimSequenceBase* IdleAnim,
	float MeshScale, float MeshZOffset, float CaptureDistance, float AimHeight)
{
	if (!PreviewMesh || !Mesh) return;

	PreviewMesh->SetSkeletalMesh(Mesh);
	PreviewMesh->SetRelativeLocation(FVector(0.0f, 0.0f, MeshZOffset));
	PreviewMesh->SetRelativeScale3D(FVector(MeshScale));
	PreviewMesh->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f)); // Start facing the camera

	// Code-driven idle playback (works on any skeleton, no AnimBP required)
	PreviewMesh->SetAnimInstanceClass(USkylandersSimpleAnimInstance::StaticClass());
	if (USkylandersSimpleAnimInstance* Simple = Cast<USkylandersSimpleAnimInstance>(PreviewMesh->GetAnimInstance()))
	{
		Simple->IdleAnim = IdleAnim;
		Simple->RunAnim = nullptr;
	}

	// Frame the character: camera in front, aimed at its vertical middle
	Capture->SetRelativeLocation(FVector(CaptureDistance, 0.0f, AimHeight));
	Capture->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));

	// Scale the lights with the subject so big characters aren't dim or blown out
	if (KeyLight)
	{
		KeyLight->SetRelativeLocation(FVector(CaptureDistance * 0.7f, -CaptureDistance * 0.45f, AimHeight * 1.6f + 80.0f));
		KeyLight->SetAttenuationRadius(CaptureDistance * 6.0f);
	}
	if (FillLight)
	{
		FillLight->SetRelativeLocation(FVector(CaptureDistance * 0.55f, CaptureDistance * 0.55f, AimHeight));
		FillLight->SetAttenuationRadius(CaptureDistance * 6.0f);
	}
}

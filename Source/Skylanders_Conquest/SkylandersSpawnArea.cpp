// Skylanders Conquest - Spawn Area Implementation

#include "SkylandersSpawnArea.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "SkylandersCharacter.h"
#include "SkylandersEnemyGod.h"
#include "SkylandersMinion.h"
#include "SkylandersTower.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

ASkylandersSpawnArea::ASkylandersSpawnArea()
{
	PrimaryActorTick.bCanEverTick = true;

	SpawnRadius = 500.0f;
	FountainDPS = 500.0f; // Kills most enemies in ~2-4 seconds

	// Create trigger sphere
	SpawnZone = CreateDefaultSubobject<USphereComponent>(TEXT("SpawnZone"));
	SpawnZone->InitSphereRadius(SpawnRadius);
	SpawnZone->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	SpawnZone->SetGenerateOverlapEvents(true);
	RootComponent = SpawnZone;

	// Create visual platform (cylinder)
	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
	PlatformMesh->SetupAttachment(RootComponent);
	PlatformMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlatformMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -5.0f)); // Slightly below ground

	// Try to use engine cylinder mesh
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
		TEXT("/Engine/BasicShapes/Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		PlatformMesh->SetStaticMesh(CylinderMesh.Object);
		// Scale: cylinder is 100 units radius by default, scale to match spawn radius
		// Height very flat (disc), radius matches zone
		float RadiusScale = SpawnRadius / 50.0f; // Cylinder default radius is 50
		PlatformMesh->SetRelativeScale3D(FVector(RadiusScale, RadiusScale, 0.05f));
	}

	// Set a blue-ish translucent material color
	PlatformMesh->SetRenderCustomDepth(true);
}

void ASkylandersSpawnArea::BeginPlay()
{
	Super::BeginPlay();

	// Update sphere radius in case it was changed in editor
	SpawnZone->SetSphereRadius(SpawnRadius);

	// Create a simple translucent blue material at runtime
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
	if (BaseMat && PlatformMesh)
	{
		UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, this);
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(FName("Color"), FLinearColor(0.1f, 0.3f, 0.8f, 0.3f));
			PlatformMesh->SetMaterial(0, DynMat);
		}
	}

	// Bind overlap events
	SpawnZone->OnComponentBeginOverlap.AddDynamic(this, &ASkylandersSpawnArea::OnPlayerEnterSpawn);
	SpawnZone->OnComponentEndOverlap.AddDynamic(this, &ASkylandersSpawnArea::OnPlayerExitSpawn);

	UE_LOG(LogTemp, Log, TEXT("Spawn Area initialized. Radius: %.0f"), SpawnRadius);
}

void ASkylandersSpawnArea::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Fountain damage: hurt all enemies inside spawn area
	TArray<AActor*> OverlappingActors;
	SpawnZone->GetOverlappingActors(OverlappingActors);
	for (AActor* Actor : OverlappingActors)
	{
		ASkylandersEnemyGod* EGod = Cast<ASkylandersEnemyGod>(Actor);
		if (EGod && EGod->CurrentState != EGodAIState::Dead)
		{
			EGod->TakeDamage_Custom(FountainDPS * DeltaTime, this);
		}

		ASkylandersMinion* Minion = Cast<ASkylandersMinion>(Actor);
		if (Minion && Minion->Team == ETowerTeam::Enemy && !Minion->bDead)
		{
			Minion->TakeDamage_Custom(FountainDPS * DeltaTime, this);
		}
	}

	// Draw a cyan circle on the ground around the spawn area
	FVector Center = GetActorLocation();
	Center.Z += 5.0f; // Slightly above ground
	int32 Segments = 64;
	float AngleStep = 2.0f * PI / Segments;
	for (int32 i = 0; i < Segments; i++)
	{
		float A1 = i * AngleStep;
		float A2 = (i + 1) * AngleStep;
		FVector P1 = Center + FVector(FMath::Cos(A1) * SpawnRadius, FMath::Sin(A1) * SpawnRadius, 0.0f);
		FVector P2 = Center + FVector(FMath::Cos(A2) * SpawnRadius, FMath::Sin(A2) * SpawnRadius, 0.0f);
		DrawDebugLine(GetWorld(), P1, P2, FColor::Cyan, false, 0.05f, 1, 3.0f);
	}
}

void ASkylandersSpawnArea::OnPlayerEnterSpawn(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	ASkylandersCharacter* Player = Cast<ASkylandersCharacter>(OtherActor);
	if (Player)
	{
		Player->bIsInSpawnArea = true;
		UE_LOG(LogTemp, Log, TEXT("Player entered spawn area - shopping enabled"));

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("Entered Spawn Area - Press B to Shop"));
		}
	}
}

void ASkylandersSpawnArea::OnPlayerExitSpawn(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ASkylandersCharacter* Player = Cast<ASkylandersCharacter>(OtherActor);
	if (Player)
	{
		Player->bIsInSpawnArea = false;
		UE_LOG(LogTemp, Log, TEXT("Player left spawn area - shopping disabled"));
	}
}

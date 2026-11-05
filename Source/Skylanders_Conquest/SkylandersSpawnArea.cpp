// Skylanders Conquest - Spawn Area Implementation

#include "SkylandersSpawnArea.h"
#include "SkylandersKillFeedWidget.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "SkylandersCharacter.h"
#include "SkylandersEnemyGod.h"
#include "SkylandersMinion.h"
#include "SkylandersTower.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "SkylandersActorRegistry.h"
#include "DrawDebugHelpers.h"
#include "SkylandersStructureVisuals.h"

ASkylandersSpawnArea::ASkylandersSpawnArea()
{
	PrimaryActorTick.bCanEverTick = true;

	SpawnRadius = 500.0f;
	FountainDPS = 500.0f; // Kills most enemies in ~2-4 seconds
	Team = ETowerTeam::Friendly;
	FountainPulseAccumulator = 0.0f;

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

	// Ring posts + caps + center crystal (laid out in BeginPlay; the posts may
	// swap to the ripped round-post mesh there if its bounds are sane)
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere"));
	UStaticMesh* Cyl = CylinderMesh.Succeeded() ? CylinderMesh.Object : nullptr;
	UStaticMesh* Cone = ConeMesh.Succeeded() ? ConeMesh.Object : nullptr;
	UStaticMesh* Sphere = SphereMesh.Succeeded() ? SphereMesh.Object : nullptr;
	static const TCHAR* PostNames[] = { TEXT("Post0"), TEXT("Post1"), TEXT("Post2"), TEXT("Post3"), TEXT("Post4"), TEXT("Post5") };
	static const TCHAR* CapNames[] = { TEXT("PostCap0"), TEXT("PostCap1"), TEXT("PostCap2"), TEXT("PostCap3"), TEXT("PostCap4"), TEXT("PostCap5") };
	for (int32 i = 0; i < 6; i++)
	{
		PostParts.Add(SkyStructVis::MakePart(this, RootComponent, PostNames[i], Cyl));
		PostCaps.Add(SkyStructVis::MakePart(this, RootComponent, CapNames[i], Sphere));
	}
	CrystalPivot = CreateDefaultSubobject<USceneComponent>(TEXT("CrystalPivot"));
	CrystalPivot->SetupAttachment(RootComponent);
	CrystalParts.Add(SkyStructVis::MakePart(this, CrystalPivot, TEXT("CrystalTop"), Cone));
	CrystalParts.Add(SkyStructVis::MakePart(this, CrystalPivot, TEXT("CrystalBottom"), Cone));
	CrystalTime = 0.0f;
}

void ASkylandersSpawnArea::BeginPlay()
{
	Super::BeginPlay();
	if (USkylandersActorRegistry* Registry = USkylandersActorRegistry::Get(this))
	{
		Registry->Register(this);
	}

	// Update sphere radius in case it was changed in editor
	SpawnZone->SetSphereRadius(SpawnRadius);

	// Create a simple translucent team-colored material at runtime
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
	if (BaseMat && PlatformMesh)
	{
		UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, this);
		if (DynMat)
		{
			FLinearColor PlatformColor = (Team == ETowerTeam::Friendly)
				? FLinearColor(0.1f, 0.3f, 0.8f, 0.3f)
				: FLinearColor(0.8f, 0.15f, 0.1f, 0.3f);
			DynMat->SetVectorParameterValue(FName("Color"), PlatformColor);
			PlatformMesh->SetMaterial(0, DynMat);
		}
	}

	LayoutVisuals();

	// Bind overlap events
	SpawnZone->OnComponentBeginOverlap.AddDynamic(this, &ASkylandersSpawnArea::OnPlayerEnterSpawn);
	SpawnZone->OnComponentEndOverlap.AddDynamic(this, &ASkylandersSpawnArea::OnPlayerExitSpawn);

	UE_LOG(LogTemp, Log, TEXT("Spawn Area initialized. Radius: %.0f"), SpawnRadius);
}

void ASkylandersSpawnArea::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Crystal spin + bob (relative transform only)
	CrystalTime += DeltaTime;
	if (CrystalPivot)
	{
		CrystalPivot->SetRelativeLocationAndRotation(
			FVector(0.0f, 0.0f, 260.0f + 12.0f * FMath::Sin(CrystalTime * 1.4f)),
			FRotator(0.0f, FMath::Fmod(CrystalTime * 30.0f, 360.0f), 0.0f));
	}

	// Fountain damage: hurt everything hostile to this base, in discrete pulses
	// (per-frame TakeDamage_Custom would spawn a damage-number actor every frame)
	FountainPulseAccumulator += DeltaTime;
	if (FountainPulseAccumulator >= FountainPulseInterval)
	{
		float PulseDamage = FountainDPS * FountainPulseAccumulator;
		FountainPulseAccumulator = 0.0f;

		TArray<AActor*> OverlappingActors;
		SpawnZone->GetOverlappingActors(OverlappingActors);
		for (AActor* Actor : OverlappingActors)
		{
			if (Team == ETowerTeam::Friendly)
			{
				ASkylandersEnemyGod* EGod = Cast<ASkylandersEnemyGod>(Actor);
				if (EGod && EGod->CurrentState != EGodAIState::Dead)
				{
					EGod->TakeDamage_Custom(PulseDamage, this);
				}

				ASkylandersMinion* Minion = Cast<ASkylandersMinion>(Actor);
				if (Minion && Minion->Team == ETowerTeam::Enemy && !Minion->bDead)
				{
					Minion->TakeDamage_Custom(PulseDamage, this);
				}
			}
			else
			{
				// Enemy base fountain burns the player and friendly minions
				ASkylandersCharacter* Player = Cast<ASkylandersCharacter>(Actor);
				if (Player && Player->CurrentHealth > 0.0f)
				{
					Player->TakeDamage_Custom(PulseDamage, this);
				}

				ASkylandersMinion* Minion = Cast<ASkylandersMinion>(Actor);
				if (Minion && Minion->Team == ETowerTeam::Friendly && !Minion->bDead)
				{
					Minion->TakeDamage_Custom(PulseDamage, this);
				}
			}
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
	// Only the player's own base heals and allows shopping
	if (Team != ETowerTeam::Friendly) return;

	ASkylandersCharacter* Player = Cast<ASkylandersCharacter>(OtherActor);
	if (Player)
	{
		Player->bIsInSpawnArea = true;
		UE_LOG(LogTemp, Log, TEXT("Player entered spawn area - shopping enabled"));

		USkylandersKillFeedWidget::Post(this, TEXT("Entered Spawn Area - Press B to Shop"), FLinearColor(0.4f, 0.85f, 1.0f));
	}
}

void ASkylandersSpawnArea::OnPlayerExitSpawn(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (Team != ETowerTeam::Friendly) return;

	ASkylandersCharacter* Player = Cast<ASkylandersCharacter>(OtherActor);
	if (Player)
	{
		Player->bIsInSpawnArea = false;
		UE_LOG(LogTemp, Log, TEXT("Player left spawn area - shopping disabled"));
	}
}

void ASkylandersSpawnArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USkylandersActorRegistry* Registry = USkylandersActorRegistry::Get(this))
	{
		Registry->Unregister(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ASkylandersSpawnArea::LayoutVisuals()
{
	using namespace SkyStructVis;
	const bool bBlue = (Team == ETowerTeam::Friendly);
	// The root is the (unscaled) trigger sphere at floor level, so offsets here
	// are world units; G is the floor relative to the actor.
	const float G = -GetActorLocation().Z;
	const float PostH = 170.0f, PostR = 26.0f;

	// Prefer the ripped level_505 round post, but only if its bounds are sane:
	// plenty of the ripped meshes carry degenerate or astronomical bounds.
	UStaticMesh* RoundPost = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Environment/L505Fixed/l505_roundPost_straight"));
	float PostScale = 0.0f;
	if (RoundPost)
	{
		const FVector Ext = RoundPost->GetBounds().BoxExtent;
		const bool bSane = Ext.Z > 5.0f && Ext.GetMax() < 5000.0f;
		PostScale = bSane ? PostH / (2.0f * Ext.Z) : 0.0f;
		// Too fat once scaled to height = not a post; keep the cylinder
		if (PostScale > 0.0f && FMath::Max(Ext.X, Ext.Y) * PostScale > 70.0f) PostScale = 0.0f;
		UE_LOG(LogTemp, Log, TEXT("SpawnArea post l505_roundPost_straight bounds %s -> %s"),
			*Ext.ToString(), PostScale > 0.0f ? TEXT("used") : TEXT("rejected, cylinder posts"));
	}

	UMaterialInstanceDynamic* Stone = MakeLitMID(this, StoneColor(bBlue));
	UMaterialInstanceDynamic* Glow = MakeGlowMID(this, TeamColor(bBlue) * 3.0f);

	// Six posts just outside the platform edge, rotated 30 deg so none sits on
	// the +/-X axis the base path runs along. Solid, and small enough that the
	// navmesh simply routes around them.
	const float Ring = SpawnRadius + 60.0f;
	for (int32 i = 0; i < PostParts.Num(); i++)
	{
		const float Ang = FMath::DegreesToRadians(30.0f + 60.0f * i);
		const FVector2D XY(FMath::Cos(Ang) * Ring, FMath::Sin(Ang) * Ring);
		UStaticMeshComponent* Post = PostParts[i];
		if (PostScale > 0.0f)
		{
			Post->SetStaticMesh(RoundPost);
			const FBoxSphereBounds B = RoundPost->GetBounds();
			Post->SetRelativeScale3D(FVector(PostScale));
			Post->SetRelativeLocation(FVector(XY.X - B.Origin.X * PostScale, XY.Y - B.Origin.Y * PostScale,
				G - (B.Origin.Z - B.BoxExtent.Z) * PostScale));
		}
		else
		{
			Fit(Post, PostR, G, PostH, false, XY);
		}
		Post->SetCollisionProfileName(TEXT("BlockAllDynamic"));
		Post->SetCanEverAffectNavigation(true);
		for (int32 m = 0; m < Post->GetNumMaterials(); m++)
		{
			if (Stone) Post->SetMaterial(m, Stone);
		}
		Fit(PostCaps[i], 22.0f, G + PostH, 44.0f, false, XY);
		if (Glow) PostCaps[i]->SetMaterial(0, Glow);
	}

	// Center crystal hovering over the platform (Tick spins/bobs the pivot)
	CrystalPivot->SetRelativeLocation(FVector(0.0f, 0.0f, 260.0f));
	Fit(CrystalParts[0], 50.0f, 0.0f, 110.0f);
	Fit(CrystalParts[1], 50.0f, -110.0f, 110.0f, /*bFlip*/ true);
	for (UStaticMeshComponent* C : CrystalParts)
	{
		if (Glow) C->SetMaterial(0, Glow);
	}
}

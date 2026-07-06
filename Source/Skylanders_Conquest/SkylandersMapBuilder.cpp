// Skylanders Conquest - Map Builder Implementation (Joust Layout)

#include "SkylandersMapBuilder.h"
#include "SkylandersTower.h"
#include "SkylandersTitan.h"
#include "SkylandersMinionSpawner.h"
#include "SkylandersBuffCamp.h"
#include "SkylandersSpawnArea.h"
#include "SkylandersEnemyGod.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"

ASkylandersMapBuilder::ASkylandersMapBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	BlueTower = nullptr;
	BluePhoenix = nullptr;
	BlueTitan = nullptr;
	BlueSpawner = nullptr;
	BlueSpawnArea = nullptr;
	RedTower = nullptr;
	RedPhoenix = nullptr;
	RedTitan = nullptr;
	RedSpawner = nullptr;
	RedSpawnArea = nullptr;
	BullDemonKing = nullptr;
	DamageCamp = nullptr;
	MidCamp = nullptr;
	BlueBlueBuff = nullptr;
	RedBlueBuff = nullptr;
}

void ASkylandersMapBuilder::BeginPlay()
{
	Super::BeginPlay();
	BuildMap();
}

void ASkylandersMapBuilder::BuildMap()
{
	UWorld* World = GetWorld();
	if (!World) return;

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UE_LOG(LogTemp, Log, TEXT("=== Building Joust V2 Map ==="));

	// Palette (unlit flat colors). ONE solid ground colour — the old multi-color
	// lane/jungle/base pieces overlapped coplanar and z-fought (clipping/flicker).
	const FLinearColor Ground(0.16f, 0.36f, 0.16f);     // single solid grass
	const FLinearColor WallCol(0.06f, 0.07f, 0.10f);    // terrain walls (dark)

	// ========================================================================
	// LANE CENTERLINE (blue -X -> red +X), a point-symmetric serpentine, scaled
	// up ~1.8x vs the first pass so lane traversal matches SMITE Joust (~18s).
	// Used only for the minion waypoint paths now (the ground is one flat color).
	// ========================================================================
	TArray<FVector> LanePath;
	LanePath.Add(FVector(-8300, 0, 0));      // blue base mouth
	LanePath.Add(FVector(-6100, 1000, 0));
	LanePath.Add(FVector(-3400, -650, 0));
	LanePath.Add(FVector(0, 0, 0));          // mid
	LanePath.Add(FVector(3400, 650, 0));
	LanePath.Add(FVector(6100, -1000, 0));
	LanePath.Add(FVector(8300, 0, 0));       // red base mouth

	// ========================================================================
	// FLOOR — ONE big solid plane (top at Z=0). No overlapping pieces, so no
	// z-fighting. Walls define the bounds; the map shape reads from structure
	// and camp placement + the serpentine minion lane.
	// ========================================================================
	SpawnFloorSeg(FVector(-13500, 0, 0), FVector(13500, 0, 0), 10500.0f, 0.0f, Ground);

	// Perimeter wall ring so the map is fully enclosed (bigger + taller now)
	const float WH = 600.0f, WT = 200.0f;
	const float PX = 12600.0f, PY = 4700.0f;
	SpawnWallSeg(FVector(-PX, -PY, 0), FVector(PX, -PY, 0), WH, WT, WallCol);
	SpawnWallSeg(FVector(-PX, PY, 0), FVector(PX, PY, 0), WH, WT, WallCol);
	SpawnWallSeg(FVector(-PX, -PY, 0), FVector(-PX, PY, 0), WH, WT, WallCol);
	SpawnWallSeg(FVector(PX, -PY, 0), FVector(PX, PY, 0), WH, WT, WallCol);

	// ========================================================================
	// STRUCTURES — deferred so Team/bIsPhoenix are set before BeginPlay
	// ========================================================================
	auto SpawnStructureTower = [&](FVector Loc, ETowerTeam Team, const TCHAR* Name,
		bool bPhoenix, float Dmg, float Range) -> ASkylandersTower*
	{
		FTransform T(FRotator::ZeroRotator, Loc);
		ASkylandersTower* Tw = World->SpawnActorDeferred<ASkylandersTower>(
			ASkylandersTower::StaticClass(), T, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Tw)
		{
			Tw->Team = Team;
			Tw->TowerName = Name;
			Tw->bIsPhoenix = bPhoenix;
			Tw->AttackDamage = Dmg;
			if (bPhoenix)
			{
				Tw->MaxHealth = 2500.0f;
				Tw->CurrentHealth = 2500.0f;
				Tw->AttackRange = Range;
				Tw->PhoenixRespawnTime = 180.0f;
			}
			Tw->FinishSpawning(T);
		}
		return Tw;
	};

	auto SpawnTitanFor = [&](FVector Loc, ETowerTeam Team, const TCHAR* Name) -> ASkylandersTitan*
	{
		FTransform T(FRotator::ZeroRotator, Loc);
		ASkylandersTitan* Ti = World->SpawnActorDeferred<ASkylandersTitan>(
			ASkylandersTitan::StaticClass(), T, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Ti)
		{
			Ti->Team = Team;
			Ti->TitanName = Name;
			Ti->AttackDamage = 120.0f;
			Ti->FinishSpawning(T);
		}
		return Ti;
	};

	auto SpawnAreaFor = [&](FVector Loc, ETowerTeam Team) -> ASkylandersSpawnArea*
	{
		FTransform T(FRotator::ZeroRotator, Loc);
		ASkylandersSpawnArea* A = World->SpawnActorDeferred<ASkylandersSpawnArea>(
			ASkylandersSpawnArea::StaticClass(), T, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (A) { A->Team = Team; A->FinishSpawning(T); }
		return A;
	};

	// Blue (negative X): Spawn > Titan > Phoenix > Tower(on lane). Generous
	// spacing so the meshes don't overlap; spawn pulled well back from the Titan.
	BlueSpawnArea = SpawnAreaFor(FVector(-11500, 0, 0), ETowerTeam::Friendly);
	BlueTitan = SpawnTitanFor(FVector(-9500, 0, 150), ETowerTeam::Friendly, TEXT("Blue Titan"));
	BluePhoenix = SpawnStructureTower(FVector(-7000, 0, 15), ETowerTeam::Friendly, TEXT("Blue Phoenix"), true, 100.0f, 1000.0f);
	BlueTower = SpawnStructureTower(FVector(-3800, 300, 15), ETowerTeam::Friendly, TEXT("Blue Tower"), false, 50.0f, 0.0f);

	// Red (positive X): mirror
	RedSpawnArea = SpawnAreaFor(FVector(11500, 0, 0), ETowerTeam::Enemy);
	RedTitan = SpawnTitanFor(FVector(9500, 0, 150), ETowerTeam::Enemy, TEXT("Red Titan"));
	RedPhoenix = SpawnStructureTower(FVector(7000, 0, 15), ETowerTeam::Enemy, TEXT("Red Phoenix"), true, 100.0f, 1000.0f);
	RedTower = SpawnStructureTower(FVector(3800, -300, 15), ETowerTeam::Enemy, TEXT("Red Tower"), false, 50.0f, 0.0f);

	// Minion spawners + lane waypoint paths (spawn -> enemy base)
	BlueSpawner = World->SpawnActor<ASkylandersMinionSpawner>(
		ASkylandersMinionSpawner::StaticClass(), FVector(-8300, 0, 0), FRotator::ZeroRotator, SP);
	if (BlueSpawner)
	{
		BlueSpawner->Team = ETowerTeam::Friendly;
		BlueSpawner->LaneTargetPoint = FVector(9500, 0, 0);
		for (int32 i = 1; i < LanePath.Num(); i++) BlueSpawner->LaneWaypoints.Add(LanePath[i]);
		BlueSpawner->LaneWaypoints.Add(FVector(9500, 0, 0));
	}

	RedSpawner = World->SpawnActor<ASkylandersMinionSpawner>(
		ASkylandersMinionSpawner::StaticClass(), FVector(8300, 0, 0), FRotator::ZeroRotator, SP);
	if (RedSpawner)
	{
		RedSpawner->Team = ETowerTeam::Enemy;
		RedSpawner->LaneTargetPoint = FVector(-9500, 0, 0);
		for (int32 i = LanePath.Num() - 2; i >= 0; i--) RedSpawner->LaneWaypoints.Add(LanePath[i]);
		RedSpawner->LaneWaypoints.Add(FVector(-9500, 0, 0));
	}

	// Protection chains: Tower -> Phoenix -> Titan
	if (BluePhoenix) BluePhoenix->ProtectingTower = BlueTower;
	if (BlueTitan) BlueTitan->ProtectingTower = BluePhoenix;
	if (RedPhoenix) RedPhoenix->ProtectingTower = RedTower;
	if (RedTitan) RedTitan->ProtectingTower = RedPhoenix;

	// ========================================================================
	// JUNGLE CAMPS (positioned to match the grass pockets above)
	// ========================================================================
	auto SpawnCamp = [&](FVector Loc, const TCHAR* Name, EBuffType Buff, float Mult,
		float HP, float XP, int32 Coins) -> ASkylandersBuffCamp*
	{
		FTransform T(FRotator::ZeroRotator, Loc);
		ASkylandersBuffCamp* C = World->SpawnActorDeferred<ASkylandersBuffCamp>(
			ASkylandersBuffCamp::StaticClass(), T, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (C)
		{
			C->CampName = Name;
			C->BuffType = Buff;
			C->BuffDamageMultiplier = Mult;
			C->BuffDuration = 90.0f;
			if (HP > 0.0f) { C->MaxHealth = HP; C->CurrentHealth = HP; }
			if (XP > 0.0f) C->XPReward = XP;
			if (Coins > 0) C->CoinReward = Coins;
			C->FinishSpawning(T);
		}
		return C;
	};

	BlueBlueBuff = SpawnCamp(FVector(-5800, 2800, 75), TEXT("Blue Buff"), EBuffType::Mana, 1.0f, 0.0f, 0.0f, 0);
	RedBlueBuff = SpawnCamp(FVector(5800, -2800, 75), TEXT("Blue Buff"), EBuffType::Mana, 1.0f, 0.0f, 0.0f, 0);
	DamageCamp = SpawnCamp(FVector(-2700, -3000, 75), TEXT("Damage Buff"), EBuffType::Damage, 1.25f, 0.0f, 0.0f, 0);
	SpawnCamp(FVector(2700, 3000, 75), TEXT("Damage Buff"), EBuffType::Damage, 1.25f, 0.0f, 0.0f, 0);
	BullDemonKing = SpawnCamp(FVector(0, 3700, 75), TEXT("Bull Demon King"), EBuffType::Damage, 1.50f, 2500.0f, 200.0f, 150);
	MidCamp = SpawnCamp(FVector(0, -3700, 75), TEXT("Mid Harpies"), EBuffType::None, 1.0f, 300.0f, 80.0f, 40);

	// ========================================================================
	// ENEMY GOD (parks in the red jungle)
	// ========================================================================
	World->SpawnActor<ASkylandersEnemyGod>(
		ASkylandersEnemyGod::StaticClass(), FVector(6800, -500, 100), FRotator::ZeroRotator, SP);

	UE_LOG(LogTemp, Log, TEXT("=== Joust V2 Map Complete ==="));
}

AActor* ASkylandersMapBuilder::SpawnWall(FVector Location, FVector Scale, FRotator Rotation)
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* Wall = World->SpawnActor<AActor>(AActor::StaticClass(), Location, Rotation, Params);
	if (Wall)
	{
		UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(Wall);
		Wall->SetRootComponent(MeshComp);
		MeshComp->RegisterComponent();

		UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube"));
		if (CubeMesh)
		{
			MeshComp->SetStaticMesh(CubeMesh);
		}

		MeshComp->SetWorldScale3D(Scale);
		MeshComp->SetCollisionProfileName(TEXT("BlockAllDynamic"));

		UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
		if (BaseMat)
		{
			UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, Wall);
			if (DynMat)
			{
				DynMat->SetVectorParameterValue(FName("Color"), FLinearColor(0.15f, 0.15f, 0.2f));
				MeshComp->SetMaterial(0, DynMat);
			}
		}

#if WITH_EDITOR
		Wall->SetActorLabel(TEXT("MapWall"));
#endif
	}

	return Wall;
}

AActor* ASkylandersMapBuilder::SpawnGround(FVector Location, FVector Scale, FLinearColor Color)
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* Ground = World->SpawnActor<AActor>(AActor::StaticClass(), Location, FRotator::ZeroRotator, Params);
	if (Ground)
	{
		UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(Ground);
		Ground->SetRootComponent(MeshComp);
		MeshComp->RegisterComponent();

		UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube"));
		if (CubeMesh)
		{
			MeshComp->SetStaticMesh(CubeMesh);
		}

		MeshComp->SetWorldScale3D(Scale);
		MeshComp->SetCollisionProfileName(TEXT("BlockAllDynamic"));

		UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
		if (BaseMat)
		{
			UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, Ground);
			if (DynMat)
			{
				DynMat->SetVectorParameterValue(FName("Color"), Color);
				MeshComp->SetMaterial(0, DynMat);
			}
		}

#if WITH_EDITOR
		Ground->SetActorLabel(TEXT("MapGround"));
#endif
	}

	return Ground;
}

// Shared: spawn a colored primitive actor (cube or cylinder) with a transform
static AActor* SpawnColoredPrimitive(UWorld* World, const TCHAR* MeshPath,
	const FVector& Loc, const FRotator& Rot, const FVector& Scale, const FLinearColor& Color,
	const TCHAR* Label)
{
	if (!World) return nullptr;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* A = World->SpawnActor<AActor>(AActor::StaticClass(), Loc, Rot, Params);
	if (!A) return nullptr;

	UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(A);
	A->SetRootComponent(MeshComp);
	MeshComp->RegisterComponent();
	if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath))
	{
		MeshComp->SetStaticMesh(Mesh);
	}
	// Assigning a fresh root AFTER SpawnActor discards the spawn transform, so
	// re-apply location/rotation explicitly (otherwise every piece stacks at the
	// origin), then scale.
	A->SetActorLocationAndRotation(Loc, Rot);
	MeshComp->SetWorldScale3D(Scale);
	MeshComp->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	// Prefer the unlit flat-color map material (exact colors regardless of light);
	// fall back to the engine lit material if it hasn't been generated yet.
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/VFX/M_MapFloor.M_MapFloor"));
	if (!BaseMat) BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
	if (BaseMat)
	{
		UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, A);
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(FName("Color"), Color);
			MeshComp->SetMaterial(0, DynMat);
		}
	}

#if WITH_EDITOR
	A->SetActorLabel(Label);
#endif
	return A;
}

// Flat grass disc. z = desired TOP surface; the 20-unit-thick disc sits just below it.
AActor* ASkylandersMapBuilder::SpawnFloorDisc(FVector Center, float Radius, float Z, FLinearColor Color)
{
	// Cylinder base mesh: radius 50, height 100 -> scale XY = R/50, Z = 0.2 (20 tall)
	FVector Loc(Center.X, Center.Y, Z - 10.0f);
	FVector Scale(Radius / 50.0f, Radius / 50.0f, 0.2f);
	return SpawnColoredPrimitive(GetWorld(), TEXT("/Engine/BasicShapes/Cylinder"),
		Loc, FRotator::ZeroRotator, Scale, Color, TEXT("MapFloorDisc"));
}

// Flat grass ribbon between two points (a wide, thin rotated cube).
AActor* ASkylandersMapBuilder::SpawnFloorSeg(FVector From, FVector To, float Width, float Z, FLinearColor Color)
{
	FVector Delta = To - From; Delta.Z = 0.0f;
	float Length = Delta.Size();
	if (Length < 1.0f) return nullptr;
	FVector Mid = (From + To) * 0.5f; Mid.Z = Z - 10.0f;
	FRotator Rot = Delta.Rotation(); // yaw points along the segment (cube +X axis)
	// Cube base mesh is 100 units -> scale X = length/100, Y = width/100, Z = 0.2 (20 tall)
	FVector Scale(Length / 100.0f, Width / 100.0f, 0.2f);
	return SpawnColoredPrimitive(GetWorld(), TEXT("/Engine/BasicShapes/Cube"),
		Mid, Rot, Scale, Color, TEXT("MapFloorSeg"));
}

// Vertical wall between two points, sitting on the floor (base at Z=0).
AActor* ASkylandersMapBuilder::SpawnWallSeg(FVector From, FVector To, float Height, float Thickness, FLinearColor Color)
{
	FVector Delta = To - From; Delta.Z = 0.0f;
	float Length = Delta.Size();
	if (Length < 1.0f) return nullptr;
	FVector Mid = (From + To) * 0.5f; Mid.Z = Height * 0.5f;
	FRotator Rot = Delta.Rotation();
	FVector Scale(Length / 100.0f, Thickness / 100.0f, Height / 100.0f);
	return SpawnColoredPrimitive(GetWorld(), TEXT("/Engine/BasicShapes/Cube"),
		Mid, Rot, Scale, Color, TEXT("MapWall"));
}

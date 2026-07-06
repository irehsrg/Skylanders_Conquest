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

	// Palette (unlit flat colors — pick for readability/contrast)
	const FLinearColor Void(0.015f, 0.02f, 0.015f);     // out-of-bounds catch floor (near black)
	const FLinearColor Lane(0.26f, 0.50f, 0.20f);       // lane grass (bright)
	const FLinearColor Jungle(0.08f, 0.20f, 0.09f);     // jungle grass (dark)
	const FLinearColor BlueTint(0.10f, 0.20f, 0.55f);   // blue base (strong blue)
	const FLinearColor RedTint(0.55f, 0.14f, 0.12f);    // red base (strong red)
	const FLinearColor WallCol(0.05f, 0.06f, 0.08f);    // terrain walls (dark)

	// ========================================================================
	// LANE CENTERLINE (blue -X  ->  red +X), a point-symmetric serpentine.
	// Reused for the lane floor AND the minion waypoint paths.
	// ========================================================================
	TArray<FVector> LanePath;
	LanePath.Add(FVector(-4600, 0, 0));      // blue base mouth
	LanePath.Add(FVector(-3400, 520, 0));
	LanePath.Add(FVector(-1900, -320, 0));
	LanePath.Add(FVector(0, 0, 0));          // mid
	LanePath.Add(FVector(1900, 320, 0));
	LanePath.Add(FVector(3400, -520, 0));
	LanePath.Add(FVector(4600, 0, 0));       // red base mouth

	// ========================================================================
	// FLOORS (top surface at Z=0 so structures placed near Z=0 sit correctly)
	// ========================================================================

	// Big dark catch-floor JUST BELOW the colored floors (top at Z=-5) so nothing
	// falls into the void. Routed through SpawnFloorSeg (which sets its transform
	// correctly) — the old SpawnGround left it centered at the origin, its top at
	// Z=50, sitting on top of and hiding every colored floor piece.
	SpawnFloorSeg(FVector(-7500, 0, 0), FVector(7500, 0, 0), 8000.0f, -5.0f, Void);

	// Base chambers (bulbous ends)
	SpawnFloorDisc(FVector(-4900, 0, 0), 1500.0f, 0.0f, BlueTint);
	SpawnFloorDisc(FVector(4900, 0, 0), 1500.0f, 0.0f, RedTint);

	// Lane corridor: wide grass ribbon following the serpentine
	for (int32 i = 0; i < LanePath.Num() - 1; i++)
	{
		SpawnFloorSeg(LanePath[i], LanePath[i + 1], 1150.0f, 0.0f, Lane);
		SpawnFloorDisc(LanePath[i + 1], 600.0f, 0.0f, Lane); // round the bends
	}

	// Jungle pockets (grass discs) + short connectors back to the lane
	auto Pocket = [&](FVector At, FVector LaneJoin)
	{
		SpawnFloorSeg(LaneJoin, At, 650.0f, 0.0f, Jungle);
		SpawnFloorDisc(At, 900.0f, 0.0f, Jungle);
	};
	Pocket(FVector(-3200, 1550, 0), FVector(-3400, 520, 0));   // blue mana jungle
	Pocket(FVector(3200, -1550, 0), FVector(3400, -520, 0));   // red mana jungle
	Pocket(FVector(-1500, -1650, 0), FVector(-1900, -320, 0)); // blue damage jungle
	Pocket(FVector(1500, 1650, 0), FVector(1900, 320, 0));     // red damage jungle
	Pocket(FVector(0, 2050, 0), FVector(0, 0, 0));             // north mid pocket
	Pocket(FVector(0, -2050, 0), FVector(0, 0, 0));            // south mid pocket

	// Perimeter wall ring so the map is fully enclosed (rough bounds; the
	// organic silhouette is the grass shape sitting inside it)
	const float WH = 400.0f, WT = 120.0f;
	SpawnWallSeg(FVector(-6300, -3100, 0), FVector(6300, -3100, 0), WH, WT, WallCol);
	SpawnWallSeg(FVector(-6300, 3100, 0), FVector(6300, 3100, 0), WH, WT, WallCol);
	SpawnWallSeg(FVector(-6300, -3100, 0), FVector(-6300, 3100, 0), WH, WT, WallCol);
	SpawnWallSeg(FVector(6300, -3100, 0), FVector(6300, 3100, 0), WH, WT, WallCol);

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

	// Blue (negative X): Spawn > Titan > Phoenix > Tower(on lane)
	BlueSpawnArea = SpawnAreaFor(FVector(-5600, 0, 0), ETowerTeam::Friendly);
	BlueTitan = SpawnTitanFor(FVector(-5100, 0, 150), ETowerTeam::Friendly, TEXT("Blue Titan"));
	BluePhoenix = SpawnStructureTower(FVector(-4300, 0, 15), ETowerTeam::Friendly, TEXT("Blue Phoenix"), true, 100.0f, 1000.0f);
	BlueTower = SpawnStructureTower(FVector(-2800, 200, 15), ETowerTeam::Friendly, TEXT("Blue Tower"), false, 50.0f, 0.0f);

	// Red (positive X): mirror
	RedSpawnArea = SpawnAreaFor(FVector(5600, 0, 0), ETowerTeam::Enemy);
	RedTitan = SpawnTitanFor(FVector(5100, 0, 150), ETowerTeam::Enemy, TEXT("Red Titan"));
	RedPhoenix = SpawnStructureTower(FVector(4300, 0, 15), ETowerTeam::Enemy, TEXT("Red Phoenix"), true, 100.0f, 1000.0f);
	RedTower = SpawnStructureTower(FVector(2800, -200, 15), ETowerTeam::Enemy, TEXT("Red Tower"), false, 50.0f, 0.0f);

	// Minion spawners + lane waypoint paths (spawn -> enemy base)
	BlueSpawner = World->SpawnActor<ASkylandersMinionSpawner>(
		ASkylandersMinionSpawner::StaticClass(), FVector(-4300, 0, 0), FRotator::ZeroRotator, SP);
	if (BlueSpawner)
	{
		BlueSpawner->Team = ETowerTeam::Friendly;
		BlueSpawner->LaneTargetPoint = FVector(5100, 0, 0);
		for (int32 i = 1; i < LanePath.Num(); i++) BlueSpawner->LaneWaypoints.Add(LanePath[i]);
		BlueSpawner->LaneWaypoints.Add(FVector(5100, 0, 0));
	}

	RedSpawner = World->SpawnActor<ASkylandersMinionSpawner>(
		ASkylandersMinionSpawner::StaticClass(), FVector(4300, 0, 0), FRotator::ZeroRotator, SP);
	if (RedSpawner)
	{
		RedSpawner->Team = ETowerTeam::Enemy;
		RedSpawner->LaneTargetPoint = FVector(-5100, 0, 0);
		for (int32 i = LanePath.Num() - 2; i >= 0; i--) RedSpawner->LaneWaypoints.Add(LanePath[i]);
		RedSpawner->LaneWaypoints.Add(FVector(-5100, 0, 0));
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

	BlueBlueBuff = SpawnCamp(FVector(-3200, 1550, 75), TEXT("Blue Buff"), EBuffType::Mana, 1.0f, 0.0f, 0.0f, 0);
	RedBlueBuff = SpawnCamp(FVector(3200, -1550, 75), TEXT("Blue Buff"), EBuffType::Mana, 1.0f, 0.0f, 0.0f, 0);
	DamageCamp = SpawnCamp(FVector(-1500, -1650, 75), TEXT("Damage Buff"), EBuffType::Damage, 1.25f, 0.0f, 0.0f, 0);
	SpawnCamp(FVector(1500, 1650, 75), TEXT("Damage Buff"), EBuffType::Damage, 1.25f, 0.0f, 0.0f, 0);
	BullDemonKing = SpawnCamp(FVector(0, 2050, 75), TEXT("Bull Demon King"), EBuffType::Damage, 1.50f, 2500.0f, 200.0f, 150);
	MidCamp = SpawnCamp(FVector(0, -2050, 75), TEXT("Mid Harpies"), EBuffType::None, 1.0f, 300.0f, 80.0f, 40);

	// ========================================================================
	// ENEMY GOD (parks in the red jungle)
	// ========================================================================
	World->SpawnActor<ASkylandersEnemyGod>(
		ASkylandersEnemyGod::StaticClass(), FVector(3800, -300, 100), FRotator::ZeroRotator, SP);

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

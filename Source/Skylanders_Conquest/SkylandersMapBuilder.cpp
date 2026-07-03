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

	UE_LOG(LogTemp, Log, TEXT("=== Building Joust Map ==="));

	// ========================================================================
	// GROUND PLANES
	// ========================================================================

	// Main lane ground (dark green)
	SpawnGround(FVector(0, 0, -50), FVector(130, 12, 1), FLinearColor(0.08f, 0.22f, 0.08f));

	// North jungle ground (darker)
	SpawnGround(FVector(0, 2000, -50), FVector(100, 30, 1), FLinearColor(0.06f, 0.15f, 0.06f));

	// South jungle ground — reaches Y=-600 so it meets the lane ground edge
	// (a narrower plane left a 200-unit bottomless strip on the way to Mid Camp)
	SpawnGround(FVector(0, -1800, -50), FVector(100, 24, 1), FLinearColor(0.06f, 0.15f, 0.06f));

	// No walls — open map for now, can add in editor later

	// ========================================================================
	// PLAYER (BLUE) SIDE — negative X
	// ========================================================================

	// NOTE: structures spawn DEFERRED so Team/bIsPhoenix/etc. are set before
	// BeginPlay runs — BeginPlay derives colors and per-type setup from them.

	// Spawn Area
	{
		FTransform T(FRotator::ZeroRotator, FVector(-5500, 0, 0));
		BlueSpawnArea = World->SpawnActorDeferred<ASkylandersSpawnArea>(
			ASkylandersSpawnArea::StaticClass(), T, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (BlueSpawnArea)
		{
			BlueSpawnArea->Team = ETowerTeam::Friendly;
			BlueSpawnArea->FinishSpawning(T);
		}
	}

	// Titan
	{
		FTransform T(FRotator::ZeroRotator, FVector(-4500, 0, 150));
		BlueTitan = World->SpawnActorDeferred<ASkylandersTitan>(
			ASkylandersTitan::StaticClass(), T, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (BlueTitan)
		{
			BlueTitan->Team = ETowerTeam::Friendly;
			BlueTitan->TitanName = TEXT("Blue Titan");
			BlueTitan->AttackDamage = 120.0f;
			BlueTitan->FinishSpawning(T);
		}
	}

	// Phoenix (inner structure, respawns)
	{
		FTransform T(FRotator::ZeroRotator, FVector(-3000, 0, 15));
		BluePhoenix = World->SpawnActorDeferred<ASkylandersTower>(
			ASkylandersTower::StaticClass(), T, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (BluePhoenix)
		{
			BluePhoenix->Team = ETowerTeam::Friendly;
			BluePhoenix->TowerName = TEXT("Blue Phoenix");
			BluePhoenix->bIsPhoenix = true;
			BluePhoenix->MaxHealth = 2500.0f;
			BluePhoenix->CurrentHealth = 2500.0f;
			BluePhoenix->AttackDamage = 100.0f;
			BluePhoenix->AttackRange = 1000.0f;
			BluePhoenix->PhoenixRespawnTime = 180.0f;
			BluePhoenix->FinishSpawning(T);
		}
	}

	// Tower (outer structure)
	{
		FTransform T(FRotator::ZeroRotator, FVector(-1500, 0, 15));
		BlueTower = World->SpawnActorDeferred<ASkylandersTower>(
			ASkylandersTower::StaticClass(), T, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (BlueTower)
		{
			BlueTower->Team = ETowerTeam::Friendly;
			BlueTower->TowerName = TEXT("Blue Tower");
			BlueTower->AttackDamage = 50.0f;
			BlueTower->FinishSpawning(T);
		}
	}

	// Minion Spawner
	BlueSpawner = World->SpawnActor<ASkylandersMinionSpawner>(
		ASkylandersMinionSpawner::StaticClass(), FVector(-3800, 0, 0), FRotator::ZeroRotator, SP);
	if (BlueSpawner)
	{
		BlueSpawner->Team = ETowerTeam::Friendly;
		BlueSpawner->LaneTargetPoint = FVector(4500, 0, 0);
	}

	// ========================================================================
	// ENEMY (RED) SIDE — positive X
	// ========================================================================

	// Spawn Area
	{
		FTransform T(FRotator::ZeroRotator, FVector(5500, 0, 0));
		RedSpawnArea = World->SpawnActorDeferred<ASkylandersSpawnArea>(
			ASkylandersSpawnArea::StaticClass(), T, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (RedSpawnArea)
		{
			RedSpawnArea->Team = ETowerTeam::Enemy;
			RedSpawnArea->FinishSpawning(T);
		}
	}

	// Titan
	{
		FTransform T(FRotator::ZeroRotator, FVector(4500, 0, 150));
		RedTitan = World->SpawnActorDeferred<ASkylandersTitan>(
			ASkylandersTitan::StaticClass(), T, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (RedTitan)
		{
			RedTitan->Team = ETowerTeam::Enemy;
			RedTitan->TitanName = TEXT("Red Titan");
			RedTitan->AttackDamage = 120.0f;
			RedTitan->FinishSpawning(T);
		}
	}

	// Phoenix
	{
		FTransform T(FRotator::ZeroRotator, FVector(3000, 0, 15));
		RedPhoenix = World->SpawnActorDeferred<ASkylandersTower>(
			ASkylandersTower::StaticClass(), T, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (RedPhoenix)
		{
			RedPhoenix->Team = ETowerTeam::Enemy;
			RedPhoenix->TowerName = TEXT("Red Phoenix");
			RedPhoenix->bIsPhoenix = true;
			RedPhoenix->MaxHealth = 2500.0f;
			RedPhoenix->CurrentHealth = 2500.0f;
			RedPhoenix->AttackDamage = 100.0f;
			RedPhoenix->AttackRange = 1000.0f;
			RedPhoenix->PhoenixRespawnTime = 180.0f;
			RedPhoenix->FinishSpawning(T);
		}
	}

	// Tower
	{
		FTransform T(FRotator::ZeroRotator, FVector(1500, 0, 15));
		RedTower = World->SpawnActorDeferred<ASkylandersTower>(
			ASkylandersTower::StaticClass(), T, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (RedTower)
		{
			RedTower->Team = ETowerTeam::Enemy;
			RedTower->TowerName = TEXT("Red Tower");
			RedTower->AttackDamage = 50.0f;
			RedTower->FinishSpawning(T);
		}
	}

	// Minion Spawner
	RedSpawner = World->SpawnActor<ASkylandersMinionSpawner>(
		ASkylandersMinionSpawner::StaticClass(), FVector(3800, 0, 0), FRotator::ZeroRotator, SP);
	if (RedSpawner)
	{
		RedSpawner->Team = ETowerTeam::Enemy;
		RedSpawner->LaneTargetPoint = FVector(-4500, 0, 0);
	}

	// ========================================================================
	// PROTECTION CHAINS: Tower → Phoenix → Titan
	// ========================================================================

	// Tower protects Phoenix, Phoenix protects Titan
	if (BluePhoenix) BluePhoenix->ProtectingTower = BlueTower;
	if (BlueTitan) BlueTitan->ProtectingTower = BluePhoenix;
	if (RedPhoenix) RedPhoenix->ProtectingTower = RedTower;
	if (RedTitan) RedTitan->ProtectingTower = RedPhoenix;

	// ========================================================================
	// JUNGLE CAMPS
	// ========================================================================

	// --- Bull Demon King (major objective, north center) ---
	{
		FTransform T(FRotator::ZeroRotator, FVector(0, 2500, 75));
		BullDemonKing = World->SpawnActorDeferred<ASkylandersBuffCamp>(
			ASkylandersBuffCamp::StaticClass(), T, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	}
	if (BullDemonKing)
	{
		BullDemonKing->CampName = TEXT("Bull Demon King");
		BullDemonKing->MaxHealth = 2500.0f;
		BullDemonKing->CurrentHealth = 2500.0f;
		BullDemonKing->AttackDamage = 40.0f;
		BullDemonKing->LeashRange = 400.0f;
		BullDemonKing->RespawnDelay = 120.0f;
		BullDemonKing->BuffType = EBuffType::Damage;
		BullDemonKing->BuffDamageMultiplier = 1.50f; // +50% power
		BullDemonKing->BuffDuration = 90.0f;
		BullDemonKing->XPReward = 200.0f;
		BullDemonKing->CoinReward = 150;
		BullDemonKing->FinishSpawning(FTransform(FRotator::ZeroRotator, FVector(0, 2500, 75)));
	}

	// --- Damage Camp (north of lane, slightly west) ---
	{
		FTransform T(FRotator::ZeroRotator, FVector(-600, 1300, 75));
		DamageCamp = World->SpawnActorDeferred<ASkylandersBuffCamp>(
			ASkylandersBuffCamp::StaticClass(), T, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (DamageCamp)
		{
			DamageCamp->CampName = TEXT("Damage Buff");
			DamageCamp->BuffType = EBuffType::Damage;
			DamageCamp->BuffDamageMultiplier = 1.25f;
			DamageCamp->FinishSpawning(T);
		}
	}

	// --- Mid Camp (south of lane, XP/Gold only) ---
	{
		FTransform T(FRotator::ZeroRotator, FVector(0, -1500, 75));
		MidCamp = World->SpawnActorDeferred<ASkylandersBuffCamp>(
			ASkylandersBuffCamp::StaticClass(), T, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (MidCamp)
		{
			MidCamp->CampName = TEXT("Mid Harpies");
			MidCamp->BuffType = EBuffType::None;
			MidCamp->MaxHealth = 300.0f;
			MidCamp->CurrentHealth = 300.0f;
			MidCamp->XPReward = 80.0f;
			MidCamp->CoinReward = 40;
			MidCamp->RespawnDelay = 60.0f;
			MidCamp->FinishSpawning(T);
		}
	}

	// --- Blue Buff (Blue side, north jungle) ---
	{
		FTransform T(FRotator::ZeroRotator, FVector(-2500, 1500, 75));
		BlueBlueBuff = World->SpawnActorDeferred<ASkylandersBuffCamp>(
			ASkylandersBuffCamp::StaticClass(), T, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (BlueBlueBuff)
		{
			BlueBlueBuff->CampName = TEXT("Blue Buff");
			BlueBlueBuff->BuffType = EBuffType::Mana;
			BlueBlueBuff->BuffDuration = 90.0f;
			BlueBlueBuff->FinishSpawning(T);
		}
	}

	// --- Blue Buff (Red side, north jungle) ---
	{
		FTransform T(FRotator::ZeroRotator, FVector(2500, 1500, 75));
		RedBlueBuff = World->SpawnActorDeferred<ASkylandersBuffCamp>(
			ASkylandersBuffCamp::StaticClass(), T, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (RedBlueBuff)
		{
			RedBlueBuff->CampName = TEXT("Blue Buff");
			RedBlueBuff->BuffType = EBuffType::Mana;
			RedBlueBuff->BuffDuration = 90.0f;
			RedBlueBuff->FinishSpawning(T);
		}
	}

	// ========================================================================
	// ENEMY GOD
	// ========================================================================

	ASkylandersEnemyGod* EnemyGod = World->SpawnActor<ASkylandersEnemyGod>(
		ASkylandersEnemyGod::StaticClass(), FVector(4000, 0, 100), FRotator::ZeroRotator, SP);
	if (EnemyGod)
	{
		UE_LOG(LogTemp, Log, TEXT("Enemy God spawned at (4000, 0)"));
	}

	UE_LOG(LogTemp, Log, TEXT("=== Joust Map Complete! ==="));
	UE_LOG(LogTemp, Log, TEXT("  Blue: Spawn(-5500) > Titan(-4500) > Phoenix(-3000) > Tower(-1500)"));
	UE_LOG(LogTemp, Log, TEXT("  Red:  Tower(1500) > Phoenix(3000) > Titan(4500) > Spawn(5500)"));
	UE_LOG(LogTemp, Log, TEXT("  Jungle: Bull Demon(0,2500), Damage(-600,1300), Mid(0,-1500)"));
	UE_LOG(LogTemp, Log, TEXT("  Blue Buffs: (-2500,1500) and (2500,1500)"));
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

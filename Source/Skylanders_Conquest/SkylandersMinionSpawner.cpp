// Skylanders Conquest - Minion Wave Spawner Implementation

#include "SkylandersMinionSpawner.h"
#include "SkylandersMinion.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

ASkylandersMinionSpawner::ASkylandersMinionSpawner()
{
	PrimaryActorTick.bCanEverTick = true; // Need tick for debug visual

	// Root component so the spawner has a position
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Team = ETowerTeam::Friendly;
	LaneTargetPoint = FVector::ZeroVector;
	WaveInterval = 30.0f;
	FirstWaveDelay = 10.0f;
	MeleeCount = 3;
	RangedCount = 3;
	SpawnSpread = 80.0f;
	SpawnStaggerDelay = 0.6f; // 0.6s between each minion
	WaveNumber = 0;
	SpawnQueueIndex = 0;
}

void ASkylandersMinionSpawner::BeginPlay()
{
	Super::BeginPlay();

	// Spawn first wave after delay, then repeat
	GetWorld()->GetTimerManager().SetTimer(WaveTimerHandle, this,
		&ASkylandersMinionSpawner::SpawnWave, WaveInterval, true, FirstWaveDelay);

	UE_LOG(LogTemp, Log, TEXT("Minion Spawner (%s) at %s. Target: %s. First wave in %.0fs"),
		Team == ETowerTeam::Friendly ? TEXT("Friendly") : TEXT("Enemy"),
		*GetActorLocation().ToString(), *LaneTargetPoint.ToString(), FirstWaveDelay);
}

void ASkylandersMinionSpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Draw debug sphere so you can see where the spawner is
	FColor SpawnerColor = (Team == ETowerTeam::Friendly) ? FColor::Blue : FColor::Red;
	DrawDebugSphere(GetWorld(), GetActorLocation(), 60.0f, 8, SpawnerColor, false, 0.0f, 0, 2.0f);
}

void ASkylandersMinionSpawner::SpawnWave()
{
	WaveNumber++;
	FVector SpawnBase = GetActorLocation();
	SpawnBase.Z += 50.0f; // Offset up so minions don't spawn inside the floor

	// Direction toward lane target (used to stagger spawn positions)
	FVector LaneDir = (LaneTargetPoint - SpawnBase).GetSafeNormal2D();

	// Build queue: melee first (they march ahead), then ranged
	SpawnQueue.Empty();
	SpawnQueueIndex = 0;

	// All minions spawn at the same point - stagger delay lets them spread naturally
	// Melee first (they march ahead), then ranged
	for (int32 i = 0; i < MeleeCount; i++)
	{
		SpawnQueue.Add(TPair<EMinionType, FVector>(EMinionType::Melee, SpawnBase));
	}

	for (int32 i = 0; i < RangedCount; i++)
	{
		SpawnQueue.Add(TPair<EMinionType, FVector>(EMinionType::Ranged, SpawnBase));
	}

	// Start staggered spawning
	SpawnNextMinion();

	float HpMult = 1.0f + (WaveNumber - 1) * 0.08f;
	float DmgMult = 1.0f + (WaveNumber - 1) * 0.05f;
	UE_LOG(LogTemp, Log, TEXT("Wave %d scaling: HP x%.2f, DMG x%.2f"), WaveNumber, HpMult, DmgMult);

	UE_LOG(LogTemp, Log, TEXT("Wave %d: Spawning %d %s minions (%d melee, %d ranged)"),
		WaveNumber, MeleeCount + RangedCount,
		Team == ETowerTeam::Friendly ? TEXT("Friendly") : TEXT("Enemy"),
		MeleeCount, RangedCount);
}

void ASkylandersMinionSpawner::SpawnNextMinion()
{
	if (SpawnQueueIndex >= SpawnQueue.Num()) return;

	EMinionType Type = SpawnQueue[SpawnQueueIndex].Key;
	FVector SpawnLoc = SpawnQueue[SpawnQueueIndex].Value;
	SpawnQueueIndex++;

	// Deferred spawn: Team/MinionType must be set BEFORE BeginPlay runs, because
	// BeginPlay derives collision, body color, and per-type stats from them.
	FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLoc);
	ASkylandersMinion* Minion = GetWorld()->SpawnActorDeferred<ASkylandersMinion>(
		ASkylandersMinion::StaticClass(), SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

	if (Minion)
	{
		Minion->Team = Team;
		Minion->MinionType = Type;
		Minion->LaneTargetPoint = LaneTargetPoint;

		// BeginPlay applies the per-type base stats (melee defaults / ranged overrides)
		Minion->FinishSpawning(SpawnTransform);

		// Wave scaling on top of base stats: wave 1 is baseline, +8%/+5%/+10% per wave after
		float WaveHpMult = 1.0f + (WaveNumber - 1) * 0.08f;
		float WaveDmgMult = 1.0f + (WaveNumber - 1) * 0.05f;
		float WaveXpMult = 1.0f + (WaveNumber - 1) * 0.10f;

		Minion->MaxHealth *= WaveHpMult;
		Minion->CurrentHealth *= WaveHpMult;
		Minion->AttackDamage *= WaveDmgMult;
		Minion->XPReward *= WaveXpMult;
		Minion->CoinReward += WaveNumber - 1; // +1 coin per wave after the first
	}

	// Schedule next minion
	if (SpawnQueueIndex < SpawnQueue.Num())
	{
		GetWorld()->GetTimerManager().SetTimer(StaggerTimerHandle, this,
			&ASkylandersMinionSpawner::SpawnNextMinion, SpawnStaggerDelay, false);
	}
}
